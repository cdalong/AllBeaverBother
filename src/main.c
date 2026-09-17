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

// --- The actual reward fix, take two ---
//
// func_bonus_80024D8C (aka getBattleCrownFlagID) only ever gets called from
// the update code of a specific ACTOR INSTANCE that the Battle Arena's own
// room data places (the "character spawner"/crown-timer actor, see
// BattleCrownControllerCode / func_bonus_80024E38). Beaver Bother's own room
// doesn't place that actor at all, so redirecting there means that whole
// code path - the function, the switch, all of it - simply never runs.
// Confirmed by trace: patching func_bonus_80024D8C to use
// g_original_target_map had zero effect on the captured trace, byte-for-byte
// identical to before the patch. Reimplementing per-arena actor placement to
// make that actor exist in Beaver Bother's room isn't realistic here.
//
// Different approach: don't wait for the game's own (map-specific, actor-
// gated) reward logic at all. We already know, ourselves, which arena the
// player actually walked into (g_original_target_map) and we can already see
// reliably, via recomp_on_cutscene_play, the exact moment Beaver Bother
// itself signals a win (cutscene 33 - confirmed to fire every successful
// attempt in every trace so far). So grant the crown flag directly right
// there, by calling the base game's own real setFlag() - the same function
// every genuine collectible pickup in the game already goes through - with
// the flag that arena would have granted. This needs no actor, no hook, and
// no map-specific code to exist in Beaver Bother's room at all.
//
// Take three: calling setFlag() directly grants real credit (confirmed via
// trace - the flag write shows up), but it's invisible - no crown ever
// appears, because nothing actually spawned one. Spawning the real crown
// actor instead (via func_global_asm_806A5DF0/spawnActorWithFlag, the same
// call BattleCrownControllerCode itself makes - see Ghidra's
// getBattleCrownFlagID/BattleCrownControllerCode) lets the player physically
// touch and collect it, which is what should set the flag for real. Spawn it
// at the player's own position (not the arena's hardcoded coordinates, which
// mean nothing in Beaver Bother's own room) so it's guaranteed reachable.
extern void setFlag(s16 flagIndex, u8 newValue, u8 flagType);
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
        // The rest of the pooled bonus-barrel minigames - missed on the
        // first pass, which only covered a handful of named ones (confirmed
        // missing in-game: Searchlight Seek walked right past the redirect).
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

// --- Diagnostic trace buffer ---
//
// Individual recomp_printf calls scattered across a play session have been
// unreliable (missing lines, entire sessions with zero output, with no
// journald rate-limit warning to explain it). Buffering everything in
// memory and dumping it in one shot - triggered by leaving
// MAP_BEAVER_BOTHER_EASY, detected every frame via the same
// dk64recomp_every_frame callback already proven reliable - avoids
// relying on many individually-timed prints. Also no longer filters to
// flag == -1 only: that filter might itself be hiding whatever the real
// signal is on attempts where the result differs.
#define TRACE_CAPACITY 48

typedef struct {
    u8 kind; // 0 = flag_change, 1 = cutscene_play
    s16 a;   // flag or cutscene index
    u8 b;    // target_state or cutscene_bitfield
    u8 c;    // flag_type (kind 0 only)
    Maps map_at_event;
} TraceEntry;

static TraceEntry g_trace[TRACE_CAPACITY];
static int g_trace_count = 0;
static int g_was_in_beaver_bother = 0;
// Frames left before dumping, once we've left Beaver Bother - keeps
// accumulating trace entries during the delay instead of dumping
// immediately. A previous attempt dumped (and cleared) the buffer on the
// very first frame current_map changed away from Beaver Bother, which cut
// the trace off right as the interesting part should start - the actual
// post-win events apparently land a few frames later, not instantly.
static int g_dump_countdown = 0;

static void trace_add(u8 kind, s16 a, u8 b, u8 c) {
    if (g_trace_count < TRACE_CAPACITY) {
        g_trace[g_trace_count].kind = kind;
        g_trace[g_trace_count].a = a;
        g_trace[g_trace_count].b = b;
        g_trace[g_trace_count].c = c;
        g_trace[g_trace_count].map_at_event = current_map;
        g_trace_count++;
    }
}

static void trace_dump(void) {
    int i;
    recomp_printf("[MinigameReset] === trace dump: %d entries, original_target_map=%d ===\n",
        g_trace_count, (int)g_original_target_map);
    for (i = 0; i < g_trace_count; i++) {
        if (g_trace[i].kind == 0) {
            recomp_printf("[MinigameReset] #%d flag_change flag=%d state=%d type=%d map=%d\n",
                i, (int)g_trace[i].a, (int)g_trace[i].b, (int)g_trace[i].c, (int)g_trace[i].map_at_event);
        } else {
            recomp_printf("[MinigameReset] #%d cutscene cs=%d bitfield=%d map=%d\n",
                i, (int)g_trace[i].a, (int)g_trace[i].b, (int)g_trace[i].map_at_event);
        }
    }
    recomp_printf("[MinigameReset] === end trace dump ===\n");
    g_trace_count = 0;
}

