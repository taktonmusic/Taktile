/* Native DSP test of the NEW ENGINES parameters (e0..e9): every engine sounds, the knobs change the sound, extreme values stay safe,
   a changed sound drops the old edits, and "preset value" (-1) is the same as the explicitly written value. Built with ASAN + UBSAN. */
#include "../../src/pad_studio.c"
#include <stdio.h>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void setk(void *in, const char *fmt, int a, const char *key, const char *val) {   /* "p01_e3" style pad key */
    char k[32]; snprintf(k, sizeof k, fmt, a, key); set_param(in, k, val);
}
static void setf(void *in, int lane, const char *key, double v) { char b[32]; snprintf(b, sizeof b, "%.5f", v); setk(in, "p%02d_%s", lane + 1, key, b); }
static void seti(void *in, int lane, const char *key, int v)    { char b[32]; snprintf(b, sizeof b, "%d", v); setk(in, "p%02d_%s", lane + 1, key, b); }

/* plays one note on pad 0 for `blocks` blocks, returns the summed energy; *sum is a checksum of the samples, *bad counts clipped / silent problems */
static double play(void *in, int blocks, double *sum, int *peak) {
    int16_t out[256]; double e = 0, cs = 0; int pk = 0;
    ((inst_t *)in)->rng = 0x2545F491u;                                  /* the same noise every time: two plays of one sound are comparable */
    set_param(in, "npad", "0,100,0");
    for (int b = 0; b < blocks; b++) {
        render_block(in, out, 128);
        for (int i = 0; i < 256; i++) { e += (double)out[i] * out[i]; cs += out[i] * (double)((i % 7) + 1); if (abs(out[i]) > pk) pk = abs(out[i]); }
        if (b == blocks / 2) set_param(in, "npad", "0,0,0");          /* release halfway: gate engines go through their release too */
    }
    set_param(in, "panic", "1");
    if (sum) *sum = cs; if (peak) *peak = pk;
    return e;
}
static char rowbuf[4096];
/* builds "lane:v0,v1,..." like the UI does: the defaults of every parameter, a sound number, and e0..e9 (NULL = leave out) */
static void row(char *buf, int len, int lane, int type, const double *e, int count) {
    int n = snprintf(buf, (size_t)len, "%d:", lane);
    for (int k = 0; k < count; k++) {
        double v = k == PP_TYPE ? type : (k >= PP_E0 && k <= PP_E9 ? (e ? e[k - PP_E0] : -1.0) : PD[k].def);
        n += snprintf(buf + n, (size_t)(len - n), "%s%.5f", k ? "," : "", v);
    }
}
static unsigned lcg = 12345;
static double rnd(void) { lcg = lcg * 1664525u + 1013904223u; return (double)(lcg >> 8) / 16777216.0; }

