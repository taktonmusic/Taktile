/* the five reverb types (HALL, ECHO, SHIM, SPRING, DARK): each one has a tail, stays finite and sounds different from the others */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
static float *render(double secs) { int total = (int)(secs * 44100); int16_t *b = calloc((size_t)total * 2, 2); float *x = calloc(total, 4);
    for (int pos = 0; pos < total; pos += 128) api->render_block(inst, b + pos * 2, (total - pos < 128) ? total - pos : 128);
    for (int i = 0; i < total; i++) x[i] = b[2 * i] / 32768.0f; free(b); return x; }
static double rms(float *x, int a, int b) { double e = 0; for (int i = a; i < b; i++) e += (double)x[i] * x[i]; return sqrt(e / (b - a)); }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    const char *nm[5] = { "HALL", "ECHO", "SHIM", "SPRG", "DARK" }; float *y[5];
    for (int m = 0; m < 5; m++) {
        inst = api->create_instance(".", "{}");
        for (int t = 1; t < 16; t++) { char v[16]; snprintf(v, sizeof v, "%d,1", t); sp("mute", v); }
        sp("p01_type", "Snare"); sp("p01_rvb", "1.0"); sp("tvol", "0,1.0"); sp("rvb_ret", "0.9"); sp("rvb_size", "0.7"); sp("rvb_mode", nm[m]);
        { char v[64]; snprintf(v, sizeof v, "0,0,0,%02x80%02x004000", 0x80 | 110, 64); sp("st", v); }
        sp("bpm", "60"); sp("play", "1"); y[m] = render(5.0); api->destroy_instance(inst);
        double tail = rms(y[m], 44100 * 1, 44100 * 2), pk = 0; int bad = 0;
        for (int i = 0; i < 5 * 44100; i++) { if (!(fabsf(y[m][i]) < 4.0f)) bad = 1; if (fabsf(y[m][i]) > pk) pk = fabsf(y[m][i]); }
        printf("   %s: tail %.5f, peak %.2f\n", nm[m], tail, pk);
        OK(!bad && pk < 1.5, "%s stays finite and in range", nm[m]);
        OK(tail > 0.0003, "%s has a tail 1-2 s after the hit", nm[m]);
    }
    for (int m = 3; m < 5; m++) { double e = 0, r = 0; for (int i = 4000; i < 4 * 44100; i++) { double d = y[m][i] - y[0][i]; e += d * d; r += (double)y[0][i] * y[0][i]; }
        OK(sqrt(e / (r + 1e-12)) > 0.3, "%s sounds clearly different from HALL (%.0f%%)", nm[m], 100 * sqrt(e / (r + 1e-12))); }
    for (int m = 0; m < 5; m++) free(y[m]);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed != 0;
}
