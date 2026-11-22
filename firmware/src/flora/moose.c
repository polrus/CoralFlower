/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sornyak contributors */
/* The torch (FL_SP_PLANT, the Top algorithms), included by flora.c after sornyak.c: a starry night over a round green
 * hill, the small yellow moose sitting on top with a torch, its beam swinging slowly up into the sky, moths drifting up
 * the beam. Round the beam, a loose cloud of butterflies in three depth layers (bf_cloud below), reaching further up
 * it and fuller for every recorded track of the pattern playing (flora_set_tracks). They follow the sound: louder,
 * they circle faster and the beam grows brighter; their wings beat on the tempo and a note flashes some. When it is
 * quiet they rest on the hill and the moose, and the beam dims.
 * Drawn straight onto the canvas: integer maths, no allocation. */

#define TQ_NRING 5
static const uint8_t TQ_RING_COL[TQ_NRING][2] = {     /* each ring's wings, light and dark */
    {FL_C(FL_R_SAND, 0), FL_C(FL_R_SAND, 1)},  {FL_C(FL_R_SEA, 0), FL_C(FL_R_SEA, 1)},
    {FL_C(FL_R_ORCHID, 0), FL_C(FL_R_ORCHID, 1)}, {FL_C(FL_R_FIRE, 0), FL_C(FL_R_FIRE, 1)},
    {FL_C(FL_R_LIME, 0), FL_C(FL_R_LIME, 1)}};

static int tq_w(const flora_t *f) { return f->cx * 2; }
static int tq_h(const flora_t *f) { return f->cy - 1; }
static int tq_hill(const flora_t *f, int x)      /* the hill's top edge at x: round, its top right of the middle */
{
    int d = x - tq_w(f) * 2 / 3;
    return tq_h(f) - 14 + d * d / 520;
}
static void tq_seat(const flora_t *f, int *mx, int *my)   /* the moose's middle, on the hill's top */
{
    *mx = tq_w(f) * 2 / 3;
    *my = tq_hill(f, *mx) - 5;
}
static void tq_torch(const flora_t *f, int *ox, int *oy)  /* the torch's lit end */
{
    int mx, my;
    tq_seat(f, &mx, &my);
    *ox = mx - 8;                                /* (in its raised left paw) */
    *oy = my - 6;
}
static int32_t tq_beam_angle(const flora_t *f)           /* up and to the left, swinging slowly (1024 a turn) */
{
    return 768 - 40 + (fl_fsin((int32_t)(f->t / 12u)) * 26 >> 14) + (fl_fsin((int32_t)(f->t / 5u)) * (sk_lvl(f) / 40) >> 14);
}

static void tq_tick(flora_t *f, uint32_t dt)
{
    uint32_t lvl = (uint32_t)sk_lvl(f);
    f->tq_phase += dt * (40u + lvl / 2u);        /* the rings turn, faster when it is loud */
    f->tq_flap += dt * (200u + lvl * 3u);        /* the wings beat */
}

/* the night: dark up, a little lighter down; many stars, twinkling */
static void tq_sky(flora_t *f, flc_t *c)
{
    int W = c->w, H = c->h, x, y, i;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            c->px[y * W + x] = fl_grad(SK_SKY, 4, 40 + y * 300 / H, x, y);
    for (i = 0; i < 70; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 2246822519u + 7u);
        int sx = (int)(h % (uint32_t)W), sy = (int)((h >> 8) % (uint32_t)(H - 14)), tw = (int)(((f->t >> 6) + h) % 64u);
        uint8_t col = tw < 3 ? FL_INK : (h >> 20) % 3u == 0u ? FL_C(FL_R_SAND, 0) : FL_C(FL_R_SAGE, 1);
        if (tw < 1 && ((h >> 24) & 1u))
            continue;                            /* (now and then one goes out for a moment) */
        c->px[sy * W + sx] = col;
    }
}

/* the beam: a pale yellow wedge from the torch to the edge of the sky, dithered so the stars show through; brighter
 * (denser) when it is loud */