int main(void) {
    move_plugin_init_v2(NULL);                                          /* builds the sound tables like the host does */
    void *in = create_instance("/tmp", "{}");
    set_param(in, "key", "0");
    CHECK(in != NULL, "create_instance");
    static const char *EN[5] = { "MODAL", "FM", "VOX", "CHIP", "MORPH" };
    const int init0 = T_V4 + N_V4 - 5;
    char b[64];

    /* 1. every INIT sound plays and is neither silent nor clipping hard */
    for (int e = 0; e < 5; e++) {
        seti(in, 0, "type", init0 + e);
        double en = play(in, 120, NULL, NULL); int pk; play(in, 120, NULL, &pk);
        printf("INIT %-6s energy=%.3g peak=%d name=%s\n", EN[e], en, pk, TYPE_NAME[init0 + e]);
        CHECK(en > 1e6, "%s INIT is silent", EN[e]);
        CHECK(pk < 32767, "%s INIT clips", EN[e]);
        CHECK(VAR4[init0 + e - T_V4].eng == e, "%s INIT has the wrong engine", EN[e]);
        get_param(in, "p01_e0", b, sizeof b); CHECK(strcmp(b, "-1.000") == 0, "e0 after type change = %s (want -1.000)", b);
    }

    /* 2. all 152 presets: the preset's own values written out as e0..e9 sound the same as the sentinel -1 */
    int worst = 0;
    for (int i = 0; i < N_V4; i++) {
        const nx_t *n = &VAR4[i];
        seti(in, 0, "type", T_V4 + i);
        double s1, s2; double e1 = play(in, 60, &s1, NULL);
        for (int k = 0; k < 10; k++) {
            const double lo = ENG_LO[n->eng][k], hi = ENG_HI[n->eng][k];
            setf(in, 0, k == 0 ? "e0" : k == 1 ? "e1" : k == 2 ? "e2" : k == 3 ? "e3" : k == 4 ? "e4" : k == 5 ? "e5" : k == 6 ? "e6" : k == 7 ? "e7" : k == 8 ? "e8" : "e9",
                 hi > lo ? (n->p[k] - lo) / (hi - lo) : 0.0);
        }
        double e2 = play(in, 60, &s2, NULL);
        const double dev = fabs(e1 - e2) / (e1 + 1.0);
        if (dev > 0.002) { worst++; printf("  preset %s (%s): energy %.6g vs %.6g (dev %.4f)\n", TYPE_NAME[T_V4 + i], EN[n->eng], e1, e2, dev); }
        CHECK(e1 > 100.0, "preset %s is silent", TYPE_NAME[T_V4 + i]);
    }
    CHECK(worst == 0, "%d presets sound different with their own values written out", worst);

    /* 3. the knobs change the sound (a few examples per engine) */
    struct { int eng; const char *key; double a, b; } kn[] = {
        { 0, "e1", 0.1, 0.9 }, { 0, "e0", 0.0, 0.5 }, { 1, "e1", 0.0, 0.5 }, { 1, "e8", 0.0, 0.5 }, { 2, "e0", 0.0, 0.7 }, { 3, "e0", 0.1, 0.5 }, { 3, "e5", 0.0, 0.5 }, { 4, "e0", 0.0, 0.5 }, { 4, "e7", 0.1, 0.6 },
    };
    for (unsigned i = 0; i < sizeof kn / sizeof kn[0]; i++) {
        seti(in, 0, "type", init0 + kn[i].eng);
        if (kn[i].eng == 1) setf(in, 0, "e3", 0.5);                      /* FM: the second modulator must be audible for the algorithm to matter */
        setf(in, 0, kn[i].key, kn[i].a); double c1, c2; play(in, 60, &c1, NULL);
        setf(in, 0, kn[i].key, kn[i].b); play(in, 60, &c2, NULL);
        CHECK(fabs(c1 - c2) > 1.0, "%s %s: both values sound identical", EN[kn[i].eng], kn[i].key);
    }

    /* 4. a held note follows a knob (FM index while the note rings) */
    seti(in, 0, "type", init0 + 1); setf(in, 0, "e1", 0.0);
    { int16_t o[256]; double c1 = 0, c2 = 0; set_param(in, "npad", "0,100,0");
      for (int k = 0; k < 40; k++) { render_block(in, o, 128); } for (int i = 0; i < 256; i++) c1 += o[i] * (double)((i % 7) + 1);
      setf(in, 0, "e1", 0.9);
      for (int k = 0; k < 6; k++) { render_block(in, o, 128); } for (int i = 0; i < 256; i++) c2 += o[i] * (double)((i % 7) + 1);
      set_param(in, "npad", "0,0,0"); set_param(in, "panic", "1");
      CHECK(fabs(c1 - c2) > 1.0, "held note does not follow the knob"); }

    /* 5. extreme and random values never break anything (ASAN / UBSAN would stop here) */
    for (int e = 0; e < 5; e++) {
        seti(in, 0, "type", init0 + e);
        for (int round = 0; round < 60; round++) {
            for (int k = 0; k < 10; k++) {
                char key[8]; snprintf(key, sizeof key, "e%d", k);
                const double v = round == 0 ? 0.0 : round == 1 ? 1.0 : round == 2 ? -1.0 : round == 3 ? 5.0 : rnd();
                setf(in, 0, key, v);
            }
            int pk; play(in, 24, NULL, &pk);
        }
    }

    /* 6. another sound drops the edits of the old one (the sound number is the same type of change from the UI and the DSP) */
    seti(in, 0, "type", init0 + 1); setf(in, 0, "e3", 0.7); get_param(in, "p01_e3", b, sizeof b); CHECK(strcmp(b, "0.700") == 0, "e3 = %s", b);
    seti(in, 0, "type", init0 + 1); get_param(in, "p01_e3", b, sizeof b); CHECK(strcmp(b, "0.700") == 0, "same sound again must keep e3, got %s", b);
    seti(in, 0, "type", init0 + 2); get_param(in, "p01_e3", b, sizeof b); CHECK(strcmp(b, "-1.000") == 0, "new sound must reset e3, got %s", b);

    /* 7. whole rows (setpad) carry -1 and 0..1 values and clamp the rest */
    { double e[10] = { 0.25, -1, 0.9, 7, -5, 0.125, -1, 0, 1, 0.5 };
      row(rowbuf, sizeof rowbuf, 1, init0 + 1, e, PP_N); set_param(in, "setpad", rowbuf); }
    get_param(in, "p01_e0", b, sizeof b); CHECK(strcmp(b, "0.250") == 0, "setpad e0 = %s", b);
    get_param(in, "p01_e1", b, sizeof b); CHECK(strcmp(b, "-1.000") == 0, "setpad e1 = %s", b);
    get_param(in, "p01_e3", b, sizeof b); CHECK(strcmp(b, "1.000") == 0, "setpad e3 must clamp to 1, got %s", b);
    get_param(in, "p01_e4", b, sizeof b); CHECK(strcmp(b, "-1.000") == 0, "setpad e4 must clamp to -1, got %s", b);
    get_param(in, "p01_e5", b, sizeof b); CHECK(strcmp(b, "0.125") == 0, "setpad e5 = %s", b);

    /* 8. an old project row (only the first 61 values) must not touch e0..e9; a reset gives -1 again */
    seti(in, 1, "type", init0 + 1); setf(in, 1, "e2", 0.3);
    row(rowbuf, sizeof rowbuf, 2, init0 + 1, NULL, PP_E0); set_param(in, "setpad", rowbuf);
    get_param(in, "p02_e2", b, sizeof b); CHECK(strcmp(b, "0.300") == 0, "a short row leaves e2 as it was, got %s", b);
    set_param(in, "reset", "2"); get_param(in, "p02_e2", b, sizeof b); CHECK(strcmp(b, "-1.000") == 0, "reset_pad must give -1, got %s", b);

    /* 9. clip sounds (csnd) take the same rows */
    { double e[10] = { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };
      row(rowbuf, sizeof rowbuf, 1, init0 + 3, e, PP_N); char cs[4096]; snprintf(cs, sizeof cs, "3,0%s", strchr(rowbuf, ':')); set_param(in, "csnd", cs); }
    CHECK(in != NULL, "csnd");

    /* 10. ARP OCT: the arpeggio reaches 1..4 octaves (Up, Down, UpDn, Rand) */
    { inst_t *st = (inst_t *)in;
      for (int oc = 0; oc < 4; oc++) {
          st->pp[0][PP_AOCT] = oc / 3.0f;
          for (int mode = 1; mode <= 4; mode++) {
              st->pp[0][PP_ARP] = (float)mode; int mx = -999, mn = 999;
              for (int n = 0; n < 400; n++) { const int o = arp_offset(st, 0, n); if (o > mx) mx = o; if (o < mn) mn = o; }
              CHECK(mn >= 0 && mx >= 12 * oc && mx < 12 * (oc + 1) + 12, "arp mode %d oct %d: range %d..%d", mode, oc + 1, mn, mx);
          }
      }
      st->pp[0][PP_ARP] = 1.0f; st->pp[0][PP_AOCT] = 0.0f; setf(in, 0, "aoct", 1.0);
      get_param(in, "p01_aoct", b, sizeof b); CHECK(strcmp(b, "1.000") == 0, "aoct = %s", b);
      setf(in, 0, "aoct", 7.0); get_param(in, "p01_aoct", b, sizeof b); CHECK(strcmp(b, "1.000") == 0, "aoct clamps to 1, got %s", b);
      seti(in, 0, "type", init0 + 1); get_param(in, "p01_aoct", b, sizeof b); CHECK(strcmp(b, "1.000") == 0, "a new sound keeps ARP OCT, got %s", b); }

    /* 11. NUDGE left and right: a step with a negative nudge plays before its own step, a positive one after it */
    {
        const double sr = (double)((inst_t *)in)->sr; double on[3]; const int nud[3] = { 0, 10, -10 };
        for (int k = 0; k < 3; k++) {
            set_param(in, "panic", "1"); set_param(in, "play", "0"); set_param(in, "bpm", "120"); set_param(in, "len", "0,16"); seti(in, 0, "type", 0);
            for (int i = 0; i < 16; i++) set_param(in, "st", "0,0,0,000000000000");
            char hx[64]; snprintf(hx, sizeof hx, "0,0,4,E4804000%02X00", 0x40 | (nud[k] & 0x3F)); set_param(in, "st", hx);
            set_param(in, "play", "1");
            int16_t o[256]; long n = 0; on[k] = -1;
            for (int b = 0; b < 400 && on[k] < 0; b++) { render_block(in, o, 128); for (int i = 0; i < 128; i++) { if (abs(o[2 * i]) > 600 && on[k] < 0) on[k] = (double)(n + i); } n += 128; }
            set_param(in, "play", "0");
        }
        const double L = sr * 60.0 / 120.0 / 4.0;
        printf("nudge onsets: 0 -> %.0f, +25%% -> %.0f, -25%% -> %.0f (L = %.0f)\n", on[0], on[1], on[2], L);
        CHECK(on[0] > 0 && on[1] > 0 && on[2] > 0, "nudge: a note was heard");
        CHECK(fabs((on[1] - on[0]) - 0.25 * L) < 8.0, "positive nudge: %.0f samples later (want %.0f)", on[1] - on[0], 0.25 * L);
        CHECK(fabs((on[0] - on[2]) - 0.25 * L) < 8.0, "negative nudge: %.0f samples earlier (want %.0f)", on[0] - on[2], 0.25 * L);
    }

    destroy_instance(in);
    printf(fails ? "FAILED %d\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
