package main

import (
	"testing"

	"example.com/containerd-windows-runner/internal/sandbox"
)

func TestParseConfigDefaultsToLocalCodexImage(t *testing.T) {
	cfg, err := parseConfig(nil)
	if err != nil {
		t.Fatal(err)
	}
	if cfg.Image != sandbox.DefaultImage {
		t.Fatalf("image = %q, want %q", cfg.Image, sandbox.DefaultImage)
	}
	if cfg.PullImage {
		t.Fatal("local Codex image must not be pulled by default")
	}
	if cfg.CommandLine != `codex.cmd --version` {
		t.Fatalf("command line = %q", cfg.CommandLine)
	}
}

func TestParseConfigOverridesWorkspaceAndCommand(t *testing.T) {
	cfg, err := parseConfig([]string{
		"-workspace", `D:\src\project`,
		"-workspace-container", `C:\WorkingDirectory`,
		"-workdir", `C:\WorkingDirectory`,
		"-command-line", `codex.cmd exec "pregledaj projekat"`,
		"-network", "",
		"-tty",
	})
	if err != nil {
		t.Fatal(err)
	}
	if cfg.WorkspaceHost != `D:\src\project` {
		t.Fatalf("workspace = %q", cfg.WorkspaceHost)
	}
	if cfg.WorkspaceContainer != `C:\WorkingDirectory` {
		t.Fatalf("container workspace = %q", cfg.WorkspaceContainer)
	}
	if cfg.WorkingDirectory != `C:\WorkingDirectory` {
		t.Fatalf("working directory = %q", cfg.WorkingDirectory)
	}
	if cfg.NetworkName != "" {
		t.Fatalf("network = %q, want disabled", cfg.NetworkName)
	}
	if !cfg.Terminal {
		t.Fatal("TTY override was not applied")
	}
}
