/* SPDX-License-Identifier: GPL-3.0-only
 * The power-on splash: a comet falls into the Sornyak's night meadow and leaves a glowing crater, then the name
 * drops in under it on an arch, over the field. Played by main.c while it boots (IRQs still off: a busy-wait clock, the panel by polled DMA). SAFE MODE
 * shows its notice instead. Included after cf_screens.c. */

#define CF_SPLASH_MS 2500u
#include "cf_title.h"                            /* (build/gen: tools/gen_title.py) */

#define CF_TITLE_S 4                             /* screen px a font pixel */
#define CF_TITLE_Y 176                           /* its top, landed (the ends of its arch) */

/* letter i's lift at t ms: px above its place (0 landed, -2..2 in the wave); -99: not in yet */
static int cf_title_drop(uint32_t t, int i)
{
    uint32_t t0 = 1450u + (uint32_t)i * 80u, a;   /* (after the comet's dust settles) */
    if (t < t0)
        return -99;
    a = t - t0;
    if (a < 160u)                                /* falling from 28 px, faster and faster */
        return (int)(28u * (160u - a) * (160u - a) / (160u * 160u));
    if (a < 320u)                                /* a bounce up 6 px and back */
        return (int)((fl_fsin((int32_t)((a - 160u) * 512u / 160u)) * 6) >> 14);
    if (a < 420u)                                /* .. and a small one */
        return (int)((fl_fsin((int32_t)((a - 320u) * 512u / 100u)) * 2) >> 14);
    return (int)((fl_fsin((int32_t)(t / 3u) - i * 120) * 2) >> 14);   /* then a slow wave, 2 px */
}

static void cf_title_draw(uint32_t t)
{
    /* a rainbow, S to k: each letter's colour over, under, and its underside (palette ramps, 0 light .. 3 dark) */
    static const uint8_t COL[CF_TITLE_N][3] = {
        {FL_C(FL_R_FIRE, 0), FL_C(FL_R_FIRE, 1), FL_C(FL_R_FIRE, 2)},       /* S yellow */
        {FL_C(FL_R_FIRE, 1), FL_C(FL_R_FIRE, 2), FL_C(FL_R_FIRE, 3)},       /* o orange */
        {FL_C(FL_R_ROSE, 1), FL_C(FL_R_ROSE, 2), FL_C(FL_R_ROSE, 3)},       /* r rose red */
        {FL_C(FL_R_ORCHID, 1), FL_C(FL_R_ORCHID, 2), FL_C(FL_R_ORCHID, 3)}, /* n magenta */
        {FL_C(FL_R_SEA, 0), FL_C(FL_R_SEA, 1), FL_C(FL_R_SEA, 2)},          /* y sky */
        {FL_C(FL_R_JADE, 1), FL_C(FL_R_JADE, 2), FL_C(FL_R_JADE, 3)},       /* a jade */
        {FL_C(FL_R_LIME, 0), FL_C(FL_R_LIME, 1), FL_C(FL_R_LIME, 2)}};      /* k lime */
    const int S = CF_TITLE_S, x0 = 120 - CF_TITLE_W * S / 2;
    int pass, i, r, b;
    for (pass = 0; pass < 2; pass++)             /* the outlines of all first, so no letter's covers another's */
        for (i = 0; i < CF_TITLE_N; i++) {
            const cf_title_t *L = &CF_TITLE[i];
            int d = cf_title_drop(t, i), lx, top = CF_TITLE_H, bot = 0, m = 2 * i - (CF_TITLE_N - 1);
            if (d < -2)                          /* (-99: not in yet; the wave goes 2 px either way) */
                continue;
            /* on a gentle arch: the middle letter 16 px up, the ends level */
            d += 16 - 16 * m * m / ((CF_TITLE_N - 1) * (CF_TITLE_N - 1));
            lx = x0 + L->x * S;
            for (r = 0; r < CF_TITLE_H; r++)
                if (L->rows[r]) {
                    if (r < top)
                        top = r;
                    bot = r;
                }
            for (r = 0; r < CF_TITLE_H; r++)
                for (b = 0; b < L->w; b++) {
                    int px = lx + b * S, py = CF_TITLE_Y - d + r * S;
                    uint8_t c;
                    if (!(L->rows[r] & (0x8000u >> b)))
                        continue;
                    if (!pass) {                 /* the outline: 2 px round it, the shadow 2 px lower */
                        fb_rect(px - 2, py - 2, S + 4, S + 6, FL_BLACK);
                        continue;
                    }
                    c = r * 2 <= top + bot ? COL[i][0] : COL[i][1];   /* (light over, darker under) */
                    fb_rect(px, py, S, S, c);
                    if (r == bot || !(L->rows[r + 1] & (0x8000u >> b)))
                        fb_rect(px, py + S - 1, S, 1, COL[i][2]);   /* (its underside) */
                    if (!r || !(L->rows[r - 1] & (0x8000u >> b)))
                        fb_rect(px, py, S - 1, 1, FL_INK);   /* (a glint on its top edge) */
                }
        }
}

