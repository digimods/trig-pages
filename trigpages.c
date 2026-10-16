/* SPDX-License-Identifier: GPL-2.0-or-later */
/* trig-pages: more TRIG sub-pages for the Digitakt mk1 (OS 1.53, core 4.0, seq-events), where each audio
 * track picks a generator (include/trigpages.h):
 *
 *     TRIG 1/3   the stock page
 *     TRIG 2/3   GEN on knob A, the generator's knobs on B-H, drawn as the stock pages draw theirs
 *     TRIG 3/3   the generator's view (only while the track has one: with GEN OFF, 1/2 and 2/2)
 *
 * The page: the TRIG page's view keeps the list of its sub-pages (page ids) as a vector at +0x7c (begin,
 * end, capacity), the shown one at +0x90; pressing TRIG again steps through the list (the stock title
 * names it, "TRIG 2/3"). The TRIG page's list is one id, 1. Its draw (the view's vtable, slot 4) is
 * patched to tp_draw, which points the list at tp_pages ({1, 1, 1}: two or three of them, as the track
 * has a generator) and draws sub-pages 2 and 3 itself. The stock code sees the TRIG page on each, so the
 * trig keys, the title and the page's knob filter work as on 1/3; the knobs A-H are taken there
 * (ev_enc), so they never edit a trig. LEVEL stays the stock's.
 *
 * The step fire: seq-events' generator events go to the track's generator (tp_trig and the rest). */
#include "digitakt-mk1/core3.h"
#include "trigpages.h"

#if !defined(FW_TRIG_DRAW) || !defined(FW_TRACK_OF) || !defined(FW_TEXTBOX) || !defined(FW_KNOB_SET) || \
    !defined(FW_NO_KNOB) || !defined(FW_KNOB_INIT) || !defined(FW_KNOB_TICK) || !defined(FW_KNOB_FILTER) || \
    !defined(FW_KNOB_CORE) || !defined(FW_KNOB_NOTCH) || !defined(FW_EK_OPEN) || !defined(FW_EK_MOUNTED)
#error "the firmware addresses come from mod.json's cflags"
#endif

typedef uint8_t u8;
typedef int32_t s32;
typedef uint32_t u32;

#define TRIG_DRAW       ((void (*)(void *, void *))FW_TRIG_DRAW)
#define TRACK_OF        ((s32 (*)(void *))FW_TRACK_OF)
#define TEXTBOX         ((void (*)(void *, const void *, s32, s32, s32, s32, const char *, const char *, ...))FW_TEXTBOX)
#define VIEW_VT(v)      (*(void ***)(v))
#define SUBPAGE(v)      (*(volatile s32 *)((u8 *)(v) + 0x90))
#define PG_BEGIN(v)     (*(u32 **)((u8 *)(v) + 0x7c))
#define PG_END(v)       (*(u32 **)((u8 *)(v) + 0x80))
#define PG_CAP(v)       (*(u32 **)((u8 *)(v) + 0x84))
#define PAGE_TRIG       1                       /* the TRIG page's id (an audio track's; a MIDI track's is 15) */
#define LEVEL_DRAW      (0x98 / 4)              /* the page's slot that draws LEVEL, bottom left */
#define MAXGENS         15

extern tp_gen_fn tp_gens[];                     /* the generators, in order; 0 ends them */

volatile u8 tp_val[8][TP_KNOBS];
static const struct tp_gen *gens[MAXGENS];
static s32 ngens;
static u8 gens_ok;
static const struct tp_gen *volatile tp_on[8];  /* each track's generator, 0 none */
static u8 tp_ix[8];                             /* its place in gens, + 1 (0: OFF) */

static void copy(void *d, const void *s, u32 n)
{
    u8 *x = (u8 *)d;
    const u8 *y = (const u8 *)s;
    while (n--)
        *x++ = *y++;
}

static int same(const void *a, const void *b, u32 n)
{
    const u8 *x = (const u8 *)a, *y = (const u8 *)b;
    while (n--)
        if (*x++ != *y++)
            return 0;
    return 1;
}

static void gens_init(void)
{
    tp_gen_fn *h;
    if (gens_ok)
        return;
    for (h = tp_gens; *h && ngens < MAXGENS; h++) {
        const struct tp_gen *g = (*h)();
        if (g && g->abi == TP_ABI)
            gens[ngens++] = g;
    }
    gens_ok = 1;
}

