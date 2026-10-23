/* the three newest engines (ACID 303, SWARM, DX 6-OP FM): every sound plays, stays finite and below clipping, is not silent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api; static void *inst;
static void sp(const char *k, const char *v) { api->set_param(inst, k, v); }
#define N 88200
static float L[N];
static void run(const char *type) {
    inst = api->create_instance(".", "{}");
    for (int t = 1; t < 16; t++) { char v[16]; snprintf(v, sizeof v, "%d,1", t); sp("mute", v); }
    sp("p01_type", type);
    char v[64]; snprintf(v, sizeof v, "0,0,0,%02x80%02x004000", 0x80 | 100, 64); sp("st", v); sp("bpm", "120"); sp("play", "1");
    int16_t *b = calloc((size_t)N * 2, 2);
    for (int pos = 0; pos < N; pos += 128) api->render_block(inst, b + pos * 2, (N - pos < 128) ? N - pos : 128);
    for (int i = 0; i < N; i++) L[i] = b[2 * i] / 32768.0f; free(b); api->destroy_instance(inst);
}
static const char *ALL[] = { "AcdInit", "Acd303", "AcdSqr", "AcdSquel", "AcdDeep", "AcdRez", "AcdDrive", "AcdBurp", "AcdTranc", "AcdGoa", "SwmInit", "SwmFat", "SwmDrift", "SwmCloud", "SwmSwirl", "SwmPad", "SwmBass", "SwmLead", "SwmChoir", "SwmStab", "DxInit", "DxEPiano", "DxBass", "DxBrass", "DxBell", "DxMarimba", "DxOrgan", "DxPad", "DxLead", "DxReese" };
int main(int argc, char **argv) {
    if (argc < 2) { argc = 1 + (int)(sizeof ALL / sizeof ALL[0]); static char *av[64]; av[0] = argv[0]; for (int i = 1; i < argc; i++) av[i] = (char *)ALL[i - 1]; argv = av; }
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; api = move_plugin_init_v2(&h);
    int failed = 0; const char *dump = getenv("NS_DUMP");
    for (int i = 1; i < argc; i++) {
        run(argv[i]);
        double e1 = 0, pk = 0; int bad = 0;
        for (int k = 0; k < N; k++) { float x = L[k]; if (!(x == x) || fabsf(x) > 4.0f) bad++; if (fabs(x) > pk) pk = fabs(x); if (k < 44100) e1 += (double)x * x; }
        double tail = 0; for (int k = N - 4410; k < N; k++) tail += (double)L[k] * L[k];
        int ok = !bad && pk > 0.01 && pk < 0.999;
        printf("%s %-10s peak %.3f  level %.1f dB  tail %.5f%s\n", ok ? "ok  " : "FAIL", argv[i], pk, 20 * log10(sqrt(e1 / 44100.0) + 1e-9), sqrt(tail / 4410.0), bad ? "  NAN/INF" : "");
        if (!ok) failed++;
        if (dump) { char fn[128]; snprintf(fn, sizeof fn, "%s/%s.raw", dump, argv[i]); FILE *f = fopen(fn, "wb"); fwrite(L, 4, N, f); fclose(f); }
    }
    return failed != 0;
}
