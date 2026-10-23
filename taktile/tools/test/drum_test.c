/* every drum sound shares the DRUM ENGINE knobs (Sweep, SweepTime, Noise, Click); the middle position is the sound as it was */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst;
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
static float *run(const char *type, const char *key, const char *val) {
    inst = api->create_instance(".", "{}");
    for (int t = 1; t < 16; t++) { char v[16]; snprintf(v, sizeof v, "%d,1", t); sp("mute", v); }
    sp("p01_type", type); if (key) { char k[40]; snprintf(k, sizeof k, "p01_%s", key); sp(k, val); }
    char v[64]; snprintf(v, sizeof v, "0,0,0,%02x80%02x004000", 0x80 | 100, 64); sp("st", v); sp("bpm", "120"); sp("play", "1");
    int total = 44100; int16_t *b = calloc((size_t)total * 2, 2); float *x = calloc(total, 4);
    for (int pos = 0; pos < total; pos += 128) api->render_block(inst, b + pos * 2, (total - pos < 128) ? total - pos : 128);
    for (int i = 0; i < total; i++) x[i] = b[2 * i] / 32768.0f; free(b); api->destroy_instance(inst); return x;
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    int failed = 0;
    const struct { const char *t, *k, *what; } C[] = { {"Kick","dsw","Sweep"}, {"Kick","dst","SweepTime"}, {"Snare","dnz","Noise"}, {"Snare","dck","Click"}, {"HatCl","dck","Click"}, {"Tom","dsw","Sweep"}, {"Conga","dnz","Noise"} };
    for (int c = 0; c < 7; c++) {
        float *lo = run(C[c].t, C[c].k, "0.15"), *hi = run(C[c].t, C[c].k, "0.85"), *mid = run(C[c].t, C[c].k, "0.5"), *def = run(C[c].t, NULL, NULL);
        double e = 0, r = 0, m = 0; for (int i = 0; i < 44100; i++) { double d = hi[i] - lo[i]; e += d * d; r += lo[i] * lo[i] + hi[i] * hi[i]; m += fabs(mid[i] - def[i]); }
        int ok = sqrt(e / (r + 1e-12)) > 0.03 && m < 1e-3;
        printf("%s   %s %s changes the sound (%.0f%%) and the middle is the sound as it was (diff %.5f)\n", ok ? "ok  " : "FAIL", C[c].t, C[c].what, 100 * sqrt(e / (r + 1e-12)), m);
        if (!ok) failed++;
        free(lo); free(hi); free(mid); free(def);
    }
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed != 0;
}