/* a step darker on the ground's own ramp (or the night's tints) */
static uint8_t cf_darker(uint8_t c)
{
    if (c >= FL_RAMP0 && c < FL_RAMP0 + FL_NRAMPS * 4)
        return (uint8_t)((c - FL_RAMP0) % 4u < 3u ? c + 1 : c);
    if (c > FL_S0 && c <= FL_S3)
        return (uint8_t)(c - 1);
    return c;
}
static uint8_t cf_lighter(uint8_t c)
{
    if (c >= FL_RAMP0 && c < FL_RAMP0 + FL_NRAMPS * 4)
        return (uint8_t)((c - FL_RAMP0) % 4u > 0u ? c - 1 : c);
    if (c >= FL_S0 && c < FL_S3)
        return (uint8_t)(c + 1);
    return c;
}

/* the scene's ground carried on down under the picture to the screen's foot, so the title sits in a field: the
 * ground's colour (the commonest in the picture's last row) in the picture's 2 px blocks, fading darker down into
 * the deep night, grass blades swaying, and fireflies drifting and blinking over it */
static void cf_field_below(int y0, uint32_t t)
{
    static uint8_t cnt[256];
    const uint8_t *last = fl_px + (FLH - 1) * FLW;
    uint8_t ground = last[FLW / 2], dark, blade;
    int x, y, i, H = FB_H - y0;
    for (x = 0; x < 256; x++)
        cnt[x] = 0;
    for (x = 0; x < FLW; x++)
        if (cnt[last[x]] < 255)
            cnt[last[x]]++;
    for (x = 0; x < 256; x++)
        if (cnt[x] > cnt[ground])
            ground = (uint8_t)x;
    {                                            /* the ground, darker and darker down into the deep night */
        const uint8_t deep[3] = {ground, FL_S0, FL_BLACK};
        for (y = y0; y < FB_H; y += 2)
            for (x = 0; x < FB_W; x += 2)
                fb_rect(x, y, 2, 2, fl_grad(deep, 3, (y - y0) * 480 / H, x / 2, y / 2));
    }
    dark = cf_darker(ground);
    blade = cf_lighter(ground) != ground ? cf_lighter(ground) : FL_C(FL_R_JADE, 2);
    for (i = 0; i < 40; i++) {                   /* grass blades, 2 or 3 blocks tall, their tips swaying */
        uint32_t h = fl_hash32((uint32_t)i * 2654435761u + 5u);
        int bx = (int)(h % (FB_W / 2u)) * 2, by = y0 + 6 + (int)((h >> 8) % (uint32_t)(H - 8)) / 2 * 2, k = 2 + (int)((h >> 16) & 1u);
        int lean = (fl_fsin((int32_t)(t / 3u) + (int32_t)(h >> 20)) * 2) >> 14;   /* (-2..2 px) */
        for (y = 0; y < k; y++)
            fb_rect(bx + (y == k - 1 ? lean / 2 * 2 : 0), by - y * 2, 2, 2, y ? blade : dark);
    }
    for (i = 0; i < 7; i++) {                    /* fireflies: drifting about, glowing and fading */
        uint32_t h = fl_hash32((uint32_t)i * 40503u + 99u);
        int fx = (int)(h % (uint32_t)FB_W) + ((fl_fsin((int32_t)(t / 4u) + (int32_t)(h >> 10)) * 10) >> 14);
        int fy = y0 + 4 + (int)((h >> 8) % (uint32_t)(H - 8)) + ((fl_fcos((int32_t)(t / 5u) + (int32_t)(h >> 20)) * 5) >> 14);
        int glow = (int)(((t + (h >> 4)) / 60u) % 24u);   /* (lit 0..9 of a 24-step blink) */
        if (glow > 9)
            continue;
        fb_rect(fx, fy, 2, 2, glow > 2 && glow < 7 ? FL_C(FL_R_FIRE, 0) : FL_C(FL_R_LIME, 0));
        if (glow > 2 && glow < 7) {              /* (its halo, at the brightest) */
            fb_pset(fx - 1, fy, FL_C(FL_R_LIME, 1)), fb_pset(fx + 2, fy + 1, FL_C(FL_R_LIME, 1));
            fb_pset(fx, fy - 1, FL_C(FL_R_LIME, 1)), fb_pset(fx + 1, fy + 2, FL_C(FL_R_LIME, 1));
        }
    }
}

