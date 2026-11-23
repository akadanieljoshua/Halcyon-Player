// Halcyon's own page (display.cpp, SDK 1.22) over the host model of the
// player: the frames the device would draw, 60 a second at 120 BPM.
#include "harness.hpp"
#include "../display.cpp"
#include <cstring>

namespace {
namespace panel = halcyon_player::panel;
using opxy::draw::unit;

struct Page {
    Host host;
    wp::DisplayState state;
    wp::Frame frame;
    opxy::draw::Input input;
    Page() : state(), frame(), input() {
        host.controls.bpm_x10 = 1200;
        wp::display_reset(state);
        input.elapsed = 17;
    }
    // One GUI frame: 8 player steps, then the page.
    void tick(u32 frames = 1) {
        for (u32 n = 0; n < frames; ++n) {
            host.step(8);
            for (u32 i = 0; i < 8; ++i) input.slots[i] = host.controls.value[i];
            for (u32 i = 0; i < 4; ++i) { input.shown[i] = static_cast<opxy::u8>(i); input.parameters[i] = host.controls.value[i]; }
            wp::display(state, host.state, host.controls, input, frame);
            require(opxy::draw::valid(frame));
        }
    }
    const opxy::draw::Node& node(u32 i) const { return frame.nodes[i]; }
    bool hidden(u32 i) const { return node(i).x == opxy::draw::hidden_x * unit; }
    bool at(u32 i, u32 semitone, u32 octaves) const {
        u32 ring = semitone / 12;
        if (ring >= octaves) ring = octaves - 1;
        return node(i).x == panel::place_x[ring][semitone % 12] && node(i).y == panel::place_y[ring][semitone % 12];
    }
};

void header_scale_and_rings() {
    Page page;
    page.tick();
    const char* const values[16] = {"", "", "", "", "65", "dor", "1/4", "80", "", "", "", "", "50", "8", "2 oct", "25"};
    for (u32 i = 4; i < 16; ++i)
        if (values[i][0]) require(!std::strcmp(page.frame.texts[i].chars, values[i]));
    require(!std::strcmp(page.frame.texts[0].chars, "chance") || !std::strcmp(page.frame.texts[0].chars, "Chance"));
    // Clock on free: Rate in Halcyon Wander's seconds.
    page.host.clock_free(true);
    page.host.amount(2, 0);
    page.tick();
    require(!std::strcmp(page.frame.texts[6].chars, "4.0s"));
    page.host.amount(2, 0.5);
    page.tick();
    require(!std::strcmp(page.frame.texts[6].chars, "0.50s"));
    page.host.amount(2, 1.0);
    page.tick();
    require(!std::strcmp(page.frame.texts[6].chars, "63ms"));
    page.host.clock_free(false);
    // Shift held: the header names the Shift row, Hold and Clock, and shows their settings.
    page.host.controls.shift = 1;
    page.tick();
    require(!std::strcmp(page.frame.texts[0].chars, "hold") && !std::strcmp(page.frame.texts[1].chars, "clock"));
    require(!std::strcmp(page.frame.texts[4].chars, "off") && !std::strcmp(page.frame.texts[5].chars, "sync"));
    page.host.hold(true);
    page.host.clock_free(true);
    page.tick();
    require(!std::strcmp(page.frame.texts[4].chars, "on") && !std::strcmp(page.frame.texts[5].chars, "free"));
    page.host.controls.shift = 0;
    page.host.hold(false);
    page.host.clock_free(false);
    page.tick();
    require(!std::strcmp(page.frame.texts[0].chars, "chance") && !std::strcmp(page.frame.texts[6].chars, "1/32"));
    // Spread 2 octaves: two rings and their places; dorian's notes lit.
    require(!page.hidden(panel::ring0) && !page.hidden(panel::ring0 + 1) && page.hidden(panel::ring0 + 2));
    for (u32 n = 0; n < 12; ++n) {
        const bool lit = n == 0 || n == 2 || n == 3 || n == 5 || n == 7 || n == 9 || n == 10;
        require(page.node(panel::dot0_0 + n).shade == (lit ? panel::shade_lit : panel::shade_dim));
        require(page.hidden(panel::dot0_0 + 24 + n));
    }
    // Nothing held: no marble, the sun at rest, no hold line.
    require(page.hidden(panel::marble) && page.node(panel::sun).scale == unit);
    require(page.frame.texts[panel::text_hold].chars[0] == 0);
    page.host.hold(true);
    page.tick();
    require(!std::strcmp(page.frame.texts[panel::text_hold].chars, "hold"));
}

void the_marble_lands_on_each_note_and_glides_to_the_next() {
    Page page;
    page.host.set(0, 20, 21);          // every step sounds
    page.host.set(6, 3, 4);            // four octaves
    page.host.amount(7, 0);            // on the beat
    page.host.press(48);
    page.tick();
    require(page.node(panel::sun).scale == unit + unit / 4);
    require(page.at(panel::marble, 0, 4));   // the held key first
    u32 landed = 0, glided = 0, notes = 1;
    std::vector<u32> trail(1, 0);
    for (u32 f = 0; f < 60 * 8; ++f) {
        const size_t before = page.host.ons().size();
        page.tick();
        const std::vector<Event> on = page.host.ons();
        if (on.size() != before) {
            const u32 semitone = on.back().pitch - 48;
            trail.insert(trail.begin(), semitone);
            ++notes;
            // On its place the frame it sounds; the halo swelled.
            require(page.at(panel::marble, semitone, 4));
            require(page.node(panel::halo).scale > unit);
            // The trail: the notes before it, newest first.
            for (u32 k = 1; k < 5 && k < trail.size(); ++k) require(page.at(panel::trail0 + k - 1, trail[k], 4));
            ++landed;
        } else if (!page.at(panel::marble, trail[0], 4)) {
            ++glided;                       // on its way to the next note
        }
    }
    require(notes > 12 && landed == notes - 1);
    // A quarter note is 240 steps, the glide 72 of them: about 9 frames a note.
    require(glided > 6 * (notes - 2) && glided < 10 * notes);
    // The halo settles between notes.
    page.host.set(0, 0, 21);
    page.tick(60);
    require(page.node(panel::halo).scale == unit);
}

void the_marble_lingers_a_second_after_the_last_key() {
    Page page;
    page.host.set(0, 20, 21);
    page.host.press(60);
    page.tick(100);
    page.host.release(60);
    page.tick(30);
    require(!page.hidden(panel::marble) && page.node(panel::sun).scale == unit);
    page.tick(40);
    require(page.hidden(panel::marble));
    for (u32 k = 0; k < 4; ++k) require(page.hidden(panel::trail0 + k));
}

void a_wider_note_sits_on_the_outer_ring() {
    Page page;
    page.host.set(0, 20, 21); page.host.set(6, 3, 4); page.host.amount(7, 0);
    page.host.press(48);
    page.tick(60 * 4);
    page.host.set(6, 0, 4);
    page.tick();
    require(page.hidden(panel::ring0 + 1));
    // The trail's notes from four octaves now sit on the one ring, at their places.
    const std::vector<Event> on = page.host.ons();
    u32 wide = 0;
    for (u32 k = 1; k < 5; ++k) {
        const u32 semitone = on[on.size() - 1 - k].pitch - 48;
        wide += semitone >= 12;
        require(page.at(panel::trail0 + k - 1, semitone, 1));
    }
    require(wide > 0);

}
}

int main() {
    header_scale_and_rings();
    the_marble_lands_on_each_note_and_glides_to_the_next();
    the_marble_lingers_a_second_after_the_last_key();
    a_wider_note_sits_on_the_outer_ring();
    std::puts("PASS");
    return 0;
}
