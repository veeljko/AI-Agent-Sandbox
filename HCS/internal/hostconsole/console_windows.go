//go:build windows

package hostconsole

import (
	"errors"
	"fmt"
	"io"

	"golang.org/x/sys/windows"
)

type fdProvider interface {
	Fd() uintptr
}

type savedMode struct {
	handle windows.Handle
	mode   uint32
}

// Session owns temporary host console modes used by an interactive container
// task. Close restores every mode even when setup only partially succeeded.
type Session struct {
	modes  []savedMode
	closed bool
}

func Enable(stdin io.Reader, stdout, stderr io.Writer) (*Session, error) {
	session := &Session{}

	stdoutHandle, stdoutMode, err := consoleMode(stdout, "stdout")
	if err != nil {
		return nil, err
	}
	session.modes = append(session.modes, savedMode{handle: stdoutHandle, mode: stdoutMode})
	if err := windows.SetConsoleMode(stdoutHandle, virtualTerminalOutputMode(stdoutMode)); err != nil {
		_ = session.Close()
		return nil, fmt.Errorf("uključivanje VT output režima za stdout: %w", err)
	}

	if stderrHandle, stderrMode, err := consoleMode(stderr, "stderr"); err == nil {
		if stderrHandle != stdoutHandle {
			session.modes = append(session.modes, savedMode{handle: stderrHandle, mode: stderrMode})
			if err := windows.SetConsoleMode(stderrHandle, virtualTerminalOutputMode(stderrMode)); err != nil {
				_ = session.Close()
				return nil, fmt.Errorf("uključivanje VT output režima za stderr: %w", err)
			}
		}
	}

	stdinHandle, stdinMode, err := consoleMode(stdin, "stdin")
	if err != nil {
		_ = session.Close()
		return nil, err
	}
	session.modes = append(session.modes, savedMode{handle: stdinHandle, mode: stdinMode})
	if err := windows.SetConsoleMode(stdinHandle, virtualTerminalInputMode(stdinMode)); err != nil {
		_ = session.Close()
		return nil, fmt.Errorf("uključivanje raw/VT input režima za stdin: %w", err)
	}

	return session, nil
}

func (s *Session) Close() error {
	if s == nil || s.closed {
		return nil
	}
	s.closed = true

	var restoreErrs []error
	for index := len(s.modes) - 1; index >= 0; index-- {
		saved := s.modes[index]
		if err := windows.SetConsoleMode(saved.handle, saved.mode); err != nil {
			restoreErrs = append(restoreErrs, err)
		}
	}
	s.modes = nil
	if err := errors.Join(restoreErrs...); err != nil {
		return fmt.Errorf("vraćanje Windows console režima: %w", err)
	}
	return nil
}

func Size(output io.Writer) (uint32, uint32, error) {
	handle, _, err := consoleMode(output, "stdout")
	if err != nil {
		return 0, 0, err
	}

	var info windows.ConsoleScreenBufferInfo
	if err := windows.GetConsoleScreenBufferInfo(handle, &info); err != nil {
		return 0, 0, fmt.Errorf("čitanje veličine Windows konzole: %w", err)
	}
	width := int32(info.Window.Right) - int32(info.Window.Left) + 1
	height := int32(info.Window.Bottom) - int32(info.Window.Top) + 1
	if width <= 0 || height <= 0 {
		return 0, 0, fmt.Errorf("Windows konzola je vratila neispravnu veličinu %dx%d", width, height)
	}
	return uint32(width), uint32(height), nil
}

func consoleMode(stream any, name string) (windows.Handle, uint32, error) {
	provider, ok := stream.(fdProvider)
	if !ok {
		return 0, 0, fmt.Errorf("%s nije Windows console stream", name)
	}
	handle := windows.Handle(provider.Fd())
	var mode uint32
	if err := windows.GetConsoleMode(handle, &mode); err != nil {
		return 0, 0, fmt.Errorf("%s nije aktivna Windows konzola: %w", name, err)
	}
	return handle, mode, nil
}

func virtualTerminalOutputMode(mode uint32) uint32 {
	return mode | windows.ENABLE_PROCESSED_OUTPUT | windows.ENABLE_VIRTUAL_TERMINAL_PROCESSING
}

func virtualTerminalInputMode(mode uint32) uint32 {
	mode &^= windows.ENABLE_ECHO_INPUT |
		windows.ENABLE_LINE_INPUT |
		windows.ENABLE_PROCESSED_INPUT |
		windows.ENABLE_MOUSE_INPUT |
		windows.ENABLE_INSERT_MODE |
		windows.ENABLE_QUICK_EDIT_MODE
	return mode |
		windows.ENABLE_EXTENDED_FLAGS |
		windows.ENABLE_WINDOW_INPUT |
		windows.ENABLE_VIRTUAL_TERMINAL_INPUT
}