/* ---- the cover: a comet falls into the night meadow --------------------------------------------------------- */
/* A night meadow (sornyak.c's trees, the torch's green) without the plant or the moose. A comet streaks down from the top
 * right with a glowing tail and lands where the Sornyak grows: the sky flashes, the picture shakes, earth flies up
 * and a ring of dust runs out along the ground. A crater stays, a pit glowing from inside, its light breathing
 * in the air over it and motes of it floating up. Drawn on the flora canvas, integer maths. */
#define CF_COMET_T0 250u                         /* it comes into the sky */
#define CF_COMET_HIT 1100u                       /* it lands */

/* the cover's meadow: green (the torch's hill colours), darker the deeper it goes; grass swaying, pale flowers */
static void cf_intro_ground(flc_t *c, int g, uint32_t t)
{
    static const uint8_t HILL[5] = {FL_C(FL_R_JADE, 2), FL_C(FL_R_LIME, 2), FL_C(FL_R_LIME, 3), FL_C(FL_R_JADE, 3),
                                    FL_S1};
    int W = c->w, H = c->h, x, y, i;
    for (x = 0; x < W; x++) {
        int top = g + ((fl_fsin(x * 9 + 200) * 1) >> 14);
        for (y = top; y < H; y++)
            fl_put(c, x, y, y == top ? FL_C(FL_R_JADE, 1) : fl_grad(HILL, 5, (y - top) * 40, x, y));
    }
    for (i = 0; i < 40; i++) {                   /* grass */
        uint32_t h = fl_hash32((uint32_t)i * 104729u + 77u);
        int gx = (int)(h % (uint32_t)W), gy = g + 2 + (int)((h >> 8) % (uint32_t)(H - g - 2)), k = 1 + (int)((h >> 12) % 3u);
        int lean = (fl_fsin((int32_t)(t >> 4) + (int32_t)(h & 1023u)) * 1) >> 14;
        for (y = 0; y < k; y++)
            fl_put(c, gx + (y == k - 1 ? lean : 0), gy - y, (h >> 20) & 1u ? FL_C(FL_R_JADE, 1) : FL_C(FL_R_LIME, 1));
    }
}
/* pale flowers dotted over the meadow: a ring of petals round a dark middle (drawn after the scorch: it spares them) */
static void cf_intro_flowers(flc_t *c, int g, int cx, int cy)
{
    int i;
    for (i = 0; i < 16; i++) {
        uint32_t h = fl_hash32((uint32_t)i * 15485863u + 21u);
        int fx = 2 + (int)(h % (uint32_t)(c->w - 4)), fy = g + 3 + (int)((h >> 8) % (uint32_t)(c->h - g - 5));
        int ex = fx - cx, ey = (fy - cy) * 3;
        if (ex * ex + ey * ey < 15 * 15)
            continue;                            /* (none in the hole) */
        fl_put(c, fx - 1, fy, FL_C(FL_R_SAND, 0)), fl_put(c, fx + 1, fy, FL_C(FL_R_SAND, 0));
        fl_put(c, fx, fy - 1, FL_C(FL_R_SAND, 1)), fl_put(c, fx, fy, FL_C(FL_R_SAND, 3));
    }
}

