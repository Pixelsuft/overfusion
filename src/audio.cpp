#define WIN32_LEAN_AND_MEAN
#include "audio.hpp"
#include "config.hpp"
#include "lock.hpp"
#include "mem.hpp"
#include "state.hpp"
#include <Windows.h>
#include <mmsystem.h>
// after
#include <algorithm>
#include <dsound.h>
#include "log.hpp"
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

// Happens when something changes in the current audio
struct AudioEvent {
    uint64_t timeOffset;
    LONG volume;
    LONG pan;
    DWORD frequency;
};

// Remember the timeline, including events
struct AudioCapture {
    uint64_t startTime;
    uint64_t endTime;
    uint64_t hash;
    std::vector<AudioEvent> events;
    int idx;

    AudioCapture() : startTime(0), endTime(0), hash(0), idx(0) {}
};

class IDSBProxy;
// History for generating filter
static std::vector<AudioCapture> history;
// Cache so we can include still-not-finished audio on 'flush'
static std::vector<IDSBProxy*> cache;
// WAVs path
static string base_path;
// Global audio lock
static lock::CriticalSection acs;
// Are we capturing?
static bool capturing;
// For generating fn
static uint64_t last_time;
// Time offset
static uint64_t cap_time_offset;
// For generating fn
static int last_uid;

// Generate deterministic fn based on the current time
static int gen_uid(uint64_t mytime) {
    if (mytime != last_time) {
        last_time = mytime;
        last_uid = 0;
    }
    return last_uid++;
}

// Layer func for getting current audio time
inline uint64_t audio_get_time() {
    return state::get_time(state::TimeOffset::None) - audio::cap_time_offset;
}

// Generate unique filename via 'time' and 'idx'
inline string audio_get_fn(uint64_t hash, uint64_t a, int b) {
    if (hash == 0)
        return "a" + std::to_string(a) + "_" + std::to_string(b) + ".wav";
    else
        return "ah" + std::to_string(hash) + ".wav";
}

// Hash func to avoid some 1-to-1 duplicates
static uint64_t murmurhash3(const std::vector<uint8_t>& vec, uint64_t seed = 0xadc83b19ULL) {
    if (vec.size() > 1024 * 1024) // TODO: configure this
        return 0;
    const uint8_t* data = vec.data();
    const size_t len = vec.size();
    const size_t nblocks = len / 8;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5b8ed44174149ULL;

    const uint64_t* blocks = reinterpret_cast<const uint64_t*>(data);

    for (size_t i = 0; i < nblocks; ++i) {
        uint64_t k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 31) | (k1 >> 33);
        k1 *= c2;
        h1 ^= k1;

        h1 = (h1 << 27) | (h1 >> 37);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;
    }

    const uint8_t* tail = data + nblocks * 8;
    uint64_t k2 = 0;

    switch (len & 7) {
    case 7:
        k2 ^= static_cast<uint64_t>(tail[6]) << 48;
    case 6:
        k2 ^= static_cast<uint64_t>(tail[5]) << 40;
    case 5:
        k2 ^= static_cast<uint64_t>(tail[4]) << 32;
    case 4:
        k2 ^= static_cast<uint64_t>(tail[3]) << 24;
    case 3:
        k2 ^= static_cast<uint64_t>(tail[2]) << 16;
    case 2:
        k2 ^= static_cast<uint64_t>(tail[1]) << 8;
    case 1:
        k2 ^= static_cast<uint64_t>(tail[0]);
        k2 *= c1;
        k2 = (k2 << 31) | (k2 >> 33);
        k2 *= c2;
        h1 ^= k2;
    };

    h1 ^= len;
    h1 ^= h1 >> 33;
    h1 *= 0xff51afd7ed558ccdULL;
    h1 ^= h1 >> 33;
    h1 *= 0xc4ceb9fe1a85ec53ULL;
    h1 ^= h1 >> 33;

    return h1;
}

// IDirectSoundBuffer proxy hook
class IDSBProxy : public IDirectSoundBuffer {
    WavHeader header;
    AudioCapture cap;
    std::vector<uint8_t> data;
    uint64_t lastRealTime;
    double virtualTimeAcc;
    IDirectSoundBuffer* pBuf;
    DWORD currentFreq;
    DWORD originalFreq;
    bool inited;

    void push_event() {
        // Remember current params in case something has changed
        DWORD newFreq;
        LONG vol, pan;
        pBuf->GetFrequency(&newFreq);
        pBuf->GetVolume(&vol);
        pBuf->GetPan(&pan);

        uint64_t now = audio_get_time();

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
            // Last event has the same time -> override it
            if (last.timeOffset == offset) {
                last = {offset, vol, pan, newFreq};
                return;
            }
            // Last event has the same params -> skip it
            if (last.frequency == newFreq && last.volume == vol && last.pan == pan)
                return;
        }
        cap.events.push_back({offset, vol, pan, newFreq});
    }

    void reinit_wav() {
        if (!capturing)
            return;
        // Open audio file, write default WAV header
        auto cur_time = audio_get_time();
        auto idx = gen_uid(cur_time);
        data.clear();
        inited = true;
        cap.startTime = cap.endTime = cur_time;
        cap.idx = idx;
        cap.events.clear();
        push_event();
    }

