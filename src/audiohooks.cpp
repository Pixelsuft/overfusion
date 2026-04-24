#define WIN32_LEAN_AND_MEAN
#include "audiohooks.hpp"
#include "config.hpp"
#include "lock.hpp"
#include "mem.hpp"
#include "state.hpp"
#include <Windows.h>
#include <mmsystem.h>
// after
#include <dsound.h>
#include <spdlog/spdlog.h>

// To get TAS time: state::get_time(state::TimeOffset::None) -> uint64_t

class IDSBProxy : public IDirectSoundBuffer {
    std::vector<BYTE> m_virtualData;
    IDirectSoundBuffer* pReal;
    lock::CriticalSection alock;
    DWORD m_bufSize;
    DWORD m_bytesPerSec;
    double m_vPlayPos;
    uint64_t m_lastTASClock;
    bool m_playing;

    void UpdateTAS() {
        lock::CSLock cs(alock);
        uint64_t currentTAS = state::get_time(state::TimeOffset::None);
        if (m_playing && m_lastTASClock != 0 && currentTAS > m_lastTASClock) {
            uint64_t deltaMs = currentTAS - m_lastTASClock;
            double bytesToAdvance = (deltaMs / 1000.0) * m_bytesPerSec;
            m_vPlayPos = std::fmod(m_vPlayPos + bytesToAdvance, (double)m_bufSize);
            DWORD rPlay, rWrite;
            pReal->GetCurrentPosition(&rPlay, &rWrite);
            void *p1, *p2;
            DWORD d1, d2;
            if (SUCCEEDED(pReal->Lock(0, m_bufSize, &p1, &d1, &p2, &d2, 0))) {
                std::memcpy(p1, m_virtualData.data(), d1);
                if (p2)
                    std::memcpy(p2, m_virtualData.data() + d1, d2);
                pReal->Unlock(p1, d1, p2, d2);
            }
        } else if (currentTAS == m_lastTASClock) {
            pReal->Stop();
        } else if (m_playing) {
            pReal->Play(0, 0, DSBPLAY_LOOPING);
        }
        m_lastTASClock = currentTAS;
    }

public:
    IDSBProxy(IDirectSoundBuffer* pReal)
        : pReal(pReal), m_bufSize(0), m_bytesPerSec(0), m_vPlayPos(0), m_lastTASClock(0),
          m_playing(false) {
        DSBCAPS caps = {sizeof(caps)};
        pReal->GetCaps(&caps);
        m_bufSize = caps.dwBufferBytes;
        m_virtualData.resize(m_bufSize, 0);

        WAVEFORMATEX wfx;
        if (SUCCEEDED(pReal->GetFormat(&wfx, sizeof(wfx), NULL))) {
            m_bytesPerSec = wfx.nAvgBytesPerSec;
        }
    }
    virtual ~IDSBProxy() {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pReal->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pReal->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pReal->Release();
        if (count == 0)
            delete this;
        return count;
    }
    STDMETHOD(GetCaps)(LPDSBCAPS pCaps) override { return pReal->GetCaps(pCaps); }
    STDMETHOD(GetCurrentPosition)(LPDWORD pdwPlay, LPDWORD pdwWrite) override {
        UpdateTAS();
        if (pdwPlay)
            *pdwPlay = (DWORD)m_vPlayPos;
        if (pdwWrite)
            *pdwWrite = ((DWORD)m_vPlayPos + (m_bytesPerSec / 10)) % m_bufSize;
        return DS_OK;
    }
    STDMETHOD(GetFormat)(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated,
                         LPDWORD pdwSizeWritten) override {
        return pReal->GetFormat(pwfxFormat, dwSizeAllocated, pdwSizeWritten);
    }
    STDMETHOD(GetVolume)(LPLONG plVolume) override { return pReal->GetVolume(plVolume); }
    STDMETHOD(GetPan)(LPLONG plPan) override { return pReal->GetPan(plPan); }
    STDMETHOD(GetFrequency)(LPDWORD pdwFrequency) override {
        return pReal->GetFrequency(pdwFrequency);
    }
    STDMETHOD(GetStatus)(LPDWORD pdwStatus) override { return pReal->GetStatus(pdwStatus); }

    STDMETHOD(Initialize)(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc) override {
        return pReal->Initialize(pDirectSound, pcDSBufferDesc);
    }
    STDMETHOD(Lock)(DWORD offset, DWORD bytes, LPVOID* pp1, LPDWORD pd1, LPVOID* pp2, LPDWORD pd2,
                    DWORD flags) override {
        if (offset >= m_bufSize)
            offset %= m_bufSize;
        *pp1 = m_virtualData.data() + offset;
        DWORD canWrite = m_bufSize - offset;
        if (bytes > canWrite) {
            *pd1 = canWrite;
            *pp2 = m_virtualData.data();
            *pd2 = bytes - canWrite;
        } else {
            *pd1 = bytes;
            *pp2 = nullptr;
            *pd2 = 0;
        }
        return DS_OK;
    }
    STDMETHOD(Play)(DWORD r, DWORD p, DWORD f) override {
        spdlog::debug("DirectSoundBuffer::Play");
        m_playing = true;
        m_lastTASClock = state::get_time(state::TimeOffset::None);
        return pReal->Play(r, p, f | DSBPLAY_LOOPING);
    }
    STDMETHOD(SetCurrentPosition)(DWORD pos) override {
        m_vPlayPos = pos;
        return DS_OK;
    }
    STDMETHOD(SetFormat)(LPCWAVEFORMATEX p) override {
        if (p)
            m_bytesPerSec = p->nAvgBytesPerSec;
        return pReal->SetFormat(p);
    }
    STDMETHOD(SetVolume)(LONG lVolume) override { return pReal->SetVolume(lVolume); }
    STDMETHOD(SetPan)(LONG lPan) override { return pReal->SetPan(lPan); }
    STDMETHOD(SetFrequency)(DWORD dwFrequency) override { return pReal->SetFrequency(dwFrequency); }
    STDMETHOD(Stop)() override {
        m_playing = false;
        return pReal->Stop();
    }
    STDMETHOD(Unlock)(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2,
                      DWORD dwAudioBytes2) override {
        return DS_OK;
    }
    STDMETHOD(Restore)() override { return pReal->Restore(); }
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
    auto& cfg = conf::get();
    if (!cfg.allow_audio_hook && !cfg.disable_audio)
        return;
    HOOK_STR_ONLY("winmm.dll", mciSendCommand);
    HOOK_AUTO("dsound.dll", DirectSoundCreate);
    spdlog::debug("Audio hooks enabled");
}
