/* Sampler track: a low and a high note held together. Voices = Mono plays only one of them, Voices = Poly plays both */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
#define N 44100
static void mkwav(void) {
    FILE *f = fopen("/tmp/ppsmp/sine.wav", "wb"); int32_t d = N * 2, r = 44100, br = 88200; int16_t one = 1, ch = 1, ba = 2, bits = 16; int32_t sz = 16;
    fwrite("RIFF", 1, 4, f); int32_t tot = 36 + d; fwrite(&tot, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sz, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&d, 4, 1, f); for (int i = 0; i < N; i++) { int16_t v = (int16_t)(12000.0 * sin(6.283185307 * 441.0 * i / 44100.0)); fwrite(&v, 2, 1, f); } fclose(f);
}
static double goertzel(const float *x, int n, double f) { double w = 6.283185307 * f / 44100.0, c = 2 * cos(w), s0, s1 = 0, s2 = 0; for (int i = 0; i < n; i++) { s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; } return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n; }
static void run(const char *poly, double *lo, double *hi) {
    inst = api->create_instance(".", "{}");
    sp("p01_type", "359"); sp("p01_smp", "0"); sp("p01_smd", "1.0"); sp("p01_decay", "1.0"); sp("p01_att", "0"); sp("p01_poly", poly); sp("p01_rvb", "0"); sp("p01_dly", "0");
    sp("npad", "32,100,0");                      /* a low note (pad 32 = -12 semitones: 220 Hz) */
    int16_t b[256]; for (int i = 0; i < 40; i++) api->render_block(inst, b, 128);
    sp("npad", "56,100,0");                      /* ... and a high one while the first is still held (+12 semitones: 882 Hz) */
    int n = 8192; float *x = calloc(n, 4); for (int i = 0; i < n / 128; i++) { api->render_block(inst, b, 128); for (int j = 0; j < 128; j++) x[i * 128 + j] = b[2 * j] / 32768.0f; }
    *lo = goertzel(x, n, 220.5); *hi = goertzel(x, n, 882.0);
    free(x); api->destroy_instance(inst);
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    system("rm -rf /tmp/ppsmp; mkdir -p /tmp/ppsmp"); setenv("TAKTILE_SMP_DIR", "/tmp/ppsmp", 1); mkwav();
    api = move_plugin_init_v2(&h);
    double lo, hi; run("0", &lo, &hi); printf("   Mono: low note %.5f, high note %.5f\n", lo, hi);
    OK(hi > 0.002 && lo < hi * 0.3, "Mono: the new high note replaces the held low note");
    run("1", &lo, &hi); printf("   Poly: low note %.5f, high note %.5f\n", lo, hi);
    OK(hi > 0.002 && lo > 0.002, "Poly: low and high note sound together");
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
