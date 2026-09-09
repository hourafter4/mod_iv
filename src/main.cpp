// mod_iv — GTA IV port of mod_v / mod_sp, the single-player ports of mod_sa's
// cheat menu. Two builds come out of this one file (see src/backend.h):
//
//   mod_iv.asi     GTAIV.exe 1.0.7.0 / 1.0.8.0, on Zolika1351's IV-SDK
//   mod_iv_ce.asi  Complete Edition (1.2.0.x) and older, pattern-scanned
//
// Either way the per-frame handler runs inside the game's script processing,
// so IV natives can be called directly, and everything below this header uses
// natives plus the four backend calls only.
//
// Menu structure, callbacks (MENU_OP_ENABLED / SELECT / DEC / INC), the s0beit
// look (bottom-center window, titlebar breadcrumb, green enabled items, bottom
// HUD bar with [Inv] [VehInv] [Tires] [Bike] [AirBrk] tags) are the same as in
// mod_v/main.cpp; the cheat bodies are re-expressed with GTA IV natives.
//
// Menu: ] open/close, Up/Down select, Right/Enter activate or open submenu,
// Left back/close; on value items Left/Right adjust and Enter resets.
// Hotkeys: hold Left Alt (mod_sa's own boost key, unbound in GTA IV) or B =
// speed boost; / = airbrake toggle (WASD + Space/X, camera-relative on foot,
// boost key for 2x); Numpad +/-/* = boost & airbrake strength.
//
// GTA IV's own phone cheats are in the Cheats menu (health/armor/weapon sets,
// wanted level, weather) and the Vehicles > Spawn menu (the cheat spawns),
// labelled with the phone numbers they replace. TLAD / TBoGT cheats whose
// models are not installed are hidden; TBoGT abilities (explosive sniper
// rounds, super punch, parachute) have no native in GTAIV.exe and are dropped.
//
// GPLv3 — derived from mod_sp / mod_sa (https://github.com/BlastHackNet/mod_sa)
// and mod_v; IV-SDK is GPLv3 as well.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "backend.h"       // IV-SDK or pattern-scanning backend, and Scripting
#include "vehicles_iv.h"
#include "weapons_iv.h"

using namespace Scripting;

// natives IV-SDK has hashes for but no wrapper (sdk/ivsdk/Scripting/NativeHashes.h)
static Blip GET_NEXT_BLIP_INFO_ID(int type) { return NativeInvoke::Invoke<Blip>(NATIVE_GET_NEXT_BLIP_INFO_ID, type); }
static unsigned int GET_BLIP_SPRITE(Blip blip) { return NativeInvoke::Invoke<u32>(NATIVE_GET_BLIP_SPRITE, blip); }

