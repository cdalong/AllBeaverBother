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
extern Maps current_map;
extern u8 D_global_asm_8076A0B2;

// Remembers which map the player actually tried to enter, so the reward
// fix below knows what was really requested - we never touch this global
// ourselves otherwise, only next_map.
static Maps g_original_target_map;

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
        // Battle Arenas (single-player)
        case MAP_BATTLE_ARENA_BEAVER_BRAWL:
        case MAP_BATTLE_ARENA_KRITTER_KARNAGE:
        case MAP_BATTLE_ARENA_ARENA_AMBUSH:
        case MAP_BATTLE_ARENA_MORE_KRITTER_KARNAGE:
        case MAP_BATTLE_ARENA_FOREST_FRACAS:
        case MAP_BATTLE_ARENA_BISH_BASH_BRAWL:
        case MAP_BATTLE_ARENA_KAMIKAZE_KREMLINGS:
        case MAP_BATTLE_ARENA_PLINTH_PANIC:
        case MAP_BATTLE_ARENA_PINNACLE_PALAVER:
        case MAP_BATTLE_ARENA_SHOCKWAVE_SHOWDOWN:
            return 1;
        default:
            return 0;
    }
}

RECOMP_CALLBACK("*", dk64recomp_every_frame) void redirect_everything_to_beaver_bother(void) {
    if (D_global_asm_8076A0B2 != 0 && is_redirect_target_map(next_map)) {
        g_original_target_map = next_map;
        next_map = MAP_BEAVER_BOTHER_EASY;
    }
}

// --- Fix Battle Arena rewards ---
//
// Confirmed by logging every flag change while winning a redirected
// challenge: the reward-granting code DOES run right after the win
// cutscene, but computes flag == -1 (an invalid/no-op sentinel) instead of
// the real reward, because it derives "which reward" from `current_map` -
// which is now Beaver Bother's, not the arena actually entered.
//
// func_bonus_80024D8C (dk64_decomp src/bonus/code_0.c) is the real,
// decompiled function that maps a Battle Arena's `current_map` to its
// crown's permanent flag index (returning -1 for anything else, matching
// exactly what we observed). This mirrors that same mapping, but keyed on
// the ORIGINAL map the player actually entered (captured above, before we
// overwrote next_map) instead of the current (wrong) one.
//
// recomp_on_flag_change is a real declared event, fired right before any
// flag change, passing the flag index/target state/flag type BY POINTER -
// same safe RECOMP_CALLBACK mechanism as the redirect above, not a hook.
// Rewriting *flag here changes what the game's own pending setFlag call
// actually applies.
static s16 battle_arena_reward_flag(Maps map) {
    switch (map) {
        case MAP_BATTLE_ARENA_BEAVER_BRAWL:
            return 0x261;
        case MAP_BATTLE_ARENA_KRITTER_KARNAGE:
            return 0x262;
        case MAP_BATTLE_ARENA_ARENA_AMBUSH:
            return 0x263;
        case MAP_BATTLE_ARENA_MORE_KRITTER_KARNAGE:
            return 0x264;
        case MAP_BATTLE_ARENA_KAMIKAZE_KREMLINGS:
            return 0x265;
        case MAP_BATTLE_ARENA_FOREST_FRACAS:
            return 0x266;
        case MAP_BATTLE_ARENA_BISH_BASH_BRAWL:
            return 0x267;
        case MAP_BATTLE_ARENA_PLINTH_PANIC:
            return 0x268;
        case MAP_BATTLE_ARENA_PINNACLE_PALAVER:
            return 0x269;
        case MAP_BATTLE_ARENA_SHOCKWAVE_SHOWDOWN:
            return 0x26A;
        default:
            return -1;
    }
}

// TEMPORARY: observe only, no mutation, and no early `return;` mid-function
// (single fall-through exit only, matching the shape of the
// dk64recomp_every_frame callback that's still working) - isolating
// whether an explicit early return is itself what breaks this specific
// callback, since that's the main structural difference from the version
// that worked.
RECOMP_CALLBACK("*", recomp_on_flag_change) void fix_battle_arena_reward_flag(s16 *flag, u8 *target_state, u8 *flag_type) {
    if (*flag == -1) {
        recomp_printf("[MinigameReset] flag_change: flag=-1 target_state=%d flag_type=%d (original_target_map=%d, current_map=%d)\n",
            (int)*target_state, (int)*flag_type, (int)g_original_target_map, (int)current_map);
    }
}
