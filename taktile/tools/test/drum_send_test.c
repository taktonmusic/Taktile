/* drum track: the Delay and Reverb sends of a drum sound reach the shared delay / reverb (echoes and a tail after a short hit) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
static double tail(const char *key, const char *val) {
    inst = api->create_instance(".", "{}");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    sp("kind", "0,0"); sp("p17_type", "DHatCl"); sp("p17_dly", "0"); sp("p17_rvb", "0"); sp(key, val);
    sp("dhit", "16,110,0"); sp("dhit", "16,0,0");
    int16_t b[256]; double e = 0; int n = 0;
    for (int blk = 0; blk < 44100 / 128; blk++) { api->render_block(inst, b, 128); if (blk * 128 > 15000) for (int i = 0; i < 128; i++) { double x = b[2 * i] / 32768.0; e += x * x; n++; } }
    api->destroy_instance(inst); return sqrt(e / (n ? n : 1));
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    double d0 = tail("p17_dly", "0"), d1 = tail("p17_dly", "1.0"), r0 = tail("p17_rvb", "0"), r1 = tail("p17_rvb", "1.0");
    printf("   tail level: dry %.5f / delay send %.5f | dry %.5f / reverb send %.5f\n", d0, d1, r0, r1);
    OK(d1 > d0 * 3 + 1e-5, "a drum sound's Delay send makes echoes");
    OK(r1 > r0 * 3 + 1e-5, "a drum sound's Reverb send makes a tail");
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
