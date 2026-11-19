// Fuzz NetRocks' parsers for FTP server directory listings (server-controlled data):
// LIST output (ftpparse, FTPParseLIST.cpp) and MLSD/MLST facts (ParseMLsxLine,
// FTPParseMLST.cpp). The input is split into lines as ProtocolFTP does with the data
// connection, and every line goes through both parsers.
#include <stdint.h>
#include <string.h>
#include <string>
#include <exception>
#include "Protocol/FTP/FTPParseLIST.h"
#include "Protocol/FTP/FTPParseMLST.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const char *p = (const char *)data, *end = p + size;
	while (p < end) {
		const char *eol = (const char *)memchr(p, '\n', end - p);
		const char *le = eol ? eol : end;
		const char *lt = (le > p && le[-1] == '\r') ? le - 1 : le;
		// both parsers expect a NUL-terminated copy (ProtocolFTP passes std::string data)
		std::string line(p, lt - p);

		struct ftpparse fp;
		memset(&fp, 0, sizeof(fp));
		ftpparse(&fp, line.c_str(), (int)line.size());

		FileInformation fi;
		uid_t uid = 0;
		gid_t gid = 0;
		std::string name, lnkto;
		try {
			ParseMLsxLine(line.c_str(), line.c_str() + line.size(), fi, &uid, &gid, &name, &lnkto);
		} catch (std::exception &) {
			// ProtocolFTP reports parse failures as exceptions to the session; not a crash
		}
		p = eol ? eol + 1 : end;
	}
	return 0;
}
