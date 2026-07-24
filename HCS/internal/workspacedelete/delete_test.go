package workspacedelete

import (
	"os"
	"path/filepath"
	"testing"
)

func TestDeleteFileDeletesRegularFileInsideWorkspace(t *testing.T) {
	t.Parallel()
	root := t.TempDir()
	target := filepath.Join(root, "nested", "test.txt")
	if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(target, []byte("test"), 0o644); err != nil {
		t.Fatal(err)
	}

	if err := DeleteFile(root, target); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(target); !os.IsNotExist(err) {
		t.Fatalf("target still exists or stat failed unexpectedly: %v", err)
	}
}

func TestDeleteFileRejectsPathOutsideWorkspace(t *testing.T) {
	t.Parallel()
	parent := t.TempDir()
	root := filepath.Join(parent, "workspace")
	if err := os.Mkdir(root, 0o755); err != nil {
		t.Fatal(err)
	}
	target := filepath.Join(parent, "outside.txt")
	if err := os.WriteFile(target, []byte("keep"), 0o644); err != nil {
		t.Fatal(err)
	}

	if err := DeleteFile(root, target); err == nil {
		t.Fatal("expected outside-workspace deletion to be rejected")
	}
	if _, err := os.Stat(target); err != nil {
		t.Fatalf("outside file was changed: %v", err)
	}
}

func TestDeleteFileRejectsDirectory(t *testing.T) {
	t.Parallel()
	root := t.TempDir()
	target := filepath.Join(root, "directory")
	if err := os.Mkdir(target, 0o755); err != nil {
		t.Fatal(err)
	}

	if err := DeleteFile(root, target); err == nil {
		t.Fatal("expected directory deletion to be rejected")
	}
	if info, err := os.Stat(target); err != nil || !info.IsDir() {
		t.Fatalf("directory was changed: info=%v err=%v", info, err)
	}
}

func TestDeleteFileRejectsSymlinkedParent(t *testing.T) {
	t.Parallel()
	parent := t.TempDir()
	root := filepath.Join(parent, "workspace")
	outside := filepath.Join(parent, "outside")
	if err := os.Mkdir(root, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(outside, 0o755); err != nil {
		t.Fatal(err)
	}
	target := filepath.Join(outside, "keep.txt")
	if err := os.WriteFile(target, []byte("keep"), 0o644); err != nil {
		t.Fatal(err)
	}
	link := filepath.Join(root, "link")
	if err := os.Symlink(outside, link); err != nil {
		t.Skipf("symlink creation is unavailable: %v", err)
	}

	if err := DeleteFile(root, filepath.Join(link, "keep.txt")); err == nil {
		t.Fatal("expected symlink traversal to be rejected")
	}
	if _, err := os.Stat(target); err != nil {
		t.Fatalf("outside file was changed: %v", err)
	}
}
