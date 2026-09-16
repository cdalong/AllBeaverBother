#ifndef MINIGAME_INTERNAL_H
#define MINIGAME_INTERNAL_H

// Minimal Actor definition: only the fields this mod touches, at their real
// offsets (padding fills the rest). See dk64_decomp include/structs.h
// `struct actor` for the authoritative layout.
typedef struct MinigameActor MinigameActor;
struct MinigameActor {
    u8 pad0[0x60];
    u32 object_properties_bitfield; // at 0x60 - bit 0x10 = "one-time init already ran"
    u8 pad64[0x11C - 0x64];
    MinigameActor *unk11C;          // at 0x11C - companion actor (timer/result driver for bonus barrels)
    u8 pad120[0x154 - 0x120];
    u8 control_state;               // at 0x154
    u8 control_state_progress;      // at 0x155
};

extern MinigameActor *gCurrentActorPointer;
extern MinigameActor *gPlayerPointer;

void func_jetpac_80024F9C(s32 arg0);

// Bonus barrel minigames (src/bonus/*.c) - shared win/fail entry points used
// by every barrel variant (K.Rool barrel tests, Batty Barrel Bandit,
// Kremling Kosh, Rambi Arena, etc).
void func_bonus_800264E0(u8 arg0, u8 textIndex);
void func_bonus_800265C0(u8 arg0, u8 textIndex);

#endif /* MINIGAME_INTERNAL_H */
