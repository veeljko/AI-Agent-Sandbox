package main

import (
	"flag"

	"example.com/containerd-windows-runner/internal/sandbox"
)

func parseConfig(args []string) (sandbox.RunConfig, error) {
	cfg := sandbox.DefaultConfig()
	flags := flag.NewFlagSet("sandbox-runner", flag.ContinueOnError)
	flags.SetOutput(cfg.Stderr)

	flags.StringVar(&cfg.Address, "address", cfg.Address, "containerd named-pipe adresa")
	flags.StringVar(&cfg.Namespace, "namespace", cfg.Namespace, "containerd namespace")
	flags.StringVar(&cfg.Platform, "platform", cfg.Platform, "OCI platforma (mora windows/amd64)")
	flags.StringVar(&cfg.Runtime, "runtime", cfg.Runtime, "containerd runtime")
	flags.StringVar(&cfg.Snapshotter, "snapshotter", cfg.Snapshotter, "snapshotter override; prazan bira jedini healthy Windows snapshotter")
	flags.StringVar(&cfg.Image, "image", cfg.Image, "Codex WCOW image referenca")
	flags.BoolVar(&cfg.PullImage, "pull", cfg.PullImage, "povuci image iz registra pre starta")

	flags.StringVar(&cfg.WorkspaceHost, "workspace", cfg.WorkspaceHost, "host direktorijum koji se mountuje kao workspace")
	flags.StringVar(&cfg.WorkspaceContainer, "workspace-container", cfg.WorkspaceContainer, "workspace putanja u containeru")
	flags.BoolVar(&cfg.WorkspaceReadOnly, "workspace-read-only", cfg.WorkspaceReadOnly, "mountuj workspace read-only")
	flags.StringVar(&cfg.CodexHomeHost, "codex-home", cfg.CodexHomeHost, "host direktorijum za trajni Codex config/auth/session state; prazan ga ne mountuje")
	flags.StringVar(&cfg.CodexHomeContainer, "codex-home-container", cfg.CodexHomeContainer, "CODEX_HOME putanja u containeru")
	flags.StringVar(&cfg.WorkingDirectory, "workdir", cfg.WorkingDirectory, "početni radni direktorijum procesa")
	flags.StringVar(&cfg.CommandLine, "command-line", cfg.CommandLine, "Windows command line; prazan koristi image ENTRYPOINT/CMD")

	flags.StringVar(&cfg.NetworkName, "network", cfg.NetworkName, "postojeća HCN mreža; prazan string isključuje mrežu")
	flags.BoolVar(&cfg.Terminal, "tty", cfg.Terminal, "dodeli Windows pseudokonzolu procesu")
	flags.UintVar(&cfg.TerminalWidth, "tty-width", cfg.TerminalWidth, "početna širina TTY-a")
	flags.UintVar(&cfg.TerminalHeight, "tty-height", cfg.TerminalHeight, "početna visina TTY-a")
	flags.Uint64Var(&cfg.MemoryLimitBytes, "memory-limit-bytes", cfg.MemoryLimitBytes, "Hyper-V utility VM memory limit u bajtovima; 0 koristi runtime default")
	flags.Uint64Var(&cfg.CPUCount, "cpu-count", cfg.CPUCount, "broj virtualnih CPU-a; 0 koristi runtime default")

	flags.DurationVar(&cfg.DialTimeout, "dial-timeout", cfg.DialTimeout, "containerd dial timeout")
	flags.DurationVar(&cfg.OperationTimeout, "operation-timeout", cfg.OperationTimeout, "maksimalno trajanje kompletne sesije")
	flags.DurationVar(&cfg.StopTimeout, "stop-timeout", cfg.StopTimeout, "grace period pre prinudnog stopa")
	flags.DurationVar(&cfg.CleanupTimeout, "cleanup-timeout", cfg.CleanupTimeout, "cleanup timeout")
	flags.DurationVar(&cfg.LeaseLifetime, "lease-lifetime", cfg.LeaseLifetime, "containerd lease lifetime")

	if err := flags.Parse(args); err != nil {
		return sandbox.RunConfig{}, err
	}
	if flags.NArg() != 0 {
		return sandbox.RunConfig{}, flag.ErrHelp
	}
	return cfg, nil
}
