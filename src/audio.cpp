#define WIN32_LEAN_AND_MEAN
#include "audio.hpp"
#include "config.hpp"
#include "files.hpp"
#include "lock.hpp"
#include "mem.hpp"
#include "state.hpp"
#include <Windows.h>
#include <mmsystem.h>
// after
#include <dsound.h>
#include <spdlog/spdlog.h>
#include <vector>

using std::string;

namespace audio {
struct AudioEvent {
    uint64_t timeOffset;
    DWORD frequency;
    LONG volume;
    LONG pan;
};

struct BufferSession {
    std::vector<AudioEvent> events;
    uint64_t startTime;
    uint64_t endTime;
    int bufferId;
};

// To get TAS time: state::get_time(state::TimeOffset::None) -> uint64_t
// To use lock: lock::CSLock cs(acs);
static std::vector<BufferSession> g_history;
static string base_path;
static lock::CriticalSection acs;
static int g_buffer_counter;
static bool capture_audio;

inline double ds_vol_to_linear(LONG vol) {
    if (vol <= -10000)
        return 0.0;
    return std::pow(10.0, (double)vol / 2000.0);
}

inline void ds_pan_to_weights(LONG pan, double& left, double& right) {
    double p = (double)pan / 10000.0;
    left = (p > 0) ? (1.0 - p) : 1.0;
    right = (p < 0) ? (1.0 + p) : 1.0;
}

class IDSBProxy : public IDirectSoundBuffer {
    IDirectSoundBuffer* pBuf;
    int internal_id;
    ofs::File rec_file;
    BufferSession session;
    WAVEFORMATEX wfx;
    uint32_t data_bytes;

    void push_event() {
        DWORD freq;
        LONG vol, pan;
        pBuf->GetFrequency(&freq);
        pBuf->GetVolume(&vol);
        pBuf->GetPan(&pan);
        uint64_t now = state::get_time(state::TimeOffset::None);
        session.events.push_back({now - session.startTime, freq, vol, pan});
    }

    void finalize_wav() {
        if (!rec_file.is_open())
            return;
        uint32_t riff_size = data_bytes + 36;
        rec_file.seek(4, ofs::SeekBegin);
        rec_file.write(&riff_size, 4);
        rec_file.seek(40, ofs::SeekBegin);
        rec_file.write(&data_bytes, 4);
        rec_file.close();
        session.endTime = state::get_time(state::TimeOffset::None);
        lock::CSLock lock(acs);
        g_history.push_back(session);
        session.events.clear();
    }

