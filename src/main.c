#include "modding.h"
#include "ultra64.h"
#include "enums.h" // real MAP_* constants, straight from the decomp - no hand-copied values to get wrong

// --- Everything becomes Beaver Bother ---
//
// Not a hook into a specific function's body - RECOMP_CALLBACK subscribes
// to an event the base game already declares and fires itself
// (dk64recomp_every_frame, see boot_logos_patches.c upstream), implemented
// in the runtime as a plain list of function pointers the game calls
// directly (mod_events.cpp) - a completely different, much simpler code
// path than the hook trampoline mechanism that proved unreliable
// elsewhere in this mod's history (see project memory).
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

static int is_redirect_target_map(Maps map) {
    switch (map) {
        // Bonus barrels
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
        // Jetpac
        case MAP_JETPAC:
        // Animal races
        case MAP_AZTEC_BEETLE_RACE:
        case MAP_CAVES_BEETLE_RACE:
        case MAP_FACTORY_CAR_RACE:
        case MAP_CASTLE_CAR_RACE:
        case MAP_GALLEON_SEAL_RACE:
        // Minecart Mayhem
        case MAP_MINECART_MAYHEM_EASY:
        case MAP_MINECART_MAYHEM_NORMAL:
        case MAP_MINECART_MAYHEM_HARD:
            return 1;
        default:
            return 0;
    }
}

RECOMP_CALLBACK("*", dk64recomp_every_frame) void redirect_everything_to_beaver_bother(void) {
    if (D_global_asm_8076A0B2 != 0 && is_redirect_target_map(next_map)) {
        next_map = MAP_BEAVER_BOTHER_EASY;
    }
}
