#pragma once
#include <opxy/page_header.hpp>

namespace halcyon_player {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py writes the artwork (panel.h).
using Frame = opxy::draw::Frame<0, 0, 70, 17, 9>;
// What the page remembers between frames. The line itself is the player's
// State, which the page reads (SDK 1.26): it knows each note as it sounds and
// when the next is due, so the marble can set off before it.
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::u32 order;          // the shown key's press, 0 for none yet
    opxy::u32 sounded;        // State.sounded when its last note was seen
    opxy::u32 trail[5];       // the last notes, semitones above the root, newest first
    opxy::u32 trail_count;
    opxy::i32 since;          // Q10 seconds since the last note sounded
    opxy::i32 seen;           // Q10 seconds since the last key stopped, for the fade
};
}
