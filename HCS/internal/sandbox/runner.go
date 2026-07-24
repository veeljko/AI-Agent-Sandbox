package sandbox

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"syscall"
	"time"

	"example.com/containerd-windows-runner/internal/containerdclient"
	"example.com/containerd-windows-runner/internal/winnetwork"
	"github.com/containerd/containerd"
	"github.com/containerd/containerd/errdefs"
)

type Runner struct {
	client *containerdclient.Client
	logf   containerdclient.LogFunc

	mu          sync.Mutex
	cfg         RunConfig
	prepared    bool
	snapshotter string
	leaseID     string
	image       containerd.Image
	created     *containerdclient.CreatedContainer
	task        containerd.Task
	endpoint    *winnetwork.Endpoint
	waitDone    chan struct{}
	exitCode    uint32
	exitErr     error
}

func NewRunner(client *containerdclient.Client, logf containerdclient.LogFunc) *Runner {
	if logf == nil {
		logf = func(string, ...any) {}
	}
	return &Runner{client: client, logf: logf}
}

func (r *Runner) Prepare(ctx context.Context, cfg RunConfig) error {
	r.mu.Lock()
	if r.prepared {
		r.mu.Unlock()
		return fmt.Errorf("runner je već pripremljen")
	}
	r.mu.Unlock()

	normalized, err := cfg.NormalizeAndValidate()
	if err != nil {
		return fmt.Errorf("run konfiguracija: %w", err)
	}
	snapshotter, err := r.client.DetectWindowsSnapshotter(ctx, normalized.Snapshotter)
	if err != nil {
		return err
	}
	r.logf("izabran snapshotter: %s", snapshotter)

	leaseID, err := r.client.CreateLease(ctx, normalized.LeaseLifetime)
	if err != nil {
		return err
	}
	rollbackLease := true
	defer func() {
		if !rollbackLease {
			return
		}
		cleanupCtx, cancel := context.WithTimeout(context.Background(), normalized.CleanupTimeout)
		defer cancel()
		if err := r.client.DeleteLease(cleanupCtx, leaseID); err != nil {
			r.logf("upozorenje: rollback lease-a nije uspeo: %v", err)
		}
	}()

	leaseCtx := r.client.LeaseContext(ctx, leaseID)
	var image containerd.Image
	if normalized.PullImage {
		image, err = r.client.PullAndUnpack(leaseCtx, normalized.Image, snapshotter, r.logf)
	} else {
		image, err = r.client.GetUnpackedImage(leaseCtx, normalized.Image, snapshotter)
	}
	if err != nil {
		return err
	}

	r.mu.Lock()
	r.cfg = normalized
	r.snapshotter = snapshotter
	r.leaseID = leaseID
	r.image = image
	r.prepared = true
	r.mu.Unlock()
	rollbackLease = false
	return nil
}

func (r *Runner) Start(ctx context.Context) (*Session, error) {
	r.mu.Lock()
	if !r.prepared {
		r.mu.Unlock()
		return nil, fmt.Errorf("pozovi Prepare pre Start")
	}
	if r.task != nil || r.created != nil || r.endpoint != nil {
		r.mu.Unlock()
		return nil, fmt.Errorf("runner je već pokrenuo sesiju")
	}
	cfg := r.cfg
	image := r.image
	leaseID := r.leaseID
	r.mu.Unlock()

	var endpoint *winnetwork.Endpoint
	if cfg.NetworkName != "" {
		var err error
		endpoint, err = winnetwork.CreateEndpoint(cfg.NetworkName)
		if err != nil {
			return nil, err
		}
		r.mu.Lock()
		r.endpoint = endpoint
		r.mu.Unlock()
		r.logf("kreiran HCN endpoint: network=%s endpoint=%s", cfg.NetworkName, endpoint.ID())
	}

	opCtx := r.client.LeaseContext(ctx, leaseID)
	created, err := r.client.CreateWindowsContainer(opCtx, image, containerdclient.ContainerConfig{
		Platform:           cfg.Platform,
		Runtime:            cfg.Runtime,
		Snapshotter:        r.snapshotter,
		WorkspaceHost:      cfg.WorkspaceHost,
		WorkspaceContainer: cfg.WorkspaceContainer,
		WorkspaceReadOnly:  cfg.WorkspaceReadOnly,
		CodexHomeHost:      cfg.CodexHomeHost,
		CodexHomeContainer: cfg.CodexHomeContainer,
		WorkingDirectory:   cfg.WorkingDirectory,
		CommandLine:        cfg.CommandLine,
		NetworkEndpointID:  endpoint.ID(),
		Terminal:           cfg.Terminal,
		TerminalWidth:      cfg.TerminalWidth,
		TerminalHeight:     cfg.TerminalHeight,
		MemoryLimitBytes:   cfg.MemoryLimitBytes,
		CPUCount:           cfg.CPUCount,
	})
	if err != nil {
		return nil, err
	}
	r.mu.Lock()
	r.created = created
	r.mu.Unlock()

	creator := containerdclient.NewIOCreator(cfg.Stdin, cfg.Stdout, cfg.Stderr, cfg.Terminal)
	started, err := r.client.CreateAndStartTask(opCtx, created.Container, creator)
	if err != nil {
		return nil, err
	}

	waitDone := make(chan struct{})
	r.mu.Lock()
	r.task = started.Task
	r.waitDone = waitDone
	r.mu.Unlock()

	go r.collectExit(started.Wait, waitDone)
	session := &Session{
		ContainerID: created.Container.ID(),
		SnapshotKey: created.SnapshotKey,
		StartedAt:   time.Now().UTC(),
		Terminal:    cfg.Terminal,
	}
	r.logf("task pokrenut: container=%s snapshot=%s", session.ContainerID, session.SnapshotKey)
	return session, nil
}

