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

// --- Battle Arena crown reward ---
//
// func_bonus_80024D8C (aka getBattleCrownFlagID) only ever gets called from
// the update code of a specific ACTOR INSTANCE that the Battle Arena's own
// room data places (the "character spawner"/crown-timer actor, see
// BattleCrownControllerCode / func_bonus_80024E38). Beaver Bother's own room
// doesn't place that actor at all, so redirecting there means that whole
// code path - the function, the switch, all of it - simply never runs.
//
// Instead, we watch for Beaver Bother's own win cutscene (cutscene 33) and
// spawn the real crown actor ourselves, with the correct flag for whichever
// arena was actually walked into, via the game's own
// func_global_asm_806A5DF0/spawnActorWithFlag call - the same one the
// arena's own crown-granting code would have used. Spawning it at the
// player's own position means they're already standing on it, so the game's
// own real pickup/collision code grants it for real - this mod never writes
// the flag itself.
extern void func_global_asm_806A5DF0(s16 actor, f32 x, f32 y, f32 z, s16 angle, u8 spawn_type, s16 flag, s32 param8);

// Minimal mirror of just the leading position fields of Actor (see
// dk64_decomp/include/structs.h) - avoids pulling in the full struct (and
// everything it drags in) just to read x/y/z off gPlayerPointer.
typedef struct {
    u8 pad_0x7C[0x7C];
    f32 x_position;
    f32 y_position;
    f32 z_position;
} PlayerPositionMirror;
extern PlayerPositionMirror *gPlayerPointer;

static s32 battle_arena_crown_flag(Maps map) {
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

// Set the moment a redirect happens (only for maps we know how to grant a
// reward for) and cleared once granted, so a player's genuine, un-redirected
// Beaver Bother win never accidentally grants a stale crown from an earlier
// session.
static int g_pending_reward = 0;

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
        // The rest of the pooled bonus-barrel minigames
        case MAP_STEALTHY_SNOOP_NORMAL_NO_LOGO:
        case MAP_STEALTHY_SNOOP_VERY_EASY:
        case MAP_STEALTHY_SNOOP_EASY:
        case MAP_STEALTHY_SNOOP_NORMAL:
        case MAP_STEALTHY_SNOOP_HARD:
        case MAP_TEETERING_TURTLE_TROUBLE_VERY_EASY:
        case MAP_TEETERING_TURTLE_TROUBLE_EASY:
        case MAP_TEETERING_TURTLE_TROUBLE_NORMAL:
        case MAP_TEETERING_TURTLE_TROUBLE_HARD:
        case MAP_MAD_MAZE_MAUL_EASY:
        case MAP_MAD_MAZE_MAUL_NORMAL:
        case MAP_MAD_MAZE_MAUL_HARD:
        case MAP_MAD_MAZE_MAUL_INSANE:
        case MAP_STASH_SNATCH_EASY:
        case MAP_STASH_SNATCH_NORMAL:
        case MAP_STASH_SNATCH_HARD:
        case MAP_STASH_SNATCH_INSANE:
        case MAP_BUSY_BARREL_BARRAGE_EASY:
        case MAP_BUSY_BARREL_BARRAGE_NORMAL:
        case MAP_BUSY_BARREL_BARRAGE_HARD:
        case MAP_SPLISH_SPLASH_SALVAGE_EASY:
        case MAP_SPLISH_SPLASH_SALVAGE_NORMAL:
        case MAP_SPLISH_SPLASH_SALVAGE_HARD:
        case MAP_SPEEDY_SWING_SORTIE_EASY:
        case MAP_SPEEDY_SWING_SORTIE_NORMAL:
        case MAP_SPEEDY_SWING_SORTIE_HARD:
        case MAP_KRAZY_KONG_KLAMOUR_EASY:
        case MAP_KRAZY_KONG_KLAMOUR_NORMAL:
        case MAP_KRAZY_KONG_KLAMOUR_HARD:
        case MAP_KRAZY_KONG_KLAMOUR_INSANE:
        case MAP_BIG_BUG_BASH_VERY_EASY:
        case MAP_BIG_BUG_BASH_EASY:
        case MAP_BIG_BUG_BASH_NORMAL:
        case MAP_BIG_BUG_BASH_HARD:
        case MAP_SEARCHLIGHT_SEEK_VERY_EASY:
        case MAP_SEARCHLIGHT_SEEK_EASY:
        case MAP_SEARCHLIGHT_SEEK_NORMAL:
        case MAP_SEARCHLIGHT_SEEK_HARD:
        case MAP_PERIL_PATH_PANIC_VERY_EASY:
        case MAP_PERIL_PATH_PANIC_EASY:
        case MAP_PERIL_PATH_PANIC_NORMAL:
        case MAP_PERIL_PATH_PANIC_HARD:
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

static int g_was_in_beaver_bother = 0;

RECOMP_CALLBACK("*", dk64recomp_every_frame) void redirect_everything_to_beaver_bother(void) {
    if (D_global_asm_8076A0B2 != 0 && is_redirect_target_map(next_map)) {
        g_original_target_map = next_map;
        if (battle_arena_crown_flag(g_original_target_map) != -1) {
            g_pending_reward = 1;
        }
        next_map = MAP_BEAVER_BOTHER_HARD;
    }

    if (current_map == MAP_BEAVER_BOTHER_HARD) {
        g_was_in_beaver_bother = 1;
    } else if (g_was_in_beaver_bother) {
        // Left Beaver Bother without the win cutscene ever firing (quit/fail)
        // - don't grant a crown later by mistake on some future, unrelated
        // win.
        g_was_in_beaver_bother = 0;
        g_pending_reward = 0;
    }
}

RECOMP_CALLBACK("*", recomp_on_cutscene_play) void grant_battle_arena_crown(s16 *cutscene, u8 *cutscene_bitfield) {
    // Cutscene 33 is Beaver Bother's own win cutscene.
    if (*cutscene == 33 && g_pending_reward) {
        s32 flag = battle_arena_crown_flag(g_original_target_map);
        if (flag != -1) {
            func_global_asm_806A5DF0(0x56, gPlayerPointer->x_position, gPlayerPointer->y_position,
                gPlayerPointer->z_position, 0, 0, (s16)flag, 0);
        }
        g_pending_reward = 0;
    }
}