// ===================== keyboard (IV keyboard natives, DirectInput scan codes) =====================
// IS_GAME_KEYBOARD_KEY_* only report keys the game itself receives; the
// foreground check additionally keeps an alt-tabbed windowed game from reacting
// to typing elsewhere (same check as mod_sp / mod_v).
static bool game_focused(void)
{
    DWORD pid = 0;
    HWND fg = GetForegroundWindow();
    if (fg)
        GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static bool key_down(int key)
{
    return game_focused() && IS_GAME_KEYBOARD_KEY_PRESSED(key);
}

static bool key_pressed(int key)
{
    return game_focused() && IS_GAME_KEYBOARD_KEY_JUST_PRESSED(key);
}

// Left Alt is mod_sa's / mod_sp's boost key and GTA IV leaves it unbound (aim
// is the right mouse button, sprint Left Shift, crouch Left Ctrl), so unlike
// mod_v — where Alt switches character — it can be used here. B stays as a
// second binding.
#define KEY_BOOST    KEY_LEFT_ALT              // hold: speed boost
#define KEY_BOOST2   KEY_B                     // hold: speed boost (alternate)
#define KEY_MENU     KEY_SQUARE_BRACKET_RIGHT  // ]
#define KEY_AIRBRAKE KEY_FORWARDSLASH          // /

static bool boost_held(void)
{
    return key_down(KEY_BOOST) || key_down(KEY_BOOST2);
}

// ===================== geometry constants =====================
// s0beit geometry (mod_sa dumb_menu.h: MENU_WIDTH 400/440, MENU_ROWS 12) in
// GTA IV's normalized screen space (0..1 both axes, DRAW_RECT / DISPLAY_TEXT).
#define MENU_ROWS   12
#define MENU_W      0.23f
#define TEXT_SX     0.26f         // SET_TEXT_SCALE width / height (font 0)
#define TEXT_SY     0.36f
#define ROW_H       0.031f
#define TEXT_DY     0.0015f       // text top inside a row

struct Col { int r, g, b, a; };
#define RGBA(r, g, b, a) Col{ r, g, b, a }

static const Col COL_TITLEBAR_BG  = RGBA(20, 20, 20, 190);
static const Col COL_MENU_BG      = RGBA(20, 20, 20, 180);
static const Col COL_BORDER_TITLE = RGBA(91, 91, 91, 223);
static const Col COL_BORDER_MENU  = RGBA(63, 63, 63, 223);
static const Col COL_SEL_BAR      = RGBA(127, 127, 127, 190);
static const Col COL_TEXT         = RGBA(223, 223, 223, 255);
static const Col COL_TEXT_ON      = RGBA(93, 210, 41, 255);
static const Col COL_SEPARATOR    = RGBA(157, 157, 157, 255);
static const Col COL_DIM          = RGBA(127, 127, 127, 191);
static const Col COL_BAR          = RGBA(0, 0, 0, 127);
static const Col COL_TAG_BRACKET  = RGBA(255, 255, 255, 191);
static const Col COL_TAG_ON       = RGBA(63, 255, 63, 191);
static const Col COL_TAG_OFF      = RGBA(255, 255, 255, 191);

// ===================== game helpers =====================
static Player me_pl(void) { return CONVERT_INT_TO_PLAYERINDEX(GET_PLAYER_ID()); }

static Ped me(void)
{
    Ped p = 0;
    GET_PLAYER_CHAR(me_pl(), &p);
    return p;
}

static Vehicle my_veh(void)
{
    Ped p = me();
    if (!IS_CHAR_IN_ANY_CAR(p))
        return 0;
    Vehicle v = 0;
    GET_CAR_CHAR_IS_USING(p, &v);
    return (v && DOES_VEHICLE_EXIST(v)) ? v : 0;
}

static void my_pos(float *x, float *y, float *z)
{
    Vehicle v = my_veh();
    if (v)
        GET_CAR_COORDINATES(v, x, y, z);
    else
        GET_CHAR_COORDINATES(me(), x, y, z);
}

// GTA IV health has a 100 offset: a ped dies at 100 and the player's full bar
// is 200 (ScriptHookDotNet's Ped::Health adds/subtracts the 100 as well).
#define HEALTH_DEAD 100
#define HEALTH_FULL 200

// script handles of every live slot in a RAGE pool (see struct iv_pool)
static int pool_handles(struct iv_pool *p, int *out, int max)
{
    if (!p || !p->storage || !p->flags || p->size <= 0)
        return 0;
    int n = 0;
    for (int i = 0; i < p->size && n < max; i++)
        if (!(p->flags[i] & 0x80))
            out[n++] = (i << 8) | p->flags[i];
    return n;
}

// streams a model in synchronously
static bool load_model(unsigned int model)
{
    if (!IS_MODEL_IN_CDIMAGE(model))
        return false;
    backend_request_model(model);
    return HAS_MODEL_LOADED(model) != 0;
}

// yaw/pitch (degrees, GET_CAM_ROT of the game cam) -> unit front + horizontal right
static void cam_axes(float *front, float *right)
{
    int cam = 0;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    GET_GAME_CAM(&cam);
    GET_CAM_ROT(cam, &rx, &ry, &rz);
    float yaw = rz * 3.14159265f / 180.0f, pitch = rx * 3.14159265f / 180.0f;
    front[0] = -sinf(yaw) * cosf(pitch);
    front[1] =  cosf(yaw) * cosf(pitch);
    front[2] =  sinf(pitch);
    right[0] =  cosf(yaw);
    right[1] =  sinf(yaw);
    right[2] =  0.0f;
}

// ===================== cheat state =====================
static bool  god_player   = false;
static bool  god_vehicle  = false;
static bool  bike_prot    = false;
static bool  tire_prot    = false;
static bool  hud_ind      = true;
static bool  npc_hpbars   = false;
static bool  inf_ammo     = false;
static bool  inf_sprint   = false;
static bool  never_wanted = false;
static bool  airbrake     = false;
static bool  seatbelt     = true;  // no windshield ejection (boost crashes!)
static bool  gravity_off  = false;
static float game_speed   = 1.0f;
static float strength     = 1.0f;  // boost/airbrake strength (numpad +/-)
static int   weather      = -1;    // eWeather index, -1 = don't force
static int   force_hour   = -1;    // -1 = don't force

// vehicles the per-frame handlers touched last, so their flags can be restored
static Vehicle god_veh_last  = 0;
static Vehicle tire_veh_last = 0;
static bool    foot_boost_active = false;  // on-foot boost changed the anim speed

// GTA IV weather types (eWeather)
static const char *WEATHER_NAMES[] = {
    "Extra sunny", "Sunny", "Sunny & windy", "Cloudy", "Rain", "Drizzle", "Foggy", "Thunderstorm",
};
#define WEATHER_COUNT ((int)(sizeof(WEATHER_NAMES) / sizeof(WEATHER_NAMES[0])))

// teleports; coordinates are player positions from GTA Connected's v-essentials
// resource (github.com/VortrexFTW/v-essentials, GTA IV location list).
// "Teleport to map waypoint" covers everything else.
static const char *TP_GROUPS[] = { "Safehouses & hangouts", "Police & hospitals", "Shops & services", "Landmarks & misc" };
#define TP_GROUP_COUNT ((int)(sizeof(TP_GROUPS) / sizeof(TP_GROUPS[0])))
static const struct { const char *name; float x, y, z; unsigned char group; } TELEPORTS[] = {
    { "Hove Beach safehouse (parking)",        904.27f,  -498.00f, 14.522f, 0 },
    { "South Bohan safehouse",                 589.42f,  1402.15f, 10.364f, 0 },
    { "The Lost MC clubhouse (Acter)",       -1713.29f,   358.25f, 25.449f, 0 },
    { "Perestroika (Hove Beach)",              957.58f,  -292.58f, 19.644f, 0 },
    { "Maisonette 9 (Westminster)",           -482.28f,   155.56f,  7.555f, 0 },
    { "Bahama Mamas (Westminster)",           -387.33f,   412.33f,  5.674f, 0 },
    { "Triangle Club (Bohan)",                1210.90f,  1718.18f, 16.667f, 0 },
    { "Firefly Island bowling",               1198.99f,  -681.49f, 16.445f, 0 },
    { "Broker police station",                 894.99f,  -357.39f, 18.185f, 1 },
    { "South Bohan police station",            435.40f,  1592.29f, 17.353f, 1 },
    { "Middle Park East police station",        50.12f,   679.88f, 15.316f, 1 },
    { "East Holland police station",            85.21f,  1189.82f, 14.755f, 1 },
    { "Chinatown police station",              213.12f,  -211.70f, 10.752f, 1 },
    { "Acter police station",                -1714.95f,   276.31f, 22.134f, 1 },
    { "Port Tudor police station",           -1220.73f,  -231.53f,  3.024f, 1 },
    { "Leftwood police station",              -927.66f,  1263.63f, 24.587f, 1 },
    { "Francis Intl. Airport police",         2170.87f,   448.87f,  6.085f, 1 },
    { "Schottler Medical Center",             1199.59f,   196.78f, 33.554f, 1 },
    { "Northern Gardens Medical Center",       980.71f,  1831.61f, 23.898f, 1 },
    { "Leftwood hospital",                   -1317.27f,  1277.20f, 22.370f, 1 },
    { "Acter Medical Center",                -1538.43f,   344.58f, 20.943f, 1 },
    { "Downtown Broker gun shop",             1054.11f,    86.84f, 33.408f, 2 },
    { "Chinatown gun shop",                     65.43f,  -342.36f, 14.767f, 2 },
    { "Port Tudor gun shop",                 -1338.77f,   307.61f, 13.378f, 2 },
    { "Hove Beach Pay 'n' Spray",             1058.57f,  -282.58f, 20.760f, 2 },
    { "Leftwood Pay 'n' Spray",              -1148.69f,  1171.52f, 16.457f, 2 },
    { "Hove Beach Russian clothes shop",       896.31f,  -442.59f, 15.888f, 2 },
    { "Star Junction Burger Shot",            -174.00f,   276.96f, 14.818f, 2 },
    { "South Bohan Burger Shot",               441.95f,  1516.64f, 16.289f, 2 },
    { "Outlook internet cafe (TW@)",           977.42f,  -169.11f, 24.013f, 2 },
    { "Berchem internet cafe (TW@)",         -1584.46f,   466.05f, 25.398f, 2 },
    { "Willis car wash",                      1831.02f,   360.20f, 22.061f, 2 },
    { "Hove Beach fuel station",              1128.51f,  -359.55f, 18.441f, 2 },
    { "Chinatown Bank of Liberty",             -34.92f,  -466.80f, 14.750f, 3 },
    { "Suffolk church",                       -274.30f,  -281.63f, 14.360f, 3 },
    { "City Hall station",                    -115.31f,  -501.22f, 14.755f, 3 },
    { "Castle Gardens station",                 82.95f,  -757.81f,  4.965f, 3 },
    { "Happiness Island",                     -621.81f,  -963.22f,  4.843f, 3 },
    { "The Exchange docks",                   -354.68f,  -661.62f,  4.791f, 3 },
    { "Francis Intl. Airport runway",         2610.75f,   262.42f,  5.875f, 3 },
    { "Francis Intl. Airport station",        2297.57f,   474.62f,  6.086f, 3 },
    { "Broker bus depot",                     1004.15f,   279.19f, 31.512f, 3 },
    { "Hove Beach station",                   1000.41f,  -544.82f, 14.854f, 3 },
    { "Alderney State Correctional Facility",-1155.21f,  -374.34f,  2.885f, 3 },
    { "Tudor fire station",                  -2144.97f,   164.15f, 12.051f, 3 },
    { "Northwood fire station",               -271.02f,  1542.15f, 20.420f, 3 },
};
#define TELEPORT_COUNT ((int)(sizeof(TELEPORTS) / sizeof(TELEPORTS[0])))

// ===================== ported cheat actions =====================
// move self (vehicle if in one) to a position, momentum zeroed
static void teleport_to(float x, float y, float z)
{
    Vehicle v = my_veh();
    if (v)
    {
        SET_CAR_COORDINATES(v, x, y, z);
        SET_CAR_FORWARD_SPEED(v, 0.0f);
    }
    else
    {
        Ped p = me();
        SET_CHAR_COORDINATES(p, x, y, z);
        SET_CHAR_VELOCITY(p, 0.0f, 0.0f, 0.0f);
    }
}

// far teleport: stream the target area first so we don't fall through the world
static void teleport_far(float x, float y, float z)
{
    REQUEST_COLLISION_AT_POSN(x, y, z);
    LOAD_SCENE(x, y, z);
    teleport_to(x, y, z);
}

// teleport to the map waypoint. The waypoint is the blip with sprite 8
// (BLIP_WAYPOINT) in one of the blip type lists (ScriptHookDotNet's
// Game::GetWaypoint scans them the same way); it has no height, so stream the
// area and ask for the ground there, fall from 50 m if none is found
static Blip find_waypoint(void)
{
    for (int type = 0; type < 10; type++)
        for (Blip b = GET_FIRST_BLIP_INFO_ID(type); DOES_BLIP_EXIST(b); b = GET_NEXT_BLIP_INFO_ID(type))
            if (GET_BLIP_SPRITE(b) == 8)
                return b;
    return 0;
}

static void teleport_to_waypoint(void)
{
    Blip b = find_waypoint();
    if (!b)
        return;
    Vector3 c;
    GET_BLIP_COORDS(b, &c);

    REQUEST_COLLISION_AT_POSN(c.x, c.y, 50.0f);
    LOAD_SCENE(c.x, c.y, 50.0f);
    float gz = 0.0f;
    GET_GROUND_Z_FOR_3D_COORD(c.x, c.y, 500.0f, &gz);
    teleport_to(c.x, c.y, gz > 0.0f ? gz + 1.5f : 50.0f);
}

// mod_sa cheat_teleport_nearest_car: on foot warp into the driver seat of the
// nearest empty drivable car; in a vehicle move it on top of that car
static void teleport_nearest_car(void)
{
    Vehicle mine = my_veh();
    float px, py, pz;
    GET_CHAR_COORDINATES(me(), &px, &py, &pz);

    static int vehs[512];
    int n = pool_handles(backend_veh_pool(), vehs, 512);

    Vehicle best = 0;
    float best_d = 0.0f;
    for (int i = 0; i < n; i++)
    {
        Vehicle v = vehs[i];
        if (v == mine || !DOES_VEHICLE_EXIST(v) || IS_CAR_DEAD(v))  // VEHICLE_ALIVE
            continue;
        Ped drv = 0;
        unsigned int pass = 0;
        GET_DRIVER_OF_CAR(v, &drv);
        GET_NUMBER_OF_PASSENGERS(v, &pass);
        if (drv || pass)                                            // VEHICLE_EMPTY
            continue;
        float qx, qy, qz;
        GET_CAR_COORDINATES(v, &qx, &qy, &qz);
        float dx = qx - px, dy = qy - py, dz = qz - pz;
        float d = dx * dx + dy * dy + dz * dz;
        if (!best || d < best_d)
        {
            best = v;
            best_d = d;
        }
    }
    if (!best)
        return;

    if (mine)
    {
        float qx, qy, qz;
        GET_CAR_COORDINATES(best, &qx, &qy, &qz);
        teleport_to(qx, qy, qz + 1.5f);
    }
    else
        WARP_CHAR_INTO_CAR(me(), best);
}

// the cheat-code spawner of mod_sa: streams the model, drops the car in front
// of the player and puts them in the driver seat
#define SPAWN_KEEP 8
static Vehicle spawned[SPAWN_KEEP];  // ring of cars we still own
static int     spawned_next;

static void spawn_vehicle(unsigned int model)
{
    if (!IS_THIS_MODEL_A_VEHICLE(model) || !load_model(model))
        return;
    Ped p = me();
    float x, y, z, heading = 0.0f;
    GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS(p, 0.0f, 5.0f, 0.0f, &x, &y, &z);
    GET_CHAR_HEADING(p, &heading);
    Vehicle v = 0;
    CREATE_CAR(model, x, y, z, &v, TRUE);
    if (!v || !DOES_VEHICLE_EXIST(v))
        return;
    SET_CAR_HEADING(v, heading);
    WARP_CHAR_INTO_CAR(p, v);
    MARK_MODEL_AS_NO_LONGER_NEEDED(model);
    // keep the last SPAWN_KEEP spawns as script vehicles so the game doesn't
    // cull them the moment they're out of range; release older ones
    Vehicle *slot = &spawned[spawned_next];
    if (*slot && *slot != v && DOES_VEHICLE_EXIST(*slot))
        MARK_CAR_AS_NO_LONGER_NEEDED(slot);
    *slot = v;
    spawned_next = (spawned_next + 1) % SPAWN_KEEP;
}

static void fix_vehicle(void)
{
    Vehicle v = my_veh();
    if (!v)
        return;
    FIX_CAR(v);
    SET_CAR_HEALTH(v, 1000);
    SET_ENGINE_HEALTH(v, 1000.0f);
    SET_PETROL_TANK_HEALTH(v, 1000.0f);
    WASH_VEHICLE_TEXTURES(v, 255);
}

static void heal_player(void)  // health + armor (phone cheat 362-555-0100)
{
    Ped p = me();
    unsigned int max_armour = 100;
    GET_PLAYER_MAX_ARMOUR(me_pl(), &max_armour);
    SET_CHAR_HEALTH(p, HEALTH_FULL);
    ADD_ARMOUR_TO_CHAR(p, max_armour);
}

// give a weapon with a full ammo load (max ammo of that type for this ped)
static void give_weapon_full(Ped p, int type)
{
    unsigned int ammo = 9999;
    GET_MAX_AMMO(p, type, &ammo);
    if (ammo == 0)
        ammo = 1;  // melee
    GIVE_WEAPON_TO_CHAR(p, type, ammo, FALSE);
}

static void give_weapon_set(const int *set, int n)  // phone cheats 486-555-0150 / -0100
{
    Ped p = me();
    for (int i = 0; i < n; i++)
        give_weapon_full(p, set[i]);
}

static void give_all_weapons(void)
{
    Ped p = me();
    for (int i = 0; i < WEAPON_COUNT; i++)
        give_weapon_full(p, WEAPONS[i].type);
}

static void wanted_delta(int d)  // phone cheats 267-555-0150 (raise); -0100 is clear
{
    Player pl = me_pl();
    unsigned int lvl = 0;
    STORE_WANTED_LEVEL(pl, &lvl);
    int n = (int)lvl + d;
    if (n < 0) n = 0;
    if (n > 6) n = 6;
    ALTER_WANTED_LEVEL(pl, n);
    APPLY_WANTED_LEVEL_CHANGE_NOW(pl);
}

static void weather_apply(void)
{
    if (weather >= 0)
    {
        FORCE_WEATHER_NOW(weather);
        FORCE_WEATHER(weather);
    }
    else
        RELEASE_WEATHER();
}

static void weather_cycle(void)  // phone cheat 468-555-0100: next weather type
{
    if (weather >= 0)
    {
        weather = (weather + 1) % WEATHER_COUNT;
        weather_apply();
        return;
    }
    int w = 0;
    GET_CURRENT_WEATHER(&w);
    FORCE_WEATHER_NOW((w + 1) % WEATHER_COUNT);
}

// ===================== speed boost (port of gta_speed / mod_sp boost_tick) =====================
// SA velocity units were 1 unit = 50 m/s; the constants are the same numbers in m/s.
static void boost_tick(float dt)
{
    Vehicle veh = my_veh();
    if (veh)
    {
        float spd = 0.0f;
        GET_CAR_SPEED(veh, &spd);  // m/s along the car
        if (spd < 2.5f)
        {
            // standstill: kick the car forward along its own facing
            SET_CAR_FORWARD_SPEED(veh, spd + 100.0f * dt * strength);
            return;
        }
        float cap = 75.0f * strength;  // 1.5 SA units = 270 km/h
        float f = 1.0f + 4.0f * dt * strength;
        float n = spd * f;
        if (n > cap)
            n = cap;
        if (n > spd)
            SET_CAR_FORWARD_SPEED(veh, n);
    }
    else
    {
        // on foot the locomotion system owns the ped's position, so run the
        // movement animation faster instead (mod_v does the same)
        Ped p = me();
        if (!IS_CHAR_ON_FOOT(p))
            return;
        float rate = 1.0f + 2.0f * strength;  // 3x at 100%
        if (rate > 6.0f)
            rate = 6.0f;
        SET_CHAR_MOVE_ANIM_SPEED_MULTIPLIER(p, rate);
        foot_boost_active = true;
    }
}

// ===================== airbrake (port of mod_sa cheat_vehicle.cpp, camera-relative) =====================
static Vehicle airbrake_veh = 0;   // what the last airbrake frame froze
static Ped     airbrake_ped = 0;

static void airbrake_restore(void)
{
    if (airbrake_veh && DOES_VEHICLE_EXIST(airbrake_veh))
    {
        SET_CAR_COLLISION(airbrake_veh, TRUE);
        FREEZE_CAR_POSITION(airbrake_veh, FALSE);
    }
    if (airbrake_ped && DOES_CHAR_EXIST(airbrake_ped))
    {
        SET_CHAR_COLLISION(airbrake_ped, TRUE);
        FREEZE_CHAR_POSITION(airbrake_ped, FALSE);
    }
    airbrake_veh = 0;
    airbrake_ped = 0;
}

static void airbrake_freeze(bool on)
{
    if (airbrake_veh)
        FREEZE_CAR_POSITION(airbrake_veh, on);
    else
        FREEZE_CHAR_POSITION(airbrake_ped, on);
}

static void airbrake_tick(float dt)
{
    Vehicle veh = my_veh();
    Ped ped = me();

    if ((veh && airbrake_veh != veh) || (!veh && airbrake_ped != ped))
        airbrake_restore();
    airbrake_veh = veh;
    airbrake_ped = veh ? 0 : ped;

    // "prevent all kinds of movement" (mod_sa): no collision, no momentum
    if (veh)
    {
        SET_CAR_COLLISION(veh, FALSE);
        SET_CAR_FORWARD_SPEED(veh, 0.0f);
    }
    else
    {
        SET_CHAR_COLLISION(ped, FALSE);
        SET_CHAR_VELOCITY(ped, 0.0f, 0.0f, 0.0f);
    }

    if (!game_focused())  // stay frozen, but don't fly on background keys
    {
        airbrake_freeze(true);
        return;
    }

    // Use the camera's yaw and pitch both on foot and in vehicles.
    float front[3], right[3];
    cam_axes(front, right);

    float d[3] = { 0, 0, 0 };
    if (key_down(KEY_W)) { d[0] += front[0]; d[1] += front[1]; d[2] += front[2]; }
    if (key_down(KEY_S)) { d[0] -= front[0]; d[1] -= front[1]; d[2] -= front[2]; }
    if (key_down(KEY_D)) { d[0] += right[0]; d[1] += right[1]; d[2] += right[2]; }
    if (key_down(KEY_A)) { d[0] -= right[0]; d[1] -= right[1]; d[2] -= right[2]; }
    if (key_down(KEY_SPACE)) d[2] += 1.0f;
    if (key_down(KEY_X))     d[2] -= 1.0f;

    if (d[0] == 0.0f && d[1] == 0.0f && d[2] == 0.0f)
    {
        airbrake_freeze(true);  // hover: no gravity creep
        return;
    }

    float step = 112.5f * dt * strength;
    if (boost_held())  // hold the boost key: 2x airbrake speed
        step *= 2.0f;
    airbrake_freeze(false);
    float x, y, z;
    my_pos(&x, &y, &z);
    x += d[0] * step; y += d[1] * step; z += d[2] * step;
    if (veh)
        SET_CAR_COORDINATES_NO_OFFSET(veh, x, y, z);
    else
        SET_CHAR_COORDINATES_NO_OFFSET(ped, x, y, z);
}

// ===================== per-frame cheat handlers (port of cheat_generic.cpp logic) =====================
static void apply_cheats(float dt)
{
    Ped ped = me();
    Player player = me_pl();
    if (!ped || !DOES_CHAR_EXIST(ped))
        return;

    if (god_player)
    {
        SET_CHAR_INVINCIBLE(ped, TRUE);
        SET_CHAR_PROOFS(ped, TRUE, TRUE, TRUE, TRUE, TRUE);
        unsigned int hp = 0;
        GET_CHAR_HEALTH(ped, &hp);
        if ((int)hp < HEALTH_FULL)
            SET_CHAR_HEALTH(ped, HEALTH_FULL);
    }

    if (inf_ammo)
    {
        unsigned int cur = 0;
        if (GET_CURRENT_CHAR_WEAPON(ped, &cur) && cur > 3)  // 0..3 = unarmed / melee
        {
            unsigned int ammo = 0, max = 0;
            GET_AMMO_IN_CHAR_WEAPON(ped, cur, &ammo);
            GET_MAX_AMMO(ped, cur, &max);
            if (max > 0 && ammo < max)
                SET_CHAR_AMMO(ped, cur, max);
        }
    }

    if (inf_sprint)
        SET_PLAYER_NEVER_GETS_TIRED(player, TRUE);

    if (never_wanted)
    {
        SET_MAX_WANTED_LEVEL(0);
        CLEAR_WANTED_LEVEL(player);
    }

    if (bike_prot)
        SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE(ped, FALSE);

    if (seatbelt)
        SET_CHAR_WILL_FLY_THROUGH_WINDSCREEN(ped, FALSE);

    Vehicle veh = my_veh();

    if (god_vehicle && veh)
    {
        if (god_veh_last && god_veh_last != veh && DOES_VEHICLE_EXIST(god_veh_last))
        {
            SET_CAR_PROOFS(god_veh_last, FALSE, FALSE, FALSE, FALSE, FALSE);
            SET_CAR_CAN_BE_DAMAGED(god_veh_last, TRUE);
            SET_CAR_CAN_BE_VISIBLY_DAMAGED(god_veh_last, TRUE);
        }
        god_veh_last = veh;
        SET_CAR_PROOFS(veh, TRUE, TRUE, TRUE, TRUE, TRUE);
        SET_CAR_CAN_BE_DAMAGED(veh, FALSE);
        SET_CAR_CAN_BE_VISIBLY_DAMAGED(veh, FALSE);  // bCanBeDamaged = false (mod_sa)
        unsigned int hp = 0;
        GET_CAR_HEALTH(veh, &hp);
        if (hp < 1000)
        {
            SET_CAR_HEALTH(veh, 1000);
            SET_ENGINE_HEALTH(veh, 1000.0f);
            SET_PETROL_TANK_HEALTH(veh, 1000.0f);
        }
    }

    if (tire_prot && veh)  // mod_sa cheat_vehicle_tires_set(temp, 0)
    {
        if (tire_veh_last && tire_veh_last != veh && DOES_VEHICLE_EXIST(tire_veh_last))
            SET_CAN_BURST_CAR_TYRES(tire_veh_last, TRUE);
        tire_veh_last = veh;
        SET_CAN_BURST_CAR_TYRES(veh, FALSE);
    }

    bool boosting = !airbrake && boost_held();
    if (airbrake)
        airbrake_tick(dt);
    else if (boosting)
        boost_tick(dt);
    if (!boosting && foot_boost_active)  // released: back to normal run speed
    {
        foot_boost_active = false;
        SET_CHAR_MOVE_ANIM_SPEED_MULTIPLIER(ped, 1.0f);
    }

    if (game_speed != 1.0f)
        SET_TIME_SCALE(game_speed);
}

// toggle-off handlers: undo what the per-frame handlers pinned
static void god_player_off(void)
{
    Ped p = me();
    SET_CHAR_INVINCIBLE(p, FALSE);
    SET_CHAR_PROOFS(p, FALSE, FALSE, FALSE, FALSE, FALSE);
}

static void god_vehicle_off(void)
{
    if (god_veh_last && DOES_VEHICLE_EXIST(god_veh_last))
    {
        SET_CAR_PROOFS(god_veh_last, FALSE, FALSE, FALSE, FALSE, FALSE);
        SET_CAR_CAN_BE_DAMAGED(god_veh_last, TRUE);
        SET_CAR_CAN_BE_VISIBLY_DAMAGED(god_veh_last, TRUE);
    }
    god_veh_last = 0;
}

static void tire_prot_off(void)
{
    if (tire_veh_last && DOES_VEHICLE_EXIST(tire_veh_last))
        SET_CAN_BURST_CAR_TYRES(tire_veh_last, TRUE);
    tire_veh_last = 0;
}

static void bike_prot_off(void)
{
    SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE(me(), TRUE);
}

// ===================== menu framework (port of mod_sa menu.cpp) =====================
#define MENU_OP_ENABLED 0
#define MENU_OP_SELECT  1
#define MENU_OP_DEC     2
#define MENU_OP_INC     3

#define ID_NONE        -1

struct menu;
struct menu_item;
typedef int (*menu_callback)(int op, struct menu_item *item);

struct menu_item
{
    const char  *name;
    int          id;
    struct menu *submenu;
    struct menu *menu;
    void        *data;
    bool         separator;
};

struct menu
{
    struct menu      *parent;
    menu_callback     callback;
    void            (*activate)(struct menu *m);  // mod_sa menu_event_activate
    struct menu_item *item;
    int               count, pos, top_pos;
};

static struct menu *menu_active = NULL;
static bool menu_open = false;

static struct menu *menu_new(struct menu *parent, menu_callback callback)
{
    struct menu *m = (struct menu *)calloc(1, sizeof(struct menu));
    m->parent = parent;
    m->callback = callback;
    return m;
}

static struct menu_item *menu_item_add(struct menu *m, struct menu *submenu, const char *name, int id, void *data)
{
    int i = m->count++;
    m->item = (struct menu_item *)realloc(m->item, sizeof(struct menu_item) * m->count);

    struct menu_item *item = &m->item[i];
    memset(item, 0, sizeof(*item));
    item->submenu = submenu;
    item->menu = m;
    item->name = _strdup(name);
    item->id = id;
    item->data = data;
    return item;
}

static void menu_sep_add(struct menu *m, const char *name)
{
    menu_item_add(m, NULL, name, ID_NONE, NULL)->separator = true;
}

static void menu_item_name_set(struct menu_item *item, const char *fmt, ...)
{
    char name[96];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);

    free((void *)item->name);
    item->name = _strdup(name);
}

