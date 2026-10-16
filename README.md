# TRIG pages (Digitakt mk1)

More TRIG sub-pages for the Digitakt mk1, OS 1.53, where each audio track
picks a **generator**: a mod that changes what the track's steps play. TRIG
pages is the framework; it has no generator of its own. The first one is
[TURING](../turing).

It is an [elekloader](https://github.com/irpina/elekloader) mod, on core 4.0
and elekloader's seq-events: elekloader builds a custom OS file on your own
machine, from your stock OS file and the mods you pick; nothing from Elektron
is distributed.

| sub-page | shows |
|---|---|
| TRIG 1/3 | the stock TRIG page, as it is |
| TRIG 2/3 | **A GEN**: OFF or a generator; **B-H** the generator's knobs |
| TRIG 3/3 | the generator's view (only while the track has one) |

Press **TRIG** again to step through them, as on SRC, FLTR and LFO; the title
names the sub-page. With GEN OFF the page has two sub-pages, 1/2 and 2/2.

- 2/3 is drawn as the stock pages are: the stock knob rings, the labels in the
  stock font (a knob's value under it while it turns) and the header's pop-up
  with its long name and value ("Loop Lock=75%"). A generator may draw a knob's
  graphic of its own in the ring's place.
- Knobs A-H on 2/3 and 3/3 set the generator, never a trig; LEVEL is the
  stock's. The trig keys work as on 1/3.
- Each track's generator and knob values are kept on the +Drive
  (`/cfw/trigp.a` and `/cfw/trigp.b`, written in turn) and loaded at start-up.
  A generator is kept by its id, so a build without it finds the track OFF.
- The values are the track's, not the pattern's, and they do not lock.

## How it works

- **The sub-pages.** A page view keeps the list of its sub-pages (page ids) as
  a vector at +0x7c (begin, end, capacity), the shown one at +0x90; pressing
  the page's key again steps through the list (`% count`), and the stock title
  names it "TRIG 2/3". The audio tracks' TRIG page is built with one id, 1
  (SRC, FLTR and LFO with two). TRIG pages patches the page's draw (its
  vtable's slot 4, `0x40184018`: `0x400368ba` → `tp_draw`), which points the
  list at its own `{1, 1, 1}`, two or three long, and draws 2/3 and 3/3
  itself. The stock code sees the TRIG page on each. A MIDI track's TRIG page
  (id 15) is left as it is.
- **The knobs.** While 2/3 or 3/3 shows, `ev_enc` takes A-H through the stock
  knob filter (a short list a notch at a time), as CUT's settings page does.
- **The pop-up.** `ev_draw` draws the inverted bar over the header, in the
  stock pop-up's font, while a knob turns.
- **The step fire.** seq-events' `ev_gen_trig`, `ev_gen_notes`, `ev_fire_vel`
  and `ev_fire_locks` (order 40) go to the track's generator.

The stock assets it draws with, found in the firmware and in an emulator's RAM:

| | 1.53 | what it is |
|---|---|---|
| `FW_KNOB_SET` | `0x421f84c4` | the knob ring's 96 frames (a vector of images, 0x1c bytes each); a knob shows 12-84 |
| `FW_NO_KNOB` | `0x421f7f60` | the dotted frame of a knob without a parameter |
| `tp_font_label` | `0x401fe09c` | the knob labels' font: 5 rows |
| `tp_font_pop` | `0x401fd1cc` | the header pop-up's: 6 rows, any case |
| `tp_font_big` | `0x401f9160` | the track box's digit font: 9 rows |
| | cells | x 24, 50, 76, 102; rows at y 27 and 0; the ring from y + 7, the label centred at x + 9 |

## Writing a generator

[include/trigpages.h](include/trigpages.h) is the whole API. A generator adds
one entry to the `tp_gens` table, a function that gives its descriptor: its id
and name, up to seven knobs (name, long name, range, default, value text, an
optional graphic), the step-fire callbacks, a UI tick, its view for 3/3 and
GEN's graphic. It reads its values from `tp_val[track]`.

```json
"subscribe": [{ "event": "tp_gens", "fn": "turing_gen", "order": 50 }],
"cflags": ["-I../trig-pages/include"],
"requires": ["core", "seq-events", "trig-pages"]
```

## Build

seq-events is in elekloader (`mods/seq-events-dt1`); build it the same way.

```bash
python -m elekloader.sdk.build . --stock Digitakt_OS1.53.syx            # -> out/trig-pages-0.1.elemod
python -m elekloader.lint out/trig-pages-0.1.elemod --stock Digitakt_OS1.53.syx \
    --with core-4.0.elemod --with seq-events-0.1.elemod
```

## Status

0.1: built and linked by elekloader with core 4.0 and seq-events 0.1, alone,
with TURING, and beside every mod of this set (digislicer, euclid, duck, GRAIN,
WAVE, CUT). Not yet run. Things to check on the first run:

- TRIG pressed again: 2/2 (GEN OFF), the title, the GEN knob; with TURING 2/3
  and 3/3, and back to 1/3;
- on 2/3 the rings, labels and graphics where the stock page has them; a
  knob's value under it and in the header while it turns;
- A-H never change a trig on 2/3 or 3/3; LEVEL still works; the trig keys do;
- another track: its own generator; a track without one on 3/3 goes to 2/2;
- the settings back after a restart.

Planned: open on 2/3 when the track has a generator; values per pattern (core
4.0 has the room); the trig keys' LEDs showing what the generator plays.

The stock OS file always recovers the unit: hold [FUNC] while powering on, press
[TRIG 4] for OS UPGRADE, and send the stock `.syx` with Transfer.

## Licence

GPL-2.0-or-later: [LICENSE](LICENSE). Not affiliated with Elektron. Custom
firmware is at your own risk.
