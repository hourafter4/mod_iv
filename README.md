# mod_iv.asi — mod_sp cheat menu for GTA IV

GTA IV port of [mod_v](https://github.com/hourafter4/mod_v) /
[mod_sp](https://github.com/hourafter4/mod_sp), the single-player port of
[mod_sa](https://github.com/BlastHackNet/mod_sa)'s cheat menu. Same hierarchical
menu, same s0beit look (bottom-center window, titlebar breadcrumb, green enabled
items, bottom HUD bar with `[Inv] [VehInv] [Tires] [Bike] [AirBrk]` tags, coords
+ speed, colored FPS), same callbacks — the cheat bodies are re-expressed with
GTA IV natives, and **GTA IV's own phone cheats** are in the menu under the
numbers you would dial.

Runs on **GTAIV.exe 1.0.7.0 and 1.0.8.0** (what
[IV-SDK](https://github.com/Zolika1351/iv-sdk) supports; the Complete Edition
1.2.0.x is not, downgrade first). Built with mingw on macOS/Linux, no Visual
Studio needed. The titlebar shows the detected version.

**Untested in-game so far** — this is a straight port written against the IV
native list; see "Status" below.

## Controls

| Key           | Action                       |
|---------------|------------------------------|
| ]             | open/close menu              |
| Up / Down     | select item                  |
| Right / Enter | activate / toggle / open submenu (Enter resets game speed, boost strength) |
| Left          | back to parent menu / close  |
| Left / Right  | decrease / increase value items (game speed, strength, colors, livery) |
| **hold B**    | speed boost (on foot or car) |
| / (or num /)  | airbrake toggle: WASD + Space/X, camera-relative on foot, hold B for 2x |
| Numpad +/-/*  | boost & airbrake strength    |

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

Compiles and links (`make`), the hook trampolines and native calls check out in
the disassembly, but nothing has been run in the game yet. Things most likely
to need a tweak on first run: `TEXT_SX/TEXT_SY/ROW_H` (IV text scale vs. row
height), whether `GET_STRING_WIDTH_WITH_STRING` and
`GET_VIEWPORT_POSITION_OF_COORD` return 0..1 units (assumed, like `DRAW_RECT`),
and the camera-rotation axes used by the on-foot airbrake.

## Build

```sh
make          # needs mingw-w64 (brew install mingw-w64); GTA IV is 32-bit, so i686
```

`sdk/ivsdk/` is IV-SDK (GPLv3) with a handful of mingw patches, documented in
`sdk/ivsdk/README-mingw.md`. The vehicle and weapon tables are hand-written
(`src/vehicles_iv.h`, `src/weapons_iv.h`).

## Install

1. An ASI loader for GTA IV (e.g. [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
   as `dinput8.dll`, or ZolikaPatch's built-in loader).
2. Copy `mod_iv.asi` next to `GTAIV.exe` (1.0.7.0 or 1.0.8.0).

Single-player only.

License: GPLv3 (derived from mod_sp / mod_sa / mod_v; IV-SDK is GPLv3).
