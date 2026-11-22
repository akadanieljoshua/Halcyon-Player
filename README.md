# Halcyon

A generative note player for the Teenage Engineering OP–XY, built with the
unofficial [Open-XY SDK](https://github.com/etcetc000/open-xy). Hold a key and
the track's own instrument wanders in a scale of its own, from the key you hold.
After Mutable Instruments' Marbles.

Halcyon is [Halcyon Wander](https://github.com/colorsinspace1/halcyon-wander)
by Colors In Space with the synth taken out: the same line (Rate, Chance,
Deja vu, Length, Spread, Jitter, the same eight scales), now sent as notes to
whatever instrument the track plays, stock or SDK.

| Page 1 | |
| --- | --- |
| **Rate** | how often the line steps, on the project tempo: 2/1 down to 1/32, with triplets |
| **Chance** | how many of those steps sound, 0 to 100 % |
| **Scale** | minor, dorian, harmonic minor, phrygian, lydian, major, major pentatonic, minor pentatonic |
| **Gate** | how long each note holds, 10 to 100 % of a step, or **tie**: until the next note sounds |

| Page 2 (press) | |
| --- | --- |
| **Deja vu** | as on Marbles: 0 always new notes, full loops the last Length steps |
| **Length** | the looped phrase, 1 to 16 steps |
| **Spread** | how far it roams above the key: 1 to 4 octaves |
| **Jitter** | loosens the timing: each step lands up to a quarter step early or late |

| Shift + encoder 1 | |
| --- | --- |
| **Hold** | off or on. On: lines keep playing after you let go; the next key you press after letting go of everything replaces them, and turning Hold off stops what no finger holds. The page shows "hold" while it is on |

The first note is always the key you hold, at the velocity you played it; the
notes after it are up to a quarter quieter. Every press starts a new line. Hold
a chord and each key wanders on a line of its own (up to eight keys). Let go
and its line stops, unless Hold is on; the instrument's release carries the
last note.

Sequenced notes play it too: a long step (or a held note in a pattern) is a
held key, so give the sequenced notes the length you want the line to run.

The page is Halcyon Wander's orrery, drawn as Halcyon Wander draws it: the key
you hold is the sun, each ring an octave of Spread, the scale's notes lit at
their clock positions, and a marble landing on each note as it sounds, its last
moves trailing behind. The marble sets off for the next note 0.15 s before it is
due and lands as it sounds; the sun grows while a line plays, its halo swells
as each note sounds, and the marble lingers a second after the last key stops.
With a chord held, the orrery follows the first key. "hold" shows under it while
Hold is on.

The page needs a note player that draws its own screen, which the Open-XY SDK
adds in 1.22 (a pull request on `staging`, not yet merged). Until then the
`main` branch keeps the SDK 1.16 page made of the SDK's 24 dots.

## What changed from Halcyon Wander

- **Sound** is gone: the track's instrument makes the sound. Halcyon's fifteen
  sounds stay in [Halcyon Wander](https://github.com/colorsinspace1/halcyon-wander).
- **Gate** is new: a synth decided how long its notes rang, a player has to say.
- **Rate** follows the project tempo in note lengths instead of seconds.
- Page 1's controls are lists that step one choice per detent: on a player's
  first page the SDK otherwise edits encoder 2 in four steps and encoder 4 as
  off/on.
- The line stops when the key is let go, unless **Hold** is on (new). Hold is on
  the player's Shift row, which the SDK doesn't save with the project, so it is
  off when a project loads. With eight keys latched, the first key of the next
  chord is silent (stock holds eight roots until the old ones are stopped); the
  chord's other keys play.
- The screen is Halcyon Wander's own, drawn from the player's State instead of
  a copy of the line the synth's screen ran beside its sound.

## Develop

Keep the [Open-XY SDK](https://github.com/etcetc000/open-xy) beside this
repository as `../open-xy`, on a branch with SDK 1.22 (a player's own drawn
page) until that reaches `staging`, then:

```
python ../open-xy/sdk.py test --machine .
```

That checks the definition and artwork, draws the page on the host over a held
key's frames, and runs `tests/player_test.cpp`, a host
model of stock's chord records around the player (first note, Chance, Deja vu
loops, Gate and tie, Jitter's window, chords, a ninth key, long silences,
release and reset, Hold) and `tests/page_test.cpp` (the header, rings and scale,
the marble landing on each note and gliding to the next, its trail, the halo and
the second it lingers). `python tools/make_panel.py` rewrites the
artwork; `tools/shot.cpp` and `tools/screens.py` draw the page for review.

Not yet checked on an OP–XY. The SDK reads stock's player step (240 per
quarter note) from stock's code without timing it on a device, so Rate and
Gate rest on that, and the SDK has not yet confirmed on a device that Shift +
encoder turns reach a player, which Hold needs.

## Licence

MIT, see [LICENSE](LICENSE). The line and its scales are Colors In Space's,
from Halcyon Wander; the player's timing follows the approach of
[Euclidean Arp](https://github.com/yfukami/op-xy-euclidean-arp) (MIT).