/* A track's generator: gens[ix - 1], 0 OFF; its knobs to their defaults unless keep. The step fire reads
 * tp_on at interrupt level: it is cleared while the values change. */
static void set_gen(int t, int ix, int keep)
{
    const struct tp_gen *g = ix > 0 && ix <= ngens ? gens[ix - 1] : 0;
    int k;
    tp_on[t] = 0;
    tp_ix[t] = (u8)(g ? ix : 0);
    if (g && !keep)
        for (k = 0; k < TP_KNOBS; k++)
            tp_val[t][k] = g->knob[k].name ? g->knob[k].def : 0;
    tp_on[t] = g;
}

/* ---- drawing ----------------------------------------------------------------------------------------------- */

void tp_text(void *bmp, const void *font, s32 x, s32 y, s32 align, const char *s)
{
    TEXTBOX(bmp, font, x, y, align, 0, s, "%s", s);
}

void tp_ring(void *bmp, s32 x, s32 y, s32 value, s32 max)
{
    u32 frames = *(const u32 *)FW_KNOB_SET;     /* the stock ring's frames, 0x1c bytes each: 12 the lowest,
                                                 * 84 the highest a knob shows */
    s32 f = 12 + (max > 0 ? (value * 72 + max / 2) / max : 0);
    if (!frames)
        return;
    fw_blit(bmp, (const void *)(frames + 0x1c * (u32)f), x, y + 7, 0);
}

static char *put_num(char *d, u32 v)
{
    char tmp[10];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *d++ = tmp[--n];
    *d = 0;
    return d;
}

static char *put_str(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}

/* knob k's value text (k 0 GEN, 1-7 the generator's B-H) */
static void value_text(char *buf, int t, int k, int lng)
{
    const struct tp_gen *g = tp_on[t];
    buf[0] = 0;
    if (k == 0) {
        put_str(buf, g ? (lng ? g->lname : g->name) : (lng ? "Off" : "OFF"));
        return;
    }
    if (!g || !g->knob[k - 1].name)
        return;
    if (g->knob[k - 1].text)
        g->knob[k - 1].text(buf, tp_val[t][k - 1], t, lng);
    else
        put_num(buf, tp_val[t][k - 1]);
}

static s32 touch_k = -1, touch_n;               /* the knob turned last, and the UI ticks it shows its value */
static u8 tp_live, tp_track, tp_sub;            /* frames sub-page 2 or 3 still shows, of which track */
static s32 knob_state[64];

#define KNOB_INIT       ((void (*)(void *))FW_KNOB_INIT)
#define KNOB_TICK       ((void (*)(void *, void *))FW_KNOB_TICK)
#define KNOB_FILTER     ((s32 (*)(void *, const void *, const void *))FW_KNOB_FILTER)
#define KNOB_CORE       ((s32 (*)(void *, s32, s32, const void *, s32))FW_KNOB_CORE)
#define KNOB_NOTCH      ((const void *)FW_KNOB_NOTCH)

/* TRIG 2/3: GEN and the generator's knobs, in the stock cells (x 24, 50, 76, 102; y 27 and 0): the
 * graphic from y + 7, the label under it, which shows the value while the knob turns. */
static void knobs_draw(void *bmp, int t)
{
    const struct tp_gen *g = tp_on[t];
    char buf[24];
    int k;
    for (k = 0; k < 8; k++) {
        s32 x = 24 + 26 * (k & 3), y = k < 4 ? 27 : 0;
        const struct tp_knob *kn = k && g ? &g->knob[k - 1] : 0;
        const char *lab;
        if (k == 0) {
            if (g && g->icon)
                g->icon(bmp, x, y, t);
            else
                tp_ring(bmp, x, y, tp_ix[t], ngens > 0 ? ngens : 1);
            lab = "GEN";
        } else if (!kn || !kn->name) {
            fw_blit(bmp, (const void *)FW_NO_KNOB, x + 1, y + 7, 0);    /* the stock's knob without a parameter */
            continue;
        } else {
            s32 v = tp_val[t][k - 1];
            if (!kn->draw || !kn->draw(bmp, x, y, v, t))
                tp_ring(bmp, x, y, v, kn->max);
            lab = kn->name;
        }
        if (k == touch_k && touch_n > 0) {
            value_text(buf, t, k, 0);
            lab = buf;
        }
        tp_text(bmp, tp_font_label, x + 9, y, 1, lab);
    }
}

