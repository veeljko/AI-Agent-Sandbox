//go:build !windows

package hostconsole

import (
	"fmt"
	"io"
)

type Session struct{}

func Enable(stdin io.Reader, stdout, stderr io.Writer) (*Session, error) {
	return nil, fmt.Errorf("interaktivni WCOW TTY zahteva Windows host konzolu")
}

func (s *Session) Close() error {
	return nil
}

func Size(output io.Writer) (uint32, uint32, error) {
	return 0, 0, fmt.Errorf("interaktivni WCOW TTY zahteva Windows host konzolu")
}
