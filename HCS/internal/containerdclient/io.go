package containerdclient

import (
	"io"

	"github.com/containerd/containerd/cio"
)

func NewIOCreator(stdin io.Reader, stdout, stderr io.Writer, terminal bool) cio.Creator {
	opts := []cio.Opt{cio.WithStreams(stdin, stdout, stderr)}
	if terminal {
		opts = append(opts, cio.WithTerminal)
	}
	return cio.NewCreator(opts...)
}