    void start_wav() {
        finalize_wav();
        session.bufferId = internal_id;
        session.startTime = state::get_time(state::TimeOffset::None);
        data_bytes = 0;

        char path[MAX_PATH];
        sprintf_s(path, "%s\\%llu_%d.wav", base_path.c_str(), session.startTime, internal_id);

        if (rec_file.open(path, 1)) {
            DWORD read;
            pBuf->GetFormat(&wfx, sizeof(wfx), &read);
            rec_file.write("RIFF\0\0\0\0WAVEfmt ", 16);
            uint32_t f_sz = 16;
            rec_file.write(&f_sz, 4);
            rec_file.write(&wfx, 16);
            rec_file.write("data\0\0\0\0", 8);
            push_event();
        }
    }

public:
    IDSBProxy(IDirectSoundBuffer* pReal)
        : pBuf(pReal), internal_id(g_buffer_counter++), data_bytes(0) {}
    virtual ~IDSBProxy() { finalize_wav(); }
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
        start_wav();
        return pBuf->Play(dwReserved1, dwPriority, dwFlags);
    }
    STDMETHOD(SetCurrentPosition)(DWORD dwNewPosition) override {
        start_wav();
        return pBuf->SetCurrentPosition(dwNewPosition);
    }
    STDMETHOD(SetFormat)(LPCWAVEFORMATEX pcfxFormat) override {
        return pBuf->SetFormat(pcfxFormat);
    }
    STDMETHOD(SetFrequency)(DWORD f) override {
        HRESULT hr = pBuf->SetFrequency(f);
        push_event();
        return hr;
    }
    STDMETHOD(SetVolume)(LONG v) override {
        HRESULT hr = pBuf->SetVolume(v);
        push_event();
        return hr;
    }
    STDMETHOD(SetPan)(LONG p) override {
        HRESULT hr = pBuf->SetPan(p);
        push_event();
        return hr;
    }
    STDMETHOD(Stop)() override {
        finalize_wav();
        return pBuf->Stop();
    }
    STDMETHOD(Unlock)(LPVOID p1, DWORD b1, LPVOID p2, DWORD b2) override {
        if (rec_file.is_open()) {
            if (p1) {
                rec_file.write(p1, b1);
                data_bytes += b1;
            }
            if (p2) {
                rec_file.write(p2, b2);
                data_bytes += b2;
            }
        }
        return pBuf->Unlock(p1, b1, p2, b2);
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
} // namespace audio

static HRESULT(WINAPI* DirectSoundCreateO)(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk);
static HRESULT WINAPI DirectSoundCreateH(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk) {
    if (conf::get().disable_audio)
        return DSERR_NODRIVER;
    HRESULT hr = DirectSoundCreateO(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds) {
        spdlog::info("Wrapping IDirectSound into IDSProxy");
        *ds = new audio::IDSProxy(*ds);
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

void audio::init() {
    auto& cfg = conf::get();
    if (!cfg.allow_audio_hook && !cfg.disable_audio)
        return;
    capture_audio = cfg.record_audio;
    g_buffer_counter = 0;
    if (capture_audio) {
        base_path = string(files::get_cwd()) + '\\' + cfg.project_name + "\\temp_audio";
        auto dir_ret = ofs::make_dir(base_path);
        ENSURE(dir_ret);
    }
    HOOK_STR_ONLY("winmm.dll", mciSendCommand);
    HOOK_AUTO("dsound.dll", DirectSoundCreate);
    spdlog::debug("Audio hooked");
}

void audio::flush() {
    if (!capture_audio || g_history.empty())
        return;
    capture_audio = false;
    ofs::File fFile(base_path + "\\filters.txt", 1);
    ofs::File bFile(base_path + "\\mix.bat", 1);
    std::string filters = "";
    std::string mix_labels = "";
    for (size_t i = 0; i < g_history.size(); ++i) {
        auto& h = g_history[i];
        std::string finalLabel = "[final" + std::to_string(i) + "]";
        std::string fn = std::to_string(h.startTime) + "_" + std::to_string(h.bufferId) + ".wav";
        double totalDur = (double)(h.endTime - h.startTime) / 1000.0;

        // Split source for each event segment
        filters += "amovie=" + fn + ",asplit=" + std::to_string(h.events.size());
        for (size_t e = 0; e < h.events.size(); ++e)
            filters += "[s" + std::to_string(i) + "_" + std::to_string(e) + "]";
        filters += ";\n";

        std::string concat_in = "";
        for (size_t e = 0; e < h.events.size(); ++e) {
            auto& ev = h.events[e];
            double start = (double)ev.timeOffset / 1000.0;
            double end =
                (e + 1 < h.events.size()) ? (double)h.events[e + 1].timeOffset / 1000.0 : totalDur;

            double v = ds_vol_to_linear(ev.volume);
            double l, r;
            ds_pan_to_weights(ev.pan, l, r);

            std::string bOut = "[p" + std::to_string(i) + "_" + std::to_string(e) + "]";
            filters += "[s" + std::to_string(i) + "_" + std::to_string(e) +
                       "]atrim=start=" + std::to_string(start) + ":end=" + std::to_string(end) +
                       ",asetrate=" + std::to_string(ev.frequency) +
                       ",pan=stereo|c0=" + std::to_string(l) + "*c0|c1=" + std::to_string(r) +
                       "*c1" + ",volume=" + std::to_string(v) + ",aresample=48000" + bOut + ";\n";
            concat_in += bOut;
        }

        filters += concat_in + "concat=n=" + std::to_string(h.events.size()) +
                   ":v=0:a=1,adelay=" + std::to_string(h.startTime) + ":all=1" + finalLabel + ";\n";
        mix_labels += finalLabel;
    }

    filters += mix_labels + "amix=inputs=" + std::to_string(g_history.size()) + ":normalize=0[out]";
    fFile.write(filters.c_str(), filters.size());

    std::string bat = "@echo off\nffmpeg -y -/filter_complex_script filters.txt -map \"[out]\" -ar "
                      "48000 audio_final.wav\npause";
    bFile.write(bat.c_str(), bat.size());
}
