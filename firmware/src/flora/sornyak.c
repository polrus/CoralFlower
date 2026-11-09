/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sornyak contributors */
/* The Sornyak (FL_SP_SORNYAK, the Side algorithms): a tall weed on a night meadow, included by flora.c.
 *
 * A pale stem in tiers, a shelf cap at every tier with glowing buds on it, a spotted ball for a crown. It grows a
 * tier for every track of the pattern playing that has something in it (flora_set_tracks: Chords, Bass, Layer,
 * Drums); with all four the crown shines: a white core, a wide halo and light rays over the whole sky. The light
 * follows the sound: the rays and the halo breathe with the loudness, a strong hit flashes the sky, every note lights
 * a bud and sends a spark up. A small round yellow moose with headphones sits in the grass and listens: it bobs to
 * the beat, sings when it is loud, and raises its arms when the Sornyak shines.
 * Drawn straight onto the canvas (not from flora's segments): integer maths, no allocation. */

#define SK_TIER 9
static int abs_i(int v) { return v < 0 ? -v : v; }
static void sk_swarm(flora_t *f, flc_t *c);       /* (moose.c: the butterflies dancing round the Sornyak) */                                /* px between two caps */
static const uint8_t SK_SKY[5] = {FL_S0, FL_S1, FL_S2, FL_S3, FL_C(FL_R_SAGE, 0)};

static int sk_popcount(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1u)
        n++;
    return n;
}
static int sk_w(const flora_t *f) { return f->cx * 2; }          /* the canvas, from where the base was put */
static int sk_ground(const flora_t *f) { return f->cy - 9; }
static int sk_bx(const flora_t *f) { return sk_w(f) * 5 / 8; }
static int sk_height(const flora_t *f) { return f->sk_h >> 8; }   /* the stem now, px */
static int sk_lvl(const flora_t *f) { return (int)(f->level >> 8); }   /* 0..255 */
static int sk_shine(const flora_t *f) { return (int)(f->sk_shine >> 8); }

/* the stem's centre at height h over the ground: it leans left as it rises and sways with the sound */
static int sk_x(const flora_t *f, int h)
{
    int hs = sk_height(f) > 8 ? sk_height(f) : 8;
    int32_t q = (int32_t)h * h * 256 / (hs * hs);                 /* 0..256 up the stem */
    int32_t amp = 256 + sk_lvl(f) * 3;
    int32_t sway = (fl_fsin((int32_t)(f->t >> 3)) * amp >> 14) * q >> 8;
    return sk_bx(f) - (int)(q * 6 >> 8) + (int)(sway >> 8);
}
static int sk_crown_r(const flora_t *f) { return 3 + sk_popcount(f->tracks) / 2 + (sk_shine(f) > 160); }
static void sk_crown(const flora_t *f, int *x, int *y)
{
    int h = sk_height(f);
    *x = sk_x(f, h);
    *y = sk_ground(f) - h - sk_crown_r(f);
}
static int sk_ncaps(const flora_t *f) { return (sk_height(f) - 4) / SK_TIER; }
static void sk_cap_at(const flora_t *f, int k, int *x, int *y)   /* cap k, 1 = the lowest */
{
    int h = k * SK_TIER;
    *x = sk_x(f, h) + ((k & 1) ? 2 : -2);
    *y = sk_ground(f) - h;
}

