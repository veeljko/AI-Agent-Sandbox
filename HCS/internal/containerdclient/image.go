package containerdclient

import (
	"context"
	"fmt"
	"time"

	"github.com/containerd/containerd"
)

type LogFunc func(format string, args ...any)

func (c *Client) PullAndUnpack(
	ctx context.Context,
	reference string,
	snapshotter string,
	logf LogFunc,
) (containerd.Image, error) {
	// ctx = c.Context(ctx)
	if logf == nil {
		logf = func(string, ...any) {}
	}

	logf("pull %s za platformu %s; snapshotter=%s", reference, c.platform, snapshotter)
	monitorCtx, stopMonitor := context.WithCancel(ctx)
	monitorDone := make(chan struct{})
	go func() {
		defer close(monitorDone)
		c.monitorDownloads(monitorCtx, logf)
	}()

	image, err := c.raw.Pull(
		ctx,
		reference,
		containerd.WithPlatform(c.platform),
		containerd.WithPullSnapshotter(snapshotter),
		containerd.WithPullUnpack,
	)
	stopMonitor()
	<-monitorDone
	if err != nil {
		return nil, fmt.Errorf("pull/unpack image-a %s za %s: %w", reference, c.platform, err)
	}

	unpacked, err := image.IsUnpacked(ctx, snapshotter)
	if err != nil {
		return nil, fmt.Errorf("provera unpack statusa za %s: %w", reference, err)
	}
	if !unpacked {
		return nil, fmt.Errorf("image %s je povučen, ali nije unpackovan snapshotterom %s", reference, snapshotter)
	}
	logf("image spreman: %s (%s)", image.Name(), image.Target().Digest)
	return image, nil
}

func (c *Client) GetUnpackedImage(ctx context.Context, reference, snapshotter string) (containerd.Image, error) {
	ctx = c.Context(ctx)
	image, err := c.raw.GetImage(ctx, reference)
	if err != nil {
		return nil, fmt.Errorf("image %s nije dostupan u namespace-u %s: %w", reference, c.namespace, err)
	}
	unpacked, err := image.IsUnpacked(ctx, snapshotter)
	if err != nil {
		return nil, fmt.Errorf("provera unpack statusa za %s: %w", reference, err)
	}
	if !unpacked {
		return nil, fmt.Errorf("image %s nije unpackovan snapshotterom %s", reference, snapshotter)
	}
	return image, nil
}

func (c *Client) monitorDownloads(ctx context.Context, logf LogFunc) {
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			statuses, err := c.raw.ContentStore().ListStatuses(ctx)
			if err != nil || len(statuses) == 0 {
				continue
			}
			var received, total int64
			for _, status := range statuses {
				received += status.Offset
				total += status.Total
			}
			logf("aktivna preuzimanja=%d, primljeno=%s, ukupno=%s", len(statuses), byteCount(received), byteCount(total))
		}
	}
}

func byteCount(value int64) string {
	const unit = int64(1024)
	if value < unit {
		return fmt.Sprintf("%d B", value)
	}
	divisor, exponent := unit, 0
	for quotient := value / unit; quotient >= unit; quotient /= unit {
		divisor *= unit
		exponent++
	}
	return fmt.Sprintf("%.1f %ciB", float64(value)/float64(divisor), "KMGTPE"[exponent])
}
