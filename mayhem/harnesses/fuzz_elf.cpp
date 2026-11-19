// Fuzz the inside plugin's ELF reader (plugins/inside/src/elf/ELFInfo.h): the ELF header,
// program header and section header tables of a file the user opens with Ctrl+PgDn.
// Mirrors inside.cpp OpenFilePlugin: HasElvenEars() accepts the file's leading bytes, then
// PluginImplELF's constructor runs FillELFInfo<> for the class/endianness bytes.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include "elf/ELFInfo.h"

// inside.cpp: #define MINIMAL_LEN 0x34 + HasElvenEars()
static bool HasElvenEars(const unsigned char *Data, size_t DataSize)
{
	if (DataSize < 0x34 || Data[0] != 0x7F || Data[1] != 'E' || Data[2] != 'L' || Data[3] != 'F')
		return false;
	if (Data[4] != 1 && Data[4] != 2)
		return false;
	if (Data[5] != 0 && Data[5] != 1 && Data[5] != 2)
		return false;
	if (Data[6] != 1)
		return false;
	return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (!HasElvenEars(data, size))
		return -1;
	const char *tmpdir = getenv("TMPDIR");
	if (!tmpdir || !*tmpdir)
		tmpdir = "/tmp";
	std::string path = std::string(tmpdir) + "/far2l_elf_XXXXXX";
	int fd = mkstemp(&path[0]);
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

	ELFInfo info;
	const uint8_t bitness = data[4], endianness = data[5];
	if (bitness == 2) {
		if (endianness == 2)
			FillELFInfo<ELF_EndiannessBig, Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>(info, path.c_str());
		else
			FillELFInfo<ELF_EndiannessLittle, Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>(info, path.c_str());
	} else {
		if (endianness == 2)
			FillELFInfo<ELF_EndiannessBig, Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>(info, path.c_str());
		else
			FillELFInfo<ELF_EndiannessLittle, Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>(info, path.c_str());
	}
	unlink(path.c_str());
	return 0;
}
