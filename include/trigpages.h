/* SPDX-License-Identifier: GPL-2.0-or-later */
/* trig-pages: more TRIG sub-pages for the Digitakt mk1 (OS 1.53, core 4.0, seq-events), where a track
 * picks a generator, a mod that changes what its steps play:
 *
 *     TRIG 1/3   the stock page, as it is
 *     TRIG 2/3   GEN on knob A (OFF or a generator), the generator's knobs on B-H
 *     TRIG 3/3   the generator's view
 *
 * With GEN OFF the page has two sub-pages (1/2, 2/2). trig-pages draws 2/3 as the stock pages are drawn
 * (the knob rings, the labels, the header's pop-up while a knob turns), keeps each track's generator and
 * knob values on the +Drive, and hands the sequencer's step fire (seq-events) to the track's generator.
 *
 * A generator is a mod with one entry in the tp_gens table, a function that gives its descriptor:
 *
 *     "subscribe": [{ "event": "tp_gens", "fn": "turing_gen", "order": 50 }]
 *     const struct tp_gen *turing_gen(void) { return &turing; }
 *
 * build it with -I../trig-pages/include and "requires": ["trig-pages"]. Every callback is optional (0). */
#ifndef TRIGPAGES_H
#define TRIGPAGES_H

#include <stdint.h>
#include "digitakt-mk1/seq.h"

#define TP_ABI      1
#define TP_KNOBS    7                           /* B-H */

/* A generator's knob. Its value is 0 to max (an index for a list), default def; 0 name: unused (a blank
 * knob). text writes the value's text into buf (up to 23 chars): long 0 the short text under the knob
 * (capitals, the label font), long 1 the pop-up's (any case); 0 text: the number. draw gets the knob's
 * cell (x, y; y = 0 the screen's bottom row), draws its graphic in the ring's place (17 x 17: x + 1 to
 * x + 17, y + 7 to y + 23) and returns nonzero; 0 draw (or 0 returned): the stock ring, turned as far as
 * the value is through 0-max. The label is drawn under it, at y. */
struct tp_knob {
    const char *name;                           /* 4 capitals, under the knob */
    const char *lname;                          /* the pop-up's: "Loop Lock" */
    uint8_t max, def;
    uint16_t pad;
    int32_t (*text)(char *buf, int32_t value, int32_t track, int32_t lng);
    int32_t (*draw)(void *bmp, int32_t x, int32_t y, int32_t value, int32_t track);
};

/* The step fire, at interrupt level, as seq-events gives it (seq-events.h), for the generator's tracks
 * only. tick: the UI task, 30 times a second, per track with the generator. view: TRIG 3/3, under the
 * header (x 16-127, y 0-52; the track box and LEV stay); the screen is redrawn every frame while it shows.
 * icon: GEN's graphic (17 x 17, as draw). */
struct tp_gen {
    uint32_t abi;                               /* TP_ABI */
    uint32_t id;                                /* four letters, kept on the +Drive: 'TURI' */
    const char *name;                           /* GEN's value under the knob, 6 capitals at most */
    const char *lname;                          /* the pop-up's */
    struct tp_knob knob[TP_KNOBS];
    void (*trig)(int32_t track, int32_t step, int32_t first, uint32_t *trig);
    void (*notes)(int32_t track, int32_t step, int32_t generated, int8_t notes[SQ_NOTES]);
    void (*vel)(int32_t track, int32_t step, int32_t *vel);
    int32_t (*locks)(int32_t track, int32_t step, struct sq_lock *out, int32_t room);
    void (*tick)(int32_t track);
    void (*view)(void *bmp, int32_t track);
    void (*icon)(void *bmp, int32_t x, int32_t y, int32_t track);
};
typedef const struct tp_gen *(*tp_gen_fn)(void);

/* A track's knob values B-H (0-6), as the generator's knobs have them. Interrupt level or UI task. */
extern volatile uint8_t tp_val[8][TP_KNOBS];

/* The stock fonts and drawing, for a generator's view and graphics. tp_text: align 0 left, 1 centred, 2
 * right at x; y the text's bottom row. tp_ring: the stock knob ring for value of 0-max, its cell at x, y
 * (the ring from x, y + 7). */
extern const char tp_font_label[];              /* the knobs' labels: 5 rows, capitals */
extern const char tp_font_pop[];                /* the pop-up's: 6 rows, any case */
extern const char tp_font_big[];                /* 9 rows: digits, A-G and # */
extern void tp_text(void *bmp, const void *font, int32_t x, int32_t y, int32_t align, const char *s);
extern void tp_ring(void *bmp, int32_t x, int32_t y, int32_t value, int32_t max);

#endif