static void tq_beam(flora_t *f, flc_t *c)
{
    int W = c->w, H = c->h, x, y, ox, oy;
    int32_t a = tq_beam_angle(f), dx = fl_fcos(a), dy = fl_fsin(a);   /* Q14 */
    int32_t dens = 3 + sk_aw(f) * 3 / 255 + sk_lvl(f) / 28;   /* of 16 (dim when it is quiet) */
    tq_torch(f, &ox, &oy);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int32_t vx = x - ox, vy = y - oy, along = (vx * dx + vy * dy) >> 14, perp = (vx * dy - vy * dx) >> 14;
            int32_t half = 1 + along / 7, ap = perp < 0 ? -perp : perp, b = fl_bayer[(y & 3) * 4 + (x & 3)];
            if (along <= 0 || ap > half)
                continue;
            if (ap * 3 < half) {                 /* its core */
                if (b < dens + 2)
                    c->px[y * W + x] = FL_C(FL_R_SAND, 0);
            } else if (b < dens - 2) {
                c->px[y * W + x] = FL_C(FL_R_SAND, 1);
            }
        }
}

/* spruces on the left, far, and a few on the right */
static void tq_trees(flc_t *c)
{
    static const int8_t TX[7] = {2, 9, 16, 24, 31, 108, 116};
    int i, y, H = c->h;
    for (i = 0; i < 7; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 7907u + 41u);
        int top = H - 26 - (int)(h % 8u), base = H - 8;
        for (y = top; y <= base; y++) {
            int hw = (y - top) * 2 / 5 - (((y - top) & 3) == 3), x;
            for (x = -hw; x <= hw; x++)
                fl_put(c, TX[i] + x, y, FL_S0);
        }
    }
}

/* the hill: green, darker down, with darker spots and tufts of grass on its edge */
static void tq_ground(flora_t *f, flc_t *c)
{
    static const uint8_t HILL[4] = {FL_C(FL_R_JADE, 2), FL_C(FL_R_LIME, 2), FL_C(FL_R_LIME, 3), FL_C(FL_R_JADE, 3)};
    int W = c->w, H = c->h, x, y, i;
    for (x = 0; x < W; x++) {
        int top = tq_hill(f, x);
        for (y = top < 0 ? 0 : top; y < H; y++) {
            uint32_t h = fl_hash32((uint32_t)(x / 3) * 131u + (uint32_t)(y / 2) * 7919u);
            uint8_t col = fl_grad(HILL, 4, (y - top) * 60, x, y);
            if (y > top + 1 && (h & 31u) == 0u)
                col = FL_C(FL_R_JADE, 3);        /* (spots) */
            c->px[y * W + x] = y == top ? FL_C(FL_R_JADE, 1) : col;
        }
    }
    for (i = 0; i < 14; i++) {                   /* tufts on the edge */
        uint32_t h = fl_hash32((uint32_t)i * 104729u + 3u);
        int gx = (int)(h % (uint32_t)W), gy = tq_hill(f, gx);
        fl_put(c, gx, gy - 1, FL_C(FL_R_JADE, 1));
        if (h & 0x100u)
            fl_put(c, gx + 1, gy - 2, FL_C(FL_R_JADE, 1));
    }
}

/* the torch in the moose's paw: a small dark body, its lit end */
static void tq_torch_draw(flora_t *f, flc_t *c)
{
    int ox, oy;
    tq_torch(f, &ox, &oy);
    fl_put(c, ox + 1, oy + 1, FL_C(FL_R_SAGE, 3));
    fl_put(c, ox + 1, oy + 2, FL_C(FL_R_SAGE, 2));
    fl_put(c, ox, oy, FL_INK);
}

