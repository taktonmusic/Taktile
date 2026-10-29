/* ---------------- SPEECH engine, part 1: phoneme table + letter-to-sound rules (English and German) ----------------
   Text (A-Z, 0-9, space, . , ? ! -) -> a list of phoneme ids. No dependencies on the synth: it can be tested on its own. */
typedef struct { const char *nm; int16_t f1, f2, f3; uint16_t ms; uint8_t av, af; int16_t fc, fbw; uint8_t ah, nas; } spkp_t;
/* name, F1 F2 F3 (Hz), duration (ms), voicing, frication, frication centre / bandwidth, aspiration, nasal */
static const spkp_t SPK_P[] = {
    { "_",   500, 1500, 2500,  60,   0,   0,    0,    0,   0,   0 },   /* 0: gap between words */
    { ",",   500, 1500, 2500, 220,   0,   0,    0,    0,   0,   0 },   /* 1: comma */
    { ".",   500, 1500, 2500, 400,   0,   0,    0,    0,   0,   0 },   /* 2: full stop */
    { "IY",  270, 2290, 3010, 115, 215,   0,    0,    0,   0,   0 },
    { "IH",  390, 1990, 2550,  70, 215,   0,    0,    0,   0,   0 },
    { "EH",  530, 1840, 2480,  85, 220,   0,    0,    0,   0,   0 },
    { "AE",  660, 1720, 2410, 110, 225,   0,    0,    0,   0,   0 },
    { "AA",  730, 1090, 2440, 115, 230,   0,    0,    0,   0,   0 },
    { "AO",  570,  840, 2410, 110, 225,   0,    0,    0,   0,   0 },
    { "UH",  440, 1020, 2240,  70, 215,   0,    0,    0,   0,   0 },
    { "UW",  300,  870, 2240, 110, 205,   0,    0,    0,   0,   0 },
    { "AH",  640, 1190, 2390,  80, 225,   0,    0,    0,   0,   0 },
    { "AX",  500, 1500, 2500,  50, 185,   0,    0,    0,   0,   0 },
    { "ER",  490, 1350, 1690, 110, 205,   0,    0,    0,   0,   0 },
    { "IHg", 390, 1990, 2550,  65, 200,   0,    0,    0,   0,   0 },   /* glide end of AY / EY / OY */
    { "UHg", 440, 1020, 2240,  65, 200,   0,    0,    0,   0,   0 },   /* glide end of AW / OW */
    { "M",   250, 1100, 2200,  65, 190,   0,    0,    0,   0, 220 },
    { "N",   250, 1700, 2600,  60, 190,   0,    0,    0,   0, 220 },
    { "NG",  250, 1900, 2400,  70, 190,   0,    0,    0,   0, 220 },
    { "L",   360, 1300, 2700,  65, 190,   0,    0,    0,   0,   0 },
    { "R",   310, 1060, 1380,  70, 190,   0,    0,    0,   0,   0 },
    { "W",   290,  610, 2150,  60, 175,   0,    0,    0,   0,   0 },
    { "Y",   260, 2070, 3020,  55, 175,   0,    0,    0,   0,   0 },
    { "HH",  500, 1500, 2500,  65,   0,   0,    0,    0, 190,   0 },
    { "F",   500, 1500, 2500,  85,   0,  100, 5500, 6000,   0,   0 },
    { "TH",  500, 1500, 2500,  85,   0,  80, 6200, 6000,   0,   0 },
    { "S",   500, 1500, 2500,  95,   0, 150, 7200, 2500,   0,   0 },
    { "SH",  500, 1500, 2500, 100,   0, 170, 3300, 1800,   0,   0 },
    { "V",   250, 1100, 2200,  55, 110,  70, 5500, 6000,   0,   0 },
    { "DH",  250, 1300, 2400,  45, 110,  55, 6000, 6000,   0,   0 },
    { "Z",   250, 1500, 2500,  75, 110, 90, 7200, 2500,   0,   0 },
    { "ZH",  250, 1500, 2500,  75, 110, 110, 3300, 1800,   0,   0 },
    { "XI",  500, 1500, 2500,  95,   0, 150, 4800, 3500,   0,   0 },   /* German "ich" sound */
    { "XA",  500, 1500, 2500,  95,   0, 140, 1800, 1500,   0,   0 },   /* German "ach" sound */
    { "CL",  500, 1500, 2500,  55,   0,   0,    0,    0,   0,   0 },   /* closure of P T K */
    { "CLV", 200, 1300, 2300,  45,  70,   0,    0,    0,   0,   0 },   /* closure of B D G: the voice bar */
    { "BP", 250, 1000, 2200,  12,   0, 115, 1100, 2200,   0,   0 },
    { "BT", 250, 1800, 2700,  14,   0, 175, 4500, 2600,   0,   0 },
    { "BK", 250, 1600, 2300,  16,   0, 150, 2100, 1600,   0,   0 },
    { "BB", 250, 1000, 2200,  10,  80,  70, 1100, 2200,   0,   0 },
    { "BD", 250, 1800, 2700,  10,  80, 100, 4200, 2600,   0,   0 },
    { "BG", 250, 1600, 2300,  12,  80,  90, 2000, 1600,   0,   0 },
    { "ASP", 500, 1500, 2500,  26,   0,   0,    0,    0, 150,   0 },   /* breath after P T K */
    { "ga",  700, 1220, 2600,  75, 225,   0,    0,    0,   0,   0 },   /* German vowels: short / long */
    { "gaa", 730, 1100, 2600, 140, 230,   0,    0,    0,   0,   0 },
    { "ge",  530, 1840, 2480,  80, 220,   0,    0,    0,   0,   0 },
    { "gee", 400, 2100, 2750, 135, 215,   0,    0,    0,   0,   0 },
    { "gi",  390, 1990, 2550,  70, 215,   0,    0,    0,   0,   0 },
    { "gii", 280, 2250, 3010, 125, 210,   0,    0,    0,   0,   0 },
    { "go",  570,  840, 2410,  80, 225,   0,    0,    0,   0,   0 },
    { "goo", 400,  800, 2600, 140, 215,   0,    0,    0,   0,   0 },
    { "gu",  440, 1020, 2240,  70, 215,   0,    0,    0,   0,   0 },
    { "guu", 300,  870, 2240, 125, 205,   0,    0,    0,   0,   0 },
    { "goe", 520, 1400, 2400,  80, 215,   0,    0,    0,   0,   0 },
    { "goee",400, 1550, 2350, 135, 210,   0,    0,    0,   0,   0 },
    { "gy",  390, 1500, 2250,  70, 215,   0,    0,    0,   0,   0 },
    { "gue", 280, 1650, 2100, 125, 205,   0,    0,    0,   0,   0 },
    { "gax", 500, 1500, 2500,  50, 185,   0,    0,    0,   0,   0 },   /* schwa */
    { "gr",  600, 1300, 2500,  85, 200,   0,    0,    0,   0,   0 },   /* vocalised r (-er) */
};
#define SPK_NP ((int)(sizeof SPK_P / sizeof SPK_P[0]))
#define SPK_MAXP 380
static int spk_id(const char *nm) { for (int i = 0; i < SPK_NP; i++) if (strcmp(SPK_P[i].nm, nm) == 0) return i; return 0; }