// ===================== menu callbacks =====================
enum
{
    IT_GOD_PLAYER,
    IT_INF_AMMO,
    IT_INF_SPRINT,
    IT_NEVER_WANTED,
    IT_HEAL,
    IT_MONEY,
    IT_CLEAR_WANTED,
    IT_WANTED_UP,
    IT_WANTED_DOWN,
    IT_CHEAT_HEALTH,        // 362-555-0100
    IT_CHEAT_HEALTH_WEAP,   // 482-555-0100
    IT_CHEAT_WEAPONS1,      // 486-555-0150
    IT_CHEAT_WEAPONS2,      // 486-555-0100
    IT_CHEAT_WEATHER,       // 468-555-0100
    IT_GAME_SPEED,
    IT_GRAVITY_OFF,
    IT_GOD_VEHICLE,
    IT_BIKE_PROT,
    IT_TIRE_PROT,
    IT_TP_NEAREST,
    IT_FIX_VEH,
    IT_SEATBELT,
    IT_SPAWN,
    IT_COLOR1,
    IT_COLOR2,
    IT_COLOR3,
    IT_COLOR4,
    IT_LIVERY,
    IT_WEAPON,
    IT_ALL_WEAPONS,
    IT_TP_WAYPOINT,
    IT_TP_STATIC,
    IT_WEATHER,
    IT_HOUR,
    IT_AIRBRAKE,
    IT_STRENGTH,
    IT_HUD_IND,
    IT_NPC_HP,
    IT_PRESET,
};

