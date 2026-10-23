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
static void setup(const char *tape, const char *hiss, int notes) {
    inst = api->create_instance(".", "{}");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    for (int t = 1; t < 16; t++) { char v[16]; snprintf(v, sizeof v, "%d,1", t); sp("mute", v); }
    sp("m_tape", tape); sp("m_hiss", hiss);
    if (notes) { sp("p01_type", "Sub808"); for (int s = 0; s < 16; s += 4) setstep(0, s, 110); sp("bpm", "120"); sp("play", "1"); }
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    setup("0", "0", 0); float *x = render(1.0); double s0 = rms_db(x, 4000, 40000); free(x); api->destroy_instance(inst);
    setup("0", "0.25", 0); x = render(1.0); double s1 = rms_db(x, 4000, 40000); free(x); api->destroy_instance(inst);
    setup("0", "1", 0); x = render(1.0); double s2 = rms_db(x, 4000, 40000); free(x); api->destroy_instance(inst);
    printf("   hiss on silence: 0 -> %.1f dB, 25%% -> %.1f dB, 100%% -> %.1f dB\n", s0, s1, s2);
    OK(s1 > -75 && s2 > -52 && s2 > s1 + 6, "HISS is clearly audible (25%% %.1f dB, 100%% %.1f dB)", s1, s2);
    setup("0", "0", 1); float *a = render(2.0); double t0 = rms_db(a, 4000, 80000); api->destroy_instance(inst);
    setup("1", "0", 1); float *b = render(2.0); double t1 = rms_db(b, 4000, 80000);
    double e = 0, r = 0; for (int i = 4000; i < 80000; i++) { double d = b[i] - a[i]; e += d * d; r += (double)a[i] * a[i]; }
    double rel = sqrt(e / r); free(a); free(b); api->destroy_instance(inst);
    printf("   tape on a kick: off %.1f dB, full %.1f dB, difference signal = %.0f%% of the signal\n", t0, t1, rel * 100);
    OK(rel > 0.1, "TAPE at 100%% clearly changes the sound (difference %.0f%% of the signal, must be above 10%%)", rel * 100);
    setup("0", "1", 0); { int n = 44100; int16_t *bb = calloc((size_t)n * 2, 2); for (int p = 0; p < n; p += 128) api->render_block(inst, bb + p * 2, (n - p < 128) ? n - p : 128);
      int diff = 0; for (int i = 4000; i < n; i++) if (bb[2 * i] != bb[2 * i + 1]) diff++; free(bb);
      OK(diff == 0, "HISS is mono (left = right, %d samples differ)", diff); } api->destroy_instance(inst);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed != 0;
}
