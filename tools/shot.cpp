// Runs Wander Player and its own page on the host through a short performance at
// 120 BPM, 30 frames a second, and prints every frame as JSON for tools/screens.py:
// the page's nodes and texts, a caption, and the notes played.
// g++ -std=c++11 -O2 -I ../open-xy/sdk/include tools/shot.cpp -o shot
#include "../tests/harness.hpp"
#include "../display.cpp"

int main() {
    Host h;
    h.controls.bpm_x10 = 1200;
    h.set(0, 5, 10);          // 1/8
    h.set(1, 15, 21);         // 75 %
    wp::DisplayState state = {};
    wp::Frame frame = {};
    opxy::draw::Input input = {};
    input.elapsed = 34;       // 30 frames a second
    wp::display_reset(state);
    const u32 fps = 30, per = 16, frames = 13 * fps;
    const char* caption = "nothing held";
    std::printf("{\"frames\":[\n");
    for (u32 n = 0; n < frames; ++n) {
        if (n == 1 * fps) { h.press(57, 100); caption = "holding A3: dorian, 2 octaves"; }
        if (n == 4 * fps) { h.set(6, 3, 4); caption = "Spread turned to 4 octaves"; }
        if (n == 6 * fps + 15) { h.set(2, 6, 8); caption = "Scale turned to major pentatonic"; }
        if (n == 8 * fps) { h.hold(true); caption = "Hold on (Shift + encoder 1)"; }
        if (n == 8 * fps + 15) { h.release(57); caption = "Hold on, key let go: it keeps wandering"; }
        if (n == 11 * fps) { h.hold(false); caption = "Hold off: the line stops, the marble lingers"; }
        if (n == 12 * fps + 10) caption = "at rest";
        h.step(per);
        for (u32 i = 0; i < 8; ++i) input.slots[i] = h.controls.value[i];
        for (u32 i = 0; i < 4; ++i) { input.shown[i] = static_cast<opxy::u8>(i); input.parameters[i] = h.controls.value[i]; }
        wp::display(state, h.state, h.controls, input, frame);
        if (!opxy::draw::valid(frame)) { std::fprintf(stderr, "invalid frame %u\n", n); return 1; }
        std::printf("%s{\"caption\":\"%s\",\"nodes\":[", n ? ",\n" : "", caption);
        for (u32 i = 0; i < wp::Frame::node_count; ++i) {
            const opxy::draw::Node& d = frame.nodes[i];
            std::printf("%s[%d,%d,%d,%d,%d,%u,%u]", i ? "," : "", d.x, d.y, d.scale, d.cos, d.sin, d.shade, static_cast<unsigned>(d.paint));
        }
        std::printf("],\"texts\":[");
        for (u32 i = 0; i < wp::Frame::text_count; ++i) {
            const opxy::draw::Text& t = frame.texts[i];
            std::printf("%s[%d,%d,%d,\"%s\"]", i ? "," : "", t.x, t.y, t.scale, t.chars);
        }
        std::printf("]}");
    }
    std::printf("],\n\"steps_per_second\":480,\"events\":[");
    for (size_t i = 0; i < h.events.size(); ++i) {
        const Event& e = h.events[i];
        std::printf("%s[%u,%u,%u,%d]", i ? "," : "", e.time, e.pitch, e.velocity, e.on ? 1 : 0);
    }
    std::printf("]}\n");
    return 0;
}
