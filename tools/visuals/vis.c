/* SPDX-License-Identifier: GPL-3.0-only
 * Sornyak's visuals for a projector: the firmware's flora engine (firmware/src/flora: the torch, the Sornyak, the
 * butterflies) as a WebAssembly module, driven from the page (tools/visuals/vis.js) by the FM-1's MIDI out. The page
 * calls the setters as the firmware's cf_feed_plant does, then vis_frame each animation frame, and draws vis_px()
 * (palette indices) through vis_pal() (0xRRGGBB). Built by tools/visuals/build.sh. */
#include <stdint.h>
#include <string.h>
/* one unit, as the firmware builds it (coralflower.c): the intro (cf_splash.c) uses the Sornyak's own helpers */
#include "flora_canvas.c"
#include "flora.c"

#define VIS_MAXW 320
#define VIS_MAXH 180
static flora_t f;
static uint8_t px[VIS_MAXW * VIS_MAXH];
static flc_t cv = {px, 160, 90};
static uint32_t pal[64];
static fl_chord_t chord;

#define EXPORT __attribute__((used, visibility("default")))

/* the canvas (at most 320 x 180) and a fresh organism on it */
EXPORT void vis_init(int w, int h, int species, uint32_t seed)
{
    uint32_t i;
    cv.w = w < 32 ? 32 : w > VIS_MAXW ? VIS_MAXW : w;
    cv.h = h < 32 ? 32 : h > VIS_MAXH ? VIS_MAXH : h;
    memset(px, 0, sizeof px);
    memset(&chord, 0, sizeof chord);
    flora_init(&f, seed, cv.w / 2, cv.h + 1);
    flora_set_species(&f, species);
    flora_set_bpm(&f, 120);
    flora_set_reverb(&f, 60);
    for (i = 0; i < FL_PAL_SIZE && i < 48u; i++)  /* (as the firmware's cf_palette_build, the theme Lime's) */
        pal[i] = fl_pal_rgb[i];
    for (i = 0; i < 4u; i++)
        pal[48 + i] = pal[52 + i] = i ? fl_pal_rgb[FL_C(FL_R_LIME, i)] : 0xB0E34Cu;
    for (i = 0; i < 8u; i++)
        pal[56 + i] = fl_back_rgb[i];
}
EXPORT void vis_species(int s) { flora_set_species(&f, s); }
EXPORT void vis_key(int pc) { flora_set_key(&f, pc); }
EXPORT void vis_backdrop(int b) { f.backdrop = (uint8_t)b; }
/* the chord: n notes (vis_chord_notes() first), its root's pitch class, FL_Q_* quality, the bass note (-1: none) */
EXPORT uint8_t *vis_chord_notes(void) { return chord.note; }
EXPORT void vis_chord(int n, int root, int quality, int bass)
{
    chord.n = (uint8_t)(n < 0 ? 0 : n > 8 ? 8 : n);
    chord.root = (uint8_t)(root % 12);
    chord.quality = (uint8_t)quality;
    chord.bass = (int8_t)bass;
    flora_set_chord(&f, &chord);
}
EXPORT void vis_note_on(int note, int vel) { flora_note_on(&f, (uint8_t)note, (uint8_t)vel); }
EXPORT void vis_level(int l) { flora_set_level(&f, (uint16_t)(l < 0 ? 0 : l > 65535 ? 65535 : l)); }
EXPORT void vis_tracks(int m) { flora_set_tracks(&f, (unsigned)m); }
EXPORT void vis_reverb(int a) { flora_set_reverb(&f, (unsigned)a); }
EXPORT void vis_bpm(int b) { flora_set_bpm(&f, (unsigned)b); }
EXPORT void vis_live(void) { flora_live_note(&f); }
EXPORT void vis_playing(int on) { flora_set_playing(&f, on); }
EXPORT void vis_beat(int pos, int beats) { flora_set_beat(&f, (unsigned)pos, (unsigned)beats); }
/* a frame: dt ms on, drawn */
EXPORT void vis_frame(int dt)
{
    flora_tick(&f, (uint32_t)(dt < 0 ? 0 : dt > 100 ? 100 : dt));
    flora_render(&f, &cv);
}
EXPORT uint8_t *vis_px(void) { return px; }
EXPORT uint32_t *vis_pal(void) { return pal; }
EXPORT int vis_w(void) { return cv.w; }
EXPORT int vis_h(void) { return cv.h; }

/* ---- the intro: the firmware's own (cf_splash.c, its comet, crater and arched title), on a 240 x 240 screen of
 * palette indices as the FM-1's. Below: the few things it takes from the firmware, as small stand-ins */
#define FB_W 240
#define FB_H 240
#define FLW 120
#define FLH 70
static uint8_t fb[FB_W * FB_H];
static uint8_t fl_px[FLW * FLH];
static flc_t fl_cv = {fl_px, FLW, FLH};
static flora_t flo;
static uint8_t fb_full;
static void fb_pset(int x, int y, uint8_t c)
{
    if (x >= 0 && x < FB_W && y >= 0 && y < FB_H)
        fb[y * FB_W + x] = c;
}
static void fb_rect(int x, int y, int w, int h, uint8_t c)
{
    int x1 = x + w > FB_W ? FB_W : x + w, y1 = y + h > FB_H ? FB_H : y + h, i, j;
    for (j = y < 0 ? 0 : y; j < y1; j++)
        for (i = x < 0 ? 0 : x; i < x1; i++)
            fb[j * FB_W + i] = c;
}
static void fb_fill(uint8_t c) { fb_rect(0, 0, FB_W, FB_H, c); }
static void fb_blit2x(int x, int y, const uint8_t *src, int w, int h)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            fb_rect(x + 2 * i, y + 2 * j, 2, 2, src[j * w + i]);
}
#define fb_text_c(...) ((void)0)                 /* (the version line: not on the projector) */
#define FELUCCA_VERSION ""
#define C_ACC(i) ((uint8_t)(48 + (i)))
#define CF_ALG_SIDE 2
static int CF_FONT_UI;
static const int cr_safe = 1;                    /* (cf_splash / cf_intro_draw, the device's: not called here) */
static volatile uint8_t cf_boot_go;
static struct { struct { int tonic, algo; } chord; int backdrop, boot_sound; } cs;
static struct { uint32_t idle_ms; } cu;
static void cs_defaults(void *p) { (void)p; }
static void cf_settings_load(void) {}
static void cf_palette_build(void) {}
static void cf_present(void) {}
#include "../../firmware/src/cf_splash.c"

/* the intro from its start, on the scene shown now (the torch's or the Sornyak's meadow) */
EXPORT void vis_intro_start(void)
{
    flora_init(&flo, 0x5EED1u, FLW / 2, FLH + 1);
    flo.bloom = FL_R_ROSE + 1u;
    flora_set_species(&flo, f.species);
}
/* its frame at t ms (0 .. CF_SPLASH_MS): the 240 x 240 screen; 0 when it is over */
EXPORT int vis_intro_frame(int t)
{
    if (t < 0 || t >= (int)CF_SPLASH_MS)
        return 0;
    flora_set_level(&flo, (uint16_t)(t < 700 ? t * 40 : 28000));
    cf_splash_frame((uint32_t)t);
    return 1;
}
EXPORT uint8_t *vis_fb(void) { return fb; }
