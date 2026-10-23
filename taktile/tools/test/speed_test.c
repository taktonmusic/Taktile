/* track speed: every track can run at its own ratio (1/2 = half time, 3/2, 2/3 ...) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
/* all 16 steps of track 1 hit a short loud sound; returns the number of onsets and the biggest deviation (ms) of an onset from an even grid */
static int onsets(int n, int d, double secs, double *dev) {
    inst = api->create_instance(".", "{}"); sp("bpm", "120");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    sp("p01_decay", "0.05"); sp("p01_att", "0");
    for (int i = 0; i < 16; i++) { char st[64]; snprintf(st, sizeof st, "0,0,%d,%02x%02x%02x%02x%02x%02x", i, 0x80 | 110, 0, 64, 0, 0x40, 0); sp("st", st); }
    char v[32]; snprintf(v, sizeof v, "0,%d,%d", n, d); sp("tspd", v); sp("play", "1");
    int total = (int)(secs * 44100); int16_t b[256]; int cnt = 0, hot = 0; double first = -1, last = 0; static double tt[2000];
    for (int pos = 0; pos < total; pos += 128) {
        api->render_block(inst, b, 128);
        for (int i = 0; i < 128; i += 16) {
            double e = 0; for (int k = 0; k < 16; k++) { double x = b[2 * (i + k)] / 32768.0; e += x * x; }
            e = sqrt(e / 16);
            if (e > 0.02 && !hot && (pos + i) / 44.1 - last > 25.0) { hot = 1; if (cnt < 2000) tt[cnt] = (pos + i) / 44.1; cnt++; if (first < 0) first = (pos + i) / 44.1; last = (pos + i) / 44.1; }
            else if (e < 0.004) hot = 0;
        }
    }
    double span = cnt > 1 ? (last - first) / (cnt - 1) : 0; *dev = 0;
    for (int i = 1; i < cnt && i < 2000; i++) { double dd = fabs((tt[i] - tt[i - 1]) - span); if (dd > *dev) *dev = dd; }
    api->destroy_instance(inst); return cnt;
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    static const int R[][2] = { { 1, 1 }, { 1, 2 }, { 2, 1 }, { 3, 2 }, { 2, 3 }, { 1, 3 }, { 4, 3 }, { 3, 4 } };
    for (int q = 0; q < 8; q++) {
        double dev; int c = onsets(R[q][0], R[q][1], 8.0, &dev);
        double expect = 8.0 * 8.0 * R[q][0] / R[q][1];                  /* 8 s at 120 bpm = 32 beats = 64 eighth... 16th steps: 8 steps per second */
        OK(fabs(c - expect) <= 2.0, "speed %d/%d: %d onsets in 8 s (expect %.0f)", R[q][0], R[q][1], c, expect);
        OK(dev < 6.0, "speed %d/%d: even spacing (largest gap error %.1f ms)", R[q][0], R[q][1], dev);
    }
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