static struct menu *m_misc_menu = NULL;  // holds the "Boost strength" item

// personal preset (menu item): player+vehicle invuln, tire protection,
// bike fall-off protection, NPC health bars — all on, or all off
static void preset_toggle(void)
{
    bool on = !(god_player && god_vehicle && tire_prot && bike_prot && npc_hpbars);

    god_player  = on;
    god_vehicle = on;
    tire_prot   = on;
    bike_prot   = on;
    npc_hpbars  = on;

    if (!on)
    {
        god_player_off();
        god_vehicle_off();
        tire_prot_off();
        bike_prot_off();
    }
}

static void strength_label_update(void)
{
    if (!m_misc_menu)
        return;
    for (int i = 0; i < m_misc_menu->count; i++)
        if (m_misc_menu->item[i].id == IT_STRENGTH)
            menu_item_name_set(&m_misc_menu->item[i], "Boost strength: %d%%", (int)(strength * 100.0f + 0.5f));
}

static int cb_main(int op, struct menu_item *item)
{
    if (op == MENU_OP_ENABLED && item->id == IT_PRESET)
        return god_player && god_vehicle && tire_prot && bike_prot && npc_hpbars;

    if (op == MENU_OP_SELECT && item->id == IT_PRESET)
    {
        preset_toggle();
        return 1;
    }

    return 0;
}

