#pragma once
// mod_iv talks to the game through one of two backends, picked at build time:
//
//   mod_iv.asi     (default)      IV-SDK, fixed addresses, GTAIV.exe 1.0.7.0 / 1.0.8.0
//   mod_iv_ce.asi  (MOD_IV_CE=1)  pattern scanning, Complete Edition (1.2.0.x) and older
//
// Everything above this line is shared: the menu, the cheats and the drawing in
// src/main.cpp only use GTA IV script natives plus the four calls below.

// RAGE pool header. Same layout in every GTA IV build (IV-SDK CPool,
// FusionFix rage::fwBasePool): a slot is free when flags[i] & 0x80, and the
// script handle of slot i is (i << 8) | flags[i].
struct iv_pool
{
    unsigned char *storage;
    unsigned char *flags;
    int            size;
    int            entry_size;
    int            first_free;
    int            used;
};

// implemented by the backend:
//   const char *backend_version_name(void)          game version for the titlebar
//   void        backend_request_model(unsigned int)  stream a model in, synchronously
//   struct iv_pool *backend_ped_pool(void)           may return NULL
//   struct iv_pool *backend_veh_pool(void)           may return NULL
// and the backend calls mod_iv's tick() once per frame from script context.

#if MOD_IV_CE
#include "backend_ce.h"
#else
#include "backend_ivsdk.h"
#endif
