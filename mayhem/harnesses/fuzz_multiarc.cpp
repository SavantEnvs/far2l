// Fuzz one multiarc format reader through the second-level plugin interface that
// MultiArc itself uses (ArcPlg.cpp / arcread.cpp): IsArchive on the file's leading
// bytes, then OpenArchive by name, GetArcItem until it stops, CloseArchive.
// The format is chosen at compile time with -DFMT=<PREFIX> (ZIP, SEVENZ, RAR, ...).
#include <windows.h>
#include <utils.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <farplug-mb.h>
using namespace oldfar;
#include "fmt.hpp"
#include "formats/all.h"

#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)
#define F(name) CAT(FMT, name)

// Defined by MultiArc.cpp (the plugin host, not linked here); only used to locate
// helper executables for extraction, which listing never does.
std::string gMultiArcPluginPath;

#ifndef FMT_EXT
#define FMT_EXT ".bin"
#endif

#ifdef FMT_HAS_SETFARINFO
static int WINAPI StubMessage(INT_PTR, DWORD, const char *, const char *const *, int, int) { return -1; }
static const char *WINAPI StubGetMsg(INT_PTR, int) { return ""; }
static int WINAPI StubInputBox(const char *, const char *, const char *, const char *, char *, int,
		const char *, DWORD) { return FALSE; }
#endif

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
#ifdef FMT_HAS_SETFARINFO
	// Same as MultiArc.cpp SetStartupInfo + ArcPlugins::AddPluginItem: formats get
	// malloc/free via FSF->Reserved and Far's message/input callbacks (stubbed: no UI).
	static struct FarStandardFunctions fsf;
	static struct PluginStartupInfo psi;
	memset(&fsf, 0, sizeof(fsf));
	memset(&psi, 0, sizeof(psi));
	fsf.Reserved[0] = (DWORD_PTR)malloc;
	fsf.Reserved[1] = (DWORD_PTR)free;
	psi.FSF = &fsf;
	psi.Message = (FARAPIMESSAGE)StubMessage;
	psi.GetMsg = (FARAPIGETMSG)StubGetMsg;
	psi.InputBox = (FARAPIINPUTBOX)StubInputBox;
	F(_SetFarInfo)(&psi);
#endif
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 0 || size > 0x7fffffff)
		return -1;
	const char *tmpdir = getenv("TMPDIR");
	if (!tmpdir || !*tmpdir)
		tmpdir = "/tmp";
	std::string path = std::string(tmpdir) + "/far2l_fuzz_XXXXXX" FMT_EXT;
	int fd = mkstemps(&path[0], (int)strlen(FMT_EXT));
	if (fd == -1)
		return 0;
	size_t off = 0;
	while (off < size) {
		ssize_t w = write(fd, data + off, size - off);
		if (w <= 0)
			break;
		off += (size_t)w;
	}
	close(fd);

	if (F(_IsArchive)(path.c_str(), data, (int)size)) {
		int type = 0;
		if (F(_OpenArchive)(path.c_str(), &type, true)) {
			for (;;) {
				ArcItemInfo item;
				if (F(_GetArcItem)(&item) != GETARC_SUCCESS)
					break;
			}
			ArcInfo info{};
			F(_CloseArchive)(&info);
		}
	}
	unlink(path.c_str());
	return 0;
}
