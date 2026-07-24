package main

import (
	"fmt"
	"os"

	"example.com/containerd-windows-runner/internal/workspacedelete"
)

const workspaceRoot = `C:\WorkingDirectory`

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: workspace-delete.exe ABSOLUTE_FILE_PATH")
		os.Exit(2)
	}
	if err := workspacedelete.DeleteFile(workspaceRoot, os.Args[1]); err != nil {
		fmt.Fprintf(os.Stderr, "workspace-delete: %v\n", err)
		os.Exit(1)
	}
}
