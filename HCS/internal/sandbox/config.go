package sandbox

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
	"time"

	"example.com/containerd-windows-runner/internal/policy"
)

const (
	DefaultAddress   = `\\.\pipe\containerd-containerd`
	DefaultNamespace = "codex-sandbox"
	DefaultPlatform  = "windows/amd64"
	DefaultRuntime   = "io.containerd.runhcs.v1"
	DefaultImage     = "localhost/codex-windows:ltsc2022"
)

type RunConfig struct {
	Address            string
	Namespace          string
	Platform           string
	Runtime            string
	Snapshotter        string
	Image              string
	PullImage          bool
	WorkspaceHost      string
	WorkspaceContainer string
	WorkspaceReadOnly  bool
	CodexHomeHost      string
	CodexHomeContainer string
	WorkingDirectory   string
	CommandLine        string
	NetworkName        string
	Terminal           bool
	TerminalWidth      uint
	TerminalHeight     uint
	MemoryLimitBytes   uint64
	CPUCount           uint64
	DialTimeout        time.Duration
	OperationTimeout   time.Duration
	StopTimeout        time.Duration
	CleanupTimeout     time.Duration
	LeaseLifetime      time.Duration
	Stdin              io.Reader
	Stdout             io.Writer
	Stderr             io.Writer
}

func DefaultConfig() RunConfig {
	return RunConfig{
		Address:            DefaultAddress,
		Namespace:          DefaultNamespace,
		Platform:           DefaultPlatform,
		Runtime:            DefaultRuntime,
		Image:              DefaultImage,
		PullImage:          false,
		WorkspaceHost:      `C:\CodexWorkspace`,
		WorkspaceContainer: `C:\workspace`,
		CodexHomeHost:      `C:\CodexHome`,
		CodexHomeContainer: `C:\codex-home`,
		WorkingDirectory:   `C:\workspace`,
		CommandLine:        `codex.cmd --version`,
		NetworkName:        "Default Switch",
		TerminalWidth:      120,
		TerminalHeight:     30,
		DialTimeout:        10 * time.Second,
		OperationTimeout:   8 * time.Hour,
		StopTimeout:        10 * time.Second,
		CleanupTimeout:     2 * time.Minute,
		LeaseLifetime:      9 * time.Hour,
		Stdin:              os.Stdin,
		Stdout:             os.Stdout,
		Stderr:             os.Stderr,
	}
}

func (cfg RunConfig) NormalizeAndValidate() (RunConfig, error) {
	cfg.Address = strings.TrimSpace(cfg.Address)
	cfg.Namespace = strings.TrimSpace(cfg.Namespace)
	cfg.Platform = strings.TrimSpace(cfg.Platform)
	cfg.Runtime = strings.TrimSpace(cfg.Runtime)
	cfg.Snapshotter = strings.TrimSpace(cfg.Snapshotter)
	cfg.Image = strings.TrimSpace(cfg.Image)
	cfg.NetworkName = strings.TrimSpace(cfg.NetworkName)

	workspace, err := policy.ValidateWorkspace(cfg.WorkspaceHost)
	if err != nil {
		return RunConfig{}, err
	}
	cfg.WorkspaceHost = workspace
	if err := policy.ValidateContainerPath(cfg.WorkspaceContainer); err != nil {
		return RunConfig{}, err
	}
	if err := policy.ValidateContainerPath(cfg.WorkingDirectory); err != nil {
		return RunConfig{}, fmt.Errorf("working directory: %w", err)
	}
	if !policy.ContainsPath(cfg.WorkspaceContainer, cfg.WorkingDirectory) {
		return RunConfig{}, fmt.Errorf(
			"working directory %s mora biti unutar workspace mounta %s",
			cfg.WorkingDirectory,
			cfg.WorkspaceContainer,
		)
	}

	if cfg.CodexHomeHost != "" {
		codexHome, err := policy.ValidateWorkspace(cfg.CodexHomeHost)
		if err != nil {
			return RunConfig{}, fmt.Errorf("Codex home: %w", err)
		}
		cfg.CodexHomeHost = codexHome
		if err := policy.ValidateContainerPath(cfg.CodexHomeContainer); err != nil {
			return RunConfig{}, fmt.Errorf("Codex home u containeru: %w", err)
		}
		if policy.PathsOverlap(cfg.WorkspaceHost, cfg.CodexHomeHost) {
			return RunConfig{}, fmt.Errorf("workspace i Codex home host direktorijumi ne smeju da se preklapaju")
		}
		if policy.PathsOverlap(cfg.WorkspaceContainer, cfg.CodexHomeContainer) {
			return RunConfig{}, fmt.Errorf("workspace i Codex home mount odredišta ne smeju da se preklapaju")
		}
	}

	if cfg.Address == "" || cfg.Namespace == "" || cfg.Platform == "" || cfg.Image == "" {
		return RunConfig{}, fmt.Errorf("address, namespace, platform i image su obavezni")
	}
	if cfg.Platform != DefaultPlatform {
		return RunConfig{}, fmt.Errorf("ovaj WCOW runner podržava samo %s, dobijeno %s", DefaultPlatform, cfg.Platform)
	}
	if cfg.Runtime != DefaultRuntime {
		return RunConfig{}, fmt.Errorf("ovaj runner zahteva runtime %s, dobijeno %s", DefaultRuntime, cfg.Runtime)
	}
	if _, err := exec.LookPath("containerd-shim-runhcs-v1.exe"); err != nil {
		return RunConfig{}, fmt.Errorf("containerd-shim-runhcs-v1.exe nije u PATH-u: %w", err)
	}
	if cfg.DialTimeout <= 0 || cfg.OperationTimeout <= 0 || cfg.StopTimeout <= 0 || cfg.CleanupTimeout <= 0 || cfg.LeaseLifetime <= 0 {
		return RunConfig{}, fmt.Errorf("svi timeout-i i lease lifetime moraju biti pozitivni")
	}
	if cfg.Terminal && (cfg.TerminalWidth == 0 || cfg.TerminalHeight == 0) {
		return RunConfig{}, fmt.Errorf("TTY širina i visina moraju biti veće od nule")
	}
	return cfg, nil
}
