# All Beaver Bother

Every bonus barrel, Jetpac, animal race, Minecart Mayhem, and single-player Battle Arena attempt
loads Beaver Bother instead.

**Started as a joke mod, turned out rewards mostly still work.** The real minigame you tried to
enter never actually loads - you always play Beaver Bother - but that doesn't mean the actual
reward is lost:

- **Bonus barrels** (K.Rool barrel challenges, Batty Barrel Bandit, Kremling Kosh, Rambi/Enguarde
  Arena, and the full pool of Teetering Turtle Trouble/Stealthy Snoop/Mad Maze Maul/Stash
  Snatch/Busy Barrel Barrage/Splish Splash Salvage/Speedy Swing Sortie/Krazy Kong Klamour/Big Bug
  Bash/Searchlight Seek/Peril Path Panic): the Golden Banana is granted correctly, for real,
  confirmed in-game. The reward isn't computed inside the minigame at all - it's the physical
  barrel actor back in the world that grants it once it sees a win, and the redirect never touches
  that actor. See [How It Works](#how-it-works).
- **Single-player Battle Arenas**: the crown is granted correctly too, via a small extra fix in
  this mod (spawning the real crown actor for you to collect), confirmed working for Beaver Brawl.
- **Animal races and Minecart Mayhem**: not yet confirmed either way.

See [Known Limitations](#known-limitations) for what's left.

## Installation

1. Download the latest `all_beaver_bother.nrm` from [Releases](../../releases/latest).
2. Place it in your DK64 Recompiled mods folder:
   - **Windows:** `%LOCALAPPDATA%\DK64Recompiled\mods`
   - **Linux:** `~/.config/DK64Recompiled/mods`
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
cd AllBeaverBother
make
./build.sh
```

The mod file is written to `bin/all_beaver_bother.nrm`.

You can also output directly to your mods folder instead of using `build.sh`:

```bash
./RecompModTool mod.toml ~/.config/DK64Recompiled/mods
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
Arena, the full pool of other bonus-barrel minigames (Teetering Turtle Trouble, Stealthy Snoop, Mad
Maze Maul, Stash Snatch, Busy Barrel Barrage, Splish Splash Salvage, Speedy Swing Sortie, Krazy Kong
Klamour, Big Bug Bash, Searchlight Seek, Peril Path Panic), Jetpac, the animal races (both beetle
races, both car races, the seal race), all three Minecart Mayhem difficulties, and all ten
single-player Battle Arenas (Beaver Brawl, Kritter Karnage, Arena Ambush, More Kritter Karnage,
Forest Fracas, Bish Bash Brawl, Kamikaze Kremlings, Plinth Panic, Pinnacle Palaver, Shockwave
Showdown).

**Not covered on purpose:** the Kong Battle Arena maps (multiplayer-specific, untested), and
`MAP_KROOLS_ARENA` (the final boss fight room - redirecting that would likely make the game
unbeatable).

### Why most rewards still work

Bonus barrel Golden Bananas turned out not to depend on which minigame map actually loaded at all.
The reward is computed and spawned by `BonusBarrelCode` - the state machine for the *physical
barrel actor sitting in the overworld* (`dk64_decomp` doesn't have this decompiled; see the
Ghidra project referenced in project notes) - using that actor's own `barrel_index`, which comes
from level placement data and is never touched by this mod's redirect. Once you win (any) redirected
minigame and return, that untouched barrel actor sees the same "you succeeded" signal it always
would have and grants its own real, correct Golden Banana - completely independent of what you
actually played inside.

Battle Arena crowns don't work this way - their reward is computed by an actor placed only inside
the *arena's own room*, which never loads at all once redirected (Beaver Bother's room doesn't have
it). For those, this mod watches for Beaver Bother's own win cutscene (cutscene 33 on Easy - not yet
re-confirmed on Hard, which this mod now redirects to) and directly
spawns the real crown actor - with the correct flag for whichever arena you actually walked into -
at your current position, via the game's own `func_global_asm_806A5DF0`/`spawnActorWithFlag` call
(the same one the arena's own crown-granting code would have used). Spawning it at your own position
means you're already standing on it, so the game's own real pickup/collision code grants it for
real - this mod never writes the flag itself.

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

- Beaver Bother's own win reward (if any) also fires alongside a redirected Battle Arena's crown,
  since Beaver Bother doesn't know it's standing in for something else. Lower priority - not yet
  addressed.
- Animal races (Beetle Race, Car Race, Seal Race) redirect the map transition, but their reward
  (and possibly even returning you to the right spot afterward) is suspected broken and hasn't been
  fixed. Their checkpoint/reward actors live inside the race's own map (never loads once
  redirected, like Battle Arenas), and the function that returns you to the overworld
  (`initMapChangeOnRaceExit`) looks up `CurrentMap` in a table keyed by the real race maps - since
  that's now Beaver Bother, it may not navigate you back correctly either. Investigated, not fixed;
  left as-is.
- Minecart Mayhem redirects the map transition correctly but its reward behavior hasn't been
  confirmed in-game.
- Only Beaver Brawl has been individually confirmed for the Battle Arena crown fix; the other nine
  arenas go through the same code path and *should* work identically, but haven't been tested one
  by one.
- The Rareware Coin's mechanism hasn't been investigated.
- Built and reviewed against the decomp source; please report any issues.
