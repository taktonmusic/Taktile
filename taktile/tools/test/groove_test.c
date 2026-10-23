/* groove templates: odd sixteenths come late by the template's value x amount; OFF leaves the timing alone */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst; static int failed = 0;
#define OK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failed++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
/* returns the mean gap (ms) from an even onset to the next odd one, and from odd to even */
static void gaps(const char *tmpl, const char *amt, double *ev2od, double *od2ev) {
    inst = api->create_instance(".", "{}"); sp("bpm", "120");
    for (int t = 0; t < 16; t++) { char v[24]; snprintf(v, sizeof v, "%d,1.0", t); sp("tvol", v); }
    sp("p01_decay", "0.05"); sp("p01_att", "0"); sp("m_grv", tmpl); sp("m_gam", amt);
    for (int i = 0; i < 16; i++) { char st[64]; snprintf(st, sizeof st, "0,0,%d,%02x%02x%02x%02x%02x%02x", i, 0x80 | 110, 0, 64, 0, 0x40, 0); sp("st", st); }
    sp("play", "1");
    int total = 44100 * 6; int16_t b[256]; int hot = 0, cnt = 0; double last = -100, t0[400];
    for (int pos = 0; pos < total; pos += 128) {
        api->render_block(inst, b, 128);
        for (int i = 0; i < 128; i += 8) {
            double e = 0; for (int k = 0; k < 8; k++) { double x = b[2 * (i + k)] / 32768.0; e += x * x; } e = sqrt(e / 8);
            double ms = (pos + i) / 44.1;
            if (e > 0.02 && !hot && ms - last > 25.0) { hot = 1; if (cnt < 400) t0[cnt] = ms; cnt++; last = ms; } else if (e < 0.004) hot = 0;
        }
    }
    double a = 0, c = 0; int na = 0, nc = 0;
    for (int i = 4; i + 1 < cnt && i < 399; i++) { double g = t0[i + 1] - t0[i]; if (i % 2 == 0) { a += g; na++; } else { c += g; nc++; } }
    *ev2od = a / (na ? na : 1); *od2ev = c / (nc ? nc : 1); api->destroy_instance(inst);
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    double a, b;
    gaps("OFF", "1.0", &a, &b); OK(fabs(a - 125) < 4 && fabs(b - 125) < 4, "OFF: even grid (%.1f / %.1f ms, expect 125)", a, b);
    gaps("MPC", "1.0", &a, &b); OK(fabs(a - 145) < 5 && fabs(b - 105) < 5, "MPC 100%%: odd steps 20 ms late (%.1f / %.1f ms)", a, b);
    gaps("MPC", "0.5", &a, &b); OK(fabs(a - 135) < 5 && fabs(b - 115) < 5, "MPC 50%%: half of it (%.1f / %.1f ms)", a, b);
    gaps("SHUFFLE", "1.0", &a, &b); OK(fabs(a - 166) < 5 && fabs(b - 84) < 5, "SHUFFLE 100%%: odd steps 41 ms late (%.1f / %.1f ms)", a, b);
    gaps("MPC", "0.0", &a, &b); OK(fabs(a - 125) < 4 && fabs(b - 125) < 4, "amount 0 = no groove (%.1f / %.1f ms)", a, b);
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed ? 1 : 0;
}
