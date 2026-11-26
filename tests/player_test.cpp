// Halcyon's own checks, compiled for the host and run by
// python sdk.py test --machine <folder>.
#include "harness.hpp"

namespace {
const u32 kQuarter = 240;   // player steps

bool in_scale(u32 semitone, u32 scale) {
    for (u32 n = 0; n < wp::kScaleSize[scale]; ++n)
        if (wp::kScaleNotes[scale][n] == semitone % 12) return true;
    return false;
}

void first_note_is_the_held_key() {
    Host h;
    require(h.press(60, 90));
    h.step();
    const std::vector<Event> on = h.ons();
    require(on.size() == 1 && on[0].pitch == 60 && on[0].velocity == 90 && on[0].time == 0);
}

void chance_zero_plays_only_the_key() {
    Host h;
    h.set(1, 0, 21);
    h.set(2, 9, 10);          // 1/32: many steps
    h.press(60);
    h.step(kQuarter * 16);
    require(h.ons().size() == 1);
}

void chance_full_plays_every_step_in_scale_and_range() {
    for (u32 scale = 0; scale < wp::kScales; ++scale)
        for (u32 spread = 0; spread < 4; ++spread) {
            Host h;
            h.set(2, 7, 10);  // 1/16 = 60 steps
            h.set(1, 20, 21); // 100 %
            h.set(0, scale, 8);
            h.set(6, spread, 4);
            h.amount(4, 0); h.amount(7, 0);
            h.press(48);
            h.step(60 * 64);
            const std::vector<Event> on = h.ons();
            require(on.size() == 64);
            for (u32 i = 0; i < on.size(); ++i) {
                require(on[i].time == 60 * i);                       // on the grid without Jitter
                require(on[i].pitch >= 48 && on[i].pitch < 48 + 12 * (spread + 1));
                require(in_scale(on[i].pitch - 48, scale));
                require(on[i].velocity >= 75 && on[i].velocity <= 100);   // up to a quarter quieter
            }
            require(h.sounding() <= 1);
        }
}

void deja_vu_full_loops_the_phrase() {
    Host h;
    h.set(2, 7, 10); h.set(1, 20, 21); h.amount(4, 1.0); h.set(5, 3, 16); h.amount(7, 0);
    h.press(60);
    h.step(60 * 40);
    const std::vector<Event> on = h.ons();
    require(on.size() == 40);
    for (u32 i = 8; i < on.size(); ++i) require(on[i].pitch == on[i - 4].pitch);
    // The loop starts with the held key.
    require(on[4].pitch == 60 && on[8].pitch == 60);
}

void deja_vu_zero_wanders() {
    Host h;
    h.set(2, 7, 10); h.set(1, 20, 21); h.amount(4, 0); h.set(5, 3, 16); h.amount(7, 0); h.set(6, 3, 4);
    h.press(60);
    h.step(60 * 40);
    const std::vector<Event> on = h.ons();
    u32 repeats = 0;
    for (u32 i = 8; i < on.size(); ++i) repeats += on[i].pitch == on[i - 4].pitch;
    require(repeats < 16);
}

void every_press_starts_a_new_line() {
    Host h;
    h.set(2, 7, 10); h.set(1, 20, 21); h.amount(7, 0); h.set(6, 3, 4);
    h.press(60); h.step(60 * 16); h.release(60);
    const std::vector<Event> a = h.ons();
    h.events.clear();
    h.press(60); h.step(60 * 16); h.release(60);
    const std::vector<Event> b = h.ons();
    require(a.size() == 16 && b.size() == 16);
    u32 same = 0;
    for (u32 i = 0; i < 16; ++i) same += a[i].pitch == b[i].pitch;
    require(same < 12);
}

void gate_sets_the_note_length() {
    Host h;
    h.set(2, 3, 10);          // 1/4 = 240 steps
    h.set(1, 20, 21); h.amount(7, 0);
    h.set(3, 4, 11);          // 50 %
    h.press(60);
    h.step(240 * 8);
    const std::vector<Event> on = h.ons(), off = h.offs();
    require(on.size() == 8 && off.size() >= 7);
    for (u32 i = 0; i < 7; ++i) {
        const u32 length = off[i].time - on[i].time;
        require(length == 120);   // half of 240
    }
}

void tie_holds_until_the_next_note() {
    Host h;
    h.set(2, 5, 10);          // 1/8 = 120
    h.set(1, 10, 21);         // 50 %: rests between notes
    h.set(3, 10, 11);         // tie
    h.amount(7, 0);
    h.press(60);
    for (u32 n = 0; n < 120 * 32; ++n) { h.step(); require(h.sounding() == 1); }
    const std::vector<Event> on = h.ons(), off = h.offs();
    require(on.size() > 4 && on.size() < 32);
    require(off.size() == on.size() - 1);
    for (u32 i = 0; i < off.size(); ++i) require(off[i].time == on[i + 1].time);
}

void jitter_stays_within_a_quarter_step() {
    Host h;
    h.set(2, 3, 10); h.set(1, 20, 21); h.amount(7, 1.0);
    h.press(60);
    h.step(240 * 40);
    const std::vector<Event> on = h.ons();
    require(on.size() >= 39);
    require(on[0].time == 0);
    u32 off_grid = 0;
    for (u32 i = 1; i < on.size(); ++i) {
        const i32 error = static_cast<i32>(on[i].time) - static_cast<i32>(240 * i);
        require(error >= -60 && error <= 60);   // a quarter of 240
        off_grid += error != 0;
    }
    require(off_grid > 20);
}

void release_ends_the_line() {
    Host h;
    h.set(2, 7, 10); h.set(1, 20, 21);
    h.press(60);
    h.step(600);
    require(!h.release(60));
    require(h.sounding() == 0);
    const size_t before = h.events.size();
    h.step(2000);
    require(h.events.size() == before && h.stale == 0);
}

void each_key_of_a_chord_wanders() {
    Host h;
    h.set(2, 7, 10); h.set(1, 20, 21); h.set(3, 10, 11);
    const u32 keys[3] = {48, 55, 63};
    for (u32 k : keys) h.press(k);
    for (u32 n = 0; n < 60 * 64; ++n) { h.step(); require(h.sounding() <= 3); }
    require(h.largest_drain <= 8);
    const std::vector<Event> on = h.ons();
    require(on.size() >= 3 * 60);
    h.release(55);
    h.step(600);
    for (u32 k : keys) h.release(k);
    require(h.sounding() == 0 && h.stale == 0);
}

void eight_keys_and_a_ninth() {
    Host h;
    h.set(2, 9, 10); h.set(1, 20, 21); h.amount(7, 1.0);
    for (u32 k = 0; k < 8; ++k) require(h.press(40 + 3 * k));
    require(h.press(90));     // taken, silent: the SDK holds no ninth root
    for (u32 n = 0; n < 30 * 100; ++n) { h.step(); require(h.sounding() <= 8); }
    require(h.largest_drain <= 8);
    require(h.release(90));   // nothing of stock's to release
    require(h.dropped == 0);
    for (u32 k = 0; k < 8; ++k) h.release(40 + 3 * k);
    require(h.sounding() == 0 && h.stale == 0);
}

void a_long_silence_never_leaks_a_parked_tone() {
    Host h;
    h.set(1, 0, 21);          // only the first note
    h.set(3, 0, 11);          // 10 % gate: then parked
    h.press(60);
    h.step(200000);           // past the parked tone's delay several times
    const std::vector<Event> on = h.ons();
    require(on.size() == 1);
    for (const Event& e : on) require(e.velocity > 1);
    h.release(60);
    require(h.sounding() == 0);
}

void high_keys_stay_in_midi() {
    Host h;
    h.set(2, 9, 10); h.set(1, 20, 21); h.set(6, 3, 4);
    h.press(120);
    h.step(30 * 200);
    for (const Event& e : h.ons()) require(e.pitch <= 127);
}

void reset_clears_held_keys() {
    Host h;
    h.press(60); h.press(64);
    h.step(500);
    wp::reset(h.state);
    require(!h.state.active && !h.state.sounding && !h.state.on_mask && !h.state.off_mask);
    require(h.state.rng != 0);
}


// Pitches sounding now, from the model's levels.
std::vector<u32> sounding_pitches(const Host& h) {
    std::vector<u32> out;
    for (u32 n = 0; n < 128; ++n) if (h.level[n] > 0) out.push_back(n);
    return out;
}

void hold_keeps_lines_playing() {
    Host h;
    h.set(2, 7, 10); h.set(1, 20, 21); h.set(6, 0, 4); h.set(3, 10, 11);
    h.hold(true);
    h.press(36);
    h.step(600);
    require(h.release(36));                // taken: stock keeps the record
    const size_t before = h.ons().size();
    h.step(60 * 20);
    require(h.ons().size() >= before + 19);
    require(h.sounding() == 1);
    // A new key after letting go of everything replaces the latched line.
    h.press(84);
    h.step(60 * 20);
    for (u32 p : sounding_pitches(h)) require(p >= 84);
    std::vector<Event> on = h.ons();
    for (u32 i = on.size() - 19; i < on.size(); ++i) require(on[i].pitch >= 84 && on[i].pitch < 96);
    // Keys pressed together are latched together, and replaced together.
    h.press(48);                            // 84 still held: 48 joins it
    h.step(300);
    h.release(84); h.release(48);
    h.step(60 * 10);
    require(h.sounding() == 2);
    h.press(60);
    h.step(60 * 10);
    require(h.sounding() == 1);
    for (u32 p : sounding_pitches(h)) require(p >= 60 && p < 72);
    require(h.stale == 0 && h.dropped == 0 && h.largest_drain <= 8);
}

void hold_off_stops_what_no_finger_holds() {
    Host h;
    h.set(2, 7, 10); h.set(1, 20, 21); h.set(6, 0, 4); h.set(3, 10, 11);
    h.hold(true);
    h.press(36); h.press(60);
    h.step(300);
    h.release(36);                          // latched; 60 still held
    h.step(300);
    h.hold(false);
    h.step(2);
    require(h.sounding() == 1);
    for (u32 p : sounding_pitches(h)) require(p >= 60 && p < 72);
    h.release(60);
    require(h.sounding() == 0);
    const size_t before = h.events.size();
    h.step(1000);
    require(h.events.size() == before && h.stale == 0);
}

void hold_eight_then_a_new_chord() {
    Host h;
    h.set(2, 9, 10); h.set(1, 20, 21); h.set(6, 0, 4); h.set(3, 10, 11);
    h.hold(true);
    for (u32 k = 0; k < 8; ++k) h.press(30 + 10 * k);
    h.step(100);
    for (u32 k = 0; k < 8; ++k) h.release(30 + 10 * k);
    h.step(100);
    require(h.sounding() == 8);
    // Stock still holds eight records when the first new key arrives: it is silent,
    // the latched lines stop, and the chord's next key starts.
    h.press(55);
    h.press(57);
    h.step(300);
    require(h.sounding() == 1 && h.dropped == 0 && h.stale == 0);
    for (u32 p : sounding_pitches(h)) require(p >= 57 && p < 69);
    h.release(55); h.release(57);
    h.hold(false);
    h.step(2);
    require(h.sounding() == 0);
}
}

