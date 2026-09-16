#include "modding.h"
#include "ultra64.h"
#include "common_structs.h"
#include "minigame_int.h"

// Hold L + R + Z together to force a minigame reset at any point during play.
#define RESET_COMBO (L_TRIG | R_TRIG | Z_TRIG)

extern OSContPad *D_global_asm_807ECDEC;

static int reset_combo_pressed(void) {
    static int was_held = 0;
    int held = D_global_asm_807ECDEC != NULL && (D_global_asm_807ECDEC->button & RESET_COMBO) == RESET_COMBO;
    int pressed = held && !was_held;
    was_held = held;
    return pressed;
}

// --- Jetpac (arcade minigame in Cranky's Lab) ---
//
// Bisection build: this is the ONLY hook in this branch, to isolate whether
// DK64 Recompiled's mod loader can handle this specific hook in isolation.
// See the `full-mod` branch for the complete mod (bonus barrels, Minecart
// Mayhem, countdown, win combo) - main is deliberately minimal right now
// while tracking down a startup crash. Add minigames back one at a time
// from full-mod once each is confirmed not to trigger the crash.
//
// Opaque - never dereferenced, only needed to match func_jetpac_80025368's
// real parameter type for the entry hook below.
typedef struct JetpacCompetitor JetpacCompetitor;

// func_jetpac_80025368 is the per-round-end dispatcher: it decides whether
// to end the game (state 5), respawn the current player (state 2), or
// return to the title (state 0) based on remaining lives.
//
// TEMPORARY: using RECOMP_HOOK (entry, before the original body) here
// instead of RECOMP_HOOK_RETURN, to test whether RECOMP_HOOK_RETURN
// specifically is what the mod loader can't handle - DK64 Recompiled's own
// base-game patches never use RECOMP_HOOK/RECOMP_HOOK_RETURN at all, only
// RECOMP_PATCH, so this is unproven territory. This changes behavior
// slightly (our check now runs before the dispatcher decides the outcome,
// so a very unlucky same-frame race is possible), acceptable for this
// isolation test.
RECOMP_HOOK("func_jetpac_80025368") void jetpac_round_end_reset_hook(JetpacCompetitor *arg0) {
    (void)arg0;
    if (reset_combo_pressed()) {
        func_jetpac_80024F9C(2);
    }
}