public:
    void finalize_wav() {
        // Update header, close file, remove if empty
        if (inited) {
            inited = false;
            uint64_t now = audio_get_time();
            double scale = static_cast<double>(currentFreq) / static_cast<double>(originalFreq);
            virtualTimeAcc += static_cast<double>(now - lastRealTime) * scale;
            if (virtualTimeAcc <= 0.0) {
                of::debug("Audio got virtualTimeAcc <= 0");
                return;
            }
            cap.endTime = cap.startTime + static_cast<uint64_t>(virtualTimeAcc);
            if (cap.endTime > cap.startTime && !data.empty()) {
                cap.hash = murmurhash3(data);
                auto path = base_path + '\\' + audio_get_fn(cap.hash, cap.startTime, cap.idx);
                if (!ofs::file_exists(path)) {
                    auto file = ofs::File(path, 1);
                    ENSURE(file.is_open());
                    if (!file.is_open())
                        return;
                    header.fileSize = data.size() + 36;
                    header.dataLen = data.size();
                    file.write(&header, sizeof(WavHeader));
                    file.write(data.data(), data.size());
                } else
                    of::debug("Audio duplicate skipped");
                history.push_back(cap);
                return;
            }
            of::debug("Audio got endTime <= startTime");
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
        lastRealTime = 0;
        virtualTimeAcc = 0.0;
        inited = false;
        reinit_wav();
        lock::CSLock lock(acs);
        if (capturing)
            cache.push_back(this);
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pBuf->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pBuf->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pBuf->Release();
        if (count == 0) {
            // We should manually delete IDSBProxy
            if (capturing) {
                lock::CSLock lock(acs);
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
        of::debug("DirectSoundBuffer::Play");
        return pBuf->Play(dwReserved1, dwPriority, dwFlags);
    }
    STDMETHOD(SetCurrentPosition)(DWORD dwNewPosition) override {
        auto ret = pBuf->SetCurrentPosition(dwNewPosition);
        of::debug("DirectSoundBuffer::SetCurrentPosition");
        return ret;
    }
    STDMETHOD(SetFormat)(LPCWAVEFORMATEX pcfxFormat) override {
        return pBuf->SetFormat(pcfxFormat);
    }
    STDMETHOD(SetFrequency)(DWORD f) override {
        HRESULT hr = pBuf->SetFrequency(f);
        if (capturing) {
            lock::CSLock lock(acs);
            push_event();
        }
        return hr;
    }
    STDMETHOD(SetVolume)(LONG v) override {
        HRESULT hr = pBuf->SetVolume(v);
        if (capturing) {
            lock::CSLock lock(acs);
            push_event();
        }
        return hr;
    }
    STDMETHOD(SetPan)(LONG p) override {
        HRESULT hr = pBuf->SetPan(p);
        if (capturing) {
            lock::CSLock lock(acs);
            push_event();
        }
        return hr;
    }
    STDMETHOD(Stop)() override {
        if (capturing) {
            lock::CSLock lock(acs);
            finalize_wav();
        }
        of::debug("DirectSoundBuffer::Stop");
        return pBuf->Stop();
    }
    STDMETHOD(Unlock)(LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) override {
        if (capturing) {
            lock::CSLock lock(acs);
            if (!inited) {
                reinit_wav();
                return pBuf->Unlock(pv1, db1, pv2, db2);
            }
            if (pv1 && db1 > 0)
                data.insert(data.end(), reinterpret_cast<uint8_t*>(pv1),
                            reinterpret_cast<uint8_t*>(pv1) + db1);
            if (pv2 && db2 > 0)
                data.insert(data.end(), reinterpret_cast<uint8_t*>(pv2),
                            reinterpret_cast<uint8_t*>(pv2) + db2);
        }
        return pBuf->Unlock(pv1, db1, pv2, db2);
    }
    STDMETHOD(Restore)() override { return pBuf->Restore(); }
};

// IDirectSound proxy hook
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
        // Again, IDSProxy should be deleted manually
        if (count == 0)
            delete this;
        return count;
    }
    STDMETHOD(CreateSoundBuffer)(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER* ppDSBuffer,
                                 LPUNKNOWN pUnkOuter) override {
        HRESULT hr = pDev->CreateSoundBuffer(pcDSBufferDesc, ppDSBuffer, pUnkOuter);
        if (SUCCEEDED(hr) && ppDSBuffer && *ppDSBuffer && pcDSBufferDesc->lpwfxFormat) {
            of::debug("Wrapping IDirectSoundBuffer into IDSBProxy");
            *ppDSBuffer = new IDSBProxy(*ppDSBuffer, pcDSBufferDesc);
        }
        return hr;
    }
    STDMETHOD(GetCaps)(LPDSCAPS pDSCaps) override { return pDev->GetCaps(pDSCaps); }
    STDMETHOD(DuplicateSoundBuffer)(LPDIRECTSOUNDBUFFER pDSBufferOriginal,
                                    LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate) override {
        // Seems to be unused anyway
        of::error("Not implemented: DuplicateSoundBuffer");
        ASS(false);
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
    // of::debug("DirectSoundCreate");
    if (conf::get().disable_audio)
        return DSERR_NODRIVER;
    HRESULT hr = DirectSoundCreateO(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds) {
        of::info("Wrapping IDirectSound into IDSProxy");
        *ds = new audio::IDSProxy(*ds);
    }
    return hr;
}

static MCIERROR mciSendCommandAH(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand,
                                 DWORD_PTR dwParam) {
    of::error("mciSendCommandA: MCI audio playback is not supported");
    return MCIERR_DRIVER;
}

static MCIERROR mciSendCommandWH(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand,
                                 DWORD_PTR dwParam) {
    of::error("mciSendCommandW: MCI audio playback is not supported");
    return MCIERR_DRIVER;
}

static BOOL WINAPI BeepH(DWORD dwFreq, DWORD dwDuration) {
    of::info("Beep (freq={}, duration={})", dwFreq, dwDuration);
    return FALSE;
}

bool audio::is_recording() { return capturing; }

void audio::reinit_capture() {
    capturing = conf::get().record_audio;
    last_uid = 0;
    last_time = 0;
    cap_time_offset = 0; // TODO: configure it
}

void audio::init() {
    auto& cfg = conf::get();
    if (!cfg.allow_audio_hook && cfg.record_audio) {
        of::warn(
            "Audio hook was disabled, but audio recording is enabled; enabling audio hook");
        cfg.disable_audio = false;
        cfg.allow_audio_hook = true;
    } else if (cfg.disable_audio && cfg.record_audio) {
        of::warn("Audio is disabled, but recording is enabled; enabling audio");
        cfg.disable_audio = false;
        cfg.allow_audio_hook = true;
    }
    if (!cfg.allow_audio_hook && !cfg.disable_audio)
        return;
    if (cfg.record_audio)
        of::warn("Audio recording is still in BETA");
    reinit_capture();
    if (capturing) {
        base_path = string(ofs::get_cwd()) + '\\' + cfg.project_name + "\\temp_audio";
        auto dir_ret = ofs::make_dir(base_path);
        ENSURE(dir_ret);
    }
    IAT_STR_ONLY("winmm.dll", mciSendCommand);
    IAT_ONLY("kernel32.dll", Beep);
    IAT_AUTO("dsound.dll", DirectSoundCreate);
    // Keep this because mmfs2.dll might use Ordinal 1 import instead of DirectSoundCreate
    auto hook_ret1 = hook::iat_hook_by_addr(mem::get_base("mmfs2.dll"), "dsound.dll",
                                            mem::get_addr("dsound.dll", "DirectSoundCreate"),
                                            DirectSoundCreateH, &DirectSoundCreateO);
    ENSURE(hook_ret1);
    of::info("Audio hooked");
}

void audio::flush() {
    if (!capturing)
        return;
    lock::CSLock lock(acs);
    auto& cfg = conf::get();
    for (IDSBProxy* c : cache)
        c->finalize_wav();
    capturing = false;
    of::debug("flush {}", history.size());
    if (history.empty())
        return;

    // Let's make a FFmpeg script with filter file to join all the WAVs
    ofs::File filters(base_path + "\\audio_filters.txt", 1);
    string mix = "";
    size_t count = 0;
    bool support_pan = cfg.support_audio_panning;

    for (const auto& c : history) {
        string finalLabel = "[f" + std::to_string(count) + "]";
        double totalDuration = static_cast<double>(c.endTime - c.startTime) / 1000.0;
        ASS(!c.events.empty());
        size_t numSegs = c.events.size();
        filters.write("amovie=" + audio_get_fn(c.hash, c.startTime, c.idx) +
                      ",asplit=" + std::to_string(numSegs));
        for (size_t e = 0; e < numSegs; e++) {
            filters.write("[b" + std::to_string(count) + "s" + std::to_string(e) + "]");
        }
        filters.writeln(";");
        string segmentLabels = "";
        for (size_t e = 0; e < c.events.size(); e++) {
            string branchIn = "[b" + std::to_string(count) + "s" + std::to_string(e) + "]";
            string branchOut = "[p" + std::to_string(count) + "s" + std::to_string(e) + "]";

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
    auto bat_path = base_path + "\\..\\audio_merge.bat";
    if (!ofs::file_exists(bat_path)) {
        // Write bat script only once, don't touch if user modified it
        ofs::File bat(bat_path, 1);
        bat.writeln("@echo off");
        bat.writeln("cd " + base_path);
        // Note: this "-/filter_complex" syntax is fine (for stupid AI)
        bat.writeln(
            "ffmpeg -y -/filter_complex audio_filters.txt -map \"[out]\" -ar 48000 ../audio.wav");
        bat.writeln("echo Waiting to delete wav cache...");
        bat.writeln("pause");
        bat.writeln("del audio_filters.txt");
        bat.writeln("del a*.wav");
        bat.writeln("cd ..");
    }
    history.clear();
    state::set_last_msg("Audio flushed, run audio_merge.bat script!");
}