// Clock on free: Halcyon Wander's own Rate. Its step lengths in 44.1 kHz frames, from
// Halcyon Wander's journey::interval at these Rate amounts, and the same in seconds at
// any tempo.
void free_clock_is_halcyon_wanders_rate() {
    const u32 original[7][2] = {{0, 176400}, {5461, 88206}, {10922, 44106}, {16383, 22055},
                                {21845, 11026}, {27306, 5514}, {32767, 2757}};
    for (u32 i = 0; i < 7; ++i) require(wp::free_frames(original[i][0]) == original[i][1]);
    const u32 tempos[3] = {600, 1200, 1730};
    for (u32 t = 0; t < 3; ++t)
        for (u32 i = 0; i < 7; ++i) {
            Host h;
            h.controls.bpm_x10 = tempos[t];
            h.clock_free(true);
            h.controls.value[2] = static_cast<i32>(original[i][0] << 16);
            h.set(1, 20, 21); h.amount(7, 0);
            // The expected step in player steps (bpm_x10 * 0.4 a second), to the nearest.
            const double seconds = original[i][1] / 44100.0;
            const u32 step = static_cast<u32>(seconds * tempos[t] * 0.4 + 0.5);
            h.press(60);
            h.step(step * 6 + 1);
            const std::vector<Event> on = h.ons();
            require(on.size() == 7);
            for (u32 n = 1; n < on.size(); ++n) require(on[n].time - on[n - 1].time == step);
        }
    // Sync is unchanged: Rate's ten note lengths on the tempo.
    Host h;
    h.set(2, 3, 10); h.set(1, 20, 21); h.amount(7, 0);
    h.press(60);
    h.step(240 * 4 + 1);
    require(h.ons().size() == 5 && h.ons()[1].time == 240);
}

