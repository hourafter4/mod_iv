#pragma once
// Backend for the Complete Edition (GTAIV.exe 1.2.0.x). IV-SDK only knows the
// addresses of 1.0.7.0 / 1.0.8.0, so nothing here is a fixed address: the three
// things mod_iv needs from the game are located by pattern at load time.
//
//   1. the native table   the game keeps its natives in a hash table
//                         (hash -> function); we look calls up there instead
//                         of calling the game's own lookup function. The CE
//                         rehashed every native, so hashes go through the
//                         translation table in sdk/patterns/native_hash_ce.h.
//   2. the per-frame tick GtaThread::Run, hooked in the thread vftable. It is
//                         called for every script thread every frame from the
//                         game's script processing, which is the context the
//                         drawing and cheat natives expect. The game timer
//                         only moves between frames, so it gates us to once
//                         per frame.
//   3. the ped / vehicle pools, for NPC health bars and "nearest empty car".
//
// Patterns are from IV.EFLC.Rainbomizer (natives, running thread, thread
// vftable) and GTAIV.EFLC.FusionFix (pools); both cover the CE and the older
// versions, so this .asi also runs on 1.0.4.0 - 1.0.8.0.

#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <unordered_map>

#include "Patterns.hh"
#include "injector/injector.hpp"
#include "native_hash_ce.h"

// --- the slice of IV-SDK the native wrappers need. IVSDK.cpp itself is not
// included: everything in it is version-locked to 1.0.7.0 / 1.0.8.0.
#define VALIDATE_SIZE(struc, size)
#define VALIDATE_OFFSET(struc, member, offset)
#include "CVector.h"
#include "CQuaternion.h"

// IV-SDK's NativeInvoke calls this to turn a native hash into a function
// pointer; the CE build resolves it from the game's native table instead.
class CTheScripts
{
public:
    static uint32_t FindNativeAddress (uint32_t nativeHash);
};

#include "Scripting/Scripting.h"

// ===================== pattern helpers =====================
// hook::get_pattern asserts when a pattern is missing; every lookup here is
// allowed to fail instead, so that a pattern that broke costs one feature
// rather than the whole .asi.
static void *
ce_find (const char *pattern, ptrdiff_t offset)
{
    hook::pattern p (pattern);
    if (p.size () < 1)
        return NULL;
    return p.get (0).get<void> (offset);
}

// tries the CE pattern first, then the pre-CE one
static void *
ce_find2 (const char *ce, const char *old, ptrdiff_t offset, bool *was_ce)
{
    void *m = ce_find (ce, offset);
    if (m)
    {
        if (was_ce)
            *was_ce = true;
        return m;
    }
    if (was_ce)
        *was_ce = false;
    return ce_find (old, offset);
}

// ===================== native table =====================
struct ce_native
{
    uint32_t hash;
    void *   fn;
};

static ce_native ** g_natives      = NULL;  // address of the game's table pointer
static uint32_t *   g_native_count = NULL;  // address of its size
static int          g_translate    = -1;    // -1 = not probed yet
static const char * g_version_name = "unknown";

// the game's open-addressing probe (IV.EFLC.Rainbomizer CTheScripts.cc)
static int
ce_native_index (uint32_t hash)
{
    ce_native *tbl = *g_natives;
    uint32_t   max = *g_native_count;
    if (!tbl || !max)
        return -1;

    uint32_t i = hash % max, j = hash;
    for (uint32_t step = 0; step < max && tbl[i].hash != 0; step++)
    {
        if (tbl[i].hash == hash)
            return (int) i;
        j = (j >> 1) + 1;
        i = (j + i) % max;
    }
    return -1;
}

// The CE rehashed the natives. Probe with one known native: if its pre-CE hash
// is absent but its translated hash is there, every hash needs translating.
static void
ce_probe_translation (void)
{
    const uint32_t probe = 0x8b3fed78;

    g_translate = 0;
    if (ce_native_index (probe) != -1)
        return;

    const std::unordered_map<uint32_t, uint32_t> &t = GetNativeTranslationTable ();
    std::unordered_map<uint32_t, uint32_t>::const_iterator it = t.find (probe);
    if (it != t.end () && ce_native_index (it->second) != -1)
        g_translate = 1;
}

uint32_t
CTheScripts::FindNativeAddress (uint32_t nativeHash)
{
    if (!g_natives || !g_native_count)
        return 0;
    if (g_translate < 0)
        ce_probe_translation ();

    if (g_translate)
    {
        const std::unordered_map<uint32_t, uint32_t> &t = GetNativeTranslationTable ();
        std::unordered_map<uint32_t, uint32_t>::const_iterator it = t.find (nativeHash);
        if (it != t.end ())
            nativeHash = it->second;
    }

    int i = ce_native_index (nativeHash);
    return (i < 0) ? 0 : (uint32_t) (*g_natives)[i].fn;
}