static int cb_cheats(int op, struct menu_item *item)
{
    int mod = (op == MENU_OP_DEC) ? -1 : 1;

    switch (op)
    {
    case MENU_OP_ENABLED:
        switch (item->id)
        {
        case IT_GOD_PLAYER:   return god_player;
        case IT_GOD_VEHICLE:  return god_vehicle;
        case IT_INF_AMMO:     return inf_ammo;
        case IT_INF_SPRINT:   return inf_sprint;
        case IT_NEVER_WANTED: return never_wanted;
        case IT_GRAVITY_OFF:  return gravity_off;
        }
        return 0;

    case MENU_OP_SELECT:
        switch (item->id)
        {
        case IT_GOD_PLAYER:
            god_player = !god_player;
            if (!god_player)
                god_player_off();
            return 1;

        case IT_GOD_VEHICLE:
            god_vehicle = !god_vehicle;
            if (!god_vehicle)
                god_vehicle_off();
            return 1;

        case IT_INF_AMMO:
            inf_ammo = !inf_ammo;
            return 1;

        case IT_INF_SPRINT:
            inf_sprint = !inf_sprint;
            if (!inf_sprint)
                SET_PLAYER_NEVER_GETS_TIRED(me_pl(), FALSE);
            return 1;

        case IT_NEVER_WANTED:
            never_wanted = !never_wanted;
            if (!never_wanted)
                SET_MAX_WANTED_LEVEL(6);
            return 1;

        case IT_HEAL:
        case IT_CHEAT_HEALTH:
            heal_player();
            return 1;

        case IT_CHEAT_HEALTH_WEAP:
            heal_player();
            give_weapon_set(CHEAT_WEAPONS_2, 8);
            return 1;

        case IT_CHEAT_WEAPONS1: give_weapon_set(CHEAT_WEAPONS_1, 8); return 1;
        case IT_CHEAT_WEAPONS2: give_weapon_set(CHEAT_WEAPONS_2, 8); return 1;
        case IT_CHEAT_WEATHER:  weather_cycle(); return 1;

        case IT_MONEY:
            ADD_SCORE(me_pl(), 250000);
            return 1;

        case IT_CLEAR_WANTED:
            CLEAR_WANTED_LEVEL(me_pl());
            return 1;

        case IT_WANTED_UP:   wanted_delta(+1); return 1;
        case IT_WANTED_DOWN: wanted_delta(-1); return 1;

        case IT_GRAVITY_OFF:
            gravity_off = !gravity_off;
            SET_GRAVITY_OFF(gravity_off);
            return 1;

        case IT_GAME_SPEED:  // select = reset
            game_speed = 1.0f;
            SET_TIME_SCALE(1.0f);
            menu_item_name_set(item, "Game speed: 100%%");
            return 1;
        }
        return 0;

    case MENU_OP_DEC:
    case MENU_OP_INC:
        if (item->id == IT_GAME_SPEED)
        {
            game_speed += mod * 0.25f;
            if (game_speed < 0.25f) game_speed = 0.25f;
            if (game_speed > 4.0f)  game_speed = 4.0f;
            if (game_speed == 1.0f) SET_TIME_SCALE(1.0f);
            menu_item_name_set(item, "Game speed: %d%%", (int)(game_speed * 100.0f + 0.5f));
            return 1;
        }
        return 0;
    }

    return 0;
}

static int cb_vehicles(int op, struct menu_item *item)
{
    if (op == MENU_OP_ENABLED)
    {
        if (item->id == IT_BIKE_PROT)
            return bike_prot;
        if (item->id == IT_TIRE_PROT)
            return tire_prot;
        if (item->id == IT_SEATBELT)
            return seatbelt;
        return 0;
    }

    if (op == MENU_OP_SELECT)
    {
        switch (item->id)
        {
        case IT_TIRE_PROT:
            tire_prot = !tire_prot;
            if (!tire_prot)
                tire_prot_off();
            return 1;

        case IT_BIKE_PROT:
            bike_prot = !bike_prot;
            if (!bike_prot)
                bike_prot_off();
            return 1;

        case IT_SEATBELT:
            seatbelt = !seatbelt;
            if (!seatbelt)
                SET_CHAR_WILL_FLY_THROUGH_WINDSCREEN(me(), TRUE);
            return 1;

        case IT_TP_NEAREST: teleport_nearest_car(); return 1;
        case IT_FIX_VEH:    fix_vehicle(); return 1;

        case IT_SPAWN:
            spawn_vehicle((unsigned int)(UINT_PTR)item->data);
            return 1;
        }
    }

    return 0;
}

// refresh the live color/livery labels when the paint menu opens
static void paint_menu_activate(struct menu *m)
{
    Vehicle veh = my_veh();
    int c[4] = { 0, 0, 0, 0 };
    if (veh)
    {
        GET_CAR_COLOURS(veh, &c[0], &c[1]);
        GET_EXTRA_CAR_COLOURS(veh, &c[2], &c[3]);
    }
    for (int i = 0; i < m->count; i++)
    {
        struct menu_item *it = &m->item[i];
        if (it->id >= IT_COLOR1 && it->id <= IT_COLOR4)
        {
            int ci = it->id - IT_COLOR1;
            if (veh) menu_item_name_set(it, "Color %d: %d", ci + 1, c[ci]);
            else     menu_item_name_set(it, "Color %d: -", ci + 1);
        }
        else if (it->id == IT_LIVERY)
        {
            int n = 0, l = 0;
            if (veh)
            {
                GET_NUM_CAR_LIVERIES(veh, &n);
                GET_CAR_LIVERY(veh, &l);
            }
            if (veh && n > 0) menu_item_name_set(it, "Livery: %d / %d", l, n - 1);
            else              menu_item_name_set(it, "Livery: -");
        }
    }
}

static int cb_paint(int op, struct menu_item *item)
{
    Vehicle veh = my_veh();
    if (!veh || (op != MENU_OP_DEC && op != MENU_OP_INC))
        return 0;

    int mod = (op == MENU_OP_DEC) ? -1 : 1;

    if (item->id >= IT_COLOR1 && item->id <= IT_COLOR4)
    {
        int c[4];
        GET_CAR_COLOURS(veh, &c[0], &c[1]);
        GET_EXTRA_CAR_COLOURS(veh, &c[2], &c[3]);
        int ci = item->id - IT_COLOR1;
        c[ci] += mod;
        if (c[ci] < 0)   c[ci] = 0;
        if (c[ci] > 133) c[ci] = 133;  // carcols.dat index range
        if (ci < 2) CHANGE_CAR_COLOUR(veh, c[0], c[1]);
        else        SET_EXTRA_CAR_COLOURS(veh, c[2], c[3]);
        menu_item_name_set(item, "Color %d: %d", ci + 1, c[ci]);
        return 1;
    }

    if (item->id == IT_LIVERY)
    {
        int n = 0, l = 0;
        GET_NUM_CAR_LIVERIES(veh, &n);
        if (n <= 0)
            return 1;
        GET_CAR_LIVERY(veh, &l);
        l += mod;
        if (l < 0)     l = 0;
        if (l > n - 1) l = n - 1;
        SET_CAR_LIVERY(veh, l);
        menu_item_name_set(item, "Livery: %d / %d", l, n - 1);
        return 1;
    }

    return 0;
}

