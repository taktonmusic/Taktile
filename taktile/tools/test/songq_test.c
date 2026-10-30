/* Song (the position read back is one ahead: 17 = first step of the new bar): the tempo of the next scene waits for the bar line (queued key qbpm + qbar); mutes and transposition are queued the same way */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
static int gs(void) { char b[64]; api->get_param(inst, "pos", b, sizeof b); int p, g; sscanf(b, "%d,%d", &p, &g); return g; }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    inst = api->create_instance(".", "{}"); int16_t b[256];
    sp("bpm", "120"); sp("play", "1");
    long frames = 0; int queued = 0, g0 = -1, g1 = -1; long f0 = 0, f1 = 0, f2 = 0, f3 = 0; int gprev = -1;
    for (long i = 0; i < 44100L * 12 / 128; i++) {
        api->render_block(inst, b, 128); frames += 128; int g = gs();
        if (!queued && g >= 5) { sp("qbpm", "240"); sp("qsctr", "7"); sp("qmute", "0,1"); sp("qbar", "1"); queued = 1; g0 = g; f0 = frames; }
        if (queued && g >= 12 && f1 == 0) { f1 = frames; g1 = g; }
        if (queued && g >= 17 && f2 == 0) { f2 = frames; }
        if (queued && g >= 21 && f3 == 0) { f3 = frames; }
        gprev = g;
    }
    double per_before = (double)(f1 - f0) / (g1 - g0), per_after = (double)(f3 - f2) / 4.0;
    printf("frames per step before the bar line %.0f, after %.0f\n", per_before, per_after);
    OK(per_before > 5200 && per_before < 5800, "tempo is still 120 until the bar line");
    OK(per_after > 2500 && per_after < 3100, "tempo is 240 from the bar line on");
    api->destroy_instance(inst);
    return failed ? 1 : 0;
}
