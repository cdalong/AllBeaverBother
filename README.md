# Minigame Reset

Every bonus barrel, Jetpac, animal race, and Minecart Mayhem attempt loads Beaver Bother instead.

**This is a joke mod, not a completionist tool.** Since the real minigame you tried to enter never
actually loads, winning only ever completes Beaver Bother - not whatever Golden Banana/crown/coin
the thing you actually walked into would have granted. See [Known Limitations](#known-limitations).

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

A map transition is requested elsewhere in the game by writing the global `next_map` and setting a
countdown (`D_global_asm_8076A0B2 = 3`, see `func_global_asm_805FF378` in `dk64_decomp`
`src/global_asm/code_3C10.c`); the countdown decrements once per frame and the transition actually
completes - loading the new map's real room and actors - when it reaches 0. That leaves a ~3 frame
window, every frame, where a pending transition's target can still be swapped before the real load
happens.

This mod subscribes to `dk64recomp_every_frame` - a real event the base game declares and fires
itself every frame (see `patches/boot_logos_patches.c` in the upstream
Rainchus/Donkey-Kong-64-Recompiled repo) - via `RECOMP_CALLBACK("*", dk64recomp_every_frame)`. Each
frame, if a transition is pending and its target is one of the minigame maps below, the target gets
overwritten to `MAP_BEAVER_BOTHER_EASY` before the load happens - so the room and actors that
actually get loaded are Beaver Bother's own, not just a `current_map` value lied about over the
wrong room.

Covered maps: every K.Rool barrel challenge, Batty Barrel Bandit, Kremling Kosh, Rambi/Enguarde
Arena, Jetpac, the animal races (both beetle races, both car races, the seal race), and all three
Minecart Mayhem difficulties.

**Not covered on purpose:** the multiplayer Battle Arena and Kong Battle Arena maps (not reachable
the same way in single-player, untested), and `MAP_KROOLS_ARENA` (the final boss fight room -
redirecting that would likely make the game unbeatable).

### Why RECOMP_CALLBACK and not a hook or a patch

Earlier versions of this mod used `RECOMP_HOOK`/`RECOMP_HOOK_RETURN` to extend several minigames
with a manual reset combo, an instant-win combo, and a countdown overlay, without needing to fully
reimplement any game logic. In testing, both hook mechanisms turned out to be unreliable in this
game's actual runtime build:

- `RECOMP_HOOK_RETURN` reliably crashed the game while loading the mod, for any function - tested
  on two unrelated functions, both crashed.
- `RECOMP_HOOK` (entry) avoided that specific crash, but further testing showed the identical
  compiled mod crashing at different, inconsistent points across repeated launches (mod load,
  entering a minigame, or dying) - the signature of memory corruption, not a deterministic bug.

This is consistent with the fact that DK64 Recompiled's own developers never use `RECOMP_HOOK` or
`RECOMP_HOOK_RETURN` anywhere in their own built-in patches - only `RECOMP_PATCH`. An earlier
version of this mod used `RECOMP_PATCH` instead (fully reimplementing Jetpac's round-end dispatcher
to auto-respawn on game over), which was reliable but meant every feature needed its own large,
fully-reimplemented function, and auto-reset for bonus barrels/Minecart Mayhem wasn't safely
achievable that way (their fail path doesn't have a small, isolated interception point the way
Jetpac's did).

`RECOMP_CALLBACK` turned out to be a third, different mechanism: it subscribes to an event the game
already declares and fires itself, implemented in the runtime as a plain list of function pointers
the game calls directly - not the same trampoline/code-regeneration machinery hooks use. It worked
reliably in testing, which is what made this mod's current approach possible without reimplementing
any large minigame function at all.

## Project Layout

| Path | Description |
|---|---|
| `src/main.c` | The map-redirect callback |
| `mod.toml` | Mod metadata and packaging config |
| `Dk64Syms/` | DK64 symbol tables used by RecompModTool |
| `dk64_decomp/` | DK64 decomp headers used during compilation |

## Known Limitations

- **Golden Bananas, crowns, and the Rareware Coin cannot be earned from a redirected minigame**
  while this mod is enabled. Completing Beaver Bother only ever grants whatever Beaver Bother
  itself grants (if anything) - it does not know or care which real challenge you originally tried
  to enter, and DK64's collectible flags for these challenges appear to be tied to level-placement
  data on the specific barrel/actor instance, not to the map type, so there's no way found so far to
  look up and separately grant "what you would have earned." Don't use this mod on a save you care
  about 100%-ing.
- Beetle Race, Car Race, and Seal Race redirect the map transition but haven't been individually
  tested in-game.
- Built and reviewed against the decomp source; please report any issues.
