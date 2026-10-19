/* MIC test: level meter, take, wait-for-sound, early stop, WAV file, no input, CPU cost of the mic code. Desktop only (fake mailbox). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
static plugin_api_v2_t *api;
static float fake_bpm(void) { return 120.0f; }
static double fake_beat(void) { return 0.0; }
static void sp(void *i, const char *k, const char *v) { api->set_param(i, k, v); }
static int failed = 0;
#define OK(c, ...) do { int ok_ = (c); printf("%s ", ok_ ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!ok_) failed++; } while (0)
static uint8_t mail[4096];
static void fill(double amp, int left, int right) {
    static double ph = 0; int16_t *in = (int16_t *)(mail + MOVE_AUDIO_IN_OFFSET);
    for (int i = 0; i < 128; i++) { double v = amp * sin(ph); ph += 2 * M_PI * 1000.0 / 44100.0; in[2 * i] = left ? (int16_t)v : 0; in[2 * i + 1] = right ? (int16_t)v : 0; }
}
static void blocks(void *inst, int n, double amp, int l, int r) { int16_t out[256]; for (int b = 0; b < n; b++) { fill(amp, l, r); api->render_block(inst, out, 128); } }
static int geti(void *inst, const char *k, int idx) { char b[128]; if (api->get_param(inst, k, b, sizeof b) < 0) return -1; const char *p = b; for (int i = 0; i < idx; i++) { p = strchr(p, ','); if (!p) return -1; p++; } return atoi(p); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static double cost(void *inst, int n) { int16_t out[256]; double t0 = now(); for (int b = 0; b < n; b++) { fill(8000, 1, 0); api->render_block(inst, out, 128); } return (now() - t0) / n; }

int main(void) {
    system("rm -rf /tmp/micsmp; mkdir -p /tmp/micsmp"); setenv("TAKTILE_SMP_DIR", "/tmp/micsmp", 1);
    host_api_v1_t host; memset(&host, 0, sizeof host);
    host.sample_rate = 44100; host.get_beat_position = fake_beat; host.get_bpm = fake_bpm; host.mapped_memory = mail; host.audio_in_offset = MOVE_AUDIO_IN_OFFSET;
    api = move_plugin_init_v2(&host);
    void *inst = api->create_instance(".", "{}");

    blocks(inst, 20, 8000, 1, 0);
    OK(geti(inst, "micpeak", 0) == 0, "mic off: the level stays 0 (the input is not even read)");
    sp(inst, "micmon", "1"); blocks(inst, 30, 8000, 1, 0);
    int lv = geti(inst, "micpeak", 0); OK(lv >= 22 && lv <= 26 && geti(inst, "micpeak", 2) == 1, "level meter shows %d%% (expected about 24), input found", lv);
    sp(inst, "micgain", "20"); blocks(inst, 30, 8000, 1, 0);
    lv = geti(inst, "micpeak", 0); OK(lv >= 46 && lv <= 52, "gain x2 doubles the level (%d%%)", lv);
    sp(inst, "micgain", "80"); blocks(inst, 30, 8000, 1, 0); OK(geti(inst, "micpeak", 1) == 1, "gain x8 on a loud input reports clipping");

    /* a take of 1 s: mono mic on the left side only */
    sp(inst, "micmon", "0"); sp(inst, "micarm", "1,0,10,-2,0,50,0");
    OK(geti(inst, "recstate", 0) == 2, "micarm 1 s: recording at once");
    blocks(inst, 100, 8000, 1, 0); int pct = geti(inst, "recstate", 1); OK(pct > 20 && pct < 40, "progress %d%% after 100 blocks", pct);
    blocks(inst, 300, 8000, 1, 0); OK(geti(inst, "recstate", 0) == 3, "take is complete after 1 s");
    sp(inst, "recsave", "1");
    FILE *f = fopen("/tmp/micsmp/mic_001.wav", "rb"); OK(f != NULL, "WAV mic_001.wav written");
    if (f) {
        uint8_t h[44]; fread(h, 1, 44, f); uint32_t dl; memcpy(&dl, h + 40, 4);
        OK(!memcmp(h, "RIFF", 4) && dl == 44100 * 4, "WAV header ok, %u data bytes", dl);
        int16_t *d = malloc(dl); fread(d, 1, dl, f); fclose(f);
        int pk = 0, diff = 0; for (unsigned i = 0; i < dl / 2; i += 2) { int a = abs(d[i]); if (a > pk) pk = a; if (d[i] != d[i + 1]) diff++; }
        OK(pk > 7500 && pk < 8500, "peak of the file %d (expected about 8000)", pk);
        OK(diff == 0, "mono mic: both sides of the file are the same");
        OK(d[0] == 0 && abs(d[2]) < 200, "fade-in at the start");
        free(d);
    }
    OK(geti(inst, "recstate", 0) == 0, "state is idle again after saving");

    /* wait for sound */
    sp(inst, "micarm", "1,10,10,-2,0,50,0"); OK(geti(inst, "recstate", 0) == 5, "wait mode: state 5");
    blocks(inst, 60, 0, 1, 0); OK(geti(inst, "recstate", 0) == 5 && geti(inst, "recstate", 1) == 0, "silence: still waiting");
    blocks(inst, 5, 8000, 1, 0); OK(geti(inst, "recstate", 0) == 2, "sound starts the take");
    blocks(inst, 50, 8000, 1, 0); sp(inst, "micstop", "1"); OK(geti(inst, "recstate", 0) == 3, "early stop keeps what was recorded");
    sp(inst, "recsave", "1"); f = fopen("/tmp/micsmp/mic_002.wav", "rb"); OK(f != NULL, "second take mic_002.wav");
    if (f) { fseek(f, 0, SEEK_END); long sz = ftell(f); fclose(f); OK(sz > 44 + 50 * 128 * 4 && sz < 44 + 70 * 128 * 4, "length of the short take %ld bytes", sz); }
    sp(inst, "micarm", "5,10,10,-2,0,50,0"); sp(inst, "micstop", "1"); OK(geti(inst, "recstate", 0) == 0, "stop while waiting cancels the take");


    /* effects inside the take + normalise + other sources */
    {
        static int16_t d[44100 * 2 * 2]; long n;
        #define TAKE(arm, name, ch) do { sp(inst, "micarm", arm); blocks(inst, 400, 8000, 1, 0); sp(inst, "recsave", "1"); FILE *ff = fopen("/tmp/micsmp/" name, "rb"); if (ff) { fseek(ff, 44, SEEK_SET); n = (long)fread(d, 2, 44100 * 2, ff) / 2; fclose(ff); } else n = 0; } while (0)
        TAKE("1,0,10,-2,0,50,1", "mic_003.wav", 0); int pk = 0; for (long i = 0; i < n * 2; i++) if (abs(d[i]) > pk) pk = abs(d[i]);
        OK(n > 40000 && pk > 28500 && pk < 29500, "normalise brings the take to about -1 dB (peak %d)", pk);
        TAKE("1,0,10,-2,2,100,0", "mic_004.wav", 0); pk = 0; for (long i = 0; i < n * 2; i++) if (abs(d[i]) > pk) pk = abs(d[i]);
        OK(n > 40000 && pk > 12000, "DRIVE makes the take louder / harder (peak %d, input 8000)", pk);
        TAKE("1,0,10,-2,3,100,0", "mic_005.wav", 0); int held = 0; for (long i = 1000; i < 3000; i++) if (d[2 * i] == d[2 * i - 2]) held++;
        OK(held > 1500, "LO-FI holds samples (%d of 2000 equal to the previous one)", held);
        TAKE("1,0,10,-2,0,0,0", "mic_006.wav", 0); int a0 = 0; for (long i = 5000; i < 9000; i += 2) a0 += abs(d[2 * i]); /* reference */
        TAKE("1,0,10,-2,1,100,0", "mic_007.wav", 0); int a1 = 0; for (long i = 5000; i < 9000; i += 2) a1 += abs(d[2 * i]);
        OK(a1 > a0 * 8 / 10, "LOW CUT leaves a 1 kHz tone alone (%d vs %d)", a1, a0);
        /* low cut on a constant (DC) input: it must die away */
        { int16_t *in = (int16_t *)(mail + MOVE_AUDIO_IN_OFFSET); sp(inst, "micarm", "1,0,10,-2,1,0,0");
          for (int b = 0; b < 400; b++) { for (int i = 0; i < 256; i++) in[i] = 8000; int16_t out[256]; api->render_block(inst, out, 128); }
          sp(inst, "recsave", "1"); FILE *ff = fopen("/tmp/micsmp/mic_008.wav", "rb"); long m = 0; if (ff) { fseek(ff, 44, SEEK_SET); m = (long)fread(d, 2, 44100 * 2, ff) / 2; fclose(ff); }
          OK(m > 40000 && abs(d[2 * 30000]) < 800 && abs(d[2 * 200]) > 1000, "LOW CUT removes a constant offset (start %d, later %d)", abs(d[2 * 200]), abs(d[2 * 30000])); }
        /* DELAY and REVERB: the take gets a tail (echoes / room ring on after the input has stopped) */
        { static int16_t e[44100 * 5 * 2]; const char *nm[2] = { "mic_009.wav", "mic_010.wav" }; const char *ar[2] = { "1,0,10,-2,4,60,0", "1,0,10,-2,5,60,0" };
          for (int k = 0; k < 2; k++) {
              sp(inst, "micarm", ar[k]); blocks(inst, 1300, 8000, 1, 0); OK(geti(inst, "recstate", 0) == 3, "%s take completes (with tail)", k ? "REVERB" : "DELAY");
              sp(inst, "recsave", "1"); FILE *ff = fopen(nm[k] [0] ? (k ? "/tmp/micsmp/mic_010.wav" : "/tmp/micsmp/mic_009.wav") : "", "rb"); long m = 0; if (ff) { fseek(ff, 44, SEEK_SET); m = (long)fread(e, 2, 44100 * 5 * 2, ff) / 2; fclose(ff); }
              long a1 = 0, a2 = 0, a3 = 0; int bad = 0; for (long i = 46000; i < 60000 && i < m; i++) a1 += abs(e[2 * i]); for (long i = 80000; i < 90000 && i < m; i++) a2 += abs(e[2 * i]); for (long i = 0; i < m; i++) if (abs(e[2 * i]) >= 32767) bad++;
              for (long i = m - 400; i < m; i++) if (i >= 0) a3 += abs(e[2 * i]);
              OK(m > 44100 * 2 + 20000, "%s take is longer than the 1 s of input (%ld frames)", k ? "REVERB" : "DELAY", m);
              OK(a1 > 14000L * 300, "%s rings on after the input stopped (energy %ld)", k ? "REVERB" : "DELAY", a1);
              OK(bad < 50, "%s does not clip (%d hard-clipped samples)", k ? "REVERB" : "DELAY", bad);
              (void)a2; (void)a3; } }
        sp(inst, "micarm", "1,0,10,-1,0,50,1"); OK(geti(inst, "recstate", 0) == 2, "source MASTER starts at once");
        blocks(inst, 400, 0, 1, 0); OK(geti(inst, "recstate", 0) == 3, "a master take of 1 s completes"); sp(inst, "recsave", "1");
        FILE *ff = fopen("/tmp/micsmp/resample_001.wav", "rb"); OK(ff != NULL, "master take saved as resample_001.wav"); if (ff) fclose(ff);
        sp(inst, "micarm", "1,0,10,3,0,50,1"); blocks(inst, 400, 0, 1, 0); OK(geti(inst, "recstate", 0) == 3, "a track take (track 4) completes"); sp(inst, "recsave", "1");
    }

    /* CPU: cost of one block with the mic code idle, level meter on, recording */
    sp(inst, "micmon", "0"); cost(inst, 2000); double c0 = cost(inst, 20000);
    sp(inst, "micmon", "1"); double c1 = cost(inst, 20000);
    sp(inst, "micmon", "0"); sp(inst, "micarm", "30,0,10,-2,0,50,0"); double c2 = cost(inst, 20000); sp(inst, "micstop", "1");
    double worst = 0, cf[5]; for (int f = 1; f <= 5; f++) { char a[64]; snprintf(a, sizeof a, "30,0,10,-2,%d,60,0", f); sp(inst, "micarm", a); cf[f - 1] = cost(inst, 20000); sp(inst, "micstop", "1"); sp(inst, "reccancel", "1"); }
    printf("cpu  recording with effect: low cut +%.2f us, drive +%.2f us, lo-fi +%.2f us, delay +%.2f us, reverb +%.2f us per block\n", (cf[0] - c0) * 1e6, (cf[1] - c0) * 1e6, (cf[2] - c0) * 1e6, (cf[3] - c0) * 1e6, (cf[4] - c0) * 1e6);
    for (int f = 0; f < 5; f++) if (cf[f] - c0 > worst) worst = cf[f] - c0;
    double bud = 128.0 / 44100.0;
    printf("cpu  per block (budget %.0f us): mic idle %.1f us, meter %.1f us (+%.2f us), recording %.1f us (+%.2f us)\n", bud * 1e6, c0 * 1e6, c1 * 1e6, (c1 - c0) * 1e6, c2 * 1e6, (c2 - c0) * 1e6);
    if (c2 - c0 > worst) worst = c2 - c0;
    OK(worst < bud * 0.02, "recording (with effect) adds less than 2%% of one block's time budget (%.3f%%)", worst / bud * 100);

    /* no mailbox: no crash */
    host.mapped_memory = NULL; api->destroy_instance(inst); api = move_plugin_init_v2(&host); inst = api->create_instance(".", "{}");
    sp(inst, "micmon", "1"); blocks(inst, 10, 8000, 1, 0); OK(geti(inst, "micpeak", 2) == 0, "no input available: reported, no crash");
    printf(failed ? "%d FAILED\n" : "all ok\n", failed); return failed != 0;
}
