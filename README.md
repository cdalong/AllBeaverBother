# Minigame Reset

Lets you reset a DK64 minigame — automatically on failure, or on demand with a button combo —
instead of sitting through the fail text and outro cutscene, with a brief "3, 2, 1, GO!" overlay
before you're dropped back into the fresh attempt. A second combo instantly wins the minigame
you're playing.

Covers Jetpac (Cranky's Lab), every banana barrel bonus minigame (K.Rool barrel challenges,
Batty Barrel Bandit, Kremling Kosh, Rambi Arena), and Minecart Mayhem.

## Installation

1. Download the latest `minigame_reset.nrm` from [Releases](../../releases/latest).
2. Place it in your DK64 Recompiled mods folder:
   - **Windows:** `%LOCALAPPDATA%\DK64Recompiled\mods`
   - **Linux:** `~/.local/share/DK64Recompiled/mods`
3. Launch DK64 Recompiled and enable the mod from the mods menu.

## Controls

Hold **L + R + Z** together during a minigame to trigger a reset.

Hold **L + R + C-Up** together to instantly win the bonus barrel or Minecart Mayhem you're
currently playing. Not available for Jetpac — it doesn't have a single "win the level" moment the
same way, just an ongoing score.

Failing a bonus barrel minigame or Minecart Mayhem also triggers a reset on its own — either way,
the fail text/sound and outro cutscene are skipped and replaced with a short "3, 2, 1, GO!"
countdown before the minigame starts over.

Jetpac resets are instant, with no countdown — it's a fast arcade loop and has no cutscenes to
skip in the first place.

## Building from Source

### Requirements

- `clang` with MIPS target support (LLVM, not Apple clang)
- `ld.lld`
- `make`

> **macOS:** Apple clang does not support MIPS. Install LLVM via `brew install llvm` and pass
> `CC=/opt/homebrew/opt/llvm/bin/clang LD=/opt/homebrew/opt/llvm/bin/ld.lld` to make.

### Steps

```bash
git clone --recurse-submodules <this-repo-url>
cd MinigameReset
make
./build.sh
```

The mod file is written to `bin/minigame_reset.nrm`.

You can also output directly to your mods folder instead of using `build.sh`:

```bash
./RecompModTool mod.toml ~/.local/share/DK64Recompiled/mods
```

## How It Works

### Jetpac

`func_jetpac_80025368` is the per-round-end dispatcher: based on remaining lives it either ends
the game, respawns the current player, or returns to the title. Jetpac has no cutscenes, so
there's nothing to skip — a `RECOMP_HOOK_RETURN` on that function runs after the real dispatcher
has decided the outcome, and if the reset combo is held, forces an immediate respawn
(`func_jetpac_80024F9C(2)`) regardless of what it decided.

### Bonus barrel minigames

Every barrel variant funnels its outcome through two functions shared by the whole overlay:

- `func_bonus_800264E0` — universal win
- `func_bonus_800265C0` — universal fail

**Auto-reset on failure** hooks `func_bonus_800265C0` with `RECOMP_HOOK_RETURN`, so it fires right
after the game's own fail-state transition has been applied, starting the countdown before the
fail text/outro cutscene would otherwise play.

**Manual combo reset** hooks each barrel variant's own per-frame update function with
`RECOMP_HOOK` (entry, before the original body runs):

| Function | File |
|---|---|
| `func_bonus_80024158` | K.Rool barrel challenges, Mad Maze Maul, Stealthy Snoop |
| `func_bonus_8002570C` | Batty Barrel Bandit |
| `func_bonus_800277F8` | Kremling Kosh |
| `func_bonus_8002D2F0` | Rambi Arena |

Both reset paths work the same way: they clear bit `0x10` of the actor's
`object_properties_bitfield` and zero its `control_state`/`control_state_progress` (also doing
the same to its companion actor at `unk11C`, which drives the fail/timer condition for most
variants). That bit means "one-time setup for this actor already ran" — the engine's generic
per-actor update pass (`func_global_asm_80678CC8`) sets it back to 1 unconditionally at the end of
every frame, and each barrel variant's per-frame function only checks it at the very top to decide
whether to (re)run its one-time setup. Clearing it makes the barrel replay exactly the same
one-time setup a fresh spawn would, in place, on the next frame — which is the cleanest available
way to get a full reset without hand-tracking every variant's private counters/timers.

