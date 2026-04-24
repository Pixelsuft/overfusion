#define WIN32_LEAN_AND_MEAN
#include "audiohooks.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "lock.hpp"
#include "state.hpp"
#include <Windows.h>
#include <mmsystem.h>
// after
#include <dsound.h>
#include <spdlog/spdlog.h>

static lock::CriticalSection acs;
// To get TAS time: state::get_time(state::TimeOffset::None) -> uint64_t

class IDSBProxy : public IDirectSoundBuffer {
    IDirectSoundBuffer* pBuf;

public:
    IDSBProxy(IDirectSoundBuffer* pReal) : pBuf(pReal) {}
    virtual ~IDSBProxy() {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pBuf->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pBuf->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pBuf->Release();
        if (count == 0)
            delete this;
        return count;
    }
    STDMETHOD(GetCaps)(LPDSBCAPS pDSBCaps) override { return pBuf->GetCaps(pDSBCaps); }
    STDMETHOD(GetCurrentPosition)(LPDWORD pdwCurrentPlayCursor,
                                  LPDWORD pdwCurrentWriteCursor) override {
        return pBuf->GetCurrentPosition(pdwCurrentPlayCursor, pdwCurrentWriteCursor);
    }
    STDMETHOD(GetFormat)(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated,
                         LPDWORD pdwSizeWritten) override {
        return pBuf->GetFormat(pwfxFormat, dwSizeAllocated, pdwSizeWritten);
    }
    STDMETHOD(GetVolume)(LPLONG plVolume) override { return pBuf->GetVolume(plVolume); }
    STDMETHOD(GetPan)(LPLONG plPan) override { return pBuf->GetPan(plPan); }
    STDMETHOD(GetFrequency)(LPDWORD pdwFrequency) override {
        return pBuf->GetFrequency(pdwFrequency);
    }
    STDMETHOD(GetStatus)(LPDWORD pdwStatus) override { return pBuf->GetStatus(pdwStatus); }

    STDMETHOD(Initialize)(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc) override {
        return pBuf->Initialize(pDirectSound, pcDSBufferDesc);
    }
    STDMETHOD(Lock)(DWORD dwOffset, DWORD dwBytes, LPVOID* ppvAudioPtr1, LPDWORD pdwAudioBytes1,
                    LPVOID* ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags) override {
        return pBuf->Lock(dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2,
                          pdwAudioBytes2, dwFlags);
    }
    STDMETHOD(Play)(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) override {
        spdlog::debug("DirectSoundBuffer::Play");
        return pBuf->Play(dwReserved1, dwPriority, dwFlags);
    }
    STDMETHOD(SetCurrentPosition)(DWORD dwNewPosition) override {
        return pBuf->SetCurrentPosition(dwNewPosition);
    }
    STDMETHOD(SetFormat)(LPCWAVEFORMATEX pcfxFormat) override {
        return pBuf->SetFormat(pcfxFormat);
    }
    STDMETHOD(SetVolume)(LONG lVolume) override { return pBuf->SetVolume(lVolume); }
    STDMETHOD(SetPan)(LONG lPan) override { return pBuf->SetPan(lPan); }
    STDMETHOD(SetFrequency)(DWORD dwFrequency) override { return pBuf->SetFrequency(dwFrequency); }
    STDMETHOD(Stop)() override { return pBuf->Stop(); }
    STDMETHOD(Unlock)(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2,
                      DWORD dwAudioBytes2) override {
        return pBuf->Unlock(pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
    }
    STDMETHOD(Restore)() override { return pBuf->Restore(); }
};

class IDSProxy : public IDirectSound {
    IDirectSound* pDev;

public:
    IDSProxy(IDirectSound* pReal) : pDev(pReal) {}
    virtual ~IDSProxy() {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pDev->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pDev->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pDev->Release();
        if (count == 0)
            delete this;
        return count;
    }
    STDMETHOD(CreateSoundBuffer)(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER* ppDSBuffer,
                                 LPUNKNOWN pUnkOuter) override {
        HRESULT hr = pDev->CreateSoundBuffer(pcDSBufferDesc, ppDSBuffer, pUnkOuter);
        if (SUCCEEDED(hr) && ppDSBuffer && *ppDSBuffer) {
            spdlog::debug("Wrapping IDirectSoundBuffer into IDSBProxy");
            *ppDSBuffer = new IDSBProxy(*ppDSBuffer);
        }
        return hr;
    }
    STDMETHOD(GetCaps)(LPDSCAPS pDSCaps) override { return pDev->GetCaps(pDSCaps); }
    STDMETHOD(DuplicateSoundBuffer)(LPDIRECTSOUNDBUFFER pDSBufferOriginal,
                                    LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate) override {
        HRESULT hr = pDev->DuplicateSoundBuffer(pDSBufferOriginal, ppDSBufferDuplicate);
        if (SUCCEEDED(hr) && ppDSBufferDuplicate && *ppDSBufferDuplicate) {
            spdlog::debug("Duplicating IDSBProxy");
            *ppDSBufferDuplicate = new IDSBProxy(*ppDSBufferDuplicate);
        }
        return hr;
    }
    STDMETHOD(SetCooperativeLevel)(HWND hwnd, DWORD dwLevel) override {
        spdlog::debug("DirectSound::SetCooperativeLevel (HWND: {:p}, Level: {})", (void*)hwnd,
                      dwLevel);
        return pDev->SetCooperativeLevel(hwnd, dwLevel);
    }
    STDMETHOD(Compact)() override { return pDev->Compact(); }
    STDMETHOD(GetSpeakerConfig)(LPDWORD pdwSpeakerConfig) override {
        return pDev->GetSpeakerConfig(pdwSpeakerConfig);
    }
    STDMETHOD(SetSpeakerConfig)(DWORD dwSpeakerConfig) override {
        return pDev->SetSpeakerConfig(dwSpeakerConfig);
    }
    STDMETHOD(Initialize)(LPCGUID pcGuidDevice) override { return pDev->Initialize(pcGuidDevice); }
};

static HRESULT(WINAPI* DirectSoundCreateO)(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk);
static HRESULT WINAPI DirectSoundCreateH(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk) {
    if (conf::get().disable_audio)
        return DSERR_NODRIVER;
    HRESULT hr = DirectSoundCreateO(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds) {
        spdlog::info("Wrapping IDirectSound into IDSProxy");
        *ds = new IDSProxy(*ds);
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
    if (!conf::get().allow_audio_hook)
        return;
    HOOK_STR_ONLY("winmm.dll", mciSendCommand);
    HOOK_AUTO("dsound.dll", DirectSoundCreate);
}
