// SPDX-License-Identifier: AGPL-3.0-or-later
#include <assert.h>
#include "gate.hpp"
int main() {
    RematchGate g;
    assert(!g.step(1,false,true,true)); // menus/online/wrong participants
    assert(!g.step(1,true,false,false));
    assert(!g.step(1,true,false,true)); // opening countdown cannot restart
    assert(!g.step(1,true,true,true)); // held from countdown cannot restart
    assert(!g.step(1,true,true,false));
    assert(g.step(1,true,true,true));
    for (int i=0;i<300;++i) assert(!g.step(1,true,true,true)); // held -> no loop
    assert(!g.step(1,true,false,false));
    assert(g.step(1,true,false,true)); // supports ready flag falling at match end
    assert(!g.step(2,true,true,true)); // new scene held -> no loop
    assert(!g.step(2,true,true,false));
    assert(g.step(2,true,true,true));
    assert(!g.step(2,true,true,false));
    assert(!g.step(2,true,true,true)); // rapid second request suppressed
    assert(!g.step(2,false,true,false));
    assert(!g.step(2,true,false,false));
    assert(!g.step(2,true,false,true)); // eligibility loss clears stale ready state
}
