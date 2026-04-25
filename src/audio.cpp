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
#include <algorithm>
#include <dsound.h>
#include <spdlog/spdlog.h>
#include <vector>
#undef min
#undef max

using std::string;

namespace audio {
#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtLen;
    uint16_t formatTag;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataLen;

    WavHeader() {
        std::memcpy(riff, "RIFF", 4);
        fileSize = 0;
        std::memcpy(wave, "WAVE", 4);
        std::memcpy(fmt, "fmt ", 4);
        fmtLen = 16;
        formatTag = 1;
        channels = 0;
        sampleRate = 0;
        byteRate = 0;
        blockAlign = 0;
        bitsPerSample = 0;
        std::memcpy(data, "data", 4);
        dataLen = 0;
    }
};
#pragma pack(pop)

struct AudioEvent {
    uint64_t timeOffset;
    LONG volume;
    LONG pan;
    DWORD frequency;
};

struct AudioCapture {
    ofs::File file;
    WavHeader h;
    uint64_t startTime;
    uint64_t endTime;
    uint32_t bytesWritten;
    std::vector<AudioEvent> events;
    LONG lastVol;
    LONG lastPan;
    int idx;

    AudioCapture() : startTime(0), endTime(0), bytesWritten(0), idx(0), lastVol(0), lastPan(0) {}
};

class IDSBProxy;
static std::vector<AudioCapture> g_history;
static std::vector<IDSBProxy*> cache;
static string base_path;
static lock::CriticalSection acs;
static int g_buffer_counter;
static bool capture_audio;
static uint64_t last_time;
static int last_uid;

static int gen_uid(uint64_t mytime) {
    if (mytime != last_time) {
        last_time = mytime;
        last_uid = 0;
    }
    return last_uid++;
}

inline uint64_t audio_get_time() { return state::get_time(state::TimeOffset::None); }

static void write_original_raw(AudioCapture& cap, const void* data, uint32_t len) {
    cap.file.write(data, len);
    cap.bytesWritten += len;
}

class IDSBProxy : public IDirectSoundBuffer {
    IDirectSoundBuffer* pBuf;
    AudioCapture cap;

    void push_event() {
        DWORD freq;
        LONG vol, pan;
        pBuf->GetFrequency(&freq);
        pBuf->GetVolume(&vol);
        pBuf->GetPan(&pan);

        uint64_t now = state::get_time(state::TimeOffset::None);
        uint64_t offset = now - cap.startTime;

        if (!cap.events.empty()) {
            auto& last = cap.events.back();
            if (last.timeOffset == offset) {
                last = {offset, vol, pan, freq};
                return;
            }
            if (last.frequency == freq && last.volume == vol && last.pan == pan)
                return;
        }
        cap.events.push_back({offset, vol, pan, freq});
    }

