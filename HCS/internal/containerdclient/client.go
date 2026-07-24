package containerdclient

import (
	"context"
	"fmt"
	"time"

	"github.com/containerd/containerd"
	introspectionapi "github.com/containerd/containerd/api/services/introspection/v1"
	"github.com/containerd/containerd/namespaces"
	"github.com/containerd/containerd/platforms"
	ocispec "github.com/opencontainers/image-spec/specs-go/v1"
)

const snapshotterPluginType = "io.containerd.snapshotter.v1"

type Client struct {
	raw       *containerd.Client
	namespace string
	platform  string
}

func New(ctx context.Context, address, namespace, platform string, dialTimeout time.Duration) (*Client, error) {
	parsedPlatform, err := platforms.Parse(platform)
	if err != nil {
		return nil, fmt.Errorf("platform %q nije ispravna: %w", platform, err)
	}

	raw, err := containerd.New(
		address,
		containerd.WithDefaultNamespace(namespace),
		containerd.WithDefaultPlatform(platforms.OnlyStrict(parsedPlatform)),
		containerd.WithTimeout(dialTimeout),
	)
	if err != nil {
		return nil, fmt.Errorf("povezivanje na containerd %s: %w", address, err)
	}

	c := &Client{raw: raw, namespace: namespace, platform: platform}
	return c, nil
}

func (c *Client) Context(ctx context.Context) context.Context {
	return namespaces.WithNamespace(ctx, c.namespace)
}

func (c *Client) Raw() *containerd.Client {
	return c.raw
}

func (c *Client) Close() error {
	return c.raw.Close()
}

// DetectWindowsSnapshotter reads the live introspection plugin list. It does
// not assume that the snapshotter is named "windows". If override is supplied,
// that exact plugin must be healthy and advertise the requested platform.
func (c *Client) DetectWindowsSnapshotter(ctx context.Context, override string) (string, error) {
	// ctx = c.Context(ctx)
	response, err := c.raw.IntrospectionService().Plugins(ctx, nil)
	if err != nil {
		return "", fmt.Errorf("listanje containerd plugina: %w", err)
	}

	wanted, err := platforms.Parse(c.platform)
	if err != nil {
		return "", fmt.Errorf("interna platforma %q: %w", c.platform, err)
	}
	matcher := platforms.OnlyStrict(wanted)

	candidates := findSnapshotterCandidates(response.Plugins, matcher, override)

	if len(candidates) == 0 {
		if override != "" {
			return "", fmt.Errorf("snapshotter %q nije healthy plugin za %s", override, c.platform)
		}
		return "", fmt.Errorf("nije pronađen healthy Windows snapshotter za %s", c.platform)
	}
	if len(candidates) > 1 && override == "" {
		return "", fmt.Errorf("više snapshottera podržava %s (%v); izaberi jedan eksplicitno", c.platform, candidates)
	}
	return candidates[0], nil
}

func findSnapshotterCandidates(plugins []*introspectionapi.Plugin, matcher platforms.Matcher, override string) []string {
	var candidates []string
	for _, plugin := range plugins {
		if plugin.Type != snapshotterPluginType || plugin.InitErr != nil {
			continue
		}
		if override != "" && plugin.ID != override {
			continue
		}

		for _, advertised := range plugin.Platforms {
			candidate := platforms.Normalize(ocispec.Platform{
				OS:           advertised.OS,
				Architecture: advertised.Architecture,
				Variant:      advertised.Variant,
			})
			if matcher.Match(candidate) {
				candidates = append(candidates, plugin.ID)
				break
			}
		}
	}
	return candidates
}
