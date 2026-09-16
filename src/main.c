#include "modding.h"
#include "ultra64.h"
#include "common_structs.h"
#include "minigame_int.h"

// --- Jetpac (arcade minigame in Cranky's Lab) ---
//
// func_jetpac_80025368 is the per-round-end dispatcher, called once when a
// player dies. Full RECOMP_PATCH reimplementation of the original logic
// (see dk64_decomp src/jetpac/code_0.c), changed in exactly one place: when
// the original would end the game for good (state 5, "game over" - no
// lives left and not already in the game-over state), this instead
// respawns the player (state 2) the same way the surrounding code already
// does for every other "still has lives" case. The two-player switching
// logic is untouched.
//
// RECOMP_HOOK/RECOMP_HOOK_RETURN were tried first but caused
// non-deterministic memory corruption in this game's runtime (crashes at
// different points across identical runs - see project memory). RECOMP_PATCH
// is the only mechanism confirmed reliable here, matching what DK64
// Recompiled's own developers and the JetPacInfiniteLives sibling mod both
// use exclusively. This patches a different function than
// JetPacInfiniteLives (func_jetpac_80025368 here vs func_jetpac_80026A3C
// there), so the two mods don't conflict if both are enabled.
RECOMP_PATCH void func_jetpac_80025368(Competitor *arg0) {
    s32 other_player_index;
    Competitor *other_player;

    other_player_index = D_jetpac_8002EC30.player_index ^ 1;
    other_player = &D_jetpac_8002EC30.player[other_player_index];
    if ((arg0->lives < 0) && (D_jetpac_8002EC30.unk78C != 5)) {
        if (D_jetpac_8002EC30.unk18 < arg0->current_score) {
            D_jetpac_8002EC30.unk18 = arg0->current_score;
            func_jetpac_80024A4C();
        }
        // Reset to a fresh starting life count - same values func_jetpac_80024390
        // uses at a real game start - instead of respawning with the exhausted
        // (negative) count that got us into this branch in the first place.
        arg0->lives = D_jetpac_8002EC30.unk798 != 0 ? 3 : 5;
        func_jetpac_80024F9C(2); // was 5 (game over) - auto-reset: respawn instead
    } else {
        if (other_player->lives >= 0) {
            D_jetpac_8002EC30.player_index = other_player_index;
            if (other_player->level < 0) {
                func_jetpac_800250A0();
            } else {
                func_jetpac_80024F9C(2);
            }
        } else {
            if (arg0->lives >= 0) {
                func_jetpac_80024F9C(2);
            } else {
                func_jetpac_80024F9C(0);
            }
        }
    }
}

// --- Bonus barrel minigames: RECOMP_HOOK_RETURN re-test (barrels only) ---
//
// func_bonus_800265C0 is the shared "you failed" entry point used by every
// bonus barrel variant. Earlier testing found RECOMP_HOOK_RETURN unreliable
// on func_jetpac_80025368 specifically; this re-tests it in isolation on a
// completely different function, since that earlier result doesn't
// necessarily generalize. If this build loads and runs without crashing,
// the hook clears the actor's init-gate bit and resets control_state on
// both the actor and its companion (unk11C) - the same trick used
// throughout this mod's history - so the barrel's own per-frame function
// replays its one-time setup next frame, as if freshly spawned.
//
// Whether this actually skips the outro cutscene depends on the specific
// barrel type: for K.Rool barrel challenges (src/bonus/code_0.c), calling
// this function is the last thing most fail branches do, so a hook running
// before it returns should stick. For Batty Barrel Bandit and Kremling
// Kosh, their callers unconditionally play a cutscene and overwrite
// control_state immediately after calling this function - a hook here
// cannot prevent that (a callee can't undo what its caller does after
// the call returns), so the cutscene will still play for those even if
// this hook works.
RECOMP_HOOK_RETURN("func_bonus_800265C0") void bonus_barrel_fail_reset_hook(void) {
    if (gCurrentActorPointer == NULL) {
        return;
    }
    gCurrentActorPointer->object_properties_bitfield &= ~0x10u;
    gCurrentActorPointer->control_state = 0;
    gCurrentActorPointer->control_state_progress = 0;
    if (gCurrentActorPointer->unk11C != NULL) {
        gCurrentActorPointer->unk11C->object_properties_bitfield &= ~0x10u;
        gCurrentActorPointer->unk11C->control_state = 0;
        gCurrentActorPointer->unk11C->control_state_progress = 0;
    }
}
