/* LFOs: a square LFO on PAN swings the sound between the speakers in time with the tempo; turning it off gives the knob value back exactly */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
static double gp(const char *k) { char b[64] = ""; api->get_param(inst, k, b, sizeof b); return atof(b); }
/* balance (L-R)/(L+R) in each 250 ms window */
static int run(int lfo, double *bal, int nwin) {
    inst = api->create_instance(".", "{}"); sp("bpm", "120");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    sp("p01_decay", "1.0"); sp("p01_att", "0"); sp("p01_pan", "0.5");
    for (int i = 0; i < 16; i += 2) { char st[64]; snprintf(st, sizeof st, "0,0,%d,%02x%02x%02x%02x%02x%02x", i, 0x80 | 110, 0, 64, 0, 0x40, 0); sp("st", st); }
    if (lfo) { sp("p01_l1t", "5"); sp("p01_l1s", "4"); sp("p01_l1y", "6"); sp("p01_l1d", "1.0"); }
    sp("play", "1");
    int16_t b[256]; int w = 0; double l = 0, r = 0; int pos = 0;
    while (w < nwin) {
        api->render_block(inst, b, 128); pos += 128;
        for (int i = 0; i < 128; i++) { l += (double)b[2 * i] * b[2 * i]; r += (double)b[2 * i + 1] * b[2 * i + 1]; }
        if (pos >= 11025 * (w + 1)) { bal[w++] = (sqrt(l) - sqrt(r)) / (sqrt(l) + sqrt(r) + 1e-9); l = r = 0; }
    }
    if (lfo) {
        double mid = gp("p01_pan"); (void)mid;
        sp("p01_l1d", "0"); api->render_block(inst, b, 128); api->render_block(inst, b, 128);
        double pn = gp("p01_pan"); OK(fabs(pn - 0.5) < 0.002, "LFO off: the pan knob is back at its own value (%.3f)", pn);
    }
    api->destroy_instance(inst); return w;
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    double a[32], c[32]; run(0, a, 16); run(1, c, 16);
    double ra = 0, rc = 0; for (int i = 4; i < 16; i++) { ra = fmax(ra, fabs(a[i])); rc = fmax(rc, fabs(c[i])); }
    OK(ra < 0.05, "no LFO: centred (max balance %.3f)", ra);
    int flips = 0; for (int i = 5; i < 16; i++) if (c[i] * c[i - 1] < 0) flips++;
    OK(rc > 0.4 && flips >= 4, "square LFO on PAN: swings (max %.2f, %d side changes in 3 s)", rc, flips);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
