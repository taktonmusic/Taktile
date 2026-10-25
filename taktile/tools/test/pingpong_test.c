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
#define N 11025   /* 0.25 s ramp, rising */
static void mkwav(void) {
    FILE *f = fopen("/tmp/ppsmp/ramp.wav", "wb"); int32_t d = N * 2, r = 44100, br = 88200, fl = 16 + d + 20; int16_t one = 1, ch = 1, ba = 2, bits = 16;
    int32_t sz = 16;
    fwrite("RIFF", 1, 4, f); int32_t tot = 36 + d; fwrite(&tot, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sz, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&d, 4, 1, f); for (int i = 0; i < N; i++) { int16_t v = (int16_t)(-12000 + 24000.0 * i / N); fwrite(&v, 2, 1, f); } fclose(f); (void)fl;
}
/* mode value -> slope sign per 1024-frame window while the pad is held for 1.2 s, then after release */
static void run(const char *md, int *sgn, double *en, int nw, int *tailE) {
    inst = api->create_instance(".", "{}"); sp("p01_type", "359"); sp("p01_smp", "0"); sp("p01_smd", md); sp("p01_decay", "1.0"); sp("p01_att", "0"); sp("p01_srel", "0");
    sp("p01_sgn", "0.5"); sp("p01_pan", "0.5");
    sp("pad", "0,110");
    int16_t b[256]; int w = 0, pos = 0; double a = 0, c = 0; int first = 1, last = 0;
    while (w < nw) {
        api->render_block(inst, b, 128);
        for (int i = 0; i < 128; i++) { double x = b[2 * i]; if (pos == 0 && i == 0) {} a += x * x; }
        c = b[2 * 127]; if (pos % 1024 == 0) { if (!first) sgn[w - 1 < 0 ? 0 : 0] = 0; }
        pos += 128; if (pos % 1024 == 0) { double v = b[2 * 127]; sgn[w] = first ? 0 : (v > last ? 1 : (v < last ? -1 : 0)); last = (int)v; first = 0; en[w] = sqrt(a / 1024); a = 0; w++; }
        if (pos == 44100 * 12 / 10 / 128 * 128 && 0) {}
        (void)c;
    }
    api->destroy_instance(inst); (void)tailE;
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    system("rm -rf /tmp/ppsmp; mkdir -p /tmp/ppsmp"); setenv("TAKTILE_SMP_DIR", "/tmp/ppsmp", 1); mkwav();
    api = move_plugin_init_v2(&h);
    int sg[64]; double en[64]; int tail = 0;
    /* pad held the whole time: a 0.25 s ramp played for 1.4 s */
    run("0.75", sg, en, 60, &tail);
    int ch = 0, up = 0, dn = 0; for (int i = 2; i < 60; i++) { if (sg[i] > 0) up++; if (sg[i] < 0) dn++; if (sg[i] && sg[i - 1] && sg[i] != sg[i - 1]) ch++; }
    double late = 0; for (int i = 50; i < 60; i++) late += en[i];
    OK(up > 8 && dn > 8, "FWD-REV: rises and falls (%d up, %d down windows)", up, dn);
    OK(ch >= 4, "FWD-REV: changes direction repeatedly (%d times in 1.4 s)", ch);
    OK(late > 1000, "FWD-REV: still sounding after 1.2 s while the pad is held (energy %.0f)", late);
    run("1", sg, en, 60, &tail);
    late = 0; for (int i = 50; i < 60; i++) late += en[i];
    OK(late > 1000, "LOOP still loops (energy %.0f)", late);
    run("0", sg, en, 60, &tail);
    late = 0; for (int i = 50; i < 60; i++) late += en[i];
    OK(late < 100, "ONE shot has ended after the sample (energy %.0f)", late);
    run("0.5", sg, en, 60, &tail);
    int dn2 = 0; for (int i = 2; i < 8; i++) if (sg[i] < 0) dn2++;
    OK(dn2 >= 3, "REV runs backwards (%d falling windows)", dn2);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
