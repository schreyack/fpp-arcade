#include "FPPArcadeSound.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <thread>

#ifndef PLATFORM_OSX
#include <pulse/error.h>
#include <pulse/simple.h>
#define ARCADE_SOUND_PULSE
#endif

namespace ArcadeSound {

static constexpr int RATE = 44100;
static constexpr int CHUNK = 512;              // frames per write, about 12 ms
static constexpr int EDGE = RATE / 500;        // 2 ms fade at note edges, so notes don't click
static constexpr auto IDLE_CLOSE = std::chrono::seconds(10);

float hz(const std::string &note) {
    static const int SEMIS[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    if (note.empty() || note[0] < 'A' || note[0] > 'G') {
        return 0;
    }
    int semi = SEMIS[note[0] - 'A'];
    size_t i = 1;
    if (i < note.size() && note[i] == '#') {
        semi++;
        i++;
    } else if (i < note.size() && note[i] == 'b') {
        semi--;
        i++;
    }
    int octave = i < note.size() ? std::atoi(note.c_str() + i) : 4;
    int midi = 12 * (octave + 1) + semi;
    return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
}

std::vector<Note> tune(const std::string &notes, int bpm) {
    std::vector<Note> out;
    std::istringstream in(notes);
    std::string tok;
    float quarter = 60000.0f / bpm;
    while (in >> tok) {
        size_t colon = tok.find(':');
        std::string name = tok.substr(0, colon);
        std::string len = colon == std::string::npos ? "4" : tok.substr(colon + 1);
        bool dotted = !len.empty() && len.back() == '.';
        float ms = quarter * 4.0f / std::max(1, std::atoi(len.c_str()));
        if (dotted) {
            ms *= 1.5f;
        }
        float f = name == "-" ? 0 : hz(name);
        out.push_back(Note{ f, f, (int)ms });
    }
    return out;
}

namespace {

struct Voice {
    Sound sound;
    bool active = false;
    size_t note = 0;
    double pos = 0;      // samples into the current note
    double phase = 0;    // 0..1 through the current wave cycle
    float tempo = 1.0f;
    uint16_t lfsr = 1;
    float noise = 1.0f;

    float next() {
        while (active) {
            if (note >= sound.notes.size()) {
                if (sound.loop && !sound.notes.empty()) {
                    note = 0;
                } else {
                    active = false;
                    break;
                }
            }
            const Note &n = sound.notes[note];
            double len = std::max(1.0, n.ms * (double)RATE / 1000.0);
            if (pos >= len) {
                pos -= len;
                note++;
                continue;
            }
            float out = 0;
            if (n.from > 0) {
                float f = n.from + (n.to - n.from) * (float)(pos / len);
                float edge = (float)std::min(1.0, std::min(pos, len - pos) / EDGE);
                out = sample(f) * edge * sound.level;
            }
            pos += tempo;
            return out;
        }
        return 0;
    }

    float sample(float f) {
        phase += f / RATE;
        bool wrapped = phase >= 1.0;
        phase -= std::floor(phase);
        switch (sound.wave) {
        case Wave::Square:
            return (phase < 0.5 ? 1.0f : -1.0f) * 0.30f;
        case Wave::Pulse:
            return (phase < 0.25 ? 1.0f : -1.0f) * 0.30f;
        case Wave::Triangle:
            return (float)(4.0 * std::fabs(phase - 0.5) - 1.0) * 0.45f;
        case Wave::Noise:
            // An NES-style shift register, clocked at the note's pitch.
            if (wrapped) {
                uint16_t bit = (lfsr ^ (lfsr >> 1)) & 1;
                lfsr = (lfsr >> 1) | (bit << 14);
                noise = (lfsr & 1) ? 1.0f : -1.0f;
            }
            return noise * 0.25f;
        }
        return 0;
    }
};

std::mutex lock;
std::condition_variable wake;
std::array<Voice, NUM_CHANNELS> voices;
bool enabled = true;
float master = 1.0f;
std::string sinkName;
bool quitting = false;
std::thread worker;

bool anyActive() {
    for (auto &v : voices) {
        if (v.active) {
            return true;
        }
    }
    return false;
}

// Called with the lock held.
void render(std::array<int16_t, CHUNK * 2> &buf) {
    for (int i = 0; i < CHUNK; i++) {
        float s = 0;
        for (auto &v : voices) {
            s += v.next();
        }
        s = std::max(-1.0f, std::min(1.0f, s * master));
        int16_t v = (int16_t)(s * 32767);
        buf[i * 2] = v;
        buf[i * 2 + 1] = v;
    }
}

#ifdef ARCADE_SOUND_PULSE
// The stream is opened when a sound starts and closed after IDLE_CLOSE of
// quiet, so the sink is left alone (and free to suspend) between games.
void run() {
    pa_simple *stream = nullptr;
    auto lastSound = std::chrono::steady_clock::now();
    bool reportedFailure = false;
    std::array<int16_t, CHUNK * 2> buf;
    while (true) {
        std::unique_lock<std::mutex> l(lock);
        if (quitting) {
            break;
        }
        if (stream == nullptr) {
            if (!anyActive()) {
                wake.wait(l, [] { return quitting || anyActive(); });
                continue;
            }
            std::string sink = sinkName;
            l.unlock();

            pa_sample_spec spec;
            spec.format = PA_SAMPLE_S16LE;
            spec.rate = RATE;
            spec.channels = 2;
            // A short buffer, so a paddle hit is heard as it happens.
            pa_buffer_attr attr;
            attr.maxlength = (uint32_t)-1;
            attr.tlength = CHUNK * 4 * 4;
            attr.prebuf = (uint32_t)-1;
            attr.minreq = (uint32_t)-1;
            attr.fragsize = (uint32_t)-1;
            int err = 0;
            stream = pa_simple_new(nullptr, "FPP Arcade", PA_STREAM_PLAYBACK,
                                   sink.empty() ? nullptr : sink.c_str(),
                                   "Game sounds", &spec, nullptr, &attr, &err);
            lastSound = std::chrono::steady_clock::now();
            if (stream == nullptr) {
                if (!reportedFailure) {
                    fprintf(stderr, "FPP Arcade: could not open game sound output '%s': %s\n",
                            sink.c_str(), pa_strerror(err));
                    reportedFailure = true;
                }
                // Drop what was asked for rather than play it late, and try
                // again with the next sound.
                l.lock();
                for (auto &v : voices) {
                    v.active = false;
                }
                wake.wait_for(l, std::chrono::seconds(5), [] { return quitting; });
                continue;
            }
            reportedFailure = false;
            continue;
        }
        render(buf);
        bool sounding = anyActive();
        l.unlock();

        auto now = std::chrono::steady_clock::now();
        if (sounding) {
            lastSound = now;
        } else if (now - lastSound > IDLE_CLOSE) {
            pa_simple_drain(stream, nullptr);
            pa_simple_free(stream);
            stream = nullptr;
            continue;
        }
        // Blocks until the server has room, which paces this loop.
        if (pa_simple_write(stream, buf.data(), sizeof(buf), nullptr) < 0) {
            pa_simple_free(stream);
            stream = nullptr;
        }
    }
    if (stream) {
        pa_simple_free(stream);
    }
}
#endif

// Called with the lock held.
void ensureWorker() {
#ifdef ARCADE_SOUND_PULSE
    if (!worker.joinable() && !quitting) {
        worker = std::thread(run);
    }
#endif
}
}

void play(int channel, const Sound &s) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return;
    }
    std::lock_guard<std::mutex> l(lock);
    if (!enabled || master <= 0 || quitting) {
        return;
    }
    Voice &v = voices[channel];
    v.sound = s;
    v.note = 0;
    v.pos = 0;
    v.tempo = 1.0f;
    v.active = !s.notes.empty();
    ensureWorker();
    wake.notify_all();
}

