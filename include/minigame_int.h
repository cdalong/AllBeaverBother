#ifndef MINIGAME_INTERNAL_H
#define MINIGAME_INTERNAL_H

// --- Jetpac (src/jetpac/code_0.c) ---
//
// Minimal mirrors of the real Competitor/JetpacGameStruct (see dk64_decomp
// include/structs.h), built from their authoritative field offsets rather
// than hand-duplicating the full nested struct internals (JetpacPickupStruct
// etc.) - only level/current_score/lives are ever read/written directly,
// and Competitor's total size (0x194, confirmed by the real struct's
// player[2] array followed by player_index at a known offset) has to be
// exactly right for `player[N]` array indexing to compute correct
// addresses, or this silently corrupts memory.
typedef struct {
    s32 level;
    s32 current_score;
    s32 lives;
    u8 pad0C[0x194 - 0x0C];
} Competitor;

typedef struct {
    u8 pad0[0x18];
    s32 unk18; // high score, compared against a losing player's current_score
    Competitor player[2];
    s32 player_index;
    u8 pad348[0x78C - 0x348];
    s32 unk78C;
    u8 pad790[0x798 - 0x790];
    u8 unk798; // difficulty flag: nonzero = 3 starting lives, zero = 5
} JetpacGameStruct;

extern JetpacGameStruct D_jetpac_8002EC30;

void func_jetpac_80024A4C(void);
void func_jetpac_800250A0(void);
void func_jetpac_80024F9C(s32 arg0);

#endif /* MINIGAME_INTERNAL_H */