typedef struct { uint8_t *o; int n; } spk_out_t;
static void spk_put(spk_out_t *w, const char *nm) { if (w->n < SPK_MAXP) w->o[w->n++] = (uint8_t)spk_id(nm); }
static void spk_tok(spk_out_t *w, const char *t) {                      /* one phoneme symbol -> its sound(s) */
    if (!t[0]) return;
    if (!strcmp(t, "P")) { spk_put(w, "CL"); spk_put(w, "BP"); spk_put(w, "ASP"); }
    else if (!strcmp(t, "T")) { spk_put(w, "CL"); spk_put(w, "BT"); spk_put(w, "ASP"); }
    else if (!strcmp(t, "K")) { spk_put(w, "CL"); spk_put(w, "BK"); spk_put(w, "ASP"); }
    else if (!strcmp(t, "B")) { spk_put(w, "CLV"); spk_put(w, "BB"); }
    else if (!strcmp(t, "D")) { spk_put(w, "CLV"); spk_put(w, "BD"); }
    else if (!strcmp(t, "G")) { spk_put(w, "CLV"); spk_put(w, "BG"); }
    else if (!strcmp(t, "CH")) { spk_put(w, "CL"); spk_put(w, "BT"); spk_put(w, "SH"); }
    else if (!strcmp(t, "JH")) { spk_put(w, "CLV"); spk_put(w, "BD"); spk_put(w, "ZH"); }
    else if (!strcmp(t, "TS")) { spk_put(w, "CL"); spk_put(w, "BT"); spk_put(w, "S"); }
    else if (!strcmp(t, "PF")) { spk_put(w, "CL"); spk_put(w, "BP"); spk_put(w, "F"); }
    else if (!strcmp(t, "EY")) { spk_put(w, "EH"); spk_put(w, "IHg"); }
    else if (!strcmp(t, "AY")) { spk_put(w, "AA"); spk_put(w, "IHg"); }
    else if (!strcmp(t, "OW")) { spk_put(w, "AO"); spk_put(w, "UHg"); }
    else if (!strcmp(t, "AW")) { spk_put(w, "AA"); spk_put(w, "UHg"); }
    else if (!strcmp(t, "OY")) { spk_put(w, "AO"); spk_put(w, "IHg"); }
    else if (!strcmp(t, "GAI")) { spk_put(w, "ga"); spk_put(w, "IHg"); }
    else if (!strcmp(t, "GAU")) { spk_put(w, "ga"); spk_put(w, "UHg"); }
    else if (!strcmp(t, "GOI")) { spk_put(w, "go"); spk_put(w, "IHg"); }
    else if (!strcmp(t, "WH")) spk_put(w, "W");
    else spk_put(w, t);
}
static void spk_toks(spk_out_t *w, const char *s) {                     /* "K W AO R" -> sounds */
    char b[8]; int k = 0;
    for (const char *c = s;; c++) {
        if (*c == ' ' || *c == 0) { if (k) { b[k] = 0; spk_tok(w, b); k = 0; } if (!*c) break; }
        else if (k < 7) b[k++] = *c;
    }
}

