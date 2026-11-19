// Globals the far2l executable (far2l/src/global.cpp) exports to WinPort/utils code.
// The harnesses link WinPort/utils without the far2l executable, so define them here
// with far2l's own defaults.
extern "C" __attribute__((visibility("default"))) bool g_far2l_use_vs16 = true;
