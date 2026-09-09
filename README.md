# mod_iv — mod_sp cheat menu for GTA IV

GTA IV port of [mod_v](https://github.com/hourafter4/mod_v) /
[mod_sp](https://github.com/hourafter4/mod_sp), the single-player port of
[mod_sa](https://github.com/BlastHackNet/mod_sa)'s cheat menu. Same hierarchical
menu, same s0beit look (bottom-center window, titlebar breadcrumb, green enabled
items, bottom HUD bar with `[Inv] [VehInv] [Tires] [Bike] [AirBrk]` tags, coords
+ speed, colored FPS), same callbacks — the cheat bodies are re-expressed with
GTA IV natives, and **GTA IV's own phone cheats** are in the menu under the
numbers you would dial.

`make` builds two .asi files from the same source, differing only in how they
reach the game (`src/backend.h`):

| File            | Game                                        | How |
|-----------------|---------------------------------------------|-----|
| `mod_iv.asi`    | GTAIV.exe **1.0.7.0 / 1.0.8.0**             | [IV-SDK](https://github.com/Zolika1351/iv-sdk), fixed addresses |
| `mod_iv_ce.asi` | **Complete Edition 1.2.0.x** (and 1.0.4-1.0.8) | pattern scanning, no fixed addresses |

Built with mingw on macOS/Linux, no Visual Studio needed. The titlebar shows
which build and which game version it detected.

**CE startup-crash fix awaiting in-game verification** — see "Status" below.

## Controls

| Key           | Action                       |
|---------------|------------------------------|
| ]             | open/close menu              |
| Up / Down     | select item                  |
| Right / Enter | activate / toggle / open submenu (Enter resets game speed, boost strength) |
| Left          | back to parent menu / close  |
| Left / Right  | decrease / increase value items (game speed, strength, colors, livery) |
| **hold Left Alt** (or B) | speed boost (on foot or car) |
| / (or num /)  | airbrake toggle: WASD + Space/X, camera-relative on foot, hold the boost key for 2x |
| Numpad +/-/*  | boost & airbrake strength    |

Boost is on **Left Alt**, the key mod_sa and mod_sp used: GTA IV leaves Alt
unbound (aim is the right mouse button, sprint Left Shift, crouch Left Ctrl),
so the reason mod_v had to move it to B — Alt switches character in GTA V —
does not apply. B still works too.

Player controls are *not* disabled while the menu is open (unlike mod_v): GTA
IV binds none of the menu keys on foot. Boost on foot speeds up the movement
animation (3x at 100% strength); in a car it pushes the forward speed, capped
at 270 km/h x strength. Seatbelt (Vehicles menu, on by default) is IV's
`SET_CHAR_WILL_FLY_THROUGH_WINDSCREEN` off.

## Menu

**My preset** — one item that toggles player+vehicle god, tire + bike fall-off
protection, and NPC health bars as a group.

**Cheats** — player god mode (`SET_CHAR_INVINCIBLE` + all proofs + HP pinned),
vehicle god mode (`SET_CAR_PROOFS` + can't be damaged, health pinned), infinite
ammo (current weapon topped up to its max), infinite sprint
(`SET_PLAYER_NEVER_GETS_TIRED`), never wanted (max wanted level 0), heal, give
$250000 (`ADD_SCORE`), lower wanted level. Then the **GTA IV phone cheats**,
each labelled with its number:

| Item                       | Number       | What it does |
|----------------------------|--------------|--------------|
| Health & armor             | 362-555-0100 | full health + armor |
| Health, armor & weapons    | 482-555-0100 | the above + weapon set 2 |
| Weapons set 1              | 486-555-0150 | Knife, Molotovs, Pistol, Pump Shotgun, Micro SMG, AK-47, Sniper Rifle, RPG |
| Weapons set 2              | 486-555-0100 | Baseball Bat, Grenades, Combat Pistol, Combat Shotgun, MP5, M4, Combat Sniper, RPG |
| Remove wanted level        | 267-555-0100 | `CLEAR_WANTED_LEVEL` |
| Raise wanted level         | 267-555-0150 | +1 star (`ALTER_WANTED_LEVEL`) |
| Change weather             | 468-555-0100 | next weather type |

Values: game speed (`SET_TIME_SCALE`, 25%–400%), gravity off (`SET_GRAVITY_OFF`).

**Weapons** — give all weapons, then every base-game weapon (16) grouped by
type, given with max ammo and selected; the currently held weapon is green.

**Vehicles** — seatbelt, bike fall-off protection
(`SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE`), tire protection (`SET_CAN_BURST_CAR_TYRES`
off), spawn vehicle, paint (colors 1–4 = `CHANGE_CAR_COLOUR` +
`SET_EXTRA_CAR_COLOURS`, livery), teleport to nearest empty car (on foot: warps
you into the driver seat; in a car: moves it on top), fix vehicle.

The **spawn** menu starts with the phone-cheat spawns, numbers included:
Annihilator 359-555-0100, Cognoscenti 227-555-0142, Comet 227-555-0175, FIB
Buffalo 227-555-0100, Jetmax 938-555-0100, NRG 900 625-555-0100, Sanchez
625-555-0150, Super GT 227-555-0168, Turismo 227-555-0147; the TLAD ones
(Innovation 245-555-0100, Hexer 245-555-0150, Double T 245-555-0125, Hakuchou
245-555-0199, Gang Burrito 826-555-0150, Slamvan 826-555-0100) and TBoGT ones
(APC 272-555-8265, Buzzard 359-555-2899, Bullet GT 227-555-9666, Akuma
625-555-0200, Vader 625-555-3273, Floater 938-555-0150) appear only when the
model is installed (`IS_MODEL_IN_CDIMAGE`). Below that, all 123 base-game
vehicles by class plus the EFLC list, filtered the same way. Spawned cars stay
script-owned (last 8) so the game doesn't cull them.

**Teleports** — to the map waypoint (finds the blip with the waypoint sprite,
streams the area, probes for ground) and ~45 places in four groups: safehouses &
hangouts (Hove Beach, South Bohan, The Lost MC clubhouse, Perestroika,
Maisonette 9, Bahama Mamas, ...), police stations & hospitals, shops & services
(gun shops, Pay 'n' Sprays, Burger Shots, TW@, ...), landmarks & misc (Happiness
Island, airport runway, prison, Bank of Liberty, ...). The `TELEPORTS` table at
the top of `src/main.cpp` is easy to extend.

**Freeze weather / Freeze time** — the 8 IV weather types (`FORCE_WEATHER`) /
any hour (`FORCE_TIME_OF_DAY`); re-select to release.

**Misc** — airbrake, boost/airbrake strength, HUD indicators, NPC health bars
(red distance-scaled bars above peds within 60 m, read from the ped pool).

### Dropped

- **TBoGT-only cheats** — explosive sniper rounds (486-555-2526), super punch
  (276-555-2666), parachute (359-555-7272): no native for them in GTAIV.exe,
  and the EFLC weapon types don't exist there either.
- **Skip mission, jetpack, nitro, muscle** (SA), **vehicle upgrades, slippery
  cars, super jump, skyfall, drunk mode** (V) — no IV equivalents.

## Status

Both builds compile and link, and the generated hook code checks out in the
disassembly (IV-SDK's trampolines; on the CE side the thread hook passes `this`
in `ecx` and returns with `ret $4`, matching the `__thiscall` it replaces).
The CE build was reported to crash when entering story mode. The current
build fixes an incorrect operand offset in the running-script-thread lookup:
CE needs byte +11, but the original code read +10 and then accessed a malformed
pointer on the first script tick. Hook installation now requires that thread
pointer, and the menu waits until the native table and game timer resolve.
Synthetic CE/legacy instruction tests confirm the offset fix and thread-pointer
restoration; the rebuilt CE binary still needs an in-game story-mode retest.
The normal IV-SDK build has not been validated in-game.

Most likely to need a tweak on first run, both builds: `TEXT_SX/TEXT_SY/ROW_H`
(IV text scale vs. row height), whether `GET_STRING_WIDTH_WITH_STRING` and
`GET_VIEWPORT_POSITION_OF_COORD` return 0..1 units (assumed, like `DRAW_RECT`),
and the camera-rotation axes used by the on-foot airbrake.

CE build only: every pattern is second-hand, so if the Rockstar patch level
differs from the one Rainbomizer and FusionFix target, the native table lookup
is the piece that has to be re-found first (nothing else runs without it).
If the current-thread pattern is missing, the hook is not installed; if native
registration is incomplete, the original game scripts keep running while the
mod waits.

## How the Complete Edition build works

IV-SDK is a list of addresses for 1.0.7.0 and 1.0.8.0, so it refuses to
initialise on the Complete Edition. `mod_iv_ce.asi` therefore keeps IV-SDK's
native *wrappers* and replaces the three things that are version-specific with
byte-pattern lookups at load time (`src/backend_ce.h`):

- **Natives.** The game keeps its natives in a hash table. The backend probes
  its hash format using Rainbomizer's translation table; the SDK wrapper hashes
  already match the CE values and pass through unchanged. If the table pattern
  does not resolve, the .asi installs nothing at all.
- **The per-frame tick.** `GtaThread::Run` is hooked in the script thread
  vftable. It runs for every script thread every frame, inside the game's
  script processing, which is the context the drawing and cheat natives expect;
  the game timer gates it to one tick per frame. A dummy script thread is
  installed around the tick, as IV-SDK does.
- **The ped and vehicle pools**, used by NPC health bars and "teleport to
  nearest empty car". These are optional: if their patterns miss, those two
  features go quiet and the rest of the menu still works.

Patterns and the hash table come from other people's work on these versions:
[IV.EFLC.Rainbomizer](https://github.com/Parik27/IV.EFLC.Rainbomizer) (native
table, running thread, thread vftable, and `sdk/patterns/native_hash_ce.h`
verbatim), [GTAIV.EFLC.FusionFix](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix)
(pool patterns and the pool handle layout), and the CitizenFX pattern scanner
(`sdk/patterns/Patterns.*`).

## Build

```sh
make          # both .asi files; needs mingw-w64 (brew install mingw-w64)
make mod_iv.asi mod_iv_ce.asi    # or one at a time
```

GTA IV is 32-bit, so everything is built with the i686 toolchain.
`sdk/ivsdk/` is IV-SDK (GPLv3) with a handful of mingw patches, documented in
`sdk/ivsdk/README-mingw.md`. The vehicle and weapon tables are hand-written
(`src/vehicles_iv.h`, `src/weapons_iv.h`).

## Install

1. An ASI loader for GTA IV (e.g. [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
   as `dinput8.dll`; the Complete Edition needs one too, and ZolikaPatch or
   FusionFix ship one).
2. Copy the .asi for your game next to `GTAIV.exe`: `mod_iv.asi` for
   1.0.7.0 / 1.0.8.0, `mod_iv_ce.asi` for the Complete Edition. Installing
   both at once would run two menus, so pick one.

Single-player only.

License: GPLv3 (derived from mod_sp / mod_sa / mod_v; IV-SDK, Rainbomizer's
translation table and FusionFix are GPLv3 as well).
