// Fuzz far2l's own regular expression engine (far2l/src/mix/RegExp.cpp): the compiler and
// matcher behind editor/viewer search and replace, find-file, file masks and highlighting.
// Input: [option byte][pattern UTF-8] '\0' [subject UTF-8]. Both are decoded to wide
// strings the way far2l does (MB2Wide), then RegExp::Compile + Optimize + Search/Match.
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <memory>
#include "headers.hpp"
#include <WideMB.h>
#include "RegExp.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 1)
		return -1;
	const int opts = data[0] & (OP_IGNORECASE | OP_SINGLELINE | OP_MULTILINE | OP_XTENDEDSYNTAX |
			OP_PERLSTYLE | OP_STRICT | OP_CPPMODE);
	const char *rest = (const char *)data + 1;
	const size_t rest_len = size - 1;
	const char *sep = (const char *)memchr(rest, 0, rest_len);
	const size_t pat_len = sep ? (size_t)(sep - rest) : rest_len;
	std::wstring pattern, subject;
	MB2Wide(rest, pat_len, pattern);
	if (sep)
		MB2Wide(sep + 1, rest_len - pat_len - 1, subject);

	RegExp re;
	if (!re.Compile(ReStringView(pattern.c_str(), pattern.size()), opts))
		return 0;
	re.Optimize();
	std::vector<RegExpMatch> m(re.GetBracketsCount() + 1);
	int count = (int)m.size();
	MatchHash hm;
	re.Search(ReStringView(subject.c_str(), subject.size()), m.data(), count, &hm);
	count = (int)m.size();
	re.Match(ReStringView(subject.c_str(), subject.size()), m.data(), count, &hm);
	return 0;
}