    void reinit_wav() {
        auto cur_time = audio_get_time();
        auto idx = gen_uid(cur_time);
        string fn =
            base_path + "\\audio_" + std::to_string(cur_time) + "_" + std::to_string(idx) + ".wav";
        cap.file = ofs::File(fn, 1);
        cap.file.write(&cap.h, sizeof(WavHeader));
        cap.bytesWritten = 0;
        cap.startTime = cap.endTime = cur_time;
        cap.idx = idx;
        cap.events.clear();
        push_event();
    }

public:
    void finalize_wav() {
        if (cap.file.is_open()) {
            cap.endTime = audio_get_time();
            uint32_t finalFileSize = cap.bytesWritten + 36;
            cap.file.seek(4, ofs::SeekBegin);
            cap.file.write(&finalFileSize, 4);
            cap.file.seek(24, ofs::SeekBegin);
            cap.file.write(&cap.h.sampleRate, 4);
            cap.file.seek(28, ofs::SeekBegin);
            cap.file.write(&cap.h.byteRate, 4);
            cap.file.seek(40, ofs::SeekBegin);
            cap.file.write(&cap.bytesWritten, 4);
            cap.file.close();
            g_history.push_back(std::move(cap));
        }
    }
    IDSBProxy(IDirectSoundBuffer* pReal, LPCDSBUFFERDESC desc) {
        pBuf = pReal;
        cap.h.sampleRate = desc->lpwfxFormat->nSamplesPerSec;
        cap.h.channels = desc->lpwfxFormat->nChannels;
        cap.h.bitsPerSample = desc->lpwfxFormat->wBitsPerSample;
        cap.h.blockAlign = desc->lpwfxFormat->nBlockAlign;
        cap.h.byteRate = desc->lpwfxFormat->nAvgBytesPerSec;
        reinit_wav();
        cache.push_back(this);
    }
    virtual ~IDSBProxy() {
        auto it = std::find(cache.begin(), cache.end(), this);
        ENSURE(it != cache.end());
        cache.erase(it);
    }
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
        // start_wav();
        return pBuf->Play(dwReserved1, dwPriority, dwFlags);
    }
    STDMETHOD(SetCurrentPosition)(DWORD dwNewPosition) override {
        auto ret = pBuf->SetCurrentPosition(dwNewPosition);
        return ret;
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
        lock::CSLock lock(acs);
        finalize_wav();
        return pBuf->Stop();
    }
    STDMETHOD(Unlock)(LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) override {
        lock::CSLock lock(acs);
        if (!cap.file.is_open()) {
            reinit_wav();
            return pBuf->Unlock(pv1, db1, pv2, db2);
        }
        if (pv1 && db1 > 0) {
            cap.file.write(pv1, db1);
            cap.bytesWritten += db1;
        }
        if (pv2 && db2 > 0) {
            cap.file.write(pv2, db2);
            cap.bytesWritten += db2;
        }
        return pBuf->Unlock(pv1, db1, pv2, db2);
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
        if (SUCCEEDED(hr) && ppDSBuffer && *ppDSBuffer && pcDSBufferDesc->lpwfxFormat) {
            spdlog::debug("Wrapping IDirectSoundBuffer into IDSBProxy");
            *ppDSBuffer = new IDSBProxy(*ppDSBuffer, pcDSBufferDesc);
        }
        return hr;
    }
    STDMETHOD(GetCaps)(LPDSCAPS pDSCaps) override { return pDev->GetCaps(pDSCaps); }
    STDMETHOD(DuplicateSoundBuffer)(LPDIRECTSOUNDBUFFER pDSBufferOriginal,
                                    LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate) override {
        HRESULT hr = pDev->DuplicateSoundBuffer(pDSBufferOriginal, ppDSBufferDuplicate);
        if (SUCCEEDED(hr) && ppDSBufferDuplicate && *ppDSBufferDuplicate) {
            spdlog::debug("Duplicating IDSBProxy");
            // FIXME
            *ppDSBufferDuplicate = new IDSBProxy(*ppDSBufferDuplicate, nullptr);
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
    last_uid = 0;
    last_time = 0;
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
    lock::CSLock lock(acs);
    for (IDSBProxy* c : cache)
        c->finalize_wav();
    spdlog::debug("flush {}", g_history.size());
    if (g_history.empty())
        return;

    ofs::File filterF(base_path + "\\audio_filters.txt", 1);
    ofs::File batF(base_path + "\\..\\audio_merge.bat", 1);

    string mix = "";

    for (size_t i = 0; i < g_history.size(); i++) {
        auto& c = g_history[i];
        string fn = "audio_" + std::to_string(c.startTime) + "_" + std::to_string(c.idx) + ".wav";
        string finalLabel = "[f" + std::to_string(i) + "]";
        double totalDuration = (double)(c.endTime - c.startTime) / 1000.0;
        if (c.events.empty()) {
            filterF.writeln("amovie=" + fn + ",atrim=duration=" + std::to_string(totalDuration) +
                            ",aresample=48000,adelay=" + std::to_string(c.startTime) + ":all=1" +
                            finalLabel + ";");
        } else {
            int numSegs = (int)c.events.size();
            filterF.write("amovie=" + fn + ",asplit=" + std::to_string(numSegs));
            for (int e = 0; e < numSegs; ++e) {
                filterF.write("[b" + std::to_string(i) + "s" + std::to_string(e) + "]");
            }
            filterF.writeln(";");
            std::string segmentLabels = "";
            for (size_t e = 0; e < c.events.size(); ++e) {
                std::string branchIn = "[b" + std::to_string(i) + "s" + std::to_string(e) + "]";
                std::string branchOut = "[p" + std::to_string(i) + "s" + std::to_string(e) + "]";

                double start = (double)c.events[e].timeOffset / 1000.0;
                double end = (e + 1 < c.events.size()) ? (double)c.events[e + 1].timeOffset / 1000.0
                                                       : totalDuration;
                double volLinear = pow(10.0, (double)c.events[e].volume / 2000.0);
                double panNorm = static_cast<double>(c.events[e].pan) / 10000.0;
                double volLeft = std::min(1.0, 1.0 - panNorm) *
                                 std::pow(10.0, static_cast<double>(c.events[e].volume) / 2000.0);
                double volRight = std::min(1.0, 1.0 + panNorm) *
                                  std::pow(10.0, static_cast<double>(c.events[e].volume) / 2000.0);

                filterF.writeln(branchIn + "atrim=start=" + std::to_string(start) +
                                ":end=" + std::to_string(end) +
                                ",asetrate=" + std::to_string(c.events[e].frequency) +
                                ",volume=" + std::to_string(volLinear) +
                                //",pan=stereo|c0=" + std::to_string(volLeft) +
                                // "*c0|c1=" + std::to_string(volRight) + "*c1" +
                                ",aresample=48000" + branchOut + ";");

                segmentLabels += branchOut;
            }
            filterF.writeln(segmentLabels + "concat=n=" + std::to_string(numSegs) +
                            ":v=0:a=1,adelay=" + std::to_string(c.startTime) + ":all=1" +
                            finalLabel + ";");
        }
        mix += finalLabel;
    }

    filterF.write(mix + "amix=inputs=" + std::to_string(g_history.size()) + ":normalize=0[out]");

    batF.writeln("@echo off");
    batF.writeln("cd temp_audio");
    batF.writeln(
        "ffmpeg -y -/filter_complex audio_filters.txt -map \"[out]\" -ar 48000 ../audio.wav");
    batF.writeln("pause");
    g_history.clear();
}
