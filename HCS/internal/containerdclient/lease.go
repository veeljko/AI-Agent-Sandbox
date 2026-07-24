package containerdclient

import (
	"context"
	"fmt"
	"time"

	"github.com/containerd/containerd/errdefs"
	"github.com/containerd/containerd/leases"
)

func (c *Client) CreateLease(ctx context.Context, lifetime time.Duration) (string, error) {
	ctx = c.Context(ctx)
	lease, err := c.raw.LeasesService().Create(
		ctx,
		leases.WithRandomID(),
		leases.WithExpiration(lifetime),
	)
	if err != nil {
		return "", fmt.Errorf("kreiranje containerd lease-a: %w", err)
	}
	return lease.ID, nil
}

func (c *Client) LeaseContext(ctx context.Context, leaseID string) context.Context {
	ctx = c.Context(ctx)
	if leaseID == "" {
		return ctx
	}
	return leases.WithLease(ctx, leaseID)
}

func (c *Client) DeleteLease(ctx context.Context, leaseID string) error {
	if leaseID == "" {
		return nil
	}
	ctx = c.Context(ctx)
	if err := c.raw.LeasesService().Delete(ctx, leases.Lease{ID: leaseID}); err != nil && !errdefs.IsNotFound(err) {
		return fmt.Errorf("brisanje lease-a %s: %w", leaseID, err)
	}
	return nil
}
