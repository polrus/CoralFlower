/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sornyak contributors */
/* The torch (FL_SP_PLANT, the Top algorithms), included by flora.c after sornyak.c: a starry night over a round green
 * hill, the small yellow moose sitting on top with a torch, its beam swinging slowly up into the sky, moths drifting up
 * the beam. Around the moose, rings of butterflies circle: one ring for nothing recorded, one more for every track of
 * the pattern playing that has something in it (flora_set_tracks), up to five. They follow the sound: louder, they
 * flap and circle faster and the beam grows brighter; every note makes one butterfly of each ring flash and jump.
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
    int32_t dens = 6 + sk_lvl(f) / 28;          /* of 16 */
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

/* the rings of butterflies round the light: each circles the beam like a halo threaded on it, the first low on the
 * beam, one more further up for every recorded track; they tilt with the beam as it swings and widen as it does.
 * Back halves (front = 0, darker) or front halves */
static void tq_rings(flora_t *f, flc_t *c, int front)
{
    int n = 1 + sk_popcount(f->tracks), k, i, ox, oy;
    int32_t ba = tq_beam_angle(f), bx = fl_fcos(ba), by = fl_fsin(ba);    /* along the beam, Q14 */
    tq_torch(f, &ox, &oy);
    for (k = 0; k < n && k < TQ_NRING; k++) {
        int along = 8 + k * 9 + ((fl_fsin((int32_t)(f->t >> 4) + k * 200) * 1) >> 14);   /* (each floats a little) */
        int rx = 15 + k * 2, ry = 5, cnt = 12 + k * 2 + f->reverb / 32, dir = k & 1 ? -1 : 1;   /* (more with reverb) */
        int cx = ox + (int)((bx * along) >> 14), cy = oy + (int)((by * along) >> 14);
        int32_t ph = (int32_t)(f->tq_phase / (uint32_t)(64 + k * 16)) * dir;
        for (i = 0; i < cnt; i++) {
            int32_t a = ph + i * 1024 / cnt, co = fl_fcos(a), si = fl_fsin(a);
            int pc = (k * 5 + i) % 12, lit = f->pulse_on[pc] && f->t - f->pulse_t[pc] < 300u, r = rx + (lit ? 2 : 0);
            /* across the beam (its perpendicular, -by, bx) by the ring's width, along it by its tilt */
            int x = cx + (int)(((-by * co >> 14) * r + (bx * si >> 14) * ry) >> 14);
            int y = cy + (int)(((bx * co >> 14) * r + (by * si >> 14) * ry) >> 14);
            int open = (int)(((f->tq_flap >> 7) + (uint32_t)(i * 3 + k)) & 3u) < 2;
            if ((si >= 0) != front)              /* (the near half in front, the far half behind, darker) */
                continue;
            tq_fly(c, x, y, open, front ? TQ_RING_COL[k][0] : TQ_RING_COL[k][1], front ? TQ_RING_COL[k][1] : FL_S3, lit);
        }
    }
}

/* the Sornyak's butterflies: dancing round it at every height of its stem, each on a circle of its own (its width,
 * its speed, its way round) with a loop up and down; a few to begin with, three more for every recorded track, and
 * many more with the reverb. They beat faster when it is loud; a note flashes some. The far side of a circle darker */
static void sk_swarm(flora_t *f, flc_t *c)
{
    int n = 3 + 3 * sk_popcount(f->tracks) + f->reverb / 4, i, hs = sk_height(f), g = sk_ground(f);
    if (n > 80)
        n = 80;
    for (i = 0; i < n; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 2654435761u + 17u);
        int hgt = 4 + (int)(h % (uint32_t)(hs + 6)), r = 7 + (int)((h >> 8) % 14u), dir = (h >> 20) & 1u ? 1 : -1;
        int32_t sp = 48 + (int32_t)((h >> 12) % 64u), a = (int32_t)(f->tq_phase / (uint32_t)sp) * dir + (int32_t)(h >> 22);
        int32_t co = fl_fcos(a), si = fl_fsin(a);
        int pc = (int)(h % 12u), lit = f->pulse_on[pc] && f->t - f->pulse_t[pc] < 300u;
        int x = sk_x(f, hgt > hs ? hs : hgt) + (int)((co * (r + (lit ? 2 : 0))) >> 14);
        int y = g - hgt + (int)((si * 3) >> 14) + (int)((fl_fsin(a * 2 + (int32_t)(h & 1023)) * 2) >> 14);
        int open = (int)(((f->tq_flap >> 7) + h) & 3u) < 2, k = (int)((h >> 16) % TQ_NRING);
        tq_fly(c, x, y, open, si >= 0 ? TQ_RING_COL[k][0] : TQ_RING_COL[k][1], si >= 0 ? TQ_RING_COL[k][1] : FL_S3, lit);
    }
}

/* with the reverb, more butterflies fluttering loose round the beam, each on a circle of its own about it, at its own
 * height up the light */
static void tq_free(flora_t *f, flc_t *c)
{
    int n = f->reverb / 5, i, ox, oy, top = 10 + sk_popcount(f->tracks) * 9;
    int32_t ba = tq_beam_angle(f), bx = fl_fcos(ba), by = fl_fsin(ba);
    tq_torch(f, &ox, &oy);
    for (i = 0; i < n; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 2246822519u + 101u);
        int along = 4 + (int)(h % (uint32_t)top), r = 6 + (int)((h >> 8) % 18u), dir = (h >> 20) & 1u ? 1 : -1;
        int32_t a = (int32_t)(f->tq_phase / (48u + (h >> 12) % 64u)) * dir + (int32_t)(h >> 22), co = fl_fcos(a), si = fl_fsin(a);
        int pc = (int)(h % 12u), lit = f->pulse_on[pc] && f->t - f->pulse_t[pc] < 300u, k = (int)((h >> 16) % TQ_NRING);
        int bob = (fl_fsin(a * 2 + (int32_t)(h & 1023)) * 2) >> 14;
        int x = ox + (int)((bx * along) >> 14) + (int)(((-by * co >> 14) * r + (bx * si >> 14) * 4) >> 14);
        int y = oy + (int)((by * along) >> 14) + (int)(((bx * co >> 14) * r + (by * si >> 14) * 4) >> 14) + bob;
        tq_fly(c, x, y, (int)(((f->tq_flap >> 7) + h) & 3u) < 2, si >= 0 ? TQ_RING_COL[k][0] : TQ_RING_COL[k][1],
               si >= 0 ? TQ_RING_COL[k][1] : FL_S3, lit);
    }
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
    tq_rings(f, c, 0);
    sk_moose_at(f, c, mx, my, -1, 2);
    tq_torch_draw(f, c);
    tq_rings(f, c, 1);
    tq_free(f, c);
}
