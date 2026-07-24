package policy

import (
	"path/filepath"
	"testing"
)

func TestValidateWorkspaceAcceptsDirectory(t *testing.T) {
	t.Parallel()
	want, err := filepath.Abs(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	got, err := ValidateWorkspace(want)
	if err != nil {
		t.Fatalf("ValidateWorkspace: %v", err)
	}
	if !SamePath(got, want) {
		t.Fatalf("ValidateWorkspace = %q, want %q", got, want)
	}
}

func TestValidateContainerPathRejectsDriveRoot(t *testing.T) {
	t.Parallel()
	if err := ValidateContainerPath(`C:\`); err == nil {
		t.Fatal("expected drive root to be rejected")
	}
}

func TestContainsPath(t *testing.T) {
	t.Parallel()
	root := `C:\workspace`
	if !ContainsPath(root, `C:\workspace`) {
		t.Fatal("root must contain itself")
	}
	if !ContainsPath(root, `C:\workspace\src\main.cpp`) {
		t.Fatal("root must contain a descendant")
	}
	if ContainsPath(root, `C:\workspace-other\main.cpp`) {
		t.Fatal("sibling path must not be treated as a descendant")
	}
}

func TestPathsOverlap(t *testing.T) {
	t.Parallel()
	if !PathsOverlap(`C:\workspace`, `C:\workspace\.codex`) {
		t.Fatal("nested paths must overlap")
	}
	if PathsOverlap(`C:\workspace`, `C:\codex-home`) {
		t.Fatal("separate paths must not overlap")
	}
}
