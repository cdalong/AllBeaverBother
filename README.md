# Minigame Reset

Lets you instantly reset a DK64 minigame — automatically on failure, or on demand with a button
combo — instead of sitting through the fail text and outro cutscene.

Covers Jetpac (Cranky's Lab), every banana barrel bonus minigame (K.Rool barrel challenges,
Batty Barrel Bandit, Kremling Kosh, Rambi Arena), and Minecart Mayhem.

## Installation

1. Download the latest `minigame_reset.nrm` from [Releases](../../releases/latest).
2. Place it in your DK64 Recompiled mods folder:
   - **Windows:** `%LOCALAPPDATA%\DK64Recompiled\mods`
   - **Linux:** `~/.local/share/DK64Recompiled/mods`
3. Launch DK64 Recompiled and enable the mod from the mods menu.

## Controls

Hold **L + R + Z** together during a minigame to reset it immediately.

Failing a bonus barrel minigame or Minecart Mayhem also resets it immediately on its own — the
fail text/sound and outro cutscene are skipped entirely.

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

**Auto-reset on failure** hooks `func_bonus_800265C0` with `RECOMP_HOOK_RETURN`, so it runs right
after the game's own fail-state transition has been applied — meaning our reset is what actually
sticks, before the fail text/outro cutscene would otherwise play.

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

`RECOMP_HOOK_RETURN` on the fail function handles auto-reset; `RECOMP_HOOK` on the ride update
function handles the manual combo reset. Both reuse the exact same `unk11C`/bit-`0x10` reset as
the bonus barrels.

## Project Layout

| Path | Description |
|---|---|
| `src/main.c` | Hook implementations |
| `include/minigame_int.h` | Minimal Actor/Jetpac struct definitions |
| `mod.toml` | Mod metadata and packaging config |
| `Dk64Syms/` | DK64 symbol tables used by RecompModTool |
| `dk64_decomp/` | DK64 decomp headers used during compilation |

## Known Limitations

- Race minigames (e.g. Kremling Kaos, animal races) use a different win/fail subsystem than the
  bonus barrels/minecart and are not covered yet.
- The manual combo reset doesn't check whether a minigame is mid win/fail transition when pressed;
  triggering it during that window hasn't been tested.
- Built and reviewed against the decomp source, but not yet verified in-game — please report
  issues.