static bool
ce_natives_init (void)
{
    // the table pointer and its size are operands inside the game's own
    // native lookup, which these patterns reach through its call site
    static const struct { const char *pat; ptrdiff_t call; ptrdiff_t tbl; const char *name; } sites[] = {
        { "c1 e7 08 0b f8 57 e8",         6, 31, "Complete Edition" },
        { "0b c1 c1 e0 08 0b c2 8b f0",   9, 24, "1.0.4.0 - 1.0.8.0" },
    };

    for (int i = 0; i < 2; i++)
    {
        void *m = ce_find (sites[i].pat, sites[i].call);
        if (!m)
            continue;
        uint32_t fn = injector::GetBranchDestination (m).as_int ();
        if (!fn)
            continue;
        g_natives      = injector::ReadMemory<ce_native **> (fn + sites[i].tbl);
        g_native_count = injector::ReadMemory<uint32_t *> (fn + 3);
        if (!g_natives || !g_native_count)
            continue;
        g_version_name = sites[i].name;
        return true;
    }
    return false;
}

// ===================== per-frame tick (GtaThread::Run) =====================
struct ce_thread_vftable
{
    void *dtor, *reset, *run, *update, *kill;
};

static void (*g_tick) (void)   = NULL;
static uint32_t *g_running_thread = NULL;  // CTheScripts::m_pCurrentThread
static uint8_t   g_thread_dummy[256];      // stands in for a script thread, as IV-SDK does

// GtaThread::Run is __thiscall (this in ecx, args on the stack, callee pops).
// A __fastcall function with an unused second parameter has exactly that
// layout, so the original can be called back without any assembly.
static int (__fastcall *g_orig_run) (void *thread, void *edx, unsigned int a) = NULL;

static int __fastcall
ce_run_hook (void *thread, void *edx, unsigned int a)
{
    static unsigned int last_frame = 0xFFFFFFFFu;

    unsigned int now = 0;
    Scripting::GET_GAME_TIMER (&now);
    if (now != last_frame)
    {
        last_frame = now;
        if (g_tick)
        {
            // natives that create things expect a running script thread
            uint32_t bak = 0;
            if (g_running_thread)
            {
                bak = *g_running_thread;
                *g_running_thread = (uint32_t) (uintptr_t) g_thread_dummy;
            }
            g_tick ();
            if (g_running_thread)
                *g_running_thread = bak;
        }
    }

    return g_orig_run (thread, edx, a);
}

static bool
ce_tick_init (void (*tick) (void))
{
    // CTheScripts::m_pCurrentThread, so the dummy thread can be installed
    // around our tick the way IV-SDK does (patterns: Rainbomizer CTheScripts.cc)
    void *rt = ce_find2 ("83 f8 03 0f 84 ? ? ? ? 8b 35",
                         "83 f8 03 0f 84 ? ? ? ? a1", 0, NULL);
    if (rt)
        g_running_thread = injector::ReadMemory<uint32_t *> (
            (char *) rt + (injector::ReadMemory<uint8_t> ((char *) rt + 9) == 0x35 ? 11 : 10));

    void *m = ce_find ("c7 86 a8 00 00 00 00 00 00 00 8b c6 5e c3", -9);
    if (!m)
        return false;
    ce_thread_vftable *vt = injector::ReadMemory<ce_thread_vftable *> (m);
    if (!vt || !vt->run)
        return false;

    DWORD old = 0;
    injector::UnprotectMemory (vt, sizeof (*vt), old);

    g_tick     = tick;
    g_orig_run = (int (__fastcall *) (void *, void *, unsigned int)) vt->run;
    vt->run    = (void *) ce_run_hook;
    return true;
}

// ===================== pools =====================
static struct iv_pool **g_ped_pool = NULL;
static struct iv_pool **g_veh_pool = NULL;

static void
ce_pools_init (void)
{
    void *m;

    m = ce_find2 ("8B 3D ? ? ? ? 8B F1 8B 47",
                  "8B 15 ? ? ? ? 81 EC ? ? ? ? 8B C1", 2, NULL);
    if (m)
        g_ped_pool = injector::ReadMemory<struct iv_pool **> (m);

    m = ce_find2 ("8B 15 ? ? ? ? 46 3B 72 ? 7C ? 5E",
                  "8B 3D ? ? ? ? 8B CE FF D2 6A ? 6A ? 6A ? EB", 2, NULL);
    if (m)
        g_veh_pool = injector::ReadMemory<struct iv_pool **> (m);
}

// ===================== backend interface =====================
static const char *backend_version_name (void) { return g_version_name; }
static struct iv_pool *backend_ped_pool (void) { return g_ped_pool ? *g_ped_pool : NULL; }
static struct iv_pool *backend_veh_pool (void) { return g_veh_pool ? *g_veh_pool : NULL; }

// IV-SDK has the hash for REQUEST_MODEL but leaves the wrapper commented out
static void
backend_request_model (unsigned int model)
{
    NativeInvoke::Invoke<Scripting::ScriptVoid> (NATIVE_REQUEST_MODEL, model);
    Scripting::LOAD_ALL_OBJECTS_NOW ();
}

// src/main.cpp defines this, the way it defines plugin::gameStartupEvent for
// the IV-SDK build; it calls ce_natives_init / ce_pools_init / ce_tick_init.
void mod_iv_ce_startup (void);

BOOL APIENTRY
DllMain (HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        mod_iv_ce_startup ();
    return TRUE;
}