static void cf_intro_scene(uint32_t t, flc_t *c)
{
    flora_t *f = &flo;
    int W = c->w, H = c->h, x, y, i, g = c->h - 22, cx = sk_bx(f), cy = g + 9;   /* (the hole: on the meadow) */
    int hit = t >= CF_COMET_HIT;
    uint32_t since = hit ? t - CF_COMET_HIT : 0u;
    int flash = hit && since < 600u ? (int)(600u - since) * 2 / 3 : 0;
    int sx0 = W + 16, sy0 = -18;                 /* (where the comet comes from) */
    int32_t ddx = cx - sx0, ddy = cy - sy0;      /* (its way) */
    /* the night: blue at the horizon, through dark blue up to black at the top; lighter for a moment at the impact */
    {
        static const uint8_t SKY[4] = {FL_BLACK, FL_S0, FL_S1, FL_S2};
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
                c->px[y * W + x] = fl_grad(SKY, 4, y * 768 / g + flash * 3 / 2, x, y);   /* (black, the darkest
                                                  * blue, dark blue, then blue at the horizon) */
    }
    /* while it falls: faint streaks of light across the sky, along its way, rushing past */
    if (t >= CF_COMET_T0 && (!hit || since < 300u))
        for (i = 0; i < 9; i++) {
            uint32_t h = fl_hash32((uint32_t)i * 2654435761u + 5u);
            int len = 8 + (int)(h % 10u), k;
            int32_t run = (int32_t)((t / 4u + (h >> 8)) % 140u) - 20;   /* (along the way, px) */
            int bx = (int)(h % (uint32_t)(W + 40)) - 10, by = (int)((h >> 12) % (uint32_t)(g - 4)) - 30;
            for (k = 0; k < len; k++) {
                int px = bx + (int)(ddx * (run + k) / 110), py = by + (int)(ddy * (run + k) / 110);
                if (py < g && fl_bayer[(py & 3) * 4 + (px & 3)] < 9 - k * 8 / len)
                    fl_put(c, px, py, k < len / 2 ? FL_S3 : FL_S2);
            }
        }
    for (i = 0; i < 45; i++) {                   /* stars */
        uint32_t h = fl_hash32((uint32_t)i * 2246822519u + 31u);
        int sx = (int)(h % (uint32_t)W), sy = (int)((h >> 8) % (uint32_t)(g - 12)), tw = (int)(((t >> 6) + h) % 48u);
        fl_put(c, sx, sy, tw < 3 ? FL_INK : (h >> 20) % 3u == 0u ? FL_C(FL_R_SAND, 0) : FL_C(FL_R_SAGE, 1));
    }
    sk_trees(c, g);
    cf_intro_ground(c, g, t);
    if (hit) {
        int r = since < 160u ? 3 + (int)(12u * since / 160u) : 15, ry = r / 2 + 1, R, Ry, dx, dy;
        /* the light breathing in the pit: 0..256, in and out every 1.6 s (bright at the impact) */
        int breath = since < 500u ? 256 : 128 + ((fl_fsin((int32_t)((since - 500u) * 1024u / 1600u) - 256) * 128) >> 14);
        /* the scorch: a ragged black patch burnt round the hole, spreading out at the blast */
        R = since < 220u ? 4 + (int)(30u * since / 220u) : 34;
        Ry = R / 3 + 1;
        for (dy = -Ry; dy <= Ry; dy++)
            for (dx = -R; dx <= R; dx++) {
                uint32_t n = fl_hash32((uint32_t)((dx + 64) / 3) * 131u + (uint32_t)((dy + 64) / 2) * 977u);
                int32_t q = (int32_t)dx * dx * 256 / (R * R) + (int32_t)dy * dy * 256 / (Ry * Ry);
                int32_t edge = 150 + (int32_t)(n % 106u);   /* (ragged) */
                int px = cx + dx, py = cy + dy;
                if (q > edge || py <= g)
                    continue;
                fl_put(c, px, py, q < edge - 60 || fl_bayer[(py & 3) * 4 + (px & 3)] < 9 ? FL_BLACK : FL_S0);
            }
        cf_intro_flowers(c, g, cx, cy);
        /* the dust ring running out along the ground */
        if (since < 700u) {
            int d = R + (int)(since / 12u);
            for (i = -1; i <= 1; i += 2)
                for (dx = 0; dx < 4; dx++)
                    fl_put(c, cx + i * (d + dx), cy - (dx & 1), since < 400u ? FL_C(FL_R_SAND, 1) : FL_C(FL_R_SAND, 2));
        }
        /* its light in the air over the hole: a soft dithered halo, swelling and fading with the breath */
        {
            int ax = r - 2 + breath / 50, ay = ry + 9 + breath / 24;   /* (a soft column, taller as it swells) */
            for (dy = -ay; dy <= 0; dy++)
                for (dx = -ax; dx <= ax; dx++) {
                    int px = cx + dx, py = cy - 1 + dy;
                    int32_t q = (int32_t)dx * dx * 256 / (ax * ax) + (int32_t)dy * dy * 256 / (ay * ay);
                    int dens = (int)((256 - q) * (20 + breath) / 7000);   /* (0..about 8 of 16, thin at its edge) */
                    if (q >= 256 || fl_bayer[(py & 3) * 4 + (px & 3)] >= dens)
                        continue;
                    fl_put(c, px, py, q < 60 && breath > 170 ? FL_C(FL_R_PEACH, 0) : q < 140 ? FL_C(FL_R_PEACH, 1)
                                                                                        : FL_C(FL_R_ROSE, 3));
                }
        }
        /* the hole, a pit seen from the front and a little above: earth heaped in a ring round it (lit on top, its
         * front casting a shadow), the far wall going down inside lit red from below, the floor deeper and nearer
         * glowing orange, the rock on it a mauve dome with a pink glint, and the near lip raised over the front */
        {
            int fy = ry / 4, frx = r * 3 / 5, fry = ry / 2 + 1;   /* (the floor: lower, smaller, set back) */
            int pulse = (breath - 128) / 3;      /* (-43..43: the floor's and the wall's light breathing) */
            for (dy = -ry - 4; dy <= ry + 4; dy++)
                for (dx = -r - 5; dx <= r + 5; dx++) {
                    int32_t q = (int32_t)dx * dx * 256 / (r * r) + (int32_t)dy * dy * 256 / (ry * ry);
                    int32_t qb = (int32_t)dx * dx * 256 / ((r + 5) * (r + 5)) + (int32_t)dy * dy * 256 / ((ry + 3) * (ry + 3));
                    uint32_t n = fl_hash32((uint32_t)(dx + 64) * 53u + (uint32_t)(dy + 64) * 11u);
                    int px = cx + dx, py = cy + dy, dith = fl_bayer[(py & 3) * 4 + (px & 3)];
                    uint8_t col;
                    if (q >= 256 + (int32_t)(n % 40u)) { /* outside the opening: the heaped earth */
                        if (qb > 256)
                            continue;
                        if (dy < 0)              /* (its top, catching the glow) */
                            col = qb > 200 ? FL_C(FL_R_SAND, 3) : dith < 6 ? FL_C(FL_R_PEACH, 2) : FL_C(FL_R_SAND, 2);
                        else                     /* (its front, in shadow) */
                            col = qb > 210 ? FL_BLACK : dith < 8 ? FL_C(FL_R_SAND, 3) : FL_BLACK;
                    } else if (dy > 0 && q > 150) {   /* the near lip, raised: a lit top edge, dark below it */
                        col = q < 190 ? FL_C(FL_R_SAND, 1) : q < 215 ? FL_C(FL_R_SAND, 2) : FL_C(FL_R_SAND, 3);
                    } else {
                        int32_t qf = (int32_t)dx * dx * 256 / (frx * frx) + (int32_t)(dy - fy) * (dy - fy) * 256 / (fry * fry);
                        if (qf < 256) {          /* the floor, glowing (the rock on it: drawn after, a ball) */
                            col =qf < 150 + pulse ? FL_C(FL_R_FIRE, 1) : dith < 8 ? FL_C(FL_R_FIRE, 1) : FL_C(FL_R_FIRE, 2);
                        } else {                 /* the far wall: bright by the floor, dark up at the rim, darker at
                                                  * its sides */
                            int v = (dy + ry) * 256 / (ry + fy - fry + 1), side = abs_i(dx) * 4 > r * 3;
                            v -= pulse / 2;
                            col = v > 220 ? FL_C(FL_R_FIRE, 1) : v > 170 ? FL_C(FL_R_FIRE, 2)
                                : v > 120 ? (dith < 8 ? FL_C(FL_R_ROSE, 2) : FL_C(FL_R_FIRE, 2))
                                : v > 70 ? FL_C(FL_R_ROSE, 3) : v > 30 ? FL_C(FL_R_PEACH, 3) : FL_BLACK;
                            if (side)
                                col = col == FL_C(FL_R_FIRE, 2) ? FL_C(FL_R_ROSE, 2) : col == FL_C(FL_R_ROSE, 2) ? FL_C(FL_R_ROSE, 3) : FL_BLACK;
                            if (q > 236 && dy < 0)
                                col = FL_C(FL_R_SAND, 2);   /* (the far rim's edge) */
                        }
                    }
                    fl_put(c, px, py, col);
                }
        }
        /* the rock: a round ball sitting on the floor of the pit, lit from the top left, its shadow on the lower right,
         * and the floor's glow on its underside; warmer, its glint bigger, as the light swells. The near lip stays in
         * front of its foot */
        {
            int rb = r / 3 + 1, bx0 = cx, by0 = cy + ry / 4 - rb / 2;
            for (dy = -rb; dy <= rb; dy++)
                for (dx = -rb; dx <= rb; dx++) {
                    int d2 = dx * dx + dy * dy, px = bx0 + dx, py = by0 + dy;
                    int32_t q = (int32_t)(px - cx) * (px - cx) * 256 / (r * r) + (int32_t)(py - cy) * (py - cy) * 256 / (ry * ry);
                    int hx2 = dx + rb / 2, hy2 = dy + rb / 2;   /* (from the lit spot, up and left) */
                    int lit = 160 - (hx2 * hx2 + hy2 * hy2) * 256 / (rb * rb) + (breath - 128) / 4;   /* (rings of light round it) */
                    int dith = fl_bayer[(py & 3) * 4 + (px & 3)];
                    uint8_t col;
                    if (d2 > rb * rb + rb / 2 || (py > cy && q > 150))
                        continue;                /* (outside it, or behind the near lip) */
                    if ((dx + rb / 3) * (dx + rb / 3) + (dy + rb / 3) * (dy + rb / 3) <= 1 + breath / 120)
                        col = breath > 190 ? FL_INK : FL_C(FL_R_ROSE, 0);           /* (its glint) */
                    else if (dy > rb / 2 && d2 > (rb - 1) * (rb - 1))
                        col = dith < 9 ? FL_C(FL_R_FIRE, 1) : FL_C(FL_R_ROSE, 2);   /* (the glow on its underside) */
                    else if (lit > 90)
                        col = FL_C(FL_R_ROSE, 1);
                    else if (lit > 20)
                        col = dith < 8 ? FL_C(FL_R_ROSE, 1) : FL_C(FL_R_ORCHID, 2);
                    else if (lit > -60)
                        col = FL_C(FL_R_ORCHID, 2);
                    else if (lit > -130)
                        col = dith < 8 ? FL_C(FL_R_ORCHID, 2) : FL_C(FL_R_ORCHID, 3);
                    else
                        col = FL_C(FL_R_ORCHID, 3);
                    fl_put(c, px, py, col);
                }
        }
        /* earth and fire flying out, falling back */
        if (since < 1300u)
            for (i = 0; i < 34; i++) {
                uint32_t h = fl_hash32((uint32_t)i * 40503u + 7u);
                int32_t s = (int32_t)since;
                int32_t vx = (int32_t)(h % 81u) - 40, vy = 35 + (int32_t)((h >> 8) % 55u);   /* px a second */
                int px = cx + (int)(vx * s / 1000), py = cy - 2 - (int)((vy * s - 60 * s * s / 1000) / 1000);
                uint8_t col = i & 3 ? FL_C(FL_R_SAND, 2) : since < 400u ? FL_C(FL_R_FIRE, 0) : FL_C(FL_R_FIRE, 2);
                if (py > cy + 4)
                    continue;                    /* (landed) */
                fl_put(c, px, py, col);
                if (i % 3 == 0)
                    fl_put(c, px + 1, py, col), fl_put(c, px, py + 1, col), fl_put(c, px + 1, py + 1, col);
            }
        /* smoke: a thick plume at first, thinning away */
        if (since > 150u && since < 1600u)
            for (i = 0; i < 12; i++) {
                uint32_t h = fl_hash32((uint32_t)i * 7919u + 3u), life = 1600u, a = (since + (h % 1600u)) % life;
                int sx = cx - 6 + (int)(h % 13u) + (int)(a * ((h >> 9) % 4u) / 400u), sy = cy - 2 - (int)(a / 40u);
                int pr = 2 + (int)(a / 450u), thin = (int)(since / 120u);   /* (the plume thins away) */
                for (dy = -pr; dy <= pr; dy++)
                    for (dx = -pr; dx <= pr; dx++)
                        if (dx * dx + dy * dy <= pr * pr &&
                            fl_bayer[((sy + dy) & 3) * 4 + ((sx + dx) & 3)] < 13 - (int)(a / 140u) - thin)
                            fl_put(c, sx + dx, sy + dy, a < 500u ? FL_C(FL_R_SAGE, 2) : FL_C(FL_R_SAGE, 3));
            }
        for (i = 0; i < 6; i++) {                /* motes of its light floating up slowly, swaying, fading */
            uint32_t h = fl_hash32((uint32_t)i * 104729u + 11u), life = 1800u + h % 900u, a = (since + h % life) % life;
            int mx = cx - 8 + (int)(h % 17u) + (int)((fl_fsin((int32_t)(a / 3u + h)) * 3) >> 14), my = cy - 2 - (int)(a / 70u);
            uint8_t col = (h >> 8) % 3u == 0u ? FL_C(FL_R_ROSE, 0) : (h >> 8) % 3u == 1u ? FL_C(FL_R_FIRE, 0) : FL_C(FL_R_PEACH, 0);
            if (since < 400u || (a > life * 3u / 4u && ((a / 80u) & 1u)))
                continue;                        /* (after the blast; blinking out at the end) */
            fl_put(c, mx, my, col);
            if (a < life / 3u && breath > 150) {   /* (a soft halo while it is young and the light is up) */
                fl_put(c, mx - 1, my, FL_C(FL_R_PEACH, 1)), fl_put(c, mx + 1, my, FL_C(FL_R_PEACH, 1));
                fl_put(c, mx, my - 1, FL_C(FL_R_PEACH, 1)), fl_put(c, mx, my + 1, FL_C(FL_R_PEACH, 1));
            }
        }
    } else {
        cf_intro_flowers(c, g, cx, -100);
    }
    if (t >= CF_COMET_T0 && !hit) {              /* the comet: faster and faster, down onto the meadow */
        uint32_t p = (t - CF_COMET_T0) * 256u / (CF_COMET_HIT - CF_COMET_T0);   /* 0..255 */
        int32_t e = (int32_t)(p * p / 256u);     /* (accelerating) */
        int hx = sx0 + (int)(ddx * e / 256), hy = sy0 + (int)(ddy * e / 256), k;
        /* its tail: billowing smoke, salmon by the head, coral red, then brown-red far behind, puffs swelling */
        for (k = 46; k >= 6; k -= 3) {
            uint32_t h = fl_hash32((uint32_t)k * 7907u + t / 70u);
            int px = hx - (int)(ddx * k / 120) + (int)((h % 5u)) - 2, py = hy - (int)(ddy * k / 120) + (int)((h >> 4) % 5u) - 2;
            int pr = 3 + k / 10, dx, dy;
            uint8_t col = k < 16 ? FL_C(FL_R_PEACH, 1) : k < 28 ? FL_C(FL_R_ROSE, 1) : k < 38 ? FL_C(FL_R_ROSE, 2) : FL_C(FL_R_PEACH, 2);
            for (dy = -pr; dy <= pr; dy++)
                for (dx = -pr; dx <= pr; dx++) {
                    uint32_t n = fl_hash32((uint32_t)(dx + 16) * 31u + (uint32_t)(dy + 16) * 7u + (uint32_t)k);
                    int d2 = dx * dx + dy * dy;
                    if (d2 > pr * pr || (d2 > (pr - 1) * (pr - 1) && (n & 1u)))
                        continue;                /* (lumpy edges) */
                    fl_put(c, px + dx, py + dy, d2 < pr && dy < 0 && k < 30 ? FL_C(FL_R_PEACH, 0) : col);
                }
        }
        for (y = -12; y <= 12; y++)              /* the glow round its head: wide and soft */
            for (x = -12; x <= 12; x++) {
                int d2 = x * x + y * y;
                if (d2 > 144 || d2 <= 30)
                    continue;
                if (d2 <= 64 || fl_bayer[((hy + y) & 3) * 4 + ((hx + x) & 3)] < (144 - d2) / 6)
                    fl_put(c, hx + x, hy + y, d2 <= 64 ? FL_C(FL_R_FIRE, 1) : FL_C(FL_R_PEACH, 1));
            }
        for (y = -6; y <= 6; y++)                /* the rock: lumpy, pale orange, lit on top */
            for (x = -6; x <= 6; x++) {
                uint32_t n = fl_hash32((uint32_t)(x + 16) * 97u + (uint32_t)(y + 16) * 13u);
                int d2 = x * x + y * y, lump = 25 + (int)(n % 12u);
                if (d2 > lump)
                    continue;
                fl_put(c, hx + x, hy + y, (n % 7u) == 0u ? FL_C(FL_R_FIRE, 0) : x + y > 3 ? FL_C(FL_R_PEACH, 1)
                                          : d2 < 10 && x + y < 0 ? FL_C(FL_R_SAND, 0) : FL_C(FL_R_PEACH, 0));
            }
        for (i = 0; i < 6; i++) {                /* embers shed round it */
            uint32_t h = fl_hash32((uint32_t)i * 2654435761u + t / 60u);
            fl_put(c, hx - 8 + (int)(h % 24u), hy - 10 + (int)((h >> 8) % 18u), h & 512u ? FL_C(FL_R_FIRE, 0) : FL_INK);
        }
    }
}