// Chance 100 %, no Jitter: a note on every step of the line, so the gaps are the Rate.
static Host steady(u32 bpm_x10) {
    Host h;
    h.controls.bpm_x10 = bpm_x10;
    h.set(1, 20, 21);
    h.amount(7, 0.0);
    return h;
}
static std::vector<u32> gaps_from(const Host& h, u32 from) {
    std::vector<u32> out;
    const std::vector<Event> on = h.ons();
    for (size_t i = 1; i < on.size(); ++i) if (on[i].time >= from) out.push_back(on[i].time - on[i - 1].time);
    return out;
}

void sync_rates_are_note_lengths() {
    // 2/1 to 1/32 across the knob, in player steps (240 a quarter), at any tempo.
    const u32 want[10] = {1920, 960, 480, 240, 160, 120, 80, 60, 40, 30};
    for (u32 b = 0; b < 10; ++b) {
        Host h = steady(b & 1 ? 900 : 1500);
        h.set(2, b, 10);
        h.press(60);
        h.step(want[b] * 6 + 10);
        const std::vector<u32> g = gaps_from(h, 0);
        require(g.size() >= 5);
        for (u32 x : g) require(x == want[b]);
    }
}

void free_rate_is_seconds_at_any_tempo() {
    // Rate's ends, 4 s and 62.5 ms, whatever the tempo; and across a tempo change.
    const u32 tempos[3] = {600, 1200, 1800};
    for (u32 t = 0; t < 3; ++t) {
        const u32 per_second = tempos[t] * 4 / 10;   // player steps a second
        for (u32 end = 0; end < 2; ++end) {
            Host h = steady(tempos[t]);
            h.clock_free(true);
            h.amount(2, end);
            h.press(60);
            const u32 want = end ? per_second / 16 : per_second * 4;
            h.step(want * 5 + 10);
            const std::vector<u32> g = gaps_from(h, 0);
            require(g.size() >= 4);
            for (u32 x : g) require(x == want);
        }
    }
    Host h = steady(1200);
    h.clock_free(true);
    h.amount(2, 0.5);                                // 0.5 s: 240 steps at 120 BPM
    h.press(60);
    h.step(2000);
    h.controls.bpm_x10 = 600;
    const u32 at = h.now;
    h.step(3000);
    const std::vector<u32> g = gaps_from(h, at);
    require(g.size() >= 5);
    for (size_t i = 1; i < g.size(); ++i) require(g[i] == 120);   // still 0.5 s
}

