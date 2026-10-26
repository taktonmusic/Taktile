/* sidechain with a kick played LIVE on the first drum track (pad 1) ducking a SAMPLER track; Duck Src ALL on another sound; Duck 0 = no effect */
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
static double rms(const float *x, int a, int b) { double e = 0; for (int i = a; i < b; i++) e += x[i] * x[i]; return sqrt(e / (b - a)); }
/* kick lane = 16 (pad 1 of drum track 1). duck: Duck amount of the sampler; hitlane: which drum pad is played live */
static double run(const char *duck, int hitlane) {
    inst = api->create_instance(".", "{}");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    sp("kind", "0,0");
    sp("p02_type", "359"); sp("p02_smp", "0"); sp("p02_smd", "1.0"); sp("p02_decay", "1.0"); sp("p02_att", "0"); sp("p02_poly", "1"); sp("p02_rvb", "0"); sp("p02_dly", "0");
    sp("p02_duck", duck); sp("p02_duckr", "0.4");
    { char v[16]; snprintf(v, sizeof v, "p%02d_vol", hitlane + 1); sp(v, "0"); }       /* the kick is silent: only the sampler is measured */
    sp("sel", "1"); sp("npad", "44,100,0");
    int16_t b[256]; float *x = calloc(44100, 4); int pos = 0;
    for (int blk = 0; blk < 20; blk++) api->render_block(inst, b, 128);     /* settle */
    for (int hit = 0; hit < 2; hit++) {
        char v[32]; snprintf(v, sizeof v, "%d,110,0", hitlane); sp("dhit", v); snprintf(v, sizeof v, "%d,0,0", hitlane); sp("dhit", v);
        for (int blk = 0; blk < 22050 / 128; blk++) { api->render_block(inst, b, 128); for (int j = 0; j < 128 && pos < 44100; j++) x[pos++] = b[2 * j] / 32768.0f; }
    }
    double after = rms(x, 1500, 5000), before = rms(x, 17000, 21000);
    free(x); api->destroy_instance(inst); return after / (before + 1e-9);
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    system("rm -rf /tmp/ppsmp; mkdir -p /tmp/ppsmp"); setenv("TAKTILE_SMP_DIR", "/tmp/ppsmp", 1); mkwav();
    api = move_plugin_init_v2(&h);
    double r0 = run("0", 16), r1 = run("1", 16), r2 = run("1", 17);
    printf("   level just after the hit / just before the next one: Duck 0 %.2f, Duck 100%% %.2f, other pad %.2f\n", r0, r1, r2);
    OK(r0 > 0.85, "Duck 0: the sampler is not touched by the kick");
    OK(r1 < 0.6, "Duck 100%%: a live hit of pad 1 ducks the sampler track");
    OK(r2 > 0.85, "a live hit of pad 2 does not duck");
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