static u32 tp_pages[3] = { PAGE_TRIG, PAGE_TRIG, PAGE_TRIG };

/* The TRIG page's draw (its vtable's slot 4). A MIDI track's TRIG page (its list another id) is left as
 * it is. */
void tp_draw(void *view, void *bmp)
{
    u32 *b = PG_BEGIN(view);
    s32 t, n, sub;
    if (!b || (b != tp_pages && (PG_END(view) - b != 1 || b[0] != PAGE_TRIG))) {
        TRIG_DRAW(view, bmp);
        return;
    }
    gens_init();
    t = TRACK_OF(*(void **)((u8 *)view + 116));
    if (t < 0 || t > 7) {
        TRIG_DRAW(view, bmp);
        return;
    }
    n = tp_on[t] ? 3 : 2;
    PG_BEGIN(view) = tp_pages;
    PG_END(view) = tp_pages + n;
    PG_CAP(view) = tp_pages + 3;
    sub = SUBPAGE(view);
    if (sub >= n) {                             /* 3/3 on a track without a generator: 2/2 */
        sub = n - 1;
        SUBPAGE(view) = sub;
    }
    if (sub <= 0) {
        TRIG_DRAW(view, bmp);
        return;
    }
    if (!tp_live || tp_track != (u8)t || tp_sub != (u8)sub) {
        KNOB_INIT(knob_state);                  /* shown afresh: the knob filter afresh */
        touch_k = -1;
    }
    tp_live = 3;
    tp_track = (u8)t;
    tp_sub = (u8)sub;
    fw_fillrect(bmp, 17, 0, 127, 52, 0);
    if (sub == 1)
        knobs_draw(bmp, (int)t);
    else if (tp_on[t]->view)
        tp_on[t]->view(bmp, t);
    ((void (*)(void *, void *, s32))VIEW_VT(view)[LEVEL_DRAW])(view, bmp, 0);
}

/* ev_draw: while a knob turns on 2/3 or 3/3, the header shows its long name and value, as the stock
 * pages' pop-up does: an inverted bar right of the pattern's box (x 17-127, y 54-63). */
void tp_overlay(void *bmp, void *ctrl)
{
    char buf[48], *d;
    const struct tp_gen *g;
    (void)ctrl;
    if (!tp_live || touch_k < 0 || touch_n <= 0)
        return;
    g = tp_on[tp_track];
    if (touch_k == 0)
        d = put_str(buf, "Generator");
    else if (g && g->knob[touch_k - 1].lname)
        d = put_str(buf, g->knob[touch_k - 1].lname);
    else
        return;
    d = put_str(d, "=");
    value_text(d, tp_track, touch_k, 1);
    fw_fillrect(bmp, 17, 54, 127, 63, 0);
    tp_text(bmp, tp_font_pop, 19, 56, 0, buf);
    fw_fillrect(bmp, 17, 54, 127, 63, -1);
    fw_fillrect(bmp, 17, 54, 17, 54, 0);        /* its corners round, as the stock bar's */
    fw_fillrect(bmp, 17, 63, 17, 63, 0);
    fw_fillrect(bmp, 127, 54, 127, 54, 0);
    fw_fillrect(bmp, 127, 63, 127, 63, 0);
}

/* ---- the knobs on 2/3 and 3/3 -------------------------------------------------------------------------------- */

#define EV_KEY(ev)      (*(const u32 *)((const char *)(ev) + 12))
#define EV_COUNTS(ev)   (*(const s32 *)((const char *)(ev) + 16))
#define SHORT_LIST      24                      /* a knob of this many values or fewer steps a notch at a time */

