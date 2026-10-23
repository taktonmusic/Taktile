/* ---------------- THREE MORE ENGINES: ACID 303, SWARM, DX 6-OP FM ----------------
   Written for Taktile in plain C after three open-source designs (all permissive licences, see NOTICE.md):
   - ACID 303: the TB-303 filter model (diode ladder, 18 dB-like, feedback high-pass) from Open303 by Robin Schmidt (MIT)
   - SWARM: the swarm of detuned saws with random grain envelopes from Mutable Instruments Plaits by Emilie Gillet (MIT)
   - DX: the 32 algorithms of the 6-operator FM core from music-synthesizer-for-android / msfa by Google (Apache 2.0)
   This file is included into pad_studio.c right after nx_t is known and before nx_init. */

/* ===== 303 ===== */
/* nx: 0 saw phase, 1 sub phase, 2..5 filter stages y1..y4, 6 feedback high-pass state, 7 smoothed frequency, 8 accent (0 / 1), 9 dc-block state, 10 control counter,
       11 cutoff Hz (base, control rate), 12 res, 13 envmod octaves, 14 drive, 15 b0, 16 k, 17 g, 18 last input (2x oversampling) */
static void ac3_init(inst_t *s, vc_t *v, int p, const nx_t *n) {
    (void)n;
    for (int k = 0; k < 24; k++) v->nx[k] = 0.0f;
    v->nx[7] = v->f0;
    v->nx[8] = v->vel >= 0.9f ? 1.0f : 0.0f;
    { const float tt = TY[v->type].tau2 * s->dm[p] * exp2f((0.5f - s->pp[p][PP_FDEC]) * 3.0f) * (v->nx[8] > 0.5f ? 0.4f : 1.0f);      /* Decay knob = length of the filter envelope; accent: shorter */
      v->c2 = tt > 0.0f ? expf(-1.0f / (tt * s->sr)) : 0.0f; }
}
static inline void ac3_coef(float cut, float res, float sr2, float *b0, float *k, float *g) {
    const float wc = 6.2831853f * cut / sr2, fx = wc * 0.70710678f / 6.2831853f;
    const float r = (1.0f - expf(-3.0f * res)) / (1.0f - expf(-3.0f));
    *b0 = (0.00045522346f + 6.1922189f * fx) / (1.0f + 12.358354f * fx + 4.4156345f * (fx * fx));
    float kk = fx * (fx * (fx * (fx * (fx * (fx + 7198.6997f) - 5837.7917f) - 476.47308f) + 614.95611f) + 213.87126f) + 16.998792f;
    float gg = kk * 0.058823529f; gg = (gg - 1.0f) * r + 1.0f; gg = gg * (1.0f + r);
    *k = kk * r; *g = gg;
}
static float ac3_render(inst_t *s, vc_t *v, int p, const nx_t *n) {
    const float *q = n->p, *pq = s->pp[p]; const ty_t *T = &TY[v->type]; const float dt = s->inv_sr;
    const float acc = v->nx[8];
    /* control rate: cutoff, resonance, envelope depth */
    if (v->nx[10] <= 0.0f) {
        const float cutn = clampf(q[1] + pq[PP_DET] * 0.6f, 0.0f, 1.0f);
        v->nx[11] = 110.0f * exp2f(cutn * 6.0f);                                        /* 110 Hz .. 7 kHz */
        v->nx[12] = clampf(q[2] + pq[PP_SUB] * 0.5f + acc * 0.1f, 0.0f, 1.0f);
        v->nx[13] = (clampf(q[3] + pq[PP_FKEY], 0.0f, 1.0f) * 4.5f) * (1.0f + acc * 0.5f * q[5]);
        v->nx[14] = q[6] * 4.0f;
        v->nx[10] = 16.0f;
    }
    v->nx[10] -= 1.0f;
    { const float sl = q[4] > 0.5f ? 1.0f - expf(-dt * 1000.0f / (q[4] * 0.2f)) : 1.0f;  v->nx[7] += (v->f0 - v->nx[7]) * sl; }     /* slide between legato notes */
    const float f = v->nx[7], d = f * dt;
    /* oscillator: saw bent like the 303's, blended with a square */
    v->nx[0] += d; if (v->nx[0] >= 1.0f) v->nx[0] -= 1.0f;
    float saw = 2.0f * v->nx[0] - 1.0f - blep(v->nx[0], d); saw = saw + 0.22f * saw * fabsf(saw);
    const float pw = 0.5f; float sqr = (v->nx[0] < pw ? 1.0f : -1.0f) + blep(v->nx[0], d) - blep(fmodf(v->nx[0] + 1.0f - pw, 1.0f), d);
    float o = saw + (sqr - saw) * q[0];
    if (q[7] > 0.0f) { v->nx[1] += 0.5f * d; if (v->nx[1] >= 1.0f) v->nx[1] -= 1.0f; o += q[7] * (v->nx[1] < 0.5f ? 0.5f : -0.5f); }
    o *= 0.6f;
    /* filter envelope (v->env2 falls with tau2 x Decay knob) sets the cutoff */
    float cut = v->nx[11] * exp2f(v->nx[13] * v->env2);
    cut = clampf(cut, 80.0f, 11000.0f);
    float b0, k, g; ac3_coef(cut, v->nx[12], s->sr * 2.0f, &b0, &k, &g);
    /* 2x oversampled diode ladder */
    float y = 0.0f; const float x0 = v->nx[18]; v->nx[18] = o;
    for (int os = 0; os < 2; os++) {
        const float in = os == 0 ? 0.5f * (x0 + o) : o;
        const float xs = clampf(v->nx[5], -1.4142f, 1.4142f), fb = k * (xs - xs * xs * xs * 0.16666667f);
        const float hp = fb - v->nx[6]; v->nx[6] += hp * 0.0213f;            /* one-pole high-pass in the feedback path, about 150 Hz */
        const float y0 = in - hp;
        v->nx[2] += 2.0f * b0 * (y0 - v->nx[2] + v->nx[3]);
        v->nx[3] += b0 * (v->nx[2] - 2.0f * v->nx[3] + v->nx[4]);
        v->nx[4] += b0 * (v->nx[3] - 2.0f * v->nx[4] + v->nx[5]);
        v->nx[5] += b0 * (v->nx[4] - 2.0f * v->nx[5]);
        y += 2.0f * g * v->nx[5];
    }
    y *= 0.5f;
    y = 1.25f * fast_tanh(0.8f * y);                              /* full resonance rings up: a soft limit keeps it musical */
    if (v->nx[14] > 0.0f) y = fast_tanh(y * (1.0f + v->nx[14])) * (1.0f / (1.0f + v->nx[14] * 0.35f));
    { const float at = att_time(p, pq[PP_ATT], T->att, 0.0008f, 2.0f);
      if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
      else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
    v->env2 *= v->c2;
    return fast_tanh(y * v->env * (1.0f + acc * 0.35f) * 0.9f);
}

/* ===== SWARM ===== */
/* nx: 8 saw voices x 6 floats (0 phase, 1 from, 2 interval, 3 grain phase, 4 grain rate multiplier, 5 amplitude), 48 control counter, 49.. per voice frequency ratio (8), 57.. per voice gain (8)
   (nx has 96 floats) */
#define SW_N 8
static inline float sw_rnd(inst_t *s) { return 0.5f + 0.5f * noise(s); }
static void swm_init(inst_t *s, vc_t *v, int p, const nx_t *n) {
    (void)p; (void)n;
    for (int k = 0; k < 96; k++) v->nx[k] = 0.0f;
    for (int i = 0; i < SW_N; i++) {
        float *e = &v->nx[i * 6];
        e[0] = sw_rnd(s); e[1] = 0.0f; e[2] = 1.0f; e[3] = 1.0f; e[4] = 0.5f + 1.5f * sw_rnd(s); e[5] = 0.5f;
        v->nx[49 + i] = 1.0f; v->nx[57 + i] = 1.0f / SW_N;
    }
}
static float swm_render(inst_t *s, vc_t *v, int p, const nx_t *n) {
    const float *q = n->p, *pq = s->pp[p]; const ty_t *T = &TY[v->type]; const float dt = s->inv_sr;
    if (v->nx[48] <= 0.0f) {                                       /* control rate (every 16 samples), like Plaits' block rate */
        const float spread0 = clampf(q[0] + pq[PP_DET], 0.0f, 1.0f), spread = spread0 * spread0 * spread0;
        const float dens = clampf(q[1] + pq[PP_SUB] - 0.0f, 0.0f, 1.0f);
        const float morph = clampf(q[2] + pq[PP_FKEY], 0.0f, 1.0f);
        const float density = 8.1758f * exp2f(dens * 10.0f) * 0.025f * 16.0f * s->inv_sr;
        float size_ratio = 0.25f * exp2f((1.0f - morph) * 84.0f / 12.0f);
        for (int i = 0; i < SW_N; i++) {
            float *e = &v->nx[i * 6]; const float rank = ((float)i - 3.0f) / 3.0f;
            e[3] += density * e[4];                                  /* grain envelope: step */
            if (e[3] >= 1.0f) { e[3] -= floorf(e[3]); e[1] += e[2]; e[2] = sw_rnd(s) - e[1]; e[4] = 0.5f + 1.5f * sw_rnd(s); }
            float expo = size_ratio < 1.0f ? 2.0f * (e[1] + e[2] * e[3]) - 1.0f : e[1];
            float tgt = 1.0f;
            if (size_ratio >= 1.0f) { float ph = (e[3] - 0.5f) * size_ratio; ph = clampf(ph, -1.0f, 1.0f); tgt = 0.5f * (cosf(1.5707963f * ph) + 1.0f); }
            e[5] += (tgt - e[5]) * 0.3f;
            float ratio = exp2f(48.0f * expo * spread * rank / 12.0f) * (1.0f + rank * (rank + 0.01f) * spread * 0.25f);
            v->nx[49 + i] = ratio; v->nx[57 + i] = e[5] * (1.0f / SW_N);
            size_ratio *= 0.97f;
        }
        v->nx[48] = 16.0f;
    }
    v->nx[48] -= 1.0f;
    const float mix = clampf(q[3] + (pq[PP_FDEC] - 0.5f) * 1.2f, 0.0f, 1.0f);       /* 0 = all saw, 1 = all sine */
    float o = 0.0f;
    for (int i = 0; i < SW_N; i++) {
        float *e = &v->nx[i * 6];
        float fr = v->f0 * v->nx[49 + i]; if (fr > 0.45f * s->sr) fr = 0.45f * s->sr;
        const float d = fr * dt;
        e[0] += d; if (e[0] >= 1.0f) e[0] -= 1.0f;
        const float saw = 2.0f * e[0] - 1.0f - blep(e[0], d);
        o += v->nx[57 + i] * (saw * (1.0f - mix) + fsin01(e[0]) * mix);
    }
    { const float at = att_time(p, pq[PP_ATT], T->att, 0.0008f, 2.0f);
      if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
      else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
    v->env2 *= v->c2;
    return o * v->env * 2.2f;
}

/* ===== DX: six operators, the 32 algorithms of the DX7 ===== */
/* flags per operator (msfa order: entry 0 is operator 6): bit0-1 output bus (0 = out), bit2 add to bus, bit4-5 input bus, bit6+7 = feedback operator */
static const uint8_t DX_ALG[32][6] = {
    { 0xc1, 0x11, 0x11, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0x11, 0x14, 0xc1, 0x14 }, { 0xc1, 0x11, 0x14, 0x01, 0x11, 0x14 }, { 0x41, 0x11, 0x94, 0x01, 0x11, 0x14 },
    { 0xc1, 0x14, 0x01, 0x14, 0x01, 0x14 }, { 0x41, 0x94, 0x01, 0x14, 0x01, 0x14 }, { 0xc1, 0x11, 0x05, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0xc5, 0x14, 0x01, 0x14 },
    { 0x01, 0x11, 0x05, 0x14, 0xc1, 0x14 }, { 0x01, 0x05, 0x14, 0xc1, 0x11, 0x14 }, { 0xc1, 0x05, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x05, 0x14, 0xc1, 0x14 },
    { 0xc1, 0x05, 0x05, 0x14, 0x01, 0x14 }, { 0xc1, 0x05, 0x11, 0x14, 0x01, 0x14 }, { 0x01, 0x05, 0x11, 0x14, 0xc1, 0x14 }, { 0xc1, 0x11, 0x02, 0x25, 0x05, 0x14 },
    { 0x01, 0x11, 0x02, 0x25, 0xc5, 0x14 }, { 0x01, 0x11, 0x11, 0xc5, 0x05, 0x14 }, { 0xc1, 0x14, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x14 },
    { 0x01, 0x14, 0x14, 0xc1, 0x14, 0x14 }, { 0xc1, 0x14, 0x14, 0x14, 0x01, 0x14 }, { 0xc1, 0x14, 0x14, 0x01, 0x14, 0x04 }, { 0xc1, 0x14, 0x14, 0x14, 0x04, 0x04 },
    { 0xc1, 0x14, 0x14, 0x04, 0x04, 0x04 }, { 0xc1, 0x05, 0x14, 0x01, 0x14, 0x04 }, { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x04 }, { 0x04, 0xc1, 0x11, 0x14, 0x01, 0x14 },
    { 0xc1, 0x14, 0x01, 0x14, 0x04, 0x04 }, { 0x04, 0xc1, 0x11, 0x14, 0x04, 0x04 }, { 0xc1, 0x14, 0x04, 0x04, 0x04, 0x04 }, { 0xc4, 0x04, 0x04, 0x04, 0x04, 0x04 } };
/* one operator in a patch: frequency ratio, output level 0..99 (0.75 dB per step), attack s, decay s (toward sustain), sustain (fraction of the level) */
typedef struct { float r, lv, a, d, su; } dxo_t;
/* operators are listed as on the DX7: operator 1 first */
typedef struct { uint8_t algo; float fb; float rel; dxo_t op[6]; } dxp_t;
#define DXN 14
static const dxp_t DXP[DXN] = {
 /* 0 E.PIANO   */ { 5, 5, .5f,  { {1.0f,99,.002f,3.2f,.0f}, {1.0f,72,.002f,.9f,.12f}, {1.004f,92,.002f,3.0f,.0f}, {14.0f,64,.001f,.10f,.0f}, {1.0f,90,.002f,3.4f,.0f}, {1.0f,50,.002f,.6f,.1f} } },
 /* 1 E.PIANO 2 */ { 5, 4, .5f,  { {1.0f,99,.002f,2.4f,.0f}, {2.0f,60,.002f,.5f,.05f}, {1.003f,95,.002f,2.2f,.0f}, {7.0f,55,.001f,.08f,.0f}, {0.5f,85,.002f,3.0f,.0f}, {1.0f,40,.002f,.5f,.1f} } },
 /* 2 FM BASS   */ { 1, 6, .12f, { {1.0f,99,.001f,1.5f,.80f}, {1.0f,82,.001f,.22f,.32f}, {0.5f,88,.001f,1.2f,.85f}, {1.0f,76,.001f,.18f,.20f}, {2.0f,60,.001f,.15f,.0f}, {1.0f,72,.001f,.3f,.3f} } },
 /* 3 SLAP BASS */ { 18, 6, .1f, { {1.0f,99,.001f,.9f,.55f}, {3.0f,72,.001f,.12f,.0f}, {1.0f,88,.001f,.8f,.5f}, {1.0f,80,.001f,.20f,.15f}, {2.0f,55,.001f,.1f,.0f}, {0.5f,70,.001f,.3f,.2f} } },
 /* 4 BRASS     */ { 22, 5, .18f, { {1.0f,99,.04f,.5f,.85f}, {1.0f,70,.06f,.6f,.5f}, {1.003f,92,.03f,.5f,.88f}, {0.998f,90,.03f,.5f,.88f}, {1.0f,85,.03f,.5f,.88f}, {1.0f,76,.05f,.7f,.55f} } },
 /* 5 BELL      */ { 5, 3, 1.8f, { {1.0f,99,.001f,5.0f,.0f}, {3.5f,78,.001f,2.2f,.0f}, {2.0f,90,.001f,4.0f,.0f}, {5.19f,70,.001f,1.5f,.0f}, {3.0f,80,.001f,3.0f,.0f}, {7.23f,60,.001f,1.0f,.0f} } },
 /* 6 TUBULAR   */ { 5, 2, 2.5f, { {1.0f,99,.001f,6.0f,.0f}, {3.5f,70,.001f,2.5f,.0f}, {4.0f,76,.001f,4.0f,.0f}, {2.76f,60,.001f,1.8f,.0f}, {2.0f,60,.001f,3.0f,.0f}, {5.4f,50,.001f,1.2f,.0f} } },
 /* 7 MARIMBA   */ { 5, 0, .25f, { {1.0f,99,.001f,.55f,.0f}, {4.0f,72,.001f,.09f,.0f}, {1.0f,88,.001f,.35f,.0f}, {10.0f,60,.001f,.03f,.0f}, {0.5f,70,.001f,.7f,.0f}, {3.0f,40,.001f,.05f,.0f} } },
 /* 8 CLAV      */ { 4, 5, .08f, { {1.0f,99,.001f,.35f,.0f}, {1.0f,80,.001f,.12f,.0f}, {1.0f,90,.001f,.30f,.0f}, {3.0f,84,.001f,.07f,.0f}, {1.0f,50,.001f,.2f,.0f}, {7.0f,60,.001f,.04f,.0f} } },
 /* 9 ORGAN     */ { 32, 2, .08f, { {0.5f,92,.004f,.1f,1.0f}, {1.0f,99,.004f,.1f,1.0f}, {2.0f,90,.004f,.1f,1.0f}, {3.0f,76,.004f,.1f,1.0f}, {4.0f,70,.004f,.1f,1.0f}, {6.0f,60,.004f,.1f,1.0f} } },
 /* 10 FAT PAD  */ { 14, 4, .8f, { {1.0f,99,.5f,.5f,1.0f}, {1.003f,92,.6f,.5f,1.0f}, {0.997f,90,.5f,.5f,1.0f}, {2.0f,66,.8f,1.0f,.6f}, {1.0f,64,.7f,1.0f,.7f}, {1.0f,58,.9f,1.0f,.6f} } },
 /* 11 SYN LEAD */ { 16, 7, .12f, { {1.0f,99,.005f,.3f,1.0f}, {2.0f,66,.005f,.5f,.7f}, {1.004f,92,.005f,.3f,1.0f}, {1.0f,72,.005f,.5f,.7f}, {0.5f,70,.005f,.3f,.9f}, {1.0f,76,.005f,.4f,.8f} } },
 /* 12 GLASS    */ { 5, 3, 1.2f, { {1.0f,99,.002f,3.0f,.2f}, {5.0f,62,.002f,1.5f,.0f}, {2.0f,88,.002f,2.5f,.1f}, {7.0f,55,.002f,1.0f,.0f}, {4.0f,70,.002f,2.0f,.1f}, {9.0f,45,.002f,.8f,.0f} } },
 /* 13 REESE FM */ { 1, 7, .15f, { {1.0f,99,.002f,1.0f,.9f}, {1.01f,80,.002f,.6f,.7f}, {0.5f,92,.002f,1.0f,.9f}, {0.495f,78,.002f,.5f,.6f}, {1.0f,64,.002f,.4f,.5f}, {2.0f,60,.002f,.3f,.4f} } },
};
/* nx: 0..5 phases (msfa order), 6..11 envelope level, 12..17 envelope stage (0 attack, 1 decay), 18,19 feedback history, 20..25 ratio, 26..31 amplitude, 32..37 decay coefficient,
   38..43 sustain level, 44..49 attack increment, 50 patch, 51 gain */
static void dx_init(inst_t *s, vc_t *v, int p, const nx_t *n) {
    const float *q = n->p, *pq = s->pp[p];
    int pi = (int)(q[0] + 0.5f); if (pi < 0) pi = 0; if (pi >= DXN) pi = DXN - 1;
    const dxp_t *P = &DXP[pi]; const int al = P->algo - 1;
    const float idx = exp2f((q[1] - 1.0f) * 1.5f + pq[PP_DET] * 2.0f);                         /* Index knob: modulator depth */
    const float dsc = exp2f((0.5f - pq[PP_FDEC]) * 3.0f) * (q[3] > 0.0f ? q[3] : 1.0f);           /* Decay knob: all envelopes longer / shorter */
    const float cents = q[4] + pq[PP_SUB] * 25.0f;                                                   /* Detune knob: carriers beat against each other */
    const float vl = 0.55f + 0.45f * v->vel;
    for (int k = 0; k < 64; k++) v->nx[k] = 0.0f;
    float ncar = 0.0f;
    for (int j = 0; j < 6; j++) {
        const dxo_t *o = &P->op[5 - j]; const uint8_t fl = DX_ALG[al][j];
        const int carrier = (fl & 3) == 0;
        float lv = o->lv; if (!carrier) lv = lv + 8.0f * (v->vel - 0.7f);
        float amp = powf(10.0f, -(99.0f - lv) * 0.75f / 20.0f);
        if (!carrier) amp *= idx * (0.5f + 0.5f * vl) * 1.4f; else { amp *= vl; ncar += 1.0f; }
        const float det = carrier ? (((j & 1) ? 1.0f : -1.0f) * cents) : 0.0f;
        v->nx[20 + j] = o->r * exp2f(det / 1200.0f);
        v->nx[26 + j] = amp;
        v->nx[32 + j] = expf(-1.0f / (o->d * dsc * s->sr));
        v->nx[38 + j] = o->su;
        v->nx[44 + j] = 1.0f / (o->a * s->sr + 1.0f);
        v->nx[6 + j] = 0.0f; v->nx[12 + j] = 0.0f;
        v->nx[j] = 0.0f;
    }
    v->nx[50] = (float)pi; v->nx[51] = 1.0f / sqrtf(ncar > 1.0f ? ncar : 1.0f);
    v->nx[52] = (P->fb + q[2] + pq[PP_FKEY] * 3.0f) > 0.0f ? exp2f(clampf(P->fb + q[2] + pq[PP_FKEY] * 3.0f, 0.0f, 7.0f) - 8.0f) : 0.0f;
    v->nx[53] = expf(-1.0f / (P->rel * s->dm[p] * s->sr));
}
static float dx_render(inst_t *s, vc_t *v, int p, const nx_t *n) {
    (void)p; (void)n; const float dt = s->inv_sr; const ty_t *T = &TY[v->type];
    const dxp_t *P = &DXP[(int)v->nx[50]]; const uint8_t *alg = DX_ALG[P->algo - 1];
    float bus[3] = { 0.0f, 0.0f, 0.0f }, out = 0.0f;
    for (int j = 0; j < 6; j++) {
        const uint8_t fl = alg[j]; const int ob = fl & 3, ib = (fl >> 4) & 3;
        float e = v->nx[6 + j];
        if (v->gate) {
            if (v->nx[12 + j] < 0.5f) { e += v->nx[44 + j]; if (e >= 1.0f) { e = 1.0f; v->nx[12 + j] = 1.0f; } }
            else { const float su = v->nx[38 + j]; e = su + (e - su) * v->nx[32 + j]; }
        }
        v->nx[6 + j] = e;
        v->nx[j] += v->f0 * v->nx[20 + j] * dt; v->nx[j] -= floorf(v->nx[j]);
        float md = ib ? bus[ib] : 0.0f;
        if ((fl & 0xc0) == 0xc0) md += 0.5f * (v->nx[18] + v->nx[19]) * v->nx[52];
        const float o = fsin01(v->nx[j] + md - floorf(v->nx[j] + md)) * v->nx[26 + j] * e;
        if (fl & 0x80) { v->nx[19] = v->nx[18]; v->nx[18] = o; }
        if (ob == 0) out += o; else if (fl & 4) bus[ob] += o; else bus[ob] = o;
    }
    { const float at = att_time(p, s->pp[p][PP_ATT], T->att, 0.0008f, 2.0f);
      if (v->gate) v->env += (1.0f - v->env) * (dt / at);
      else { v->env *= v->nx[53]; if (v->env < 1e-3f) v->active = 0; } }
    return out * v->nx[51] * v->env * 0.9f;
}
