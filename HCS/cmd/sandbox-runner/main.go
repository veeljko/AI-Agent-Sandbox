package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"time"

	"example.com/containerd-windows-runner/internal/containerdclient"
	"example.com/containerd-windows-runner/internal/hostconsole"
	"example.com/containerd-windows-runner/internal/sandbox"
)

func main() {
	exitCode, err := run(os.Args[1:])
	if errors.Is(err, flag.ErrHelp) {
		os.Exit(0)
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "sandbox-runner: %v\n", err)
		os.Exit(1)
	}
	os.Exit(exitCode)
}

func run(args []string) (exitCode int, retErr error) {
	cfg, err := parseConfig(args)
	if err != nil {
		return 1, err
	}
	if cfg.Terminal {
		width, height, err := hostconsole.Size(cfg.Stdout)
		if err != nil {
			return 1, fmt.Errorf("TTY zahteva interaktivnu Windows konzolu: %w", err)
		}
		cfg.TerminalWidth = uint(width)
		cfg.TerminalHeight = uint(height)
	}

	ctx, cancel := context.WithTimeout(context.Background(), cfg.OperationTimeout)
	defer cancel()

	client, err := containerdclient.New(ctx, cfg.Address, cfg.Namespace, cfg.Platform, cfg.DialTimeout)
	if err != nil {
		return 1, err
	}
	defer func() {
		if err := client.Close(); err != nil {
			retErr = errors.Join(retErr, fmt.Errorf("zatvaranje containerd klijenta: %w", err))
		}
	}()

	logger := log.New(os.Stderr, "containerd: ", log.LstdFlags|log.Lmicroseconds)
	runner := sandbox.NewRunner(client, logger.Printf)
	if err := runner.Prepare(ctx, cfg); err != nil {
		return 1, err
	}
	defer func() {
		cleanupCtx, cleanupCancel := context.WithTimeout(context.Background(), cfg.CleanupTimeout)
		defer cleanupCancel()
		if err := runner.Cleanup(cleanupCtx); err != nil {
			retErr = errors.Join(retErr, fmt.Errorf("cleanup: %w", err))
		}
	}()

	var consoleSession *hostconsole.Session
	if cfg.Terminal {
		consoleSession, err = hostconsole.Enable(cfg.Stdin, cfg.Stdout, cfg.Stderr)
		if err != nil {
			return 1, err
		}
		defer func() {
			if err := consoleSession.Close(); err != nil {
				retErr = errors.Join(retErr, err)
			}
		}()
	}

	session, err := runner.Start(ctx)
	if err != nil {
		return 1, err
	}
	network := cfg.NetworkName
	if network == "" {
		network = "isključena"
	}
	fmt.Fprintf(os.Stderr, "Pokrenut container %s (Hyper-V isolation, network=%s)\n", session.ContainerID, network)

	resizeDone := make(chan struct{})
	var resizeWG sync.WaitGroup
	if cfg.Terminal {
		resizeWG.Add(1)
		go func() {
			defer resizeWG.Done()
			width, height := uint32(cfg.TerminalWidth), uint32(cfg.TerminalHeight)
			ticker := time.NewTicker(300 * time.Millisecond)
			defer ticker.Stop()
			for {
				select {
				case <-resizeDone:
					return
				case <-ticker.C:
					nextWidth, nextHeight, sizeErr := hostconsole.Size(cfg.Stdout)
					if sizeErr != nil || (nextWidth == width && nextHeight == height) {
						continue
					}
					resizeCtx, resizeCancel := context.WithTimeout(context.Background(), 2*time.Second)
					resizeErr := runner.ResizeTTY(resizeCtx, nextWidth, nextHeight)
					resizeCancel()
					if resizeErr != nil {
						logger.Printf("prosleđivanje TTY resize-a: %v", resizeErr)
						continue
					}
					width, height = nextWidth, nextHeight
				}
			}
		}()
	}
	defer func() {
		close(resizeDone)
		resizeWG.Wait()
	}()

	interrupts := make(chan os.Signal, 2)
	signal.Notify(interrupts, os.Interrupt)
	proxyDone := make(chan struct{})
	defer func() {
		signal.Stop(interrupts)
		close(proxyDone)
	}()
	go func() {
		for {
			select {
			case <-proxyDone:
				return
			case <-interrupts:
				signalCtx, signalCancel := context.WithTimeout(context.Background(), 5*time.Second)
				if err := runner.Signal(signalCtx, syscall.SIGINT); err != nil {
					logger.Printf("prosleđivanje Ctrl+C/SIGINT: %v", err)
				}
				signalCancel()
			}
		}
	}()

	code, err := runner.Wait(ctx)
	if err != nil {
		stopCtx, stopCancel := context.WithTimeout(context.Background(), cfg.StopTimeout+5*time.Second)
		stopErr := runner.Stop(stopCtx)
		stopCancel()
		return 1, errors.Join(fmt.Errorf("čekanje taska: %w", err), stopErr)
	}
	fmt.Fprintf(os.Stderr, "Container exit code: %d\n", code)
	return int(code), nil
}
