#include "modding.h"
#include "ultra64.h"
#include "common_structs.h"
#include "minigame_int.h"

// Hold L + R + Z together to force a minigame reset at any point during play.
#define RESET_COMBO (L_TRIG | R_TRIG | Z_TRIG)

extern OSContPad *D_global_asm_807ECDEC;

static int reset_combo_held(void) {
    return D_global_asm_807ECDEC != NULL && (D_global_asm_807ECDEC->button & RESET_COMBO) == RESET_COMBO;
}

// Edge-triggered: true only on the frame the combo transitions from not-held
// to held, so holding it down doesn't restart the countdown every frame.
static int reset_combo_pressed(void) {
    static int was_held = 0;
    int held = reset_combo_held();
    int pressed = held && !was_held;
    was_held = held;
    return pressed;
}

// Actors are re-initialized by the engine's generic per-frame actor
// processing (func_global_asm_80678CC8) the first time it sees bit 0x10
// unset, then re-arms the bit at the end of the same frame. Clearing it here
// makes the minigame's own state machine perform a full, in-place reset next
// time its per-type update function runs - the same path a fresh spawn
// takes - without us having to hand-track every private counter/timer field.
static void reset_actor(MinigameActor *actor) {
    if (actor == NULL) {
        return;
    }
    actor->object_properties_bitfield &= ~0x10u;
    actor->control_state = 0;
    actor->control_state_progress = 0;
}

// Resets an actor and its companion (the unk11C actor that drives fail/timer
// conditions for the bonus barrels and Minecart Mayhem alike).
static void reset_minigame_actor(MinigameActor *actor) {
    if (actor == NULL) {
        return;
    }
    reset_actor(actor);
    reset_actor(actor->unk11C);
}

// --- Reset countdown ("3, 2, 1, GO!") ---
//
// Only one minigame can be active at a time, so a single pending countdown
// is all we need. printStyledText/addActorToTextOverlayRenderArray are pure
// display-list helpers (no allocation, nothing to free), and the overlay
// array is an immediate-mode queue: a draw callback has to be re-enqueued
// every frame it should stay visible, which is why this needs its own
// per-frame tick rather than a one-shot registration.
#define COUNTDOWN_FRAMES_PER_STEP 20 // ~1 second per step, assuming ~20 game-logic frames/sec
#define COUNTDOWN_STEPS 4            // "3", "2", "1", "GO!"
#define COUNTDOWN_TOTAL_FRAMES (COUNTDOWN_FRAMES_PER_STEP * COUNTDOWN_STEPS)

static MinigameActor *g_countdown_actor = NULL;
static s32 g_countdown_frames_remaining = 0;
static int g_countdown_is_race = 0;

static const char *countdown_text(s32 frames_remaining) {
    s32 step = (frames_remaining - 1) / COUNTDOWN_FRAMES_PER_STEP; // 3, 2, 1, 0
    switch (step) {
        case 3:
            return "3";
        case 2:
            return "2";
        case 1:
            return "1";
        default:
            return "GO!";
    }
}

static Gfx *draw_countdown(Gfx *dl, MinigameActor *actor) {
    (void)actor;
    if (g_countdown_frames_remaining <= 0) {
        return dl;
    }
    // Logical 320x240 screen space scaled by 4, roughly screen-centered.
    return printStyledText(dl, 3, 600, 380, (u8 *)countdown_text(g_countdown_frames_remaining), 1U);
}

// Starts a countdown targeting the current actor. No-op if one is already
// running - the fail-triggered and combo-triggered paths can both call this
// without stepping on each other.
static void countdown_start(void) {
    if (gCurrentActorPointer == NULL || g_countdown_frames_remaining > 0) {
        return;
    }
    g_countdown_actor = gCurrentActorPointer;
    g_countdown_frames_remaining = COUNTDOWN_TOTAL_FRAMES;
    g_countdown_is_race = 0;
}

// Animal races (see RaceActorExtra) keep their own outer stage counter at
// unk178, separate from the actor's own control_state, so the standard
// bit-0x10 reset alone would leave the race stuck on its results/fail
// screen instead of back at the start line. This starts the same countdown
// but additionally drives that stage counter back to 1 ("get ready"
// sequence) once it elapses - see castle_car_race_reset_hook for why that
// value specifically.
static void countdown_start_race(void) {
    countdown_start();
    if (g_countdown_actor != NULL) {
        g_countdown_is_race = 1;
    }
}

// Call once per frame from every hooked minigame's per-frame update
// function: advances any pending countdown, keeps its text enqueued for
// this frame's HUD draw, and performs the real reset once it elapses.
static void countdown_tick(void) {
    if (g_countdown_frames_remaining <= 0) {
        return;
    }
    addActorToTextOverlayRenderArray(draw_countdown, NULL, 3);
    g_countdown_frames_remaining--;
    if (g_countdown_frames_remaining == 0) {
        reset_minigame_actor(g_countdown_actor);
        if (g_countdown_is_race && g_countdown_actor != NULL) {
            RaceActorExtra *race = (RaceActorExtra *)g_countdown_actor->unk178;
            if (race != NULL) {
                race->unk34 = 1;
                race->unk35 = 0;
            }
        }
        g_countdown_actor = NULL;
        g_countdown_is_race = 0;
    }
}

// Shared body for every hooked minigame's per-frame update function: keep
// any pending countdown moving, and start a new one on a fresh combo press.
static void minigame_reset_tick(void) {
    countdown_tick();
    if (reset_combo_pressed()) {
        countdown_start();
    }
}

