/* master TAPE and HISS must be clearly audible */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
static void setstep(int t, int st, int vel) { char v[64]; snprintf(v, sizeof v, "0,%d,%d,%02x80%02x004000", t, st, 0x80 | vel, 64); sp("st", v); }
static double rms_db(float *x, int a, int b) { double e = 0; for (int i = a; i < b; i++) e += x[i] * x[i]; return 20 * log10(sqrt(e / (b - a)) + 1e-9); }
static float *render(double secs) { int total = (int)(secs * 44100); int16_t *b = calloc((size_t)total * 2, 2); float *x = calloc(total, 4);
    for (int pos = 0; pos < total; pos += 128) api->render_block(inst, b + pos * 2, (total - pos < 128) ? total - pos : 128);
    for (int i = 0; i < total; i++) x[i] = b[2 * i] / 32768.0f; free(b); return x; }

static void setstep_lock(int t, int st, int lock) { char v[64]; snprintf(v, sizeof v, "0,%d,%d:%02x80%02x0040%02x", t, st, 0x80 | 100, 64, lock); sp("seqd", v); }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    inst = api->create_instance(".", "{}");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    for (int t = 1; t < 16; t++) { char v[16]; snprintf(v, sizeof v, "%d,1", t); sp("mute", v); }
    sp("p01_type", "SpkEn"); sp("p01_dmd", "2"); sp("plkt", "0,111");
    const int NL = 33; int lets[4] = { 0, 19, 14, 26 };   /* A, S, M, SH */
    for (int i = 0; i < 4; i++) { int lock = 1 + (int)(((double)lets[i] / (NL - 1)) * 126 + 0.5); setstep_lock(0, i * 4, lock); }
    sp("bpm", "120"); sp("play", "1");
    float *x = render(2.2);
    double zc[4], rms[4];
    for (int i = 0; i < 4; i++) { int a = (int)((i * 0.5 + 0.02) * 44100), b = a + 12000; int z = 0; double e = 0; for (int k = a + 1; k < b; k++) { if ((x[k] > 0) != (x[k - 1] > 0)) z++; e += x[k] * x[k]; } zc[i] = z; rms[i] = 10 * log10(e / (b - a) + 1e-12); printf("   step %d letter %d: zero-crossings %d, level %.1f dB\n", i, lets[i], z, rms[i]); }
    OK(rms[0] > -50 && rms[1] > -50, "letters are heard (A %.1f dB, S %.1f dB)", rms[0], rms[1]);
    OK(fabs(zc[0] - zc[1]) > 100, "A and S sound different (zero-crossings %.0f vs %.0f)", zc[0], zc[1]);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed != 0;
}
