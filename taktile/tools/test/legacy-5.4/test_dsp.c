#include "../../src/pad_studio.c"
#include <stdio.h>
static const char *g_dir;
static int chk(const char *name, int ch, double mean, double tol) {
    smp_t m; memset(&m, 0, sizeof m); unsigned before = smp_bytes;
    char path[512]; snprintf(path, sizeof path, "%s/%s", g_dir, name);
    int ok = smp_load(&m, path);
    if (!ok) { printf("FAIL %s: load failed\n", name); return 1; }
    double acc = 0; for (int i = 100; i < 4000; i++) acc += m.d[i] / 32768.0; acc /= 3900.0;
    unsigned used = smp_bytes - before;
    int bad = (m.ch != ch) || (fabs(acc - mean) > tol) || (m.n != 4410) || (used != (unsigned)(4410 * 2));
    printf("%s %-9s ch=%d frames=%d mean=%.3f (want %.3f) bytes=%u\n", bad ? "FAIL" : "ok  ", name, m.ch, m.n, acc, mean, used);
    free(m.d); smp_bytes = before; return bad;
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: test_dsp <wav dir>\n"); return 2; }
    g_dir = argv[1];
    int f = 0;
    f += chk("st16.wav", 1, 0.2, 0.01); f += chk("st24.wav", 1, 0.2, 0.01); f += chk("stf32.wav", 1, 0.2, 0.01);
    f += chk("mono16.wav", 1, 0.25, 0.01); f += chk("u8.wav", 1, 0.2, 0.02); f += chk("six.wav", 1, 0.2, 0.01);
    void *in = create_instance("/tmp", "{}");
    char b[64]; set_param(in, "bpm", "120"); get_param(in, "recmaxbars", b, sizeof b); printf("recmaxbars @120 = %s (want 16)\n", b); f += strcmp(b, "16") != 0;
    set_param(in, "bpm", "90");  get_param(in, "recmaxbars", b, sizeof b); printf("recmaxbars @90  = %s (want 15)\n", b); f += strcmp(b, "15") != 0;
    set_param(in, "bpm", "60");  get_param(in, "recmaxbars", b, sizeof b); printf("recmaxbars @60  = %s (want 10)\n", b); f += strcmp(b, "10") != 0;
    printf(f ? "FAILED %d\n" : "ALL PASSED\n", f); return f != 0;
}