/* ---- life ---------------------------------------------------------------------------------------------------- */
static void sk_tick(flora_t *f, uint32_t dt)
{
    int n = sk_popcount(f->tracks), target = 14 + n * SK_TIER, pc, cx, cy;
    f->sk_h = fl_approach(f->sk_h, target << 8, dt, 900);
    f->sk_shine = fl_approach(f->sk_shine, n >= 4 ? 65535 : 0, dt, 1200);
    f->sk_flash = f->sk_flash > (int32_t)dt * 3 ? f->sk_flash - (int32_t)dt * 3 : 0;
    sk_crown(f, &cx, &cy);
    for (pc = 0; pc < 12; pc++) {                /* a note: a spark up from its bud */
        int k, bx, by, nc = sk_ncaps(f);
        if (!f->pulse_on[pc] || f->sk_pz[pc] == f->pulse_n[pc])
            continue;
        f->sk_pz[pc] = f->pulse_n[pc];
        k = nc ? 1 + pc % nc : 0;
        if (k)
            sk_cap_at(f, k, &bx, &by);
        else
            bx = cx, by = cy;
        fl_spawn(f, (int32_t)bx << 8, (int32_t)(by - 2) << 8, (int)(fl_rnd(f) % 48) - 24, -60 - (int)(fl_rnd(f) % 40),
                 900 + (int)(fl_rnd(f) % 600), pc & 1 ? FL_C(FL_R_ORCHID, 0) : FL_C(FL_R_SEA, 0), P_MOTE);
    }
    /* fireflies about the crown: more when it is loud, a swarm when it shines */
    f->sk_acc += dt * (1u + (uint32_t)(sk_lvl(f) >> 5) + (uint32_t)(sk_shine(f) >> 4));
    while (f->sk_acc > 500u) {
        int r = 6 + sk_shine(f) / 12, ax = (int)(fl_rnd(f) % (uint32_t)(2 * r + 1)) - r,
            ay = (int)(fl_rnd(f) % (uint32_t)(2 * r + 1)) - r;
        static const uint8_t FIRE_COL[3] = {FL_C(FL_R_SAGE, 0), FL_C(FL_R_SEA, 0), FL_C(FL_R_FIRE, 0)};
        f->sk_acc -= 500u;
        fl_spawn(f, (int32_t)(cx + ax) << 8, (int32_t)(cy + ay) << 8, (int)(fl_rnd(f) % 40) - 20,
                 -(int)(fl_rnd(f) % 24) - 4, 1400 + (int)(fl_rnd(f) % 1600), FIRE_COL[fl_rnd(f) % 3u], P_MOTE);
    }
}

/* ---- the sky ------------------------------------------------------------------------------------------------- */
/* a direction as a quarter-turn-per-256 angle without division by a root (0..1023: a "diamond" angle) */
static int32_t sk_angle(int dx, int dy)
{
    if (!dx && !dy)
        return 0;
    if (dy >= 0)
        return dx >= 0 ? 256 * dy / (dx + dy) : 256 + 256 * -dx / (-dx + dy);
    return dx < 0 ? 512 + 256 * -dy / (-dx - dy) : 768 + 256 * dx / (dx - dy);
}

/* night, the crown's halo and its rays: the rays turn slowly, breathe with the loudness and reach the whole sky
 * when the Sornyak shines; a strong hit flashes it */
static void sk_sky(flora_t *f, flc_t *c, int rays)
{
    int W = c->w, H = c->h, x, y, cx, cy, lvl = sk_lvl(f), sh = sk_shine(f);
    /* the rays: hard-edged wedges a step brighter (two near the crown), as long as the light is strong */
    int32_t rot = (int32_t)(f->t / 70u), power = lvl / 3 + sh, reach = 20 + power / 2 + sh / 3;
    int32_t rg = 5 + lvl / 40 + sh / 16, rg2 = rg * rg, glow = 160 + sh + lvl / 2, flash = f->sk_flash >> 1;
    sk_crown(f, &cx, &cy);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int dx = x - cx, dy = y - cy;
            int32_t t = 20 + y * 200 / H + flash, d2 = dx * dx + dy * dy, r2 = reach * reach;
            if (rays && power > 24 && d2 < r2) {
                /* 9 wedges a turn, of three widths; brighter near the crown, dithered out at their ends */
                int32_t a = (sk_angle(dx, dy) + rot) & 1023, k = a * 9 >> 10, u = (a * 9) & 1023;
                int32_t wdt = 220 + (int32_t)(fl_hash32((uint32_t)k + 5u) % 260u);
                if (u < wdt) {
                    int32_t near = d2 * 3 < r2;
                    t += near ? 256 + 128 : d2 * 5 < r2 * 4 ? 256 : (fl_bayer[(y & 3) * 4 + (x & 3)] & 1) * 256;
                }
            }
            if (d2 < rg2)
                t += (rg2 - d2) * glow / rg2;
            c->px[y * W + x] = fl_grad(SK_SKY, 5, t, x, y);
        }
}

