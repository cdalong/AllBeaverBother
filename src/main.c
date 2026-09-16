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
}