/* ev_enc: knobs A-H while 2/3 or 3/3 shows: A the generator, B-H its knobs. */
int tp_enc(void *brain, void *ev)
{
    s32 kn = (s32)EV_KEY(ev) - 1, steps, v, max;
    int t = tp_track;
    const struct tp_gen *g = tp_on[t];
    (void)brain;
    if (!tp_live || kn < 0 || kn > 7)
        return 0;
    if (kn == 0) {
        max = ngens;
        v = tp_ix[t];
    } else {
        if (!g || !g->knob[kn - 1].name)
            return 1;
        max = g->knob[kn - 1].max;
        v = tp_val[t][kn - 1];
    }
    steps = max <= SHORT_LIST ? KNOB_CORE(knob_state, kn, EV_COUNTS(ev), KNOB_NOTCH, 0)
                              : KNOB_FILTER(knob_state, ev, KNOB_NOTCH);
    touch_k = kn;
    touch_n = 45;                               /* a second and a half */
    if (!steps)
        return 1;
    v += steps;
    v = v < 0 ? 0 : v > max ? max : v;
    if (kn == 0) {
        if (v != tp_ix[t])
            set_gen(t, (int)v, 0);
    } else {
        tp_val[t][kn - 1] = (u8)v;
    }
    return 1;
}

/* ---- the settings on the +Drive ------------------------------------------------------------------------------ */

/* The firmware's ekFS calls, as duck, CUT and WAVE use them: from the UI task, only while the filesystem's
 * mutex is free and the drive mounted. Two files written in turn, so a save cut short leaves the other
 * whole; the load takes the valid one with the higher seq. A generator is kept by its id, so a build with
 * other generators finds it, or OFF. */
typedef struct { u32 inode, pos, writable, open; } ekfile;
#define EK_OPEN    ((s32 (*)(const char *, const char *, ekfile *))FW_EK_OPEN)
#define EK_READ    ((s32 (*)(void *, s32, ekfile *))FW_EK_READ)
#define EK_WRITE   ((s32 (*)(const void *, s32, ekfile *))FW_EK_WRITE)
#define EK_CLOSE   ((s32 (*)(ekfile *))FW_EK_CLOSE)
#define EK_MKDIR   ((s32 (*)(const char *))FW_EK_MKDIR)
#define EK_LOOKUP  ((s32 (*)(const char *, u32 *, s32, s32, s32))FW_EK_LOOKUP)
#define EK_LOCK    ((void (*)(void *))FW_EK_LOCK)
#define EK_UNLOCK  ((void (*)(void *))FW_EK_UNLOCK)
#define EK_MUTEX   ((void *)FW_EK_MUTEX)
#define EK_OWNER   (*(volatile u32 *)FW_EK_MUTEX)
#define EK_MOUNTED (*(volatile u32 *)FW_EK_MOUNTED)
#define EK_NOENT   0x19

#define ST_MAGIC   0x54525031u                  /* "TRP1" */
struct st_set {
    u32 id[8];                                  /* each track's generator, 0 OFF */
    u8 val[8][TP_KNOBS];
};
struct st_file {
    u32 magic, seq, sum;
    struct st_set s;
    u8 pad[16];                                 /* a USB listing reports size - 16 */
};
static const char *const st_path[2] = { "/cfw/trigp.b", "/cfw/trigp.a" };
static struct st_file st_img;                   /* the last saved (or loaded) */
static struct st_set st_seen;
static u8 st_loaded;
static s32 st_wait;

static u32 st_sum(const struct st_set *s)
{
    const u8 *b = (const u8 *)s;
    u32 h = 0x5eed1234u, i;
    for (i = 0; i < sizeof *s; i++)
        h = ((h << 5) | (h >> 27)) ^ b[i];
    return h;
}

static void st_now(struct st_set *s)
{
    int t, k;
    for (t = 0; t < 8; t++) {
        s->id[t] = tp_on[t] ? tp_on[t]->id : 0;
        for (k = 0; k < TP_KNOBS; k++)
            s->val[t][k] = tp_val[t][k];
    }
}

static int ek_ready(void)
{
    return EK_MOUNTED != 0 && EK_OWNER == 0;
}

static int st_read(const char *path, struct st_file *f)
{
    ekfile h;
    s32 got;
    if (EK_OPEN(path, "r", &h) != 0)
        return 0;
    got = EK_READ(f, sizeof *f, &h);
    EK_CLOSE(&h);
    return got >= (s32)(sizeof *f - sizeof f->pad) && f->magic == ST_MAGIC && f->sum == st_sum(&f->s);
}

