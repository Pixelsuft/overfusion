#define WIN32_LEAN_AND_MEAN
#include "audiohooks.hpp"
#include "mem.hpp"
#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <spdlog/spdlog.h>

static HRESULT(WINAPI* DirectSoundCreateO)(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk);
static HRESULT WINAPI DirectSoundCreateH(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk) {
    return DSERR_NODRIVER;
    HRESULT hr = DirectSoundCreateO(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds) {
        spdlog::debug("TODO: DirectSoundCreateH");
    }
    return hr;
}

static MCIERROR mciSendCommandAH(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand,
                                 DWORD_PTR dwParam) {
    spdlog::error("mciSendCommandA: MCI audio playback is not supported");
    return MCIERR_DRIVER;
}

static MCIERROR mciSendCommandWH(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand,
                                 DWORD_PTR dwParam) {
    spdlog::error("mciSendCommandW: MCI audio playback is not supported");
    return MCIERR_DRIVER;
}

void audiohooks::init() {
    HOOK_STR_ONLY("winmm.dll", mciSendCommand);
    HOOK_AUTO("dsound.dll", DirectSoundCreate);
}
