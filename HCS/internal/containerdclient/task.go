package containerdclient

import (
	"context"
	"errors"
	"fmt"
	"syscall"
	"time"

	"github.com/containerd/containerd"
	"github.com/containerd/containerd/cio"
	"github.com/containerd/containerd/errdefs"
)

type StartedTask struct {
	Task containerd.Task
	Wait <-chan containerd.ExitStatus
}

func (c *Client) CreateAndStartTask(
	ctx context.Context,
	ctr containerd.Container,
	creator cio.Creator,
) (*StartedTask, error) {
	ctx = c.Context(ctx)
	task, err := ctr.NewTask(ctx, creator)
	if err != nil {
		return nil, fmt.Errorf("kreiranje taska za %s: %w", ctr.ID(), err)
	}

	waitCh, err := task.Wait(ctx)
	if err != nil {
		cleanupCtx, cancel := context.WithTimeout(c.Context(context.Background()), 30*time.Second)
		defer cancel()
		_, cleanupErr := task.Delete(cleanupCtx, containerd.WithProcessKill)
		return nil, errors.Join(
			fmt.Errorf("registracija Wait pre Start za %s: %w", ctr.ID(), err),
			ignoreNotFound(cleanupErr),
		)
	}
	if err := task.Start(ctx); err != nil {
		cleanupCtx, cancel := context.WithTimeout(c.Context(context.Background()), 30*time.Second)
		defer cancel()
		_, cleanupErr := task.Delete(cleanupCtx, containerd.WithProcessKill)
		return nil, errors.Join(
			fmt.Errorf("start taska za %s: %w", ctr.ID(), err),
			ignoreNotFound(cleanupErr),
		)
	}
	return &StartedTask{Task: task, Wait: waitCh}, nil
}

func CloseStdin(ctx context.Context, task containerd.Task) error {
	return task.CloseIO(ctx, containerd.WithStdinCloser)
}

func Signal(ctx context.Context, task containerd.Task, signal syscall.Signal) error {
	return task.Kill(ctx, signal)
}

func ResizeTTY(ctx context.Context, task containerd.Task, width, height uint32) error {
	return task.Resize(ctx, width, height)
}

func (c *Client) DeleteTask(ctx context.Context, task containerd.Task) error {
	if task == nil {
		return nil
	}
	ctx = c.Context(ctx)
	_, err := task.Delete(ctx, containerd.WithProcessKill)
	if errdefs.IsNotFound(err) {
		return nil
	}
	if err != nil {
		return fmt.Errorf("brisanje taska %s: %w", task.ID(), err)
	}
	return nil
}

func (c *Client) DeleteContainerAndSnapshot(
	ctx context.Context,
	ctr containerd.Container,
	snapshotter, snapshotKey string,
) error {
	if ctr == nil {
		return nil
	}
	ctx = c.Context(ctx)
	if err := ctr.Delete(ctx, containerd.WithSnapshotCleanup); err == nil || errdefs.IsNotFound(err) {
		return nil
	}

	// Recovery path: remove the snapshot explicitly and then retry deletion of
	// container metadata without WithSnapshotCleanup.
	var cleanupErrs []error
	if snapshotKey != "" {
		if err := c.raw.SnapshotService(snapshotter).Remove(ctx, snapshotKey); err != nil && !errdefs.IsNotFound(err) {
			cleanupErrs = append(cleanupErrs, fmt.Errorf("brisanje snapshota %s: %w", snapshotKey, err))
		}
	}
	if err := ctr.Delete(ctx); err != nil && !errdefs.IsNotFound(err) {
		cleanupErrs = append(cleanupErrs, fmt.Errorf("brisanje container metadata %s: %w", ctr.ID(), err))
	}
	return errors.Join(cleanupErrs...)
}

func ignoreNotFound(err error) error {
	if err == nil || errdefs.IsNotFound(err) {
		return nil
	}
	return err
}
