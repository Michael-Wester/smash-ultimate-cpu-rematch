// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <stdint.h>

struct RematchGate {
    uintptr_t scene = 0;
    bool seen_ready = false;
    bool held = true; // Require release after entering a match.
    unsigned cooldown = 0;

    bool step(uintptr_t current_scene, bool eligible, bool ready, bool chord) {
        if (scene != current_scene || !eligible) {
            scene = current_scene;
            seen_ready = false;
            held = true;
            cooldown = 0;
        }
        if (!eligible) return false;
        seen_ready = seen_ready || ready;
        const bool pressed = chord && !held;
        held = chord;
        if (cooldown) { --cooldown; return false; }
        if (!seen_ready || !pressed) return false;
        cooldown = 180;
        return true;
    }
};
