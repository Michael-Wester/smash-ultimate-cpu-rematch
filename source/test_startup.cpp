// SPDX-License-Identifier: AGPL-3.0-or-later
// Exercises actual startup with the signature results from the user's v02 log.
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cassert>
static std::string trace;
extern "C" void cpu_rematch_debug_write(const char* data, size_t size) { trace.append(data,size); }
#define CPU_REMATCH_HOST_TEST
#define asm(x)
#include "plugin.cpp"
#undef asm
static std::vector<uint32_t> game(0x6000000 / 4);
static bool installed = false;
static uint32_t simulated_permissions = 7;
static const char* game_version = "13.0.3";
static void fake_version(void* out) { std::memcpy(out,game_version,std::strlen(game_version)+1); }
static void unused_stub() {}
static void original_stub(uint64_t) {}
extern "C" uint32_t cpu_rematch_query_memory(uintptr_t at, uintptr_t* base, uintptr_t* size, uint32_t* permissions) {
    *base = uintptr_t(game.data()); *size = game.size()*4; *permissions = simulated_permissions;
    return at >= *base && at < *base + *size ? 0 : 1;
}
extern "C" int lookup(uintptr_t* out, const char* name) {
    *out = 0;
    if (!std::strcmp(name,"_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE")) *out = uintptr_t(fake_version);
    // Filesystem exports deliberately absent: diagnostics must still reach trace.
    else if (!std::strncmp(name,"_ZN2nn2fs",9)) return 1;
    else *out = uintptr_t(unused_stub);
    return 0;
}
extern "C" void* getRegionAddress(uint8_t region) {
    static const uintptr_t offsets[] = {0,0x3398000,0x4f17000};
    return reinterpret_cast<uint8_t*>(game.data())+offsets[region];
}
extern "C" void A64HookFunction(void* address, void*, void** original) {
    assert(uintptr_t(address)-uintptr_t(game.data()) == 0x135b810);
    installed = true; *original = reinterpret_cast<void*>(original_stub);
}
int main() {
    std::memcpy(reinterpret_cast<uint8_t*>(game.data())+0x135b810,FRAME_PATTERN,sizeof(FRAME_PATTERN));
    std::memcpy(reinterpret_cast<uint8_t*>(game.data())+0x1740584,MODE_PATTERN,sizeof(MODE_PATTERN));
    game[0x1740580/4] = 0x94000cbc; // BL -> module + 0x1743870
    // No STATE_PATTERN, reproducing the user's failed v02 initialization.
    plugin_main();
    if (!installed) {
        std::puts("FAIL: 13.0.3 startup aborts when battle-state signature is unavailable");
        return 1;
    }
    assert(state_root-uintptr_t(game.data()) == 0x52c3760);
    std::puts("PASS: reported 13.0.3 layout reaches hook installation without state signature");
    installed = false; game_version = "13.0.4"; trace.clear();
    plugin_main();
    assert(!installed);
    assert(trace.find("game must be exactly 13.0.3") != std::string::npos);
    std::puts("PASS: other game versions remain disabled");
    game_version = "13.0.3"; trace.clear();
    game[0x1740580/4] = 0x94000cbd; // Different branch target: reject this layout.
    plugin_main();
    assert(!installed);
    assert(trace.find("fallback layout does not match") != std::string::npos);
    std::puts("PASS: mismatched executable layout does not select fixed pointer");
    game[0x1740580/4] = 0x94000cbc;
    simulated_permissions = 1; trace.clear();
    plugin_main();
    assert(!installed);
    assert(trace.find("slot is not mapped read/write") != std::string::npos);
    std::puts("PASS: invalid memory permissions prevent hook installation");
    simulated_permissions = 7;
    assert(!mapped(uintptr_t(game.data())+game.size()*4-4,8));
    assert(!mapped(0,8));
    assert(!request_restart()); // A null chain cannot reach the vtable call.
    std::puts("PASS: invalid range and null retry-chain pointers are rejected");
}
