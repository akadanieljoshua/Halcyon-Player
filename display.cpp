#include "display.h"
#include "panel.h"

// Halcyon's own page (SDK 1.22): Halcyon Wander's orrery, by Colors In
// Space, drawn from the player's State. The held key is the sun; each ring is an
// octave of Spread; the scale's notes light up at their clock positions. A marble
// lands on each note as it sounds, its last moves trailing behind, and sets off
// for the next one 0.15 s before it is due. Compiled after player.cpp, whose
// State, Params and line it reads.
namespace halcyon_player {
namespace {
using namespace opxy::draw;

const i32 kSecond = unit;               // Q10 seconds
const i32 kLead = unit * 15 / 100;      // the marble sets off 0.15 s before its note
const i32 kSwell = unit / 5;            // the halo swells for 0.2 s

void note_place(u32 semitone, u32 octaves, i32& x, i32& y) {
    u32 ring = 0, place = semitone;
    while (place >= 12) { place -= 12; ++ring; }
    if (ring >= octaves) ring = octaves - 1u;   // a note from a wider Spread sits on the outer ring
    x = panel::place_x[ring][place];
    y = panel::place_y[ring][place];
}

// 0..999 as text.
void number(char* out, u32 capacity, u32 value, const char* suffix) {
    char digits[8] = {};
    u32 n = 0, hundreds = 0, tens = 0;
    while (value >= 100) { value -= 100; ++hundreds; }
    while (value >= 10) { value -= 10; ++tens; }
    if (hundreds) digits[n++] = static_cast<char>('0' + hundreds);
    if (hundreds || tens) digits[n++] = static_cast<char>('0' + tens);
    digits[n++] = static_cast<char>('0' + value);
    for (u32 i = 0; suffix[i] && n + 1 < sizeof digits; ++i) digits[n++] = suffix[i];
    copy(out, capacity, digits);
}

// Player steps in a Q10 time at the current tempo (120 BPM when unknown): 240 a beat.
i32 steps_in(const opxy::player::Controls& c, i32 q10) {
    const i32 bpm_x10 = c.bpm_x10 ? static_cast<i32>(c.bpm_x10) : 1200;
    return (bpm_x10 * 2 / 5) * q10 >> 10;   // bpm_x10 * 0.4 steps a second
}
}

inline void display_reset(DisplayState& state) {
    // A new visit to the page: the header starts over; a playing line carries on.
    state.header.reset();
    state.order = 0;
    state.trail_count = 0;
    state.since = kSwell;
    state.seen = kSecond;
}

inline void display(DisplayState& state, const State& s, const opxy::player::Controls& c, const Input& input, Frame& frame) {
    const Params p = read_params(c);

    // The header: Rate, Chance, Scale, Gate; page 2 Deja vu, Length, Spread, Jitter.
    static const char* const titles[8] = {"Rate", "Chance", "Scale", "Gate", "Deja vu", "Length", "Spread", "Jitter"};
    char values[8][header_capacity] = {};
    copy(values[0], header_capacity, kRateNames[opxy::player::choice(c.value[0], kRates)]);
    number(values[1], header_capacity, opxy::player::choice(c.value[1], kChances) * 5u, "");
    copy(values[2], header_capacity, kScaleShort[p.scale]);
    if (p.gate >= kTie) copy(values[3], header_capacity, "tie");
    else number(values[3], header_capacity, (p.gate + 1u) * 10u, "");
    header_amount(values[4], header_capacity, c.value[4]);
    number(values[5], header_capacity, p.length, "");
    number(values[6], header_capacity, p.octaves, " oct");
    header_amount(values[7], header_capacity, c.value[7]);
    page_header(state.header, input, frame, titles, values, panel::bar0);

    // The line the orrery follows: the oldest key's, as Halcyon Wander's followed the
    // first. A note it has not seen joins the trail; another key starts a new trail.
    const i32 dt = clamp(input.elapsed, 0, 1024);
    const u32 key = shown_key(s);
    const bool playing = key != kKeys;
    if (playing) {
        const Key& k = s.keys[key];
        if (k.order != state.order) {
            state.order = k.order;
            state.trail_count = 0;
            state.sounded = s.sounded - 1u;
        }
        if (s.sounded != state.sounded && k.shown) {
            state.sounded = s.sounded;
            for (u32 n = 4; n > 0; --n) state.trail[n] = state.trail[n - 1];
            state.trail[0] = k.semitone;
            if (state.trail_count < 5) ++state.trail_count;
            state.since = 0;
        } else if (state.since < kSwell) {
            state.since += dt;
        }
        state.seen = 0;
    } else {
        state.order = 0;
        if (state.since < kSwell) state.since += dt;
        if (state.seen < kSecond) state.seen += dt;
        if (!s.fading) state.seen = kSecond;   // nothing sounded: nothing to fade
    }
    const bool shown = playing || state.seen < kSecond;   // fades for a second after the last key stops

    // Rings for the octaves in reach, and the scale's places on them.
    for (u32 ring = 0; ring < 4; ++ring) {
        if (ring < p.octaves) place(frame, panel::ring0 + ring, panel::centre_x * unit, panel::centre_y * unit, unit, panel::shade_ring, stroke);
        else hide(frame, panel::ring0 + ring);
        for (u32 n = 0; n < 12; ++n) {
            const u32 node = panel::dot0_0 + ring * 12 + n;
            if (ring >= p.octaves) { hide(frame, node); continue; }
            bool lit = false;
            for (u32 k = 0; k < kScaleSize[p.scale]; ++k) lit = lit || kScaleNotes[p.scale][k] == n;
            place(frame, node, panel::place_x[ring][n], panel::place_y[ring][n], lit ? unit : unit / 2,
                  lit ? panel::shade_lit : panel::shade_dim, stroke);
        }
    }

    // The marble and its trail.
    for (u32 k = 0; k < 5; ++k) {
        const u32 node = k == 0 ? panel::marble : panel::trail0 + (k - 1);
        if (!shown || k >= state.trail_count) { hide(frame, node); continue; }
        i32 x, y;
        note_place(state.trail[k], p.octaves, x, y);
        // The marble sets off 0.15 s early and lands on the next note as it sounds:
        // the line is known ahead, so it peeks at the coming step.
        if (k == 0 && playing) {
            Key ahead = s.keys[key];
            const Key& now = s.keys[key];
            const Step coming = next_step(ahead, p);
            const i32 lead = steps_in(c, kLead);
            const i32 due = static_cast<i32>(now.due - now.clock);
            if (coming.sounds && lead > 0 && due >= 0 && due < lead) {
                i32 tx, ty;
                note_place(semitone_of(coming.degree, p.scale), p.octaves, tx, ty);
                const i32 t = 256 - due * 256 / lead;   // 0..256
                x = x + ((tx - x) / 256) * t;
                y = y + ((ty - y) / 256) * t;
            }
        }
        const u32 shade = k == 0 ? panel::shade_grey0 : panel::shade_grey0 + k;
        place(frame, node, x, y, unit, shade > panel::shade_grey4 ? panel::shade_grey4 : shade, fill_and_stroke);
    }
    // The sun's halo swells for a moment as each note sounds.
    const i32 swell = state.since < kSwell ? (kSwell - state.since) * 600 / kSwell : 0;
    place(frame, panel::halo, panel::centre_x * unit, panel::centre_y * unit, unit + swell, panel::shade_grey2, stroke);
    // The sun brightens and grows while a line plays.
    place(frame, panel::sun, panel::centre_x * unit, panel::centre_y * unit, playing ? unit + unit / 4 : unit,
          panel::shade_grey0, fill_and_stroke);
    for (u32 k = 0; k < 3; ++k) hide(frame, panel::ripple0 + k);

    // Hold (Shift + encoder 1), under the orrery.
    text(frame, panel::text_hold, 228 * unit, 214 * unit, unit, hold_on(c) ? "hold" : "");
}
}
