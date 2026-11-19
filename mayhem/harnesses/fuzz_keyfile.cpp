// Fuzz far2l's INI-style key file reader (utils KeyFileHelper), used for every far2l
// config, bookmarks, NetRocks site lists and multiarc custom format definitions.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <KeyFileHelper.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const char *tmpdir = getenv("TMPDIR");
	if (!tmpdir || !*tmpdir)
		tmpdir = "/tmp";
	std::string path = std::string(tmpdir) + "/far2l_kf_XXXXXX.ini";
	int fd = mkstemps(&path[0], 4);
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

	for (int ci = 0; ci < 2; ++ci) {
		KeyFileReadHelper kf(path, nullptr, ci != 0);
		for (const auto &sec : kf.EnumSections()) {
			kf.EnumSectionsAt(sec, true);
			for (const auto &key : kf.EnumKeys(sec)) {
				kf.GetString(sec, key, "");
				kf.GetString(sec, key, L"");
				kf.GetInt(sec, key, 0);
				kf.GetULL(sec, key, 0);
				std::vector<unsigned char> bytes;
				kf.GetBytes(bytes, sec, key);
				unsigned char small[16];
				kf.GetBytes(small, sizeof(small), sec, key);
			}
		}
	}
	unlink(path.c_str());
	return 0;
}