static int cb_weapons(int op, struct menu_item *item)
{
    Ped ped = me();
    if (item->id == IT_ALL_WEAPONS)
    {
        if (op != MENU_OP_SELECT)
            return 0;
        give_all_weapons();
        return 1;
    }
    if (item->id != IT_WEAPON)
        return 0;

    int w = (int)(INT_PTR)item->data;

    if (op == MENU_OP_ENABLED)
    {
        unsigned int cur = 0;
        return GET_CURRENT_CHAR_WEAPON(ped, &cur) && (int)cur == w;
    }

    if (op == MENU_OP_SELECT)
    {
        give_weapon_full(ped, w);
        SET_CURRENT_CHAR_WEAPON(ped, w, TRUE);
        return 1;
    }

    return 0;
}

static int cb_teleports(int op, struct menu_item *item)
{
    if (op != MENU_OP_SELECT)
        return 0;

    if (item->id == IT_TP_WAYPOINT)
    {
        teleport_to_waypoint();
        return 1;
    }

    if (item->id == IT_TP_STATIC)
    {
        int i = (int)(UINT_PTR)item->data;
        teleport_far(TELEPORTS[i].x, TELEPORTS[i].y, TELEPORTS[i].z + 0.5f);
        return 1;
    }

    return 0;
}

static int cb_weather(int op, struct menu_item *item)
{
    int w = (int)(INT_PTR)item->data;

    if (op == MENU_OP_ENABLED)
        return weather == w;

    if (op == MENU_OP_SELECT)
    {
        weather = (weather == w) ? -1 : w;  // re-select = back to auto (mod_sa)
        weather_apply();
        return 1;
    }

    return 0;
}

static int cb_time(int op, struct menu_item *item)
{
    int h = (int)(INT_PTR)item->data;

    if (op == MENU_OP_ENABLED)
        return force_hour == h;

    if (op == MENU_OP_SELECT)
    {
        force_hour = (force_hour == h) ? -1 : h;  // re-select = unfreeze (mod_sa)
        if (force_hour >= 0)
            FORCE_TIME_OF_DAY(force_hour, 0);
        else
            RELEASE_TIME_OF_DAY();
        return 1;
    }

    return 0;
}

static int cb_misc(int op, struct menu_item *item)
{
    int mod = (op == MENU_OP_DEC) ? -1 : 1;

    switch (op)
    {
    case MENU_OP_ENABLED:
        if (item->id == IT_AIRBRAKE)
            return airbrake;
        if (item->id == IT_HUD_IND)
            return hud_ind;
        if (item->id == IT_NPC_HP)
            return npc_hpbars;
        return 0;

    case MENU_OP_SELECT:
        if (item->id == IT_AIRBRAKE)
        {
            airbrake = !airbrake;
            if (!airbrake)
                airbrake_restore();
            return 1;
        }
        if (item->id == IT_HUD_IND)
        {
            hud_ind = !hud_ind;
            return 1;
        }
        if (item->id == IT_NPC_HP)
        {
            npc_hpbars = !npc_hpbars;
            return 1;
        }
        if (item->id == IT_STRENGTH)  // select = reset
        {
            strength = 1.0f;
            strength_label_update();
            return 1;
        }
        return 0;

    case MENU_OP_DEC:
    case MENU_OP_INC:
        if (item->id == IT_STRENGTH)
        {
            strength += mod * 0.25f;
            if (strength < 0.25f) strength = 0.25f;
            if (strength > 4.0f)  strength = 4.0f;
            strength_label_update();
            return 1;
        }
        return 0;
    }

    return 0;
}

// ===================== menu layout (port of mod_sa menu_maybe_init) =====================
// runs from the per-frame hook: model checks need natives
static void menu_maybe_init(void)
{
    if (menu_active)
        return;

    char name[96];
    int  i;

    struct menu *m_main     = menu_new(NULL, cb_main);
    struct menu *m_cheats   = menu_new(m_main, cb_cheats);
    struct menu *m_weapons  = menu_new(m_main, cb_weapons);
    struct menu *m_vehicles = menu_new(m_main, cb_vehicles);
    struct menu *m_spawn    = menu_new(m_vehicles, cb_vehicles);
    struct menu *m_paint    = menu_new(m_vehicles, cb_paint);
    struct menu *m_tp       = menu_new(m_main, cb_teleports);
    struct menu *m_weather  = menu_new(m_main, cb_weather);
    struct menu *m_time     = menu_new(m_main, cb_time);
    struct menu *m_misc     = menu_new(m_main, cb_misc);

    m_paint->activate = paint_menu_activate;

    /* main menu */
    menu_item_add(m_main, NULL, "My preset", IT_PRESET, NULL);
    menu_sep_add(m_main, "GTA IV");
    menu_item_add(m_main, m_cheats, "Cheats", ID_NONE, NULL);
    menu_item_add(m_main, m_weapons, "Weapons", ID_NONE, NULL);
    menu_item_add(m_main, m_vehicles, "Vehicles", ID_NONE, NULL);
    menu_item_add(m_main, m_tp, "Teleports", ID_NONE, NULL);
    menu_item_add(m_main, m_weather, "Freeze weather", ID_NONE, NULL);
    menu_item_add(m_main, m_time, "Freeze time", ID_NONE, NULL);
    menu_item_add(m_main, m_misc, "Misc.", ID_NONE, NULL);

    /* main menu -> cheats */
    menu_item_add(m_cheats, NULL, "Player god mode", IT_GOD_PLAYER, NULL);
    menu_item_add(m_cheats, NULL, "Vehicle god mode", IT_GOD_VEHICLE, NULL);
    menu_item_add(m_cheats, NULL, "Infinite ammo", IT_INF_AMMO, NULL);
    menu_item_add(m_cheats, NULL, "Infinite sprint", IT_INF_SPRINT, NULL);
    menu_item_add(m_cheats, NULL, "Never wanted", IT_NEVER_WANTED, NULL);
    menu_item_add(m_cheats, NULL, "Heal player", IT_HEAL, NULL);
    menu_item_add(m_cheats, NULL, "Give $250000", IT_MONEY, NULL);
    menu_item_add(m_cheats, NULL, "Lower wanted level", IT_WANTED_DOWN, NULL);
    menu_sep_add(m_cheats, "GTA IV phone cheats");
    menu_item_add(m_cheats, NULL, "Health & armor (362-555-0100)", IT_CHEAT_HEALTH, NULL);
    menu_item_add(m_cheats, NULL, "Health, armor & weapons (482-555-0100)", IT_CHEAT_HEALTH_WEAP, NULL);
    menu_item_add(m_cheats, NULL, "Weapons set 1 (486-555-0150)", IT_CHEAT_WEAPONS1, NULL);
    menu_item_add(m_cheats, NULL, "Weapons set 2 (486-555-0100)", IT_CHEAT_WEAPONS2, NULL);
    menu_item_add(m_cheats, NULL, "Remove wanted level (267-555-0100)", IT_CLEAR_WANTED, NULL);
    menu_item_add(m_cheats, NULL, "Raise wanted level (267-555-0150)", IT_WANTED_UP, NULL);
    menu_item_add(m_cheats, NULL, "Change weather (468-555-0100)", IT_CHEAT_WEATHER, NULL);
    menu_sep_add(m_cheats, "Values (Left/Right adjust, Enter resets)");
    menu_item_add(m_cheats, NULL, "Game speed: 100%", IT_GAME_SPEED, NULL);
    menu_item_add(m_cheats, NULL, "Gravity off", IT_GRAVITY_OFF, NULL);

    /* main menu -> weapons (mod_sa layout: one list, group separators) */
    menu_item_add(m_weapons, NULL, "Give all weapons", IT_ALL_WEAPONS, NULL);
    for (int g = 0; g < WEAPON_GROUP_COUNT; g++)
    {
        menu_sep_add(m_weapons, WEAPON_GROUPS[g]);
        for (i = 0; i < WEAPON_COUNT; i++)
            if (WEAPONS[i].group == g)
                menu_item_add(m_weapons, NULL, WEAPONS[i].name, IT_WEAPON, (void *)(INT_PTR)WEAPONS[i].type);
    }

    /* main menu -> vehicles */
    menu_item_add(m_vehicles, NULL, "Seatbelt (no windshield eject)", IT_SEATBELT, NULL);
    menu_item_add(m_vehicles, NULL, "Bike fall-off protection", IT_BIKE_PROT, NULL);
    menu_item_add(m_vehicles, NULL, "Tire protection", IT_TIRE_PROT, NULL);
    menu_item_add(m_vehicles, m_spawn, "Spawn vehicle", ID_NONE, NULL);
    menu_item_add(m_vehicles, m_paint, "Paint", ID_NONE, NULL);
    menu_item_add(m_vehicles, NULL, "Teleport to nearest empty car", IT_TP_NEAREST, NULL);
    menu_item_add(m_vehicles, NULL, "Fix vehicle", IT_FIX_VEH, NULL);

    /* main menu -> vehicles -> spawn: the phone-cheat spawns first, then a
       submenu per class; models the installed game doesn't have (EFLC) are
       dropped here */
    menu_sep_add(m_spawn, "Phone cheat spawns");
    for (i = 0; i < SPAWN_CHEAT_COUNT; i++)
    {
        unsigned int h = GET_HASH_KEY(SPAWN_CHEATS[i].model);
        if (!IS_MODEL_IN_CDIMAGE(h))
            continue;
        snprintf(name, sizeof(name), "%s (%s)", SPAWN_CHEATS[i].name, SPAWN_CHEATS[i].number);
        menu_item_add(m_spawn, NULL, name, IT_SPAWN, (void *)(UINT_PTR)h);
    }
    menu_sep_add(m_spawn, "All vehicles");
    for (int c = 0; c < CLASS_COUNT; c++)
    {
        struct menu *mc = menu_new(m_spawn, cb_vehicles);
        for (i = 0; i < VEHICLE_COUNT; i++)
        {
            if (IV_VEHICLES[i].cls != c)
                continue;
            unsigned int h = GET_HASH_KEY(IV_VEHICLES[i].model);
            if (!IS_MODEL_IN_CDIMAGE(h) || !IS_THIS_MODEL_A_VEHICLE(h))
                continue;
            menu_item_add(mc, NULL, IV_VEHICLES[i].name, IT_SPAWN, (void *)(UINT_PTR)h);
        }
        if (mc->count > 0)
            menu_item_add(m_spawn, mc, CLASS_NAMES[c], ID_NONE, NULL);
    }

    /* main menu -> vehicles -> paint */
    menu_item_add(m_paint, NULL, "Color 1: -", IT_COLOR1, NULL);
    menu_item_add(m_paint, NULL, "Color 2: -", IT_COLOR2, NULL);
    menu_item_add(m_paint, NULL, "Color 3: -", IT_COLOR3, NULL);
    menu_item_add(m_paint, NULL, "Color 4: -", IT_COLOR4, NULL);
    menu_item_add(m_paint, NULL, "Livery: -", IT_LIVERY, NULL);

    /* main menu -> teleports */
    menu_item_add(m_tp, NULL, "Teleport to map waypoint", IT_TP_WAYPOINT, NULL);
    for (int g = 0; g < TP_GROUP_COUNT; g++)
    {
        menu_sep_add(m_tp, TP_GROUPS[g]);
        for (i = 0; i < TELEPORT_COUNT; i++)
            if (TELEPORTS[i].group == g)
                menu_item_add(m_tp, NULL, TELEPORTS[i].name, IT_TP_STATIC, (void *)(UINT_PTR)i);
    }

    /* main menu -> weather */
    menu_item_add(m_weather, NULL, "Auto", IT_WEATHER, (void *)(INT_PTR)-1);
    for (i = 0; i < WEATHER_COUNT; i++)
        menu_item_add(m_weather, NULL, WEATHER_NAMES[i], IT_WEATHER, (void *)(INT_PTR)i);

    /* main menu -> time */
    for (i = 0; i < 24; i++)
    {
        snprintf(name, sizeof(name), "%02d:00", i);
        menu_item_add(m_time, NULL, name, IT_HOUR, (void *)(UINT_PTR)i);
    }

    /* main menu -> misc */
    menu_item_add(m_misc, NULL, "Airbrake [/]", IT_AIRBRAKE, NULL);
    menu_item_add(m_misc, NULL, "Boost strength: 100%", IT_STRENGTH, NULL);
    m_misc_menu = m_misc;
    menu_item_add(m_misc, NULL, "HUD indicators", IT_HUD_IND, NULL);
    menu_item_add(m_misc, NULL, "NPC health bars", IT_NPC_HP, NULL);

    menu_active = m_main;
}

