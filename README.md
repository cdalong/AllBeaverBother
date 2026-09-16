# Minigame Reset

Lets you reset a DK64 minigame — automatically on failure, or on demand with a button combo —
instead of sitting through the fail text and outro cutscene. A brief "3, 2, 1, GO!" overlay plays
before you're dropped back into the fresh attempt.

Covers Jetpac (Cranky's Lab), every banana barrel bonus minigame (K.Rool barrel challenges,
Batty Barrel Bandit, Kremling Kosh, Rambi Arena), Minecart Mayhem, and (experimentally — see
[Known Limitations](#known-limitations)) the Creepy Castle Car Race.

## Installation

1. Download the latest `minigame_reset.nrm` from [Releases](../../releases/latest).
2. Place it in your DK64 Recompiled mods folder:
   - **Windows:** `%LOCALAPPDATA%\DK64Recompiled\mods`
   - **Linux:** `~/.local/share/DK64Recompiled/mods`
3. Launch DK64 Recompiled and enable the mod from the mods menu.

## Controls

Hold **L + R + Z** together during a minigame to trigger a reset.

Failing a bonus barrel minigame, Minecart Mayhem, or the Castle Car Race also triggers a reset on
its own — either way,
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

### Minecart Mayhem

Same architecture as the bonus barrels, just with its own function names:

- `func_minecart_80024000` — win
- `func_minecart_800240DC` — fail
- `func_minecart_80024FD0` — per-frame ride update (covers all three difficulties via `current_map`)

`RECOMP_HOOK_RETURN` on the fail function starts the countdown; `RECOMP_HOOK` on the ride update
function handles the manual combo trigger and ticks the countdown every frame. Both reuse the
exact same `unk11C`/bit-`0x10` reset as the bonus barrels.

### Castle Car Race (experimental)

Animal races don't work like the other minigames: a race's progress lives in its own struct
(`RaceActorExtra`, pointed to by the actor's `unk178`) rather than in the actor's `control_state`,
so the standard bit-`0x10` reset alone isn't enough — it resets the actor but leaves the race
itself stuck on whatever stage (results/fail screen) it was on.

- **Fail detection** hooks `func_race_8002B76C` (the results sequence) with `RECOMP_HOOK_RETURN`,
  watching its `unk35` sub-step counter. A win advances it by 1 on the decision frame; a fail
  advances it by 2 in that same frame (the fail branch increments it once itself, on top of the
  shared increment every case gets) — so "was 3 last frame, is 5 now" is an unambiguous fail
  signal, distinct from a win (which always passes through 4 first).
- **Reset** additionally sets `RaceActorExtra.unk34 = 1`, replaying the race's own "get ready"
  sequence. This value is a **best-effort guess** — it's the only stage the outer dispatch
  (`func_race_8002B964`) explicitly handles that plausibly means "back to the start line" without
  needing to read `func_race_8002B180`'s un-decompiled assembly (which is where the real
  finish-line/stage-transition logic lives). See [Known Limitations](#known-limitations).

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
| `include/minigame_int.h` | Minimal Actor/race struct definitions |
| `mod.toml` | Mod metadata and packaging config |
| `Dk64Syms/` | DK64 symbol tables used by RecompModTool |
| `dk64_decomp/` | DK64 decomp headers used during compilation |

## Known Limitations

- **Castle Car Race support is experimental and unverified in-game.** The fail-detection logic is
  high-confidence, but the reset target (`unk34 = 1`) is a best-effort guess, not a confirmed
  value — see [Castle Car Race (experimental)](#castle-car-race-experimental). If it misbehaves
  (race doesn't restart properly, camera/controls end up in a weird state), please report it.
- Other Animal Races (beetle, seal, Frantic Factory car race) are not covered yet.
  - **Gloomy Galleon Seal Race was investigated and deliberately skipped.** It first looked like
    the safest next candidate (its stage counter, unlike Beetle/Factory, does get reset by the
    same init block the bit-`0x10` trick reruns). But that same init block also unconditionally
    replays the intro cutscene and repositions the actor every time it reruns — unlike Castle,
    Seal Race's cutscene isn't gated behind a one-time flag, so the standard reset trick would
    bring the cutscene *back*, defeating the point of the mod. A more surgical fix (reset the
    stage fields directly, skip bit-`0x10` entirely) avoids the cutscene, but doing a *complete*
    reset also means repositioning the seal to the start line, which depends on globals the DK64
    decomp project's own headers mark as `// TODO: What is this datatype?` — unlike Castle's
    single guessed value, this is a real crash-risk-level unknown (wrong struct layout, not just
    wrong stage number), so it was left out rather than shipped.
  - Beetle Race and Frantic Factory Car Race carry a similar-in-spirit but distinct risk: their
    stage counter isn't assigned anywhere in the file that reads it, meaning it's set once
    elsewhere and untouched by the bit-`0x10` trick, so a reset would likely leave stale
    race-stage state behind. Not investigated as deeply as Seal Race yet.
- The countdown's ~1 second-per-step pacing is a rough estimate of the game's logic tick rate, not
  a confirmed value — it may run faster or slower in practice.
- The manual combo reset doesn't check whether a minigame is mid win/fail transition when pressed;
  triggering it during that window hasn't been tested.
- Built and reviewed against the decomp source, but not yet verified in-game — please report
  issues.
