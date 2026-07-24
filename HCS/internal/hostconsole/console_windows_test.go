//go:build windows

package hostconsole

import (
	"testing"

	"golang.org/x/sys/windows"
)

func TestVirtualTerminalOutputMode(t *testing.T) {
	t.Parallel()
	mode := virtualTerminalOutputMode(windows.ENABLE_WRAP_AT_EOL_OUTPUT)
	if mode&windows.ENABLE_PROCESSED_OUTPUT == 0 {
		t.Fatal("processed output nije uključen")
	}
	if mode&windows.ENABLE_VIRTUAL_TERMINAL_PROCESSING == 0 {
		t.Fatal("virtual terminal processing nije uključen")
	}
}

func TestVirtualTerminalInputMode(t *testing.T) {
	t.Parallel()
	original := uint32(
		windows.ENABLE_ECHO_INPUT |
			windows.ENABLE_LINE_INPUT |
			windows.ENABLE_PROCESSED_INPUT |
			windows.ENABLE_QUICK_EDIT_MODE,
	)
	mode := virtualTerminalInputMode(original)
	if mode&(windows.ENABLE_ECHO_INPUT|windows.ENABLE_LINE_INPUT|windows.ENABLE_PROCESSED_INPUT|windows.ENABLE_QUICK_EDIT_MODE) != 0 {
		t.Fatalf("raw input flagovi nisu isključeni: %#x", mode)
	}
	if mode&windows.ENABLE_VIRTUAL_TERMINAL_INPUT == 0 {
		t.Fatal("virtual terminal input nije uključen")
	}
}