**Manual combo win** calls `func_bonus_800264E0` directly with `textIndex 0` — the value
`code_12A0.c`'s own win call uses, and bank `0x1A` is shared by the whole bonus overlay, so it's a
message guaranteed valid no matter which variant is active.

### Minecart Mayhem

Same architecture as the bonus barrels, just with its own function names:

- `func_minecart_80024000` — win
- `func_minecart_800240DC` — fail
- `func_minecart_80024FD0` — per-frame ride update (covers all three difficulties via `current_map`)

`RECOMP_HOOK_RETURN` on the fail function starts the countdown; `RECOMP_HOOK` on the ride update
function handles the manual combo trigger and ticks the countdown every frame. Both reuse the
exact same `unk11C`/bit-`0x10` reset as the bonus barrels. The win combo calls `func_minecart_80024000`
directly with `textIndex 0xE` (the value `code_0.c`'s own win call uses) and `arg0=0` to skip the
outro cutscene the real win path optionally plays.

### The countdown

Reset doesn't happen instantly — it queues a ~4 second "3, 2, 1, GO!" countdown (`main.c`'s
`countdown_start`/`countdown_tick`), then performs the actual actor reset once it elapses. Only
one minigame can be active at a time, so the mod tracks a single pending countdown rather than
one per actor.

The countdown text is drawn with `printStyledText`, a pure display-list function (it just appends
draw commands to the `Gfx*` list and returns the advanced pointer — no allocation or handle to
free) queued each frame via `addActorToTextOverlayRenderArray`, the same mechanism the bonus
barrels already use for their own on-screen hint text. Because that queue is immediate-mode (a
draw callback only stays visible if re-enqueued every frame it should appear), the countdown has
to be driven by a per-frame tick rather than a one-shot registration — which is why every
minigame's manual-reset hook now calls a shared `minigame_reset_tick()` each frame instead of
resetting directly.

The combo trigger is edge-detected (`reset_combo_pressed`), so holding L+R+Z doesn't restart the
countdown on every frame it's held.

## Project Layout

| Path | Description |
|---|---|
| `src/main.c` | Hook implementations |
| `include/minigame_int.h` | Minimal Actor struct definitions |
| `mod.toml` | Mod metadata and packaging config |
| `Dk64Syms/` | DK64 symbol tables used by RecompModTool |
| `dk64_decomp/` | DK64 decomp headers used during compilation |

## Known Limitations

- **No Animal Races are covered** (Castle Car Race, Gloomy Galleon Seal Race, Beetle Race, Frantic
  Factory Car Race). Unlike the bonus barrels/minecart, they don't have a shared win/fail
  chokepoint or a stage counter cleanly reset by the bit-`0x10` trick, and each was investigated
  and found to carry a real risk that outweighed shipping it:
  - **Castle Car Race** was implemented and later removed. Its fail detection was solid, but the
    reset target depended on a best-effort guess (the real finish-line/stage-transition logic
    lives in un-decompiled assembly), and it was pulled rather than keep shipping an unverified
    guess.
  - **Gloomy Galleon Seal Race** looked like the next-safest candidate (its stage counter, unlike
    Beetle/Factory, does get reset by the same init block the bit-`0x10` trick reruns) — but that
    same init block also unconditionally replays the intro cutscene every time it reruns, and a
    *complete* reset would also need repositioning logic built on globals the DK64 decomp
    project's own headers mark as `// TODO: What is this datatype?` — a real crash risk, not just
    a cosmetic one. Left out.
  - **Beetle Race and Frantic Factory Car Race** are worse still: their stage counter isn't
    assigned anywhere in the files that read it — not even in an un-matched reference translation
    like Castle/Seal had — so there's no concrete basis for a reset target at all, just contextual
    inference. Factory Car Race's win/fail decision doesn't even appear to live in the same file
    as its main per-frame logic.
- The countdown's ~1 second-per-step pacing is a rough estimate of the game's logic tick rate, not
  a confirmed value — it may run faster or slower in practice.
- Neither combo checks whether a minigame is mid win/fail transition when pressed; triggering
  either during that window hasn't been tested.
- The win combo doesn't check that a Golden Banana/reward hasn't already been collected for the
  current barrel/minecart run — it just calls the same win path the game itself uses, so it should
  behave the same as a legitimate win, but this hasn't been verified for every variant.
- Built and reviewed against the decomp source, but not yet verified in-game — please report
  issues.
