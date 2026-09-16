# Minigame Reset

Jetpac (Cranky's Lab) never really ends: when you'd normally see "GAME OVER", this respawns you
instead, so a run only stops when you decide to leave.

## Installation

1. Download the latest `minigame_reset.nrm` from [Releases](../../releases/latest).
2. Place it in your DK64 Recompiled mods folder:
   - **Windows:** `%LOCALAPPDATA%\DK64Recompiled\mods`
   - **Linux:** `~/.local/share/DK64Recompiled/mods`
3. Launch DK64 Recompiled and enable the mod from the mods menu.

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

`func_jetpac_80025368` is the per-round-end dispatcher, called once each time a player dies. It's
fully replaced with `RECOMP_PATCH`, reimplementing the original logic (see `dk64_decomp`
`src/jetpac/code_0.c`) unchanged except in one place: where the original would end the game for
good (state 5 - no lives left, not already game-over), this instead respawns the player (state 2)
the same way the surrounding code already does for every other "still has lives" case. The
two-player switching logic is untouched.

This patches a **different** function than the separate `infinite_jetpac_lives` mod
(`func_jetpac_80026A3C`, the death-timer handler, vs `func_jetpac_80025368` here), so the two
don't conflict if both happen to be enabled - they just produce a similar effect via different
means.

### Why RECOMP_PATCH and not a hook

Earlier versions of this mod used `RECOMP_HOOK`/`RECOMP_HOOK_RETURN` to extend several minigames
(Jetpac, bonus barrels, Minecart Mayhem) with a manual reset combo, an instant-win combo, and a
countdown overlay, without needing to fully reimplement any game logic. In testing, both hook
mechanisms turned out to be unreliable in this game's actual runtime build:

- `RECOMP_HOOK_RETURN` reliably crashed the game while loading the mod, for any function, even
  ones the base game doesn't otherwise touch.
- `RECOMP_HOOK` (entry) avoided that specific crash, but further testing showed the identical
  compiled mod crashing at different, inconsistent points across repeated launches (mod load,
  entering a minigame, or dying) - the signature of memory corruption, not a deterministic bug.
  With zero mods enabled, or with only `RECOMP_PATCH`-based mods, the game was completely stable.

This is consistent with the fact that DK64 Recompiled's own developers never use `RECOMP_HOOK` or
`RECOMP_HOOK_RETURN` anywhere in their own built-in patches - only `RECOMP_PATCH`. That mechanism
was confirmed reliable through extensive testing, so this mod now uses it exclusively, and the
manual reset combo, win combo, and countdown were removed rather than shipped on a foundation that
doesn't hold up.

## Project Layout

| Path | Description |
|---|---|
| `src/main.c` | The Jetpac patch |
| `include/minigame_int.h` | Minimal `Competitor`/`JetpacGameStruct` definitions |
| `mod.toml` | Mod metadata and packaging config |
| `Dk64Syms/` | DK64 symbol tables used by RecompModTool |
| `dk64_decomp/` | DK64 decomp headers used during compilation |

## Known Limitations

- **Only Jetpac is covered.** The original goal included auto-reset for bonus barrel minigames and
  Minecart Mayhem too, but their fail path doesn't have a Jetpac-style clean interception point:
  the small function that shows the fail message isn't what triggers the outro cutscene or
  actually restarts the minigame - the *caller* (a large, per-frame state-machine function) does
  that unconditionally right afterward, regardless of what the small function does. Achieving
  auto-reset there safely would mean either fully reimplementing that large function as a
  `RECOMP_PATCH` (high effort, high risk of subtly wrong behavior) or finding a smaller,
  Jetpac-like interception point earlier in the chain (not yet found). Left out rather than
  shipped as a hook-based feature known to be unreliable.
- No manual reset combo, no instant-win combo, no "3, 2, 1, GO!" countdown - see above for why.
- Built and reviewed against the decomp source and tested in-game on the specific crash this
  README describes; please report any other issues.
