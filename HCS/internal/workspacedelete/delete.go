package workspacedelete

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func DeleteFile(workspaceRoot, target string) error {
	root, err := filepath.Abs(workspaceRoot)
	if err != nil {
		return fmt.Errorf("resolve workspace root: %w", err)
	}
	root = filepath.Clean(root)

	absoluteTarget, err := filepath.Abs(target)
	if err != nil {
		return fmt.Errorf("resolve target: %w", err)
	}
	absoluteTarget = filepath.Clean(absoluteTarget)

	if samePath(root, absoluteTarget) || !containsPath(root, absoluteTarget) {
		return fmt.Errorf("refusing to delete path outside workspace %s: %s", root, absoluteTarget)
	}
	if err := rejectSymlinkParents(root, absoluteTarget); err != nil {
		return err
	}

	info, err := os.Lstat(absoluteTarget)
	if err != nil {
		return fmt.Errorf("inspect target %s: %w", absoluteTarget, err)
	}
	if !info.Mode().IsRegular() {
		return fmt.Errorf("refusing to delete non-regular file: %s", absoluteTarget)
	}

	if err := os.Remove(absoluteTarget); err != nil {
		return fmt.Errorf("delete file %s: %w", absoluteTarget, err)
	}
	return nil
}

func rejectSymlinkParents(root, target string) error {
	relative, err := filepath.Rel(root, target)
	if err != nil {
		return fmt.Errorf("resolve target relative to workspace: %w", err)
	}
	parts := strings.FieldsFunc(relative, func(character rune) bool {
		return character == '\\' || character == '/'
	})
	parentParts := parts
	if len(parentParts) > 0 {
		parentParts = parentParts[:len(parentParts)-1]
	}
	current := root
	for _, part := range parentParts {
		current = filepath.Join(current, part)
		info, err := os.Lstat(current)
		if err != nil {
			return fmt.Errorf("inspect target parent %s: %w", current, err)
		}
		if info.Mode()&os.ModeSymlink != 0 {
			return fmt.Errorf("refusing to traverse symlinked workspace path: %s", current)
		}
	}
	return nil
}

func containsPath(root, candidate string) bool {
	relative, err := filepath.Rel(root, candidate)
	if err != nil {
		return false
	}
	relative = filepath.Clean(relative)
	parentPrefix := ".." + string(filepath.Separator)
	return relative != ".." && !strings.HasPrefix(relative, parentPrefix)
}

func samePath(first, second string) bool {
	return strings.EqualFold(filepath.Clean(first), filepath.Clean(second))
}
