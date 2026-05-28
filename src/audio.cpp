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

// TODO: write extended WAV header with channels number info

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
    uint64_t startTime;
    uint64_t endTime;
    std::vector<AudioEvent> events;
    int idx;

    AudioCapture() : startTime(0), endTime(0), idx(0) {}
};

class IDSBProxy;
static std::vector<AudioCapture> history;
static std::vector<IDSBProxy*> cache;
static string base_path;
static lock::CriticalSection acs;
static bool capture;
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

inline string get_fn(uint64_t a, int b) {
    return "a_" + std::to_string(a) + "_" + std::to_string(b) + ".wav";
}

class IDSBProxy : public IDirectSoundBuffer {
    AudioCapture cap;
    WavHeader header;
    ofs::File file;
    IDirectSoundBuffer* pBuf;
    uint32_t bytesWritten;
    uint64_t lastRealTime;
    double virtualTimeAcc;
    DWORD currentFreq;
    DWORD originalFreq;

    void push_event() {
        DWORD newFreq;
        LONG vol, pan;
        pBuf->GetFrequency(&newFreq);
        pBuf->GetVolume(&vol);
        pBuf->GetPan(&pan);

        uint64_t now = state::get_time(state::TimeOffset::None);

        if (cap.events.empty()) {
            cap.startTime = now;
            lastRealTime = now;
            virtualTimeAcc = 0;
            currentFreq = newFreq;
            originalFreq = newFreq;
        } else {
            uint64_t deltaReal = now - lastRealTime;
            double scale =
                (originalFreq > 0.0) ? (static_cast<double>(currentFreq) / originalFreq) : 1.0;
            virtualTimeAcc += static_cast<double>(deltaReal) * scale;
            lastRealTime = now;
            currentFreq = newFreq;
        }

        uint64_t offset = static_cast<uint64_t>(virtualTimeAcc);
        if (!cap.events.empty()) {
            auto& last = cap.events.back();
            if (last.timeOffset == offset) {
                last = {offset, vol, pan, newFreq};
                return;
            }
            if (last.frequency == newFreq && last.volume == vol && last.pan == pan)
                return;
        }
        cap.events.push_back({offset, vol, pan, newFreq});
    }

    void reinit_wav() {
        if (!capture)
            return;
        auto cur_time = audio_get_time();
        auto idx = gen_uid(cur_time);
        file = ofs::File(base_path + '\\' + get_fn(cur_time, idx), 1);
        file.write(&header, sizeof(WavHeader));
        bytesWritten = 0;
        cap.startTime = cap.endTime = cur_time;
        cap.idx = idx;
        cap.events.clear();
        push_event();
    }

public:
    void finalize_wav() {
        if (file.is_open()) {
            uint64_t now = audio_get_time();
            double scale = static_cast<double>(currentFreq) / static_cast<double>(originalFreq);
            virtualTimeAcc += static_cast<double>(now - lastRealTime) * scale;
            if (virtualTimeAcc <= 0.0) {
                spdlog::debug("Sound: got virtualTimeAcc <= 0");
                file.close();
                auto rem_ret = ofs::remove_file(base_path + '\\' + get_fn(cap.startTime, cap.idx));
                ENSURE(rem_ret);
                return;
            }
            cap.endTime = cap.startTime + static_cast<uint64_t>(virtualTimeAcc);
            uint32_t finalFileSize = bytesWritten + 36;
            file.seek(4, ofs::SeekBegin);
            file.write(&finalFileSize, 4);
            file.seek(24, ofs::SeekBegin);
            file.write(&header.sampleRate, 4);
            file.seek(28, ofs::SeekBegin);
            file.write(&header.byteRate, 4);
            file.seek(40, ofs::SeekBegin);
            file.write(&bytesWritten, 4);
            file.close();
            if (cap.endTime > cap.startTime) {
                history.push_back(cap);
                return;
            }
            spdlog::debug("Sound: got endTime <= startTime");
            auto rem_ret = ofs::remove_file(base_path + '\\' + get_fn(cap.startTime, cap.idx));
            ENSURE(rem_ret);
        }
    }

