// Prints Wander Player's page for a few moments as JSON, for tools/screens.py.
// g++ -std=c++11 -I ../open-xy/sdk/include tools/shot.cpp -o shot
#include "../tests/harness.hpp"

namespace {
void frame(const Host& h, const char* caption, bool page2) {
    player::Display d = {};
    wp::display(h.state, h.controls, d);
    std::printf("{\"caption\":\"%s\",\"page\":%d,\"lit\":%u,\"text\":\"%s\",\"values\":[", caption, page2 ? 1 : 0, d.lit, d.text);
    for (u32 i = 0; i < 8; ++i) std::printf("%s\"%s\"", i ? "," : "", d.values[i]);
    std::printf("],\"played\":[");
    bool first = true;
    for (u32 n = 0; n < 128; ++n)
        if (h.level[n] > 0) { std::printf("%s%u", first ? "" : ",", n); first = false; }
    std::printf("]}");
}
}

int main() {
    std::printf("[");
    {
        Host h;
        frame(h, "nothing held: dorian, its seven places lit", false);
    }
    std::printf(",");
    {
        Host h;
        h.press(57);                  // A3
        h.step(240 * 5 + 10);
        frame(h, "holding A3: the marble on the note that sounded last", false);
    }
    std::printf(",");
    {
        Host h;
        h.set(0, 7, 10); h.set(1, 20, 21); h.set(2, 6, 8); h.set(6, 3, 4); h.set(3, 10, 11);
        h.press(53); h.press(60); h.press(65);
        h.step(60 * 23 + 5);
        frame(h, "a chord, major pentatonic, 1/16, spread 4 oct: each key wanders", false);
    }
    std::printf(",");
    {
        Host h;
        h.set(2, 0, 8); h.set(6, 0, 4); h.amount(4, 0.85); h.set(5, 3, 16);
        h.press(62);
        h.step(240 * 9 + 3);
        frame(h, "page 2: minor, deja vu 85 %, length 4, spread 1 oct", true);
    }
    std::printf("]\n");
    return 0;
}