/* spruces along the far edge of the meadow, dark against the night */
static void sk_trees(flc_t *c, int ground)
{
    static const int8_t TX[11] = {1, 7, 13, 20, 27, 34, 88, 95, 101, 108, 115};
    int i, y;
    for (i = 0; i < 11; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 7907u + 3u);
        int top = ground - 9 - (int)(h % 9u), base = ground + 1;
        for (y = top; y <= base; y++) {
            int hw = (y - top) * 2 / 5 - (((y - top) & 3) == 3), x;
            for (x = -hw; x <= hw; x++)
                fl_put(c, TX[i] + x + (int)((h >> 8) & 1u), y, FL_S0);
        }
    }
}

/* the meadow: lit blue towards the Sornyak, grass blades and a few pale flowers */
static void sk_meadow(flora_t *f, flc_t *c, int ground)
{
    int W = c->w, H = c->h, x, y, i, bx = sk_bx(f), sh = sk_shine(f);
    for (x = 0; x < W; x++) {
        int g = ground + ((fl_fsin(x * 11 + 300) * 2) >> 14), dx = x - bx;
        int32_t lit = 300 - (dx < 0 ? -dx : dx) * 4 + sh / 2 + sk_lvl(f) / 4;
        for (y = g; y < H; y++)
            fl_put(c, x, y, fl_grad(SK_SKY, 4, fl_clampi(lit - (y - g) * 30, 0, 640), x, y));
    }
    for (i = 0; i < 46; i++) {                   /* grass, swaying a little */
        uint32_t h = fl_hash32((uint32_t)i * 104729u + 77u);
        int gx = (int)(h % (uint32_t)W), gy = ground + 1 + (int)((h >> 8) % (uint32_t)(H - ground)), k = 1 + (int)((h >> 12) % 3u);
        int lean = (fl_fsin((int32_t)(f->t >> 4) + (int32_t)(h & 1023)) * (1 + sk_lvl(f) / 120)) >> 14;
        for (y = 0; y < k; y++)
            fl_put(c, gx + (y == k - 1 ? lean : 0), gy - y, (h >> 20) & 1 ? FL_S3 : FL_C(FL_R_SAGE, 2));
    }
    for (i = 0; i < 5; i++) {                    /* pale flowers */
        uint32_t h = fl_hash32((uint32_t)i * 15485863u + 11u);
        fl_put(c, (int)(h % (uint32_t)W), ground + 2 + (int)((h >> 8) % (uint32_t)(H - ground - 2)), FL_C(FL_R_SAGE, 0));
    }
}

/* ---- the Sornyak --------------------------------------------------------------------------------------------- */
static uint8_t sk_bud_col(const flora_t *f, int pc, int j)
{
    if (f->pulse_on[pc] && f->t - f->pulse_t[pc] < 260u)
        return FL_INK;                           /* (its note: lit) */
    if (sk_shine(f) > 200 && ((f->t >> 8) + (uint32_t)j) % 5u == 0u)
        return FL_INK;                           /* (shining: they twinkle) */
    return j & 1 ? FL_C(FL_R_ORCHID, 0) : FL_C(FL_R_SEA, 0);
}

static void sk_stem(flora_t *f, flc_t *c)
{
    int hs = sk_height(f), g = sk_ground(f), h;
    for (h = 0; h <= hs; h++) {
        int x0 = sk_x(f, h), hw = 2 - h * 2 / (hs + 1), x;   /* 5 px at the foot, 3 at the top */
        for (x = -hw; x <= hw; x++)
            fl_put(c, x0 + x, g - h, x == hw ? FL_C(FL_R_SAGE, 2) : x > 0 ? FL_C(FL_R_SAGE, 1)
                                     : (x == 0 && ((h ^ x0) & 1)) ? FL_C(FL_R_SAGE, 1) : FL_C(FL_R_SAGE, 0));
    }
}

