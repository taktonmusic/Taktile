/*
 * Takton Groove v0.1 - Schwung overtake groovebox (UI in ui.js + this DSP) for Ableton Move
 *
 * All 32 pads (MIDI notes 36..67) are independent voices. Hit a pad while the
 * module is on screen and it becomes the focused pad; knobs 1-8 then edit
 * that pad. Every pad has its own:
 *   Type (30 sounds: kicks, drums, basses, pluck/stab/pad/lead/bell, fx)
 *   Tune, Decay, Tone (low-pass), Drive, Volume, Pan
 *   Arp (Off/Up/Down/UpDn/Rand/Roll) + Rate, Delay send, Reverb send
 * Master: Volume, Key, Scale, Delay (time/feedback/return),
 *         Reverb (size/dark/return) - the dark long reverb makes the "rumble".
 *
 * Realtime rules (Schwung docs/MODULES.md): every entry point runs on the SPI
 * audio callback. No malloc/free, no file I/O, no logging. Instances live in a
 * static pool.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#include "host/plugin_api_v1.h"

/* Pin libm symbols to old glibc versions so the module loads on the Move. */
#if defined(__aarch64__) && defined(__GLIBC__)
__asm__(".symver expf,expf@GLIBC_2.17");
__asm__(".symver powf,powf@GLIBC_2.17");
#endif

#define NPADS      32                 /* MIDI pads */
#define NLANE      80                 /* sound lanes: 0-15 = tracks, 16-79 = four drum kits x 16 sounds */
#define PAD_BASE   36
#define MAX_INST   4
#define TWO_PI     6.28318530717958647692f
#define DLY_SIZE   32768          /* per track */
#define CH_SIZE    2048
#define SB_SIZE    8192           /* stutter / reverse capture buffer per track */
#define XB_SIZE    1024           /* insert effect buffers (flanger, comb) */
#define DLY_MASK   (DLY_SIZE - 1)
#define RV_SIZE    4096
#define RV_MASK    (RV_SIZE - 1)
#define NLINES     4
#define NTRK       16
#define NPAT       8
#define MAXSTEP    128

/* ---------------- per-pad parameters ---------------- */
#include "variants_n.h"
#include "variants3_n.h"
#include "variants4_n.h"
enum { PP_TYPE, PP_VOL, PP_PAN, PP_TUNE, PP_DECAY, PP_TONE, PP_DRIVE, PP_DLY, PP_RVB, PP_ARP, PP_RATE, PP_CRUSH, PP_LOCUT, PP_RING, PP_PUMP, PP_WOB, PP_CUT, PP_RES, PP_FENV, PP_WAVE, PP_DET, PP_SUB, PP_ATT, PP_POLY, PP_DTIME, PP_DFB, PP_RSIZE, PP_RDARK, PP_CHOR, PP_CHRT, PP_CHDP, PP_PHAS, PP_PHRT, PP_PHFB, PP_XFX, PP_XMIX, PP_XRT, PP_XDP, PP_MFX, PP_MAMT, PP_MPRM, PP_SMP, PP_SST, PP_SEN, PP_SMD, PP_DUCK, PP_DUCKR, PP_SGN, PP_SATK, PP_SREL, PP_SSTR, PP_SCS, PP_TSW, PP_FDEC, PP_FKEY, PP_FMODE, PP_RMB, PP_RMT, PP_EQL, PP_EQM, PP_EQH, PP_N };
#define NGH 24                       /* ghost voices: released tails of retriggered notes in POLY mode */

enum {
    T_KICK, T_KICKHRD, T_KICKGOA, T_RUMBKICK, T_SUB808, T_CLAP, T_SNARE, T_RIM,
    T_HATCL, T_HATOP, T_SHAKER, T_CRASH, T_RIDE, T_TOM, T_CONGA, T_COWBELL,
    T_PERC, T_ZAP, T_RISER, T_SUBBASS, T_ROLLBASS, T_ACID, T_REESE, T_PLUCK,
    T_STAB, T_PAD, T_LEAD, T_BELL, T_SWEEP, T_IMPACT,
    T_V0,
    T_S0 = T_V0 + 43,                       /* synth family: SynLead SynBass SuperSaw EPiano Organ Strings FMBass Hoover */
    T_SLEAD = T_S0, T_SBASS, T_SSUPER, T_SEPIANO, T_SORGAN, T_SSTRINGS, T_SFMBASS, T_SHOOVER,
    T_S1 = T_S0 + 8,                        /* variants of the synth family */
    T_V2 = T_S1 + 15,                       /* generated variants (src/variants.inc) */
    T_SAMPLE = T_V2 + N_V2,
    T_V3 = T_SAMPLE + 1,                    /* extra presets after the sample slot (src/variants3.inc), keeps old saves valid */
    T_V4 = T_V3 + N_V3,                     /* NEW ENGINES: modal, FM, vocal, chip, morph (src/variants4.inc) */
    T_COUNT = T_V4 + N_V4
};

typedef struct { const char *key, *name; int enum_n; float def; } pd_t;
static const pd_t PD[PP_N] = {
    { "type",  "Type",   T_COUNT, 0.0f },
    { "vol",   "Vol",    0, 0.80f },
    { "pan",   "Pan",    0, 0.50f },
    { "tune",  "Tune",   0, 0.50f },
    { "decay", "Decay",  0, 0.50f },
    { "tone",  "Tone",   0, 1.00f },
    { "drive", "Drive",  0, 0.00f },
    { "dly",   "Delay",  0, 0.00f },
    { "rvb",   "Reverb", 0, 0.00f },
    { "arp",   "Arp",    6, 0.0f },
    { "rate",  "Rate",   4, 1.0f },
    { "crush", "Crush",  0, 0.00f },
    { "locut", "LowCut", 0, 0.00f },
    { "ring",  "Ring",   0, 0.00f },
    { "pump",  "Pump",   0, 0.00f },
    { "wob",   "Wobble", 0, 0.00f },
    { "cut",   "Cutoff", 0, 1.00f },
    { "res",   "Reso",   0, 0.00f },
    { "fenv",  "FltEnv", 0, 0.00f },
    { "wave",  "Wave",   16, 0.0f },
    { "det",   "Detune", 0, 0.00f },
    { "sub",   "Sub",    0, 0.00f },
    { "att",   "Attack", 0, 0.00f },
    { "poly",  "Voices", 2, 0.0f },
    { "dtime", "D Time", 5, 1.0f },
    { "dfb",   "D Fdbk", 0, 0.40f },
    { "rsize", "R Size", 0, 0.55f },
    { "rdark", "R Dark", 0, 0.50f },
    { "chor",  "Chorus", 0, 0.00f },
    { "chrt",  "Ch Rate",0, 0.30f },
    { "chdp",  "Ch Depth",0,0.50f },
    { "phas",  "Phaser", 0, 0.00f },
    { "phrt",  "Ph Rate",0, 0.30f },
    { "phfb",  "Ph Fdbk",0, 0.50f },
    { "xfx",   "FX",     11, 0.0f },
    { "xmix",  "FX Amt", 0, 0.50f },
    { "xrt",   "FX Rate",0, 0.50f },
    { "xdp",   "FX Dpth",0, 0.50f },
    { "mfx",   "MIDI FX",11, 0.0f },
    { "mamt",  "M Amt",  0, 0.50f },
    { "mprm",  "M Prm",  0, 0.50f },
    { "smp",   "Sample", 0, 0.00f },
    { "sst",   "Start",  0, 0.00f },
    { "sen",   "End",    0, 1.00f },
    { "smd",   "Mode",   0, 0.00f },
    { "duck",  "Duck",   0, 0.00f },
    { "duckr", "Duck Rel",0, 0.40f },
    { "sgn",   "S Gain", 0, 0.50f },
    { "satk",  "S Atk",  0, 0.00f },
    { "srel",  "S Rel",  0, 0.00f },
    { "sstr",  "Stretch",0, 0.50f },
    { "scs",   "Duck Src",0, 0.00f },
    { "tsw",   "Swing+", 0, 0.50f },
    { "fdec",  "F Decay",0, 0.50f },
    { "fkey",  "F Key",  0, 0.00f },
    { "fmode", "F Mode", 0, 0.00f },
    { "rmb",   "Rumble", 0, 0.00f },
    { "rmt",   "R Tail", 0, 0.50f },
    { "eql",   "EQ Low", 0, 0.50f },
    { "eqm",   "EQ Mid", 0, 0.50f },
    { "eqh",   "EQ High",0, 0.50f },
};

static const char *TYPE_NAME[T_COUNT] = {
    "Kick", "KickHrd", "KickGoa", "RumbKick", "Sub808", "Clap", "Snare", "Rim",
    "HatCl", "HatOp", "Shaker", "Crash", "Ride", "Tom", "Conga", "Cowbell",
    "PercFM", "Zap", "Riser", "SubBass", "RollBass", "Acid", "Reese", "Pluck",
    "Stab", "Pad", "Lead", "Bell", "Sweep", "Impact",
    "KickDeep", "KickClick", "KickPsy", "KickDub", "Kick909", "Sub808Lo", "Sub808Hi", "SnrTight", "SnrFat", "ClapLong", "ClapShrt", "HatTick", "HatLong", "ShakeLng", "RimClick", "TomLo", "TomHi", "CongaHi", "CongaLo", "PercHi", "PercLo", "ZapHi", "ZapLo", "Bass1", "BassHi", "AcidLo", "AcidHi", "ReeseLo", "ReeseHi", "SubLow", "PluckLo", "PluckHi", "StabLo", "StabHi", "PadLo", "PadSlow", "LeadLo", "LeadHi", "BellLo", "BellHi", "SweepLng", "ImpactLo", "RiserLng",
    "SynLead", "SynBass", "SuperSaw", "EPiano", "Organ", "Strings", "FMBass", "Hoover",
    "LeadSoft", "LeadHi", "SawBass", "BassHi", "SuperHi", "SuperLo", "KeysHi", "KeysLo", "OrganLo", "OrganHi", "StringLo", "StringSlw", "FMBassHi", "HooverLo", "HooverHi",
    #include "variants_names.inc"
    , "Sample",
    #include "variants3_names.inc"
    ,
    #include "variants4_names.inc"
};
static const char *WAVE_NAME[16] = { "Auto", "Saw", "Square", "Tri", "Sine", "Pulse", "Pulse12", "RampDn", "HalfSin", "Organ", "Hollow", "Fold", "Stair", "Digi", "Vowel", "Sync" };
static const char *POLY_NAME[2] = { "Mono", "Poly" };
static const char *ARP_NAME[6]   = { "Off", "Up", "Down", "UpDn", "Rand", "Roll" };
static const char *RATE_NAME[4]  = { "1/8", "1/16", "1/16T", "1/32" };
static const float RATE_SPB[4]   = { 2.0f, 4.0f, 6.0f, 8.0f };
static const float RPT_SPB[16]   = { 0.5f, 0.6667f, 1.0f, 1.3333f, 1.5f, 2.0f, 2.6667f, 3.0f, 4.0f, 5.3333f, 6.0f, 8.0f, 12.0f, 16.0f, 24.0f, 32.0f };   /* note repeat rates: steps per beat, slow to fast */
static const char *KEY_NAME[12]  = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char *SCALE_NAME[3] = { "Minor", "Phryg", "Major" };
static const char *DLYT_NAME[4]  = { "1/8", "3/16", "1/4", "1/16" };
static const float DLYT_BEATS[4] = { 0.5f, 0.75f, 1.0f, 0.25f };
static const char *DT_NAME[5]  = { "1/16", "1/8", "3/16", "1/4", "1/2" };
static const float DT_BEATS[5] = { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
static const int   SCALE_TBL[3][6] = { { 0, 3, 7, 12, 15, 19 }, { 0, 1, 7, 12, 13, 19 }, { 0, 4, 7, 12, 16, 19 } };

/* ---------------- global parameters ---------------- */
enum { G_VOL, G_KEY, G_SCALE, G_DLYT, G_DLYFB, G_DLYRET, G_RVSIZE, G_RVDARK, G_RVRET, G_FILT, G_SAT, G_WID, G_COMP, G_HUM, G_MET, G_QNT, G_EQL, G_EQM, G_EQH, G_LIM, G_CHP, G_N };
static const pd_t GD[G_N] = {
    { "m_vol",    "Volume", 0, 0.80f },
    { "key",      "Key",    12, 5.0f },
    { "scale",    "Scale",  3, 0.0f },
    { "dly_time", "D Time", 4, 1.0f },
    { "dly_fb",   "D Fdbk", 0, 0.40f },
    { "dly_ret",  "D Ret",  0, 0.50f },
    { "rvb_size", "R Size", 0, 0.60f },
    { "rvb_dark", "R Dark", 0, 0.50f },
    { "rvb_ret",  "R Ret",  0, 0.50f },
    { "m_filt",   "Filter", 0, 0.50f },
    { "m_sat",    "Satur",  0, 0.00f },
    { "m_wid",    "Width",  0, 0.50f },
    { "m_comp",   "Comp",   0, 0.00f },
    { "m_hum",    "Human",  0, 0.00f },
    { "m_met",    "Metro",  0, 0.00f },
    { "m_qnt",    "Quant",  0, 0.50f },
    { "m_eql",    "EQ Low", 0, 0.50f },
    { "m_eqm",    "EQ Mid", 0, 0.50f },
    { "m_eqh",    "EQ High",0, 0.50f },
    { "m_lim",    "Limiter",0, 0.00f },
    { "m_chp",    "Comp HP",0, 0.00f },
};

/* ---------------- sound types ---------------- */
typedef struct {
    float f0;        /* nominal frequency (drums), 0 for melodic */
    float oct;       /* melodic: multiplier of the key root; 0 = drum */
    float tau1, tau2;/* amp / secondary decay (s) at Decay = 0.5 */
    float pet;       /* pitch-envelope time constant (s) */
    float trim;
    int   sus;       /* gate-sustained */
    float att;       /* attack time for sustained types (s) */
} ty_t;

static ty_t TY[T_COUNT] = {
    /* f0    oct   tau1   tau2    pet    trim   sus att */
    { 48,    0,    0.26f, 0.003f, 0.030f, 0.60f, 0, 0 },     /* Kick     */
    { 52,    0,    0.18f, 0.003f, 0.018f, 0.51f, 0, 0 },     /* KickHrd  */
    { 46,    0,    0.22f, 0.003f, 0.040f, 0.54f, 0, 0 },     /* KickGoa  */
    { 46,    0,    0.32f, 0.003f, 0.030f, 0.45f, 0, 0 },     /* RumbKick */
    { 42,    0,    0.35f, 0,      0.020f, 0.73f, 0, 0 },     /* Sub808   */
    { 0,     0,    0.07f, 0.11f,  0, 0.47f, 0, 0 },     /* Clap     */
    { 165,   0,    0.07f, 0.11f,  0.012f, 0.36f, 0, 0 },     /* Snare    */
    { 0,     0,    0.012f,0.006f, 0, 0.47f, 0, 0 },     /* Rim      */
    { 0,     0,    0.022f,0,      0, 1.36f, 0, 0 },     /* HatCl    */
    { 0,     0,    0.18f, 0,      0, 1.24f, 0, 0 },     /* HatOp    */
    { 0,     0,    0.05f, 0,      0, 0.28f, 0, 0 },     /* Shaker   */
    { 0,     0,    0.60f, 0,      0, 0.90f, 0, 0 },     /* Crash    */
    { 0,     0,    0.90f, 0.50f,  0, 0.70f, 0, 0 },     /* Ride     */
    { 110,   0,    0.22f, 0,      0.030f, 0.72f, 0, 0 },     /* Tom      */
    { 260,   0,    0.18f, 0,      0.030f, 0.75f, 0, 0 },     /* Conga    */
    { 0,     0,    0.12f, 0,      0, 0.55f, 0, 0 },     /* Cowbell  */
    { 520,   0,    0.10f, 0.06f,  0, 0.55f, 0, 0 },     /* PercFM   */
    { 180,   0,    0.20f, 0.06f,  0, 0.56f, 0, 0 },     /* Zap      */
    { 0,     0,    0,     0,      0, 0.20f, 0, 0 },     /* Riser    */
    { 0,     2.0f, 0.12f, 0,      0, 0.60f, 1, 0.003f },/* SubBass  */
    { 0,     2.0f, 0.13f, 0.08f,  0, 0.90f, 0, 0 },     /* RollBass */
    { 0,     2.0f, 0.28f, 0.12f,  0, 0.62f, 0, 0 },     /* Acid     */
    { 0,     2.0f, 0.30f, 0,      0, 0.25f, 1, 0.008f },/* Reese    */
    { 0,     8.0f, 0.20f, 0.20f,  0, 0.50f, 0, 0 },     /* Pluck    */
    { 0,     4.0f, 0.35f, 0.12f,  0, 0.34f, 0, 0 },     /* Stab     */
    { 0,     4.0f, 0.50f, 0,      0, 0.30f, 1, 0.120f },/* Pad      */
    { 0,     8.0f, 0.15f, 0,      0, 0.56f, 1, 0.003f },/* Lead     */
    { 0,     8.0f, 0.90f, 0.50f,  0, 0.55f, 0, 0 },     /* Bell     */
    { 0,     0,    0.60f, 0.50f,  0, 0.27f, 0, 0 },     /* Sweep    */
    { 45,    0,    1.20f, 0.25f,  0.030f, 0.33f, 0, 0 },     /* Impact   */
};

typedef struct { int base; float f0, oct, tau1, tau2, pet, trim; } var_t;
static const var_t VARI[43] = {
    { T_KICK, 40.0f, -1.0f, 0.4f, -1.0f, 0.045f, 1.0f },
    { T_KICKHRD, 58.0f, -1.0f, 0.12f, -1.0f, 0.012f, 1.0f },
    { T_KICKGOA, 50.0f, -1.0f, 0.16f, -1.0f, 0.05f, 1.0f },
    { T_RUMBKICK, 36.0f, -1.0f, 0.55f, -1.0f, 0.05f, 1.0f },
    { T_KICK, 55.0f, -1.0f, 0.22f, -1.0f, 0.02f, 1.0f },
    { T_SUB808, 33.0f, -1.0f, 0.7f, -1.0f, -1.0f, 1.0f },
    { T_SUB808, 58.0f, -1.0f, 0.28f, -1.0f, -1.0f, 1.0f },
    { T_SNARE, 200.0f, -1.0f, 0.045f, 0.07f, -1.0f, 1.0f },
    { T_SNARE, 135.0f, -1.0f, 0.14f, 0.16f, 0.02f, 1.0f },
    { T_CLAP, -1.0f, -1.0f, 0.12f, 0.22f, -1.0f, 1.0f },
    { T_CLAP, -1.0f, -1.0f, 0.04f, 0.06f, -1.0f, 1.0f },
    { T_HATCL, -1.0f, -1.0f, 0.01f, -1.0f, -1.0f, 1.0f },
    { T_HATOP, -1.0f, -1.0f, 0.4f, -1.0f, -1.0f, 1.0f },
    { T_SHAKER, -1.0f, -1.0f, 0.12f, -1.0f, -1.0f, 1.0f },
    { T_RIM, -1.0f, -1.0f, 0.006f, 0.003f, -1.0f, 1.1f },
    { T_TOM, 80.0f, -1.0f, 0.32f, -1.0f, 0.04f, 1.0f },
    { T_TOM, 190.0f, -1.0f, 0.16f, -1.0f, 0.025f, 1.0f },
    { T_CONGA, 360.0f, -1.0f, 0.12f, -1.0f, -1.0f, 1.0f },
    { T_CONGA, 200.0f, -1.0f, 0.24f, -1.0f, -1.0f, 1.0f },
    { T_PERC, 880.0f, -1.0f, 0.07f, 0.04f, -1.0f, 1.0f },
    { T_PERC, 300.0f, -1.0f, 0.18f, 0.1f, -1.0f, 1.0f },
    { T_ZAP, 400.0f, -1.0f, 0.14f, 0.05f, -1.0f, 1.0f },
    { T_ZAP, 90.0f, -1.0f, 0.3f, 0.09f, -1.0f, 1.0f },
    { T_ROLLBASS, -1.0f, 1.0f, 0.16f, -1.0f, -1.0f, 1.0f },
    { T_ROLLBASS, -1.0f, 4.0f, 0.11f, -1.0f, -1.0f, 1.0f },
    { T_ACID, -1.0f, 1.0f, 0.3f, -1.0f, -1.0f, 1.0f },
    { T_ACID, -1.0f, 4.0f, 0.22f, -1.0f, -1.0f, 1.0f },
    { T_REESE, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f },
    { T_REESE, -1.0f, 4.0f, -1.0f, -1.0f, -1.0f, 1.0f },
    { T_SUBBASS, -1.0f, 1.0f, 0.14f, -1.0f, -1.0f, 1.0f },
    { T_PLUCK, -1.0f, 4.0f, 0.26f, -1.0f, -1.0f, 1.0f },
    { T_PLUCK, -1.0f, 16.0f, 0.14f, -1.0f, -1.0f, 1.0f },
    { T_STAB, -1.0f, 2.0f, 0.4f, -1.0f, -1.0f, 1.0f },
    { T_STAB, -1.0f, 8.0f, 0.28f, -1.0f, -1.0f, 1.0f },
    { T_PAD, -1.0f, 2.0f, -1.0f, -1.0f, -1.0f, 1.0f },
    { T_PAD, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f },
    { T_LEAD, -1.0f, 4.0f, -1.0f, -1.0f, -1.0f, 1.0f },
    { T_LEAD, -1.0f, 16.0f, -1.0f, -1.0f, -1.0f, 0.9f },
    { T_BELL, -1.0f, 4.0f, 1.1f, -1.0f, -1.0f, 1.0f },
    { T_BELL, -1.0f, 16.0f, 0.7f, -1.0f, -1.0f, 0.9f },
    { T_SWEEP, -1.0f, -1.0f, 1.4f, -1.0f, -1.0f, 1.0f },
    { T_IMPACT, 33.0f, -1.0f, 2.0f, -1.0f, -1.0f, 1.0f },
    { T_RISER, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f },
};
/* synth family: default wave (1 saw, 2 square, 3 tri, 4 sine, 5 pulse), detune, sub level, attack */
typedef struct { int wave; float det, sub, att; } sy_t;
static const sy_t SYD[8] = {
    { 1, 0.006f, 0.30f, 0.004f }, { 1, 0.003f, 0.45f, 0.002f }, { 1, 0.018f, 0.15f, 0.010f }, { 4, 0.0f, 0.0f, 0.002f },
    { 4, 0.0f, 0.2f, 0.006f }, { 1, 0.012f, 0.0f, 0.280f }, { 4, 0.0f, 0.3f, 0.002f }, { 5, 0.030f, 0.2f, 0.012f },
};
/* variants: base, oct, tau1 (-1 keep), att (-1 keep), trim */
typedef struct { int base; float oct, tau1, att, trim; } sv_t;
static const sv_t SVAR[15] = {
    { T_SLEAD, -1, 0.35f, 0.040f, 1.0f }, { T_SLEAD, 16.0f, -1, -1, 0.9f }, { T_SBASS, 1.0f, -1, -1, 1.0f }, { T_SBASS, 4.0f, 0.10f, -1, 1.0f },
    { T_SSUPER, 8.0f, -1, -1, 0.9f }, { T_SSUPER, 2.0f, -1, -1, 1.0f }, { T_SEPIANO, 8.0f, 0.7f, -1, 0.9f }, { T_SEPIANO, 2.0f, 1.2f, -1, 1.0f },
    { T_SORGAN, 2.0f, -1, -1, 1.0f }, { T_SORGAN, 8.0f, -1, -1, 0.9f }, { T_SSTRINGS, 2.0f, -1, -1, 1.0f }, { T_SSTRINGS, -1, -1, 0.600f, 1.0f },
    { T_SFMBASS, 4.0f, 0.12f, -1, 0.9f }, { T_SHOOVER, 2.0f, -1, -1, 1.0f }, { T_SHOOVER, 8.0f, -1, -1, 0.9f },
};
/* generated variants: base, f0, oct, tau1, tau2, pet, trim, att (-1 keep), wave (0 keep), det, sub (-1 keep) */
typedef struct { int base; float f0, oct, tau1, tau2, pet, trim, att; int wave; float det, sub; } v2_t;
static const v2_t VAR2[N_V2] = {
#include "variants.inc"
};
static const v2_t VAR3[N_V3] = {
#include "variants3.inc"
};
enum { T_E_MODAL = 4000, T_E_FM, T_E_VOX, T_E_CHIP, T_E_MORPH };
typedef struct { int eng; float f0, oct, tau1, tau2, att, trim; int sus; float p[10]; } nx_t;
static const nx_t VAR4[N_V4] = {
#include "variants4.inc"
};
/* modal sets: 8 mode ratios + 8 amplitudes each (0 ratio = unused) */
#define N_MSET 19
static const float MD_R[N_MSET][8] = {
    { 1, 3.93f, 9.2f, 15.8f, 0, 0, 0, 0 },                  /* 0 marimba */
    { 1, 4.0f, 10.0f, 17.5f, 0, 0, 0, 0 },                  /* 1 vibes */
    { 1, 3.0f, 6.9f, 11.3f, 0, 0, 0, 0 },                   /* 2 xylophone */
    { 1, 5.4f, 12.1f, 0, 0, 0, 0, 0 },                      /* 3 kalimba */
    { 1, 2.76f, 5.4f, 8.93f, 13.3f, 18.6f, 0, 0 },          /* 4 glockenspiel */
    { 1, 2.0f, 3.0f, 4.1f, 5.2f, 6.4f, 7.7f, 9.0f },        /* 5 tubular bell */
    { 0.5f, 1, 1.19f, 1.5f, 2, 2.5f, 3, 4.1f },             /* 6 church bell */
    { 1, 2.0f, 3.01f, 4.03f, 5.2f, 6.1f, 0, 0 },            /* 7 steel drum */
    { 1, 2, 3, 4, 5, 6, 7, 8 },                              /* 8 harp */
    { 1, 2.001f, 3.005f, 4.01f, 5.02f, 6.03f, 7.05f, 8.06f },/* 9 nylon guitar */
    { 1, 2, 3, 4, 5, 6, 0, 0 },                              /* 10 koto */
    { 1, 1.5f, 1.99f, 2.44f, 2.89f, 3.34f, 0, 0 },          /* 11 timpani */
    { 1, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f, 0, 0 },         /* 12 membrane */
    { 1, 2.78f, 5.4f, 0, 0, 0, 0, 0 },                      /* 13 wood */
    { 1, 2.71f, 5.15f, 8.4f, 12.1f, 0, 0, 0 },              /* 14 singing bowl */
    { 1, 1.47f, 2.09f, 2.56f, 3.2f, 3.94f, 4.8f, 5.6f },    /* 15 gamelan */
    { 1, 2.32f, 4.25f, 6.63f, 9.38f, 0, 0, 0 },             /* 16 crystal */
    { 1, 1.62f, 2.44f, 3.18f, 3.97f, 5.02f, 6.1f, 7.4f },   /* 17 metal plate */
    { 1, 3, 5, 7, 9, 0, 0, 0 },                              /* 18 hollow / pipe */
};
static const float MD_A[N_MSET][8] = {
    { 1, .45f, .2f, .08f, 0, 0, 0, 0 }, { 1, .35f, .15f, .06f, 0, 0, 0, 0 }, { 1, .6f, .35f, .18f, 0, 0, 0, 0 }, { 1, .5f, .2f, 0, 0, 0, 0, 0 },
    { 1, .7f, .5f, .35f, .2f, .1f, 0, 0 }, { 1, .8f, .7f, .5f, .4f, .3f, .2f, .15f }, { .8f, 1, .9f, .7f, .6f, .4f, .3f, .2f }, { 1, .9f, .6f, .4f, .3f, .2f, 0, 0 },
    { 1, .87f, .76f, .66f, .58f, .5f, .43f, .38f }, { 1, .8f, .5f, .4f, .25f, .2f, .1f, .08f }, { 1, .3f, .6f, .2f, .4f, .1f, 0, 0 }, { 1, .7f, .6f, .45f, .35f, .25f, 0, 0 },
    { 1, .8f, .6f, .5f, .4f, .3f, 0, 0 }, { 1, .5f, .25f, 0, 0, 0, 0, 0 }, { 1, .8f, .5f, .3f, .2f, 0, 0, 0 }, { 1, .7f, .6f, .5f, .4f, .3f, .2f, .1f },
    { 1, .6f, .4f, .3f, .2f, 0, 0, 0 }, { 1, .9f, .8f, .7f, .6f, .5f, .4f, .3f }, { 1, .33f, .2f, .14f, .11f, 0, 0, 0 },
};
static const float VW_F[7][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 }, { 450, 800, 2830 }, { 325, 700, 2700 }, { 660, 1720, 2410 }, { 500, 1400, 2500 } };
static const int CH_ARP[7][4] = { { 0, 0, 0, 0 }, { 0, 4, 7, 12 }, { 0, 3, 7, 12 }, { 0, 12, 0, 12 }, { 0, 7, 12, 7 }, { 0, 4, 7, 4 }, { 0, 7, 0, 7 } };
static const int CH_ARPN[7] = { 1, 4, 4, 4, 4, 4, 4 };
static sy_t SYT[T_COUNT];
static int BASE_T[T_COUNT];
/* per-preset CHARACTER: every sound type carries its own filter / drive / crush / ring / tone offsets on top of the pad's knobs, so presets of one family still sound different.
   Generated at start from a seeded random draw per class of sound; src/char_gain.inc (measured) levels them to the plain sound. */
