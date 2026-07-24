package containerdclient

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/containerd/containerd"
	"github.com/containerd/containerd/containers"
	"github.com/containerd/containerd/errdefs"
	"github.com/containerd/containerd/oci"
	specs "github.com/opencontainers/runtime-spec/specs-go"
)

type ContainerConfig struct {
	Platform           string
	Runtime            string
	Snapshotter        string
	WorkspaceHost      string
	WorkspaceContainer string
	WorkspaceReadOnly  bool
	CodexHomeHost      string
	CodexHomeContainer string
	WorkingDirectory   string
	CommandLine        string
	NetworkEndpointID  string
	Terminal           bool
	TerminalWidth      uint
	TerminalHeight     uint
	MemoryLimitBytes   uint64
	CPUCount           uint64
}

type CreatedContainer struct {
	Container   containerd.Container
	SnapshotKey string
}

func (c *Client) CreateWindowsContainer(
	ctx context.Context,
	image containerd.Image,
	cfg ContainerConfig,
) (*CreatedContainer, error) {
	containerID, err := uniqueID("codex")
	if err != nil {
		return nil, err
	}
	snapshotKey := containerID + "-rootfs"

	mounts := []specs.Mount{
		windowsMappedDirectory(cfg.WorkspaceHost, cfg.WorkspaceContainer, cfg.WorkspaceReadOnly),
	}
	if cfg.CodexHomeHost != "" {
		mounts = append(mounts, windowsMappedDirectory(cfg.CodexHomeHost, cfg.CodexHomeContainer, false))
	}

	specOpts := []oci.SpecOpts{
		oci.WithDefaultSpecForPlatform(cfg.Platform),
		oci.WithImageConfig(image),
		oci.WithProcessCwd(cfg.WorkingDirectory),
		oci.WithWindowsHyperV,
		oci.WithMounts(mounts),
	}
	if cfg.CodexHomeHost != "" {
		specOpts = append(specOpts, withCodexHome(cfg.CodexHomeContainer))
		toolsDirectory := strings.TrimRight(cfg.CodexHomeContainer, `\/`) + `\hcs-tools`
		specOpts = append(specOpts, withPrependedPath(toolsDirectory))
	}
	if cfg.CommandLine != "" {
		specOpts = append(specOpts, oci.WithProcessCommandLine(cfg.CommandLine))
	}
	if cfg.NetworkEndpointID != "" {
		specOpts = append(specOpts, withWindowsNetworkEndpoint(cfg.NetworkEndpointID))
	}
	if cfg.Terminal {
		specOpts = append(specOpts, oci.WithTTY)
		specOpts = append(specOpts, oci.WithTTYSize(int(cfg.TerminalWidth), int(cfg.TerminalHeight)))
	}
	if cfg.MemoryLimitBytes > 0 {
		specOpts = append(specOpts, oci.WithMemoryLimit(cfg.MemoryLimitBytes))
	}
	if cfg.CPUCount > 0 {
		specOpts = append(specOpts, oci.WithWindowsCPUCount(cfg.CPUCount))
	}

	ctr, err := c.raw.NewContainer(
		ctx,
		containerID,
		containerd.WithImage(image),
		containerd.WithSnapshotter(cfg.Snapshotter),
		containerd.WithNewSnapshot(snapshotKey, image),
		containerd.WithNewSpec(specOpts...),
		containerd.WithRuntime(cfg.Runtime, nil),
	)
	if err != nil {
		cleanupCtx, cancel := context.WithTimeout(c.Context(context.Background()), 30*time.Second)
		defer cancel()
		cleanupErr := c.raw.SnapshotService(cfg.Snapshotter).Remove(cleanupCtx, snapshotKey)
		if cleanupErr != nil && !errdefs.IsNotFound(cleanupErr) {
			return nil, errors.Join(
				fmt.Errorf("kreiranje container objekta %s: %w", containerID, err),
				fmt.Errorf("rollback snapshota %s: %w", snapshotKey, cleanupErr),
			)
		}
		return nil, fmt.Errorf("kreiranje container objekta %s: %w", containerID, err)
	}

	return &CreatedContainer{Container: ctr, SnapshotKey: snapshotKey}, nil
}

// hcsshim v0.11.4 WCOW expects Type == "" for an ordinary mapped
// directory. "bind" is accepted in the LCOW path, but rejected by the
// WCOW setupMounts switch in internal/hcsoci/resources_wcow.go.
func windowsMappedDirectory(source, destination string, readOnly bool) specs.Mount {
	mount := specs.Mount{
		Source:      source,
		Destination: destination,
		Type:        "",
	}
	if readOnly {
		mount.Options = []string{"ro"}
	}
	return mount
}

func withCodexHome(path string) oci.SpecOpts {
	return oci.WithEnv([]string{"CODEX_HOME=" + path})
}

func withPrependedPath(directory string) oci.SpecOpts {
	return func(_ context.Context, _ oci.Client, _ *containers.Container, spec *oci.Spec) error {
		if spec.Process == nil {
			return fmt.Errorf("OCI process nije inicijalizovan")
		}
		for index, entry := range spec.Process.Env {
			name, value, found := strings.Cut(entry, "=")
			if found && strings.EqualFold(name, "PATH") {
				spec.Process.Env[index] = name + "=" + directory + ";" + value
				return nil
			}
		}
		spec.Process.Env = append(spec.Process.Env, "PATH="+directory)
		return nil
	}
}

func withWindowsNetworkEndpoint(endpointID string) oci.SpecOpts {
	return func(_ context.Context, _ oci.Client, _ *containers.Container, spec *oci.Spec) error {
		if spec.Windows == nil {
			return fmt.Errorf("Windows OCI sekcija nije inicijalizovana")
		}
		spec.Windows.Network = &specs.WindowsNetwork{
			EndpointList: []string{endpointID},
		}
		return nil
	}
}

func uniqueID(prefix string) (string, error) {
	random := make([]byte, 6)
	if _, err := rand.Read(random); err != nil {
		return "", fmt.Errorf("generisanje ID-a: %w", err)
	}
	return fmt.Sprintf("%s-%d-%s", prefix, time.Now().UTC().UnixMilli(), hex.EncodeToString(random)), nil
}
