#ifndef __FPPARCADE_SOUND__
#define __FPPARCADE_SOUND__

#include <string>
#include <vector>

// Retro game sounds, made in code (square, triangle and noise waves, like the
// sound chips of the time) and played on FPP's own PipeWire sink alongside
// whatever the show is playing. Nothing here touches the system volume: the
// volume setting only scales these sounds.
//
// Everything is safe to call from any thread; the mixing happens on a thread
// of its own, which only runs while something is sounding.
namespace ArcadeSound {

enum class Wave { Square, Pulse, Triangle, Noise };

// One step of a sound: a tone that slides from 'from' Hz to 'to' Hz over
// 'ms' milliseconds. A 'from' of 0 is a rest.
struct Note {
    float from;
    float to;
    int ms;
};
inline Note tone(float hz, int ms) { return Note{ hz, hz, ms }; }
inline Note slide(float from, float to, int ms) { return Note{ from, to, ms }; }
inline Note rest(int ms) { return Note{ 0, 0, ms }; }

// Note names to Hz: "C4", "F#5", "Bb3".
float hz(const std::string &note);
// A tune from "E5:4 B4:8 C5:8 -:4" - note, then its length (4 a quarter,
// 8 an eighth, 16 a sixteenth, a trailing '.' dotted), '-' a rest.
std::vector<Note> tune(const std::string &notes, int bpm);

struct Sound {
    Wave wave = Wave::Square;
    std::vector<Note> notes;
    float level = 1.0f;
    bool loop = false;
};

// Channels: a new sound on a channel replaces what was on it. MUSIC and BASS
// are for the looping theme, the others for effects.
enum Channel { MUSIC = 0, BASS = 1, FX1 = 2, FX2 = 3, FX3 = 4, NUM_CHANNELS = 5 };

void play(int channel, const Sound &s);
void play(int channel, Wave w, const std::vector<Note> &notes, float level = 1.0f);
void stop(int channel);
void stopAll();
// How fast a channel plays its notes: 1 is as written, 2 twice as fast.
void setTempo(int channel, float speed);
bool isPlaying(int channel);

// From plugin.fpp-arcade.json: sounds on or off, and their level, 0-100;
// sink is FPP's PipeWire sink for media ("" for the default).
void configure(bool enabled, int volume, const std::string &sink);
// Joins the mixing thread; called as the plugin unloads.
void shutdown();
}

#endif