static void st_load(void)
{
    static struct st_file f0;
    int ok0, ok1, t, k, i;
    if (!ek_ready())
        return;
    ok0 = st_read(st_path[0], &f0);
    ok1 = st_read(st_path[1], &st_img);
    if (ok0 && (!ok1 || f0.seq > st_img.seq))
        copy(&st_img, &f0, sizeof f0);
    if (ok0 || ok1) {
        for (t = 0; t < 8; t++) {
            const struct tp_gen *g = 0;
            for (i = 0; i < ngens; i++)
                if (st_img.s.id[t] && gens[i]->id == st_img.s.id[t])
                    break;
            if (i < ngens)
                g = gens[i];
            tp_on[t] = 0;
            for (k = 0; k < TP_KNOBS; k++) {
                u8 v = st_img.s.val[t][k];
                tp_val[t][k] = g && g->knob[k].name ? (v <= g->knob[k].max ? v : g->knob[k].def) : 0;
            }
            set_gen(t, g ? i + 1 : 0, 1);
        }
    } else {
        st_img.seq = 0;
    }
    st_now(&st_img.s);
    st_now(&st_seen);
    st_loaded = 1;
}

static int st_save(void)
{
    ekfile h;
    struct st_file f;
    u32 ino;
    s32 r, w;
    if (!ek_ready())
        return 0;
    EK_LOCK(EK_MUTEX);
    r = EK_LOOKUP("/cfw", &ino, 0, 0, 0);
    EK_UNLOCK(EK_MUTEX);
    if (r == EK_NOENT && EK_MKDIR("/cfw") != 0)
        return -1;
    copy(&f, &st_img, sizeof f);
    f.magic = ST_MAGIC;
    f.seq = st_img.seq + 1;
    st_now(&f.s);
    f.sum = st_sum(&f.s);
    if (EK_OPEN(st_path[f.seq & 1], "w", &h) != 0)
        return -1;
    w = EK_WRITE(&f, sizeof f, &h);
    EK_CLOSE(&h);
    if (w != (s32)sizeof f)
        return -1;
    copy(&st_img, &f, sizeof f);
    return 1;
}

/* Every UI tick: load once the drive is ready; save a second after the last change. */
static void st_tick(void)
{
    struct st_set now;
    if (!st_loaded) {
        st_load();
        return;
    }
    st_now(&now);
    if (!same(&now, &st_seen, sizeof now)) {
        copy(&st_seen, &now, sizeof now);
        st_wait = 30;
        return;
    }
    if (same(&now, &st_img.s, sizeof now))
        return;
    if (st_wait > 0) {
        st_wait--;
        return;
    }
    if (st_save() < 0)
        st_wait = 300;                          /* failed: again in 10 s */
}

/* ev_tick: the generators found, the settings kept, each generator's tick; while 2/3 or 3/3 shows, the
 * knob filter ticked and the screen redrawn every frame. */
void tp_tick(void *ctrl)
{
    int t;
    gens_init();
    st_tick();
    for (t = 0; t < 8; t++) {
        const struct tp_gen *g = tp_on[t];
        if (g && g->tick)
            g->tick(t);
    }
    if (touch_n > 0)
        touch_n--;
    if (tp_live) {
        tp_live--;
        KNOB_TICK(knob_state, 0);               /* stock ticks its knob filters at 100 Hz: 3 a UI tick */
        KNOB_TICK(knob_state, 0);
        KNOB_TICK(knob_state, 0);
        *((u8 *)ctrl + 0x20) = 1;
    }
}

/* ---- the step fire (seq-events), interrupt level: the track's generator --------------------------------------- */

void tp_trig(s32 track, s32 step, s32 first, u32 *trig)
{
    const struct tp_gen *g = track < 8 ? tp_on[track] : 0;
    if (g && g->trig)
        g->trig(track, step, first, trig);
}

void tp_notes(s32 track, s32 step, s32 generated, int8_t notes[SQ_NOTES])
{
    const struct tp_gen *g = track < 8 ? tp_on[track] : 0;
    if (g && g->notes)
        g->notes(track, step, generated, notes);
}

void tp_vel(s32 track, s32 step, s32 *vel)
{
    const struct tp_gen *g = track < 8 ? tp_on[track] : 0;
    if (g && g->vel)
        g->vel(track, step, vel);
}

s32 tp_locks(s32 track, s32 step, struct sq_lock *out, s32 room)
{
    const struct tp_gen *g = track < 8 ? tp_on[track] : 0;
    return g && g->locks ? g->locks(track, step, out, room) : 0;
}
