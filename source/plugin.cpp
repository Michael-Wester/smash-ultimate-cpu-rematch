// SPDX-License-Identifier: AGPL-3.0-or-later
// CPU Rematch, experimental build, 2026-10-31.
// Match restart and instruction signatures derived from HewDraw Remix.
// See ../CREDITS.md for upstream sources and license information.
#include <stdint.h>
#include <stddef.h>
#include "gate.hpp"

extern "C" void* getRegionAddress(uint8_t);
extern "C" void A64HookFunction(void*, void*, void**);
extern "C" int lookup(uintptr_t*, const char*)
    asm("_ZN2nn2ro12LookupSymbolEPmPKc");

namespace {
using U8 = uint8_t;
using U32 = uint32_t;
using U64 = uint64_t;
using Addr = uintptr_t;
using Frame = void (*)(U64);
using Mode = void (*)(U32*, U32*);
using Count = int (*)(void*);
using Entry = int (*)(void*, int);
using Info = void* (*)(void*, int);
using Flag = bool (*)(void*);
using Ready = bool (*)();

struct NpadState {
    U64 sample, buttons;
    int32_t lx, ly, rx, ry;
    U32 attributes, padding;
};
static_assert(sizeof(NpadState) == 40, "HID ABI");
using NpadRead = void (*)(NpadState*, const U32*);
using NpadStyle = U32 (*)(const U32*);

static Frame original_frame;
static Mode get_mode;
static Count entry_count;
static Entry entry_id;
static Info fighter_info;
static Flag is_cpu, is_results;
static Ready ready_go;
static NpadRead full_key, joy_dual, handheld;
static NpadStyle controller_style;
static void** manager_slot;
static Addr state_root, text_start, ro_start, data_start, main_image_end;
static RematchGate gate;

template<class T> bool resolve(T& out, const char* name) {
    Addr addr = 0;
    if (lookup(&addr, name) != 0 || !addr) return false;
    out = reinterpret_cast<T>(addr);
    return true;
}

static bool equal(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

#include "diagnostics.hpp"
#include "memory.hpp"

// Match only aligned instructions, require exactly one occurrence.
template<size_t N> static Addr unique(const U32 (&pattern)[N]) {
    Addr found = 0;
    for (Addr at = text_start; at + sizeof(pattern) <= ro_start; at += 4) {
        bool same = true;
        for (size_t i = 0; i < N; ++i)
            if (reinterpret_cast<const U32*>(at)[i] != pattern[i]) { same = false; break; }
        if (same) { if (found) return 0; found = at; }
    }
    return found;
}

static const U32 FRAME_PATTERN[] = {
    0xd101c3ff, 0xf9000bfb, 0xa90267fa, 0xa9035ff8, 0xa90457f6,
    0xa9054ff4, 0xa9067bfd, 0x910183fd, 0xf940900b
};
static const U32 STATE_PATTERN[] = {
    0xf81d0ff5, 0xa9014ff4, 0xa9027bfd, 0x910083fd,
    0xb9401408, 0xaa0103f4, 0xaa0003f3, 0x7100051f
};
static const U32 MODE_PATTERN[] = {0xf85103a8, 0xf9400108, 0xb89483a2, 0x8b140100};

static Addr decode_state_root(Addr at) {
    U32 adrp = *reinterpret_cast<const U32*>(at);
    U32 ldr = *reinterpret_cast<const U32*>(at + 4);
    if ((adrp & 0x9f000000u) != 0x90000000u || (ldr & 0xffc00000u) != 0xf9400000u)
        return 0;
    if ((adrp & 31) != ((ldr >> 5) & 31)) return 0;
    int64_t imm = int64_t(((adrp >> 5) & 0x7ffffu) << 2 | ((adrp >> 29) & 3u));
    if (imm & 0x100000) imm -= 0x200000;
    return (at & ~Addr(0xfff)) + imm * 4096 + ((ldr >> 10) & 0xfffu) * 8;
}

static Addr decode_call(Addr at) {
    U32 op = *reinterpret_cast<const U32*>(at);
    if ((op & 0xfc000000u) != 0x94000000u) return 0;
    int64_t imm = op & 0x03ffffffu;
    if (imm & 0x02000000) imm -= 0x04000000;
    Addr target = at + imm * 4;
    return target >= text_start && target < ro_start ? target : 0;
}

static bool chord_down() {
    constexpr U64 chord = (1ull << 6) | (1ull << 7) | (1ull << 3); // L + R + Y
    static U32 last_style = ~0u;
    static U64 last_buttons = ~0ull;
    for (U32 i = 0; i < 9; ++i) {
        const U32 id = i == 8 ? 0x20 : i;
        const U32 style = controller_style(&id);
        if (i == 0 && style != last_style) { log_value("Player 1 controller style", style); last_style = style; }
        NpadRead read = style & 1 ? full_key : style & 4 ? joy_dual : style & 2 ? handheld : nullptr;
        if (!read) continue;
        NpadState state = {};
        read(&state, &id);
        const U64 tracked = state.buttons & (chord | (1ull << 2));
        if (i == 0 && tracked != last_buttons) {
            log_value("Player 1 buttons (L=40,R=80,Y=8,X=4)", tracked);
            log_value("Player 1 connection attributes", state.attributes);
            last_buttons = tracked;
        }
        if ((state.attributes & 1) && (state.buttons & chord) == chord) return true;
    }
    return false;
}

static bool eligible_match() {
    static U64 previous = ~0ull;
    auto status = [](U64 code) {
        if (code != previous) { log_value("Eligibility status", code); previous = code; }
    };
    // Give each output its own aligned 8-byte allocation. Reverse-engineered
    // native callers use an 8-byte submode slot. A 4-byte submode allocation
    // in v03 was directly before main_mode in the AArch64 stack layout.
    alignas(8) U32 main_output[2] = {};
    alignas(8) U32 sub_output[2] = {};
    get_mode(main_output, sub_output);
    const U32 main_mode = main_output[0];
    const U64 sub_mode = U64(sub_output[0]) | U64(sub_output[1]) << 32;
    static U32 previous_main = ~0u;
    static U64 previous_sub = ~0ull;
    if (main_mode != previous_main || sub_mode != previous_sub) {
        log_value("Main mode output", main_mode);
        log_value("Submode output (64-bit)", sub_mode);
        previous_main = main_mode; previous_sub = sub_mode;
    }
    // HDR's documented mode ID: 1 = regular local Smash.
    if (main_mode != 1) { status(0x10000ull | main_mode); return false; }
    void* manager = *manager_slot;
    if (!manager) { status(2); return false; }
    if (is_results(manager)) { status(3); return false; }
    const int count = entry_count(manager);
    if (count != 2) { status(0x20000ull | U32(count)); return false; }
    unsigned cpus = 0;
    for (int i = 0; i < 2; ++i) {
        const int id = entry_id(manager, i);
        if (id < 0) { status(4); return false; }
        void* info = fighter_info(manager, id);
        if (!info) { status(5); return false; }
        cpus += is_cpu(info) ? 1 : 0;
    }
    status(cpus == 1 ? 0 : 0x30000ull | cpus);
    return cpus == 1;
}

static bool inspect_retry(Addr& out_state, Addr& out_target) {
    // Same three-level pointer walk and vtable slot used by HDR's match reset.
    Addr p = state_root;
    for (int i = 0; i < 3; ++i) {
        if (!p || (p & 7) || !mapped(p, sizeof(Addr))) {
            log_value("Invalid retry-chain read", p);
            return false;
        }
        p = *reinterpret_cast<const Addr*>(p);
    }
    if (!p || (p & 7) || !mapped(p, sizeof(Addr))) return false;
    log_value("Retry state", p);
    const Addr vtable = *reinterpret_cast<const Addr*>(p);
    log_value("Retry vtable", vtable);
    // Relocated C++ vtables can be in module .data as well as .rodata.
    // Bound them to the main image, still require readable memory, and require
    // the retry function itself to be executable code in the main text region.
    if ((vtable & 7) || vtable < ro_start || main_image_end < 6 * sizeof(Addr) ||
        vtable > main_image_end - 6 * sizeof(Addr) ||
        !mapped(vtable, 6 * sizeof(Addr))) return false;
    const Addr retry = reinterpret_cast<const Addr*>(vtable)[5];
    log_value("Retry target", retry);
    if ((retry & 3) || retry < text_start || retry >= ro_start || !mapped(retry, 4, 4)) return false;
    out_state = p;
    out_target = retry;
    return true;
}

static bool request_restart() {
    Addr state = 0, retry = 0;
    if (!inspect_retry(state, retry)) return false;
    log("Restart requested: local 1v1 CPU match.");
    reinterpret_cast<void (*)(const void*)>(retry)(reinterpret_cast<const void*>(state));
    return true;
}

static void frame_hook(U64 scene) {
    static bool entered = false;
    if (!entered) { log("Frame hook entered."); entered = true; }
    const bool eligible = eligible_match();
    const bool chord = eligible && chord_down();
    const bool ready = eligible && ready_go();
    static U64 checked_scene = 0;
    if (eligible && ready && checked_scene != scene) {
        Addr state = 0, target = 0;
        log("Read-only retry preflight:");
        log(inspect_retry(state, target) ? "Retry preflight passed; no restart requested." :
            "Retry preflight failed; inspect pointer records above.");
        checked_scene = scene;
    }
    static int previous_ready = -1;
    if (eligible && int(ready) != previous_ready) { log_value("Ready flag", ready); previous_ready = int(ready); }
    static bool previous_chord = false;
    if (chord && !previous_chord) { log("Full L+R+Y chord detected."); log_value("Gate cooldown", gate.cooldown); }
    previous_chord = chord;
    if (gate.step(scene, eligible, ready, chord)) {
        if (request_restart()) return; // Let the next frame service the new state.
        log("Restart skipped: runtime state validation failed.");
    }
    original_frame(scene);
}

static bool symbols() {
    return
        required(manager_slot, "_ZN3lib9SingletonIN3app14FighterManagerEE9instance_E") &&
        required(entry_count, "_ZN3app8lua_bind32FighterManager__entry_count_implEPNS_14FighterManagerE") &&
        required(entry_id, "_ZN3app8lua_bind33FighterManager__get_entry_id_implEPNS_14FighterManagerEi") &&
        required(fighter_info, "_ZN3app8lua_bind44FighterManager__get_fighter_information_implEPNS_14FighterManagerENS_14FighterEntryIDE") &&
        required(is_cpu, "_ZN3app8lua_bind41FighterInformation__is_operation_cpu_implEPNS_18FighterInformationE") &&
        required(is_results, "_ZN3app8lua_bind35FighterManager__is_result_mode_implEPNS_14FighterManagerE") &&
        required(ready_go, "_ZN3app14sv_information11is_ready_goEv") &&
        required(controller_style, "_ZN2nn3hid15GetNpadStyleSetERKj") &&
        required(full_key, "_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj") &&
        required(joy_dual, "_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj") &&
        required(handheld, "_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj");
}
}

extern "C" __attribute__((visibility("default"))) void plugin_main() asm("main");
extern "C" void plugin_main() {
    log("CPU Rematch 0.5.0 ARM-validated test build; expected game 13.0.3.", true);
    struct Version { char text[16]; } version = {};
    void (*get_version)(Version*) = nullptr;
    if (!resolve(get_version, "_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE")) {
        log("Disabled: game version function missing."); return;
    }
    get_version(&version);
    version.text[15] = '\0';
    log("Detected game version:"); log(version.text);
    if (!equal(version.text, "13.0.3")) { log("Disabled: game must be exactly 13.0.3."); return; }
    text_start = reinterpret_cast<Addr>(getRegionAddress(0));
    ro_start = reinterpret_cast<Addr>(getRegionAddress(1));
    data_start = reinterpret_cast<Addr>(getRegionAddress(2));
    log_value("Text base", text_start);
    log_value("Rodata base", ro_start);
    log_value("Data base", data_start);
    if (!text_start || ro_start <= text_start || data_start <= ro_start || ro_start - text_start > 0x8000000) {
        log("Disabled: invalid executable regions."); return;
    }
    Addr frame = unique(FRAME_PATTERN);
    Addr state = unique(STATE_PATTERN);
    Addr mode = unique(MODE_PATTERN);
    log_value("Frame signature", frame);
    log_value("State signature", state);
    log_value("Mode signature", mode);
    if (!frame || !mode || (state && state + 0x30 > ro_start) || mode < text_start + 4) {
        log("Disabled: frame/mode signature missing or ambiguous."); return;
    }
    Addr mode_fn = decode_call(mode - 4);
    if (state) {
        state_root = decode_state_root(state + 0x28);
    } else {
        // Exact-version map from HDR commit 73eceada (October 2024).
        // The uploaded v02 log confirms frame +0x135b810 and caller +0x1740584.
        // Require the complete known layout before selecting the data-slot offset.
        if (frame - text_start != 0x135b810 || mode - text_start != 0x1740584 ||
            mode_fn - text_start != 0x1743870) {
            log("Disabled: 13.0.3 fallback layout does not match."); return;
        }
        state_root = text_start + 0x52c3760;
        log("Using version-specific 13.0.3 battle-state slot: module + 0x52c3760.");
    }
    log_value("Decoded state root", state_root);
    log_value("Decoded mode function", mode_fn);
    if (!mode_fn || state_root < data_start || state_root > data_start + 0x2000000 || (state_root & 7)) {
        log("Disabled: instruction decoding failed."); return;
    }
    if (!mapped(state_root, sizeof(Addr), 3)) {
        log("Disabled: battle-state slot is not mapped read/write."); return;
    }
    Addr region_base = 0, region_size = 0;
    U32 region_permissions = 0;
    if (cpu_rematch_query_memory(state_root, &region_base, &region_size, &region_permissions) ||
        !region_size || region_base > state_root || region_size > ~Addr(0) - region_base) {
        log("Disabled: could not determine main image's data-memory bound."); return;
    }
    main_image_end = region_base + region_size;
    log_value("Main image data-memory bound", main_image_end);
    // Preserve a bounded helper fingerprint for diagnosis if the mode remains
    // unexpected; no game dump is required from the user.
    if (mode_fn + 24 * sizeof(U32) <= ro_start) {
        log("Mode helper instruction words (24):");
        for (unsigned i = 0; i < 24; ++i)
            log_value("Mode instruction", reinterpret_cast<const U32*>(mode_fn)[i]);
    }
    get_mode = reinterpret_cast<Mode>(mode_fn);
    if (!symbols()) { log("Disabled: required game/SDK symbol missing."); return; }
    A64HookFunction(reinterpret_cast<void*>(frame), reinterpret_cast<void*>(frame_hook),
        reinterpret_cast<void**>(&original_frame));
    log(original_frame ? "Installed. L + R + Y; local human-versus-CPU only." : "Hook installation failed.");
}
