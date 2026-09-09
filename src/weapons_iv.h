// GTA IV weapon types (eWeapon in sdk/ivsdk/Scripting/ScriptingEnums.h), the
// ones the player can hold in the base game. EFLC weapon types (21..44) are
// episode-only and do not exist in GTAIV.exe, so they are left out.
// {weapon type, display name, group}
static const char *WEAPON_GROUPS[] = { "Melee", "Handguns", "Shotguns", "SMGs", "Assault rifles", "Sniper rifles", "Heavy", "Throwables" };
#define WEAPON_GROUP_COUNT 8
static const struct { int type; const char *name; unsigned char group; } WEAPONS[] = {
    { 1,  "Baseball Bat",                  0 },
    { 2,  "Pool Cue",                      0 },
    { 3,  "Knife",                         0 },
    { 7,  "Pistol (Glock)",                1 },
    { 9,  "Combat Pistol (Desert Eagle)",  1 },
    { 10, "Pump Shotgun",                  2 },
    { 11, "Combat Shotgun (Baretta)",      2 },
    { 12, "Micro SMG (Uzi)",               3 },
    { 13, "SMG (MP5)",                     3 },
    { 14, "Assault Rifle (AK-47)",         4 },
    { 15, "Carbine Rifle (M4)",            4 },
    { 16, "Sniper Rifle",                  5 },
    { 17, "Combat Sniper (M40A1)",         5 },
    { 18, "RPG",                           6 },
    { 4,  "Grenades",                      7 },
    { 5,  "Molotovs",                      7 },
};
#define WEAPON_COUNT ((int)(sizeof(WEAPONS) / sizeof(WEAPONS[0])))

// the two weapon cheat sets (GTA IV phone numbers 486-555-0150 / 486-555-0100)
static const int CHEAT_WEAPONS_1[] = { 3, 5, 7, 10, 12, 14, 16, 18 };  // knife, molotov, pistol, pump shotgun, micro SMG, AK-47, sniper, RPG
static const int CHEAT_WEAPONS_2[] = { 1, 4, 9, 11, 13, 15, 17, 18 };  // bat, grenades, combat pistol, combat shotgun, MP5, M4, combat sniper, RPG