typedef struct { float cm, res, fenv, drive, crush, ring, lo, tm, gn; int wv; float pf, pg, pq; } ch_t;
static ch_t CHR[T_COUNT];
#ifndef CHAR_NEUTRAL
static const float CHGAIN[T_COUNT] = {
#include "char_gain.inc"
};
#endif
static uint32_t ch_rng(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }
static float ch_f(uint32_t *s) { return (float)(ch_rng(s) & 0xFFFFFF) / 16777216.0f; }
static float ch_r(uint32_t *s, float lo, float hi) { return lo + (hi - lo) * ch_f(s); }
static float ch_p(uint32_t *s, float p, float lo, float hi) { return ch_f(s) < p ? ch_r(s, lo, hi) : 0.0f; }   /* with probability p a value in lo..hi, else 0 */
static void init_chars(void);

static void init_types(void) {
    for (int i = 0; i < T_V0; i++) BASE_T[i] = i;
    { ty_t t = { 0, 0, 0, 0, 0, 0.9f, 0, 0 }; TY[T_SAMPLE] = t; BASE_T[T_SAMPLE] = T_SAMPLE; }
    for (int i = 0; i < 43; i++) {
        const var_t *w = &VARI[i]; ty_t t = TY[w->base]; int ti = T_V0 + i;
        if (w->f0 >= 0) t.f0 = w->f0;
        if (w->oct >= 0) t.oct = w->oct;
        if (w->tau1 >= 0) t.tau1 = w->tau1;
        if (w->tau2 >= 0) t.tau2 = w->tau2;
        if (w->pet >= 0) t.pet = w->pet;
        t.trim *= w->trim; if (ti == T_V0 + 33) t.att = 0.35f;
        TY[ti] = t; BASE_T[ti] = w->base;
    }
    /* synth bases: oct tau1 tau2 pet trim sus att */
    static const ty_t SB[8] = {
        { 0, 8.0f, 0.15f, 0.0f,  0.0f,  0.50f, 1, 0.004f },   /* SynLead  */
        { 0, 2.0f, 0.22f, 0.10f, 0.0f,  0.70f, 0, 0.0f },     /* SynBass  */
        { 0, 4.0f, 0.20f, 0.0f,  0.0f,  0.42f, 1, 0.010f },   /* SuperSaw */
        { 0, 4.0f, 0.90f, 0.35f, 0.0f,  0.60f, 0, 0.0f },     /* EPiano   */
        { 0, 4.0f, 0.12f, 0.0f,  0.0f,  0.42f, 1, 0.006f },   /* Organ    */
        { 0, 4.0f, 0.60f, 0.0f,  0.0f,  0.40f, 1, 0.280f },   /* Strings  */
        { 0, 2.0f, 0.30f, 0.15f, 0.0f,  0.70f, 0, 0.0f },     /* FMBass   */
        { 0, 4.0f, 0.25f, 0.0f,  0.08f, 0.28f, 1, 0.012f },   /* Hoover   */
    };
    for (int i = 0; i < 8; i++) { TY[T_S0 + i] = SB[i]; BASE_T[T_S0 + i] = T_S0 + i; SYT[T_S0 + i] = SYD[i]; }
    for (int i = 0; i < 15; i++) {
        const sv_t *w = &SVAR[i]; ty_t t = TY[w->base]; int ti = T_S1 + i;
        if (w->oct > 0) t.oct = w->oct;
        if (w->tau1 > 0) t.tau1 = w->tau1;
        if (w->att >= 0) t.att = w->att;
        t.trim *= w->trim;
        TY[ti] = t; BASE_T[ti] = w->base; SYT[ti] = SYT[w->base];
    }
    for (int i = 0; i < N_V2; i++) {
        const v2_t *w = &VAR2[i]; ty_t t = TY[w->base]; int ti = T_V2 + i;
        if (w->f0 >= 0) t.f0 = w->f0;
        if (w->oct >= 0) t.oct = w->oct;
        if (w->tau1 >= 0) t.tau1 = w->tau1;
        if (w->tau2 >= 0) t.tau2 = w->tau2;
        if (w->pet >= 0) t.pet = w->pet;
        if (w->att >= 0) t.att = w->att;
        t.trim *= w->trim;
        TY[ti] = t; BASE_T[ti] = w->base; SYT[ti] = SYT[w->base];
        if (w->wave > 0) SYT[ti].wave = w->wave;
        if (w->det >= 0) SYT[ti].det = w->det;
        if (w->sub >= 0) SYT[ti].sub = w->sub;
    }
    for (int i = 0; i < N_V3; i++) {
        const v2_t *w = &VAR3[i]; ty_t t = TY[w->base]; int ti = T_V3 + i;
        if (w->f0 >= 0) t.f0 = w->f0;
        if (w->oct >= 0) t.oct = w->oct;
        if (w->tau1 >= 0) t.tau1 = w->tau1;
        if (w->tau2 >= 0) t.tau2 = w->tau2;
        if (w->pet >= 0) t.pet = w->pet;
        if (w->att >= 0) t.att = w->att;
        t.trim *= w->trim;
        TY[ti] = t; BASE_T[ti] = w->base; SYT[ti] = SYT[w->base];
        if (w->wave > 0) SYT[ti].wave = w->wave;
        if (w->det >= 0) SYT[ti].det = w->det;
        if (w->sub >= 0) SYT[ti].sub = w->sub;
    }
    for (int i = 0; i < N_V4; i++) {                       /* NEW ENGINES */
        const nx_t *w = &VAR4[i]; const int ti = T_V4 + i;
        ty_t t = { w->f0, w->oct, w->tau1, w->tau2, 0.0f, w->trim, w->sus, w->att };
        TY[ti] = t; BASE_T[ti] = T_E_MODAL + w->eng; memset(&SYT[ti], 0, sizeof SYT[ti]);
    }
}

/* ---------------- sample bank: WAV files from /data/UserData/schwung/taktile-samples ---------------- */
#define NSMP 64
#define SMP_MAX_BYTES (40u * 1024u * 1024u)
typedef struct { char name[40]; int16_t *d; int n, ch; float sr; } smp_t;
static smp_t SMP[NSMP];
static unsigned smp_bytes;
static char SMP_DIR[256], SMP_IDX[300];
static const smp_t *smp_get(int i) {
    if (i < 0 || i >= NSMP) return NULL;
    const smp_t *m = &SMP[i];
    int16_t *d = __atomic_load_n(&m->d, __ATOMIC_ACQUIRE);
    return (d && m->n > 8) ? m : NULL;
}
static int rd_u16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd_u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int smp_load(smp_t *m, const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len < 44 || len > 80L * 1024 * 1024) { fclose(f); return 0; }
    uint8_t *b = (uint8_t *)malloc((size_t)len);
    if (!b) { fclose(f); return 0; }
    if (fread(b, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(b); return 0; }
    fclose(f);
    int ok = 0;
    if (memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WAVE", 4) == 0) {
        long pos = 12; int fmt = 0, ch = 0, bits = 0; uint32_t sr = 0; const uint8_t *data = NULL; long dlen = 0;
        while (pos + 8 <= len) {
            uint32_t cl = rd_u32(b + pos + 4); const uint8_t *c = b + pos + 8;
            if (memcmp(b + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= len) {
                fmt = rd_u16(c); ch = rd_u16(c + 2); sr = rd_u32(c + 4); bits = rd_u16(c + 14);
                if (fmt == 0xFFFE && cl >= 26 && pos + 8 + 26 <= len) fmt = rd_u16(c + 24);
            } else if (memcmp(b + pos, "data", 4) == 0) {
                data = c; dlen = (long)cl; if (pos + 8 + dlen > len) dlen = len - pos - 8; break;
            }
            pos += 8 + (long)cl + (long)(cl & 1);
        }
        int bps = bits / 8;
        if (data && ch >= 1 && sr >= 8000 && sr <= 192000 && ((fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32))) {
            long frames = dlen / (long)(bps * ch);
            int oc = ch >= 2 ? 2 : 1;
            long room = (long)((SMP_MAX_BYTES - smp_bytes) / (unsigned)(2 * oc));
            if (frames > room) frames = room;
            if (frames > 8) {
                int16_t *o = (int16_t *)malloc((size_t)frames * (size_t)oc * 2);
                if (o) {
                    for (long i = 0; i < frames; i++) for (int k = 0; k < oc; k++) {
                        const uint8_t *q = data + (i * ch + k) * bps; float v;
                        if (fmt == 3) { float fv; memcpy(&fv, q, 4); v = fv * 32767.0f; }
                        else if (bits == 8) v = ((float)q[0] - 128.0f) * 256.0f;
                        else if (bits == 16) v = (float)(int16_t)(q[0] | (q[1] << 8));
                        else if (bits == 24) v = (float)((int32_t)((uint32_t)q[0] << 8 | (uint32_t)q[1] << 16 | (uint32_t)q[2] << 24) >> 16);
                        else v = (float)((int32_t)rd_u32(q) >> 16);
                        if (v > 32767.0f) v = 32767.0f; if (v < -32768.0f) v = -32768.0f;
                        o[i * oc + k] = (int16_t)v;
                    }
                    m->n = (int)frames; m->ch = oc; m->sr = (float)sr;
                    smp_bytes += (unsigned)(frames * oc * 2);
                    __atomic_store_n(&m->d, o, __ATOMIC_RELEASE);
                    ok = 1;
                }
            }
        }
    }
    free(b);
    return ok;
}
static int smp_cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void smp_scan(void) {                         /* slots stay stable: names are remembered in taktile-samples.idx */
    char names[NSMP][40]; int nn = 0, changed = 0;
    for (int i = 0; i < NSMP; i++) names[i][0] = 0;
    FILE *f = fopen(SMP_IDX, "r");
    if (f) { char ln[128]; while (nn < NSMP && fgets(ln, sizeof ln, f)) { size_t l = strlen(ln); while (l && (ln[l - 1] == '\n' || ln[l - 1] == '\r')) ln[--l] = 0; if (l && l < 40) { strcpy(names[nn], ln); } nn++; } fclose(f); }
    char found[NSMP][40]; int nf = 0;
    DIR *d = opendir(SMP_DIR);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && nf < NSMP) {
            size_t l = strlen(e->d_name);
            if (l < 5 || l >= 40 || e->d_name[0] == '.') continue;
            if (strcmp(e->d_name + l - 4, ".wav") && strcmp(e->d_name + l - 4, ".WAV")) continue;
            strcpy(found[nf++], e->d_name);
        }
        closedir(d);
    }
    qsort(found, (size_t)nf, 40, smp_cmp);
    for (int i = 0; i < nf; i++) {
        int have = 0; for (int k = 0; k < NSMP; k++) if (names[k][0] && !strcmp(names[k], found[i])) have = 1;
        if (have) continue;
        for (int k = 0; k < NSMP; k++) if (!names[k][0]) { strcpy(names[k], found[i]); changed = 1; break; }
    }
    for (int k = 0; k < NSMP; k++) {
        if (!names[k][0]) continue;
        strcpy(SMP[k].name, names[k]);
        if (SMP[k].d) continue;
        int onDisk = 0; for (int i = 0; i < nf; i++) if (!strcmp(found[i], names[k])) onDisk = 1;
        if (!onDisk) continue;
        char path[400]; snprintf(path, sizeof path, "%s/%s", SMP_DIR, names[k]);
        smp_load(&SMP[k], path);
    }
    if (changed) {
        f = fopen(SMP_IDX, "w");
        if (f) { int last = -1; for (int k = 0; k < NSMP; k++) if (names[k][0]) last = k; for (int k = 0; k <= last; k++) fprintf(f, "%s\n", names[k]); fclose(f); }
    }
}
/* ---- file browser: any folder of the Move; a chosen WAV is copied into the sample bank ---- */
#define NBR 480
#define NBR_DIRS 200
typedef struct { char name[96]; int dir; } br_t;
static br_t BR[NBR]; static int br_n, br_err, br_skip, br_trunc; static char BR_DIR[256] = "/"; static int br_res = -1;
static int br_cmp(const void *a, const void *b) {
    const br_t *x = a, *y = b; if (x->dir != y->dir) return y->dir - x->dir; return strcmp(x->name, y->name);
}
static int br_is_wav(const char *nm, size_t n) {
    if (n < 5 || nm[n - 4] != '.') return 0;
    const char a = (char)(nm[n - 3] | 0x20), b = (char)(nm[n - 2] | 0x20), c = (char)(nm[n - 1] | 0x20);
    return a == 'w' && b == 'a' && c == 'v';
}
static void br_list(const char *path) {                  /* folders first (up to 200), then WAV files; nothing is dropped silently: errors and skipped entries are counted */
    snprintf(BR_DIR, sizeof BR_DIR, "%s", (path && path[0] == '/') ? path : "/");
    size_t l = strlen(BR_DIR); while (l > 1 && BR_DIR[l - 1] == '/') BR_DIR[--l] = 0;
    br_n = 0; br_err = 0; br_skip = 0; br_trunc = 0;
    DIR *d = opendir(BR_DIR);
    if (!d) { br_err = errno ? errno : 1; return; }
    for (int pass = 0; pass < 2; pass++) {
        rewinddir(d);
        struct dirent *e;
        while ((e = readdir(d))) {
            size_t n = strlen(e->d_name);
            if (e->d_name[0] == '.') continue;
            if (n >= sizeof BR[0].name) { if (pass == 0) br_skip++; continue; }
            char full[512]; snprintf(full, sizeof full, "%s/%s", strcmp(BR_DIR, "/") ? BR_DIR : "", e->d_name);
            struct stat st; if (stat(full, &st)) { if (pass == 0) br_skip++; continue; }
            const int isd = S_ISDIR(st.st_mode);
            if (isd != (pass == 0)) continue;
            if (!isd && !br_is_wav(e->d_name, n)) continue;
            if (br_n >= NBR || (isd && br_n >= NBR_DIRS)) { br_trunc++; continue; }
            strcpy(BR[br_n].name, e->d_name); BR[br_n].dir = isd; br_n++;
        }
    }
    closedir(d);
    qsort(BR, (size_t)br_n, sizeof(br_t), br_cmp);
}
static smp_t PVW; static int16_t *pv_old; static volatile int pv_on; static double pv_pos;
static int br_preview(int i) {                  /* audition a WAV from the browser (max 12 MB, first 20 s) without touching the sample bank */
    pv_on = 0;
    if (i < 0 || i >= br_n || BR[i].dir) return 0;
    char src[400]; snprintf(src, sizeof src, "%s/%s", strcmp(BR_DIR, "/") ? BR_DIR : "", BR[i].name);
    struct stat st; if (stat(src, &st) || st.st_size > 12L * 1024 * 1024) return 0;
    int16_t *prev = __atomic_exchange_n(&PVW.d, NULL, __ATOMIC_ACQ_REL);
    if (pv_old) { free(pv_old); pv_old = NULL; }
    if (prev) { pv_old = prev; unsigned b = (unsigned)(PVW.n * PVW.ch * 2); smp_bytes = smp_bytes > b ? smp_bytes - b : 0; }
    PVW.n = 0; PVW.ch = 1; PVW.sr = 44100.0f;
    if (!smp_load(&PVW, src)) return 0;
    smp_bytes -= (unsigned)(PVW.n * PVW.ch * 2);
    if (PVW.n > (int)(PVW.sr * 20.0f)) PVW.n = (int)(PVW.sr * 20.0f);
    pv_pos = 0.0; pv_on = 1;
    return 1;
}
static void br_import(int i) {
    br_res = -1;
    if (i < 0 || i >= br_n || BR[i].dir) return;
    char src[400]; snprintf(src, sizeof src, "%s/%s", strcmp(BR_DIR, "/") ? BR_DIR : "", BR[i].name);
    char nm[40]; size_t n = strlen(BR[i].name), base = n - 4; if (base > 35) base = 35;
    memcpy(nm, BR[i].name, base); nm[base] = 0; strcat(nm, ".wav");
    for (char *p = nm; *p; p++) if (*p == '/' || *p == '\\') *p = '_';
    char dst[400]; snprintf(dst, sizeof dst, "%s/%s", SMP_DIR, nm);
    struct stat st;
    if (stat(dst, &st)) {                       /* not in the bank yet: copy (max 40 MB) */
        if (stat(src, &st) || st.st_size > (off_t)SMP_MAX_BYTES) return;
        FILE *a = fopen(src, "rb"); if (!a) return;
        FILE *b = fopen(dst, "wb"); if (!b) { fclose(a); return; }
        char buf[8192]; size_t r; int ok = 1;
        while ((r = fread(buf, 1, sizeof buf, a)) > 0) if (fwrite(buf, 1, r, b) != r) { ok = 0; break; }
        fclose(a); if (fclose(b)) ok = 0;
        if (!ok) { remove(dst); return; }
    }
    smp_scan();
    for (int k = 0; k < NSMP; k++) if (!strcmp(SMP[k].name, nm) && smp_get(k)) { br_res = k; break; }
}
static void smp_init(void) {
    const char *e = getenv("TAKTILE_SMP_DIR");
    snprintf(SMP_DIR, sizeof SMP_DIR, "%s", e ? e : "/data/UserData/schwung/taktile-samples");
    snprintf(SMP_IDX, sizeof SMP_IDX, "%s.idx", SMP_DIR);
    mkdir(SMP_DIR, 0777);
    smp_scan();
}

static const float HAT_FREQ[6] = { 554.3f, 821.9f, 997.9f, 1411.3f, 1458.0f, 2160.0f };

/* ---------------- instance ---------------- */
typedef struct {
    int   active, gate, type;
    float t, vel, env, env2, pe, trel;
    float ph, ph2, ph3, ph6[6];
    float z1, z2, z3, z4;
    float fm, f0, c1, c2, pek, third;
    float l1, l2;                  /* tone filter state */
    float cr_hold, cr_cnt, hp_z, ring_ph, svf_lp, svf_bp, fenv, fkoff, pk_lp, pk_bp;   /* crush + low-cut state */
    int   fol, fgen;               /* ghost layer of a MIDI effect: follows the owner's note (fgen = trigger generation) */
    float nx[48]; int ni[2];                                           /* NEW ENGINES: modal / FM / vocal / chip / morph state */
    int   sw, gk; float sdet, ssub, satt;                              /* synth family options */
    double sp; double ga, g0[2]; float gp[2]; int stch; float stg; int s0, s1, sidx, smode; float srate, sgain, satk_inc, srel_c, sgate_env;                   /* sample playback */
} vc_t;

typedef struct {
    int   on, held, n, base, key;     /* base = note offset of a held note pad, key = which pad runs the arp (-1 none) */
    float cnt, vel, life;
} ar_t;

typedef struct {
    float pp[NLANE][PP_N];
    float fdk[NLANE];
    float g[G_N];
    int   focused;                  /* 1..32 */

    float sr, inv_sr;
    float root_hz;
    int mo_clk, mo_trk, mo_chn, mo_xp, mo_vp, mo_dn; float mclk; struct { int cnt; uint8_t st, d1; } mo_q[64];   /* MIDI out: clock + notes per track */
    int16_t *rec_buf; long rec_n, rec_len, rec_max; int rec_state, rec_bars, rec_src; char rec_last[48]; int rec_slot;   /* resampling */
    float sc_w[128], sc_lp[8], sc_env[8]; int sc_i, sc_c;            /* master scope ring (decimated) and 8 band levels for the UI scope page */
    float dk_env[NTRK], dk_c[NTRK], meter[NTRK];                 /* sidechain ducking (kick -> other tracks) and level meters */
    float semi[NLANE], dm[NLANE], a_tone[NLANE], dg[NLANE], dnorm[NLANE], gain[NLANE], pl[NLANE], pr[NLANE];
    int   isdrum[NTRK], dtrk[4]; float tg[NTRK];       /* drum tracks: dtrk[slot] = track, tg = track gain */
    float a_clap_hp, a_clap_lp, a_snr_hp, a_rim_hp, a_hat_hp, a_shk_hp, a_shk_lp, a_crash_hp;
    float a_reese, a_pad, a_lead, a_dly;

    vc_t vc[NLANE + NGH];
    int  vown[NLANE + NGH], gnext;
    ar_t ar[NLANE];
    uint32_t rng;

    /* focus pairing (pad note + finger vouch) */
    uint32_t clock;
    int   pend_pad;  uint32_t pend_note_t;
    uint32_t pend_vouch_t; int has_vouch;

    /* effects */
    /* per-track effects: delay + reverb (sends), chorus + phaser (inserts) */
    float dlL[NLANE][DLY_SIZE], dlR[NLANE][DLY_SIZE];
    int   dw[NLANE];
    float dly_z[NLANE][2];
    float rv[NLANE][NLINES][RV_SIZE];
    float rv_z[NLANE][NLINES];
    int   rw[NLANE];
    int   rv_len[NLANE][NLINES];
    float rv_g[NLANE][NLINES], rv_a[NLANE];
    float chb[NLANE][CH_SIZE]; int chw[NLANE]; float ch_ph[NLANE];
    float ph_z[NLANE][2][4], ph_fb[NLANE][2], ph_ph[NLANE];
    int   fx_act[NLANE], dr_act[NLANE];
    int   tgen[NLANE];                         /* trigger counter per lane (MIDI effect layers follow it) */
    struct { int cnt, lane, semi, follow; float vel; } eq[32];   /* delayed notes: echo + strum */
    /* per-track insert effect (10 types): lfo phase, filter states, delay buffers, sample-hold */
    float xph[NLANE], xz[NLANE][2][4], xbuf[NLANE][2][XB_SIZE], xhold[NLANE][2]; int xw[NLANE], xcnt[NLANE];
    float sbuf[NLANE][2][SB_SIZE]; int scnt[NLANE], sn[NLANE];

    /* sequencer */
    uint8_t stp[NPAT][NLANE][MAXSTEP][6];   /* a = on<<7 | vel, b = probability 0..7, c = semi + 64, d = ratchet | gate<<2 | cond<<5, e = nudge 0..7, f = parameter lock value 0 (none) .. 127 */
    uint8_t plk_t[NLANE]; float plk_b[NLANE]; float cdet_z[2];   /* per-lane lock target (PP index, 0 = none), its base value, compressor detector HPF state */
    int   seqlen[NTRK], mute[NTRK];
    uint8_t lmute[NLANE];            /* per-sound mute (drum pads): the sequencer skips these lanes */
    int   llen[NLANE];                      /* own loop length of one lane (0 = follow the track) */
    int   playing, cur_pat, sel_track, gs;
    int   clip[NTRK], qclip[NTRK];   /* active clip per track (-1 = stopped), pending (-2 = none) */
    float bpm, swing;
    float pump_ph, wob_ph;
    float mf_z[2][2], mf_a, comp_env; int mf_mode;
    float eq_c[NLANE][3][5], eq_z[NLANE][3][2][2], eq_g[NLANE][3]; int eq_on[NLANE];       /* per-sound 3-band EQ (low shelf, peak, high shelf) */
    float rm_buf[4][4096], rm_lp, rm_env, rm_hp; int rm_w, rm_act;       /* kick rumble bus: dark comb reverb, ducked by the kicks */
    float eq_lo[2], eq_hi[2], lim_g;                              /* master EQ band splitters + limiter gain */
    float met_env, met_ph, met_f;
    int pads_off, pad_oct, lay_s; float cpu_avg, cpu_peak;
    double tsamp, next_time, last_time;
    int   gate_left[NLANE];
    int   rt_n[NLANE], rt_semi[NLANE], rt_gate[NLANE]; float rt_cnt[NLANE], rt_int[NLANE], rt_vel[NLANE];   /* ratchet / nudge scheduler */
    float cpp[NPAT][NLANE][PP_N]; uint8_t cpp_set[NPAT][NLANE];   /* sound of every clip */
    int   np_trk[32];                      /* track sounding on each note pad */
    int   rep_idx, rep_on, rep_t, rep_k; float rep_semi, rep_vel, rep_cnt;   /* note repeat */

    int   in_use;
} inst_t;

static inst_t g_pool[MAX_INST];
static int    g_sr = 44100;
static const host_api_v1_t *g_host = NULL;
static double (*g_get_beat)(void) = NULL;
static float  (*g_get_bpm)(void)  = NULL;