// Same as minigame_reset_tick, for animal races - see countdown_start_race.
static void race_reset_tick(void) {
    countdown_tick();
    if (reset_combo_pressed()) {
        countdown_start_race();
    }
}

// --- Jetpac (arcade minigame in Cranky's Lab) ---
//
// func_jetpac_80025368 is the per-round-end dispatcher: it decides whether
// to end the game (state 5), respawn the current player (state 2), or
// return to the title (state 0) based on remaining lives. Jetpac has no
// cutscenes and no countdown of its own - it's a fast arcade loop - so we
// keep its reset instant rather than adding the countdown delay used
// elsewhere. RECOMP_HOOK_RETURN gives no access to the original
// arguments/return value, but we don't need them here - just the "after"
// timing.
RECOMP_HOOK_RETURN("func_jetpac_80025368") void jetpac_round_end_reset_hook(void) {
    if (reset_combo_pressed()) {
        func_jetpac_80024F9C(2);
    }
}

// --- Banana barrel / bonus minigames ---
//
// Every bonus barrel variant (K.Rool barrel challenges, Batty Barrel Bandit,
// Kremling Kosh, Rambi Arena, ...) funnels its outcome through the same two
// shared functions, so hooking those two covers all of them without needing
// per-variant patches.

// Auto-reset on failure: func_bonus_800265C0 is the shared "you failed"
// entry point. It runs once, right when a fail condition is detected, and
// is what normally plays the failure text/sound and queues the outro
// cutscene. Starting the countdown in a HOOK_RETURN means it begins right
// after that logic has already set its fail state - the player briefly
// sees "3, 2, 1, GO!" over the failed barrel instead of the fail
// text/outro cutscene, then the barrel starts over.
RECOMP_HOOK_RETURN("func_bonus_800265C0") void bonus_barrel_fail_reset_hook(void) {
    countdown_start();
}

// Manual combo reset: hooked at the entry of each barrel variant's own
// per-frame update function (one physical function per src/bonus/*.c file;
// code_0.c's covers several K.Rool barrel challenges internally via its own
// switch).
RECOMP_HOOK("func_bonus_80024158") void bonus_barrel_manual_reset_hook_a(void) {
    minigame_reset_tick();
}

RECOMP_HOOK("func_bonus_8002570C") void bonus_barrel_manual_reset_hook_b(void) {
    minigame_reset_tick();
}

RECOMP_HOOK("func_bonus_800277F8") void bonus_barrel_manual_reset_hook_c(void) {
    minigame_reset_tick();
}

RECOMP_HOOK("func_bonus_8002D2F0") void bonus_barrel_manual_reset_hook_d(void) {
    minigame_reset_tick();
}

// --- Minecart Mayhem ---
//
// Same architecture as the bonus barrels: a shared win/fail pair
// (func_minecart_80024000 win, func_minecart_800240DC fail) driven by a
// companion actor at unk11C, and a single per-frame ride function
// (func_minecart_80024FD0) covering all three difficulty variants via
// current_map.

RECOMP_HOOK_RETURN("func_minecart_800240DC") void minecart_fail_reset_hook(void) {
    countdown_start();
}

RECOMP_HOOK("func_minecart_80024FD0") void minecart_manual_reset_hook(void) {
    minigame_reset_tick();
}

// --- Castle Car Race (EXPERIMENTAL - see README) ---
//
// Unlike the bonus barrels/minecart, an animal race's progress lives in a
// separate struct at the actor's unk178 (RaceActorExtra), not in the
// actor's own control_state - so on top of the usual bit-0x10 reset, a
// race reset also has to drive that struct's own stage counter (unk34)
// back to a sane value itself. See countdown_start_race/countdown_tick.
//
// Fail detection: func_race_8002B76C runs the post-race results sequence
// via a sub-step counter (unk35) advancing once per real game frame, one
// step at a time - except at the exact moment the win/fail decision is
// made (sub-step 3): a win advances it by 1 (to 4, where it then waits for
// a button press), but a fail advances it by 2 in that same tick (straight
// to 5), because the fail branch increments it once itself before the
// shared increment at the end of the switch case also runs. So "unk35 was
// 3 last frame and is 5 now" is a fail, unambiguously - it's the only way
// to reach 5 without passing through (and pausing on) 4 first.
//
// Reset target: unk34 = 1 replays the race's own "get ready" sequence
// (func_race_8002B518), which is the only stage value confirmed to be
// something the outer dispatch (func_race_8002B964) explicitly handles
// rather than falling through to undefined behavior - chosen specifically
// to fail safe if this guess is imperfect, rather than something more
// specific to "resume racing" that would need reading un-decompiled
// assembly (func_race_8002B180) to confirm.
//
// reset_minigame_actor also clears unk11C on this actor as it does for the
// other minigames; whether the race actor's unk11C plays the same
// "companion timer" role here is unconfirmed - if it's unrelated, this is
// a harmless no-op (or a stray one-time re-init on whatever it points to).
RECOMP_HOOK_RETURN("func_race_8002B76C") void castle_car_race_fail_reset_hook(void) {
    static u8 prev_unk35 = 0xFF;
    if (gCurrentActorPointer == NULL) {
        return;
    }
    RaceActorExtra *race = (RaceActorExtra *)gCurrentActorPointer->unk178;
    if (race == NULL) {
        return;
    }
    if (prev_unk35 == 3 && race->unk35 == 5) {
        countdown_start_race();
    }
    prev_unk35 = race->unk35;
}

RECOMP_HOOK("func_race_8002B964") void castle_car_race_manual_reset_hook(void) {
    race_reset_tick();
}