void play(int channel, Wave w, const std::vector<Note> &notes, float level) {
    Sound s;
    s.wave = w;
    s.notes = notes;
    s.level = level;
    play(channel, s);
}

void stop(int channel) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return;
    }
    std::lock_guard<std::mutex> l(lock);
    voices[channel].active = false;
}

void stopAll() {
    std::lock_guard<std::mutex> l(lock);
    for (auto &v : voices) {
        v.active = false;
    }
}

void setTempo(int channel, float speed) {
    if (channel < 0 || channel >= NUM_CHANNELS || speed <= 0) {
        return;
    }
    std::lock_guard<std::mutex> l(lock);
    voices[channel].tempo = speed;
}

bool isPlaying(int channel) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return false;
    }
    std::lock_guard<std::mutex> l(lock);
    return voices[channel].active;
}

void configure(bool on, int volume, const std::string &sink) {
    std::lock_guard<std::mutex> l(lock);
    enabled = on;
    master = std::max(0, std::min(100, volume)) / 100.0f;
    sinkName = sink;
    if (!enabled || master <= 0) {
        for (auto &v : voices) {
            v.active = false;
        }
    }
}

void shutdown() {
    {
        std::lock_guard<std::mutex> l(lock);
        quitting = true;
        for (auto &v : voices) {
            v.active = false;
        }
        wake.notify_all();
    }
    if (worker.joinable()) {
        worker.join();
    }
}
}
