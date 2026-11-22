// A host model of what the SDK and stock do around a note player: the chord
// records stock schedules (one per held root), the bounded update drains after
// a press or release and before every player step, and tone delays counted in
// player steps. Follows the SDK's player_plans.hpp and docs/api.md ("Note players").
#pragma once
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../player.cpp"

#define require(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

using opxy::i32;
using opxy::u32;
namespace wp = halcyon_player;
namespace player = opxy::player;

struct Event { u32 time, pitch, velocity; bool on; };

struct Host {
    struct Sound { i32 delay; u32 pitch, velocity; };
    struct Record { Sound tones[8]; u32 count, root; };

    wp::State state;
    player::Controls controls;
    Record records[8];
    u32 held, now, stale, dropped, largest_drain;
    int level[128];
    std::vector<Event> events;

    Host() : state(), controls(), records(), held(0), now(0), stale(0), dropped(0), largest_drain(0), level() { defaults(); }

    // A choice as the SDK stores it: the centre of its bucket.
    void set(u32 control, u32 index, u32 choices) {
        const u32 width = 0x7fffffffu / choices;
        controls.value[control] = static_cast<i32>(index * width + width / 2);
    }
    void amount(u32 control, double value) { controls.value[control] = static_cast<i32>(value * 2147483647.0); }
    void hold(bool on) { controls.shift_value[0] = on ? 0x60000000 : 0x20000000; }
    void clock_free(bool on) { controls.shift_value[1] = on ? 0x60000000 : 0x20000000; }
    // The definition's defaults: 65 %, dorian, 1/4, 80 %, deja vu 50 %, 8 steps, 2 oct, jitter 25 %.
    void defaults() {
        set(2, 3, 10); set(0, 13, 21); set(1, 1, 8); set(3, 7, 11);
        amount(4, 0.5); set(5, 7, 16); set(6, 1, 4); amount(7, 0.25);
    }

    Record* find(u32 root) {
        for (u32 i = 0; i < held; ++i)
            if (records[i].count && records[i].root == root) return &records[i];
        return nullptr;
    }
    void note_off(Record& r) {
        for (u32 i = 0; i < r.count; ++i)
            if (r.tones[i].delay < 0) {
                --level[r.tones[i].pitch];
                events.push_back(Event{now, r.tones[i].pitch, 0, false});
            }
        r.count = 0;
    }
    void stock_release(u32 root) {
        for (u32 i = 0; i < held; ++i)
            if (records[i].count && records[i].root == root) note_off(records[i]);
    }
    void store(u32 root, const player::Plan& plan, bool existing_only) {
        require(plan.count <= player::max_tones);
        for (u32 i = 0; i < plan.count; ++i) require(plan.tones[i].pitch <= 127 && plan.tones[i].velocity <= 127);
        Record* r = find(root);
        if (existing_only && !r) { ++stale; return; }
        if (r) stock_release(root);
        if (!plan.count) return;
        if (!r) {
            for (u32 i = 0; i < held && !r; ++i)
                if (!records[i].count) r = &records[i];
            if (!r) {
                if (held >= 8) { ++dropped; return; }   // stock drops a ninth root's plan
                r = &records[held++];
            }
        }
        r->root = root;
        r->count = plan.count;
        for (u32 i = 0; i < plan.count; ++i)
            r->tones[i] = Sound{plan.tones[i].delay, plan.tones[i].pitch, plan.tones[i].velocity};
    }
    void drain() {
        player::Update change;
        u32 i = 0;
        for (; i < 8 && wp::update(state, controls, change); ++i) {
            require(change.mode == player::Replace);
            require(change.plan.count <= 1);   // one note, or none: Hold's stop of a latched root
            store(change.root, change.plan, true);
        }
        if (i > largest_drain) largest_drain = i;
    }
    bool press(u32 note, u32 velocity = 100) {
        player::Plan plan;
        if (!wp::press(state, controls, note, velocity, plan)) return false;
        if (plan.count) store(note, plan, false);
        drain();
        return true;
    }
    bool release(u32 note) {
        const bool silent = wp::release(state, note);
        if (!silent) stock_release(note);
        drain();
        return silent;
    }
    void step(u32 count = 1) {
        for (u32 n = 0; n < count; ++n) {
            drain();
            for (u32 i = 0; i < held; ++i)
                for (u32 j = 0; j < records[i].count; ++j) {
                    Sound& t = records[i].tones[j];
                    if (t.delay == 0) {
                        ++level[t.pitch];
                        events.push_back(Event{now, t.pitch, t.velocity, true});
                    }
                    --t.delay;
                }
            ++now;
        }
    }
    std::vector<Event> ons() const {
        std::vector<Event> out;
        for (const Event& e : events) if (e.on) out.push_back(e);
        return out;
    }
    std::vector<Event> offs() const {
        std::vector<Event> out;
        for (const Event& e : events) if (!e.on) out.push_back(e);
        return out;
    }
    int sounding() const {
        int total = 0;
        for (int v : level) { require(v >= 0); total += v; }
        return total;
    }
};