static void sk_caps(flora_t *f, flc_t *c)
{
    int k, n = sk_ncaps(f), hs = sk_height(f);
    for (k = 1; k <= n; k++) {
        int x0, y0, dx, j, grown = fl_clampi((hs - k * SK_TIER - 4) * 256 / 8, 0, 256);
        int a = (12 - k) * grown / 256;          /* the lower caps wider; a new one opens */
        if (a < 2)
            continue;
        sk_cap_at(f, k, &x0, &y0);
        for (dx = -a; dx <= a; dx++) {           /* a dome: lit on the left, the gills dark under it */
            int ax = abs_i(dx), top = ax <= a / 3 ? 3 : ax <= a * 2 / 3 ? 2 : ax < a ? 1 : 0, y;
            for (y = 1; y <= top; y++)
                fl_put(c, x0 + dx, y0 - y, y == top && dx < a / 3 ? FL_C(FL_R_SAGE, 0)
                                          : dx > a / 3 ? FL_C(FL_R_SAGE, 1) : FL_C(FL_R_SAGE, 0));
            fl_put(c, x0 + dx, y0, ax == a ? FL_C(FL_R_SAGE, 1) : (dx & 1) ? FL_C(FL_R_SAGE, 3) : FL_C(FL_R_SAGE, 2));
        }
        fl_put(c, x0 - a, y0 + 1, FL_C(FL_R_SAGE, 1));   /* (the rims droop) */
        fl_put(c, x0 + a, y0 + 1, FL_C(FL_R_SAGE, 2));
        for (j = 0; j < 3; j++) {                /* three buds on its top */
            int pc = (k * 3 + j) * 7 % 12, bx = x0 - a + 1 + j * (a - 1), ax = abs_i(bx - x0);
            int top = ax <= a / 3 ? 3 : ax <= a * 2 / 3 ? 2 : 1;
            uint8_t col = sk_bud_col(f, pc, k * 3 + j);
            fl_put(c, bx, y0 - top - 1, col);
            fl_put(c, bx + 1, y0 - top - 1, col);
            fl_put(c, bx, y0 - top, col);
        }
    }
}

static void sk_crown_draw(flora_t *f, flc_t *c)
{
    static const int8_t SPOT[5][2] = {{-1, -1}, {1, 0}, {-1, 1}, {2, -2}, {0, 2}};
    int cx, cy, r = sk_crown_r(f), dx, dy, i, sh = sk_shine(f), neck = sk_height(f);
    sk_crown(f, &cx, &cy);
    (void)neck;
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > r * r + r / 2)
                continue;
            fl_put(c, cx + dx, cy + dy, d2 > r * r - r ? FL_C(FL_R_SAGE, 2)
                                        : sh > 160 && d2 * 3 < r * r ? FL_INK
                                        : dx + dy > r / 2 ? FL_C(FL_R_SAGE, 1) : FL_C(FL_R_SAGE, 0));
        }
    for (i = 0; i < 5; i++)
        if (abs_i(SPOT[i][0]) < r && abs_i(SPOT[i][1]) < r)
            fl_put(c, cx + SPOT[i][0], cy + SPOT[i][1], i & 1 ? FL_C(FL_R_ORCHID, 0) : FL_C(FL_R_SEA, 0));
}

/* ---- the listener -------------------------------------------------------------------------------------------- */
/* a small round yellow moose with headphones, sitting with its back to us, at (mx, my) (its middle); it sways a
 * little with the sound. arms: 0 at its sides, 1 both up, 2 its left paw up (holding the torch: torch.c) */