// --- Extra checkpoints ---
//
// Added to pin down exactly how far the pipeline gets on a given attempt,
// separate from the trace dump's contents: is the mod loaded at all, does
// the redirect itself fire, does the "just left Beaver Bother" edge get
// detected, and does dk64recomp_every_frame keep firing continuously
// through a whole session (a periodic heartbeat, since prior tests showed
// output that stopped appearing entirely with no explanation).
RECOMP_CALLBACK("*", recomp_on_init) void log_mod_loaded(void) {
    recomp_printf("[MinigameReset] mod loaded\n");
}

static u32 g_frame_counter = 0;

RECOMP_CALLBACK("*", dk64recomp_every_frame) void redirect_everything_to_beaver_bother(void) {
    g_frame_counter++;
    if ((g_frame_counter % 300) == 0) {
        recomp_printf("[MinigameReset] heartbeat: frame=%u current_map=%d\n", g_frame_counter, (int)current_map);
    }

    if (D_global_asm_8076A0B2 != 0 && is_redirect_target_map(next_map)) {
        g_original_target_map = next_map;
        recomp_printf("[MinigameReset] redirect triggered: original_target_map=%d\n", (int)g_original_target_map);
        if (battle_arena_crown_flag(g_original_target_map) != -1) {
            g_pending_reward = 1;
        }
        recomp_printf("[MinigameReset] pending_reward set to %d (crown_flag_lookup=%d)\n",
            g_pending_reward, (int)battle_arena_crown_flag(g_original_target_map));
        next_map = MAP_BEAVER_BOTHER_EASY;
    }

    if (current_map == MAP_BEAVER_BOTHER_EASY) {
        g_was_in_beaver_bother = 1;
    } else if (g_was_in_beaver_bother) {
        // Just left Beaver Bother - start a delay before dumping, instead
        // of dumping immediately, so events landing a few frames after the
        // map transition still get captured.
        g_was_in_beaver_bother = 0;
        recomp_printf("[MinigameReset] left Beaver Bother, dumping trace shortly\n");
        g_dump_countdown = 180; // generous - exact frame rate here isn't confirmed
        g_pending_reward = 0; // quit/fail without the win cutscene - don't grant later by mistake
    }

    if (g_dump_countdown > 0) {
        g_dump_countdown--;
        if (g_dump_countdown == 0) {
            trace_dump();
        }
    }
}

RECOMP_CALLBACK("*", recomp_on_flag_change) void trace_flag_change(s16 *flag, u8 *target_state, u8 *flag_type) {
    trace_add(0, *flag, *target_state, *flag_type);
}

RECOMP_CALLBACK("*", recomp_on_cutscene_play) void trace_cutscene_play(s16 *cutscene, u8 *cutscene_bitfield) {
    trace_add(1, *cutscene, *cutscene_bitfield, 0);
    recomp_printf("[MinigameReset] cutscene event: cs=%d pending_reward=%d original_target_map=%d\n",
        (int)*cutscene, g_pending_reward, (int)g_original_target_map);

    // Cutscene 33 is Beaver Bother's own win cutscene (confirmed in every
    // successful-attempt trace so far). Spawn the redirected arena's real
    // crown, with its real flag baked in, right here at the player's own
    // position - instead of relying on any map- or actor-specific code to do
    // it (see the comment above battle_arena_crown_flag for why that path
    // never actually runs here), and instead of setting the flag ourselves
    // directly (which worked, but left nothing to actually touch/collect -
    // see project memory for why that's also suspected to make a
    // subsequently-spawned crown just delete itself as "already collected").
    if (*cutscene == 33 && g_pending_reward) {
        s32 flag = battle_arena_crown_flag(g_original_target_map);
        if (flag != -1) {
            recomp_printf("[MinigameReset] spawning crown flag=%d for original_target_map=%d at (%f, %f, %f)\n",
                (int)flag, (int)g_original_target_map,
                gPlayerPointer->x_position, gPlayerPointer->y_position, gPlayerPointer->z_position);
            func_global_asm_806A5DF0(0x56, gPlayerPointer->x_position, gPlayerPointer->y_position,
                gPlayerPointer->z_position, 0, 0, (s16)flag, 0);
        }
        g_pending_reward = 0;
    }
}
