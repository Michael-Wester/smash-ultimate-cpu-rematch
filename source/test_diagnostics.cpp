// SPDX-License-Identifier: AGPL-3.0-or-later
// This tests the actual plugin's startup diagnostic path, without a game runtime.
#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>
static std::string debug_output;
static std::string file_output;
static bool enable_fs = false;
static bool file_exists = false;
static unsigned closed = 0;
extern "C" void cpu_rematch_debug_write(const char* text, size_t size) {
    debug_output.append(text, size);
}
#define CPU_REMATCH_HOST_TEST
#define asm(x)
#include "plugin.cpp"
#undef asm
static uint32_t mock_open(LogHandle* handle, const char* path, int mode) {
    assert(std::strcmp(path, "sd:/cpu_rematch.log") == 0);
    assert(mode == 6);
    if (!file_exists) return 1;
    handle->handle = reinterpret_cast<void*>(1);
    return 0;
}
static uint32_t mock_create(const char*, int64_t size) {
    assert(size == 0); file_exists = true; file_output.clear(); return 0;
}
static uint32_t mock_resize(LogHandle, int64_t size) {
    assert(size == 0); file_output.clear(); return 0;
}
static uint32_t mock_size(int64_t* size, LogHandle) {
    *size = int64_t(file_output.size()); return 0;
}
static uint32_t mock_write(LogHandle, int64_t offset, const void* data, uint64_t size,
                           const LogWriteOption* option) {
    assert(offset == int64_t(file_output.size()));
    assert(option->flags == 1);
    file_output.append(static_cast<const char*>(data), size); return 0;
}
static void mock_close(LogHandle) { ++closed; }
extern "C" int lookup(uintptr_t* out, const char* name) {
    *out = 0;
    if (!enable_fs) return 1;
    if (!std::strcmp(name, "_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci")) *out = uintptr_t(mock_open);
    if (!std::strcmp(name, "_ZN2nn2fs10CreateFileEPKcl")) *out = uintptr_t(mock_create);
    if (!std::strcmp(name, "_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl")) *out = uintptr_t(mock_resize);
    if (!std::strcmp(name, "_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE")) *out = uintptr_t(mock_size);
    if (!std::strcmp(name, "_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE")) *out = uintptr_t(mock_write);
    if (!std::strcmp(name, "_ZN2nn2fs9CloseFileENS0_10FileHandleE")) *out = uintptr_t(mock_close);
    return *out ? 0 : 1;
}
extern "C" void* getRegionAddress(uint8_t) { return nullptr; }
extern "C" void A64HookFunction(void*, void*, void**) {}
extern "C" uint32_t cpu_rematch_query_memory(uintptr_t, uintptr_t*, uintptr_t*, uint32_t*) { return 1; }
int main() {
    plugin_main();
    if (debug_output.find("Disabled: game version function missing.") == std::string::npos) {
        std::puts("FAIL: startup failure is invisible when libc file symbols are unavailable");
        return 1;
    }
    std::puts("PASS: startup failure remains visible without libc or SDK file symbols");
    enable_fs = true;
    debug_output.clear();
    plugin_main();
    assert(file_exists);
    assert(file_output == debug_output);
    assert(file_output.find("Disabled: game version function missing.") != std::string::npos);
    assert(closed == 2);
    log_value("test", 0xc8);
    assert(file_output == debug_output);
    assert(file_output.find("test=0x00000000000000c8") != std::string::npos);
    assert(closed == 3);
    std::puts("PASS: native filesystem creates, truncates, appends, flushes and closes log");
}