static void sk_moose_at(flora_t *f, flc_t *c, int mx, int my, int look, int arms)
{
    int dx, dy, sway = (fl_fsin((int32_t)(f->t >> 2)) * (sk_lvl(f) > 60 ? 1 : 0)) >> 14;
    const uint8_t Y0 = FL_C(FL_R_FIRE, 0), Y1 = FL_C(FL_R_FIRE, 1), HI = FL_C(FL_R_SAND, 0), BR = FL_C(FL_R_PEACH, 2),
                  PH = FL_C(FL_R_ORCHID, 1), BAND = FL_C(FL_R_ORCHID, 2), SH = FL_C(FL_R_JADE, 3);
    static const int8_t ANT[6][2] = {{3, -5}, {4, -6}, {5, -7}, {3, -7}, {6, -6}, {4, -8}};   /* antlers: two forks */
    (void)look;
    for (dx = -5; dx <= 5; dx++)                 /* its shadow on the ground */
        fl_put(c, mx + dx, my + 5, SH);
    for (dx = 0; dx < 6; dx++) {
        fl_put(c, mx + sway - ANT[dx][0], my + ANT[dx][1], BR);
        fl_put(c, mx + sway + ANT[dx][0], my + ANT[dx][1], BR);
    }
    for (dy = -5; dy <= 4; dy++)                 /* the round back, sitting: a little wider at the bottom */
        for (dx = -6; dx <= 6; dx++) {
            int d2 = dx * dx + dy * dy, x = mx + dx + (dy < 0 ? sway : 0);
            if (d2 > (dy > 1 ? 36 : 27))
                continue;
            fl_put(c, x, my + dy, dx >= 4 && ((dx ^ dy) & 1) ? Y1 : dx <= -3 && dy <= -1 && d2 > 12 ? HI : Y0);
        }
    for (dx = -4; dx <= 4; dx++)                 /* the headphones' band over the top of its head */
        fl_put(c, mx + sway + dx, my - 5 - (dx > -3 && dx < 3), BAND);
    for (dy = -2; dy <= 0; dy++) {               /* .. its cups */
        fl_put(c, mx + sway - 6, my + dy, PH);
        fl_put(c, mx + sway + 6, my + dy, PH);
    }
    fl_put(c, mx, my + 3, Y1), fl_put(c, mx + 1, my + 3, Y1), fl_put(c, mx, my + 2, HI);   /* a little tail */
    if (arms == 1) {                             /* both arms up to the light */
        fl_put(c, mx - 6, my - 3, Y0), fl_put(c, mx - 7, my - 4, Y0), fl_put(c, mx - 7, my - 5, Y0);
        fl_put(c, mx + 6, my - 3, Y0), fl_put(c, mx + 7, my - 4, Y0), fl_put(c, mx + 7, my - 5, Y0);
    } else if (arms == 2) {                      /* its left paw up */
        fl_put(c, mx - 6, my - 2, Y0), fl_put(c, mx - 7, my - 3, Y0), fl_put(c, mx - 7, my - 4, Y1);
    }
}
/* by the Sornyak: in the grass on the left, facing it, a hop on each chord, arms up when it shines */
static void sk_moose(flora_t *f, flc_t *c)
{
    uint32_t beat = f->t - f->ring_t[f->ring_i & 3];
    int my = sk_ground(f) + 1 - (f->ring_t[f->ring_i & 3] && beat < 160u);
    sk_moose_at(f, c, sk_w(f) / 5, my, 1, sk_shine(f) > 160);
}

static void sk_render(flora_t *f, flc_t *c)
{
    int i, g = sk_ground(f);
    switch (f->backdrop) {
    case FL_BD_SCENE:
        sk_sky(f, c, 1);
        sk_trees(c, g);
        sk_meadow(f, c, g);
        break;
    case FL_BD_RIPPLES:
        sk_sky(f, c, 0);
        fl_bd_rings(f, c, 1);
        break;
    default:
        flc_clear(c, FL_BG);
        break;
    }
    for (i = 0; i < FL_MAX_PART; i++)            /* far fireflies */
        if (f->part[i].life && (i & 1))
            fl_draw_part(f, c, i);
    sk_stem(f, c);
    sk_caps(f, c);
    sk_crown_draw(f, c);
    if (f->backdrop == FL_BD_SCENE)
        sk_moose(f, c);
    for (i = 0; i < FL_MAX_PART; i++)            /* near ones */
        if (f->part[i].life && !(i & 1))
            fl_draw_part(f, c, i);
    sk_swarm(f, c);                              /* (the butterflies over everything) */
}
