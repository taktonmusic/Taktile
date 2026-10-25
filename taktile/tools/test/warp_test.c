/* Sampler FWD-REV: the sound runs forward to the end marker, back to the start marker and again, for as long as the pad is held; ONE / REV / LOOP are unchanged */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
#define N 22050   /* 0.25 s ramp, rising */
static void mkwav(void) {
    FILE *f = fopen("/tmp/ppsmp/ramp.wav", "wb"); int32_t d = N * 2, r = 44100, br = 88200, fl = 16 + d + 20; int16_t one = 1, ch = 1, ba = 2, bits = 16;
    int32_t sz = 16;
    fwrite("RIFF", 1, 4, f); int32_t tot = 36 + d; fwrite(&tot, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sz, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&d, 4, 1, f); for (int i = 0; i < N; i++) { int16_t v = (int16_t)(-12000 + 24000.0 * i / N); fwrite(&v, 2, 1, f); } fclose(f); (void)fl;
}
static double dur(const char *bpm, const char *warp, const char *md) {     /* seconds until the sound is gone (pad held) */
    inst = api->create_instance(".", "{}"); sp("bpm", bpm); sp("p01_type", "359"); sp("p01_smp", "0"); sp("p01_smd", md); sp("p01_dln", warp); sp("p01_decay", "1.0"); sp("p01_att", "0"); sp("p01_sgn", "0.5");
    sp("pad", "0,110"); int16_t b[256]; double last = 0; int pos = 0, quiet = 0;
    for (int blk = 0; blk < 44100 * 6 / 128; blk++) {
        api->render_block(inst, b, 128); pos += 128; double e = 0; for (int i = 0; i < 128; i++) e += fabs((double)b[2 * i]);
        if (e > 200) { last = pos / 44100.0; quiet = 0; } else quiet++;
    }
    api->destroy_instance(inst); return last;
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    system("rm -rf /tmp/ppsmp; mkdir -p /tmp/ppsmp"); setenv("TAKTILE_SMP_DIR", "/tmp/ppsmp", 1); mkwav();
    api = move_plugin_init_v2(&h);
    double d0 = dur("120", "0", "0"); OK(d0 > 0.4 && d0 < 0.6, "no warp: the 0.5 s sample lasts 0.5 s (%.2f)", d0);
    double d1 = dur("120", "1", "0"); OK(d1 > 0.9 && d1 < 1.15, "1/2 bar at 120 BPM = 1.0 s (%.2f)", d1);
    double d2 = dur("60", "1", "0"); OK(d2 > 1.9 && d2 < 2.2, "1/2 bar at 60 BPM = 2.0 s (%.2f)", d2);
    double d3 = dur("120", "2", "0"); OK(d3 > 1.9 && d3 < 2.2, "1 bar at 120 BPM = 2.0 s (%.2f)", d3);
    double d4 = dur("120", "1", "2"); OK(d4 > 5.5, "with LOOP it keeps going (%.2f)", d4);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