/* a butterfly at (x, y): wings open or folded as the beat of its wings goes; lit: its note just played */
static void tq_fly(flc_t *c, int x, int y, int open, uint8_t c0, uint8_t c1, int lit)
{
    uint8_t w = lit ? FL_INK : c0, b = FL_C(FL_R_SAND, 3);
    if (open) {                                  /* four wings, the upper ones bigger */
        fl_put(c, x - 2, y - 1, w), fl_put(c, x - 1, y - 1, w), fl_put(c, x + 1, y - 1, w), fl_put(c, x + 2, y - 1, w);
        fl_put(c, x - 1, y, c1), fl_put(c, x + 1, y, c1);
    } else {                                     /* folded up over its back */
        fl_put(c, x - 1, y - 2, w), fl_put(c, x + 1, y - 2, w);
        fl_put(c, x - 1, y - 1, c1), fl_put(c, x + 1, y - 1, c1);
    }
    fl_put(c, x, y - 1, b), fl_put(c, x, y, b);  /* (its body) */
}

/* ---- the butterflies: a loose cloud in three depth layers (both scenes) ---------------------------------------- */
/* Not rings: each butterfly has an orbit of its own about the cloud's axis (the beam; the Sornyak's top): its own
 * radius, tilt, height on the axis and pace, most one way round, some the other. They go in small clusters (up to three
 * share a phase), breathe in and out, and now and then one loops away and comes back. Three layers: far ones are
 * specks behind everything, the middle ones the usual size (their far half behind, darker), near ones bigger and
 * brighter, in front of everything. The reverb loosens the cloud. Their wings beat twice a beat (the near ones
 * together), the cloud lifts on the bar's first beat, and a note flashes some. The torch's land round the moose
 * when it is quiet and lift off, one by one, when the music starts. */
enum { BF_FAR = 0, BF_MID, BF_NEAR };
typedef struct {
    int ox, oy;                /* where the cloud's axis starts */
    int32_t ax, ay;            /* .. its direction, Q14 */
    int lo, hi;                /* how far along the axis they spread, px */
    int rad;                   /* their orbits' radius, px (each its own, about it) */
    int n;                     /* how many */
    uint32_t seed;
    int rest;                  /* 1: they land when it is quiet (the torch) */
    const uint8_t (*col)[2];   /* the colours (light, dark) */
    int ncol;
} bf_cloud_t;

static int bf_layer(uint32_t h) { uint32_t v = h % 10u; return v < 4u ? BF_FAR : v < 8u ? BF_MID : BF_NEAR; }

static void bf_speck(flc_t *c, int x, int y, int open, uint8_t col)   /* a far one: a speck, its wings a flicker */
{
    fl_put(c, x, y, col);
    if (open)
        fl_put(c, x - 1, y - 1, col), fl_put(c, x + 1, y - 1, col);
}
static void bf_big(flc_t *c, int x, int y, int open, uint8_t c0, uint8_t c1, int lit)   /* a near one */
{
    uint8_t w = lit ? FL_INK : c0, b = FL_C(FL_R_SAND, 3);
    if (open) {
        fl_put(c, x - 2, y - 2, w), fl_put(c, x + 2, y - 2, w);
        fl_put(c, x - 3, y - 1, w), fl_put(c, x - 2, y - 1, w), fl_put(c, x - 1, y - 1, w);
        fl_put(c, x + 1, y - 1, w), fl_put(c, x + 2, y - 1, w), fl_put(c, x + 3, y - 1, w);
        fl_put(c, x - 2, y, c1), fl_put(c, x - 1, y, c1), fl_put(c, x + 1, y, c1), fl_put(c, x + 2, y, c1);
    } else {
        fl_put(c, x - 1, y - 3, w), fl_put(c, x + 1, y - 3, w), fl_put(c, x - 1, y - 2, w), fl_put(c, x + 1, y - 2, w);
        fl_put(c, x - 1, y - 1, c1), fl_put(c, x + 1, y - 1, c1);
    }
    fl_put(c, x, y - 1, b), fl_put(c, x, y, b);
}

/* where one rests when it is quiet: on the hill round the moose, a few on the moose's back */
static void tq_rest_at(const flora_t *f, uint32_t h, int *x, int *y)
{
    int mx, my;
    tq_seat(f, &mx, &my);
    if ((h >> 3) % 6u == 0u) {
        *x = mx - 3 + (int)((h >> 7) % 7u);
        *y = my - 5;
        return;
    }
    *x = mx - 44 + (int)((h >> 7) % 80u);
    if (*x > tq_w(f) - 3)
        *x -= 80;
    *y = tq_hill(f, *x) - 1;
}

