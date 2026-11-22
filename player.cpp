// Wander Player: Halcyon Wander's line as a note player. Hold a key and the
// track's own instrument plays a line that wanders in a scale of its own, from
// the key you hold, after Mutable Instruments' Marbles. By Colors In Space.
//
// Rate sets how often the line steps (on the project tempo), Chance how many of
// those steps sound, Scale which notes it may land on, Gate how long each note
// holds. Deja vu and Length set how much it repeats itself, Spread how far it
// roams, Jitter loosens its timing. Every held key wanders on its own line.
//
// How it plays through the SDK's player interface (opxy/player.hpp):
// - each held key owns one chord record, which holds the note its line is on;
// - a step that sounds replaces that record's tone with the new note;
// - update() is drained before every player step and after every press and
//   release; press and release mark their drain, so the others are the clock;
// - a key that is silent between notes keeps its record with a far-future
//   "parked" tone, because an empty Replace would end the root for good.
// Callbacks are bounded: no loops longer than the eight keys or sixteen steps,
// no division, no allocation.
#include <opxy/player.hpp>

namespace wander_player {
using opxy::i32;
using opxy::u8;
using opxy::u16;
using opxy::u32;
using opxy::player::Controls;
using opxy::player::Display;
using opxy::player::Plan;
using opxy::player::Tone;
using opxy::player::Update;

const u32 kKeys = opxy::player::max_tones;  // the SDK holds at most eight roots
const u32 kLoop = 16;                       // Length's longest phrase
const u32 kDrainLimit = 8;                  // the SDK's updates per drain
const u16 kParkedDelay = 65535;             // player steps before a parked tone would sound
const u32 kRearmInterval = 32768;           // parked tones are renewed well before that

// --- Controls ----------------------------------------------------------------------
// Rate: each choice's length in player steps (240 a quarter note).
const u32 kRates = 10;
const u16 kRateSteps[kRates] = {1920, 960, 480, 240, 160, 120, 80, 60, 40, 30};
const char kRateNames[kRates][6] = {"2/1", "1/1", "1/2", "1/4", "1/4t", "1/8", "1/8t", "1/16", "1/16t", "1/32"};
const u32 kChances = 21;                     // 0..100 % in fives
const u32 kGates = 11;                       // 10..100 % in tens, then tie
const u32 kTie = kGates - 1;
const u32 kSpreads = 4;                      // 1..4 octaves

// The scales, as in Halcyon Wander.
const u32 kScales = 8;
const u8 kScaleSize[kScales] = {7, 7, 7, 7, 7, 7, 5, 5};
const u8 kScaleNotes[kScales][7] = {
    {0, 2, 3, 5, 7, 8, 10},   // minor
    {0, 2, 3, 5, 7, 9, 10},   // dorian
    {0, 2, 3, 5, 7, 8, 11},   // harmonic minor
    {0, 1, 3, 5, 7, 8, 10},   // phrygian
    {0, 2, 4, 6, 7, 9, 11},   // lydian
    {0, 2, 4, 5, 7, 9, 11},   // major
    {0, 2, 4, 7, 9, 0, 0},    // major pentatonic (five notes)
    {0, 3, 5, 7, 10, 0, 0},   // minor pentatonic (five notes)
};
const char kScaleShort[kScales][6] = {"min", "dor", "harm", "phry", "lyd", "maj", "Mpen", "mpen"};
const char kScaleNames[kScales][17] = {"minor", "dorian", "harmonic minor", "phrygian",
                                       "lydian", "major", "major pentatonic", "minor pentatonic"};

struct Params {
    u32 interval;   // player steps per line step
    u32 chance;     // Q15 threshold; 32768 always sounds
    u32 scale;
    u32 gate;       // choice, kTie holds until the next note
    u32 dejavu;     // Q15
    u32 length;     // 1..16
    u32 octaves;    // 1..4
    u32 jitter;     // Q15
};

inline u32 amount(i32 v) { return v <= 0 ? 0u : static_cast<u32>(v) >> 16; }   // 0..32767

inline Params read_params(const Controls& c) {
    using opxy::player::choice;
    Params p;
    p.interval = kRateSteps[choice(c.value[0], kRates)];
    p.chance = choice(c.value[1], kChances) * 5u * 335544u >> 10;   // percent to Q15, 100 % = 32768
    p.scale = choice(c.value[2], kScales);
    p.gate = choice(c.value[3], kGates);
    p.dejavu = amount(c.value[4]);
    p.length = 1u + choice(c.value[5], kLoop);
    p.octaves = 1u + choice(c.value[6], kSpreads);
    p.jitter = amount(c.value[7]);
    return p;
}

// --- The line (Halcyon Wander's journey.h, one per held key) ------------------------
inline u32 hash(u32 x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

struct Key {
    u32 loop[kLoop];   // the phrase Deja vu repeats
    u32 seed;          // this press's line
    u32 steps;         // line steps taken
    u32 slot;          // where in the phrase the next step goes
    u32 clock;         // the key's player steps so far, 0 the step its own note sounds on
    u32 next;          // when the next line step is due on the beat
    u32 due;           // when it lands, Jitter included
    u32 gate;          // player steps left of the sounding note; 0 holds it
    u32 order;         // press order, oldest shown on screen
    u8 root, velocity;
    u8 next_pitch, next_velocity;
    u8 semitone;       // the last sounded note, above the root
    u8 shown;          // a note has sounded since the press
    u8 pad[2];
};

struct State {
    Key keys[kKeys];
    u32 rng, order_counter, rearm;
    u8 active, sounding, off_mask, on_mask;
    u8 draining, drain_calls, event_pending, pad;
};

struct Step { bool sounds; u32 degree; u32 gain; };   // degree 0 is the held key; gain Q15

// The next step. The first is always the held key.
inline Step next_step(Key& k, const Params& p) {
    const u32 n = k.steps++;
    if (k.slot >= p.length) k.slot = 0;
    const u32 slot = k.slot;
    k.slot = slot + 1 >= p.length ? 0 : slot + 1;
    Step s = {true, 0, 32767};
    const bool again = n >= p.length &&
                       (p.dejavu >= 32767 || (hash(k.seed ^ (n * 0x9e3779b9u) ^ 0xd5u) >> 17) < p.dejavu);
    u32 value;
    if (again) value = k.loop[slot];
    else { value = hash(k.seed + n * 0x85ebca6bu + 1u); k.loop[slot] = value; }
    if (n == 0) { k.loop[0] = 0; return s; }   // the held key, sounding
    s.sounds = (value & 32767u) < p.chance;
    s.degree = ((value >> 16) * (kScaleSize[p.scale] * p.octaves)) >> 16;   // anywhere in the range
    s.gain = 32767u - (((value >> 8) & 255u) * 32u);                        // up to a quarter quieter
    return s;
}

// Jitter: when the next step lands, given when it is due on the beat: up to a
// quarter step early or late. The held key's first note is always on time.
inline u32 landing(const Key& k, const Params& p) {
    if (!p.jitter || !k.steps) return k.next;
    const u32 r = hash(0x2c1b3c6du ^ (k.steps * 0x9e3779b9u) ^ (k.seed * 0x85ebca6bu)) >> 17;   // 0..32767
    const u32 half = p.interval >> 1;
    const u32 swing = (half * p.jitter) >> 15;                       // the whole window
    return k.next - (swing >> 1) + ((half * ((r * p.jitter) >> 15)) >> 15);
}

inline u32 semitone_of(u32 degree, u32 scale) {
    const u32 size = kScaleSize[scale];
    u32 octave = 0;
    while (degree >= size) { degree -= size; ++octave; }   // at most four rounds
    return kScaleNotes[scale][degree] + 12u * octave;
}

inline u32 pitch_of(u32 root, u32 semitone) {
    u32 pitch = root + semitone;
    while (pitch > 127) pitch -= 12;
    return pitch;
}

inline u32 velocity_of(u32 velocity, u32 gain) {
    const u32 v = (velocity * gain) >> 15;
    return v < 1 ? 1 : v > 127 ? 127 : v;
}

inline u32 gate_steps(const Params& p) {
    if (p.gate >= kTie) return 0;
    const u32 steps = (p.interval * (p.gate + 1u) * 6554u) >> 16;   // tenths of a step
    return steps ? steps : 1;
}

inline u32 next_random(State& s) {
    u32 x = s.rng ? s.rng : 0x9e3779b9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s.rng = x;
    return x;
}

inline u32 find_key(const State& s, u32 note) {
    for (u32 i = 0; i < kKeys; ++i)
        if ((s.active >> i & 1) && s.keys[i].root == note) return i;
    return kKeys;
}

inline u32 lowest_bit(u32 mask) {
    u32 i = 0;
    while (!(mask >> i & 1)) ++i;
    return i;
}

inline void park(Tone& tone, u32 root) {
    tone.pitch = static_cast<u8>(root);
    tone.velocity = 1;
    tone.delay = kParkedDelay;
}

inline void queue_note(State& s, u32 i, u32 semitone, u32 gain, const Params& p) {
    Key& k = s.keys[i];
    const u8 bit = static_cast<u8>(1u << i);
    k.next_pitch = static_cast<u8>(pitch_of(k.root, semitone));
    k.next_velocity = static_cast<u8>(velocity_of(k.velocity, gain));
    k.semitone = static_cast<u8>(semitone);
    k.shown = 1;
    k.gate = gate_steps(p);
    s.on_mask |= bit;
    s.off_mask &= static_cast<u8>(~bit);   // its Replace already ends the old note
}

// One player step for every held key: gates close, due steps land.
inline void clock_step(State& s, const Controls& c) {
    if (!s.active) return;
    const Params p = read_params(c);
    if (++s.rearm >= kRearmInterval) {
        s.rearm = 0;
        s.off_mask |= static_cast<u8>(s.active & ~s.sounding & ~s.on_mask);
    }
    for (u32 i = 0; i < kKeys; ++i) {
        if (!(s.active >> i & 1)) continue;
        Key& k = s.keys[i];
        // The press's own note sounds on the key's step 0; its gate counts from there.
        const u32 now = k.clock++;
        if (now && k.gate && --k.gate == 0 && (s.sounding >> i & 1)) s.off_mask |= static_cast<u8>(1u << i);
        // The shortest Rate is 30 player steps, so at most one line step falls on this one.
        if (now < k.due) continue;
        const Step step = next_step(k, p);
        k.next += p.interval;
        k.due = landing(k, p);
        if (step.sounds) queue_note(s, i, semitone_of(step.degree, p.scale), step.gain, p);
    }
}

bool press(State& s, const Controls& c, u32 note, u32 velocity, Plan& plan) {
    plan.count = 0;
    if (note > 127) return false;
    s.event_pending = 1;
    u32 i = find_key(s, note);
    if (i == kKeys) {
        if (s.active == 0xff) return true;   // the SDK cannot hold a ninth root: a silent key
        i = lowest_bit(static_cast<u8>(~s.active));
    }
    const u8 bit = static_cast<u8>(1u << i);
    const Params p = read_params(c);
    Key& k = s.keys[i];
    k.root = static_cast<u8>(note);
    k.velocity = static_cast<u8>(velocity < 1 ? 1 : velocity > 127 ? 127 : velocity);
    k.order = ++s.order_counter;
    k.seed = next_random(s) ^ (note << 8) ^ k.velocity;   // a new line for every press
    k.steps = k.slot = k.clock = 0;
    for (u32 n = 0; n < kLoop; ++n) k.loop[n] = 0;
    next_step(k, p);                      // the first step: the held key itself
    k.next = p.interval;
    k.due = landing(k, p);
    k.semitone = 0;
    k.shown = 1;
    k.gate = gate_steps(p);
    s.active |= bit;
    s.sounding |= bit;                    // the plan sounds it on the next step
    s.off_mask &= static_cast<u8>(~bit);
    s.on_mask &= static_cast<u8>(~bit);
    plan.count = 1;
    plan.tones[0].pitch = static_cast<u8>(note);
    plan.tones[0].velocity = k.velocity;
    plan.tones[0].delay = 0;
    return true;
}

bool release(State& s, u32 note) {
    s.event_pending = 1;
    const u32 i = find_key(s, note);
    if (i == kKeys) return true;          // a key press() could not hold
    const u8 keep = static_cast<u8>(~(1u << i));
    s.active &= keep;
    s.sounding &= keep;
    s.off_mask &= keep;
    s.on_mask &= keep;
    return false;                         // stock releases this root's notes
}

bool update(State& s, const Controls& c, Update& change) {
    if (!s.draining) {
        s.draining = 1;
        s.drain_calls = 0;
        if (s.event_pending) s.event_pending = 0;
        else clock_step(s, c);
    }
    if (!s.off_mask && !s.on_mask) {
        s.draining = 0;
        return false;
    }
    change.mode = opxy::player::Replace;
    change.plan.count = 1;
    if (s.off_mask) {
        const u32 i = lowest_bit(s.off_mask);
        const u8 bit = static_cast<u8>(1u << i);
        s.off_mask &= static_cast<u8>(~bit);
        s.sounding &= static_cast<u8>(~bit);
        change.root = s.keys[i].root;
        park(change.plan.tones[0], change.root);
    } else {
        const u32 i = lowest_bit(s.on_mask);
        const u8 bit = static_cast<u8>(1u << i);
        s.on_mask &= static_cast<u8>(~bit);
        s.sounding |= bit;
        change.root = s.keys[i].root;
        change.plan.tones[0].pitch = s.keys[i].next_pitch;
        change.plan.tones[0].velocity = s.keys[i].next_velocity;
        change.plan.tones[0].delay = 0;
    }
    if (++s.drain_calls >= kDrainLimit) s.draining = 0;   // the SDK stops calling here
    return true;
}

// Stock reset and all-notes-off: stock has already silenced every record.
void reset(State& s) {
    const u32 rng = s.rng;
    __builtin_memset(&s, 0, sizeof s);
    s.rng = rng;
}

// --- The page ------------------------------------------------------------------------
inline void append(char* out, u32 size, u32& at, const char* text) {
    while (*text && at + 1 < size) out[at++] = *text++;
    out[at] = 0;
}

// 0..999 without division.
inline void append_number(char* out, u32 size, u32& at, u32 value) {
    char digits[4] = {0, 0, 0, 0};
    u32 count = 0, hundreds = 0, tens = 0;
    while (value >= 100) { value -= 100; ++hundreds; }
    while (value >= 10) { value -= 10; ++tens; }
    if (hundreds) digits[count++] = static_cast<char>('0' + hundreds);
    if (hundreds || tens) digits[count++] = static_cast<char>('0' + tens);
    digits[count] = static_cast<char>('0' + value);
    append(out, size, at, digits);
}

inline void show(char (&out)[8], const char* text) {
    u32 at = 0;
    append(out, sizeof out, at, text);
}

inline void show_number(char (&out)[8], u32 value, const char* suffix) {
    u32 at = 0;
    append_number(out, sizeof out, at, value);
    append(out, sizeof out, at, suffix);
}

inline u32 percent(u32 q15) { return (q15 * 100u + 16384u) >> 15; }

// The page is an orrery, after Halcyon Wander's screen: twelve places around the
// held key, the root at the top. Dots 0..11 light the scale's notes; dots 12..23
// are the marble, landing on each note as it sounds. The line in the middle names
// the note, or the scale while nothing is held; the keyboard below shows the
// notes in every octave as they sound.
void display(const State& s, const Controls& c, Display& d) {
    using opxy::player::choice;
    const Params p = read_params(c);
    show(d.values[0], kRateNames[choice(c.value[0], kRates)]);
    show_number(d.values[1], choice(c.value[1], kChances) * 5u, "%");
    show(d.values[2], kScaleShort[p.scale]);
    if (p.gate >= kTie) show(d.values[3], "tie");
    else show_number(d.values[3], (p.gate + 1u) * 10u, "%");
    show_number(d.values[4], percent(p.dejavu), "%");
    show_number(d.values[5], p.length, "");
    show_number(d.values[6], p.octaves, " oct");
    show_number(d.values[7], percent(p.jitter), "%");

    for (u32 n = 0; n < kScaleSize[p.scale]; ++n) d.lit |= 1u << kScaleNotes[p.scale][n];

    // The oldest held key's line, as Halcyon Wander's screen followed the first key.
    u32 shown = kKeys;
    for (u32 i = 0; i < kKeys; ++i)
        if ((s.active >> i & 1) && (shown == kKeys || s.keys[i].order < s.keys[shown].order)) shown = i;
    u32 at = 0;
    if (shown == kKeys || !s.keys[shown].shown) {
        append(d.text, sizeof d.text, at, kScaleNames[p.scale]);
        return;
    }
    static const char notes[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const Key& k = s.keys[shown];
    u32 place = k.semitone;
    while (place >= 12) place -= 12;
    d.lit |= 1u << (12 + place);
    u32 pitch = pitch_of(k.root, k.semitone), octave = 0;
    while (pitch >= 12) { pitch -= 12; ++octave; }
    append(d.text, sizeof d.text, at, notes[pitch]);
    if (octave) append_number(d.text, sizeof d.text, at, octave - 1);
    else append(d.text, sizeof d.text, at, "-1");
}
}  // namespace wander_player
