package containerdclient

import (
	"context"
	"testing"

	"github.com/containerd/containerd/oci"
	specs "github.com/opencontainers/runtime-spec/specs-go"
)

func TestByteCount(t *testing.T) {
	t.Parallel()
	if got := byteCount(2048); got != "2.0 KiB" {
		t.Fatalf("byteCount(2048) = %q", got)
	}
}

func TestWindowsMappedDirectoryReadOnly(t *testing.T) {
	t.Parallel()
	mount := windowsMappedDirectory(`C:\host`, `C:\container`, true)
	if mount.Type != "" {
		t.Fatalf("mount type = %q, want empty WCOW mapped-directory type", mount.Type)
	}
	if len(mount.Options) != 1 || mount.Options[0] != "ro" {
		t.Fatalf("mount options = %v, want [ro]", mount.Options)
	}
}

func TestWithCodexHomeOverridesImageDefault(t *testing.T) {
	t.Parallel()
	spec := &oci.Spec{
		Process: &specs.Process{
			Env: []string{`CODEX_HOME=C:\codex-home`},
		},
	}
	want := `D:\state\codex-home`
	if err := withCodexHome(want)(context.Background(), nil, nil, spec); err != nil {
		t.Fatal(err)
	}
	if len(spec.Process.Env) != 1 || spec.Process.Env[0] != "CODEX_HOME="+want {
		t.Fatalf("process environment = %#v", spec.Process.Env)
	}
}

func TestWithPrependedPathPreservesImagePath(t *testing.T) {
	t.Parallel()
	spec := &oci.Spec{
		Process: &specs.Process{
			Env: []string{`Path=C:\tools\node;C:\Windows`},
		},
	}
	if err := withPrependedPath(`C:\codex-home\hcs-tools`)(context.Background(), nil, nil, spec); err != nil {
		t.Fatal(err)
	}
	want := `Path=C:\codex-home\hcs-tools;C:\tools\node;C:\Windows`
	if len(spec.Process.Env) != 1 || spec.Process.Env[0] != want {
		t.Fatalf("process environment = %#v, want %q", spec.Process.Env, want)
	}
}

func TestWithWindowsNetworkEndpoint(t *testing.T) {
	t.Parallel()
	spec := &oci.Spec{Windows: &specs.Windows{}}
	if err := withWindowsNetworkEndpoint("endpoint-1")(context.Background(), nil, nil, spec); err != nil {
		t.Fatal(err)
	}
	if spec.Windows.Network == nil || len(spec.Windows.Network.EndpointList) != 1 || spec.Windows.Network.EndpointList[0] != "endpoint-1" {
		t.Fatalf("Windows network = %#v", spec.Windows.Network)
	}
}
