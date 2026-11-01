// SPDX-License-Identifier: AGPL-3.0-or-later
// Execute the real eligibility path with the game's observed output-width ABI.
#include <cstring>
#include <cassert>
#include <cstdio>
#include <string>
static std::string trace;
extern "C" void cpu_rematch_debug_write(const char* p, size_t n) { trace.append(p,n); }
#define CPU_REMATCH_HOST_TEST
#define asm(x)
#include "plugin.cpp"
#undef asm
extern "C" int lookup(uintptr_t* out, const char*) { *out=0; return 1; }
extern "C" void* getRegionAddress(uint8_t) { return nullptr; }
extern "C" void A64HookFunction(void*, void*, void**) {}
extern "C" uint32_t cpu_rematch_query_memory(uintptr_t, uintptr_t*, uintptr_t*, uint32_t*) { return 1; }
static uint32_t simulated_main = 1;
static uint64_t simulated_sub = 0;
static unsigned simulated_cpus = 1;
static void mock_mode(uint32_t* main, uint32_t* sub) {
    *main = simulated_main;
    // Reverse-engineered callers provide an 8-byte submode output buffer.
    std::memcpy(sub,&simulated_sub,sizeof(simulated_sub));
}
static void mock_mode_narrow(uint32_t* main, uint32_t* sub) { *main=1; *sub=0; }
static int mock_count(void*) { return 2; }
static int mock_entry(void*, int i) { return i; }
static void* mock_info(void*, int id) { return reinterpret_cast<void*>(uintptr_t(id+1)); }
static bool mock_cpu(void* p) { return simulated_cpus == 2 || (simulated_cpus == 1 && uintptr_t(p)==2); }
static bool mock_results(void*) { return false; }
int main() {
    void* manager = reinterpret_cast<void*>(1);
    manager_slot = &manager;
    get_mode = mock_mode;
    entry_count = mock_count;
    entry_id = mock_entry;
    fighter_info = mock_info;
    is_cpu = mock_cpu;
    is_results = mock_results;
    assert(eligible_match());
    simulated_sub = 0x12345678abcdef00ull;
    assert(eligible_match());
    simulated_main = 18;
    assert(!eligible_match());
    simulated_main = 45;
    assert(!eligible_match());
    simulated_main = 0;
    assert(!eligible_match());
    simulated_main = 1; simulated_cpus = 0;
    assert(!eligible_match());
    simulated_cpus = 2;
    assert(!eligible_match());
    simulated_cpus = 1;
    get_mode = mock_mode_narrow;
    assert(eligible_match());
    std::puts("PASS: 8-byte mode output is safe; regular CPU match accepted; other modes and participant mixes rejected");
}