func (r *Runner) collectExit(waitCh <-chan containerd.ExitStatus, done chan struct{}) {
	status, ok := <-waitCh
	var code uint32
	var err error
	if !ok {
		err = fmt.Errorf("containerd Wait kanal je zatvoren bez exit statusa")
	} else {
		code, _, err = status.Result()
	}
	r.mu.Lock()
	r.exitCode = code
	r.exitErr = err
	r.mu.Unlock()
	close(done)
}

func (r *Runner) Wait(ctx context.Context) (uint32, error) {
	r.mu.Lock()
	done := r.waitDone
	r.mu.Unlock()
	if done == nil {
		return 0, fmt.Errorf("nema pokrenutog taska")
	}
	select {
	case <-ctx.Done():
		return 0, ctx.Err()
	case <-done:
		r.mu.Lock()
		defer r.mu.Unlock()
		return r.exitCode, r.exitErr
	}
}

func (r *Runner) Signal(ctx context.Context, signal syscall.Signal) error {
	task, leaseID, err := r.currentTask()
	if err != nil {
		return err
	}
	if err := containerdclient.Signal(r.client.LeaseContext(ctx, leaseID), task, signal); err != nil {
		return fmt.Errorf("signal %d za task %s: %w", signal, task.ID(), err)
	}
	return nil
}

func (r *Runner) CloseStdin(ctx context.Context) error {
	task, leaseID, err := r.currentTask()
	if err != nil {
		return err
	}
	if err := containerdclient.CloseStdin(r.client.LeaseContext(ctx, leaseID), task); err != nil {
		return fmt.Errorf("zatvaranje stdin-a taska %s: %w", task.ID(), err)
	}
	return nil
}

func (r *Runner) ResizeTTY(ctx context.Context, width, height uint32) error {
	task, leaseID, err := r.currentTask()
	if err != nil {
		return err
	}
	if err := containerdclient.ResizeTTY(r.client.LeaseContext(ctx, leaseID), task, width, height); err != nil {
		return fmt.Errorf("resize TTY-a taska %s: %w", task.ID(), err)
	}
	return nil
}

func (r *Runner) Stop(ctx context.Context) error {
	task, leaseID, err := r.currentTask()
	if err != nil {
		return nil
	}
	opCtx := r.client.LeaseContext(ctx, leaseID)
	status, err := task.Status(opCtx)
	if err != nil {
		if errdefs.IsNotFound(err) {
			return nil
		}
		return fmt.Errorf("status taska %s: %w", task.ID(), err)
	}
	if status.Status == containerd.Stopped {
		return nil
	}

	// SIGTERM is the graceful attempt. hcsshim validates which signals the
	// current Windows build/runtime supports. A forced all-process kill follows
	// only after the configured grace period.
	if err := task.Kill(opCtx, syscall.SIGTERM); err != nil && !errdefs.IsNotFound(err) && !errdefs.IsFailedPrecondition(err) {
		r.logf("graceful SIGTERM nije prihvaćen: %v", err)
	}

	deadline := time.Now().Add(r.cfg.StopTimeout)
	ticker := time.NewTicker(200 * time.Millisecond)
	defer ticker.Stop()
	for time.Now().Before(deadline) {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			state, statusErr := task.Status(opCtx)
			if statusErr == nil && state.Status == containerd.Stopped {
				return nil
			}
			if errdefs.IsNotFound(statusErr) {
				return nil
			}
		}
	}
	if err := task.Kill(opCtx, syscall.SIGKILL, containerd.WithKillAll); err != nil && !errdefs.IsNotFound(err) && !errdefs.IsFailedPrecondition(err) {
		return fmt.Errorf("prinudni kill taska %s: %w", task.ID(), err)
	}
	return nil
}

func (r *Runner) Cleanup(ctx context.Context) error {
	r.mu.Lock()
	task := r.task
	created := r.created
	leaseID := r.leaseID
	snapshotter := r.snapshotter
	endpoint := r.endpoint
	r.mu.Unlock()

	var cleanupErrs []error
	if err := r.client.DeleteTask(r.client.LeaseContext(ctx, leaseID), task); err != nil {
		cleanupErrs = append(cleanupErrs, err)
	}
	if created != nil {
		if err := r.client.DeleteContainerAndSnapshot(
			r.client.LeaseContext(ctx, leaseID),
			created.Container,
			snapshotter,
			created.SnapshotKey,
		); err != nil {
			cleanupErrs = append(cleanupErrs, err)
		}
	}
	if err := endpoint.Delete(); err != nil {
		cleanupErrs = append(cleanupErrs, err)
	}
	if err := r.client.DeleteLease(ctx, leaseID); err != nil {
		cleanupErrs = append(cleanupErrs, err)
	}

	r.mu.Lock()
	r.task = nil
	r.created = nil
	r.image = nil
	r.endpoint = nil
	r.leaseID = ""
	r.prepared = false
	r.mu.Unlock()
	return errors.Join(cleanupErrs...)
}

func (r *Runner) currentTask() (containerd.Task, string, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.task == nil {
		return nil, "", fmt.Errorf("nema pokrenutog taska")
	}
	return r.task, r.leaseID, nil
}