/* ---------------- helpers ---------------- */
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float fast_tanh(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float softclip(float x) {
    float a = fabsf(x);
    if (a < 0.7f) return x;
    float s = x < 0.0f ? -1.0f : 1.0f;
    return s * (0.7f + 0.3f * fast_tanh((a - 0.7f) / 0.3f * 1.0f));
}
static inline float onepole_a(float fc, float sr) { return 1.0f - expf(-TWO_PI * fc / sr); }
static inline float noise(inst_t *s) {
    s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
    return ((float)(s->rng & 0xFFFF) / 32768.0f) - 1.0f;
}
static inline int is_melodic(int ty) { return TY[ty].oct > 0.0f; }

__attribute__((unused)) static float bpm_now(void) {
    float bpm = g_get_bpm ? g_get_bpm() : 120.0f;
    if (!(bpm > 20.0f && bpm < 400.0f)) bpm = 120.0f;
    return bpm;
}

/* ---------------- derived values ---------------- */
static void update_pad(inst_t *s, int p) {
    const float *q = s->pp[p];
    int ty = (int)(q[PP_TYPE] + 0.5f);
    if (ty < 0) ty = 0;
    if (ty >= T_COUNT) ty = T_COUNT - 1;
    float semis = (q[PP_TUNE] - 0.5f) * 24.0f;
    if (is_melodic(ty)) semis = roundf(semis);
    s->semi[p] = semis;
    s->dm[p] = powf(3.0f, (q[PP_DECAY] - 0.5f) * 2.0f);
    s->a_tone[p] = q[PP_TONE] < 0.99f ? onepole_a(150.0f * powf(110.0f, q[PP_TONE]), s->sr) : 1.0f;
    s->fdk[p] = 1.5e-4f * exp2f((0.5f - q[PP_FDEC]) * 6.0f);
    float dg = 1.0f + q[PP_DRIVE] * 10.0f;
    s->dg[p] = dg;
    s->dnorm[p] = q[PP_DRIVE] > 0.01f ? 0.5f / fast_tanh(0.5f * dg) : 1.0f;
    s->gain[p] = q[PP_VOL] * q[PP_VOL] / 0.64f * TY[ty].trim;
    float a = q[PP_PAN] * 1.57079633f;
    s->pl[p] = cosf(a) * 1.41421356f;
    s->pr[p] = sinf(a) * 1.41421356f;
    if (p < NTRK) {
        float dtau = 0.03f + q[PP_DUCKR] * q[PP_DUCKR] * 0.55f;
        s->dk_c[p] = expf(-1.0f / (dtau * s->sr));
    }
    {                                                          /* this sound's / track's own reverb */
        float rt60 = 0.5f * powf(12.0f, q[PP_RSIZE]);
        static const int BASE[NLINES] = { 1033, 1327, 1759, 2143 };
        for (int i = 0; i < NLINES; i++) {
            int d = (int)((float)BASE[i] * s->sr / 44100.0f + 0.5f);
            if (d > RV_SIZE - 2) d = RV_SIZE - 2;
            if (d < 8) d = 8;
            s->rv_len[p][i] = d;
            s->rv_g[p][i] = powf(10.0f, -3.0f * (float)d / (s->sr * rt60));
        }
        s->rv_a[p] = onepole_a(6000.0f * powf(0.05f, q[PP_RDARK]), s->sr);
    }
}

static void update_global(inst_t *s) {
    const float sr = s->sr;
    int key = (int)(s->g[G_KEY] + 0.5f);
    if (key < 0) key = 0; if (key > 11) key = 11;
    {
        float fl = s->g[G_FILT];
        if (fl < 0.48f) { s->mf_mode = 1; s->mf_a = onepole_a(200.0f * powf(100.0f, fl * 2.0f), sr); }
        else if (fl > 0.52f) { s->mf_mode = 2; s->mf_a = onepole_a(20.0f * powf(150.0f, (fl - 0.5f) * 2.0f), sr); }
        else s->mf_mode = 0;
    }
    s->root_hz = 32.7032f * powf(2.0f, (float)key / 12.0f);
}

static void init_static(inst_t *s) {
    const float sr = s->sr;
    s->a_clap_hp  = onepole_a(700.0f, sr);
    s->a_clap_lp  = onepole_a(3500.0f, sr);
    s->a_snr_hp   = onepole_a(1800.0f, sr);
    s->a_rim_hp   = onepole_a(3000.0f, sr);
    s->a_hat_hp   = onepole_a(7000.0f, sr);
    s->a_shk_hp   = onepole_a(3500.0f, sr);
    s->a_shk_lp   = onepole_a(9000.0f, sr);
    s->a_crash_hp = onepole_a(5000.0f, sr);
    s->a_reese    = onepole_a(900.0f, sr);
    s->a_pad      = onepole_a(1800.0f, sr);
    s->a_lead     = onepole_a(4000.0f, sr);
    s->a_dly      = onepole_a(5000.0f, sr);
}

/* default layout: type, tune, decay, arp, rate, dly, rvb */
typedef struct { int type; float tune, decay; int arp, rate; float dly, rvb; } lay_t;
static const lay_t LAYOUT[NPADS] = {
    /* tracks 1-16 */
    { T_KICK,0.5f,0.5f,0,1,0,0 },        { T_RUMBKICK,0.5f,0.5f,0,1,0,0.9f }, { T_CLAP,0.5f,0.5f,0,1,0.1f,0.15f }, { T_SNARE,0.5f,0.5f,0,1,0,0.12f },
    { T_HATCL,0.5f,0.5f,0,1,0,0 },       { T_HATOP,0.5f,0.5f,0,1,0,0 },       { T_RIDE,0.5f,0.5f,0,1,0,0 },        { T_RIM,0.5f,0.5f,0,1,0,0 },
    { T_TOM,0.5f,0.5f,0,1,0,0 },         { T_PERC,0.5f,0.5f,0,1,0.15f,0 },    { T_SUBBASS,0.5f,0.5f,0,1,0,0 },     { T_ROLLBASS,0.5f,0.5f,0,1,0,0 },
    { T_ACID,0.5f,0.5f,0,1,0.2f,0 },     { T_PLUCK,0.5f,0.5f,0,1,0.35f,0.2f },{ T_STAB,0.5f,0.5f,0,1,0.25f,0.25f }, { T_PAD,0.5f,0.5f,0,1,0,0.5f },
    /* drum kit (lanes 16-79: each drum track owns 16 of these) */
    { T_KICK,0.5f,0.5f,0,1,0,0 },        { T_KICKHRD,0.5f,0.5f,0,1,0,0 },     { T_SNARE,0.5f,0.5f,0,1,0,0.12f },  { T_CLAP,0.5f,0.5f,0,1,0.1f,0.15f },
    { T_RIM,0.5f,0.5f,0,1,0,0 },         { T_HATCL,0.5f,0.5f,0,1,0,0 },       { T_HATOP,0.5f,0.5f,0,1,0,0 },       { T_RIDE,0.5f,0.5f,0,1,0,0 },
    { T_CRASH,0.5f,0.5f,0,1,0,0.2f },    { T_TOM,0.35f,0.5f,0,1,0,0 },        { T_TOM,0.7f,0.5f,0,1,0,0 },         { T_CONGA,0.5f,0.5f,0,1,0,0 },
    { T_COWBELL,0.5f,0.5f,0,1,0,0 },     { T_SHAKER,0.5f,0.5f,0,1,0,0 },      { T_PERC,0.5f,0.5f,0,1,0.15f,0 },    { T_ZAP,0.5f,0.5f,0,1,0.3f,0 },
};

static void reset_pad(inst_t *s, int p) {
    for (int k = 0; k < PP_N; k++) s->pp[p][k] = PD[k].def;
    const lay_t *l = &LAYOUT[p < NTRK ? p : NTRK + ((p - NTRK) & 15)];
    s->pp[p][PP_TYPE] = (float)l->type;
    s->pp[p][PP_TUNE] = l->tune;
    s->pp[p][PP_DECAY] = l->decay;
    s->pp[p][PP_ARP] = (float)l->arp;
    s->pp[p][PP_RATE] = (float)l->rate;
    s->pp[p][PP_DLY] = l->dly;
    s->pp[p][PP_RVB] = l->rvb;
    update_pad(s, p);
}

/* lanes: 0-15 are the 16 tracks, 16-79 are four drum kits (16 sounds each); a drum track owns one kit */
static inline int lane_trk(const inst_t *s, int l) {
    if (l < NTRK) return l;
    int t = s->dtrk[(l - NTRK) >> 4];
    return t < 0 ? 0 : t;
}
static inline int lane_on(const inst_t *s, int l) {
    if (l < NTRK) return !s->isdrum[l];
    return s->dtrk[(l - NTRK) >> 4] >= 0;
}
static int track_slot(const inst_t *s, int t) { for (int d = 0; d < 4; d++) if (s->dtrk[d] == t) return d; return -1; }

/* ---------------- voices ---------------- */
/* ---------------- NEW ENGINES: voice start ---------------- */
static void nx_init(inst_t *s, vc_t *v, int p, int ty) {
    const nx_t *n = &VAR4[ty - T_V4]; const float *q = n->p; const float sr = s->sr;
    const float dm = s->dm[p], f0 = v->f0;
    switch (n->eng) {
    case 0: {                                                       /* MODAL: a bank of decaying resonators, struck by a short burst */
        int set = (int)(q[0] + 0.5f); if (set < 0) set = 0; if (set >= N_MSET) set = N_MSET - 1;
        const float tilt = q[3], str = q[4];
        for (int k = 0; k < 8; k++) {
            const float rt = MD_R[set][k], am = MD_A[set][k];
            v->nx[16 + k] = v->nx[24 + k] = v->nx[32 + k] = 0.0f;
            if (rt <= 0.0f || am <= 0.0f) continue;
            const float fk = f0 * rt * (1.0f + str * (rt - 1.0f));
            if (fk > 0.45f * sr || fk < 10.0f) continue;
            float tk = n->tau1 * dm * (rt > 1.0f ? powf(rt, -tilt) : 1.0f); if (tk < 0.004f) tk = 0.004f;
            const float r = expf(-1.0f / (tk * sr)), w = TWO_PI * fk / sr;
            v->nx[16 + k] = 2.0f * r * cosf(w); v->nx[24 + k] = -r * r; v->nx[32 + k] = am * sinf(w);
        }
        const float b = clampf(q[1] * (0.45f + 0.9f * v->vel), 0.02f, 1.0f);
        v->nx[40] = 0.0f;                                           /* excitation low-pass state */
        v->nx[41] = 1.0f;                                           /* burst envelope */
        v->nx[42] = expf(-1.0f / (clampf(q[2], 0.2f, 40.0f) * 0.001f * sr));
        v->nx[43] = 0.03f + 0.97f * b * b * b;
        v->nx[44] = 1.0f;                                           /* the impulse */
        v->nx[45] = 0.0f; v->nx[46] = q[5];
        break; }
    case 1:                                                         /* FM */
        v->nx[0] = v->nx[1] = v->nx[2] = v->nx[3] = 0.0f; break;
    case 2:                                                         /* VOCAL */
        for (int k = 0; k < 8; k++) v->nx[k] = 0.0f;
        v->nx[8] = 0.0f; break;
    case 3: {                                                       /* CHIP */
        v->nx[0] = -1.0f; v->nx[1] = 1.0f; v->nx[3] = 1.0f;
        v->nx[4] = exp2f(q[7] / 12.0f * s->inv_sr);
        v->nx[5] = 0.0f; v->nx[6] = 0.0f; v->nx[7] = 0.0f; v->ni[0] = 0x4001; break; }
    default:                                                        /* MORPH */
        v->nx[0] = 0.0f; v->nx[1] = 0.37f; v->nx[2] = 0.71f; v->nx[3] = 0.0f; v->nx[4] = 0.0f; break;
    }
}
static void trig_raw(inst_t *s, int p, float vel, int arp_semi, int fresh) {
    vc_t *v = &s->vc[p];
    s->tgen[p]++;
    int ty = (int)(s->pp[p][PP_TYPE] + 0.5f);
    if (ty < 0 || ty >= T_COUNT) return;
    const ty_t *T = &TY[ty];
    float fm = powf(2.0f, (s->semi[p] + (float)arp_semi) / 12.0f);
    float base = T->oct > 0.0f ? s->root_hz * T->oct : T->f0;

    if (T->sus && v->active && v->gate && v->type == ty && !fresh) {   /* legato arp step */
        v->fm = fm; v->f0 = base * fm;
        return;
    }
    if (s->pp[p][PP_POLY] > 0.5f && v->active) {         /* POLY: the old note rings out in a ghost voice */
        int gi = -1;
        for (int i = 0; i < NGH; i++) if (!s->vc[NLANE + i].active) { gi = i; break; }
        if (gi < 0) { gi = s->gnext; s->gnext = (s->gnext + 1) % NGH; }
        s->vc[NLANE + gi] = *v; s->vown[NLANE + gi] = p;
        if (!v->gk) s->vc[NLANE + gi].gate = 0;
    }
    if (p < NLANE) {                                          /* a kick (or any hit of a track whose Duck Src = On) ducks every track that has Duck > 0 */
        const int kt = lane_trk(s, p);
        const int scs = (int)(s->pp[kt][PP_SCS] * 2.0f + 0.5f);      /* 0 Auto (kick family), 1 On (all hits), 2 Off */
        if (scs == 1 || (scs == 0 && BASE_T[ty] <= T_SUB808))
        for (int t = 0; t < NTRK; t++) if (t != kt && s->pp[t][PP_DUCK] > 0.01f) s->dk_env[t] = 1.0f;
    }
    memset(v, 0, sizeof(*v)); v->fenv = 1.0f;
    v->fkoff = s->pp[p][PP_FKEY] * 0.16f * log2f(fm > 0.05f ? fm : 0.05f);
    v->active = 1; v->gate = 1; v->type = ty;
    v->vel = 0.25f + 0.75f * vel;
    { int w0 = (int)(s->pp[p][PP_WAVE] + 0.5f); v->sw = (w0 >= 1 && w0 <= 15) ? w0 : CHR[ty].wv; }   /* 0 = the sound's own oscillator */
    v->fm = fm; v->f0 = base * fm;
    if (BASE_T[ty] >= T_S0 && BASE_T[ty] < T_S0 + 8) {
        const sy_t *y = &SYT[ty];
        int w = (int)(s->pp[p][PP_WAVE] + 0.5f);
        v->sw = (w >= 1 && w <= 15) ? w : (CHR[ty].wv ? CHR[ty].wv : y->wave);
        v->sdet = y->det + s->pp[p][PP_DET] * 0.04f;
        v->ssub = clampf(y->sub + s->pp[p][PP_SUB], 0.0f, 1.0f);
        v->satt = T->att + s->pp[p][PP_ATT] * s->pp[p][PP_ATT] * 1.5f;
        for (int k = 0; k < 6; k++) v->ph6[k] = 0.137f * (float)(k + 1);
    }
    if (ty == T_SAMPLE) {
        int si = (int)(s->pp[p][PP_SMP] * 63.0f + 0.5f);
        const smp_t *m = smp_get(si);
        if (!m) { v->active = 0; return; }
        float a = s->pp[p][PP_SST], b = s->pp[p][PP_SEN];
        if (b < a + 0.002f) b = a + 0.002f;
        v->s0 = (int)(a * (float)m->n); v->s1 = (int)(b * (float)m->n);
        if (v->s1 > m->n) v->s1 = m->n; if (v->s0 > v->s1 - 4) v->s0 = v->s1 - 4; if (v->s0 < 0) v->s0 = 0;
        v->smode = (int)(s->pp[p][PP_SMD] * 2.0f + 0.5f);
        v->sidx = si; v->srate = m->sr * s->inv_sr;
        { float sv = s->pp[p][PP_SSTR]; v->stch = (fabsf(sv - 0.5f) > 0.01f && v->smode != 1); v->stg = powf(2.0f, (sv - 0.5f) * 2.0f); /* output length factor 0.5x .. 2x */
          v->ga = (double)v->s0; v->g0[1] = (double)v->s0; v->gp[1] = 0.0f; v->gp[0] = 1024.0f; v->g0[0] = (double)v->s0 - 1024.0 * (double)(fm * v->srate); }
        v->sp = v->smode == 1 ? (double)(v->s1 - 1) : (double)v->s0;
        float dc = s->pp[p][PP_DECAY];
        v->env = 1.0f; v->env2 = 1.0f; v->pe = 0.0f; v->c2 = 0.0f; v->pek = 0.0f;
        v->c1 = dc < 0.5f ? expf(-1.0f / (0.01f * powf(1000.0f, dc * 2.0f) * s->sr)) : 1.0f;
        v->sgain = powf(2.0f, (s->pp[p][PP_SGN] - 0.5f) * 6.0f);
        { float at = s->pp[p][PP_SATK]; v->satk_inc = at < 0.005f ? 1.0f : 1.0f / (0.002f + at * at * 1.5f) * s->inv_sr; v->sgate_env = at < 0.005f ? 1.0f : 0.0f; }
        { float rl = s->pp[p][PP_SREL]; v->srel_c = rl < 0.005f ? 0.0f : expf(-1.0f / ((0.005f + rl * rl * 1.5f) * s->sr)); }
        return;
    }
    v->env = T->sus ? 0.0f : 1.0f;
    v->env2 = 1.0f; v->pe = 1.0f;
    float dm = s->dm[p];
    v->c1 = T->tau1 > 0.0f ? expf(-1.0f / (T->tau1 * dm * s->sr)) : 0.0f;
    v->c2 = T->tau2 > 0.0f ? expf(-1.0f / (T->tau2 * dm * s->sr)) : 0.0f;
    v->pek = T->pet > 0.0f ? expf(-1.0f / (T->pet * s->sr)) : 0.0f;
    v->third = powf(2.0f, (float)SCALE_TBL[(int)(s->g[G_SCALE] + 0.5f) % 3][1] / 12.0f);
    if (BASE_T[ty] >= T_E_MODAL) nx_init(s, v, p, ty);
    if (BASE_T[ty] == T_HATCL) {                         /* choke every open hat */
        for (int i = 0; i < NLANE; i++)
            if (s->vc[i].active && BASE_T[s->vc[i].type] == T_HATOP && lane_trk(s, i) == lane_trk(s, p)) s->vc[i].active = 0;
    }
}

/* ---------------- MIDI effects (per track, 10 types) ---------------- */
static const int MS7[3][7] = { { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 11 } };
static void ghost_put(inst_t *s, int p, const vc_t *lay) {
    int gi = -1;
    for (int i = 0; i < NGH; i++) if (!s->vc[NLANE + i].active) { gi = i; break; }
    if (gi < 0) { gi = s->gnext; s->gnext = (s->gnext + 1) % NGH; }
    s->vc[NLANE + gi] = *lay; s->vown[NLANE + gi] = p;
}
/* an extra note on the same sound: it plays in a ghost voice so the lane's own voice is left alone */
static void trig_layer(inst_t *s, int p, float vel, int semi, int follow) {
    vc_t save = s->vc[p]; const float pol = s->pp[p][PP_POLY]; const int g = s->tgen[p];
    s->pp[p][PP_POLY] = 0.0f;
    trig_raw(s, p, vel, semi, 1);
    vc_t lay = s->vc[p];
    s->vc[p] = save; s->pp[p][PP_POLY] = pol; s->tgen[p] = g;
    if (!lay.active) return;
    if (follow) { lay.fol = 1; lay.fgen = g; } else { lay.fol = 0; lay.gate = 0; }
    ghost_put(s, p, &lay);
}
static void eq_push(inst_t *s, int lane, float vel, int semi, int cnt, int follow) {
    for (int i = 0; i < 32; i++) if (s->eq[i].cnt <= 0) { s->eq[i].cnt = cnt < 1 ? 1 : cnt; s->eq[i].lane = lane; s->eq[i].vel = vel; s->eq[i].semi = semi; s->eq[i].follow = follow; return; }
}
static void trig(inst_t *s, int p, float vel, int arp_semi, int fresh) {
    const int mf = (int)(s->pp[p][PP_MFX] + 0.5f);
    if (mf <= 0 || mf > 10 || !fresh) { trig_raw(s, p, vel, arp_semi, fresh); return; }
    const float am = s->pp[p][PP_MAMT], pr = s->pp[p][PP_MPRM];
    const int sc = (int)(s->g[G_SCALE] + 0.5f) % 3;
    const float lv = vel * (0.35f + 0.65f * am);
    const float L16 = s->sr * 60.0f / s->bpm / 4.0f;
    static const int CHD[8][4] = { {0,4,7,-99}, {0,3,7,-99}, {0,2,7,-99}, {0,5,7,-99}, {0,4,7,10}, {0,3,7,10}, {0,4,7,11}, {0,3,6,9} };
    switch (mf) {
    case 1: {                                                    /* OCTAVE: Param = which layers (-1 / +1 / both / +1 +2) */
        trig_raw(s, p, vel, arp_semi, 1);
        if (pr < 0.25f) trig_layer(s, p, lv, arp_semi - 12, 1);
        else if (pr < 0.5f) trig_layer(s, p, lv, arp_semi + 12, 1);
        else if (pr < 0.75f) { trig_layer(s, p, lv, arp_semi + 12, 1); trig_layer(s, p, lv, arp_semi - 12, 1); }
        else { trig_layer(s, p, lv, arp_semi + 12, 1); trig_layer(s, p, lv, arp_semi + 24, 1); }
        break; }
    case 2: {                                                    /* CHORD: Param = chord type (maj min sus2 sus4 7 m7 maj7 dim) */
        int ci = (int)(pr * 7.99f); if (ci < 0) ci = 0; if (ci > 7) ci = 7;
        trig_raw(s, p, vel, arp_semi, 1);
        for (int k = 1; k < 4; k++) if (CHD[ci][k] > -90) trig_layer(s, p, lv, arp_semi + CHD[ci][k], 1);
        break; }
    case 3: {                                                    /* ARP: root, then the notes of the scale chord one by one (Param = rate 1/16, 1/8, 1/32) */
        const float st = pr < 0.33f ? 1.0f : (pr < 0.66f ? 2.0f : 0.5f);
        const int d = (int)(st * L16);
        trig_raw(s, p, vel, arp_semi, 1);
        eq_push(s, p, lv, arp_semi + MS7[sc][2], d, 0);
        eq_push(s, p, lv, arp_semi + 7, 2 * d, 0);
        eq_push(s, p, lv, arp_semi + 12, 3 * d, 0);
        if (am > 0.5f) { eq_push(s, p, lv * 0.9f, arp_semi + MS7[sc][2] + 12, 4 * d, 0); eq_push(s, p, lv * 0.8f, arp_semi + 7 + 12, 5 * d, 0); }
        break; }
    case 4: {                                                    /* STRUM: root, third, fifth one after another (Param = speed) */
        const int d = (int)((0.006f + pr * 0.06f) * s->sr);
        trig_raw(s, p, vel, arp_semi, 1);
        eq_push(s, p, lv, arp_semi + MS7[sc][2], d, 1);
        eq_push(s, p, lv, arp_semi + 7, 2 * d, 1);
        break; }
    case 5: {                                                    /* ECHO: Amount = feedback, Param = spacing */
        const int d = (int)((0.5f + pr * 3.5f) * L16); const float fb = 0.35f + 0.5f * am;
        trig_raw(s, p, vel, arp_semi, 1);
        eq_push(s, p, vel * fb, arp_semi, d, 0);
        eq_push(s, p, vel * fb * fb, arp_semi, 2 * d, 0);
        if (am > 0.4f) eq_push(s, p, vel * fb * fb * fb, arp_semi, 3 * d, 0);
        break; }
    case 6: {                                                    /* RANDOM PITCH inside the scale, range grows with Amount */
        s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
        int r = (int)((s->rng >> 4) % (unsigned)(2 + (int)(am * 12.0f)));
        int deg = MS7[sc][r % 7] + 12 * (r / 7);
        if (s->rng & 0x8000u) deg = -deg;
        trig_raw(s, p, vel, arp_semi + deg, 1); break; }
    case 7: {                                                    /* RANDOM VELOCITY */
        s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
        float rr = (float)(s->rng & 0xFFFF) / 65536.0f;
        trig_raw(s, p, clampf(vel * (1.0f - am * 0.85f * rr), 0.05f, 1.0f), arp_semi, 1); break; }
    case 8: trig_raw(s, p, clampf(vel + (0.85f - vel) * am, 0.05f, 1.0f), arp_semi, 1); break;   /* VELOCITY COMPRESSOR */
    case 9: {                                                    /* SCALE: snap every note to the current key / scale */
        int o = arp_semi >= 0 ? arp_semi / 12 : -((-arp_semi + 11) / 12), r = arp_semi - o * 12, best = 0, bd = 99;
        for (int k = 0; k < 8; k++) { int dv = k < 7 ? MS7[sc][k] : 12, dd = dv > r ? dv - r : r - dv; if (dd < bd) { bd = dd; best = dv; } }
        trig_raw(s, p, vel, o * 12 + best, 1); break; }
    default:                                                     /* TRANSPOSE: Param = -24 .. +24 semitones */
        trig_raw(s, p, vel, arp_semi + (int)((pr - 0.5f) * 48.0f + (pr >= 0.5f ? 0.5f : -0.5f)), 1); break;
    }
}

static inline float metal(vc_t *v, float scale, float dt) {
    float m = 0.0f;
    for (int k = 0; k < 6; k++) {
        v->ph6[k] += HAT_FREQ[k] * scale * dt;
        if (v->ph6[k] >= 1.0f) v->ph6[k] -= 1.0f;
        m += v->ph6[k] < 0.5f ? 1.0f : -1.0f;
    }
    return m * (1.0f / 6.0f);
}

#include "wtab.inc"
static inline float wt_at(float ph, int t) {                      /* single-cycle table (AKWF, CC0), linear interpolation */
    if (t < 0) t = 0; if (t >= NWT) t = NWT - 1;
    const float x = ph * (float)WTLEN; int i = (int)x; const float f = x - (float)i; if (i >= WTLEN) i = 0;
    const int j = (i + 1 == WTLEN) ? 0 : i + 1;
    return ((float)WT[t][i] * (1.0f - f) + (float)WT[t][j] * f) * (1.0f / 32000.0f);
}
static inline float wf(float ph, int w) {
    if (w >= 100) return wt_at(ph, w - 100);
    switch (w) {
    case 2:  return ph < 0.5f ? 1.0f : -1.0f;
    case 3:  return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
    case 4:  return sinf(TWO_PI * ph);
    case 5:  return ph < 0.25f ? 1.0f : -1.0f;
    case 6:  return ph < 0.125f ? 1.0f : -1.0f;
    case 7:  return 1.0f - 2.0f * ph;
    case 8:  return 2.0f * fabsf(sinf(TWO_PI * ph)) - 1.0f;
    case 9:  return (sinf(TWO_PI * ph) + 0.6f * sinf(TWO_PI * 2.0f * ph) + 0.35f * sinf(TWO_PI * 4.0f * ph)) * 0.6f;
    case 10: return (sinf(TWO_PI * ph) + 0.33f * sinf(TWO_PI * 3.0f * ph) + 0.2f * sinf(TWO_PI * 5.0f * ph)) * 0.75f;
    case 11: { float x = 1.9f * sinf(TWO_PI * ph); if (x > 1.0f) x = 2.0f - x; if (x < -1.0f) x = -2.0f - x; return x; }
    case 12: return floorf(ph * 5.0f) * 0.5f - 1.0f;
    case 13: return floorf((4.0f * fabsf(ph - 0.5f) - 1.0f) * 3.5f + 0.5f) * (1.0f / 3.5f);
    case 14: return (sinf(TWO_PI * ph) * 0.5f + sinf(TWO_PI * 3.0f * ph + 1.0f) * 0.5f + sinf(TWO_PI * 5.0f * ph + 2.0f) * 0.4f) * 0.8f;
    case 15: { float q = ph * 2.5f; q -= floorf(q); return 2.0f * q - 1.0f; }
    default: return 2.0f * ph - 1.0f;
    }
}
#define WRAP(x) do { if ((x) >= 1.0f) (x) -= 1.0f; } while (0)

static inline float bsin(const vc_t *v) { return v->sw >= 100 ? wt_at(v->ph, v->sw - 100) : sinf(TWO_PI * v->ph); }      /* body oscillator: sine, or the preset's own wavetable */


/* ---------------- NEW ENGINES: one sample ---------------- */
static float nx_render(inst_t *s, vc_t *v) {
    const nx_t *n = &VAR4[v->type - T_V4]; const float *q = n->p; const ty_t *T = &TY[v->type]; const float dt = s->inv_sr;
    float x = 0.0f;
    switch (n->eng) {
    case 0: {                                                       /* MODAL */
        float e = v->nx[44]; v->nx[44] = 0.0f;
        e += noise(s) * v->nx[46] * v->nx[41]; v->nx[41] *= v->nx[42];
        v->nx[40] += v->nx[43] * (e - v->nx[40]); e = v->nx[40];
        for (int k = 0; k < 8; k++) {
            const float y = v->nx[16 + k] * v->nx[k] + v->nx[24 + k] * v->nx[8 + k] + e * v->nx[32 + k];
            v->nx[8 + k] = v->nx[k]; v->nx[k] = y; x += y;
        }
        if (q[6] > 0.0f) { v->nx[45] += q[7] * dt; if (v->nx[45] >= 1.0f) v->nx[45] -= 1.0f; x *= 1.0f - q[6] * 0.5f * (1.0f - sinf(TWO_PI * v->nx[45])); }
        v->env *= v->c1; if (v->env < 1e-3f) v->active = 0;
        break; }
    case 1: {                                                       /* FM: carrier, two modulators, feedback, three algorithms */
        const float f = v->f0, algo = q[8], fl = q[9];
        const float ie = fl + (1.0f - fl) * v->env2, vs = 1.0f + q[7] * (2.0f * v->vel - 1.0f) * 0.6f;
        v->ph += f * q[5] * dt; WRAP(v->ph);
        v->ph2 += f * q[0] * dt; WRAP(v->ph2);
        v->ph3 += f * q[2] * dt; WRAP(v->ph3);
        float out;
        if (algo < 0.5f) {                                          /* C -> B -> A */
            const float mc = sinf(TWO_PI * v->ph3) * q[3] * ie * vs;
            const float sb = sinf(TWO_PI * v->ph2 + mc + q[4] * v->nx[2]); v->nx[2] = sb;
            out = sinf(TWO_PI * v->ph + sb * q[1] * ie * vs);
        } else if (algo < 1.5f) {                                   /* B + C -> A (C follows the amplitude) */
            const float sb = sinf(TWO_PI * v->ph2 + q[4] * v->nx[2]); v->nx[2] = sb;
            const float sc = sinf(TWO_PI * v->ph3);
            out = sinf(TWO_PI * v->ph + sb * q[1] * ie * vs + sc * q[3] * (0.3f + 0.7f * v->env) * vs);
        } else {                                                    /* two FM pairs side by side */
            const float sb = sinf(TWO_PI * v->ph2 + q[4] * v->nx[2]); v->nx[2] = sb;
            out = 0.65f * sinf(TWO_PI * v->ph + sb * q[1] * ie * vs) + 0.35f * sinf(TWO_PI * v->ph3 + sb * q[3] * ie * vs);
        }
        if (q[6] > 0.0f) { v->nx[3] += f * 0.5f * dt; if (v->nx[3] >= 1.0f) v->nx[3] -= 1.0f; out += q[6] * sinf(TWO_PI * v->nx[3]); }
        if (T->sus) {
            const float at = T->att > 0.0005f ? T->att : 0.0005f;
            if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        } else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        v->env2 *= v->c2;
        x = out * v->env * 0.8f;
        break; }
    case 2: {                                                       /* VOCAL: glottal pulse through three formant filters */
        v->nx[8] += q[5] * dt; if (v->nx[8] >= 1.0f) v->nx[8] -= 1.0f;
        const float vib = 1.0f + q[4] * sinf(TWO_PI * v->nx[8]) * (v->t > 0.25f ? 1.0f : v->t * 4.0f);
        v->ph += v->f0 * vib * dt; WRAP(v->ph);
        float g = v->ph < 0.45f ? 0.5f * (1.0f - cosf(TWO_PI * v->ph / 0.45f)) - 0.225f : -0.225f;
        float src = g * 3.0f; src = src * (1.0f - q[3]) + (2.0f * v->ph - 1.0f) * 0.6f * q[3];
        src += noise(s) * q[6] * 0.7f;
        const int ia = (int)(q[0] + 0.5f) % 7, ib = (int)(q[1] + 0.5f) % 7;
        const float mm = 1.0f - v->env2;
        static const float QV[3] = { 0.22f, 0.13f, 0.1f };
        const float W[3] = { 1.0f, 0.65f + 0.3f * q[8], 0.3f + 0.5f * q[8] };
        for (int k = 0; k < 3; k++) {
            float F = (VW_F[ia][k] + (VW_F[ib][k] - VW_F[ia][k]) * mm) * q[9];
            float ff = TWO_PI * F * dt; if (ff > 1.2f) ff = 1.2f;
            float *lp = &v->nx[k * 2], *bp = &v->nx[k * 2 + 1];
            *lp += ff * *bp;
            const float hp = src - *lp - QV[k] * q[7] * *bp;
            *bp += ff * hp;
            x += *bp * W[k];
        }
        x *= 0.9f;
        { const float at = T->att > 0.0005f ? T->att : 0.0005f;
          if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
        v->env2 *= v->c2;
        x *= v->env;
        break; }
    case 3: {                                                       /* CHIP */
        const int am = (int)(q[1] + 0.5f) % 7;
        float ratio = v->nx[1];
        if (am > 0) {
            const int st = (int)(v->t * q[2]) % CH_ARPN[am];
            if (st != (int)v->nx[0]) { v->nx[0] = (float)st; v->nx[1] = exp2f((float)CH_ARP[am][st] / 12.0f); }
            ratio = v->nx[1];
        }
        v->nx[3] *= v->nx[4];
        float vm = 1.0f;
        if (q[3] > 0.0f && v->t > q[4]) { v->nx[5] += 6.0f * dt; if (v->nx[5] >= 1.0f) v->nx[5] -= 1.0f; vm += q[3] * sinf(TWO_PI * v->nx[5]); }
        const float f = v->f0 * ratio * v->nx[3] * vm;
        float o;
        const int hold = (int)(q[9] + 0.5f);
        if (v->nx[6] > 0.5f) { v->nx[6] -= 1.0f; o = v->nx[7]; }
        else {
            v->ph += f * dt; int wr = 0; if (v->ph >= 1.0f) { v->ph -= 1.0f; wr = 1; }
            if (q[6] > 0.5f) {
                if (wr) { unsigned l = (unsigned)v->ni[0]; const unsigned bit = (l ^ (l >> 1)) & 1u; l = (l >> 1) | (bit << (q[6] > 1.5f ? 6 : 14)); v->ni[0] = (int)l; }
                o = (v->ni[0] & 1) ? 1.0f : -1.0f;
            } else {
                float duty = q[0] + q[8] * v->t; if (duty < 0.06f) duty = 0.06f; if (duty > 0.5f) duty = 0.5f;
                o = v->ph < duty ? 1.0f : -1.0f;
            }
            v->nx[7] = o; v->nx[6] = hold > 1 ? (float)(hold - 1) : 0.0f;
        }
        if (T->sus) {
            const float at = T->att > 0.0005f ? T->att : 0.0005f;
            if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        } else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = o * v->env * 0.6f;
        if (q[5] > 0.5f) { const float L = (float)((1 << (int)(q[5] + 0.5f)) - 1) * 0.5f; x = floorf(x * L + 0.5f) / L; }
        break; }
    default: {                                                      /* MORPH: two real wavetables crossfaded over time */
        const float f = v->f0;
        v->nx[3] += q[4] * dt; if (v->nx[3] >= 1.0f) v->nx[3] -= 1.0f;
        float m = q[3] + (q[2] - q[3]) * v->env2 + q[5] * sinf(TWO_PI * v->nx[3]); m = clampf(m, 0.0f, 1.0f);
        const int ta = (int)q[0], tb = (int)q[1];
        v->ph += f * dt; WRAP(v->ph);
        float osc = (1.0f - m) * wt_at(v->ph, ta) + m * wt_at(v->ph, tb);
        if (q[6] > 0.0f) {
            v->nx[0] += f * (1.0f + q[6]) * dt; if (v->nx[0] >= 1.0f) v->nx[0] -= 1.0f;
            v->nx[1] += f * (1.0f - q[6]) * dt; if (v->nx[1] >= 1.0f) v->nx[1] -= 1.0f;
            osc = osc * 0.5f + 0.25f * ((1.0f - m) * wt_at(v->nx[0], ta) + m * wt_at(v->nx[0], tb) + (1.0f - m) * wt_at(v->nx[1], ta) + m * wt_at(v->nx[1], tb));
        }
        if (q[9] > 0.0f) { v->nx[2] += f * 0.5f * dt; if (v->nx[2] >= 1.0f) v->nx[2] -= 1.0f; osc += q[9] * sinf(TWO_PI * v->nx[2]); }
        float fc = f * q[7] * (1.0f + q[8] * v->env2 * 4.0f); if (fc > 16000.0f) fc = 16000.0f; if (fc < 100.0f) fc = 100.0f;
        const float xx = TWO_PI * fc * dt, a = xx / (1.0f + xx);
        v->z1 += a * (osc - v->z1); v->z2 += a * (v->z1 - v->z2);
        if (T->sus) {
            const float at = T->att > 0.0005f ? T->att : 0.0005f;
            if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        } else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        v->env2 *= v->c2;
        x = v->z2 * v->env * 1.1f;
        break; }
    }
    return x;
}

/* one sample of one voice (pre tone/drive/gain) */
static float vc_render(inst_t *s, vc_t *v, int p) {
    const float dt = s->inv_sr;
    const ty_t *T = &TY[v->type];
    const float fm = v->fm;
    float x = 0.0f;

    switch (BASE_T[v->type]) {
    case T_KICK: case T_KICKHRD: case T_KICKGOA: case T_RUMBKICK: {
        static const float SW[4]  = { 150.0f, 240.0f, 300.0f, 170.0f };
        static const float DR[4]  = { 1.8f, 3.5f, 2.5f, 1.6f };
        static const float CK[4]  = { 0.20f, 0.35f, 0.25f, 0.10f };
        int k = BASE_T[v->type] - T_KICK;
        float f = v->f0 + SW[k] * fm * v->pe;
        v->pe *= v->pek;
        v->ph += f * dt; WRAP(v->ph);
        x = fast_tanh(bsin(v) * v->env * DR[k]) * 0.85f + noise(s) * v->env2 * CK[k];
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_SUB808: {
        float f = v->f0 + 100.0f * fm * v->pe;
        v->pe *= v->pek;
        v->ph += f * dt; WRAP(v->ph);
        x = fast_tanh(bsin(v) * v->env * 1.6f) * 0.8f;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_CLAP: {
        float nz = noise(s);
        v->z1 += s->a_clap_hp * (nz - v->z1); nz -= v->z1;
        v->z2 += s->a_clap_lp * (nz - v->z2); nz = v->z2;
        float e;
        if (v->t < 0.033f) e = expf(-(v->t - 0.011f * floorf(v->t / 0.011f)) * 400.0f);
        else { e = 0.75f * v->env; v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = nz * e * 2.2f;
        break; }
    case T_SNARE: {
        float f = v->f0 + 140.0f * fm * v->pe;
        v->pe *= v->pek;
        v->ph += f * dt; WRAP(v->ph);
        float body = sinf(TWO_PI * v->ph) * v->env;
        float nz = noise(s);
        v->z1 += s->a_snr_hp * (nz - v->z1);
        x = body * 0.9f + (nz - v->z1) * v->env2 * 0.8f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env2 < 1e-3f && v->env < 1e-3f) v->active = 0;
        break; }
    case T_RIM: {
        v->ph  += 1750.0f * fm * dt; WRAP(v->ph);
        v->ph2 += 480.0f * fm * dt;  WRAP(v->ph2);
        float nz = noise(s);
        v->z1 += s->a_rim_hp * (nz - v->z1);
        x = (sinf(TWO_PI * v->ph) * 0.5f + sinf(TWO_PI * v->ph2) * 0.4f) * v->env + (nz - v->z1) * v->env2 * 0.6f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_HATCL: case T_HATOP: {
        float m = metal(v, fm, dt) + noise(s) * 0.3f;
        v->z1 += s->a_hat_hp * (m - v->z1); m -= v->z1;
        v->z2 += s->a_hat_hp * (m - v->z2); m -= v->z2;
        x = m * v->env * 1.6f;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_SHAKER: {
        float nz = noise(s);
        v->z1 += s->a_shk_hp * (nz - v->z1); nz -= v->z1;
        v->z2 += s->a_shk_lp * (nz - v->z2); nz = v->z2;
        float att = 1.0f - expf(-v->t * 350.0f);
        x = nz * att * v->env * 2.4f;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_CRASH: {
        float m = metal(v, 1.15f * fm, dt) + noise(s) * 0.5f;
        v->z1 += s->a_crash_hp * (m - v->z1); m -= v->z1;
        v->z2 += s->a_crash_hp * (m - v->z2); m -= v->z2;
        x = m * v->env * 1.2f;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_RIDE: {
        float m = metal(v, 0.7f * fm, dt) + noise(s) * 0.08f;
        v->z1 += s->a_crash_hp * (m - v->z1); m -= v->z1;
        v->ph += 3150.0f * fm * dt; WRAP(v->ph);
        x = m * v->env * 0.9f + sinf(TWO_PI * v->ph) * v->env2 * 0.25f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_TOM: {
        float f = v->f0 + 130.0f * fm * v->pe;
        v->pe *= v->pek;
        v->ph += f * dt; WRAP(v->ph);
        x = fast_tanh(bsin(v) * v->env * 1.4f) * 0.85f;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_CONGA: {
        float f = v->f0 + 70.0f * fm * v->pe;
        v->pe *= v->pek;
        v->ph += f * dt; WRAP(v->ph);
        x = fast_tanh(bsin(v) * v->env * 1.5f) * 0.8f;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_COWBELL: {
        v->ph  += 587.0f * fm * dt; WRAP(v->ph);
        v->ph2 += 845.0f * fm * dt; WRAP(v->ph2);
        float sq = (v->ph < 0.5f ? 0.5f : -0.5f) + (v->ph2 < 0.5f ? 0.5f : -0.5f);
        float xx = TWO_PI * 2600.0f * dt;
        v->z1 += (xx / (1.0f + xx)) * (sq - v->z1);
        x = v->z1 * v->env;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_PERC: {
        v->ph  += v->f0 * dt;        WRAP(v->ph);
        v->ph2 += v->f0 * 1.41f * dt; WRAP(v->ph2);
        x = sinf(TWO_PI * v->ph + 3.0f * v->env2 * sinf(TWO_PI * v->ph2)) * v->env;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_ZAP: {
        float f = v->f0 + 4800.0f * fm * v->pe;
        v->pe *= v->c2;
        v->ph += f * dt; WRAP(v->ph);
        x = sinf(TWO_PI * v->ph) * v->env;
        v->env *= v->c1;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_RISER: {
        float len = 1.4f * clampf(s->dm[p], 0.5f, 2.0f);
        float u = v->t / len, amp, fcx;
        if (u < 1.0f) { amp = u * sqrtf(u); fcx = 150.0f + 8500.0f * u * u * fm; }
        else { amp = expf(-(v->t - len) * 60.0f); fcx = 8650.0f; if (amp < 1e-3f) v->active = 0; }
        float xx = TWO_PI * fcx * dt;
        float a = xx / (1.0f + xx);
        v->z1 += a * (noise(s) - v->z1);
        v->z2 += a * (v->z1 - v->z2);
        x = v->z2 * amp * 3.5f;
        break; }
    case T_SUBBASS: {
        v->ph += v->f0 * dt; WRAP(v->ph);
        if (v->gate) v->env += (1.0f - v->env) * (dt / T->att);
        else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = fast_tanh(bsin(v) * 1.4f) * v->env * 0.8f;
        break; }
    case T_ROLLBASS: {
        v->ph += v->f0 * dt; WRAP(v->ph);
        float saw = 2.0f * v->ph - 1.0f;
        float fc = 220.0f + 1600.0f * v->env2;
        float xx = TWO_PI * fc * dt, a = xx / (1.0f + xx);
        v->z1 += a * (saw - v->z1);
        v->z2 += a * (v->z1 - v->z2);
        x = (v->z2 * 0.7f + sinf(TWO_PI * v->ph) * 0.6f) * v->env;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_ACID: {
        v->ph += v->f0 * dt; WRAP(v->ph);
        float saw = v->sw ? wf(v->ph, v->sw) : 2.0f * v->ph - 1.0f;
        float fc = 120.0f + 1900.0f * sqrtf(v->env2);
        float f1 = clampf(TWO_PI * fc * dt, 0.0f, 0.9f);
        v->z1 += f1 * v->z2;
        float hp = saw - v->z1 - 0.2f * v->z2;
        v->z2 += f1 * hp;
        x = fast_tanh(v->z1 * 2.0f) * v->env * 0.9f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_REESE: {
        v->ph  += v->f0 * dt;           WRAP(v->ph);
        v->ph2 += v->f0 * 1.006f * dt;  WRAP(v->ph2);
        v->ph3 += v->f0 * 0.994f * dt;  WRAP(v->ph3);
        float saw = v->sw ? wf(v->ph, v->sw) + wf(v->ph2, v->sw) + wf(v->ph3, v->sw) : (v->ph + v->ph2 + v->ph3) * 2.0f - 3.0f;
        v->z1 += s->a_reese * (saw - v->z1);
        v->z2 += s->a_reese * (v->z1 - v->z2);
        if (v->gate) v->env += (1.0f - v->env) * (dt / T->att);
        else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = v->z2 * v->env * 0.9f;
        break; }
    case T_PLUCK: {
        v->ph  += v->f0 * dt;          WRAP(v->ph);
        v->ph2 += v->f0 * 1.007f * dt; WRAP(v->ph2);
        float saw = v->sw ? 0.5f * (wf(v->ph, v->sw) + wf(v->ph2, v->sw)) : (v->ph + v->ph2) - 1.0f;
        float xx = TWO_PI * (500.0f + 6000.0f * v->env2 * v->env2) * dt;
        float a = xx / (1.0f + xx);
        v->z1 += a * (saw - v->z1);
        v->z2 += a * (v->z1 - v->z2);
        x = v->z2 * v->env * 1.2f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_STAB: {
        v->ph  += v->f0 * dt;               WRAP(v->ph);
        v->ph2 += v->f0 * v->third * dt;    WRAP(v->ph2);
        v->ph3 += v->f0 * 1.49831f * dt;    WRAP(v->ph3);
        float saw = v->sw ? wf(v->ph, v->sw) + wf(v->ph2, v->sw) + wf(v->ph3, v->sw) : (v->ph + v->ph2 + v->ph3) * 2.0f - 3.0f;
        float xx = TWO_PI * (400.0f + 4200.0f * v->env2) * dt;
        float a = xx / (1.0f + xx);
        v->z1 += a * (saw - v->z1);
        v->z2 += a * (v->z1 - v->z2);
        x = v->z2 * v->env * 0.6f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_PAD: {
        v->ph  += v->f0 * dt;               WRAP(v->ph);
        v->ph2 += v->f0 * v->third * 1.003f * dt; WRAP(v->ph2);
        v->ph3 += v->f0 * 1.49831f * 0.997f * dt; WRAP(v->ph3);
        float saw = v->sw ? wf(v->ph, v->sw) + wf(v->ph2, v->sw) + wf(v->ph3, v->sw) : (v->ph + v->ph2 + v->ph3) * 2.0f - 3.0f;
        v->z1 += s->a_pad * (saw - v->z1);
        v->z2 += s->a_pad * (v->z1 - v->z2);
        if (v->gate) v->env += (1.0f - v->env) * (dt / T->att);
        else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = v->z2 * v->env * 0.7f;
        break; }
    case T_LEAD: {
        v->ph  += v->f0 * dt;          WRAP(v->ph);
        v->ph2 += v->f0 * 1.005f * dt; WRAP(v->ph2);
        float osc = v->sw ? wf(v->ph, v->sw) * 0.6f + wf(v->ph2, v->sw) * 0.4f : (2.0f * v->ph - 1.0f) * 0.6f + (v->ph2 < 0.5f ? 0.4f : -0.4f);
        v->z1 += s->a_lead * (osc - v->z1);
        v->z2 += s->a_lead * (v->z1 - v->z2);
        if (v->gate) v->env += (1.0f - v->env) * (dt / T->att);
        else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = v->z2 * v->env;
        break; }
    case T_BELL: {
        v->ph  += v->f0 * dt;        WRAP(v->ph);
        v->ph2 += v->f0 * 3.5f * dt; WRAP(v->ph2);
        x = sinf(TWO_PI * v->ph + 4.0f * v->env2 * sinf(TWO_PI * v->ph2)) * v->env;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_SWEEP: {
        float fcx = 300.0f + 7500.0f * v->env2 * fm;
        float xx = TWO_PI * fcx * dt, a = xx / (1.0f + xx);
        v->z1 += a * (noise(s) - v->z1);
        v->z2 += a * (v->z1 - v->z2);
        x = v->z2 * v->env * 3.0f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_IMPACT: {
        float f = v->f0 * fm + 90.0f * fm * v->pe;
        v->pe *= v->pek;
        v->ph += f * dt; WRAP(v->ph);
        v->z1 += 0.25f * (noise(s) - v->z1);
        x = sinf(TWO_PI * v->ph) * v->env * 0.9f + v->z1 * v->env2 * 1.5f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_SLEAD: case T_SBASS: case T_SSUPER: case T_SEPIANO: case T_SORGAN: case T_SSTRINGS: case T_SFMBASS: case T_SHOOVER: {
        const int b = BASE_T[v->type];
        const float f = v->f0, d = v->sdet;
        float osc = 0.0f;
        if (b == T_SLEAD || b == T_SBASS) {
            v->ph  += f * (1.0f + d) * dt; WRAP(v->ph);
            v->ph2 += f * (1.0f - d) * dt; WRAP(v->ph2);
            v->ph3 += f * 0.5f * dt;       WRAP(v->ph3);
            osc = (wf(v->ph, v->sw) + wf(v->ph2, v->sw)) * 0.5f + sinf(TWO_PI * v->ph3) * v->ssub;
            float fc = (b == T_SBASS ? 180.0f + 3200.0f * v->env2 * v->env2 : 6500.0f);
            float xx = TWO_PI * fc * dt, a = xx / (1.0f + xx);
            v->z1 += a * (osc - v->z1); v->z2 += a * (v->z1 - v->z2);
            osc = v->z2;
        } else if (b == T_SSUPER || b == T_SSTRINGS || b == T_SHOOVER) {
            float pm = 1.0f;
            if (b == T_SHOOVER) { pm = 1.0f + 0.6f * v->pe; v->pe *= v->pek; }
            static const float SP[7] = { -1.0f, -0.66f, -0.33f, 0.0f, 0.33f, 0.66f, 1.0f };
            v->ph += f * pm * dt; WRAP(v->ph);
            osc = wf(v->ph, v->sw);
            for (int k = 0; k < 6; k++) {
                v->ph6[k] += f * pm * (1.0f + d * SP[k < 3 ? k : k + 1]) * dt;
                if (v->ph6[k] >= 1.0f) v->ph6[k] -= 1.0f;
                osc += wf(v->ph6[k], v->sw);
            }
            osc *= (1.0f / 3.5f);
            v->ph3 += f * 0.5f * dt; WRAP(v->ph3);
            osc += sinf(TWO_PI * v->ph3) * v->ssub;
            float fc = (b == T_SSTRINGS) ? 2600.0f : 7500.0f;
            float xx = TWO_PI * fc * dt, a = xx / (1.0f + xx);
            v->z1 += a * (osc - v->z1); v->z2 += a * (v->z1 - v->z2);
            osc = v->z2;
        } else if (b == T_SEPIANO || b == T_SFMBASS) {
            const float ratio = (b == T_SFMBASS) ? 1.0f : (v->sw == 2 ? 2.0f : (v->sw == 3 ? 3.0f : (v->sw == 5 ? 7.0f : 1.0f)));
            const float idx = (b == T_SFMBASS ? 3.5f : 1.6f + d * 60.0f) * v->env2;
            v->ph  += f * dt;          WRAP(v->ph);
            v->ph2 += f * ratio * dt;  WRAP(v->ph2);
            v->ph3 += f * 0.5f * dt;   WRAP(v->ph3);
            if (v->sw >= 100) { float pp_ = v->ph + idx * 0.15915f * sinf(TWO_PI * v->ph2); pp_ -= floorf(pp_); osc = wt_at(pp_, v->sw - 100) * 1.2f + sinf(TWO_PI * v->ph3) * v->ssub; }
            else osc = sinf(TWO_PI * v->ph + idx * sinf(TWO_PI * v->ph2)) + sinf(TWO_PI * v->ph3) * v->ssub;
            osc *= 0.7f;
        } else {                                      /* organ: drawbars 16' 8' 4' 2 2/3' */
            v->ph  += f * dt;        WRAP(v->ph);
            v->ph2 += f * 2.0f * dt; WRAP(v->ph2);
            v->ph3 += f * 3.0f * dt; WRAP(v->ph3);
            v->z3  += f * 0.5f * dt; WRAP(v->z3);
            v->z4  += f * 4.0f * dt; WRAP(v->z4);
            osc = (v->sw >= 100 ? wt_at(v->ph, v->sw - 100) : sinf(TWO_PI * v->ph)) * 0.7f + sinf(TWO_PI * v->ph2) * 0.5f + sinf(TWO_PI * v->ph3) * (0.2f + d * 8.0f)
                + sinf(TWO_PI * v->z3) * v->ssub * 1.4f + sinf(TWO_PI * v->z4) * 0.25f;
            osc *= 0.55f;
        }
        if (T->sus) {
            float at = v->satt > 0.0005f ? v->satt : 0.0005f;
            if (v->gate) v->env += (1.0f - v->env) * (dt / at);
            else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        } else {
            v->env *= v->c1; if (v->env < 1e-3f) v->active = 0;
        }
        v->env2 *= v->c2;
        x = osc * v->env;
        break; }
    case T_SAMPLE: {
        const smp_t *m = smp_get(v->sidx);
        if (!m) { v->active = 0; break; }
        const int rev = v->smode == 1, loop = v->smode == 2;
        double pos = v->sp; int i0 = (int)pos; float fr = (float)(pos - (double)i0);
        int i1 = rev ? i0 - 1 : i0 + 1;
        if (i0 < 0) i0 = 0; if (i0 >= m->n) i0 = m->n - 1;
        if (i1 < 0) i1 = 0; if (i1 >= m->n) i1 = m->n - 1;
        float a, b;
        if (v->stch) {                                          /* time-stretch: two overlapping Hann grains, the read position moves at 1/stretch, pitch stays */
            const int G = 2048; const float fmv = fm * v->srate; x = 0.0f;
            for (int k = 0; k < 2; k++) {
                if (v->gp[k] < 0.0f) { v->gp[k] += 1.0f; if (v->gp[k] >= 0.0f) v->g0[k] = v->ga; continue; }
                if (v->gp[k] >= (float)G) { v->gp[k] = -(float)(G / 2); v->g0[k] = v->ga; continue; }
                double q = v->g0[k] + (double)v->gp[k] * (double)fmv; int j = (int)q; float fq = (float)(q - (double)j);
                if (j < 0) j = 0; if (j >= m->n - 1) j = m->n - 2; if (j < 0) j = 0;
                float aa, bb;
                if (m->ch == 2) { aa = (float)(m->d[j * 2] + m->d[j * 2 + 1]) * 0.5f; bb = (float)(m->d[(j + 1) * 2] + m->d[(j + 1) * 2 + 1]) * 0.5f; }
                else { aa = (float)m->d[j]; bb = (float)m->d[j + 1 < m->n ? j + 1 : j]; }
                float w = 0.5f - 0.5f * cosf(6.2831853f * v->gp[k] / (float)G);
                x += (aa + (bb - aa) * fq) * (1.0f / 32768.0f) * w;
                v->gp[k] += 1.0f;
            }
            v->ga += (double)(m->sr * s->inv_sr) / (double)v->stg;
            pos = v->ga; v->sp = pos; i0 = (int)pos;
            if (v->ga >= (double)v->s1) { if (loop && v->gate) v->ga -= (double)(v->s1 - v->s0); else { v->active = 0; } }
            goto smp_post;
        }
        if (m->ch == 2) { a = (float)(m->d[i0 * 2] + m->d[i0 * 2 + 1]) * 0.5f; b = (float)(m->d[i1 * 2] + m->d[i1 * 2 + 1]) * 0.5f; }
        else { a = (float)m->d[i0]; b = (float)m->d[i1]; }
        x = (a + (b - a) * fr) * (1.0f / 32768.0f);
        smp_post:;
        if (loop && v->gate) {                                  /* crossfaded loop: blend the material before the loop start into the loop end */
            int xf = (int)(0.012f * m->sr), ll = v->s1 - v->s0; if (xf > ll / 4) xf = ll / 4;
            if (xf > 8 && pos > (double)(v->s1 - xf)) {
                float w = (float)((pos - (double)(v->s1 - xf)) / (double)xf);
                int j0 = (int)(pos - (double)ll); if (j0 < 0) j0 = 0; if (j0 >= m->n) j0 = m->n - 1;
                float o = m->ch == 2 ? (float)(m->d[j0 * 2] + m->d[j0 * 2 + 1]) * 0.5f : (float)m->d[j0];
                x = x * (1.0f - w) + (o * (1.0f / 32768.0f)) * w;
            }
        }
        x *= v->sgain;
        if (v->sgate_env < 1.0f) { v->sgate_env += v->satk_inc; if (v->sgate_env > 1.0f) v->sgate_env = 1.0f; x *= v->sgate_env; }
        if (!v->gate && v->srel_c > 0.0f) { v->env *= v->srel_c; }
        float dend = rev ? (float)(pos - (double)v->s0) : (float)((double)v->s1 - pos);
        if (dend < 48.0f) x *= dend < 0.0f ? 0.0f : dend / 48.0f;
        if (v->t < 0.0015f) x *= v->t / 0.0015f;
        x *= v->env;
        v->env *= v->c1;
        if (loop && !v->gate && v->srel_c <= 0.0f) v->env *= 0.992f;
        if (v->env < 1e-3f) v->active = 0;
        double step = v->stch ? 0.0 : (double)(fm * v->srate);
        if (rev) { pos -= step; if (pos < (double)v->s0) { if (loop && v->gate) pos += (double)(v->s1 - v->s0); else v->active = 0; } }
        else { pos += step; if (pos >= (double)v->s1) { if (loop && v->gate) pos -= (double)(v->s1 - v->s0); else v->active = 0; } }
        v->sp = pos;
        break; }
    case T_E_MODAL: case T_E_FM: case T_E_VOX: case T_E_CHIP: case T_E_MORPH: x = nx_render(s, v); break;
    default: v->active = 0; break;
    }
    v->t += dt;
    return x;
}

/* ---------------- arp ---------------- */
static int arp_offset(inst_t *s, int p, int n) {
    int arp = (int)(s->pp[p][PP_ARP] + 0.5f);
    const int *tb = SCALE_TBL[(int)(s->g[G_SCALE] + 0.5f) % 3];
    switch (arp) {
    case 1: return tb[n % 6];
    case 2: return tb[5 - (n % 6)];
    case 3: { int k = n % 10; return tb[k < 6 ? k : 10 - k]; }
    case 4: { s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5; return tb[(s->rng >> 8) % 6]; }
    default: return 0;      /* Roll */
    }
}

static float arp_step_samples(inst_t *s, int p) {
    int r = (int)(s->pp[p][PP_RATE] + 0.5f);
    if (r < 0) r = 0; if (r > 3) r = 3;
    return s->sr * 60.0f / s->bpm / RATE_SPB[r];
}

static float rep_step_samples(inst_t *s) { int r = s->rep_idx; if (r < 0 || r > 15) r = 8; return s->sr * 60.0f / s->bpm / RPT_SPB[r]; }

static void pad_on(inst_t *s, int p, int vel) {
    float v = (float)vel / 127.0f;
    ar_t *a = &s->ar[p];
    int arp = (int)(s->pp[p][PP_ARP] + 0.5f);
    a->held = 1; a->vel = v; a->base = 0; a->key = -1;
    if (arp > 0) {
        a->on = 1; a->n = 0;
        trig(s, p, v, arp_offset(s, p, 0), 1);
        a->n = 1;
        a->cnt = arp_step_samples(s, p);
    } else {
        a->on = 0;
        trig(s, p, v, 0, 1);
    }
}

static void pad_off(inst_t *s, int p) {
    s->ar[p].held = 0; s->ar[p].on = 0;
    s->vc[p].gate = 0;
}

/* ---------------- sequencer ---------------- */
static const int SCALE7[3][7] = { { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 11 } };
static inline int prob_idx(int b) { return (b & 0x80) ? (b & 0x1F) : ((((b & 7) << 1) | ((b >> 3) & 1)) << 1); }       /* 32 levels (3.1 % each); older saves stored 8 or 16 levels */

static int note_pad_semi(inst_t *s, int d) {          /* d = 0..31 scale degree; 32..95 = chromatic semitone (d-32) */
    if (d >= 32) return d - 32 - 12;
    int sc = (int)(s->g[G_SCALE] + 0.5f) % 3;
    return 12 * (d / 7) + SCALE7[sc][d % 7] - 12;
}

/* a clip becomes the active clip of track t: switch to the sound stored for it (volume + pan stay track-wide) */
static void clip_activate_lane(inst_t *s, int l, int c) {
    if (c >= 0 && c < NPAT && s->cpp_set[c][l]) {
        for (int k = 0; k < PP_N; k++) if (k != PP_VOL && k != PP_PAN) s->pp[l][k] = s->cpp[c][l][k];
        if (s->plk_t[l]) s->plk_b[l] = s->pp[l][s->plk_t[l]];
        update_pad(s, l);
    }
}
static void clip_activate(inst_t *s, int t, int c) {
    s->clip[t] = c;
    if (s->isdrum[t]) { int d = track_slot(s, t); if (d >= 0) for (int k = 0; k < 16; k++) clip_activate_lane(s, NTRK + d * 16 + k, c); }
    else clip_activate_lane(s, t, c);
}
static void apply_pending(inst_t *s) {
    for (int t = 0; t < NTRK; t++) if (s->qclip[t] != -2) {
        clip_activate(s, t, s->qclip[t]); s->qclip[t] = -2;
        if (s->clip[t] < 0) for (int l = 0; l < NLANE; l++) if (lane_on(s, l) && lane_trk(s, l) == t) { s->gate_left[l] = 0; s->vc[l].gate = 0; }
    }
}
static int clip_has(const inst_t *s, int c, int t) {
    if (s->isdrum[t]) {
        int d = track_slot(s, t); if (d < 0) return 0;
        for (int k = 0; k < 16; k++) for (int i = 0; i < MAXSTEP; i++) if (s->stp[c][NTRK + d * 16 + k][i][0] & 0x80) return 1;
        return 0;
    }
    for (int i = 0; i < MAXSTEP; i++) if (s->stp[c][t][i][0] & 0x80) return 1;
    return 0;
}
static void mo_send(uint8_t cin, uint8_t st, uint8_t d1, uint8_t d2) {
    if (!g_host || !g_host->midi_send_external) return;
    uint8_t m[4] = { (uint8_t)(0x20 | cin), st, d1, d2 };
    g_host->midi_send_external(m, 4);
}
static void mo_note(inst_t *s, int lane, int trk, float vel, int semi, int gate) {
    int note = lane >= NTRK ? 36 + s->mo_dn + ((lane - NTRK) & 15) : 60 + s->mo_xp + semi; if (note < 0) note = 0; if (note > 127) note = 127;
    int v = (int)(vel * (100 + s->mo_vp) * 1.27f + 0.5f); if (v < 1) v = 1; if (v > 127) v = 127;
    uint8_t ch = (uint8_t)((trk + s->mo_chn) & 15);
    mo_send(0x9, (uint8_t)(0x90 | ch), (uint8_t)note, (uint8_t)v);
    for (int i = 0; i < 64; i++) if (s->mo_q[i].cnt <= 0) { s->mo_q[i].cnt = gate > 2000 ? gate : 2000; s->mo_q[i].st = (uint8_t)(0x80 | ch); s->mo_q[i].d1 = (uint8_t)note; return; }
    mo_send(0x8, (uint8_t)(0x80 | ch), (uint8_t)note, 0);
}
static void rt_fire(inst_t *s, int t) {
    if (!s->mute[lane_trk(s, t)]) {
        ar_t *ap = &s->ar[t];
        if (t < NTRK && s->rt_n[t] == 1 && (int)(s->pp[t][PP_ARP] + 0.5f) > 0) {      /* ARP on a sequenced step: it runs for the length of the note */
            ap->on = 1; ap->n = 1; ap->base = s->rt_semi[t]; ap->key = -2; ap->vel = s->rt_vel[t]; ap->cnt = arp_step_samples(s, t);
            ap->life = (float)(s->rt_gate[t] > 64 ? s->rt_gate[t] : 64);
            trig(s, t, s->rt_vel[t], s->rt_semi[t] + arp_offset(s, t, 0), 1);
        } else {
            if (ap->key == -2) { ap->on = 0; ap->key = -1; }
            trig(s, t, s->rt_vel[t], s->rt_semi[t], 1);
        }
        { const int tk = lane_trk(s, t); if (s->mo_trk & (1 << tk)) mo_note(s, t, tk, s->rt_vel[t], s->rt_semi[t], s->rt_gate[t]); }
        if (TY[s->vc[t].type].sus) s->gate_left[t] = s->rt_gate[t] > 0 ? s->rt_gate[t] : 1;
    }
    s->rt_vel[t] *= 0.88f;
    if (--s->rt_n[t] > 0) s->rt_cnt[t] += s->rt_int[t];
}

static void seq_step(inst_t *s) {
    const float L = s->sr * 60.0f / s->bpm / 4.0f;
    {
        float qv = s->g[G_QNT];
        static const int QM[7] = { 0, 1, 3, 7, 15, 31, 63 };   /* 1/16 1/8 1/4 1/2 bar 2bar 4bar */
        int qi = (int)(qv * 7.0f); if (qi < 0) qi = 0; if (qi > 6) qi = 6;
        int qm = QM[qi];
        if ((s->gs & qm) == 0) apply_pending(s);
    }
    if (s->rec_state == 1 && (s->gs & 15) == 0) { s->rec_state = 2; s->rec_n = 0; s->rec_len = (long)((float)(s->rec_bars * 16) * L); if (s->rec_len > s->rec_max) s->rec_len = s->rec_max; }
    if (s->g[G_MET] > 0.01f && (s->gs & 3) == 0) { s->met_env = 1.0f; s->met_ph = 0.0f; s->met_f = ((s->gs & 15) == 0) ? 1700.0f : 1100.0f; }
    for (int t = 0; t < NLANE; t++) {                              /* t = lane */
        if (!lane_on(s, t)) continue;
        const int tk = lane_trk(s, t);
        int len = s->llen[t] > 0 ? s->llen[t] : s->seqlen[tk]; if (len < 1) len = 1; if (len > MAXSTEP) len = MAXSTEP;
        if (s->clip[tk] < 0) continue;
        const uint8_t *st = s->stp[s->clip[tk]][t][s->gs % len];
        if (!(st[0] & 0x80) || s->mute[tk] || s->lmute[t]) continue;
        if (prob_idx(st[1]) > 0) {
            s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
            if ((float)(s->rng & 0xFFFF) / 65536.0f > 1.0f - 0.03125f * (float)prob_idx(st[1])) continue;
        }
        {   /* condition: play only on certain passes of the loop (1:2, 2:2, 1:3 ... 4:4) */
            static const int CN[8] = { 1, 2, 2, 3, 3, 3, 4, 4 }, CK[8] = { 0, 0, 1, 0, 1, 2, 0, 3 };
            int cd = (st[3] >> 5) & 7;
            if (cd && ((s->gs / len) % CN[cd]) != CK[cd]) continue;
        }
        if (s->plk_t[t]) { const int lv = st[5]; s->pp[t][s->plk_t[t]] = lv ? (float)(lv - 1) / 126.0f : s->plk_b[t]; }       /* parameter lock: this step's value, or back to the base */
        int semi = (int)st[2] - 64;
        float hv = (float)(st[0] & 0x7F) / 127.0f;
        if (s->g[G_HUM] > 0.01f) {
            s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
            hv *= 1.0f + s->g[G_HUM] * 0.6f * ((float)(s->rng & 0xFFFF) / 65536.0f - 0.5f);
            if (hv > 1.0f) hv = 1.0f; if (hv < 0.05f) hv = 0.05f;
        }
        {
            static const float GT[8] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 8.0f };
            int n = 1 + (st[3] & 3), gi = (st[3] >> 2) & 7;
            float iv = L / (float)n;
            float gs_ = gi == 0 ? 0.8f * iv : GT[gi] * L;
            if (n > 1 && gs_ > 0.9f * iv) gs_ = 0.9f * iv;
            s->rt_n[t] = n; s->rt_int[t] = iv; s->rt_semi[t] = semi; s->rt_vel[t] = hv; s->rt_gate[t] = (int)gs_;
            s->rt_cnt[t] = (float)((st[4] & 0x40) ? (st[4] & 0x3F) : (st[4] & 7) * 4) * 0.025f * L;      /* nudge: delay up to 70 % of a step in 2.5 % steps (older saves: 10 % steps) */
            if ((s->gs & 1) && s->pp[tk][PP_TSW] > 0.52f) s->rt_cnt[t] += (s->pp[tk][PP_TSW] - 0.5f) * 0.5f * L;   /* per-track extra swing on the off-beat steps */
            if (s->rt_cnt[t] <= 0.0f) rt_fire(s, t);
        }
    }
    float off = (2.0f * s->swing - 1.0f) * L;
    s->last_time = s->next_time;
    s->next_time += (double)(L + (((s->gs & 1) == 0) ? off : -off));
    s->gs++;
}

static void seq_start(inst_t *s) { if (s->mo_clk) { mo_send(0xF, 0xFA, 0, 0); s->mclk = 0.0f; } s->playing = 1; s->tsamp = 0.0; s->next_time = 0.0; s->last_time = 0.0; s->gs = 0; s->pump_ph = 0.0f; s->wob_ph = 0.0f; }
static void seq_stop(inst_t *s) {
    if (s->playing && s->mo_clk) mo_send(0xF, 0xFC, 0, 0);
    for (int i = 0; i < 64; i++) if (s->mo_q[i].cnt > 0) { mo_send(0x8, s->mo_q[i].st, s->mo_q[i].d1, 0); s->mo_q[i].cnt = 0; }
    s->playing = 0; s->gs = 0;
    for (int t = 0; t < NTRK; t++) if (s->qclip[t] != -2) { clip_activate(s, t, s->qclip[t]); s->qclip[t] = -2; }
    for (int t = 0; t < NLANE; t++) { s->gate_left[t] = 0; s->rt_n[t] = 0; s->vc[t].gate = 0; if (s->ar[t].key == -2) { s->ar[t].on = 0; s->ar[t].key = -1; } }
    for (int i = 0; i < 32; i++) s->eq[i].cnt = 0;
}

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0)); }
static void put_step(inst_t *s, int pat, int t, int st, const char *h, int w) {
    if (pat < 0 || pat >= NPAT || t < 0 || t >= NLANE || st < 0 || st >= MAXSTEP) return;
    s->stp[pat][t][st][0] = (uint8_t)((hexv(h[0]) << 4) | hexv(h[1]));
    s->stp[pat][t][st][1] = (uint8_t)((hexv(h[2]) << 4) | hexv(h[3]));
    s->stp[pat][t][st][2] = (uint8_t)((hexv(h[4]) << 4) | hexv(h[5]));
    s->stp[pat][t][st][3] = (uint8_t)((hexv(h[6]) << 4) | hexv(h[7]));
    s->stp[pat][t][st][4] = (uint8_t)(((hexv(h[8]) << 4) | hexv(h[9])) & 0x7F);
    s->stp[pat][t][st][5] = w >= 12 ? (uint8_t)(((hexv(h[10]) << 4) | hexv(h[11])) & 0x7F) : 0;
}

/* a pad event from Move (or the UI): 0..15 = track sound, 16..31 = note pads for the selected track */
static void pad_event(inst_t *s, int idx, int vel) {
    if (idx < 0 || idx >= NPADS) return;
    if (idx < NTRK) { if (vel > 0) pad_on(s, idx, vel); else pad_off(s, idx); return; }
    int k = idx - NTRK;
    if (vel > 0) {
        int t = s->sel_track;
        s->np_trk[k] = t;
        trig(s, t, (float)vel / 127.0f, note_pad_semi(s, k) + s->pad_oct, 1);
        s->ar[t].held = 1;
    } else {
        int t = s->np_trk[k];
        if (t >= 0 && t < NLANE) {
            if (s->vc[t].gk == k + 1 || s->vc[t].gk == 0) s->vc[t].gate = 0;
            for (int i = 0; i < NGH; i++) if (s->vc[NLANE + i].active && s->vown[NLANE + i] == t && s->vc[NLANE + i].gk == k + 1) s->vc[NLANE + i].gate = 0;
        }
    }
}

/* pad k (0..31) plays the selected track chromatically in the scale; repeat = retrigger while held at the track's rate */
static void sel_pad(inst_t *s, int k, int vel, int repeat) {
    if (k < 0 || k >= 96) return;
    int kk = k & 31;
    if (vel > 0) {
        int t = s->sel_track; if (t < 0 || t >= NLANE) t = 0;
        s->np_trk[kk] = t;
        int sm = note_pad_semi(s, k) + s->pad_oct;
        ar_t *ap = &s->ar[t];
        if (!repeat && (int)(s->pp[t][PP_ARP] + 0.5f) > 0) {               /* ARP: a held note pad steps through the scale at the Rate */
            ap->on = 1; ap->n = 1; ap->base = sm; ap->key = kk; ap->vel = (float)vel / 127.0f; ap->cnt = arp_step_samples(s, t);
            trig(s, t, (float)vel / 127.0f, sm + arp_offset(s, t, 0), 1);
        } else {
            if (ap->key >= 0) { ap->on = 0; ap->key = -1; }
            trig(s, t, (float)vel / 127.0f, sm, 1);
        }
        s->vc[t].gk = k + 1;
        s->ar[t].held = 1;
        if (repeat) { s->rep_on = 1; s->rep_t = t; s->rep_k = k; s->rep_semi = (float)sm; s->rep_vel = (float)vel / 127.0f; s->rep_cnt = rep_step_samples(s); }
    } else {
        if (s->rep_on && s->rep_k == k) s->rep_on = 0;
        int t = s->np_trk[kk];
        if (t >= 0 && t < NLANE) {
            if (s->ar[t].key == kk) { s->ar[t].on = 0; s->ar[t].key = -1; }
            if (s->vc[t].gk == k + 1 || s->vc[t].gk == 0) s->vc[t].gate = 0;      /* POLY: letting go of an older key must not cut the newer note ... */
            for (int i = 0; i < NGH; i++) if (s->vc[NLANE + i].active && s->vown[NLANE + i] == t && s->vc[NLANE + i].gk == k + 1) s->vc[NLANE + i].gate = 0;   /* ... and its ringing ghost voice has to be released too (it used to hang) */
        }
    }
}

/* drum pad: play one sound lane directly (optionally with note repeat while held) */
static void lane_hit(inst_t *s, int l, int vel, int repeat) {
    if (l < 0 || l >= NLANE) return;
    if (vel > 0) {
        trig(s, l, (float)vel / 127.0f, 0, 1);
        s->vc[l].gk = 99;
        if (repeat) { s->rep_on = 1; s->rep_t = l; s->rep_k = 100 + l; s->rep_semi = 0.0f; s->rep_vel = (float)vel / 127.0f; s->rep_cnt = rep_step_samples(s); }
    } else {
        if (s->rep_on && s->rep_k == 100 + l) s->rep_on = 0;
        s->vc[l].gate = 0;
    }
}

/* ---------------- focus pairing ---------------- */
#define PAIR_WINDOW 8820u    /* 200 ms */
static void set_focus(inst_t *s, int p) { s->focused = p + 1; }

__attribute__((unused)) static void note_seen(inst_t *s, int p) {
    if (s->has_vouch && (s->clock - s->pend_vouch_t) < PAIR_WINDOW) {
        set_focus(s, p); s->has_vouch = 0; s->pend_pad = -1;
    } else { s->pend_pad = p; s->pend_note_t = s->clock; }
}
static void vouch_seen(inst_t *s) {
    if (s->pend_pad >= 0 && (s->clock - s->pend_note_t) < PAIR_WINDOW) {
        set_focus(s, s->pend_pad); s->pend_pad = -1; s->has_vouch = 0;
    } else { s->has_vouch = 1; s->pend_vouch_t = s->clock; }
}

/* ---------------- plugin API ---------------- */
static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir; (void)json_defaults;
    for (int i = 0; i < MAX_INST; i++) {
        if (!g_pool[i].in_use) {
            inst_t *s = &g_pool[i];
            memset(s, 0, sizeof(*s));
            s->in_use = 1;
            for (int t = 0; t < NTRK; t++) { s->clip[t] = 0; s->qclip[t] = -2; }
            s->sr = (float)g_sr;
            s->inv_sr = 1.0f / s->sr;
            s->rng = 0x2545F491u + (uint32_t)i * 7919u;
            s->focused = 1;
            s->pend_pad = -1;
            s->rec_src = -1; s->rec_slot = -1;
            for (int k = 0; k < G_N; k++) s->g[k] = GD[k].def; s->lim_g = 1.0f;
            s->bpm = 120.0f; s->swing = 0.5f; s->rep_idx = 8;
            for (int t = 0; t < NTRK; t++) { s->seqlen[t] = 16; s->tg[t] = 1.0f; }
            for (int d = 0; d < 4; d++) s->dtrk[d] = -1;
            for (int k = 0; k < 32; k++) s->np_trk[k] = -1;
            init_static(s);
            update_global(s);
            for (int p = 0; p < NLANE; p++) reset_pad(s, p);
            smp_init();
            return s;
        }
    }
    return NULL;
}

static void destroy_instance(void *instance) {
    inst_t *s = (inst_t *)instance;
    if (s) s->in_use = 0;
}

static int note_to_pad(int note) {
    if (note >= 68 && note <= 99) return note - 68;          /* Move pad ids (overtake) */
    if (note >= PAD_BASE && note < PAD_BASE + NPADS) return note - PAD_BASE;  /* track notes */
    return -1;
}

static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)source;
    inst_t *s = (inst_t *)instance;
    if (!s || len < 3) return;
    uint8_t st = msg[0] & 0xF0;
    if (st != 0x90 && st != 0x80) return;
    int p = note_to_pad(msg[1]);
    if (p < 0) return;
    int vel = (st == 0x90 && msg[2] > 0) ? msg[2] : 0;
    if (msg[1] >= 68 && msg[1] <= 99) {                                 /* a Move pad: the same layout as the UI (MOVE rows 3 degrees apart, SEQ, OCT, CHR) */
        const int col = p & 7, row = p >> 3;
        p = s->lay_s == 3 ? 32 + col + 5 * row : (s->lay_s == 0 ? col + 3 * row : (s->lay_s == 1 ? row * 8 + col : col + 7 * row));
    }
    if (s->pads_off == 1) { if (!vel) sel_pad(s, p, 0, 0); return; }     /* UI owns the pads (session, steps, menus) */
    sel_pad(s, p, vel, s->pads_off == 3);
}

static int parse_pad_key(const char *key, int *pad, const char **name) {
    if (key[0] != 'p' || key[1] < '0' || key[1] > '9' || key[2] < '0' || key[2] > '9' || key[3] != '_') return 0;
    int n = (key[1] - '0') * 10 + (key[2] - '0');
    if (n < 1 || n > NLANE) return 0;
    *pad = n - 1; *name = key + 4;
    return 1;
}

static int enum_index(const char *val, const char *const *names, int n) {
    if (val[0] >= '0' && val[0] <= '9') { int k = atoi(val); return k < 0 ? 0 : (k >= n ? n - 1 : k); }
    for (int i = 0; i < n; i++) if (strcmp(val, names[i]) == 0) return i;
    return -1;
}

static const char *const *enum_names(int kind) {
    (void)kind; return NULL;
}

static void set_pad_param(inst_t *s, int p, const char *name, const char *val) {
    for (int k = 0; k < PP_N; k++) {
        if (strcmp(name, PD[k].key) != 0) continue;
        float v;
        if (k == PP_TYPE)      { int e = enum_index(val, TYPE_NAME, T_COUNT); if (e < 0) return; v = (float)e; }
        else if (k == PP_ARP)  { int e = enum_index(val, ARP_NAME, 6);        if (e < 0) return; v = (float)e; }
        else if (k == PP_RATE) { int e = enum_index(val, RATE_NAME, 4);       if (e < 0) return; v = (float)e; }
        else if (k == PP_WAVE) { int e = enum_index(val, WAVE_NAME, 16);      if (e < 0) return; v = (float)e; }
        else if (k == PP_POLY) { int e = enum_index(val, POLY_NAME, 2);       if (e < 0) return; v = (float)e; }
        else if (k == PP_DTIME) { int e = enum_index(val, DT_NAME, 5);        if (e < 0) return; v = (float)e; }
        else if (k == PP_XFX || k == PP_MFX)  { v = clampf((float)atof(val), 0.0f, 10.0f); }
        else v = clampf((float)atof(val), 0.0f, 1.0f);
        s->pp[p][k] = v;
        if (s->plk_t[p] == k) s->plk_b[p] = v;
        update_pad(s, p);
        return;
    }
}

static void set_param(void *instance, const char *key, const char *val) {
    inst_t *s = (inst_t *)instance;
    (void)enum_names;
    if (!s || !key || !val) return;
    int p; const char *nm;
    if (parse_pad_key(key, &p, &nm)) { set_pad_param(s, p, nm, val); return; }

    if (strcmp(key, "focused_pad") == 0) {
        int n = atoi(val);
        if (n >= 1 && n <= NLANE) s->focused = n;
        return;
    }
    if (strcmp(key, "live_press") == 0) { if (val[0] == '1') vouch_seen(s); return; }
    if (strcmp(key, "reset_pad") == 0) { reset_pad(s, s->focused - 1); return; }
    if (strcmp(key, "pad") == 0) {                 /* "idx,vel" from the UI (vel 0 = off) */
        int idx = atoi(val); const char *c = strchr(val, ',');
        pad_event(s, idx, c ? atoi(c + 1) : 100);
        return;
    }
    if (strcmp(key, "cpureset") == 0) { s->cpu_peak = 0.0f; return; }
    if (strcmp(key, "smprescan") == 0) { smp_scan(); return; }
    if (strcmp(key, "brdir") == 0) { br_list(val); return; }
    if (strcmp(key, "brimport") == 0) { br_import(atoi(val)); return; }
    if (strcmp(key, "brprev") == 0) { br_res = br_preview(atoi(val)) ? 1 : -1; return; }
    if (strcmp(key, "brstop") == 0) { pv_on = 0; return; }
    if (strcmp(key, "recarm") == 0) {                /* "src,bars": src -1 = master, 0..15 = one track */
        if (s->rec_state == 1 || s->rec_state == 2) return;
        int src = atoi(val); const char *c = strchr(val, ','); int bars = c ? atoi(c + 1) : 2;
        if (bars < 1) bars = 1; if (bars > 16) bars = 16; if (src < -1 || src >= NTRK) src = -1;
        s->rec_max = (long)(s->sr * 40.0f);
        if (!s->rec_buf) s->rec_buf = (int16_t *)malloc((size_t)s->rec_max * 4);
        if (!s->rec_buf) return;
        s->rec_bars = bars; s->rec_src = src; s->rec_n = 0; s->rec_state = 1;
        return;
    }
    if (strcmp(key, "reccancel") == 0) { s->rec_state = 0; return; }
    if (strcmp(key, "midiout") == 0) { s->mo_clk = atoi(val) ? 1 : 0; const char *c = strchr(val, ','); s->mo_trk = c ? (atoi(c + 1) & 0xFFFF) : 0;
        s->mo_chn = 0; s->mo_xp = 0; s->mo_vp = 0; s->mo_dn = 0;
        if (c) { const char *q = strchr(c + 1, ','); int f = 0; while (q && f < 4) { int v = atoi(q + 1);
            if (f == 0) s->mo_chn = v < 0 ? 0 : (v > 15 ? 15 : v); else if (f == 1) s->mo_xp = v < -24 ? -24 : (v > 24 ? 24 : v);
            else if (f == 2) s->mo_vp = v < -75 ? -75 : (v > 50 ? 50 : v); else s->mo_dn = v < -12 ? -12 : (v > 36 ? 36 : v);
            f++; q = strchr(q + 1, ','); } }
        return; }
    if (strcmp(key, "recsave") == 0) {               /* write the take as a WAV file into the sample folder and load it */
        if (s->rec_state != 3 || !s->rec_buf || s->rec_n < 64) { s->rec_state = 0; return; }
        char name[48], path[400]; int k;
        for (k = 1; k < 1000; k++) { snprintf(name, sizeof name, "resample_%03d.wav", k); snprintf(path, sizeof path, "%s/%s", SMP_DIR, name); FILE *t = fopen(path, "rb"); if (!t) break; fclose(t); }
        FILE *f = fopen(path, "wb");
        if (f) {
            uint32_t dl = (uint32_t)(s->rec_n * 4), sr = (uint32_t)s->sr, br = sr * 4, rl = 36 + dl; uint16_t one = 1, two = 2, bits = 16, ba = 4; uint32_t fl = 16;
            fwrite("RIFF", 1, 4, f); fwrite(&rl, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fl, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&two, 2, 1, f);
            fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&dl, 4, 1, f);
            fwrite(s->rec_buf, 1, dl, f); fclose(f);
            smp_scan();
            s->rec_slot = -1; for (int i = 0; i < NSMP; i++) if (!strcmp(SMP[i].name, name)) s->rec_slot = i;
            snprintf(s->rec_last, sizeof s->rec_last, "%s", name);
        } else s->rec_slot = -1;
        s->rec_state = 0;
        return;
    }
    if (strcmp(key, "reprate") == 0) { int r = atoi(val); s->rep_idx = r < 0 ? 0 : (r > 15 ? 15 : r); return; }
    if (strcmp(key, "laysyn") == 0) { int v = atoi(val); s->lay_s = v < 0 ? 0 : (v > 3 ? 3 : v); return; }
    if (strcmp(key, "padoct") == 0) { int o = atoi(val); s->pad_oct = o < -36 ? -36 : (o > 36 ? 36 : o); return; }
    if (strcmp(key, "padsoff") == 0) { s->pads_off = atoi(val); if (s->pads_off == 1) s->rep_on = 0; return; }
    if (strcmp(key, "npad") == 0) {                /* "k,vel,rep" from the UI */
        int k = atoi(val); const char *c = strchr(val, ','); int vel = c ? atoi(c + 1) : 100; const char *c2 = c ? strchr(c + 1, ',') : NULL;
        sel_pad(s, k, vel, c2 ? atoi(c2 + 1) : 0);
        return;
    }
    if (strcmp(key, "panic") == 0) { for (int i = 0; i < NLANE + NGH; i++) { s->vc[i].active = 0; s->vc[i].gate = 0; } for (int t = 0; t < NLANE; t++) { s->ar[t].on = 0; s->ar[t].key = -1; } s->rep_on = 0; return; }
    if (strcmp(key, "play") == 0) { if (val[0] == '1') seq_start(s); else seq_stop(s); return; }
    if (strcmp(key, "bpm") == 0)  { s->bpm = clampf((float)atof(val), 40.0f, 240.0f); return; }
    if (strcmp(key, "swing") == 0){ s->swing = clampf((float)atof(val), 0.5f, 0.75f); return; }
    if (strcmp(key, "pat") == 0)  { int n = atoi(val); if (n >= 0 && n < NPAT) s->cur_pat = n; return; }
    if (strcmp(key, "clip") == 0) {                /* "track,clip" clip -1 = stop; quantised to the bar */
        int t = atoi(val); const char *c = strchr(val, ','); if (!c || t < 0 || t >= NTRK) return;
        int cl = atoi(c + 1); if (cl < -1 || cl >= NPAT) return;
        if (s->playing) s->qclip[t] = cl; else { clip_activate(s, t, cl); s->qclip[t] = -2; }
        return;
    }
    if (strcmp(key, "scene") == 0) {               /* launch clip n on every track that has one, stop the rest */
        int cl = atoi(val); if (cl < 0 || cl >= NPAT) return;
        for (int t = 0; t < NTRK; t++) {
            int target = clip_has(s, cl, t) ? cl : -1;
            if (s->playing) s->qclip[t] = target; else { clip_activate(s, t, target); s->qclip[t] = -2; }
        }
        return;
    }
    if (strcmp(key, "dhit") == 0) {                /* "lane,vel,repeat" - play a drum sound directly */
        int l = atoi(val); const char *c = strchr(val, ','); int vel = c ? atoi(c + 1) : 100; const char *c2 = c ? strchr(c + 1, ',') : NULL;
        lane_hit(s, l, vel, c2 ? atoi(c2 + 1) : 0);
        return;
    }
    if (strcmp(key, "kind") == 0) {                /* "track,slot" slot -1 = synth track, 0..3 = drum kit */
        int t = atoi(val); const char *c = strchr(val, ','); if (!c || t < 0 || t >= NTRK) return;
        int d = atoi(c + 1); if (d < -1 || d > 3) return;
        for (int j = 0; j < 4; j++) if (s->dtrk[j] == t && j != d) { for (int k = 0; k < 16; k++) s->vc[NTRK + j * 16 + k].active = 0; s->dtrk[j] = -1; }
        if (d >= 0) { if (s->dtrk[d] >= 0 && s->dtrk[d] != t) s->isdrum[s->dtrk[d]] = 0; s->dtrk[d] = t; s->isdrum[t] = 1; s->vc[t].active = 0; }
        else s->isdrum[t] = 0;
        return;
    }
    if (strcmp(key, "tvol") == 0) {                /* "track,gain" */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NTRK) s->tg[t] = clampf((float)atof(c + 1), 0.0f, 1.5f);
        return;
    }
    if (strcmp(key, "sel") == 0)  { int n = atoi(val); if (n >= 0 && n < NLANE) { if (n != s->sel_track) s->rep_on = 0; s->sel_track = n; } return; }
    if (strcmp(key, "len") == 0)  {                /* "track,length" */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NTRK) { int l = atoi(c + 1); if (l >= 1 && l <= MAXSTEP) s->seqlen[t] = l; }
        return;
    }
    if (strcmp(key, "llen") == 0) {                /* "lane,length" (0 = follow the track) */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NLANE) { int l = atoi(c + 1); if (l >= 0 && l <= MAXSTEP) s->llen[t] = l; }
        return;
    }
    if (strcmp(key, "mute") == 0) {                /* "track,0|1" */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NTRK) s->mute[t] = atoi(c + 1) ? 1 : 0;
        return;
    }
    if (strcmp(key, "lmute") == 0) {               /* "lane,0|1": mute one drum sound */
        int l = atoi(val); const char *c = strchr(val, ',');
        if (c && l >= 0 && l < NLANE) s->lmute[l] = atoi(c + 1) ? 1 : 0;
        return;
    }
    if (strcmp(key, "st") == 0) {                  /* "pat,track,step,hhhh" */
        int pt = atoi(val); const char *c1 = strchr(val, ','); if (!c1) return;
        int t = atoi(c1 + 1); const char *c2 = strchr(c1 + 1, ','); if (!c2) return;
        int st = atoi(c2 + 1); const char *c3 = strchr(c2 + 1, ','); if (!c3 || strlen(c3 + 1) < 10) return;
        put_step(s, pt, t, st, c3 + 1, strlen(c3 + 1) >= 12 ? 12 : 10);
        return;
    }
    if (strcmp(key, "seqc") == 0) {                /* "pat,track,offset:" + n x hhhh (chunk of a longer pattern) */
        int pt = atoi(val); const char *c1 = strchr(val, ','); if (!c1) return;
        int t = atoi(c1 + 1); const char *c1b = strchr(c1 + 1, ','); if (!c1b) return;
        int off = atoi(c1b + 1); const char *c2 = strchr(c1b + 1, ':'); if (!c2) return;
        const char *h = c2 + 1; int n = (int)(strlen(h) / 10);
        for (int i = 0; i < n && off + i < MAXSTEP; i++) put_step(s, pt, t, off + i, h + 10 * i, 10);
        return;
    }
    if (strcmp(key, "seqd") == 0) {                /* like seqc, 12 hex chars per step (the last byte = parameter lock) */
        int pt = atoi(val); const char *c1 = strchr(val, ','); if (!c1) return;
        int t = atoi(c1 + 1); const char *c1b = strchr(c1 + 1, ','); if (!c1b) return;
        int off = atoi(c1b + 1); const char *c2 = strchr(c1b + 1, ':'); if (!c2) return;
        const char *h = c2 + 1; int n = (int)(strlen(h) / 12);
        for (int i = 0; i < n && off + i < MAXSTEP; i++) put_step(s, pt, t, off + i, h + 12 * i, 12);
        return;
    }
    if (strcmp(key, "plkt") == 0) {                /* "lane,paramIndex": which lane parameter the step locks drive (0 = none) */
        int l = atoi(val); const char *c = strchr(val, ','); if (!c || l < 0 || l >= NLANE) return;
        int k = atoi(c + 1);
        if (s->plk_t[l] > 0) { s->pp[l][s->plk_t[l]] = s->plk_b[l]; update_pad(s, l); }       /* put the old target back to its base value */
        if (k < 1 || k >= PP_N || k == PP_TYPE || k == PP_ARP || k == PP_RATE || k == PP_WAVE || k == PP_POLY || k == PP_DTIME || k == PP_XFX || k == PP_MFX) k = 0;
        s->plk_t[l] = (uint8_t)k; if (k) s->plk_b[l] = s->pp[l][k];
        return;
    }
    if (strcmp(key, "reset") == 0) {               /* "NN" 1-based */
        int n = atoi(val);
        if (n >= 1 && n <= NLANE) reset_pad(s, n - 1);
        return;
    }
    if (strcmp(key, "csnd") == 0) {                /* "clip,track:" + PP_N values -> stored sound of that clip */
        int c = atoi(val); const char *c1 = strchr(val, ','); if (!c1) return;
        int t = atoi(c1 + 1); const char *c2 = strchr(c1 + 1, ':'); if (!c2 || c < 0 || c >= NPAT || t < 0 || t >= NLANE) return;
        const char *q = c2 + 1;
        for (int k = 0; k < PP_N && *q; k++) {
            char *end; float v = strtof(q, &end);
            if (end == q) break;
            if (k == PP_TYPE) v = clampf(v, 0.0f, (float)(T_COUNT - 1));
            else if (k == PP_ARP) v = clampf(v, 0.0f, 5.0f);
            else if (k == PP_RATE) v = clampf(v, 0.0f, 3.0f);
            else if (k == PP_WAVE) v = clampf(v, 0.0f, 15.0f);
            else if (k == PP_POLY) v = v > 0.5f ? 1.0f : 0.0f;
            else if (k == PP_DTIME) v = clampf(v, 0.0f, 4.0f);
            else if (k == PP_XFX || k == PP_MFX) v = clampf(v, 0.0f, 10.0f);
            else v = clampf(v, 0.0f, 1.0f);
            s->cpp[c][t][k] = v;
            q = (*end == ',') ? end + 1 : end;
        }
        s->cpp_set[c][t] = 1;
        return;
    }
    if (strcmp(key, "setpad") == 0) {              /* "NN:t,vol,pan,tune,decay,tone,drive,dly,rvb,arp,rate" */
        int n = atoi(val); const char *c = strchr(val, ':');
        if (n < 1 || n > NLANE || !c) return;
        const char *q = c + 1;
        for (int k = 0; k < PP_N && *q; k++) {
            char *end; float v = strtof(q, &end);
            if (end == q) break;
            if (k == PP_TYPE) v = clampf(v, 0.0f, (float)(T_COUNT - 1));
            else if (k == PP_ARP) v = clampf(v, 0.0f, 5.0f);
            else if (k == PP_RATE) v = clampf(v, 0.0f, 3.0f);
            else if (k == PP_WAVE) v = clampf(v, 0.0f, 15.0f);
            else if (k == PP_POLY) v = v > 0.5f ? 1.0f : 0.0f;
            else if (k == PP_DTIME) v = clampf(v, 0.0f, 4.0f);
            else if (k == PP_XFX || k == PP_MFX) v = clampf(v, 0.0f, 10.0f);
            else v = clampf(v, 0.0f, 1.0f);
            s->pp[n - 1][k] = v;
            q = (*end == ',') ? end + 1 : end;
        }
        if (s->plk_t[n - 1]) s->plk_b[n - 1] = s->pp[n - 1][s->plk_t[n - 1]];
        update_pad(s, n - 1);
        return;
    }

    for (int k = 0; k < G_N; k++) {
        if (strcmp(key, GD[k].key) != 0) continue;
        float v;
        if (k == G_KEY)        { int e = enum_index(val, KEY_NAME, 12);   if (e < 0) return; v = (float)e; }
        else if (k == G_SCALE) { int e = enum_index(val, SCALE_NAME, 3);  if (e < 0) return; v = (float)e; }
        else if (k == G_DLYT)  { int e = enum_index(val, DLYT_NAME, 4);   if (e < 0) return; v = (float)e; }
        else v = clampf((float)atof(val), 0.0f, 1.0f);
        s->g[k] = v;
        update_global(s);
        if (k == G_KEY) for (int i = 0; i < NLANE; i++) update_pad(s, i);
        return;
    }
}

/* ---- JSON ---- */
typedef struct { char *buf; int cap, off, err; } jb_t;
static void jp(jb_t *j, const char *fmt, ...) {
    if (j->err) return;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(j->buf + j->off, (size_t)(j->cap - j->off), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= j->cap - j->off) { j->err = 1; return; }
    j->off += n;
}
static void jopts(jb_t *j, const char *const *names, int n) {
    jp(j, "[");
    for (int i = 0; i < n; i++) jp(j, "%s\"%s\"", i ? "," : "", names[i]);
    jp(j, "]");
}
static const char *const *opts_of_pad(int k) {
    return k == PP_TYPE ? TYPE_NAME : (k == PP_ARP ? ARP_NAME : (k == PP_WAVE ? WAVE_NAME : (k == PP_POLY ? POLY_NAME : (k == PP_DTIME ? DT_NAME : RATE_NAME))));
}
static const char *const *opts_of_global(int k) {
    return k == G_KEY ? KEY_NAME : (k == G_SCALE ? SCALE_NAME : DLYT_NAME);
}
static void jparam(jb_t *j, const pd_t *d, const char *const *opts, int first) {
    if (d->enum_n > 0) {
        jp(j, "%s{\"key\":\"%s\",\"name\":\"%s\",\"type\":\"enum\",\"options\":", first ? "" : ",", d->key, d->name);
        jopts(j, opts, d->enum_n);
        jp(j, ",\"default\":%d}", (int)d->def);
    } else {
        jp(j, "%s{\"key\":\"%s\",\"name\":\"%s\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":%.2f}",
           first ? "" : ",", d->key, d->name, (double)d->def);
    }
}
static void jchild(jb_t *j, const char *name, const int *keys, int nkeys, const int *knobs, int nknobs) {
    jp(j, "\"%s\":{\"name\":\"%s\",\"child_count\":32,\"child_label\":\"Pad\",\"child_key_template\":\"p{index}_{key}\","
          "\"child_index_base\":1,\"child_index_digits\":2,\"child_index_param\":\"focused_pad\","
          "\"child_press_param\":\"live_press\",\"child_note_base\":36,\"child_copy_keys\":[",
       name, name[0] == 'p' ? "Pad Sound" : "Arp & Pan");
    for (int i = 0; i < PP_N; i++) jp(j, "%s\"%s\"", i ? "," : "", PD[i].key);
    jp(j, "],\"params\":[");
    for (int i = 0; i < nkeys; i++) jparam(j, &PD[keys[i]], opts_of_pad(keys[i]), i == 0);
    jp(j, "],\"knobs\":[");
    for (int i = 0; i < nknobs; i++) jp(j, "%s\"%s\"", i ? "," : "", PD[knobs[i]].key);
    jp(j, "]}");
}

static char g_hier[8192];
static int  g_hier_len = 0;
static char g_chain[4096];
static int  g_chain_len = 0;

static void build_json(void) {
    {
        jb_t j = { g_hier, (int)sizeof g_hier, 0, 0 };
        static const int sound_keys[] = { PP_TYPE, PP_TUNE, PP_DECAY, PP_TONE, PP_DRIVE, PP_VOL, PP_DLY, PP_RVB };
        static const int arp_keys[]   = { PP_ARP, PP_RATE, PP_PAN };
        jp(&j, "{\"pad_layout\":\"drums\",\"levels\":{"
               "\"root\":{\"name\":\"Pad Studio\",\"params\":["
               "{\"level\":\"pads\",\"label\":\"Pad Sound\"},"
               "{\"level\":\"arp\",\"label\":\"Arp & Pan\"},"
               "{\"level\":\"master\",\"label\":\"Master FX\"}],\"knobs\":[]},");
        jchild(&j, "pads", sound_keys, 8, sound_keys, 8);
        jp(&j, ",");
        jchild(&j, "arp", arp_keys, 3, arp_keys, 3);
        jp(&j, ",\"master\":{\"name\":\"Master FX\",\"params\":[");
        for (int i = 0; i < G_N; i++) jparam(&j, &GD[i], opts_of_global(i), i == 0);
        jp(&j, "],\"knobs\":[\"m_vol\",\"key\",\"scale\",\"dly_time\",\"dly_fb\",\"dly_ret\",\"rvb_size\",\"rvb_ret\"]}");
        jp(&j, "}}");
        g_hier_len = j.err ? -1 : j.off;
    }
    {
        jb_t j = { g_chain, (int)sizeof g_chain, 0, 0 };
        jp(&j, "[");
        for (int i = 0; i < G_N; i++) jparam(&j, &GD[i], opts_of_global(i), i == 0);
        jp(&j, ",{\"key\":\"focused_pad\",\"name\":\"Pad\",\"type\":\"int\",\"min\":1,\"max\":32,\"default\":1}]");
        g_chain_len = j.err ? -1 : j.off;
    }
}

static int copy_out(const char *src, int n, char *buf, int buf_len) {
    if (n < 0 || n >= buf_len) return -1;
    memcpy(buf, src, (size_t)n); buf[n] = '\0';
    return n;
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    inst_t *s = (inst_t *)instance;
    if (!key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "ui_hierarchy") == 0 || strcmp(key, "chain_params") == 0) return -1;   /* overtake: the host UI is ours, no metadata needed */
    if (strcmp(key, "module_id") == 0) return snprintf(buf, (size_t)buf_len, "pad-studio");
    if (!s) return -1;
    if (strcmp(key, "focused_pad") == 0) return snprintf(buf, (size_t)buf_len, "%d", s->focused);
    if (strcmp(key, "live_press") == 0) return snprintf(buf, (size_t)buf_len, "0");
    if (strcmp(key, "pos") == 0)
    {
        double span = s->next_time - s->last_time;
        int fr = (s->playing && span > 1.0) ? (int)(100.0 * (s->tsamp - s->last_time) / span) : 0;
        if (fr < 0) fr = 0; if (fr > 99) fr = 99;
        return snprintf(buf, (size_t)buf_len, "%d,%d,%d,%d", s->playing, s->gs, s->cur_pat, fr);
    }
    if (strcmp(key, "brn") == 0) return snprintf(buf, (size_t)buf_len, "%d", br_n);
    if (strcmp(key, "brerr") == 0) return snprintf(buf, (size_t)buf_len, "%d,%d,%d", br_err, br_skip, br_trunc);
    if (strcmp(key, "brres") == 0) return snprintf(buf, (size_t)buf_len, "%d", br_res);
    if (strcmp(key, "brpath") == 0) return snprintf(buf, (size_t)buf_len, "%s", BR_DIR);
    if (strncmp(key, "br_", 3) == 0) { int i = atoi(key + 3); if (i < 0 || i >= br_n) return -1; return snprintf(buf, (size_t)buf_len, "%c|%s", BR[i].dir ? 'd' : 'f', BR[i].name); }
    if (strcmp(key, "smpmem") == 0) return snprintf(buf, (size_t)buf_len, "%u", (unsigned)(((unsigned long long)smp_bytes * 100ull) / SMP_MAX_BYTES));
    if (strcmp(key, "smpn") == 0) { int n = 0; for (int i = 0; i < NSMP; i++) if (SMP[i].name[0]) n = i + 1; return snprintf(buf, (size_t)buf_len, "%d", n); }
    if (strncmp(key, "smp_", 4) == 0) {
        int i = atoi(key + 4); if (i < 0 || i >= NSMP) return -1;
        const smp_t *m = smp_get(i);
        return snprintf(buf, (size_t)buf_len, "%s|%d|%d", SMP[i].name, m ? m->n : 0, m ? (int)m->sr : 0);
    }
    if (strncmp(key, "smpo_", 5) == 0) {                /* onset detection: up to 15 positions (permille), comma separated */
        int i = atoi(key + 5); const smp_t *m = smp_get(i);
        if (!m || buf_len < 120) return -1;
        const int W = 441;                               /* ~10 ms windows (at the file's own rate) */
        int nw = m->n / W; if (nw < 4) { buf[0] = 0; return 0; }
        float *e = (float *)malloc((size_t)nw * sizeof(float)); int *pos = (int *)malloc(16 * sizeof(int)); float *sc = (float *)malloc(16 * sizeof(float));
        if (!e || !pos || !sc) { free(e); free(pos); free(sc); return -1; }
        for (int w = 0; w < nw; w++) { float a2 = 0; for (int k = 0; k < W; k++) { float v = (float)m->d[(w * W + k) * m->ch]; a2 += v * v; } e[w] = sqrtf(a2 / (float)W) / 32768.0f; }
        int np = 0, minGap = (int)(0.06f * m->sr / (float)W) + 1, last = -1000; float avg = e[0];
        for (int w = 1; w < nw; w++) {
            float jump = e[w] - avg;
            if (e[w] > 0.02f && e[w] > avg * 2.2f && jump > 0.01f && w - last >= minGap) {
                if (np < 15) { pos[np] = w; sc[np] = jump; np++; }
                else { int mn = 0; for (int q = 1; q < np; q++) if (sc[q] < sc[mn]) mn = q; if (jump > sc[mn]) { pos[mn] = w; sc[mn] = jump; } }
                last = w;
            }
            avg += (e[w] - avg) * 0.25f;
        }
        for (int a1 = 0; a1 < np; a1++) for (int b1 = a1 + 1; b1 < np; b1++) if (pos[b1] < pos[a1]) { int t = pos[a1]; pos[a1] = pos[b1]; pos[b1] = t; }
        int n = 0; buf[0] = 0;
        for (int q = 0; q < np; q++) n += snprintf(buf + n, (size_t)(buf_len - n), "%s%d", q ? "," : "", (int)(1000.0f * (float)pos[q] / (float)nw));
        free(e); free(pos); free(sc);
        return n;
    }
    if (strncmp(key, "smpw_", 5) == 0) {                /* waveform: 64 columns, 'a'..'z' = peak */
        int i = atoi(key + 5); const smp_t *m = smp_get(i);
        if (!m || buf_len < 66) return -1;
        for (int c = 0; c < 64; c++) {
            int a0 = (int)((long long)m->n * c / 64), a1 = (int)((long long)m->n * (c + 1) / 64), mx = 0;
            int stp = (a1 - a0) / 64 + 1;
            for (int k = a0; k < a1; k += stp) { int v = m->d[k * m->ch]; if (v < 0) v = -v; if (v > mx) mx = v; }
            int q = (int)(sqrtf((float)mx / 32768.0f) * 25.0f + 0.5f); if (q > 25) q = 25;
            buf[c] = (char)('a' + q);
        }
        buf[64] = 0; return 64;
    }
    if (strcmp(key, "recstate") == 0) {
        int pct = (s->rec_state == 2 && s->rec_len > 0) ? (int)(100L * s->rec_n / s->rec_len) : 0;
        return snprintf(buf, (size_t)buf_len, "%d,%d,%d", s->rec_state, pct, s->rec_slot);
    }
    if (strncmp(key, "smpp_", 5) == 0) {                /* peak of a sample (0..32767) for normalize */
        int i = atoi(key + 5); const smp_t *m = smp_get(i); if (!m) return -1;
        int mx = 0; for (long k = 0; k < (long)m->n * m->ch; k++) { int v = m->d[k]; if (v < 0) v = -v; if (v > mx) mx = v; }
        return snprintf(buf, (size_t)buf_len, "%d", mx);
    }
    if (strcmp(key, "meters") == 0) {
        char tmp[96]; int n = 0;
        for (int t = 0; t < NTRK; t++) { int m = (int)(sqrtf(s->meter[t] * 0.5f) * 40.0f); if (m > 99) m = 99; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02d", m); }
        if (buf_len <= n) return -1;
        memcpy(buf, tmp, (size_t)n + 1); return n;
    }
    if (strcmp(key, "scope") == 0) {                 /* 64 wave points (2 hex digits each, triggered on a rising zero crossing) + 8 band levels (2 hex digits each) */
        char tmp[160]; int n = 0, st = 0, o = s->sc_i;
        for (int i = 0; i < 60; i++) { float a = s->sc_w[(o + i) & 127], b = s->sc_w[(o + i + 1) & 127]; if (a < 0.0f && b >= 0.0f) { st = i + 1; break; } }
        for (int i = 0; i < 64; i++) { float v = s->sc_w[(o + st + i) & 127]; int q = (int)((v * 0.5f + 0.5f) * 255.0f + 0.5f); if (q < 0) q = 0; if (q > 255) q = 255; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02x", q); }
        for (int b = 0; b < 8; b++) { int q = (int)(sqrtf(s->sc_env[b]) * 400.0f); if (q > 255) q = 255; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02x", q); }
        if (buf_len <= n) return -1;
        memcpy(buf, tmp, (size_t)n + 1); return n;
    }
    if (strcmp(key, "cpu") == 0)
        return snprintf(buf, (size_t)buf_len, "%d,%d", (int)(s->cpu_avg * 100.0f + 0.5f), (int)(s->cpu_peak * 100.0f + 0.5f));
    if (strcmp(key, "clips") == 0) {
        char tmp[200]; int n = 0;
        for (int t = 0; t < NTRK; t++) n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%d,", s->clip[t]);
        for (int t = 0; t < NTRK; t++) n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%d%s", s->qclip[t], t < NTRK - 1 ? "," : "");
        if (buf_len <= n) return -1;
        memcpy(buf, tmp, (size_t)n + 1);
        return n;
    }
    if (strncmp(key, "pstate_", 7) == 0) {
        int n = atoi(key + 7);
        if (n < 1 || n > NLANE) return -1;
        const float *q = s->pp[n - 1];
        return snprintf(buf, (size_t)buf_len, "%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f",
            (int)(q[PP_TYPE] + 0.5f), (double)q[PP_VOL], (double)q[PP_PAN], (double)q[PP_TUNE], (double)q[PP_DECAY],
            (double)q[PP_TONE], (double)q[PP_DRIVE], (double)q[PP_DLY], (double)q[PP_RVB],
            (int)(q[PP_ARP] + 0.5f), (int)(q[PP_RATE] + 0.5f), (double)q[PP_CRUSH], (double)q[PP_LOCUT]);
    }
    if (strcmp(key, "mstate") == 0)
        return snprintf(buf, (size_t)buf_len, "%.3f,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f",
            (double)s->g[G_VOL], (int)(s->g[G_KEY] + 0.5f), (int)(s->g[G_SCALE] + 0.5f), (int)(s->g[G_DLYT] + 0.5f),
            (double)s->g[G_DLYFB], (double)s->g[G_DLYRET], (double)s->g[G_RVSIZE], (double)s->g[G_RVDARK], (double)s->g[G_RVRET]);

    int p; const char *nm;
    if (parse_pad_key(key, &p, &nm)) {
        for (int k = 0; k < PP_N; k++) {
            if (strcmp(nm, PD[k].key) != 0) continue;
            if (PD[k].enum_n > 0) return snprintf(buf, (size_t)buf_len, "%d", (int)(s->pp[p][k] + 0.5f));
            return snprintf(buf, (size_t)buf_len, "%.3f", (double)s->pp[p][k]);
        }
        return -1;
    }
    for (int k = 0; k < G_N; k++) {
        if (strcmp(key, GD[k].key) != 0) continue;
        if (GD[k].enum_n > 0) return snprintf(buf, (size_t)buf_len, "%d", (int)(s->g[k] + 0.5f));
        return snprintf(buf, (size_t)buf_len, "%.3f", (double)s->g[k]);
    }
    return -1;
}

static int get_error(void *instance, char *buf, int buf_len) {
    (void)instance;
    if (buf && buf_len > 0) buf[0] = '\0';
    return 0;
}

static void eq_coef(float *c, int band, float gdb, float sr) {     /* RBJ biquad: band 0 low shelf 100 Hz, 1 peak 1 kHz, 2 high shelf 8 kHz; c = b0 b1 b2 a1 a2 (normalised) */
    const float f = band == 0 ? 100.0f : (band == 1 ? 1000.0f : 8000.0f);
    const float A = powf(10.0f, gdb / 40.0f), w0 = 6.2831853f * f / sr, cw = cosf(w0), sw = sinf(w0);
    float b0, b1, b2, a0, a1, a2;
    if (band == 1) {
        const float al = sw / (2.0f * 0.9f);
        b0 = 1.0f + al * A; b1 = -2.0f * cw; b2 = 1.0f - al * A; a0 = 1.0f + al / A; a1 = -2.0f * cw; a2 = 1.0f - al / A;
    } else {
        const float al = sw * 0.70710678f, sa = 2.0f * sqrtf(A) * al;
        if (band == 0) { b0 = A * ((A + 1.0f) - (A - 1.0f) * cw + sa); b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw); b2 = A * ((A + 1.0f) - (A - 1.0f) * cw - sa);
                         a0 = (A + 1.0f) + (A - 1.0f) * cw + sa; a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw); a2 = (A + 1.0f) + (A - 1.0f) * cw - sa; }
        else         { b0 = A * ((A + 1.0f) + (A - 1.0f) * cw + sa); b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw); b2 = A * ((A + 1.0f) + (A - 1.0f) * cw - sa);
                         a0 = (A + 1.0f) - (A - 1.0f) * cw + sa; a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw); a2 = (A + 1.0f) - (A - 1.0f) * cw - sa; }
    }
    c[0] = b0 / a0; c[1] = b1 / a0; c[2] = b2 / a0; c[3] = a1 / a0; c[4] = a2 / a0;
}
static void render_block_impl(void *instance, int16_t *out_lr, int frames);
static void render_block(void *instance, int16_t *out_lr, int frames) {
    inst_t *s = (inst_t *)instance;
    if (!s) { memset(out_lr, 0, (size_t)frames * 2 * sizeof(int16_t)); return; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    render_block_impl(instance, out_lr, frames);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double el = (double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec);
    float load = (float)(el / ((double)frames / (double)s->sr * 1e9));
    s->cpu_avg += 0.05f * (load - s->cpu_avg);
    s->cpu_peak = load > s->cpu_peak ? load : s->cpu_peak * 0.9995f;
}
static void render_block_impl(void *instance, int16_t *out_lr, int frames) {
    inst_t *s = (inst_t *)instance;

    /* per-track delay times for this block */
    int dlen_t[NLANE];
    for (int t = 0; t < NLANE; t++) {
        int di = (int)(s->pp[t][PP_DTIME] + 0.5f); if (di < 0) di = 0; if (di > 4) di = 4;
        int dl = (int)(DT_BEATS[di] * 60.0f / s->bpm * s->sr);
        if (dl < 64) dl = 64; if (dl > DLY_SIZE - 2) dl = DLY_SIZE - 2;
        dlen_t[t] = dl;
    }
    const float dret = s->g[G_DLYRET] * 1.2f;
    const float rret = s->g[G_RVRET] * 1.6f;
    const float mvol = s->g[G_VOL] * s->g[G_VOL] / 0.64f * 0.6f;
#define RM_GAIN 1.9f
    float rm_fb = 0.8f; const float rm_alp = 1.0f - expf(-6.2831853f * 130.0f * s->inv_sr);
    { float sw = 0.0f, st2 = 0.0f; for (int i = 0; i < NLANE; i++) { const float w = s->pp[i][PP_RMB]; if (w > 0.001f) { sw += w; st2 += w * s->pp[i][PP_RMT]; } }
      if (sw > 0.0f) rm_fb = 0.78f + 0.17f * (st2 / sw); }

    for (int n = 0; n < frames; n++) {
        s->clock++;
        float L = 0.0f, R = 0.0f;
        float rmb_in = 0.0f, rmd_in = 0.0f;
        float tl[NLANE], tr[NLANE], tsd[NLANE], tsr[NLANE]; int tact[NLANE];      /* one bus per sound lane: a drum sound has its own effects, a synth track one set */
        for (int t = 0; t < NLANE; t++) { tl[t] = tr[t] = tsd[t] = tsr[t] = 0.0f; tact[t] = 0; }

        /* sequencer */
        if (s->playing) {
            if (s->tsamp >= s->next_time) seq_step(s);
            s->tsamp += 1.0;
            for (int t = 0; t < NLANE; t++) {
                if (s->rt_n[t] > 0 && (s->rt_cnt[t] -= 1.0f) <= 0.0f) rt_fire(s, t);
                if (s->gate_left[t] > 0 && --s->gate_left[t] == 0) s->vc[t].gate = 0;
            }
        }

        if (s->playing) {
            if (s->mo_clk) { s->mclk += s->bpm * 0.4f * s->inv_sr; while (s->mclk >= 1.0f) { s->mclk -= 1.0f; mo_send(0xF, 0xF8, 0, 0); } }
            if (s->mo_trk) for (int i = 0; i < 64; i++) if (s->mo_q[i].cnt > 0 && --s->mo_q[i].cnt == 0) mo_send(0x8, s->mo_q[i].st, s->mo_q[i].d1, 0);
        }
        /* arps */
        for (int p = 0; p < NLANE; p++) {
            ar_t *a = &s->ar[p];
            if (!a->on) continue;
            if (a->key == -2) { a->life -= 1.0f; if (a->life <= 0.0f) { a->on = 0; a->key = -1; continue; } }
            a->cnt -= 1.0f;
            if (a->cnt <= 0.0f) {
                if (!s->mute[lane_trk(s, p)]) trig(s, p, a->vel, a->base + arp_offset(s, p, a->n), 0);
                a->n++;
                a->cnt += arp_step_samples(s, p);
            }
        }

        for (int i = 0; i < 32; i++) {                                    /* delayed MIDI-effect notes */
            if (s->eq[i].cnt <= 0) continue;
            if (--s->eq[i].cnt == 0 && lane_on(s, s->eq[i].lane) && !s->mute[lane_trk(s, s->eq[i].lane)]) trig_layer(s, s->eq[i].lane, s->eq[i].vel, s->eq[i].semi, s->eq[i].follow);
        }
        if (s->rep_on) {
            s->rep_cnt -= 1.0f;
            if (s->rep_cnt <= 0.0f) { trig(s, s->rep_t, s->rep_vel, (int)s->rep_semi, 1); s->rep_cnt += rep_step_samples(s); }
        }

        {
            float inc = s->bpm * (1.0f / 60.0f) * s->inv_sr;
            s->pump_ph += inc; if (s->pump_ph >= 1.0f) s->pump_ph -= 1.0f;
            s->wob_ph += inc * 0.5f; if (s->wob_ph >= 1.0f) s->wob_ph -= 1.0f;
        }
        /* voices */
        for (int vi = 0; vi < NLANE + NGH; vi++) {
            vc_t *v = &s->vc[vi];
            if (!v->active) continue;
            const int p = vi < NLANE ? vi : s->vown[vi];
            if (vi >= NLANE && v->fol && (v->fgen != s->tgen[p] || !s->vc[p].gate)) { v->gate = 0; v->fol = 0; }   /* MIDI-effect layer ends with its note */
            const int gi = lane_trk(s, p);
            tact[p] = 1;
            float x = vc_render(s, v, p);
            if (v->type != T_SAMPLE) {      /* tail trim: no sound rings on for ever. Cap = 2 s x decay factor (Decay 50 % = 2 s, 0 = 0.7 s, 100 % = 6 s), then a quick fade */
                const int sus = TY[v->type].sus;
                if (!v->gate) v->trel += s->inv_sr;
                const float tt = sus ? (v->gate ? 0.0f : v->trel) : v->t, cap = (sus ? 1.2f : 2.0f) * s->dm[p];
                if (tt > cap) { const float lim = 1.0f - (tt - cap) * (1.0f / 0.35f); if (lim <= 0.0f) { v->active = 0; continue; } x *= lim; }
            }
            const ch_t *ch = &CHR[v->type];
            {
                const float at = s->a_tone[p] * ch->tm < 1.0f ? s->a_tone[p] * ch->tm : 1.0f;
                if (at < 1.0f) {
                    v->l1 += at * (x - v->l1);
                    v->l2 += at * (v->l1 - v->l2);
                    x = v->l2;
                }
            }
            {
                float cu = s->pp[p][PP_CUT] * ch->cm, fa = s->pp[p][PP_FENV] + ch->fenv, rs_ = s->pp[p][PP_RES] + ch->res;
                if (fa > 1.0f) fa = 1.0f; if (rs_ > 1.0f) rs_ = 1.0f;
                const float fmd = s->pp[p][PP_FMODE];
                if (cu < 0.995f || fa > 0.01f || rs_ > 0.01f || fmd > 0.01f) {
                    float ce = cu + fa * v->fenv + v->fkoff; if (ce > 1.0f) ce = 1.0f; if (ce < 0.0f) ce = 0.0f;
                    v->fenv *= 1.0f - s->fdk[p];
                    float fq = 40.0f + ce * ce * ce * 7400.0f;
                    float g = 6.2832f * fq * s->inv_sr; if (g > 1.0f) g = 1.0f;
                    float q = 1.0f - rs_ * 0.92f;
                    float hp = x - v->svf_lp - q * v->svf_bp;
                    v->svf_bp += g * hp; v->svf_lp += g * v->svf_bp;
                    if (!(fabsf(v->svf_bp) < 8.0f) || !(fabsf(v->svf_lp) < 8.0f)) { v->svf_bp = 0.0f; v->svf_lp = 0.0f; }
                    if (fmd <= 0.01f) x = v->svf_lp * (1.0f + rs_ * 0.5f);
                    else {                                             /* mode morph: LP -> BP -> HP */
                        const float wl = fmd < 0.5f ? 1.0f - 2.0f * fmd : 0.0f, wh = fmd > 0.5f ? 2.0f * fmd - 1.0f : 0.0f, wb = 1.0f - wl - wh;
                        x = (v->svf_lp * wl + v->svf_bp * 2.0f * wb + hp * wh) * (1.0f + rs_ * 0.5f);
                    }
                }
            }
            if (ch->pg > 0.0f) {                                           /* the preset's own formant / body resonance */
                const float hp_ = x - v->pk_lp - ch->pq * v->pk_bp;
                v->pk_bp += ch->pf * hp_; v->pk_lp += ch->pf * v->pk_bp;
                if (!(fabsf(v->pk_bp) < 8.0f) || !(fabsf(v->pk_lp) < 8.0f)) { v->pk_bp = 0.0f; v->pk_lp = 0.0f; }
                x += ch->pg * v->pk_bp * ch->pq * 0.5f;
            }
            if (s->pp[p][PP_DRIVE] > 0.01f) x = fast_tanh(x * s->dg[p]) * s->dnorm[p];
            if (ch->drive > 0.01f) x = fast_tanh(x * (1.0f + ch->drive * 9.0f)) * (1.0f / (1.0f + ch->drive * 1.6f));
            {
                float cru = s->pp[p][PP_CRUSH] + ch->crush; if (cru > 1.0f) cru = 1.0f;
                if (cru > 0.01f) {
                    v->cr_cnt -= 1.0f;
                    if (v->cr_cnt <= 0.0f) {
                        float lv = 4.0f + (1.0f - cru) * (1.0f - cru) * 2000.0f;
                        v->cr_hold = floorf(x * lv + 0.5f) / lv;
                        v->cr_cnt += 1.0f + cru * cru * 30.0f;
                    }
                    x = v->cr_hold;
                }
                float lc = s->pp[p][PP_LOCUT] + ch->lo; if (lc > 1.0f) lc = 1.0f;
                if (lc > 0.01f) {
                    v->hp_z += (0.002f + lc * lc * 0.3f) * (x - v->hp_z);
                    x -= v->hp_z;
                }
                float rg = s->pp[p][PP_RING] + ch->ring; if (rg > 1.0f) rg = 1.0f;
                if (rg > 0.01f) {
                    v->ring_ph += (80.0f + rg * 900.0f) * s->inv_sr; if (v->ring_ph >= 1.0f) v->ring_ph -= 1.0f;
                    float tri = 4.0f * fabsf(v->ring_ph - 0.5f) - 1.0f;
                    float m = rg * 3.0f; if (m > 1.0f) m = 1.0f;
                    x = x * (1.0f - m) + x * tri * m;
                }
                float pm = s->pp[p][PP_PUMP];
                if (pm > 0.01f) { float q = 1.0f - s->pump_ph; x *= 1.0f - pm * q * q; }
            }
            x *= v->vel * s->gain[p] * s->tg[gi] * ch->gn;
            float wb = s->pp[p][PP_WOB];
            if (wb > 0.01f) {
                float tw = 4.0f * fabsf(s->wob_ph - 0.5f) - 1.0f;
                tl[p] += x * s->pl[p] * (1.0f - wb * 0.5f * (1.0f + tw));
                tr[p] += x * s->pr[p] * (1.0f - wb * 0.5f * (1.0f - tw));
            } else {
                tl[p] += x * s->pl[p];
                tr[p] += x * s->pr[p];
            }
            tsd[p] += x * s->pp[p][PP_DLY];
            float rs = s->pp[p][PP_RVB];
            if (v->type == T_RUMBKICK && rs < 0.9f) rs = 0.9f;
            tsr[p] += x * rs;
            { const float rmv = s->pp[p][PP_RMB]; if (rmv > 0.001f) { rmb_in += x * rmv; rmd_in += x; } }
        }

        float rec_l = 0.0f, rec_r = 0.0f;
        /* per-bus effects (a drum sound = one bus, a synth track = one bus); ducking, meters and resampling work per track */
        float dkg[NTRK], mtr[NTRK];
        for (int k = 0; k < NTRK; k++) {
            mtr[k] = 0.0f; dkg[k] = 1.0f;
            if (s->dk_env[k] > 0.0005f) { dkg[k] = 1.0f - s->pp[k][PP_DUCK] * s->dk_env[k]; s->dk_env[k] *= s->dk_c[k]; } else s->dk_env[k] = 0.0f;
        }
        for (int t = 0; t < NLANE; t++) {
            const float L0 = L, R0 = R;
            const int tk = lane_trk(s, t);
            if (!tact[t] && s->fx_act[t] <= 0) continue;                  /* nothing sounding and no effect tail on this bus */
            if (dkg[tk] < 1.0f) { const float g = dkg[tk]; tl[t] *= g; tr[t] *= g; tsd[t] *= g; tsr[t] *= g; }
            mtr[tk] += fabsf(tl[t]) + fabsf(tr[t]);
            if (tact[t]) s->fx_act[t] = (int)(6.0f * s->sr);
            if (s->fx_act[t] <= 0) { L += tl[t]; R += tr[t]; if (s->rec_src == tk) { rec_l += tl[t]; rec_r += tr[t]; } continue; }
            s->fx_act[t]--;
            const float *q = s->pp[t];
            float l = tl[t], r_ = tr[t];
            if (q[PP_CHOR] > 0.01f) {                                  /* chorus */
                float m = q[PP_CHOR];
                s->ch_ph[t] += (0.2f + q[PP_CHRT] * 2.8f) * s->inv_sr; if (s->ch_ph[t] >= 1.0f) s->ch_ph[t] -= 1.0f;
                s->chb[t][s->chw[t]] = 0.5f * (l + r_);
                float d0 = 0.012f * s->sr, dp = (0.002f + q[PP_CHDP] * 0.008f) * s->sr;
                float dL = d0 + dp * sinf(TWO_PI * s->ch_ph[t]);
                float ph2 = s->ch_ph[t] + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
                float dR = d0 + dp * sinf(TWO_PI * ph2);
                float tp[2] = { 0, 0 }; float dd[2] = { dL, dR };
                for (int c = 0; c < 2; c++) {
                    float pos = (float)s->chw[t] - dd[c]; if (pos < 0.0f) pos += (float)CH_SIZE;
                    int i0 = (int)pos; float fr = pos - (float)i0; i0 &= (CH_SIZE - 1); int i1 = (i0 + 1) & (CH_SIZE - 1);
                    tp[c] = s->chb[t][i0] * (1.0f - fr) + s->chb[t][i1] * fr;
                }
                s->chw[t] = (s->chw[t] + 1) & (CH_SIZE - 1);
                l = l * (1.0f - 0.5f * m) + tp[0] * m * 0.9f;
                r_ = r_ * (1.0f - 0.5f * m) + tp[1] * m * 0.9f;
            }
            if (q[PP_PHAS] > 0.01f) {                                  /* phaser: 4 all-pass stages per side */
                float m = q[PP_PHAS], fbk = q[PP_PHFB] * 0.8f;
                s->ph_ph[t] += (0.1f + q[PP_PHRT] * 1.9f) * s->inv_sr; if (s->ph_ph[t] >= 1.0f) s->ph_ph[t] -= 1.0f;
                float in2[2] = { l, r_ };
                for (int c = 0; c < 2; c++) {
                    float ph = s->ph_ph[t] + 0.25f * (float)c; if (ph >= 1.0f) ph -= 1.0f;
                    float a_ = -0.75f + 1.3f * (0.5f + 0.5f * sinf(TWO_PI * ph));
                    float x = in2[c] + s->ph_fb[t][c] * fbk, y = x;
                    for (int k = 0; k < 4; k++) { float o = a_ * y + s->ph_z[t][c][k]; s->ph_z[t][c][k] = y - a_ * o; y = o; }
                    if (!(fabsf(y) < 8.0f)) { y = 0.0f; for (int k = 0; k < 4; k++) s->ph_z[t][c][k] = 0.0f; }
                    s->ph_fb[t][c] = y;
                    in2[c] = in2[c] * (1.0f - 0.5f * m) + y * 0.5f * m;
                }
                l = in2[0]; r_ = in2[1];
            }
            {   /* insert effect: 10 types (xfx), amount / rate / depth meaning depends on the type */
                const int xt = (int)(q[PP_XFX] + 0.5f);
                if (xt > 0 && xt <= 10) {
                    const float am = q[PP_XMIX], pa = q[PP_XRT], pb = q[PP_XDP];
                    float in2[2] = { l, r_ }, out2[2] = { l, r_ };
                    float *z = &s->xz[t][0][0];
                    const float wet2 = pb * 2.0f > 1.0f ? 1.0f : pb * 2.0f;
                    if (xt == 1 || xt == 2) {                                   /* 1 STUTTER / 2 REVERSE: capture a slice (1/8 .. 1/64 beat), then repeat it (or play it backwards) for some of every 4 slices */
                        static const float SD[4] = { 2.0f, 4.0f, 8.0f, 16.0f };
                        int di = (int)(pa * 3.99f); if (di < 0) di = 0; if (di > 3) di = 3;
                        int ls = (int)(60.0f / s->bpm * s->sr / SD[di]); if (ls < 64) ls = 64; if (ls > SB_SIZE) ls = SB_SIZE;
                        int cn = s->scnt[t]; if (cn >= ls) { cn = 0; s->sn[t]++; }
                        const int hold = (s->sn[t] & 3) < (int)(pb * 3.0f + 0.5f);
                        if (hold) {
                            const int ri = xt == 1 ? cn : ls - 1 - cn;
                            float g = 1.0f; if (cn < 48) g = (float)cn / 48.0f; else if (ls - cn < 48) g = (float)(ls - cn) / 48.0f;
                            for (int c = 0; c < 2; c++) out2[c] = in2[c] * (1.0f - am) + s->sbuf[t][c][ri] * g * am;
                        } else {
                            for (int c = 0; c < 2; c++) s->sbuf[t][c][cn] = in2[c];
                        }
                        s->scnt[t] = cn + 1;
                    } else if (xt == 3 || xt == 6) {                            /* FLANGER / COMB */
                        const int w = s->xw[t];
                        if (xt == 3) {
                            s->xph[t] += (0.05f + pa * pa * 5.0f) * s->inv_sr; if (s->xph[t] >= 1.0f) s->xph[t] -= 1.0f;
                        }
                        for (int c = 0; c < 2; c++) {
                            float ph = s->xph[t] + 0.25f * (float)c; if (ph >= 1.0f) ph -= 1.0f;
                            float d = xt == 3 ? (0.0005f + (0.5f + 0.5f * sinf(TWO_PI * ph)) * 0.0035f) * s->sr : 22.0f + pa * 400.0f;
                            if (d > (float)(XB_SIZE - 4)) d = (float)(XB_SIZE - 4);
                            float pos = (float)w - d; if (pos < 0.0f) pos += (float)XB_SIZE;
                            int i0 = (int)pos; float fr = pos - (float)i0; i0 &= (XB_SIZE - 1); int i1 = (i0 + 1) & (XB_SIZE - 1);
                            float y = s->xbuf[t][c][i0] * (1.0f - fr) + s->xbuf[t][c][i1] * fr;
                            float fb = xt == 3 ? pb * 0.85f : 0.2f + pb * 0.7f;
                            s->xbuf[t][c][w] = in2[c] + y * fb;
                            out2[c] = xt == 3 ? in2[c] + y * am : in2[c] * (1.0f - 0.4f * am) + y * am * 0.7f;
                        }
                        s->xw[t] = (w + 1) & (XB_SIZE - 1);
                    } else if (xt == 4 || xt == 5) {                            /* TREMOLO / AUTO-PAN */
                        s->xph[t] += (0.5f + pa * pa * 14.0f) * s->inv_sr; if (s->xph[t] >= 1.0f) s->xph[t] -= 1.0f;
                        if (xt == 4) {
                            for (int c = 0; c < 2; c++) {
                                float ph = s->xph[t] + 0.5f * pb * (float)c; if (ph >= 1.0f) ph -= 1.0f;
                                out2[c] = in2[c] * (1.0f - am * (0.5f + 0.5f * sinf(TWO_PI * ph)));
                            }
                        } else {
                            float lfo = tanhf(sinf(TWO_PI * s->xph[t]) * (1.0f + pb * 8.0f)) * am;
                            out2[0] = in2[0] * (1.0f - (lfo > 0.0f ? lfo : 0.0f));
                            out2[1] = in2[1] * (1.0f + (lfo < 0.0f ? lfo : 0.0f));
                        }
                    } else if (xt == 7) {                                       /* AUTO-WAH (state variable band-pass) */
                        s->xph[t] += (0.2f + pa * pa * 6.0f) * s->inv_sr; if (s->xph[t] >= 1.0f) s->xph[t] -= 1.0f;
                        const float fc = 300.0f + 2400.0f * (0.5f + 0.5f * sinf(TWO_PI * s->xph[t]));
                        const float f = 2.0f * sinf(3.14159f * fc * s->inv_sr), qd = 1.0f - pb * 0.9f;
                        for (int c = 0; c < 2; c++) {
                            float *st = &z[c * 4];
                            float hp = in2[c] - st[0] - qd * st[1];
                            st[1] += f * hp; st[0] += f * st[1];
                            if (!(fabsf(st[0]) < 8.0f) || !(fabsf(st[1]) < 8.0f)) { st[0] = st[1] = 0.0f; }
                            out2[c] = in2[c] * (1.0f - am) + st[1] * am * 2.5f;
                        }
                    } else if (xt == 8) {                                       /* GATE: tempo-synced chop (1/4 .. 1/32) */
                        static const float DIV[4] = { 1.0f, 2.0f, 4.0f, 8.0f };
                        int di = (int)(pa * 3.99f); if (di < 0) di = 0; if (di > 3) di = 3;
                        s->xph[t] += s->bpm / 60.0f * DIV[di] * s->inv_sr; if (s->xph[t] >= 1.0f) s->xph[t] -= 1.0f;
                        const float tgt = s->xph[t] < 0.5f ? 1.0f : 1.0f - am, k = 0.003f + (1.0f - pb) * 0.2f;
                        s->xhold[t][0] += k * (tgt - s->xhold[t][0]);
                        out2[0] = in2[0] * s->xhold[t][0]; out2[1] = in2[1] * s->xhold[t][0];
                    } else if (xt == 9) {                                       /* LO-FI: sample-rate + bit reduction */
                        const int n = 1 + (int)(am * 15.0f);
                        const float lv = 8.0f + (1.0f - pa) * 2000.0f;
                        if (++s->xcnt[t] >= n) { s->xcnt[t] = 0; for (int c = 0; c < 2; c++) s->xhold[t][c] = floorf(in2[c] * lv + 0.5f) / lv; }
                        for (int c = 0; c < 2; c++) out2[c] = in2[c] * (1.0f - wet2) + s->xhold[t][c] * wet2;
                    } else {                                                    /* DJ FILTER: left = low-pass, right = high-pass, middle = off */
                        const float pos = pa - 0.5f, ap = fabsf(pos) * 2.0f;
                        if (ap > 0.04f) {
                            float fc = pos < 0.0f ? 18000.0f * powf(0.006f, ap) : 20.0f * powf(300.0f, ap);
                            if (fc > 0.2f * s->sr) fc = 0.2f * s->sr;
                            const float f = 2.0f * sinf(3.14159f * fc * s->inv_sr), qd = 1.0f - am * 0.85f;
                            for (int c = 0; c < 2; c++) {
                                float *st = &z[c * 4];
                                float hp = in2[c] - st[0] - qd * st[1];
                                st[1] += f * hp; st[0] += f * st[1];
                                if (!(fabsf(st[0]) < 8.0f) || !(fabsf(st[1]) < 8.0f)) { st[0] = st[1] = 0.0f; }
                                out2[c] = pos < 0.0f ? st[0] : hp;
                            }
                        }
                    }
                    l = out2[0]; r_ = out2[1];
                    if (!(fabsf(l) < 16.0f)) l = 0.0f;
                    if (!(fabsf(r_) < 16.0f)) r_ = 0.0f;
                }
            }
            {   /* per-sound EQ: low shelf 100 Hz, peak 1 kHz, high shelf 8 kHz, +-12 dB (skipped while flat) */
                const float gq[3] = { (q[PP_EQL] - 0.5f) * 24.0f, (q[PP_EQM] - 0.5f) * 24.0f, (q[PP_EQH] - 0.5f) * 24.0f };
                if (fabsf(gq[0]) > 0.15f || fabsf(gq[1]) > 0.15f || fabsf(gq[2]) > 0.15f) {
                    if (!s->eq_on[t]) { memset(s->eq_z[t], 0, sizeof s->eq_z[t]); for (int b = 0; b < 3; b++) s->eq_g[t][b] = 999.0f; s->eq_on[t] = 1; }
                    for (int b = 0; b < 3; b++) if (s->eq_g[t][b] != gq[b]) { eq_coef(s->eq_c[t][b], b, gq[b], s->sr); s->eq_g[t][b] = gq[b]; }
                    float io[2] = { l, r_ };
                    for (int c = 0; c < 2; c++) {
                        float x = io[c];
                        for (int b = 0; b < 3; b++) {
                            const float *k = s->eq_c[t][b]; float *z = s->eq_z[t][b][c];
                            float y = k[0] * x + z[0]; z[0] = k[1] * x - k[3] * y + z[1]; z[1] = k[2] * x - k[4] * y; x = y;
                        }
                        if (!(fabsf(x) < 16.0f)) { x = 0.0f; memset(s->eq_z[t], 0, sizeof s->eq_z[t]); }
                        io[c] = x;
                    }
                    l = io[0]; r_ = io[1];
                } else s->eq_on[t] = 0;
            }
            L += l; R += r_;

            if (tsd[t] != 0.0f || tsr[t] != 0.0f) s->dr_act[t] = (int)(6.0f * s->sr);     /* delay + reverb only run while this bus sends to them (and for their tail) */
            if (s->dr_act[t] > 0) {
                if (--s->dr_act[t] == 0) {
                    memset(s->dlL[t], 0, sizeof s->dlL[t]); memset(s->dlR[t], 0, sizeof s->dlR[t]); memset(s->rv[t], 0, sizeof s->rv[t]);
                    memset(s->rv_z[t], 0, sizeof s->rv_z[t]); s->dly_z[t][0] = s->dly_z[t][1] = 0.0f;
                }
            /* delay (ping-pong) */
            {
                const float dfb = q[PP_DFB] * 0.9f;
                int rp = (s->dw[t] - dlen_t[t]) & DLY_MASK;
                float yl = s->dlL[t][rp], yr = s->dlR[t][rp];
                s->dly_z[t][0] += s->a_dly * (yl - s->dly_z[t][0]);
                s->dly_z[t][1] += s->a_dly * (yr - s->dly_z[t][1]);
                s->dlL[t][s->dw[t]] = tsd[t] + s->dly_z[t][1] * dfb;
                s->dlR[t][s->dw[t]] = s->dly_z[t][0] * dfb;
                s->dw[t] = (s->dw[t] + 1) & DLY_MASK;
                L += yl * dret; R += yr * dret;
            }
            /* reverb: 4-line FDN, Hadamard mixing */
            {
                float o[NLINES];
                for (int i = 0; i < NLINES; i++) {
                    int rr = (s->rw[t] - s->rv_len[t][i]) & RV_MASK;
                    float y = s->rv[t][i][rr];
                    s->rv_z[t][i] += s->rv_a[t] * (y - s->rv_z[t][i]);
                    o[i] = s->rv_z[t][i] * s->rv_g[t][i];
                }
                float h0 = 0.5f * (o[0] + o[1] + o[2] + o[3]);
                float h1 = 0.5f * (o[0] - o[1] + o[2] - o[3]);
                float h2 = 0.5f * (o[0] + o[1] - o[2] - o[3]);
                float h3 = 0.5f * (o[0] - o[1] - o[2] + o[3]);
                float in = tsr[t];
                s->rv[t][0][s->rw[t]] = h0 + in + 1e-20f;
                s->rv[t][1][s->rw[t]] = h1 + in - 1e-20f;
                s->rv[t][2][s->rw[t]] = h2 + in + 1e-20f;
                s->rv[t][3][s->rw[t]] = h3 + in - 1e-20f;
                s->rw[t] = (s->rw[t] + 1) & RV_MASK;
                L += (o[0] - o[2]) * 0.7f * rret;
                R += (o[1] - o[3]) * 0.7f * rret;
            }
            }
            if (s->rec_src == tk) { rec_l += L - L0; rec_r += R - R0; }
        }
        for (int k = 0; k < NTRK; k++) s->meter[k] = mtr[k] > s->meter[k] ? mtr[k] : s->meter[k] * 0.9997f;
        if (rmb_in != 0.0f) s->rm_act = (int)(4.0f * s->sr);
        if (s->rm_act > 0) {                                           /* kick rumble: low-passed kick > 4 dark combs > ducked by the dry kick > high-passed */
            if (--s->rm_act == 0) { memset(s->rm_buf, 0, sizeof s->rm_buf); s->rm_lp = s->rm_env = s->rm_hp = 0.0f; }
            else {
                static const int RDL[4] = { 1327, 1583, 1861, 2053 };
                s->rm_lp += rm_alp * (rmb_in - s->rm_lp);
                const float ae = fabsf(rmd_in); s->rm_env += (ae > s->rm_env ? 0.02f : 0.00025f) * (ae - s->rm_env);
                float y = 0.0f;
                for (int i = 0; i < 4; i++) {
                    const float c = s->rm_buf[i][(s->rm_w - RDL[i]) & 4095];
                    y += c; s->rm_buf[i][s->rm_w] = s->rm_lp + c * rm_fb;
                }
                s->rm_w = (s->rm_w + 1) & 4095;
                float dg = s->rm_env * 5.0f; if (dg > 1.0f) dg = 1.0f;
                y *= RM_GAIN * (1.0f - 0.9f * dg);
                s->rm_hp += 0.0035f * (y - s->rm_hp); y -= s->rm_hp;
                if (!(fabsf(y) < 8.0f)) { memset(s->rm_buf, 0, sizeof s->rm_buf); s->rm_lp = s->rm_env = s->rm_hp = 0.0f; y = 0.0f; }
                L += y; R += y;
            }
        }

        {
            float wd = s->g[G_WID];
            if (wd < 0.48f || wd > 0.52f) {
                float mid = 0.5f * (L + R), sd2 = 0.5f * (L - R) * (wd * 2.0f);
                L = mid + sd2; R = mid - sd2;
            }
            if (s->mf_mode) {
                float in[2] = { L, R };
                for (int c = 0; c < 2; c++) {
                    s->mf_z[c][0] += s->mf_a * (in[c] - s->mf_z[c][0]);
                    s->mf_z[c][1] += s->mf_a * (s->mf_z[c][0] - s->mf_z[c][1]);
                    in[c] = (s->mf_mode == 1) ? s->mf_z[c][1] : (in[c] - s->mf_z[c][1]);
                }
                L = in[0]; R = in[1];
            }
            float st = s->g[G_SAT];
            if (st > 0.01f) {
                float dg2 = 1.0f + st * 5.0f, nm = 0.5f / fast_tanh(0.5f * dg2);
                L = fast_tanh(L * dg2) * nm; R = fast_tanh(R * dg2) * nm;
            }
            float cp = s->g[G_COMP];
            if (cp > 0.01f) {
                float dl = L, dr = R;
                if (s->g[G_CHP] > 0.01f) {                       /* sidechain high-pass: bass no longer pumps the compressor */
                    const float ha = 1.0f - expf(-6.2831853f * (40.0f + 360.0f * s->g[G_CHP]) * s->inv_sr);
                    s->cdet_z[0] += ha * (L - s->cdet_z[0]); s->cdet_z[1] += ha * (R - s->cdet_z[1]);
                    dl = L - s->cdet_z[0]; dr = R - s->cdet_z[1];
                }
                float pk = fmaxf(fabsf(dl), fabsf(dr));
                s->comp_env += (pk > s->comp_env ? 0.02f : 0.0004f) * (pk - s->comp_env);
                float thr = 0.35f - 0.3f * cp;
                if (s->comp_env > thr) {
                    float gn = (thr + (s->comp_env - thr) * 0.25f) / s->comp_env;
                    L *= gn; R *= gn;
                }
                float mk = 1.0f + cp * 1.5f; L *= mk; R *= mk;
            }
        }
        if (s->met_env > 0.001f) {
            s->met_ph += s->met_f * s->inv_sr; if (s->met_ph >= 1.0f) s->met_ph -= 1.0f;
            float cl = (4.0f * fabsf(s->met_ph - 0.5f) - 1.0f) * s->met_env * s->g[G_MET] * 0.5f;
            s->met_env *= 0.9965f; L += cl; R += cl;
        }
        if (!(fabsf(L) < 16.0f) || !(fabsf(R) < 16.0f)) {            /* NaN / runaway: wipe the feedback buffers */
            L = 0.0f; R = 0.0f;
            memset(s->dlL, 0, sizeof s->dlL); memset(s->dlR, 0, sizeof s->dlR);
            memset(s->rv, 0, sizeof s->rv); memset(s->rv_z, 0, sizeof s->rv_z);
            memset(s->chb, 0, sizeof s->chb); memset(s->ph_z, 0, sizeof s->ph_z); memset(s->ph_fb, 0, sizeof s->ph_fb);
            s->mf_z[0][0] = s->mf_z[0][1] = s->mf_z[1][0] = s->mf_z[1][1] = 0.0f; s->comp_env = 0.0f; s->lim_g = 1.0f; s->eq_lo[0] = s->eq_lo[1] = s->eq_hi[0] = s->eq_hi[1] = 0.0f;
        }
        {   /* master EQ (3 bands: <250 Hz, 250-3.5k, >3.5k; +-12 dB) */
            const float el = s->g[G_EQL], em = s->g[G_EQM], eh = s->g[G_EQH];
            if (fabsf(el - 0.5f) > 0.01f || fabsf(em - 0.5f) > 0.01f || fabsf(eh - 0.5f) > 0.01f) {
                const float gl = powf(10.0f, (el - 0.5f) * 24.0f / 20.0f), gm = powf(10.0f, (em - 0.5f) * 24.0f / 20.0f), gh = powf(10.0f, (eh - 0.5f) * 24.0f / 20.0f);
                const float al = 1.0f - expf(-6.2831853f * 250.0f * s->inv_sr), ah = 1.0f - expf(-6.2831853f * 3500.0f * s->inv_sr);
                float in2[2] = { L, R };
                for (int c = 0; c < 2; c++) {
                    s->eq_lo[c] += al * (in2[c] - s->eq_lo[c]); s->eq_hi[c] += ah * (in2[c] - s->eq_hi[c]);
                    in2[c] = s->eq_lo[c] * gl + (s->eq_hi[c] - s->eq_lo[c]) * gm + (in2[c] - s->eq_hi[c]) * gh;
                }
                L = in2[0]; R = in2[1];
            }
        }
        L *= mvol; R *= mvol;
        {   /* master limiter: instant attack, slow release; ceiling falls from 1.0 to 0.3 */
            const float lm = s->g[G_LIM];
            if (lm > 0.01f) {
                const float thr = 1.0f - 0.7f * lm, pk = fmaxf(fabsf(L), fabsf(R));
                const float tg = pk > thr ? thr / pk : 1.0f;
                if (tg < s->lim_g) s->lim_g = tg; else s->lim_g += (1.0f - s->lim_g) * 0.0006f;
                L *= s->lim_g; R *= s->lim_g;
            } else s->lim_g = 1.0f;
        }
        L = softclip(L); R = softclip(R);
        if (pv_on) {                                                  /* browser audition, independent of master volume */
            int16_t *pd = __atomic_load_n(&PVW.d, __ATOMIC_ACQUIRE); int pi = (int)pv_pos;
            if (!pd || pi >= PVW.n - 1) pv_on = 0;
            else {
                float fr = (float)(pv_pos - pi), a0, b0;
                if (PVW.ch == 2) { a0 = pd[pi * 2] * (1.0f - fr) + pd[pi * 2 + 2] * fr; b0 = pd[pi * 2 + 1] * (1.0f - fr) + pd[pi * 2 + 3] * fr; }
                else { a0 = b0 = pd[pi] * (1.0f - fr) + pd[pi + 1] * fr; }
                L += a0 * (0.7f / 32768.0f); R += b0 * (0.7f / 32768.0f);
                pv_pos += (double)(PVW.sr * s->inv_sr);
            }
        }
        if (s->rec_state == 2 && s->rec_buf) {
            float cl = s->rec_src < 0 ? L : softclip(rec_l * mvol), cr = s->rec_src < 0 ? R : softclip(rec_r * mvol);
            int ci = (int)(cl * 32767.0f), cj = (int)(cr * 32767.0f);
            if (ci > 32767) ci = 32767; if (ci < -32768) ci = -32768; if (cj > 32767) cj = 32767; if (cj < -32768) cj = -32768;
            s->rec_buf[2 * s->rec_n] = (int16_t)ci; s->rec_buf[2 * s->rec_n + 1] = (int16_t)cj;
            if (++s->rec_n >= s->rec_len) s->rec_state = 3;
        }
        {   /* scope + 8-band level meter of the master output (only reads, never changes the audio) */
            static const float scf[8] = { 80.f, 160.f, 320.f, 640.f, 1280.f, 2560.f, 5120.f, 10240.f };
            float m = (L + R) * 0.5f, prev = 0.0f;
            if (++s->sc_c >= 24) { s->sc_c = 0; s->sc_w[s->sc_i] = m; s->sc_i = (s->sc_i + 1) & 127; }
            for (int b = 0; b < 8; b++) {
                float a = 1.0f - expf(-6.2831853f * scf[b] * s->inv_sr);
                s->sc_lp[b] += a * (m - s->sc_lp[b]);
                float band = s->sc_lp[b] - prev; prev = s->sc_lp[b];
                s->sc_env[b] += (fabsf(band) - s->sc_env[b]) * (s->sc_env[b] < fabsf(band) ? 0.01f : 0.0006f);
            }
        }
        int li = (int)(L * 32767.0f), ri = (int)(R * 32767.0f);
        if (li > 32767) li = 32767; if (li < -32768) li = -32768;
        if (ri > 32767) ri = 32767; if (ri < -32768) ri = -32768;
        out_lr[2 * n] = (int16_t)li;
        out_lr[2 * n + 1] = (int16_t)ri;
    }
}

static plugin_api_v2_t g_api = {
    .api_version = 2,
    .create_instance = create_instance,
    .destroy_instance = destroy_instance,
    .on_midi = on_midi,
    .set_param = set_param,
    .get_param = get_param,
    .get_error = get_error,
    .render_block = render_block,
};

static const uint8_t CHSALT[T_COUNT] = {
#include "char_salt.inc"
};
static void init_chars(void) {
    /* archetypes: cm res fenv drive crush ring lo tm */
    static const float AR[12][8] = {
        { 1.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f },     /* clean   */
        { 0.42f, 0.15f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.55f },     /* dark    */
        { 0.62f, 0.65f, 0.35f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f },     /* resonant sweep */
        { 0.85f, 0.25f, 0.00f, 0.80f, 0.00f, 0.00f, 0.00f, 1.00f },     /* scream  */
        { 1.00f, 0.00f, 0.00f, 0.35f, 0.38f, 0.00f, 0.10f, 1.00f },     /* gritty  */
        { 0.85f, 0.20f, 0.00f, 0.00f, 0.00f, 0.55f, 0.00f, 1.00f },     /* metallic ring */
        { 0.33f, 0.50f, 0.75f, 0.15f, 0.00f, 0.00f, 0.00f, 1.00f },     /* closing filter */
        { 0.70f, 0.30f, 0.00f, 0.00f, 0.00f, 0.00f, 0.40f, 0.85f },     /* hollow  */
        { 1.00f, 0.00f, 0.00f, 0.95f, 0.00f, 0.00f, 0.00f, 0.60f },     /* fuzz    */
        { 0.80f, 0.00f, 0.00f, 0.00f, 0.55f, 0.00f, 0.00f, 0.45f },     /* lo-fi   */
        { 0.52f, 0.85f, 0.15f, 0.20f, 0.00f, 0.00f, 0.00f, 1.00f },     /* nasal   */
        { 0.45f, 0.00f, 0.00f, 0.50f, 0.00f, 0.25f, 0.00f, 0.75f },     /* dark ring */
    };
    for (int ty = 0; ty < T_COUNT; ty++) {
        ch_t *h = &CHR[ty]; h->cm = 1.0f; h->res = h->fenv = h->drive = h->crush = h->ring = h->lo = 0.0f; h->tm = 1.0f; h->gn = 1.0f; h->wv = 0; h->pf = h->pg = h->pq = 0.0f;
        if (ty == T_SAMPLE || ty >= T_V4) continue;
#ifdef CHAR_NEUTRAL
        continue;
#else
        const int bt = BASE_T[ty];
        uint32_t s = 2166136261u ^ (uint32_t)(ty * 2654435761u) ^ ((uint32_t)CHSALT[ty] * 0x9E3779B1u); for (int k = 0; k < 6; k++) ch_rng(&s);
        const int ai = (int)(((uint32_t)ty * 5u + (ch_rng(&s) % 3u)) % 12u);
        const float *a = AR[ai];
        {   /* envelope / pitch spread: every preset gets its own decay, pitch drop and (drums) fundamental */
            ty_t *T = &TY[ty]; uint32_t s3 = s ^ 0x85EBCA6Bu; ch_rng(&s3);
            if (T->oct <= 0.0f && T->f0 > 0.0f && bt != T_RISER && bt != T_SWEEP && bt != T_IMPACT) {      /* drums / percussion */
                T->f0 *= ch_r(&s3, 0.68f, 1.6f);
                if (T->tau1 > 0.0f) T->tau1 *= ch_r(&s3, 0.65f, 1.7f);
                if (T->tau2 > 0.0f) T->tau2 *= ch_r(&s3, 0.65f, 1.6f);
                if (T->pet > 0.0f) T->pet *= ch_r(&s3, 0.6f, 1.9f);
            } else if (T->oct > 0.0f) {                                                                     /* melodic */
                { const float r = ch_f(&s3); if (r < 0.18f && T->oct > 1.5f) T->oct *= 0.5f; else if (r > 0.72f && T->oct < 8.1f) T->oct *= 2.0f; }      /* register: some presets sit an octave lower or higher */
                if (T->tau1 > 0.0f) T->tau1 *= ch_r(&s3, 0.6f, 1.8f);
                if (T->sus && T->att > 0.0f) T->att *= ch_r(&s3, 0.5f, 2.0f);
                if (T->pet > 0.0f) T->pet *= ch_r(&s3, 0.6f, 1.8f);
            }
        }
        /* class scaling: res, fenv, drive, crush, ring, lo, cutoff floor */
        float kr = 1, kf = 1, kd = 1, kc = 1, kg = 1, kl = 1, floor_ = 0.3f;
        if (bt <= T_SUB808) { kg = 0.3f; kl = 0.4f; floor_ = 0.42f; kd = 0.9f; }
        else if (bt <= T_RIM) { floor_ = 0.45f; }
        else if (bt <= T_RIDE) { kd = 0.6f; floor_ = 0.5f; }
        else if (bt <= T_ZAP) { kg = 1.3f; }
        else if (bt == T_RISER || bt == T_SWEEP || bt == T_IMPACT) { floor_ = 0.25f; kg = 0.8f; }
        else if (bt == T_SUBBASS || bt == T_ROLLBASS || bt == T_ACID || bt == T_REESE || bt == T_SBASS || bt == T_SFMBASS) { kg = 0.3f; kl = 0.25f; kc = 0.5f; floor_ = 0.35f; }
        else if (bt == T_PAD || bt == T_SSTRINGS || bt == T_SSUPER || bt == T_SORGAN) { kc = 0.3f; kd = 0.6f; kg = 0.3f; kf = 0.6f; floor_ = 0.3f; }
        else if (bt == T_LEAD || bt == T_SLEAD || bt == T_SHOOVER) { kg = 0.5f; kl = 0.4f; floor_ = 0.4f; }
        float j = 0.9f + 0.5f * ch_f(&s);                               /* jitter 0.9 .. 1.4 */
        h->cm = 1.0f - (1.0f - a[0]) * j; if (h->cm < floor_) h->cm = floor_; if (h->cm > 1.0f) h->cm = 1.0f;
        h->res = clampf(a[1] * kr * (0.85f + 0.5f * ch_f(&s)), 0.0f, 0.9f);
        h->fenv = clampf(a[2] * kf * (0.85f + 0.5f * ch_f(&s)), 0.0f, 0.9f);
        h->drive = clampf(a[3] * kd * (0.85f + 0.5f * ch_f(&s)), 0.0f, 0.95f);
        h->crush = clampf(a[4] * kc * (0.85f + 0.5f * ch_f(&s)), 0.0f, 0.7f);
        h->ring = clampf(a[5] * kg * (0.85f + 0.5f * ch_f(&s)), 0.0f, 0.8f);
        h->lo = clampf(a[6] * kl * (0.85f + 0.5f * ch_f(&s)), 0.0f, 0.5f);
        h->tm = 1.0f - (1.0f - a[7]) * (0.85f + 0.5f * ch_f(&s)); if (h->tm < 0.35f) h->tm = 0.35f; if (h->tm > 1.0f) h->tm = 1.0f;
        if (h->res > 0.01f && h->cm >= 0.999f && h->fenv <= 0.01f) h->cm = 0.8f;      /* resonance needs a filter that is not wide open */
        {   /* oscillator shape: wave-capable sounds that do not already carry a deliberate wave get a random one half of the time */
            const int wc = bt == T_ACID || bt == T_REESE || bt == T_PLUCK || bt == T_STAB || bt == T_PAD || bt == T_LEAD || bt == T_SLEAD || bt == T_SBASS || bt == T_SSUPER || bt == T_SSTRINGS || bt == T_SHOOVER;
            int own = 0;
            if (ty >= T_V3) own = VAR3[ty - T_V3].wave; else if (ty >= T_V2 && ty < T_SAMPLE) own = VAR2[ty - T_V2].wave;
            static const int WP[13] = { 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
            uint32_t s2 = s ^ 0x9E3779B9u; ch_rng(&s2);
            if (wc && !own && ch_f(&s2) < 0.55f) h->wv = WP[ch_rng(&s2) % 13u];
            /* real wavetables: every melodic / body preset gets its own single cycle (dark ones for bass and drum bodies, brighter ones for leads) */
            int lo = 0, hi = 0;
            if (bt <= T_RUMBKICK || bt == T_SUB808 || bt == T_TOM || bt == T_CONGA || bt == T_SUBBASS) { lo = 0; hi = 330; }
            else if (bt == T_ACID || bt == T_REESE || bt == T_SBASS || bt == T_SFMBASS) { lo = 40; hi = 360; }
            else if (bt == T_PAD || bt == T_SSTRINGS || bt == T_SSUPER || bt == T_SORGAN || bt == T_SHOOVER) { lo = 150; hi = 520; }
            else if (bt == T_LEAD || bt == T_SLEAD || bt == T_PLUCK || bt == T_STAB || bt == T_SEPIANO) { lo = 240; hi = 560; }
            if (hi > 0 && ch_f(&s2) < 0.92f) {
                static const int ST[5] = { 97, 89, 83, 79, 73 };
                const int g = bt <= T_RUMBKICK ? 0 : (bt == T_SUB808 ? 1 : (bt == T_TOM || bt == T_CONGA ? 2 : (bt == T_SUBBASS ? 3 : 4)));
                static int cnt[24]; const int grp = g < 4 ? g : 4 + (lo / 40) % 20;
                const int n = cnt[grp]++; const int span = hi - lo;
                h->wv = 100 + lo + (int)(((uint32_t)n * (uint32_t)ST[grp % 5] + (ch_rng(&s2) % 7u)) % (uint32_t)span);
            }
        }
        {   /* formant: a resonant body at a random frequency, stronger on the noisy / percussive classes */
            uint32_t s4 = s ^ 0xC2B2AE35u; ch_rng(&s4); ch_rng(&s4);
            const int noisy = bt == T_CLAP || bt == T_SNARE || bt == T_RIM || bt == T_HATCL || bt == T_HATOP || bt == T_SHAKER || bt == T_CRASH || bt == T_RIDE || bt == T_COWBELL || bt == T_PERC || bt == T_ZAP || bt == T_RISER || bt == T_SWEEP || bt == T_IMPACT || bt == T_BELL;
            const float pr = noisy ? 0.95f : (bt <= T_SUB808 ? 0.4f : 0.7f);
            if (ch_f(&s4) < pr) {
                const float fq = 260.0f * powf(24.0f, ch_f(&s4));                 /* 260 Hz .. 6.2 kHz */
                const float x2 = TWO_PI * fq / g_sr; h->pf = x2 / (1.0f + x2);
                h->pq = 1.0f - ch_r(&s4, 0.55f, 0.9f);                              /* damping: lower = more resonant */
                h->pq = ch_r(&s4, 0.28f, 0.65f);
                h->pg = ch_r(&s4, 2.0f, 5.5f);
            }
        }
        h->gn = CHGAIN[ty];
#endif
    }
}

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) {
    init_types();
    init_chars();
    if (host) {
        if (host->sample_rate > 0) g_sr = host->sample_rate;
        g_get_beat = host->get_beat_position;
        g_get_bpm = host->get_bpm;
        g_host = host;
    }
    (void)g_get_beat;
    return &g_api;
}