// ===================== menu input (port of mod_sa menu_run) =====================
static void menu_input(void)
{
    if (!game_focused())
        return;

    if (key_pressed(KEY_MENU))
        menu_open = !menu_open;

    // global hotkeys (work with menu closed too)
    if (key_pressed(KEY_AIRBRAKE) || key_pressed(KEY_NUMPAD_FORWARDSLASH))
    {
        airbrake = !airbrake;
        if (!airbrake)
            airbrake_restore();
    }
    float strength_old = strength;
    if (key_pressed(KEY_NUMPAD_PLUS))  strength += 0.25f;
    if (key_pressed(KEY_NUMPAD_MINUS)) strength -= 0.25f;
    if (key_pressed(KEY_ASTERISK))     strength = 1.0f;
    if (strength < 0.25f) strength = 0.25f;
    if (strength > 4.0f)  strength = 4.0f;
    if (strength != strength_old)
        strength_label_update();

    if (!menu_open || menu_active == NULL)
        return;

    struct menu *m = menu_active;

    if (key_pressed(KEY_ARROW_UP))
        m->pos--;
    if (key_pressed(KEY_ARROW_DOWN))
        m->pos++;
    if (m->pos < 0)
        m->pos = m->count - 1;
    if (m->pos >= m->count)
        m->pos = 0;

    struct menu_item *item = (m->count > 0) ? &m->item[m->pos] : NULL;

    /* Left: decrease value items; otherwise back / close (mod_sa) */
    if (key_pressed(KEY_ARROW_LEFT))
    {
        if (item != NULL && m->callback != NULL && m->callback(MENU_OP_DEC, item))
            return;
        if (m->parent == NULL)
            menu_open = false;
        else
        {
            menu_active = m->parent;
            if (menu_active->activate)
                menu_active->activate(menu_active);
        }
        return;
    }

    /* Right: increase value items; otherwise activate / open submenu */
    if (key_pressed(KEY_ARROW_RIGHT))
    {
        if (item != NULL && m->callback != NULL && m->callback(MENU_OP_INC, item))
            return;
        if (item != NULL && m->callback != NULL && m->callback(MENU_OP_SELECT, item))
            return;
        if (item != NULL && item->submenu != NULL)
        {
            menu_active = item->submenu;
            if (menu_active->activate)
                menu_active->activate(menu_active);
        }
        return;
    }

    /* Enter: activate / open submenu; on value items = reset */
    if (key_pressed(KEY_ENTER) || key_pressed(KEY_NUMPAD_ENTER))
    {
        if (item != NULL && m->callback != NULL && m->callback(MENU_OP_SELECT, item))
            return;
        if (item != NULL && item->submenu != NULL)
        {
            menu_active = item->submenu;
            if (menu_active->activate)
                menu_active->activate(menu_active);
        }
    }
}

// ===================== drawing primitives (GTA IV natives) =====================
static void draw_box(float x, float y, float w, float h, Col c)
{
    DRAW_RECT(x + w / 2.0f, y + h / 2.0f, w, h, c.r, c.g, c.b, c.a);
}

static void draw_box_border(float x, float y, float w, float h, Col border, Col bg)
{
    // 1px border in normalized units
    float bx = 1.0f / 1920.0f, by = 1.0f / 1080.0f;
    draw_box(x, y, w, h, border);
    draw_box(x + bx, y + by, w - 2 * bx, h - 2 * by, bg);
}

static void text_setup(void)
{
    SET_TEXT_FONT(0);
    SET_TEXT_SCALE(TEXT_SX, TEXT_SY);
    SET_TEXT_CENTRE(FALSE);
    SET_TEXT_RIGHT_JUSTIFY(FALSE);
    SET_TEXT_WRAP(0.0f, 1.0f);
    SET_TEXT_BACKGROUND(FALSE);
    SET_TEXT_DROPSHADOW(TRUE, 0, 0, 0, 255);
    SET_TEXT_EDGE(FALSE, 0, 0, 0, 0);
}

static void draw_text(float x, float y, Col c, const char *txt)
{
    text_setup();
    SET_TEXT_COLOUR(c.r, c.g, c.b, c.a);
    DISPLAY_TEXT_WITH_LITERAL_STRING(x, y + TEXT_DY, "STRING", txt);
}

static float text_width(const char *txt)
{
    text_setup();
    return GET_STRING_WIDTH_WITH_STRING("STRING", txt);
}

#if MOD_IV_CE
#define MOD_IV_NAME "mod_iv CE"
#else
#define MOD_IV_NAME "mod_iv"
#endif

