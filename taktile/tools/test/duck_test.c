/* sidechain: pad 1 of the first drum track ducks every sound with Duck > 0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
static void setstep(int pat, int t, int st, int vel, int semi) { char v[64]; snprintf(v, sizeof v, "%d,%d,%d,%02x80%02x004000", pat, t, st, 0x80 | vel, semi + 64); sp("st", v); }
static float *render(double secs) { int total = (int)(secs * 44100); int16_t *b = calloc((size_t)total * 2, 2); float *x = calloc(total, 4);
    for (int pos = 0; pos < total; pos += 128) api->render_block(inst, b + pos * 2, (total - pos < 128) ? total - pos : 128);
    for (int i = 0; i < total; i++) x[i] = b[2 * i] / 32768.0f; free(b); return x; }
static double rms(float *x, int a, int b) { double e = 0; for (int i = a; i < b; i++) e += x[i] * x[i]; return sqrt(e / (b - a)); }
static double run(const char *duck, int klane) {
    inst = api->create_instance(".", "{}");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    sp("kind", "0,0");                                   /* track 1 = drum track (slot 0) */
    for (int t = 2; t < 16; t++) { char v[16]; snprintf(v, sizeof v, "%d,1", t); sp("mute", v); }
    sp("p02_type", "PadWarm"); sp("p02_duck", duck); sp("p02_duckr", "0.4"); sp("p02_rvb", "0"); sp("p02_dly", "0");
    for (int s = 0; s < 16; s += 4) setstep(0, klane, s, 110, 0);       /* kick = pad 1 of drum track 1 */
    setstep(0, 1, 0, 100, 0); sp("len", "1,16"); sp("bpm", "120");
    { char v[16]; snprintf(v, sizeof v, "p%02d_vol", klane + 1); sp(v, "0"); }                                  /* the kick itself is silent: only the pad is measured (the kick still triggers the duck) */
    sp("play", "1"); float *x = render(4.0);
    /* the pad sound is held; compare level just after a kick (beat 0.5 s: 22050 + 2000) with the level just before the next kick */
    double after = rms(x, 22050 + 1500, 22050 + 5000), before = rms(x, 22050 + 17000, 22050 + 21000);
    printf("   duck %s: after kick %.4f, before next kick %.4f\n", duck, after, before);
    free(x); api->destroy_instance(inst); return after / (before + 1e-9);
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    double r0 = run("0", 16), r1 = run("1", 16), r2 = run("1", 17);
    OK(r1 < r0 * 0.6, "Duck 100%% pulls the pad down right after the kick of pad 1 (ratio %.2f vs %.2f without)", r1, r0);
    OK(r2 > r0 * 0.8, "a hit of pad 2 does not duck (ratio %.2f)", r2);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed != 0;
}
