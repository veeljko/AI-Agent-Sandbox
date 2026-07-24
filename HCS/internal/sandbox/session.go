package sandbox

import (
	"context"
	"time"
)

type Session struct {
	ContainerID string
	SnapshotKey string
	StartedAt   time.Time
	Terminal    bool
}

type SandboxRunner interface {
	Prepare(ctx context.Context, cfg RunConfig) error
	Start(ctx context.Context) (*Session, error)
	Wait(ctx context.Context) (uint32, error)
	Stop(ctx context.Context) error
	Cleanup(ctx context.Context) error
}
