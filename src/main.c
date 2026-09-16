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

// Actors are re-initialized by the engine's generic per-frame actor
// processing (func_global_asm_80678CC8) the first time it sees bit 0x10
// unset, then re-arms the bit at the end of the same frame. Clearing it here
// makes the barrel's own state-machine perform a full, in-place reset next
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

static void reset_current_bonus_barrel(void) {
    if (!reset_combo_held()) {
        return;
    }
    reset_actor(gCurrentActorPointer);
    if (gCurrentActorPointer != NULL) {
        reset_actor(gCurrentActorPointer->unk11C);
    }
}

// --- Jetpac (arcade minigame in Cranky's Lab) ---
//
// func_jetpac_80025368 is the per-round-end dispatcher: it decides whether
// to end the game (state 5), respawn the current player (state 2), or
// return to the title (state 0) based on remaining lives. Jetpac has no
// cutscenes, so there is nothing to skip - we just let the real dispatcher
// run, then force an immediate respawn if the reset combo is held.
// RECOMP_HOOK_RETURN gives no access to the original arguments/return value,
// but we don't need them here - just the "after" timing.
RECOMP_HOOK_RETURN("func_jetpac_80025368") void jetpac_round_end_reset_hook(void) {
    if (reset_combo_held()) {
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
// cutscene. Running our reset in a HOOK_RETURN means it fires after that
// logic has already set its fail state, so our reset is the one that sticks
// - the player never sees the fail screen or outro cutscene, the barrel
// just immediately starts over.
RECOMP_HOOK_RETURN("func_bonus_800265C0") void bonus_barrel_fail_reset_hook(void) {
    reset_actor(gCurrentActorPointer);
    if (gCurrentActorPointer != NULL) {
        reset_actor(gCurrentActorPointer->unk11C);
    }
}

// Manual combo reset: hooked at the entry of each barrel variant's own
// per-frame update function (one physical function per src/bonus/*.c file;
// code_0.c's covers several K.Rool barrel challenges internally via its own
// switch). An entry hook runs before the original body, so if the combo is
// held we can clear the init-gate bit in time for the same frame's
// "was I already initialized" check to see it and rerun setup - restarting
// the barrel instantly, mid-play, without waiting for a fail condition.
RECOMP_HOOK("func_bonus_80024158") void bonus_barrel_manual_reset_hook_a(void) {
    reset_current_bonus_barrel();
}

RECOMP_HOOK("func_bonus_8002570C") void bonus_barrel_manual_reset_hook_b(void) {
    reset_current_bonus_barrel();
}

RECOMP_HOOK("func_bonus_800277F8") void bonus_barrel_manual_reset_hook_c(void) {
    reset_current_bonus_barrel();
}

RECOMP_HOOK("func_bonus_8002D2F0") void bonus_barrel_manual_reset_hook_d(void) {
    reset_current_bonus_barrel();
}