void rate_turned_up_waits_out_the_step_under_way() {
    // As Halcyon Wander: the next step was set when the last sounded.
    Host h = steady(1200);
    h.clock_free(true);
    h.amount(2, 0.0);
    h.press(60);
    h.step(100);
    h.amount(2, 1.0);
    h.step(3000);
    const std::vector<Event> on = h.ons();
    require(on.size() > 10 && on[1].time == 1920);
    for (u32 x : gaps_from(h, 1921)) require(x == 30);
}

int main() {
    static_assert(sizeof(wp::State) <= player::state_bytes, "State fits the SDK's 2048 bytes");
    static_assert(alignof(wp::State) <= player::state_alignment, "State's alignment");
    first_note_is_the_held_key();
    chance_zero_plays_only_the_key();
    chance_full_plays_every_step_in_scale_and_range();
    deja_vu_full_loops_the_phrase();
    deja_vu_zero_wanders();
    every_press_starts_a_new_line();
    gate_sets_the_note_length();
    tie_holds_until_the_next_note();
    jitter_stays_within_a_quarter_step();
    release_ends_the_line();
    each_key_of_a_chord_wanders();
    eight_keys_and_a_ninth();
    a_long_silence_never_leaks_a_parked_tone();
    high_keys_stay_in_midi();
    reset_clears_held_keys();
    hold_keeps_lines_playing();
    hold_off_stops_what_no_finger_holds();
    hold_eight_then_a_new_chord();
    free_clock_is_halcyon_wanders_rate();
    sync_rates_are_note_lengths();
    free_rate_is_seconds_at_any_tempo();
    rate_turned_up_waits_out_the_step_under_way();
    std::printf("PASS (State %u bytes)\n", static_cast<unsigned>(sizeof(wp::State)));
    return 0;
}
