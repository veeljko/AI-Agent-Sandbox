package policy

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// ValidateWorkspace rejects drive roots and broad/sensitive host directories.
// It returns an absolute path with symlinks/junctions resolved where Windows
// exposes them through filepath.EvalSymlinks.
func ValidateWorkspace(path string) (string, error) {
	if path == "" {
		return "", fmt.Errorf("host workspace putanja je prazna")
	}
	abs, err := filepath.Abs(path)
	if err != nil {
		return "", fmt.Errorf("apsolutna workspace putanja: %w", err)
	}
	abs = filepath.Clean(abs)
	if resolved, err := filepath.EvalSymlinks(abs); err == nil {
		abs = filepath.Clean(resolved)
	}

	info, err := os.Stat(abs)
	if err != nil {
		return "", fmt.Errorf("workspace %s: %w", abs, err)
	}
	if !info.IsDir() {
		return "", fmt.Errorf("workspace %s nije direktorijum", abs)
	}

	volume := filepath.VolumeName(abs)
	if volume == "" {
		return "", fmt.Errorf("workspace mora biti apsolutna Windows putanja: %s", abs)
	}
	root := filepath.Clean(volume + `\`)
	if SamePath(abs, root) {
		return "", fmt.Errorf("zabranjeno je mountovati koren diska %s", root)
	}

	protected := []string{
		filepath.Clean(volume + `\Windows`),
		filepath.Clean(volume + `\Program Files`),
		filepath.Clean(volume + `\Program Files (x86)`),
		filepath.Clean(volume + `\ProgramData`),
		filepath.Clean(volume + `\Users`),
	}
	if profile := os.Getenv("USERPROFILE"); profile != "" {
		protected = append(protected, filepath.Clean(profile))
	}
	for _, forbidden := range protected {
		if SamePath(abs, forbidden) {
			return "", fmt.Errorf("workspace je preširok ili osetljiv host direktorijum: %s", abs)
		}
	}
	return abs, nil
}

func ValidateContainerPath(path string) error {
	if path == "" || !filepath.IsAbs(path) {
		return fmt.Errorf("container workspace mora biti apsolutna Windows putanja: %q", path)
	}
	clean := filepath.Clean(path)
	root := filepath.Clean(filepath.VolumeName(clean) + `\`)
	if SamePath(clean, root) {
		return fmt.Errorf("container workspace ne sme biti koren diska: %s", clean)
	}
	return nil
}

func SamePath(a, b string) bool {
	return strings.EqualFold(filepath.Clean(a), filepath.Clean(b))
}

// ContainsPath reports whether candidate is root itself or one of its
// descendants. Both values are expected to be absolute paths.
func ContainsPath(root, candidate string) bool {
	root = filepath.Clean(root)
	candidate = filepath.Clean(candidate)
	if SamePath(root, candidate) {
		return true
	}

	relative, err := filepath.Rel(root, candidate)
	if err != nil {
		return false
	}
	relative = filepath.Clean(relative)
	parentPrefix := ".." + string(filepath.Separator)
	return relative != ".." && !strings.HasPrefix(relative, parentPrefix)
}

func PathsOverlap(first, second string) bool {
	return ContainsPath(first, second) || ContainsPath(second, first)
}
