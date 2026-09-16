#include "modding.h"
#include "ultra64.h"
#include "common_structs.h"
#include "minigame_int.h"
#include "enums.h" // real MAP_* constants, straight from the decomp - no hand-copied values to get wrong

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

// --- EXPERIMENTAL: redirect every bonus barrel to Beaver Bother ---
//
// Not a hook into a specific function's body - RECOMP_CALLBACK subscribes
// to an event the base game already declares and fires itself
// (dk64recomp_every_frame, see boot_logos_patches.c upstream), implemented
// in the runtime as a plain list of function pointers the game calls
// directly (mod_events.cpp) - a completely different, much simpler code
// path than the hook trampoline mechanism that proved unreliable
// elsewhere in this mod. Worth testing on its own merits.
//
// A map transition is requested by writing `next_map` and setting the
// countdown D_global_asm_8076A0B2 = 3 (see func_global_asm_805FF378,
// dk64_decomp src/global_asm/code_3C10.c); it decrements once per frame
// and the transition actually completes when it reaches 0 (checked
// elsewhere as `!D_global_asm_8076A0B2`). That gives a ~3 frame window,
// every frame, where a pending transition's target can still be swapped
// before the real load (and the target map's actual room/actors) happens -
// unlike overwriting `current_map` itself, which only takes effect *after*
// the wrong room's own content is already loaded.
extern Maps next_map;
extern u8 D_global_asm_8076A0B2;

static int is_bonus_barrel_map(Maps map) {
    switch (map) {
        case MAP_KROOL_BARREL_TINY_MUSHROOM_GAME:
        case MAP_KROOL_BARREL_DK_TARGET_GAME:
        case MAP_KROOL_BARREL_LANKY_MAZE:
        case MAP_KROOL_BARREL_DIDDY_KREMLING_GAME:
        case MAP_KROOL_BARREL_DIDDY_ROCKETBARREL_GAME:
        case MAP_KROOL_BARREL_LANKY_SHOOTING_GAME:
        case MAP_KROOL_BARREL_CHUNKY_HIDDEN_KREMLING_GAME:
        case MAP_KROOL_BARREL_TINY_PONY_TAIL_TWIRL_GAME:
        case MAP_KROOL_BARREL_CHUNKY_SHOOTING_GAME:
        case MAP_KROOL_BARREL_DK_RAMBI_GAME:
        case MAP_BATTY_BARREL_BANDIT_EASY:
        case MAP_BATTY_BARREL_BANDIT_EASY_2:
        case MAP_BATTY_BARREL_BANDIT_NORMAL:
        case MAP_BATTY_BARREL_BANDIT_HARD:
        case MAP_KREMLING_KOSH_VERY_EASY:
        case MAP_KREMLING_KOSH_EASY:
        case MAP_KREMLING_KOSH_NORMAL:
        case MAP_KREMLING_KOSH_HARD:
        case MAP_RAMBI_ARENA:
        case MAP_ENGUARDE_ARENA:
            return 1;
        default:
            return 0;
    }
}

RECOMP_CALLBACK("*", dk64recomp_every_frame) void redirect_barrels_to_beaver_bother(void) {
    if (D_global_asm_8076A0B2 != 0 && is_bonus_barrel_map(next_map)) {
        next_map = MAP_BEAVER_BOTHER_EASY;
    }
}
