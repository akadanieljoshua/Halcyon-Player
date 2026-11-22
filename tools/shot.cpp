// Prints Halcyon's page for a few moments as JSON, for tools/screens.py.
// g++ -std=c++11 -I ../open-xy/sdk/include tools/shot.cpp -o shot
// The header is the SDK's own (opxy/page_header.hpp), as the player page draws it.
#include "../tests/harness.hpp"
#include <opxy/page_header.hpp>

namespace {
void frame(const Host& h, const char* caption, bool page2) {
    player::Display d = {};
    wp::display(h.state, h.controls, d);
    static const char* const titles[8] = {"rate", "chance", "scale", "gate", "deja vu", "length", "spread", "jitter"};
    char values[8][opxy::draw::header_capacity] = {};
    for (u32 i = 0; i < 8; ++i) for (u32 k = 0; k < 7 && d.values[i][k]; ++k) values[i][k] = d.values[i][k];
    opxy::draw::PageHeader header = {};
    opxy::draw::Frame<0, 0, 8, 16, 1> f = {};
    opxy::draw::Input in = {};
    in.elapsed = 17; in.track = 1; in.reduced_motion = 1;
    for (u32 i = 0; i < 4; ++i) in.shown[i] = static_cast<opxy::u8>(page2 ? i + 4 : i);
    opxy::draw::page_header(header, in, f, titles, values, 0);
    std::printf("{\"caption\":\"%s\",\"lit\":%u,\"text\":\"%s\",\"nodes\":[", caption, d.lit, d.text);
    for (u32 i = 0; i < 8; ++i) std::printf("%s[%d,%d,%d]", i ? "," : "", f.nodes[i].x, f.nodes[i].y, f.nodes[i].scale);
    std::printf("],\"texts\":[");
    for (u32 i = 0; i < 16; ++i) std::printf("%s[%d,%d,%d,\"%s\"]", i ? "," : "", f.texts[i].x, f.texts[i].y, f.texts[i].scale, f.texts[i].chars);
    std::printf("]}");
}
}

int main() {
    std::printf("[");
    {
        Host h;
        frame(h, "nothing held: dorian, 2 octaves", false);
    }
    std::printf(",");
    {
        Host h;
        h.set(1, 20, 21);
        h.hold(true);
        h.press(57);
        h.step(240 * 3);
        h.release(57);
        h.step(240 * 3 + 2);
        frame(h, "Hold on, the key let go: the marble on the note just sounded, the halo swelled", false);
    }
    std::printf(",");
    {
        Host h;
        h.set(1, 20, 21); h.set(2, 6, 8); h.set(6, 3, 4);
        h.press(53);
        h.step(240 * 7 + 100);
        frame(h, "major pentatonic, 4 octaves", false);
    }
    std::printf(",");
    {
        Host h;
        h.set(1, 20, 21); h.set(2, 0, 8); h.set(6, 0, 4);
        h.press(62);
        h.step(240 * 9 + 100);
        frame(h, "page 2: minor, 1 octave", true);
    }
    std::printf("]\n");
    return 0;
}