/* ---- English: NRL-style rules (left context, letters, right context -> phonemes) ---- */
static int sp_isv(char c) { return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U'; }
static int sp_isc(char c) { return c >= 'A' && c <= 'Z' && !sp_isv(c); }
static int sp_isvc(char c) { return c == 'B' || c == 'D' || c == 'V' || c == 'G' || c == 'J' || c == 'L' || c == 'M' || c == 'N' || c == 'R' || c == 'W' || c == 'Z'; }
static int sp_front(char c) { return c == 'E' || c == 'I' || c == 'Y'; }
static int sp_sib(char c) { return c == 'S' || c == 'C' || c == 'G' || c == 'Z' || c == 'X' || c == 'J'; }
static int sp_nonpal(char c) { return c == 'T' || c == 'S' || c == 'R' || c == 'D' || c == 'L' || c == 'Z' || c == 'N' || c == 'J'; }
static int sp_lm(const char *w, int pos, const char *ctx) {            /* left context, matched right to left */
    int i = pos - 1;
    for (int k = (int)strlen(ctx) - 1; k >= 0; k--) {
        const char c = ctx[k];
        if (c == ':') { while (i >= 0 && sp_isc(w[i])) i--; continue; }
        if (i < 0) return 0;
        const char ch = w[i];
        if (c == '#') { if (!sp_isv(ch)) return 0; i--; while (i >= 0 && sp_isv(w[i])) i--; }
        else if (c == '^') { if (!sp_isc(ch)) return 0; i--; }
        else if (c == '.') { if (!sp_isvc(ch)) return 0; i--; }
        else if (c == '+') { if (!sp_front(ch)) return 0; i--; }
        else if (c == '&') { if (!sp_sib(ch)) return 0; i--; }
        else if (c == '@') { if (!sp_nonpal(ch)) return 0; i--; }
        else { if (ch != c) return 0; i--; }
    }
    return 1;
}
static int sp_rm(const char *w, int pos, const char *ctx) {            /* right context, matched left to right */
    int i = pos;
    for (int k = 0; ctx[k]; k++) {
        const char c = ctx[k];
        if (c == ':') { while (w[i] && sp_isc(w[i])) i++; continue; }
        if (!w[i]) return 0;
        const char ch = w[i];
        if (c == '#') { if (!sp_isv(ch)) return 0; i++; while (w[i] && sp_isv(w[i])) i++; }
        else if (c == '^') { if (!sp_isc(ch)) return 0; i++; }
        else if (c == '.') { if (!sp_isvc(ch)) return 0; i++; }
        else if (c == '+') { if (!sp_front(ch)) return 0; i++; }
        else if (c == '&') { if (!sp_sib(ch)) return 0; i++; }
        else if (c == '@') { if (!sp_nonpal(ch)) return 0; i++; }
        else if (c == '%') {                                            /* a suffix: E, ER, ES, ED, ING, ELY at the end of the word */
            static const char *sf[] = { "ELY", "ING", "ER", "ES", "ED", "E" };
            int ok = 0;
            for (int q = 0; q < 6 && !ok; q++) { const int L = (int)strlen(sf[q]); if (strncmp(w + i, sf[q], (size_t)L) == 0 && w[i + L] == ' ') { i += L; ok = 1; } }
            if (!ok) return 0;
        }
        else { if (ch != c) return 0; i++; }
    }
    return 1;
}
typedef struct { const char *l, *m, *r, *o; } spk_rule_t;
static const spk_rule_t SPK_EN[] = {
    /* A */ { " ", "A", " ", "AX" }, { " ", "ARE", " ", "AA R" }, { " ", "AR", "O", "AX R" }, { "", "AR", "#", "EH R" }, { "^", "AS", "#", "EY S" }, { "", "A", "WA", "AX" }, { "", "AW", "", "AO" },
    { " :", "ANY", "", "EH N IY" }, { "", "A", "^+#", "EY" }, { "#:", "ALLY", "", "AX L IY" }, { " ", "AL", "#", "AX L" }, { "", "AGAIN", "", "AX G EH N" }, { "#:", "AG", "E", "IH JH" },
    { "", "A", "^%", "EY" }, { "", "A", "^+:#", "AE" }, { " :", "A", "^+ ", "EY" }, { "", "ARR", "", "AX R" }, { "", "AR", " ", "ER" }, { "", "AR", "", "AA R" }, { "", "AIR", "", "EH R" },
    { "", "AI", "", "EY" }, { "", "AY", "", "EY" }, { "", "AU", "", "AO" }, { "#:", "AL", " ", "AX L" }, { "", "ALK", "", "AO K" }, { "", "AL", "^", "AO L" }, { " :", "ABLE", "", "EY B AX L" },
    { "", "ABLE", "", "AX B AX L" }, { "", "ANG", "+", "EY N JH" }, { "", "A", "", "AE" },
    /* B */ { " ", "BE", "^#", "B IH" }, { "", "BEING", "", "B IY IH NG" }, { "", "BUIL", "", "B IH L" }, { "", "B", "", "B" },
    /* C */ { " ", "CH", "^", "K" }, { "^E", "CH", "", "K" }, { "", "CH", "", "CH" }, { " S", "CI", "#", "S AY" }, { "", "CI", "A", "SH" }, { "", "CI", "O", "SH" }, { "", "CI", "EN", "SH" },
    { "", "C", "+", "S" }, { "", "CK", "", "K" }, { "", "COM", "%", "K AH M" }, { "", "C", "", "K" },
    /* D */ { "#:", "DED", " ", "D IH D" }, { ".E", "D", " ", "D" }, { "#:^E", "D", " ", "T" }, { " ", "DE", "^#", "D IH" }, { " ", "DO", " ", "D UW" }, { " ", "DOES", "", "D AH Z" },
    { " ", "DOING", "", "D UW IH NG" }, { " ", "DOW", "", "D AW" }, { "", "DU", "A", "JH UW" }, { "", "D", "", "D" },
    /* E */ { "#:", "E", " ", "" }, { " :", "E", " ", "IY" }, { "#", "ED", " ", "D" }, { "#:", "E", "D ", "" }, { "", "EV", "ER", "EH V" }, { "", "E", "^%", "IY" }, { "", "ERI", "#", "IY R IY" },
    { "", "ERI", "", "EH R IH" }, { "#:", "ER", "#", "ER" }, { "", "ER", "#", "EH R" }, { "", "ER", "", "ER" }, { " ", "EVEN", "", "IY V EH N" }, { "#:", "E", "W", "" }, { "@", "EW", "", "UW" },
    { "", "EW", "", "Y UW" }, { "", "E", "O", "IY" }, { "#:&", "ES", " ", "IH Z" }, { "#:", "E", "S ", "" }, { "#:", "ELY", " ", "L IY" }, { "#:", "EMENT", "", "M EH N T" }, { "", "EFUL", "", "F UH L" },
    { "", "EE", "", "IY" }, { "", "EARN", "", "ER N" }, { " ", "EAR", "^", "ER" }, { "", "EAD", "", "EH D" }, { "#:", "EA", " ", "IY AX" }, { "", "EA", "SU", "EH" }, { "", "EA", "", "IY" },
    { "", "EIGH", "", "EY" }, { "", "EI", "", "IY" }, { " ", "EYE", "", "AY" }, { "", "EY", "", "IY" }, { "", "EU", "", "Y UW" }, { "", "E", "", "EH" },
    /* F */ { "", "FUL", "", "F UH L" }, { "", "F", "", "F" },
    /* G */ { "", "GIV", "", "G IH V" }, { " ", "G", "I^", "G" }, { "", "GE", "T", "G EH" }, { "SU", "GGES", "", "G JH EH S" }, { "", "GG", "", "G" }, { " B#", "G", "", "G" }, { "", "G", "+", "JH" },
    { "", "GREAT", "", "G R EY T" }, { "#", "GH", "", "" }, { "", "G", "", "G" },
    /* H */ { " ", "HAV", "", "HH AE V" }, { " ", "HERE", "", "HH IY R" }, { " ", "HOUR", "", "AW ER" }, { "", "HOW", "", "HH AW" }, { "", "H", "#", "HH" }, { "", "H", "", "" },
    /* I */ { " ", "IN", "", "IH N" }, { " ", "I", " ", "AY" }, { "", "IN", "D", "AY N" }, { "", "IER", "", "IY ER" }, { "#:R", "IED", "", "IY D" }, { "", "IED", " ", "AY D" }, { "", "IEN", "", "IY EH N" },
    { "", "IE", "T", "AY EH" }, { " :", "I", "%", "AY" }, { "", "I", "%", "IY" }, { "", "IE", "", "IY" }, { "", "I", "^+:#", "IH" }, { "", "IR", "#", "AY R" }, { "", "IZ", "%", "AY Z" },
    { "", "IS", "%", "AY Z" }, { "", "I", "D%", "AY" }, { "+^", "I", "^+", "IH" }, { "", "I", "T%", "AY" }, { "#^:", "I", "^+", "IH" }, { "", "I", "^+", "AY" }, { "", "IR", "", "ER" },
    { "", "IGH", "", "AY" }, { "", "ILD", "", "AY L D" }, { "", "IGN", " ", "AY N" }, { "", "IGN", "^", "AY N" }, { "", "IGN", "%", "AY N" }, { "", "IQUE", "", "IY K" }, { "", "I", "", "IH" },
    /* J */ { "", "J", "", "JH" },
    /* K */ { " ", "K", "N", "" }, { "", "K", "", "K" },
    /* L */ { "", "LO", "C#", "L OW" }, { "L", "L", "", "" }, { "#:^", "L", "%", "AX L" }, { "", "LEAD", "", "L IY D" }, { "", "L", "", "L" },
    /* M */ { "", "MOV", "", "M UW V" }, { "", "M", "", "M" },
    /* N */ { "E", "NG", "+", "N JH" }, { "", "NG", "R", "NG G" }, { "", "NG", "#", "NG G" }, { "", "NGL", "%", "NG G AX L" }, { "", "NG", "", "NG" }, { "", "NK", "", "NG K" }, { " ", "NOW", " ", "N AW" }, { "", "N", "", "N" },
    /* O */ { "", "OF", " ", "AX V" }, { "", "OROUGH", "", "ER OW" }, { "#:", "OR", " ", "ER" }, { "#:", "ORS", " ", "ER Z" }, { "", "OR", "", "AO R" }, { " ", "ONE", "", "W AH N" }, { "", "OW", "", "OW" },
    { "", "OVER", "", "OW V ER" }, { "", "OV", "", "AH V" }, { "", "O", "^%", "OW" }, { "", "O", "^EN", "OW" }, { "", "O", "^I#", "OW" }, { "", "OL", "D", "OW L" }, { "", "OUGHT", "", "AO T" },
    { "", "OUGH", "", "AH F" }, { " ", "OU", "", "AW" }, { "H", "OU", "S#", "AW" }, { "", "OUS", "", "AX S" }, { "", "OUR", "", "AO R" }, { "", "OULD", "", "UH D" }, { "^", "OU", "^L", "AH" },
    { "", "OUP", "", "UW P" }, { "", "OU", "", "AW" }, { "", "OY", "", "OY" }, { "", "OING", "", "OW IH NG" }, { "", "OI", "", "OY" }, { "", "OOR", "", "AO R" }, { "", "OOK", "", "UH K" },
    { "", "OOD", "", "UH D" }, { "", "OO", "", "UW" }, { "", "O", "E", "OW" }, { "", "O", " ", "OW" }, { "", "OA", "", "OW" }, { " ", "ONLY", "", "OW N L IY" }, { " ", "ONCE", "", "W AH N S" },
    { "C", "O", "N", "AA" }, { "", "O", "NG", "AO" }, { " :^", "O", "N", "AH" }, { "I", "ON", "", "AX N" }, { "#:", "ON", " ", "AX N" }, { "#^", "ON", "", "AX N" }, { "", "O", "ST ", "OW" },
    { "", "OF", "^", "AO F" }, { "", "OTHER", "", "AH DH ER" }, { "", "OSS", " ", "AO S" }, { "#:^", "OM", "", "AH M" }, { "", "O", "", "AA" },
    /* P */ { "", "PH", "", "F" }, { "", "PEOP", "", "P IY P" }, { "", "POW", "", "P AW" }, { "", "PUT", " ", "P UH T" }, { "", "P", "", "P" },
    /* Q */ { "", "QUAR", "", "K W AO R" }, { "", "QU", "", "K W" }, { "", "Q", "", "K" },
    /* R */ { " ", "RE", "^#", "R IY" }, { "", "R", "", "R" },
    /* S */ { "", "SH", "", "SH" }, { "#", "SION", "", "ZH AX N" }, { "", "SOME", "", "S AH M" }, { "#", "SUR", "#", "ZH ER" }, { "", "SURE", "", "SH UH R" }, { "#", "SU", "#", "ZH UW" },
    { "#", "SSU", "#", "SH UW" }, { "#", "SED", " ", "Z D" }, { "#", "S", "#", "Z" }, { "", "SAID", "", "S EH D" }, { "^", "SION", "", "SH AX N" }, { "", "S", "S", "" }, { ".", "S", " ", "Z" },
    { "#:.E", "S", " ", "Z" }, { "#:^#", "S", " ", "S" }, { "U", "S", " ", "S" }, { " :#", "S", " ", "Z" }, { " ", "SCH", "", "S K" }, { "", "S", "C+", "" }, { "#", "SM", "", "Z M" }, { "", "S", "", "S" },
    /* T */ { " ", "THE", " ", "DH AX" }, { "", "TO", " ", "T UW" }, { "", "THAT", " ", "DH AE T" }, { " ", "THIS", " ", "DH IH S" }, { " ", "THEY", "", "DH EY" }, { " ", "THERE", "", "DH EH R" },
    { "", "THER", "", "DH ER" }, { "", "THEIR", "", "DH EH R" }, { " ", "THAN", " ", "DH AE N" }, { " ", "THEM", " ", "DH EH M" }, { "", "THESE", " ", "DH IY Z" }, { " ", "THEN", "", "DH EH N" },
    { "", "THROUGH", "", "TH R UW" }, { "", "THOSE", "", "DH OW Z" }, { "", "THOUGH", " ", "DH OW" }, { "", "TODAY", "", "T AX D EY" }, { "", "TO", "TAL", "T OW" }, { " ", "THUS", "", "DH AH S" },
    { "", "TH", "", "TH" }, { "#:", "TED", " ", "T IH D" }, { "S", "TI", "#N", "CH" }, { "", "TI", "O", "SH" }, { "", "TI", "A", "SH" }, { "", "TIEN", "", "SH AX N" }, { "", "TUR", "#", "CH ER" },
    { "", "TU", "A", "CH UW" }, { " ", "TWO", "", "T UW" }, { "", "T", "", "T" },
    /* U */ { " ", "UN", "I", "Y UW N" }, { " ", "UN", "", "AH N" }, { " ", "UPON", "", "AX P AO N" }, { "@", "UR", "#", "UH R" }, { "", "UR", "#", "Y UH R" }, { "", "UR", "", "ER" },
    { "", "U", "^ ", "AH" }, { "", "U", "^^", "AH" }, { "", "UY", "", "AY" }, { " G", "U", "#", "" }, { "G", "U", "%", "" }, { "G", "U", "#", "W" }, { "#N", "U", "", "Y UW" }, { "@", "U", "", "UW" },
    { "", "U", "", "Y UW" },
    /* V */ { "", "VIEW", "", "V Y UW" }, { "", "V", "", "V" },
    /* W */ { " ", "WERE", "", "W ER" }, { "", "WA", "S", "W AA" }, { "", "WA", "T", "W AA" }, { "", "WHERE", "", "W EH R" }, { "", "WHAT", "", "W AA T" }, { "", "WHOL", "", "HH OW L" },
    { "", "WHO", "", "HH UW" }, { "", "WH", "", "W" }, { "", "WAR", "", "W AO R" }, { "", "WOR", "^", "W ER" }, { "", "WR", "", "R" }, { "", "W", "", "W" },
    /* X */ { "", "X", "", "K S" },
    /* Y */ { "", "YOUNG", "", "Y AH NG" }, { " ", "YOU", "", "Y UW" }, { " ", "YES", "", "Y EH S" }, { " ", "Y", "", "Y" }, { "#:^", "Y", " ", "IY" }, { "#:^", "Y", "I", "IY" }, { " :", "Y", " ", "AY" },
    { " :", "Y", "#", "AY" }, { " :", "Y", "^+:#", "IH" }, { " :", "Y", "^#", "AY" }, { "", "Y", "", "IH" },
    /* Z */ { "", "Z", "", "Z" },
};
static const char *SPK_EN_DICT[][2] = {                                  /* words the rules get wrong, and the names of this project */
    { "TAKTILE", "T AE K T AY L" }, { "TAKT", "T AA K T" }, { "ROBOT", "R OW B AA T" }, { "COMPUTER", "K AX M P Y UW T ER" }, { "HELLO", "HH EH L OW" }, { "MOVE", "M UW V" },
    { "GROOVE", "G R UW V" }, { "SCHWUNG", "SH V UH NG" }, { "ACID", "AE S IH D" }, { "BASS", "B EY S" }, { "TECHNO", "T EH K N OW" }, { "ABLETON", "AE B AX L T AX N" },
    { "WORLD", "W ER L D" }, { "SPEAK", "S P IY K" }, { "SPEECH", "S P IY CH" }, { "VOICE", "V OY S" }, { "SYNTH", "S IH N TH" }, { "DRUM", "D R AH M" }, { "THE", "DH AX" },
    { "A", "AX" }, { "TO", "T UW" }, { "OF", "AX V" }, { "ARE", "AA R" }, { "YOU", "Y UW" }, { "ONE", "W AH N" }, { "TWO", "T UW" }, { "THREE", "TH R IY" }, { "FOUR", "F AO R" },
    { "FIVE", "F AY V" }, { "SIX", "S IH K S" }, { "SEVEN", "S EH V AX N" }, { "EIGHT", "EY T" }, { "NINE", "N AY N" }, { "ZERO", "Z IY R OW" }, { "WHAT", "W AH T" },
    { "LOVE", "L AH V" }, { "COME", "K AH M" }, { "SOME", "S AH M" }, { "DONE", "D AH N" }, { "GONE", "G AO N" }, { "HAVE", "HH AE V" }, { "GIVE", "G IH V" }, { "LIVE", "L IH V" },
    { "SAY", "S EY" }, { "SAYS", "S EH Z" }, { "IS", "IH Z" }, { "AS", "AE Z" }, { "HIS", "HH IH Z" }, { "WAS", "W AH Z" }, { "OH", "OW" }, { "NO", "N OW" }, { "GO", "G OW" }, { "SO", "S OW" },
    { "DO", "D UW" }, { "WHO", "HH UW" }, { "WE", "W IY" }, { "HE", "HH IY" }, { "ME", "M IY" }, { "BE", "B IY" }, { "MY", "M AY" }, { "BY", "B AY" }, { "HI", "HH AY" },
};
static void spk_en_word(spk_out_t *w, const char *word) {
    char b[64]; int L = 0; b[L++] = ' ';
    for (const char *c = word; *c && L < 60; c++) if (*c >= 'A' && *c <= 'Z') { if (L > 1 && *c == b[L - 1] && strchr("BDFGLMNPRTZS", *c)) continue; b[L++] = *c; }      /* a doubled consonant letter is one sound */
    b[L++] = ' '; b[L] = 0;
    if (L <= 2) return;
    for (unsigned i = 0; i < sizeof SPK_EN_DICT / sizeof SPK_EN_DICT[0]; i++)
        if (strlen(SPK_EN_DICT[i][0]) == (size_t)(L - 2) && strncmp(b + 1, SPK_EN_DICT[i][0], (size_t)(L - 2)) == 0) { spk_toks(w, SPK_EN_DICT[i][1]); return; }
    int pos = 1;
    while (pos < L - 1) {
        int hit = 0;
        for (unsigned r = 0; r < sizeof SPK_EN / sizeof SPK_EN[0]; r++) {
            const spk_rule_t *u = &SPK_EN[r]; const int ml = (int)strlen(u->m);
            if (u->m[0] != b[pos] || strncmp(b + pos, u->m, (size_t)ml) != 0) continue;
            if (!sp_lm(b, pos, u->l) || !sp_rm(b, pos + ml, u->r)) continue;
            spk_toks(w, u->o); pos += ml; hit = 1; break;
        }
        if (!hit) pos++;
    }
}

/* ---- German: rules written out in code (the spelling is far more regular) ---- */
/* internal letters: a = A-umlaut, o = O-umlaut, u = U-umlaut, s = sharp s */
static int de_v(char c) { return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'a' || c == 'o' || c == 'u' || c == 'Y'; }
static int de_c(char c) { return (c >= 'A' && c <= 'Z') && !de_v(c); }
static void de_vowel(spk_out_t *w, char c, int lng) {
    switch (c) {
    case 'A': spk_put(w, lng ? "gaa" : "ga"); break;
    case 'E': spk_put(w, lng ? "gee" : "ge"); break;
    case 'I': spk_put(w, lng ? "gii" : "gi"); break;
    case 'O': spk_put(w, lng ? "goo" : "go"); break;
    case 'U': spk_put(w, lng ? "guu" : "gu"); break;
    case 'a': spk_put(w, lng ? "gee" : "ge"); break;
    case 'o': spk_put(w, lng ? "goee" : "goe"); break;
    case 'u': case 'Y': spk_put(w, lng ? "gue" : "gy"); break;
    }
}
static const char *DE_SHORT[] = { "MIT", "IST", "IN", "AN", "BIN", "HAT", "DAS", "WAS", "ES", "IM", "AM", "AB", "VON", "VOM", "ZUM", "ZUR", "UND", "AUF", "AUS", "BEI", "NACH", "NUR", "DASS", "DOCH", "NOCH", "SIND", "BIS", "ALS", "OB", "UM", "DURCH", "FUER", "MICH", "DICH", "SICH", "MAN", "WILL", "SOLL", "KANN", "MUSS", "WIRD", "HIER", "WIE", "VIEL", "GANZ", "TAKT" };
static void spk_de_word(spk_out_t *w, const char *word) {
    char b[64]; int n = 0;
    for (const char *c = word; *c && n < 58; c++) {                    /* letters, with AE OE UE SS -> internal single letters */
        if (*c < 'A' || *c > 'Z') continue;
        if (c[1] == 'E' && (*c == 'A' || *c == 'O' || *c == 'U') && !(n > 0 && b[n - 1] == 'Q')) { b[n++] = *c == 'A' ? 'a' : (*c == 'O' ? 'o' : 'u'); c++; continue; }
        b[n++] = *c;
    }
    b[n] = 0; if (!n) return;
    int shortw = 0; for (unsigned i = 0; i < sizeof DE_SHORT / sizeof DE_SHORT[0]; i++) if (!strcmp(b, DE_SHORT[i])) shortw = 1;
    int i = 0;
    while (i < n) {
        const char c = b[i], p1 = i + 1 < n ? b[i + 1] : 0, p2 = i + 2 < n ? b[i + 2] : 0, p3 = i + 3 < n ? b[i + 3] : 0;
        const char pv = i > 0 ? b[i - 1] : 0;
        const int first = (i == 0);
        if (c == 'T' && p1 == 'S' && p2 == 'C' && p3 == 'H') { spk_tok(w, "CH"); i += 4; continue; }
        if (c == 'S' && p1 == 'C' && p2 == 'H') { spk_put(w, "SH"); i += 3; continue; }
        if (c == 'C' && p1 == 'H' && p2 == 'S') { spk_tok(w, "K"); spk_put(w, "S"); i += 3; continue; }
        if (c == 'C' && p1 == 'H') { const int back = (pv == 'A' || pv == 'O' || pv == 'U'); spk_put(w, back ? "XA" : "XI"); i += 2; continue; }
        if (c == 'C' && p1 == 'K') { spk_tok(w, "K"); i += 2; continue; }
        if (c == 'P' && p1 == 'H') { spk_put(w, "F"); i += 2; continue; }
        if (c == 'P' && p1 == 'F') { spk_tok(w, "PF"); i += 2; continue; }
        if (c == 'T' && p1 == 'Z') { spk_tok(w, "TS"); i += 2; continue; }
        if (c == 'Q' && p1 == 'U') { spk_tok(w, "K"); spk_put(w, "V"); i += 2; continue; }
        if (c == 'N' && p1 == 'G') { spk_put(w, "NG"); i += 2; continue; }
        if (c == 'N' && p1 == 'K') { spk_put(w, "NG"); spk_tok(w, "K"); i += 2; continue; }
        if (c == 'D' && p1 == 'T') { spk_tok(w, "T"); i += 2; continue; }
        if (c == 'T' && p1 == 'H') { spk_tok(w, "T"); i += 2; continue; }
        if (c == 'S' && p1 == 'S') { spk_put(w, "S"); i += 2; continue; }
        if (c == 's') { spk_put(w, "S"); i++; continue; }
        if (de_c(c) && p1 == c && c != 'S') { i++; continue; }                         /* a doubled consonant is one sound */
        if (c == 'E' && p1 == 'I') { spk_tok(w, "GAI"); i += 2; continue; }
        if (c == 'A' && p1 == 'I') { spk_tok(w, "GAI"); i += 2; continue; }
        if ((c == 'E' || c == 'A') && p1 == 'Y') { spk_tok(w, "GAI"); i += 2; continue; }
        if (c == 'E' && p1 == 'U') { spk_tok(w, "GOI"); i += 2; continue; }
        if (c == 'a' && p1 == 'U') { spk_tok(w, "GOI"); i += 2; continue; }
        if (c == 'A' && p1 == 'U') { spk_tok(w, "GAU"); i += 2; continue; }
        if (c == 'I' && p1 == 'E') { spk_put(w, "gii"); i += 2; if (b[i] == 'H') i++; continue; }
        if ((c == 'A' || c == 'E' || c == 'O') && p1 == c) { de_vowel(w, c, 1); i += 2; continue; }
        if (de_v(c)) {
            if (c == 'E') {
                const int fin = (i == n - 1);
                const int suf = (i == n - 2 && (p1 == 'N' || p1 == 'L' || p1 == 'M' || p1 == 'S' || p1 == 'R')) || (i == n - 3 && p1 == 'N' && p2 == 'D');
                if (fin) { spk_put(w, "gax"); i++; continue; }
                if (i == n - 2 && p1 == 'R') { spk_put(w, "gr"); i += 2; continue; }
                if (suf) { spk_put(w, "gax"); i++; continue; }
                if (i == 1 && (pv == 'B' || pv == 'G') && de_c(p1)) { spk_put(w, "gax"); i++; continue; }
            }
            int lng;
            if (p1 == 'H') lng = 1;                                                                     /* vowel + h: long, the h is silent */
            else if (i == n - 1) lng = !shortw;                                                         /* a vowel at the end: ja, da, wo */
            else if (de_c(p1) && p1 != 'X' && (i + 2 >= n || de_v(b[i + 2]))) lng = !shortw;           /* one consonant, then a vowel (or the end): long */
            else lng = 0;
            de_vowel(w, c, lng); i++;
            if (lng && b[i] == 'H') i++;
            continue;
        }
        switch (c) {
        case 'B': case 'D': case 'G': {
            const int endsyl = (i == n - 1) || (i > 0 && de_c(p1) && p1 != 'R' && p1 != 'L');
            if (c == 'G' && pv == 'I' && i == n - 1) { spk_put(w, "XI"); break; }
            if (endsyl) spk_tok(w, c == 'B' ? "P" : (c == 'D' ? "T" : "K")); else { char t[2] = { c, 0 }; spk_tok(w, t); }
            break; }
        case 'S':
            if (first && (p1 == 'P' || p1 == 'T')) { spk_put(w, "SH"); break; }
            if (de_v(p1)) spk_put(w, "Z"); else spk_put(w, "S");
            break;
        case 'Z': spk_tok(w, "TS"); break;
        case 'C': if (p1 == 'E' || p1 == 'I' || p1 == 'a') spk_tok(w, "TS"); else spk_tok(w, "K"); break;
        case 'V': spk_put(w, "F"); break;
        case 'W': spk_put(w, "V"); break;
        case 'J': spk_put(w, "Y"); break;
        case 'X': spk_tok(w, "K"); spk_put(w, "S"); break;
        case 'H': if (de_v(p1)) spk_put(w, "HH"); break;
        case 'R':
            if (de_v(p1)) spk_put(w, "R");
            else if (de_v(pv) && (i == n - 1 || de_c(p1))) spk_put(w, "gr");
            else spk_put(w, "R");
            break;
        case 'F': spk_put(w, "F"); break;
        case 'L': spk_put(w, "L"); break;
        case 'M': spk_put(w, "M"); break;
        case 'N': spk_put(w, "N"); break;
        case 'K': spk_tok(w, "K"); break;
        case 'P': spk_tok(w, "P"); break;
        case 'T': spk_tok(w, "T"); break;
        default: break;
        }
        i++;
    }
}

/* ---- text -> phoneme list ---- */
static const char *SPK_EN_NUM[10] = { "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE" };
static const char *SPK_DE_NUM[10] = { "NULL", "EINS", "ZWEI", "DREI", "VIER", "FUENF", "SECHS", "SIEBEN", "ACHT", "NEUN" };

/* ---- single letters (PLAY = LETTER): every step of the pattern says one letter or sound. EN / DE sounds as phoneme lists ---- */
#define SPK_NLET 33
static const char *SPK_LET[SPK_NLET][2] = {
    { "AE AE AE", "gaa gaa" }, { "EH EH EH", "gee gee" }, { "IH IY IY", "gii gii" }, { "AO AO AO", "goo goo" }, { "AH AH AH", "guu guu" },
    { "B AH AH", "B gax gax gax" }, { "K AH AH", "TS gee" }, { "D AH AH", "D gax gax gax" }, { "F F AH", "F F gax" }, { "G AH AH", "G gax gax gax" },
    { "HH AH AH", "HH gax gax gax" }, { "JH AH AH", "Y gax gax gax" }, { "K AH AH", "K gax gax gax" }, { "L AH AH", "L gax gax gax" }, { "M M AH", "M M gax" },
    { "N N AH", "N N gax" }, { "P AH AH", "P gax gax gax" }, { "K W AH", "K V gax" }, { "R R AH", "R R gax gax" }, { "S S AH", "Z Z gax" },
    { "T AH AH", "T gax gax gax" }, { "V V AH", "F F gax" }, { "W AH AH", "V V gax" }, { "K S S", "K S S" }, { "Y AH AH", "gue gue" },
    { "Z Z AH", "TS gee" }, { "SH SH AH", "SH SH gax" }, { "CH AH AH", "CH gax gax gax" }, { "TH TH AH", "TH TH gax" }, { "NG NG AH", "NG NG gax" },
    { "AE AE AE", "ge ge ge" }, { "ER ER ER", "goe goe goe" }, { "UW UW IY", "gue gue gue" },
};
static int spk_letter(int idx, int lang, uint8_t *out) {
    if (idx < 0) idx = 0; if (idx >= SPK_NLET) idx = SPK_NLET - 1;
    spk_out_t w = { out, 0 }; spk_toks(&w, SPK_LET[idx][lang ? 1 : 0]); return w.n;
}
static int spk_compile(const char *txt, int lang, uint8_t *out) {
    spk_out_t w = { out, 0 };
    char word[48]; int wl = 0, any = 0;
    for (int k = 0;; k++) {
        char c = txt[k];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        const int letter = (c >= 'A' && c <= 'Z');
        if (letter && wl < 44) { word[wl++] = c; continue; }
        if (wl) {
            word[wl] = 0; if (any) spk_put(&w, "_"); any = 1;
            if (lang) spk_de_word(&w, word); else spk_en_word(&w, word);
            wl = 0;
        }
        if (!c) break;
        if (c >= '0' && c <= '9') { if (any) spk_put(&w, "_"); any = 1; const char *nw = lang ? SPK_DE_NUM[c - '0'] : SPK_EN_NUM[c - '0']; if (lang) spk_de_word(&w, nw); else spk_en_word(&w, nw); }
        else if (c == '.' || c == '!' || c == '?') { spk_put(&w, "."); any = 0; }
        else if (c == ',' || c == ';' || c == ':' || c == '-') { spk_put(&w, ","); any = 0; }
    }
    return w.n;
}