    IDSBProxy(IDirectSoundBuffer* pReal, LPCDSBUFFERDESC desc) {
        pBuf = pReal;
        header.sampleRate = desc->lpwfxFormat->nSamplesPerSec;
        header.channels = desc->lpwfxFormat->nChannels;
        header.bitsPerSample = desc->lpwfxFormat->wBitsPerSample;
        header.blockAlign = desc->lpwfxFormat->nBlockAlign;
        header.byteRate = desc->lpwfxFormat->nAvgBytesPerSec;
        originalFreq = desc->lpwfxFormat->nSamplesPerSec;
        currentFreq = originalFreq;
        bytesWritten = 0;
        lastRealTime = 0;
        virtualTimeAcc = 0.0;
        reinit_wav();
        lock::CSLock lock(acs);
        if (capture)
            cache.push_back(this);
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pBuf->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pBuf->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pBuf->Release();
        if (count == 0) {
            if (capture) {
                auto it = std::find(cache.begin(), cache.end(), this);
                ASS(it != cache.end());
                cache.erase(it);
            }
            delete this;
        }
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
        auto ret = pBuf->SetCurrentPosition(dwNewPosition);
        spdlog::debug("DirectSoundBuffer::SetCurrentPosition");
        return ret;
    }
    STDMETHOD(SetFormat)(LPCWAVEFORMATEX pcfxFormat) override {
        return pBuf->SetFormat(pcfxFormat);
    }
    STDMETHOD(SetFrequency)(DWORD f) override {
        HRESULT hr = pBuf->SetFrequency(f);
        if (capture) {
            lock::CSLock lock(acs);
            push_event();
        }
        return hr;
    }
    STDMETHOD(SetVolume)(LONG v) override {
        HRESULT hr = pBuf->SetVolume(v);
        if (capture) {
            lock::CSLock lock(acs);
            push_event();
        }
        return hr;
    }
    STDMETHOD(SetPan)(LONG p) override {
        HRESULT hr = pBuf->SetPan(p);
        if (capture) {
            lock::CSLock lock(acs);
            push_event();
        }
        return hr;
    }
    STDMETHOD(Stop)() override {
        if (capture) {
            lock::CSLock lock(acs);
            finalize_wav();
        }
        spdlog::debug("DirectSoundBuffer::Stop");
        return pBuf->Stop();
    }
    STDMETHOD(Unlock)(LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) override {
        if (capture) {
            lock::CSLock lock(acs);
            if (!file.is_open()) {
                reinit_wav();
                return pBuf->Unlock(pv1, db1, pv2, db2);
            }
            if (pv1 && db1 > 0) {
                file.write(pv1, db1);
                bytesWritten += db1;
            }
            if (pv2 && db2 > 0) {
                file.write(pv2, db2);
                bytesWritten += db2;
            }
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
        spdlog::error("Not implemented: DuplicateSoundBuffer");
        return DSERR_OUTOFMEMORY;
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
    // spdlog::debug("DirectSoundCreate");
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
    if (!cfg.allow_audio_hook && cfg.record_audio) {
        spdlog::warn(
            "Audio hook was disabled, but audio recording is enabled; enabling audio hook");
        cfg.disable_audio = false;
        cfg.allow_audio_hook = true;
    } else if (cfg.disable_audio && cfg.record_audio) {
        spdlog::warn("Audio is disabled, but recording is enabled; enabling audio");
        cfg.disable_audio = false;
        cfg.allow_audio_hook = true;
    }
    if (!cfg.allow_audio_hook && !cfg.disable_audio)
        return;
    if (cfg.record_audio)
        spdlog::warn("Audio recording is still in BETA");
    capture = cfg.record_audio;
    last_uid = 0;
    last_time = 0;
    if (capture) {
        base_path = string(files::get_cwd()) + '\\' + cfg.project_name + "\\temp_audio";
        auto dir_ret = ofs::make_dir(base_path);
        ENSURE(dir_ret);
    }
    HOOK_STR_ONLY("winmm.dll", mciSendCommand);
    auto hook_ret1 = hook::iat_hook(mem::get_base("mmfs2.dll"), "dsound.dll",
                                    mem::get_addr("dsound.dll", "DirectSoundCreate"),
                                    DirectSoundCreateH, &DirectSoundCreateO, true);
    ENSURE(hook_ret1);
    spdlog::info("Audio hooked");
}

void audio::flush() {
    if (!capture)
        return;
    lock::CSLock lock(acs);
    auto& cfg = conf::get();
    for (IDSBProxy* c : cache)
        c->finalize_wav();
    spdlog::debug("flush {}", history.size());
    if (history.empty())
        return;

    ofs::File filters(base_path + "\\audio_filters.txt", 1);
    ofs::File bat(base_path + "\\..\\audio_merge.bat", 1);
    string mix = "";
    size_t count = 0;
    bool support_pan = cfg.support_audio_panning;

    for (const auto& c : history) {
        string finalLabel = "[f" + std::to_string(count) + "]";
        double totalDuration = static_cast<double>(c.endTime - c.startTime) / 1000.0;
        ASS(!c.events.empty());
        size_t numSegs = c.events.size();
        filters.write("amovie=" + get_fn(c.startTime, c.idx) +
                      ",asplit=" + std::to_string(numSegs));
        for (size_t e = 0; e < numSegs; e++) {
            filters.write("[b" + std::to_string(count) + "s" + std::to_string(e) + "]");
        }
        filters.writeln(";");
        std::string segmentLabels = "";
        for (size_t e = 0; e < c.events.size(); e++) {
            std::string branchIn = "[b" + std::to_string(count) + "s" + std::to_string(e) + "]";
            std::string branchOut = "[p" + std::to_string(count) + "s" + std::to_string(e) + "]";

            double start = static_cast<double>(c.events[e].timeOffset) / 1000.0;
            double end = (e + 1 < c.events.size())
                             ? static_cast<double>(c.events[e + 1].timeOffset) / 1000.0
                             : totalDuration;
            double volLinear = std::pow(10.0, static_cast<double>(c.events[e].volume) / 2000.0);
            double panNorm = static_cast<double>(c.events[e].pan) / 10000.0;
            double leftGain = (panNorm <= 0.0) ? 1.0 : (1.0 - panNorm);
            double rightGain = (panNorm >= 0.0) ? 1.0 : (1.0 + panNorm);

            filters.writeln(branchIn + "atrim=start=" + std::to_string(start) +
                            ":end=" + std::to_string(end) +
                            ",asetrate=" + std::to_string(c.events[e].frequency) +
                            ",volume=" + std::to_string(volLinear) +
                            (support_pan ? ",pan=stereo|c0=" + std::to_string(leftGain) +
                                               "*c0|c1=" + std::to_string(rightGain) + "*c1"
                                         : "") +
                            ",aresample=48000" + branchOut + ";");

            segmentLabels += branchOut;
        }
        filters.writeln(segmentLabels + "concat=n=" + std::to_string(numSegs) + ":v=0:a=1,adelay=" +
                        std::to_string(c.startTime) + ":all=1" + finalLabel + ";");
        mix += finalLabel;
        count++;
    }
    filters.write(mix + "amix=inputs=" + std::to_string(count) + ":normalize=0[out]");
    bat.writeln("@echo off");
    bat.writeln("cd " + base_path);
    bat.writeln(
        "ffmpeg -y -/filter_complex audio_filters.txt -map \"[out]\" -ar 48000 ../audio.wav");
    bat.writeln("echo Waiting to delete wav cache...");
    bat.writeln("pause");
    bat.writeln("del a_*.wav");
    bat.writeln("del audio_filters.txt");
    history.clear();
    state::set_last_msg("Audio flushed, run audio_merge.bat script!");
}