static void cf_splash_frame(uint32_t t)
{
    int shake = t >= CF_COMET_HIT && t - CF_COMET_HIT < 450u ? (((t - CF_COMET_HIT) / 40u) & 1u ? 4 : 0) : 0;   /* (down only:
                                                  * the field under it covers the overlap) */
    cf_intro_scene(t, &fl_cv);
    fb_fill(FL_BG);
    fb_blit2x(0, 20 + shake, fl_px, FLW, FLH);
    cf_field_below(20 + FLH * 2, t);
    /* the name, big, in Pixelify Sans (build/gen/cf_title.h): a font pixel a 4 px block, each letter its own colour,
     * light over its top half and darker under it, a white glint on its top edges, outlined dark. From 450 ms the
     * letters drop in one by one with a bounce, then sway in a slow wave */
    cf_title_draw(t);
    if (t > 2000u)
        fb_text_c(&CF_FONT_UI, 120, 218, FELUCCA_VERSION, FL_INK3, 1);
}

/* main.c's splash (IRQs off): the first picture only, the scene before the seed sprouts; the UI's first frames grow it
 * (cf_intro_draw) while the instrument already plays */
static void cf_splash(void)
{
    cs_defaults(&cs);
    cf_settings_load();                          /* (what was set before the power-off, unless SAFE MODE) */
    cf_palette_build();
    fb_full = 1;
    if (cr_safe) {
        fb_fill(FL_BG);
        fb_text_c(&CF_FONT_UI, 120, 90, "SAFE MODE", C_ACC(0), 2);
        fb_text_c(&CF_FONT_UI, 120, 130, "settings and sounds", FL_INK2, 1);
        fb_text_c(&CF_FONT_UI, 120, 144, "are not loaded", FL_INK2, 1);
        cf_present();
        return;
    }
    flora_init(&flo, 0x5EED1u, FLW / 2, FLH + 1);
    flo.bloom = FL_R_ROSE + 1u;                  /* the intro's flower is pink, whatever the key */
    flora_set_key(&flo, cs.chord.tonic);
    flo.backdrop = cs.backdrop;
    flora_set_species(&flo, cs.chord.algo >= CF_ALG_SIDE ? FL_SP_SORNYAK : FL_SP_PLANT);
    cf_splash_frame(0);                          /* (nothing has grown yet: the scene alone) */
    cf_present();
}