// ===================== menu draw (s0beit RenderMenu look) =====================
static void menu_draw(void)
{
    if (!menu_open || menu_active == NULL)
        return;

    struct menu *m = menu_active;

    const float menu_h = ROW_H * MENU_ROWS + 0.004f;
    const float left = 0.5f - MENU_W / 2.0f;
    const float top_win = 1.0f - menu_h - ROW_H - 0.02f;  // sits above the HUD bar
    const float top_title = top_win - ROW_H - 0.006f;

    /* titlebar: name + breadcrumb path (s0beit "NAME > Sub > Sub") */
    static char title[192];
    snprintf(title, sizeof(title), MOD_IV_NAME " %s (" __DATE__ ")", backend_version_name());
    struct menu *root;
    for (root = m; root->parent != NULL; root = root->parent);
    while (root != m)
    {
        if (root->pos < 0 || root->pos >= root->count)
            break;
        struct menu_item *it = &root->item[root->pos];
        if (it->submenu == NULL)
            break;
        snprintf(title + strlen(title), sizeof(title) - strlen(title), " > %s", it->name);
        root = it->submenu;
    }
    static char title_prev[192];
    static int  title_skip;
    if (strcmp(title, title_prev) != 0)           // clip from the left if too long
    {
        strcpy(title_prev, title);
        title_skip = 0;
        while (title[title_skip] && text_width(title + title_skip) > MENU_W - 0.004f)
            title_skip++;
    }
    const char *tp = title + title_skip;

    draw_box_border(left, top_title, MENU_W, ROW_H + 0.004f, COL_BORDER_TITLE, COL_TITLEBAR_BG);
    draw_text(left + 0.003f, top_title + 0.002f, COL_TEXT, tp);

    /* window */
    draw_box_border(left, top_win, MENU_W, menu_h, COL_BORDER_MENU, COL_MENU_BG);

    /* scroll window (s0beit top_pos handling) */
    if (m->pos - MENU_ROWS >= m->top_pos)
        m->top_pos = m->pos - MENU_ROWS + 1;
    if (m->pos < m->top_pos)
        m->top_pos = m->pos;
    if (m->top_pos < 0)
        m->top_pos = 0;
    else if (m->top_pos >= m->count)
        m->top_pos = m->count - 1;

    /* items */
    float x = left + 0.003f;
    float y = top_win + 0.002f;

    for (int i = m->top_pos; i < m->top_pos + MENU_ROWS; i++, y += ROW_H)
    {
        if (i < 0 || i >= m->count)
            continue;

        struct menu_item *item = &m->item[i];
        int enabled = (!item->separator && m->callback != NULL)
            ? m->callback(MENU_OP_ENABLED, item) : 0;

        if (i == m->pos)
            draw_box(left + 0.001f, y, MENU_W - 0.002f, ROW_H, COL_SEL_BAR);

        if (item->separator)
        {
            draw_text(x, y, COL_SEPARATOR, item->name);
            draw_box(x, y + ROW_H - 0.002f, text_width(item->name), 0.0015f, COL_DIM);
        }
        else
            draw_text(x, y, enabled ? COL_TEXT_ON : COL_TEXT, item->name);

        if (item->submenu != NULL)
            draw_text(left + MENU_W - 0.012f, y, COL_DIM, ">");
    }
}

// ===================== HUD bottom bar (s0beit look) =====================
// Full-width dark strip at the very bottom: [Tag] indicators (white
// brackets, green text when the cheat is on), coords + speed, colored FPS
// at the right edge.
static float g_fps;  // EMA, updated each frame

static void hud_text(float *x, float y, Col c, const char *text)
{
    draw_text(*x, y, c, text);
    *x += text_width(text);
}

// s0beit HUD_TEXT_TGL: "[" white + name (green when on) + "] "
static void hud_tag(float *x, float y, const char *name, bool on)
{
    hud_text(x, y, COL_TAG_BRACKET, "[");
    hud_text(x, y, on ? COL_TAG_ON : COL_TAG_OFF, name);
    hud_text(x, y, COL_TAG_BRACKET, "] ");
}

static void hud_draw(void)
{
    if (!hud_ind)
        return;

    const float y = 1.0f - ROW_H - 0.004f;
    draw_box(0.0f, y - 0.002f, 1.0f, ROW_H + 0.006f, COL_BAR);

    float x = 0.004f;
    hud_tag(&x, y, "Inv", god_player);
    hud_tag(&x, y, "VehInv", god_vehicle);
    hud_tag(&x, y, "Tires", tire_prot);
    hud_tag(&x, y, "Bike", bike_prot);
    hud_tag(&x, y, "AirBrk", airbrake);

    static char buf[96];
    float px, py, pz;
    my_pos(&px, &py, &pz);
    Vehicle veh = my_veh();
    if (veh)
    {
        float spd = 0.0f;
        GET_CAR_SPEED(veh, &spd);
        snprintf(buf, sizeof(buf), " %.2f, %.2f, %.2f   %d km/h", px, py, pz, (int)(spd * 3.6f + 0.5f));
    }
    else
        snprintf(buf, sizeof(buf), " %.2f, %.2f, %.2f", px, py, pz);
    hud_text(&x, y, COL_TAG_OFF, buf);

    /* FPS, colored by value like s0beit, right-aligned */
    static char fps_buf[16];
    snprintf(fps_buf, sizeof(fps_buf), "%.0f", g_fps);
    Col fps_col = RGBA(0, 200, 0, 255);
    if ((int)g_fps <= 12)
        fps_col = RGBA(200, 0, 0, 255);
    else if ((int)g_fps <= 22)
        fps_col = RGBA(200, 200, 0, 255);
    draw_text(1.0f - text_width(fps_buf) - 0.004f, y, fps_col, fps_buf);
}

// ===================== NPC health bars =====================
// Ped handles come from the ped pool (the backend locates it); everything else
// is natives, so this is the same code on both builds.
#define HPBAR_MAX_DIST 60.0f

static void hpbars_draw(void)
{
    if (!npc_hpbars)
        return;

    Ped self = me();
    static int peds[512];
    int n = pool_handles(backend_ped_pool(), peds, 512);
    if (n == 0)
        return;

    float mx, my, mz;
    GET_CHAR_COORDINATES(self, &mx, &my, &mz);

    int vp = 0;
    GET_GAME_VIEWPORT_ID(&vp);

    for (int i = 0; i < n; i++)
    {
        Ped p = peds[i];
        if (p == self || !DOES_CHAR_EXIST(p) || IS_CHAR_DEAD(p))
            continue;

        float px, py, pz;
        GET_CHAR_COORDINATES(p, &px, &py, &pz);
        float dx = px - mx, dy = py - my, dz = pz - mz;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > HPBAR_MAX_DIST * HPBAR_MAX_DIST)
            continue;

        float sx = 0.0f, sy = 0.0f;
        if (!GET_VIEWPORT_POSITION_OF_COORD(px, py, pz + 1.0f, vp, &sx, &sy))  // just above the head
            continue;

        unsigned int hp = 0;
        GET_CHAR_HEALTH(p, &hp);
        float frac = ((float)hp - HEALTH_DEAD) / (float)(HEALTH_FULL - HEALTH_DEAD);
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;

        float dist = sqrtf(d2);
        float bw = (220.0f / (dist > 1.0f ? dist : 1.0f)) / 1920.0f;  // scale bar with distance
        if (bw < 16.0f / 1920.0f) bw = 16.0f / 1920.0f;
        if (bw > 70.0f / 1920.0f) bw = 70.0f / 1920.0f;
        float bh = 6.0f / 1080.0f;

        draw_box(sx - bw / 2 - 0.0007f, sy - 0.0012f, bw + 0.0014f, bh + 0.0024f, RGBA(0, 0, 0, 180));
        draw_box(sx - bw / 2, sy, bw * frac, bh, RGBA(220, 50, 50, 230));
    }
}

static void draw_overlays(void)
{
    hpbars_draw();
    hud_draw();
    menu_draw();
}

// ===================== per-frame hook =====================
// The backend calls this once per frame from the game's script processing
// (natives are valid there); nothing runs while the game is paused.
static void tick(void)
{
    if (!IS_PLAYER_PLAYING(me_pl()))  // main menu / loading / dead
        return;

    float dt = 0.0f;
    GET_FRAME_TIME(&dt);
    if (dt <= 0.0f)  dt = 0.02f;
    if (dt > 0.1f)   dt = 0.1f;
    g_fps += (1.0f / dt - g_fps) * 0.05f;

    menu_maybe_init();

    bool paused = IS_PAUSE_MENU_ACTIVE() != 0;

    if (!paused)
        menu_input();
    apply_cheats(dt);
    if (!paused)
        draw_overlays();
}

#if MOD_IV_CE

// Complete Edition build: find the game's native table and pools, then hook
// GtaThread::Run for the tick. Called from DllMain (src/backend_ce.h).
void mod_iv_ce_startup(void)
{
    if (!ce_natives_init())
        return;      // no native table: nothing this .asi does would work
    ce_pools_init();  // optional: NPC health bars / nearest car degrade without them
    ce_tick_init(tick);
}

#else

// IV-SDK build: the SDK calls this once it has recognised the game version.
void plugin::gameStartupEvent(void)
{
    plugin::processScriptsEvent::Add(tick);
}

#endif
