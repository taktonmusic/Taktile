#ifndef SPK_BW
#define SPK_BW 1.0f
#endif
#ifndef SPK_TAU
#define SPK_TAU 0.010f
#endif
#ifndef SPK_DUR
#define SPK_DUR 0.78f
#endif
#ifndef SPK_ACC
#define SPK_ACC 0.10f
#endif
#ifndef SPK_DEC
#define SPK_DEC 0.07f
#endif
/* ---------------- SPEECH engine, part 2: the voice (a Klatt-style formant synthesizer) ----------------
   A glottal pulse train (the note's pitch) and noise go through three formant resonators; fricatives and bursts have their own noise path.
   Every note says the whole text once (one shot). Knobs: Speed, Formant, Breath, Inflect.  p: language (0 EN, 1 DE), formant scale, speed scale */
static void spk_res(float f, float bw, float sr, float *A, float *B, float *C) {
    if (f < 60.0f) f = 60.0f; if (f > 0.46f * sr) f = 0.46f * sr; if (bw < 30.0f) bw = 30.0f;
    const float r = expf(-3.14159265f * bw / sr);
    *C = -r * r; *B = 2.0f * r * cosf(6.2831853f * f / sr); *A = 1.0f - *B - *C;
}
static void spk_start(inst_t *s, vc_t *v, int p, const nx_t *n) {
    const int lang = n->p[0] > 0.5f;
    const char *t = s->spk_txt[p][0] ? s->spk_txt[p] : (lang ? "HALLO WELT" : "HELLO WORLD");
    char one[48];
    if ((int)(s->pp[p][PP_DMD] + 0.5f) == 1) {                      /* PLAY = WORD: every note says the next word of the text */
        int nw = 0, in = 0; for (const char *c = t; *c; c++) { const int al = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9'); if (al && !in) nw++; in = al; }
        if (nw > 0) {
            const int want = s->spk_wi[p] % nw; s->spk_wi[p]++;
            int wi = -1; in = 0; const char *st = t, *en = t;
            for (const char *c = t;; c++) {
                const int al = *c && ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9'));
                if (al && !in) { wi++; st = c; }
                if (!al && in && wi == want) { en = c; break; }
                in = al; if (!*c) { en = c; break; }
            }
            int k = 0; for (const char *c = st; c < en && k < 46; c++) one[k++] = *c; one[k] = 0; t = one;
        }
    }
    uint8_t *ph = (uint8_t *)v->ks;
    int cnt;
    if ((int)(s->pp[p][PP_DMD] + 0.5f) == 2) cnt = spk_letter((int)(s->pp[p][PP_DLT] * (float)(SPK_NLET - 1) + 0.5f), lang, ph);      /* PLAY = LETTER: the letter chosen for this step (a parameter lock) */
    else cnt = spk_compile(t, lang, ph);
    static int idCL = -1, idCLV = -1; if (idCL < 0) { idCL = spk_id("CL"); idCLV = spk_id("CLV"); }
    if (cnt > 1 && (ph[0] == idCL || ph[0] == idCLV)) { memmove(ph, ph + 1, (size_t)(cnt - 1)); cnt--; }   /* start on the burst, not on the silence: the sound lands on the beat */
    float D = 0.0f; for (int i = 0; i < cnt; i++) D += SPK_P[ph[i]].ms * 0.001f;
    v->ni[1] = cnt; v->ni[0] = 0;
    for (int k = 0; k < 64; k++) v->nx[k] = 0.0f;
    v->nx[2] = 500.0f; v->nx[3] = 1500.0f; v->nx[4] = 2500.0f; v->nx[7] = 3000.0f; v->nx[8] = 3000.0f; v->nx[23] = 1.0f;
    v->nx[22] = 0.0f;                                              /* the knob / coefficient refresh counter */
    v->nx[36] = 1.0f; v->nx[37] = 1.0f; v->nx[45] = D > 0.01f ? D : 0.01f; v->env = 1.0f;
}
static float spk_render(inst_t *s, vc_t *v, int p, const nx_t *n) {
    const float dt = s->inv_sr, sr = s->sr; float *x = v->nx;
    const uint8_t *ph = (const uint8_t *)v->ks; const float *pq = s->pp[p];
    const int idx = v->ni[0], cnt = v->ni[1];
    const spkp_t *P = idx < cnt ? &SPK_P[ph[idx]] : NULL;
    if (x[22] < 0.5f) {                                             /* every 32 samples: knobs, pitch contour, resonator coefficients */
        {   /* speed: 0.5x .. 2x on top of the tempo. LENGTH AUTO: follows the BPM (normal speed at 120). LENGTH 1/16 .. 2 BAR: the text is stretched or squeezed to fit that time */
            static const float BT[7] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
            int li = (int)(pq[PP_DLN] + 0.5f); if (li < 0) li = 0; if (li > 6) li = 6;
            const float kn = exp2f((pq[PP_DSW] - 0.5f) * 2.0f), bpm = s->bpm > 1.0f ? s->bpm : 120.0f;
            if (li == 0) x[36] = n->p[2] * kn * SPK_DUR * clampf(bpm / 120.0f, 0.4f, 3.0f);      /* 0.78: unhurried, so every sound can be heard */
            else x[36] = clampf(x[45] / (BT[li] * 60.0f / bpm), 0.35f, 4.0f) * kn;
        }
        x[37] = n->p[1] * 1.15f * exp2f((pq[PP_DST] - 0.5f));               /* formant (vocal tract) scale: 0.7x .. 1.4x */
        const float bb = pq[PP_DNZ] * 2.0f; x[39] = bb * bb * 0.05f;   /* breath: aspiration along with the voice */
        const float u = cnt > 0 ? ((float)idx + (P ? x[0] / (P->ms * 0.001f / x[36]) : 1.0f)) / (float)cnt : 1.0f;
        if (idx != (int)x[50]) { x[50] = (float)idx; if (idx == 0 || (idx > 0 && ph[idx - 1] == 0)) x[49] = 1.0f; }      /* every word starts a little higher and settles: natural stress instead of a monotone */
        x[49] *= 0.9952f;
        x[23] = exp2f((pq[PP_DCK] - 0.5f) * 2.0f * 5.0f * (2.0f * u - 1.0f) / 12.0f) * (1.0f + SPK_ACC * x[49]) * (1.0f - SPK_DEC * u);   /* inflect: the pitch falls (0) or rises (1) over the sentence, 50 = flat */
        const float b1 = (80.0f + 100.0f * x[10]) * SPK_BW;
        spk_res(x[2], b1, sr, &x[24], &x[25], &x[26]);
        spk_res(x[3], 110.0f * SPK_BW, sr, &x[27], &x[28], &x[29]);
        spk_res(x[4], 170.0f * SPK_BW, sr, &x[30], &x[31], &x[32]);
        spk_res(x[7], x[8], sr, &x[33], &x[34], &x[35]);
        spk_res(3500.0f * x[37], 250.0f, sr, &x[40], &x[41], &x[42]);      /* a fixed fourth formant: a natural brightness */
    }
    x[22] += 1.0f; if (x[22] >= 32.0f) x[22] = 0.0f;
    const float fsc = x[37];
    float tf1 = x[2], tf2 = x[3], tf3 = x[4], tav = 0.0f, taf = 0.0f, tfc = x[7], tfb = x[8], tah = 0.0f, tnas = 0.0f;
    if (P) {
        static int iCL = -1, iCLV = -1, iASP = -1; if (iCL < 0) { iCL = spk_id("CL"); iCLV = spk_id("CLV"); iASP = spk_id("ASP"); }
        const spkp_t *Pf = P;      /* the closure leans towards the burst that follows, the breath after the burst still carries its place: this is what lets you hear P from T from K */
        if ((ph[idx] == iCL || ph[idx] == iCLV) && idx + 1 < cnt) Pf = &SPK_P[ph[idx + 1]]; else if (ph[idx] == iASP && idx > 0) Pf = &SPK_P[ph[idx - 1]];
        tf1 = Pf->f1 * fsc; tf2 = Pf->f2 * fsc; tf3 = Pf->f3 * fsc; tav = P->av * (1.0f / 255.0f); taf = P->af * (1.0f / 255.0f);
        if (P->af) { tfc = P->fc * fsc; tfb = (float)P->fbw; }
        tah = P->ah * (1.0f / 255.0f); tnas = P->nas * (1.0f / 255.0f);
    }
    const float aF = dt / (SPK_TAU + dt), aA = dt / (0.005f + dt), aQ = dt / (0.002f + dt);
    x[2] += (tf1 - x[2]) * aF; x[3] += (tf2 - x[3]) * aF; x[4] += (tf3 - x[4]) * aF;
    x[5] += (tav - x[5]) * aA; x[6] += (taf - x[6]) * aA; x[9] += (tah - x[9]) * aA; x[10] += (tnas - x[10]) * aF;
    if (P && P->af) { x[7] += (tfc - x[7]) * aQ; x[8] += (tfb - x[8]) * aQ; }
    /* glottal pulse (Rosenberg), differentiated */
    float f0 = v->f0 * x[23]; if (f0 < 40.0f) f0 = 40.0f;
    x[11] += f0 * dt; if (x[11] >= 1.0f) x[11] -= 1.0f;
    const float u2 = x[11];
    const float g = u2 < 0.45f ? 0.5f * (1.0f - sin2pi_raw(u2 * (1.0f / 0.9f) + 0.25f)) : (u2 < 0.6f ? sin2pi_raw((u2 - 0.45f) * (1.0f / 0.6f) + 0.25f) : 0.0f);
    const float dg = (g - x[12]) * (0.28f * sr / f0); x[12] = g;
    const float nz = noise(s);
    const float src = x[5] * (dg + 0.02f * nz) + (x[9] + x[39] * x[5]) * nz * 0.5f + 1e-9f;
    const float n1 = x[24] * src + x[25] * x[13] + x[26] * x[14]; x[14] = x[13]; x[13] = n1;
    const float n2 = x[27] * n1 + x[28] * x[15] + x[29] * x[16]; x[16] = x[15]; x[15] = n2;
    const float n3a = x[30] * n2 + x[31] * x[17] + x[32] * x[18]; x[18] = x[17]; x[17] = n3a;
    const float n3 = x[40] * n3a + x[41] * x[43] + x[42] * x[44]; x[44] = x[43]; x[43] = n3;
    const float fcr = 7000.0f / (x[7] > 1500.0f ? x[7] : 1500.0f), fcomp = clampf(fcr * fcr, 1.0f, 6.0f);      /* the lower the hiss band, the more the high-pass chain takes away: make up for it, so SH and the German CH are as clear as S */
    const float nd = nz - x[46]; x[46] = nz;                         /* frication: the noise is high-passed (differenced) before and after its resonance, so S / SH / F are a clean hiss band, not low-frequency rumble */
    const float fr = x[33] * nd + x[34] * x[19] + x[35] * x[20]; x[20] = x[19]; x[19] = fr;
    const float frd = x[33] * fr + x[34] * x[47] + x[35] * x[48]; x[48] = x[47]; x[47] = frd;      /* a second resonator of the same pitch: 4 poles, so the band is steep and nothing is left below it */
    const float vmod = x[5] > 0.1f ? 1.0f - 0.85f * (x[5] > 0.5f ? 1.0f : (x[5] - 0.1f) * 2.5f) * (1.0f - 2.2f * g) : 1.0f;   /* voiced fricatives (Z, V, ZH): the noise follows the voice pulses */
    float out = n3 * 0.10f + frd * x[6] * 0.17f * fcomp * (vmod > 0.15f ? vmod : 0.15f);
    x[21] += 0.09f * (out - x[21]); out += (x[21] - out) * x[10] * 0.85f;
    if (P) { x[0] += dt; const float dur = P->ms * 0.001f / x[36]; if (x[0] >= dur) { x[0] -= dur; v->ni[0] = idx + 1; } }
    else { x[1] += dt; if (x[1] > 0.08f) v->active = 0; }
    return fast_tanh(out * 1.1f) * 0.9f;
}
