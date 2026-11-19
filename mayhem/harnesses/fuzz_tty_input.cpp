// Fuzz far2l's TTY backend input decoding: the byte stream a terminal sends to far2l
// (escape sequences, mouse reports, far2l/kitty/iTerm2/win32-input extensions, UTF-8).
// Drives the public TTYInput class exactly as TTYBackend's reader thread does:
// OnInput() for each chunk read from the tty, OnIdleExpired() when input pauses.
#include <string.h>
#include <memory>
#include "Backend.h"
#include "ConsoleInput.h"
#include "TTY/TTYInput.h"

struct NullHandler : ITTYInputSpecialSequenceHandler
{
	void OnUsingExtension(char) override {}
	void OnInspectKeyEvent(KEY_EVENT_RECORD &) override {}
	void OnFocusChange(bool) override {}
	void OnFar2lEvent(StackSerializer &) override {}
	void OnFar2lReply(StackSerializer &) override {}
	void OnKittyGraphicsResponse(const std::string &) override {}
	void OnStatusResponse(char) override {}
	void OnCursorShape(int) override {}
	void OnInputBroken() override {}
	void OnGetCellSize(unsigned int, unsigned int) override {}
};

static ConsoleInput *s_con_in;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	s_con_in = new ConsoleInput;
	g_winport_con_in = s_con_in;
	return 0;
}

// Input layout: repeated records of [len byte][len bytes], each record one read() from
// the tty; a zero length byte means "idle timer expired" (TTYInput::OnIdleExpired).
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	_iterm2_cmd_ts = 0;
	_iterm2_cmd_state = false;
	NullHandler handler;
	{
		TTYInput input(&handler);
		size_t i = 0;
		while (i < size) {
			size_t n = data[i++];
			if (n == 0) {
				input.OnIdleExpired();
				continue;
			}
			if (n > size - i)
				n = size - i;
			input.OnInput((const char *)data + i, n);
			i += n;
		}
		input.OnIdleExpired();
	}
	s_con_in->Flush();
	return 0;
}