/* the intro: the UI's first CF_SPLASH_MS, the seed grows into a flower and the name comes in. Any key, button or
 * knob ends it at once (the plant stays: it is HOME's) */
static uint8_t cf_intro_on = 1;
static uint32_t cf_intro_t;
static int cf_intro_draw(uint32_t dt)
{
    static const fl_chord_t SEED = {4, {60, 64, 67, 74}, 0, FL_Q_MAJOR, 48};
    if (!cf_intro_on)
        return 0;
    if (!cf_intro_t) {
        flora_set_chord(&flo, &SEED);
        if (cs.boot_sound && !cr_safe)
            cf_boot_go = 1;                      /* (the bells strum a wide chord as the flower opens; cf_fx.c) */
    }
    /* (a step at most 40 ms: the first frame comes long after the boot began, and a jump there showed the name's
     * first letter at once, alone over the bare seed) */
    cf_intro_t += dt ? (dt > 40u ? 40u : dt) : 1u;
    if (cf_intro_t >= CF_SPLASH_MS || cu.idle_ms < cf_intro_t / 2u) {
        cf_intro_on = 0;
        flo.bloom = 0;                           /* from the first chord on, the colours follow the root */
        fb_full = 1;
        return 0;
    }
    flora_set_level(&flo, (uint16_t)(cf_intro_t < 700u ? cf_intro_t * 40u : 28000u));
    cf_splash_frame(cf_intro_t);
    cf_present();
    return 1;
}