/* pass 0: what is behind (the far layer, the middle one's far half); pass 1: what is in front */
static void bf_cloud(flora_t *f, flc_t *c, const bf_cloud_t *b, int pass)
{
    int i, aw = sk_aw(f), lvl = sk_lvl(f), loose = f->reverb / 40;
    int lift = aw > 128 && f->sk_pos < 160u;     /* (the bar's first beat) */
    for (i = 0; i < b->n; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 2654435761u + b->seed), h2 = fl_hash32(h + 0x9E37u);
        uint32_t hg = fl_hash32((uint32_t)(i / 3) * 40503u + b->seed);   /* (its cluster) */
        int L = bf_layer(h), dir = (hg >> 5) % 4u ? 1 : -1, k = (int)(h2 % (uint32_t)b->ncol);
        uint32_t spd = 100u + hg % 140u + (uint32_t)L * 40u;
        int32_t a = (int32_t)(((f->tq_phase >> 8) * spd) >> 7) * dir + (int32_t)(hg >> 20) + (int32_t)(h % 90u);
        int32_t co = fl_fcos(a), si = fl_fsin(a);
        int r = b->rad * (55 + (int)(h2 % 75u)) / 100, ry = 2 + (int)((h2 >> 24) % 5u), x, y, front, open, lit, pc;
        int along = b->lo + (int)((h >> 8) % (uint32_t)(b->hi - b->lo + 1));
        uint32_t cyc = 9000u + h2 % 7000u, ph = (f->t + h * 13u) % cyc;
        r += (int)((fl_fsin((int32_t)(f->t / (9u + h2 % 7u)) + (int32_t)(h2 >> 10)) * (2 + loose)) >> 14);   /* (breathing) */
        if (ph < 1500u)                          /* (a loop away and back) */
            r += (int)((fl_fsin((int32_t)(ph * 512u / 1500u)) * (b->rad / 2 + 4)) >> 14);
        along += (int)((fl_fsin((int32_t)(f->t / 13u) + (int32_t)(h & 1023u)) * (3 + loose)) >> 14);
        x = b->ox + (int)((b->ax * along) >> 14) + (int)((((-b->ay * co) >> 14) * r + ((b->ax * si) >> 14) * ry) >> 14);
        y = b->oy + (int)((b->ay * along) >> 14) + (int)((((b->ax * co) >> 14) * r + ((b->ay * si) >> 14) * ry) >> 14);
        y -= lift * (L == BF_NEAR ? 2 : 1);
        front = si >= 0;
        pc = (int)(h % 12u);
        lit = f->pulse_on[pc] && f->t - f->pulse_t[pc] < 300u;
        if (aw > 64) {                           /* wings: twice a beat, the near ones together, the far ones loose */
            uint32_t off = L == BF_NEAR ? 0u : L == BF_MID ? (h % 3u) * 170u : h % 1024u;
            open = ((f->sk_pos * 2u + off) & 1023u) < 512u;
        } else {
            open = (int)(((f->tq_flap >> 7) + h) & 3u) < 2;
        }
        if (b->rest) {                           /* quiet: down to rest; music: up again, one by one */
            int d = (int)(h2 % 150u), e = fl_clampi((aw - d) * 255 / (255 - d), 0, 255);
            if (e < 255) {
                int rx, rq;
                tq_rest_at(f, h2, &rx, &rq);
                x = rx + (x - rx) * e / 255;
                y = rq + (y - rq) * e / 255 - (int)((fl_fsin(e * 2) * 6) >> 14);   /* (a hop up on the way) */
                if (e == 0)
                    open = ((f->t / 700u + h) % 9u) == 0u;   /* (resting: wings shut, opened now and then) */
                if (e < 128)
                    front = 1, L = BF_MID;
            }
        }
        if (pass != (L == BF_NEAR || (L == BF_MID && front)))
            continue;
        if (L == BF_FAR)
            bf_speck(c, x, y, open, lit ? FL_INK : b->col[k][1]);
        else if (L == BF_MID)
            tq_fly(c, x, y, open, front ? b->col[k][0] : b->col[k][1], front ? b->col[k][1] : FL_S3, lit);
        else
            bf_big(c, x, y, open, b->col[k][0], b->col[k][1], lit || lvl > 200);
    }
}

