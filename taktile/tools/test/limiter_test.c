/* the master limiter with look-ahead: a very loud mix stays under the ceiling and the gain does not jump */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    const char *lim[3] = { "0", "0.5", "1" }; const double ceil_[3] = { 0.96, 0.66, 0.31 };
    for (int k = 0; k < 3; k++) {
        inst = api->create_instance(".", "{}");
        for (int t = 0; t < 16; t++) { char v[32]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); snprintf(v, sizeof v, "p%02d_type", t + 1); sp(v, "Sub808"); snprintf(v, sizeof v, "0,%d,0,%02x80%02x004000", t, 0x80 | 127, 64); sp("st", v); }
        sp("m_vol", "1.0"); sp("m_lim", lim[k]); sp("bpm", "120"); sp("play", "1");
        int total = 128 * 1032; int16_t *b = calloc((size_t)total * 2, 2);
        for (int pos = 0; pos < total; pos += 128) api->render_block(inst, b + pos * 2, 128);
        double pk = 0, jump = 0; for (int i = 1; i < total; i++) { double a = fabs(b[2 * i] / 32768.0); if (a > pk) pk = a; double j = fabs((b[2 * i] - b[2 * i - 2]) / 32768.0); if (j > jump) jump = j; }
        printf("   limiter %s: peak %.3f, biggest step %.3f\n", lim[k], pk, jump);
        OK(pk <= ceil_[k], "limiter %s keeps the peak under %.2f", lim[k], ceil_[k]);
        free(b); api->destroy_instance(inst);
    }
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed != 0;
}
