# Dungeons of the Emberdeep — PS Vita

A native Vita port of the browser dungeon crawler: three procedurally generated
floors, three playable classes with their own ability kits, eight enemy types
and three bosses.

Running `make` produces **`emberdeep.vpk`**, installable with VitaShell on a
HENkaku/h-encore enabled Vita.

---

## Build

Three routes, easiest first. You do **not** need to learn VitaSDK for any of them.

### 1. GitHub Actions — nothing installed, builds in the cloud

1. Create a repository on GitHub (it can be private).
2. Upload this whole folder to it.
3. **Create the workflow file inside GitHub.** Drag-and-drop upload skips
   folders beginning with a dot, so `.github/` usually does not survive the
   upload. In the repo: **Add file -> Create new file**, type the path
   `.github/workflows/build-vpk.yml`, and paste the contents of
   `WORKFLOW-build-vpk.yml.txt` (same text, kept at the top level so it is
   visible in your file explorer). Commit it.

   It does not matter whether the project ended up at the repo root or one
   folder deep - the workflow finds the `Makefile` either way.
4. Open the **Actions** tab. The build starts on its own.
5. When it finishes, open the run and download the **emberdeep-vpk** artifact.
   Inside is `emberdeep.vpk`.

   If the Actions tab shows a grid of starter workflows ("C/C++ with Make" and
   so on), the workflow file is not in the repo yet - do step 3. Do not pick
   one of those cards; they have no VitaSDK in them.

If it fails, open the failed step and copy the log — that is everything needed
to diagnose it.

### 2. Docker — one command, builds on your machine

Install Docker Desktop, then from this folder:

```bash
sh tools/build-in-docker.sh          # macOS / Linux
```

```powershell
docker run --rm -v "${PWD}:/workspace" -w /workspace vitasdk/vitasdk:latest make   # Windows PowerShell
```

First run pulls about 1.6 GB. After that it is seconds.

### 3. A local VitaSDK install — if you already have one

```bash
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH
git clone https://github.com/Rinnegatamante/vitaGL && make -C vitaGL install
make
```

### Installing on the Vita

Copy `emberdeep.vpk` to the Vita (USB, FTP via VitaShell, or an SD card), open
it in **VitaShell**, press **X**, confirm. It appears on the LiveArea.

Requires a Vita running HENkaku / h-encore / Ensō.

If linking fails on `-lvitashark` / `-lSceShaccCgExt`, your vitaGL predates the
runtime shader compiler; remove those three flags from `LIBS` in the `Makefile`
and rebuild.

---

## Controls

| Input | Action |
|---|---|
| Left stick / D-pad | Move |
| **X** | Basic attack (also confirm / begin) |
| **□** | Ability 1 — unlocked at level 1 |
| **△** | Ability 2 — unlocked at level 3 |
| **○** | Ability 3 — unlocked at level 5 |
| Up / Down | Choose class on the title screen |

Abilities spend the class resource (Vigor / Mana / Focus), which refills on its
own, so you can't simply hold every button.

---

## The three classes

| Class | Basic | Lv1 | Lv3 | Lv5 |
|---|---|---|---|---|
| **Vanguard** | Cleave | Shield Bash (dash + stun) | Whirlwind | Bulwark (damage soak + heal) |
| **Pyromancer** | Ember Bolt | Fireball (splash + burn) | Frost Nova (slow) | Meteor |
| **Ranger** | Quick Shot | Volley (3 arrows) | Shadow Dash (i-frames) | Venom Trap |

Each floor has its own enemy set and a boss with a telegraphed moveset. Kill the
boss, a gate opens, step into it to descend. Clear floor three to win.

---

## Layout

```
src/dungeon.c    room + corridor generation, grid collision, wall sliding
src/actors.c     class/enemy tables, abilities, status effects, AI, progression
src/render.c     vitaGL drawing — CPU-batched geometry, baked lighting, HUD
src/texture.c    procedural stone/masonry/hide textures generated at load
src/font.c       5x7 bitmap font for the HUD
src/main.c       entry point, controller input, frame loop, RNG
tests/host_test.c  desktop harness that plays the game with a pathfinding bot
tests/stubs/     fake VitaSDK headers, used only for type-checking off-device
```

`dungeon.c` and `actors.c` contain no platform calls, which is what makes the
desktop test below possible.

### Rendering approach

The Vita is happiest with few, large draw calls, so geometry is transformed on
the CPU into one batch and submitted in a handful of calls:

- the static world mesh is built once per floor with **lighting baked into
  vertex colours** (nearest-brazier falloff), so no per-frame relighting;
- actors, projectiles and particles go into a second dynamic batch, lit by
  distance to the player's carried torch;
- shadows are blob quads rather than shadow maps;
- the HUD is a third batch in orthographic projection.

This is deliberately simpler than the browser build. That version leans on
normal maps, roughness maps and real-time shadow mapping, none of which the
Vita's GPU should be asked to do.

---

## Testing

The simulation compiles and runs on a desktop, driven by a bot that paths
through the dungeon and fights:

```bash
make test
```

That plays all three classes through all three floors and reports level, kills
and final state.

```bash
make check
```

Type-checks `render.c` and `main.c` against the stub headers in `tests/stubs`,
so those files can be validated without VitaSDK installed.

---

## What is and isn't verified

**Verified here:** dungeon generation, collision, combat, status effects, enemy
and boss AI, leveling, floor progression and the win/lose paths — compiled with
`-Wall -Wextra` and exercised by the bot. All three classes complete all three
floors.

**Not verified:** anything that touches hardware. `render.c` and `main.c`
type-check against stub headers but have never been compiled by
`arm-vita-eabi-gcc` or run on a Vita, because the toolchain host is unreachable
from the machine this was written on. Expect to iterate on the vitaGL link flags
and on rendering details the first time you build it.