/* the torch's: round the beam, up it as far as the recorded tracks reach; more of them with more tracks, the reverb */
static void tq_cloud(flora_t *f, flc_t *c, int pass)
{
    bf_cloud_t b;
    int n = sk_popcount(f->tracks);
    tq_torch(f, &b.ox, &b.oy);
    {
        int32_t ba = tq_beam_angle(f);
        b.ax = fl_fcos(ba);
        b.ay = fl_fsin(ba);
    }
    b.lo = 6;
    b.hi = 12 + n * 7;
    b.rad = 20;
    b.n = 12 + (n * 6 + f->reverb / 24) * sk_aw(f) / 255;
    if (b.n > 38)
        b.n = 38;
    b.seed = 17u;
    b.rest = 1;
    b.col = TQ_RING_COL;
    b.ncol = TQ_NRING;
    bf_cloud(f, c, &b, pass);
}

/* the Sornyak's: they come while someone plays live over the pattern (the fifth instrument), one by one, and leave a
 * while after; a cloud round its crown and top tier */
static void sk_swarm(flora_t *f, flc_t *c, int pass)
{
    static const uint8_t COL[3][2] = {{FL_INK, FL_C(FL_R_SEA, 0)}, {FL_C(FL_R_SAND, 0), FL_C(FL_R_FIRE, 0)},
                                      {FL_C(FL_R_ORCHID, 0), FL_C(FL_R_ORCHID, 1)}};
    bf_cloud_t b;
    sk_crown(f, &b.ox, &b.oy);
    b.oy += 5;
    b.ax = 0;
    b.ay = -16384;                               /* (up) */
    b.lo = -8;
    b.hi = 8;
    b.rad = 20;
    b.n = (12 * f->sk_live + 65534) / 65535;
    b.seed = 91u;
    b.rest = 0;
    b.col = COL;
    b.ncol = 3;
    bf_cloud(f, c, &b, pass);
}

/* moths drifting up the beam, slowly, more when it is loud */
static void tq_moths(flora_t *f, flc_t *c)
{
    int ox, oy, i, m = 3 + sk_lvl(f) / 64;
    int32_t a = tq_beam_angle(f), dx = fl_fcos(a), dy = fl_fsin(a);
    tq_torch(f, &ox, &oy);
    for (i = 0; i < m; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 40503u + 9u), len = 90u;
        int32_t along = (int32_t)(((f->t / (24u + (h & 15u))) + h) % len) + 6;
        int32_t side = (fl_fsin((int32_t)(f->t >> 3) + (int32_t)(h & 1023)) * (1 + along / 10)) >> 14;
        int x = ox + (int)((dx * along - dy * side) >> 14), y = oy + (int)((dy * along + dx * side) >> 14);
        tq_fly(c, x, y, (int)(((f->tq_flap >> 7) + h) & 3u) < 2, FL_C(FL_R_SAND, 0), FL_C(FL_R_SAND, 1), 0);
    }
}

static void tq_render(flora_t *f, flc_t *c)
{
    int mx, my;
    tq_seat(f, &mx, &my);
    switch (f->backdrop) {
    case FL_BD_SCENE:
        tq_sky(f, c);
        tq_beam(f, c);
        tq_trees(c);
        tq_ground(f, c);
        break;
    case FL_BD_RIPPLES:
        tq_sky(f, c);
        fl_bd_rings(f, c, 1);
        tq_beam(f, c);
        tq_ground(f, c);
        break;
    default:
        flc_clear(c, FL_BG);
        tq_beam(f, c);
        break;
    }
    tq_moths(f, c);
    tq_cloud(f, c, 0);
    sk_moose_at(f, c, mx, my, -1, 2);
    tq_torch_draw(f, c);
    tq_cloud(f, c, 1);
}
