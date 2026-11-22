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

// Hold: the shift row's one entry, Shift + encoder 1 (definition.json "shift").
inline bool hold_on(const Controls& c) { return opxy::player::choice(c.shift_value[0], 2) == 1; }

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
    // The page's clock: player steps, counted whether or not a key is held.
    u32 ticks;
    u32 sounded;        // when the shown line's last note sounded, for the halo
    u32 released;       // when the last key was let go, for the marble's second
    u8 active, sounding, off_mask, on_mask;
    u8 draining, drain_calls, event_pending;
    u8 fading;          // the last key's marble, still shown for a second
    u8 fade_semitone;   // where it was
    u8 held;            // keys under a finger (or a sequenced note); active keys not held are latched by Hold
    u8 stop_count;      // latched roots waiting to be silenced
    u8 stopping[kKeys];
    u8 hold;            // Hold as last read on the sequencer, for release(), which gets no Controls
    u8 pad;
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

// The held key the page follows: the oldest, as Halcyon Wander's screen followed the first.
inline u32 shown_key(const State& s) {
    u32 shown = kKeys;
    for (u32 i = 0; i < kKeys; ++i)
        if ((s.active >> i & 1) && (shown == kKeys || s.keys[i].order < s.keys[shown].order)) shown = i;
    return shown;
}

inline void queue_note(State& s, u32 i, u32 semitone, u32 gain, const Params& p) {
    Key& k = s.keys[i];
    const u8 bit = static_cast<u8>(1u << i);
    k.next_pitch = static_cast<u8>(pitch_of(k.root, semitone));
    k.next_velocity = static_cast<u8>(velocity_of(k.velocity, gain));
    k.semitone = static_cast<u8>(semitone);
    k.shown = 1;
    k.gate = gate_steps(p);
    if (shown_key(s) == i) s.sounded = s.ticks;
    s.on_mask |= bit;
    s.off_mask &= static_cast<u8>(~bit);   // its Replace already ends the old note
}

// Ends a latched key's line: its slot is free at once, and the next drain sends its
// root an empty Replace, which silences it and clears stock's record.
inline void stop_key(State& s, u32 i) {
    const u8 keep = static_cast<u8>(~(1u << i));
    bool queued = false;
    for (u32 n = 0; n < s.stop_count; ++n) queued = queued || s.stopping[n] == s.keys[i].root;
    if (!queued && s.stop_count < kKeys) s.stopping[s.stop_count++] = s.keys[i].root;
    s.active &= keep;
    s.sounding &= keep;
    s.off_mask &= keep;
    s.on_mask &= keep;
    s.held &= keep;
    if (!s.active) {                      // the marble stays a second, as Halcyon Wander's did
        s.fading = s.keys[i].shown;
        s.fade_semitone = s.keys[i].semitone;
        s.released = s.ticks;
    }
}

// One player step for every held key: gates close, due steps land.
inline void clock_step(State& s, const Controls& c) {
    ++s.ticks;
    if (!s.active) return;
    // Hold turned off: what no finger holds stops.
    if (!hold_on(c) && (s.active & ~s.held))
        for (u32 i = 0; i < kKeys; ++i)
            if ((s.active >> i & 1) && !(s.held >> i & 1)) stop_key(s, i);
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
    s.hold = hold_on(c);
    // With Hold on, the first key pressed after letting go of every key replaces the
    // latched lines, as an arpeggiator's latch does.
    if (hold_on(c) && !s.held && s.active)
        for (u32 n = 0; n < kKeys; ++n)
            if ((s.active >> n & 1) && s.keys[n].root != note) stop_key(s, n);
    // A root pressed again before its stop was sent keeps its new line.
    for (u32 n = 0; n < s.stop_count; ++n)
        if (s.stopping[n] == note) s.stopping[n] = s.stopping[--s.stop_count];
    u32 i = find_key(s, note);
    if (i == kKeys) {
        // Stock holds eight roots, counting latched ones whose stop is still to be sent:
        // a ninth would be dropped, so it is a silent key.
        u32 records = s.stop_count;
        for (u32 n = 0; n < kKeys; ++n) records += s.active >> n & 1;
        if (records >= kKeys) return true;
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
    if (!s.active) {
        s.sounded = s.ticks + 1;          // the first key, whose note sounds on the next step
        s.fading = 0;
    }
    s.active |= bit;
    s.held |= bit;
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
    s.held &= keep;
    // With Hold on, the line plays on: stock keeps the root's record. Hold is as
    // last read, at most a player step ago; if it has since gone off, the next
    // step stops this key.
    if (s.hold) return true;
    s.active &= keep;
    s.sounding &= keep;
    s.off_mask &= keep;
    s.on_mask &= keep;
    if (!s.active) {                      // the marble stays a second, as Halcyon Wander's did
        s.fading = s.keys[i].shown;
        s.fade_semitone = s.keys[i].semitone;
        s.released = s.ticks;
    }
    return false;                         // stock releases this root's notes
}

bool update(State& s, const Controls& c, Update& change) {
    s.hold = hold_on(c);
    if (!s.draining) {
        s.draining = 1;
        s.drain_calls = 0;
        if (s.event_pending) s.event_pending = 0;
        else clock_step(s, c);
    }
    if (!s.stop_count && !s.off_mask && !s.on_mask) {
        s.draining = 0;
        return false;
    }
    change.mode = opxy::player::Replace;
    change.plan.count = 1;
    if (s.stop_count) {
        change.root = s.stopping[--s.stop_count];
        change.plan.count = 0;            // an empty Replace ends the root
    } else if (s.off_mask) {
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
    const u32 rng = s.rng, ticks = s.ticks;
    __builtin_memset(&s, 0, sizeof s);
    s.rng = rng;
    s.ticks = ticks;
}

}  // namespace wander_player
