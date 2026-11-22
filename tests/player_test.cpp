// Wander Player's own checks, compiled for the host and run by
// python sdk.py test --machine <folder>.
#include "harness.hpp"
#include <cstring>

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
    h.set(0, 9, 10);          // 1/32: many steps
    h.press(60);
    h.step(kQuarter * 16);
    require(h.ons().size() == 1);
}

void chance_full_plays_every_step_in_scale_and_range() {
    for (u32 scale = 0; scale < wp::kScales; ++scale)
        for (u32 spread = 0; spread < 4; ++spread) {
            Host h;
            h.set(0, 7, 10);  // 1/16 = 60 steps
            h.set(1, 20, 21); // 100 %
            h.set(2, scale, 8);
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
    h.set(0, 7, 10); h.set(1, 20, 21); h.amount(4, 1.0); h.set(5, 3, 16); h.amount(7, 0);
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
    h.set(0, 7, 10); h.set(1, 20, 21); h.amount(4, 0); h.set(5, 3, 16); h.amount(7, 0); h.set(6, 3, 4);
    h.press(60);
    h.step(60 * 40);
    const std::vector<Event> on = h.ons();
    u32 repeats = 0;
    for (u32 i = 8; i < on.size(); ++i) repeats += on[i].pitch == on[i - 4].pitch;
    require(repeats < 16);
}

void every_press_starts_a_new_line() {
    Host h;
    h.set(0, 7, 10); h.set(1, 20, 21); h.amount(7, 0); h.set(6, 3, 4);
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
    h.set(0, 3, 10);          // 1/4 = 240 steps
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
    h.set(0, 5, 10);          // 1/8 = 120
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
    h.set(0, 3, 10); h.set(1, 20, 21); h.amount(7, 1.0);
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
    h.set(0, 7, 10); h.set(1, 20, 21);
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
    h.set(0, 7, 10); h.set(1, 20, 21); h.set(3, 10, 11);
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
    h.set(0, 9, 10); h.set(1, 20, 21); h.amount(7, 1.0);
    for (u32 k = 0; k < 8; ++k) require(h.press(40 + 3 * k));
    require(h.press(90));     // taken, silent: the SDK holds no ninth root
    for (u32 n = 0; n < 30 * 100; ++n) { h.step(); require(h.sounding() <= 8); }
    require(h.largest_drain <= 8);
    require(h.release(90));   // nothing of stock's to release
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
    h.set(0, 9, 10); h.set(1, 20, 21); h.set(6, 3, 4);
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

bool terminated(const char* text, u32 size) {
    for (u32 i = 0; i < size; ++i) if (!text[i]) return true;
    return false;
}

void the_page() {
    Host h;
    player::Display d = {};
    wp::display(h.state, h.controls, d);
    // Dorian: 0 2 3 5 7 9 10, and no marble while nothing is held.
    require(d.lit == ((1u << 0) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 10)));
    require(!std::strcmp(d.text, "dorian"));
    require(!std::strcmp(d.values[0], "1/4") && !std::strcmp(d.values[1], "65%") && !std::strcmp(d.values[2], "dor"));
    require(!std::strcmp(d.values[3], "80%") && !std::strcmp(d.values[4], "50%") && !std::strcmp(d.values[5], "8"));
    require(!std::strcmp(d.values[6], "2 oct") && !std::strcmp(d.values[7], "25%"));
    h.press(62);
    h.step();
    d = player::Display();
    wp::display(h.state, h.controls, d);
    require(d.lit >> 12 == 1u && !std::strcmp(d.text, "D4"));   // the marble on the root
    // Every choice of every control formats inside its fixed arrays.
    for (u32 c = 0; c < 8; ++c)
        for (u32 i = 0; i < 32; ++i) {
            Host k;
            k.set(c, i, 32);
            k.amount(4, i / 31.0);
            k.amount(7, i / 31.0);
            d = player::Display();
            wp::display(k.state, k.controls, d);
            for (u32 v = 0; v < 8; ++v) require(terminated(d.values[v], 8) && d.values[v][0]);
            require(terminated(d.text, 32));
        }
    h.set(0, 9, 10); h.set(1, 20, 21); h.set(6, 3, 4);
    for (u32 n = 0; n < 3000; ++n) {
        h.step();
        d = player::Display();
        wp::display(h.state, h.controls, d);
        u32 marbles = 0;
        for (u32 b = 12; b < 24; ++b) marbles += d.lit >> b & 1;
        require(marbles == 1);
        require(d.lit >> 24 == 0);
    }
}
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
    the_page();
    std::printf("PASS (State %u bytes)\n", static_cast<unsigned>(sizeof(wp::State)));
    return 0;
}
