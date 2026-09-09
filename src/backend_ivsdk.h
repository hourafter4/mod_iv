#pragma once
// Backend for GTAIV.exe 1.0.7.0 / 1.0.8.0: Zolika1351's IV-SDK, which knows
// the addresses of both. Natives go through CTheScripts::FindNativeAddress,
// the per-frame callback is the SDK's processScriptsEvent (which runs inside
// the game's script processing, with a dummy script thread installed), and the
// pools are SDK globals.
#include "IVSDK.cpp"   // IV-SDK is header-only and defines DllMain

static const char *
backend_version_name (void)
{
    switch (plugin::gameVer)
    {
    case plugin::VERSION_1070: return "1.0.7.0";
    case plugin::VERSION_1080: return "1.0.8.0";
    default:                   return "unsupported";
    }
}

static void
backend_request_model (unsigned int model)
{
    CStreaming::ScriptRequestModel ((int32_t) model);
    CStreaming::LoadAllRequestedModels (false);
}

static struct iv_pool *backend_ped_pool (void) { return (struct iv_pool *) CPools::ms_pPedPool; }
static struct iv_pool *backend_veh_pool (void) { return (struct iv_pool *) CPools::ms_pVehiclePool; }
