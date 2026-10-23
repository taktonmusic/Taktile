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
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
/* the Move's C library is older than the one the build machine has: these four calls would pull in newer symbol versions (exp2f / log2f 2.27, stat 2.33, fmodf 2.38)
   and the module could not be loaded at all, so they are replaced by calls that every libc has */
static inline float tk_logf(float x) {
    if (!(x > 0.0f)) return -87.0f;
    int e; float m = frexpf(x, &e);
    if (m < 0.70710678f) { m *= 2.0f; e--; }
    const float z = (m - 1.0f) / (m + 1.0f), z2 = z * z;
    return 0.693147181f * (float)e + 2.0f * z * (1.0f + z2 * (0.333333333f + z2 * (0.2f + z2 * (0.142857143f + z2 * (0.111111111f + z2 * 0.0909090909f)))));
}
#define logf(x) tk_logf(x)
#define exp2f(x) expf((x) * 0.693147181f)
#define log2f(x) (logf(x) * 1.44269504f)
static inline float tk_fmodf(float a, float b) { return a - b * floorf(a / b); }
#define fmodf(a, b) tk_fmodf((a), (b))
static inline int tk_stat(const char *p, struct stat *st) { return (int)syscall(SYS_newfstatat, AT_FDCWD, p, st, 0); }
#define stat(a, b) tk_stat((a), (b))
#include "host/plugin_api_v1.h"

/* Pin libm symbols to old glibc versions so the module loads on the Move. */
#if defined(__aarch64__) && defined(__GLIBC__)
__asm__(".symver expf,expf@GLIBC_2.17");
__asm__(".symver powf,powf@GLIBC_2.17");
#endif

#define NPADS      32                 /* MIDI pads */
#define NLANE      80                 /* sound lanes: 0-15 = tracks, 16-79 = four drum kits x 16 sounds */
#define PAD_BASE   36
#define MAX_INST   2
#define GRN_SZ     524288         /* BEADS granulator history per sound (11 s: room for grains up to 4.5 s) */
#define GRN_G      16             /* grains that can sound at once */
#define GRN_SLOTS  4              /* sounds that can use the granulator at the same time */
#define TWO_PI     6.28318530717958647692f
static float g_sintab[2049];
static inline float sin2pi_raw(float ph) { const float x = ph * 2048.0f; const int i = (int)x; const float fr = x - (float)i; const float a = g_sintab[i & 2047]; return a + (g_sintab[(i & 2047) + 1] - a) * fr; }
static inline float sin2pi(float ph) { ph -= floorf(ph); return sin2pi_raw(ph); }      /* sin(2*pi*ph) from a 2048-point table (error about 1e-6), any phase: negative or above 1 too */
#define DLY_SIZE   32768          /* per track */
#define CH_SIZE    2048
#define SB_SIZE    8192           /* stutter / reverse capture buffer per track */
#define XB_SIZE    1024           /* insert effect buffers (flanger, comb) */
#define DLY_MASK   (DLY_SIZE - 1)
#define RV_SIZE    4096
#define RV_MASK    (RV_SIZE - 1)
#define DT_SIZE    8192
#define DT_MASK    (DT_SIZE - 1)
#define NLINES     4
#define NTRK       16
#define NPAT       8
#define MAXSTEP    128

/* ---------------- per-pad parameters ---------------- */
#include "variants_n.h"
#include "variants3_n.h"
#include "variants4_n.h"
enum { PP_TYPE, PP_VOL, PP_PAN, PP_TUNE, PP_DECAY, PP_TONE, PP_DRIVE, PP_DLY, PP_RVB, PP_ARP, PP_RATE, PP_CRUSH, PP_LOCUT, PP_RING, PP_PUMP, PP_WOB, PP_CUT, PP_RES, PP_FENV, PP_WAVE, PP_DET, PP_SUB, PP_ATT, PP_POLY, PP_DTIME, PP_DFB, PP_RSIZE, PP_RDARK, PP_CHOR, PP_CHRT, PP_CHDP, PP_PHAS, PP_PHRT, PP_PHFB, PP_XFX, PP_XMIX, PP_XRT, PP_XDP, PP_MFX, PP_MAMT, PP_MPRM, PP_SMP, PP_SST, PP_SEN, PP_SMD, PP_DUCK, PP_DUCKR, PP_SGN, PP_SATK, PP_SREL, PP_SSTR, PP_SCS, PP_TSW, PP_FDEC, PP_FKEY, PP_FMODE, PP_RMB, PP_RMT, PP_EQL, PP_EQM, PP_EQH, PP_X2FX, PP_X2MIX, PP_X2RT, PP_X2DP, PP_M2FX, PP_M2AMT, PP_M2PRM, PP_DMODE, PP_RMODE, PP_MONO, PP_SNZ, PP_FATK, PP_SFLD, PP_SCHP, PP_TREM, PP_TRAT, PP_APAN, PP_VFLT, PP_GRN, PP_GSZ, PP_GSP, PP_GPI, PP_MCH, PP_MVB, PP_TRN, PP_VHM, PP_VMN, PP_VMX, PP_ORN, PP_EQLF, PP_EQMF, PP_EQHF, PP_EQQ, PP_EQO, PP_PCV, PP_RFQ, PP_DTN, PP_DWD, PP_RWD, PP_RLC, PP_GDN, PP_GRV, PP_GFZ, PP_GTN, PP_DSW, PP_DST, PP_DNZ, PP_DCK, PP_DLN, PP_DMD, PP_DLT, PP_TKEY, PP_TSCL, PP_N };
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
    { "x2fx",  "FX2",    11, 0.0f },
    { "x2mix", "FX2 Amt",0, 0.50f },
    { "x2rt",  "FX2 Rate",0, 0.50f },
    { "x2dp",  "FX2 Dpth",0, 0.50f },
    { "m2fx",  "MIDI FX2",11, 0.0f },
    { "m2amt", "M2 Amt", 0, 0.50f },
    { "m2prm", "M2 Prm", 0, 0.50f },
    { "dmode", "D Type", 3, 0.0f },
    { "rmode", "R Type", 3, 0.0f }, { "mono", "Mono", 3, 1.0f },
    { "snz", "Noise", 0, 0.0f }, { "fatk", "FltAtk", 0, 0.0f }, { "sfld", "Fold", 0, 0.0f }, { "schp", "Chop", 0, 0.0f },
    { "trem", "Tremolo", 0, 0.0f }, { "trat", "TremRate", 0, 0.4f }, { "apan", "AutoPan", 0, 0.0f }, { "vflt", "VelFilt", 0, 0.0f },
    { "grn", "Grain", 0, 0.0f }, { "gsz", "GrSize", 0, 0.4f }, { "gsp", "GrSpread", 0, 0.5f }, { "gpi", "GrPitch", 0, 0.0f },
    { "mch", "Chance", 0, 1.0f }, { "mvb", "VelBias", 0, 0.5f }, { "trn", "Transp", 0, 0.5f }, { "vhm", "VelHum", 0, 0.0f }, { "vmn", "VelMin", 0, 0.0f }, { "vmx", "VelMax", 0, 1.0f }, { "orn", "OctRnd", 0, 0.0f },
    { "eqlf", "EQ LowF", 0, 0.5f }, { "eqmf", "EQ MidF", 0, 0.5f }, { "eqhf", "EQ HighF", 0, 0.5f }, { "eqq", "EQ Q", 0, 0.5f }, { "eqo", "EQ Out", 0, 0.5f }, { "pcv", "PumpCrv", 0, 0.0f }, { "rfq", "RingFrq", 0, 0.0f },
    { "dtn", "D Tone", 0, 0.5f }, { "dwd", "D Width", 0, 0.5f }, { "rwd", "R Width", 0, 0.5f }, { "rlc", "R LowCut", 0, 0.0f },
    { "gdn", "GrDens", 0, 0.4f }, { "grv", "GrRev", 0, 0.0f }, { "gfz", "GrFreeze", 0, 0.0f }, { "gtn", "GrTone", 0, 1.0f },
    { "dsw", "DrmSweep", 0, 0.5f }, { "dst", "DrmSwpTime", 0, 0.5f }, { "dnz", "DrmNoise", 0, 0.5f }, { "dck", "DrmClick", 0, 0.5f }, { "dln", "SpkLength", 0, 0.0f }, { "dmd", "SpkPlay", 0, 0.0f }, { "dlt", "SpkLetter", 0, 0.0f }, { "tkey", "TrkKey", 0, 0.0f }, { "tscl", "TrkScale", 0, 0.0f },      /* drum synth engine knobs, 0.5 = the sound as designed */
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
#define NSC 28
static const char *SCALE_NAME[NSC] = { "Minor", "Phryg", "Major", "Dorian", "Lydian", "Mixol", "Locr", "HarmMin", "MelMin", "PhrDom", "DblHarm", "HungMin", "PentMaj", "PentMin", "Blues", "MajBlue", "Hirajo", "InSen", "Iwato", "Egypt", "Whole", "DimWH", "DimHW", "BebopD", "NeapMin", "LydDom", "Altered", "Persian" };
static const char *DT_NAME[5]  = { "1/16", "1/8", "3/16", "1/4", "1/2" };
static const char *DMODE_NAME[3] = { "PING", "TAPE", "DUB" };
static const char *RMODE_NAME[3] = { "HALL", "ECHO", "SHIM" };
static const float DT_BEATS[5] = { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
static const int   SCALE_TBL[NSC][6] = { { 0, 3, 7, 12, 15, 19 }, { 0, 1, 7, 12, 13, 19 }, { 0, 4, 7, 12, 16, 19 }, { 0, 3, 7, 12, 15, 19 }, { 0, 4, 7, 12, 16, 19 }, { 0, 4, 7, 12, 16, 19 }, { 0, 3, 6, 12, 15, 18 }, { 0, 3, 7, 12, 15, 19 }, { 0, 3, 7, 12, 15, 19 }, { 0, 4, 7, 12, 16, 19 }, { 0, 4, 7, 12, 16, 19 }, { 0, 3, 7, 12, 15, 19 } , { 0, 4, 7, 12, 16, 19 }, { 0, 3, 7, 12, 15, 19 }, { 0, 3, 7, 12, 15, 19 }, { 0, 4, 7, 12, 16, 19 }, { 0, 3, 7, 12, 15, 19 }, { 0, 5, 7, 12, 17, 19 }, { 0, 5, 6, 12, 17, 18 }, { 0, 5, 7, 12, 17, 19 }, { 0, 4, 8, 12, 16, 20 }, { 0, 3, 6, 12, 15, 18 }, { 0, 3, 6, 12, 15, 18 }, { 0, 4, 7, 12, 16, 19 }, { 0, 3, 7, 12, 15, 19 }, { 0, 4, 7, 12, 16, 19 }, { 0, 4, 6, 12, 16, 18 }, { 0, 4, 6, 12, 16, 18 } };

/* ---------------- global parameters ---------------- */
enum { G_VOL, G_KEY, G_SCALE, G_DLYT, G_DLYFB, G_DLYRET, G_RVSIZE, G_RVDARK, G_RVRET, G_FILT, G_SAT, G_WID, G_COMP, G_HUM, G_MET, G_QNT, G_EQL, G_EQM, G_EQH, G_LIM, G_CHP, G_DMODE, G_DTN, G_DWD, G_RMODE, G_RWD, G_RLC, G_TAPE, G_HISS, G_WOW, G_GRV, G_GAM, G_N };
/* GROOVE templates: for each of the 16 sixteenths of a bar a delay (in steps, never early) and a velocity factor; the amount knob blends them in. Swing stays as it is and works on top. */
static const char *const GRV_NAME[11] = { "OFF", "MPC", "SHUFFLE", "LAIDBACK", "DILLA", "DRUNK", "PUSH", "FUNK", "GHOST", "HUMAN", "ROLL" };
static const float GRV_T[11][16] = {
    { 0 },
    { 0, .16f, 0, .16f, 0, .16f, 0, .16f, 0, .16f, 0, .16f, 0, .16f, 0, .16f },
    { 0, .33f, 0, .33f, 0, .33f, 0, .33f, 0, .33f, 0, .33f, 0, .33f, 0, .33f },
    { .04f, .06f, .05f, .06f, .14f, .06f, .05f, .06f, .04f, .06f, .05f, .06f, .14f, .06f, .05f, .08f },
    { 0, .22f, .05f, .30f, .12f, .20f, .02f, .28f, .10f, .24f, .04f, .32f, .14f, .18f, .06f, .26f },
    { .03f, .17f, .08f, .02f, .12f, .20f, .00f, .15f, .06f, .19f, .11f, .01f, .16f, .09f, .18f, .05f },
    { .04f, .10f, .10f, .10f, 0, .10f, .10f, .10f, .04f, .10f, .10f, .10f, 0, .10f, .10f, .10f },
    { 0, .12f, 0, .12f, 0, .12f, 0, .12f, 0, .12f, 0, .12f, 0, .12f, 0, .12f },
    { 0, .10f, 0, .10f, 0, .10f, 0, .10f, 0, .10f, 0, .10f, 0, .10f, 0, .10f },
    { .02f, .06f, 0, .05f, .03f, .07f, .01f, .05f, .02f, .06f, 0, .05f, .03f, .07f, .01f, .05f },
    { 0, .08f, .16f, .24f, 0, .08f, .16f, .24f, 0, .08f, .16f, .24f, 0, .08f, .16f, .24f },
};
static const float GRV_V[11][16] = {
    { 1 },
    { 1.0f, .90f, 1.0f, .90f, 1.0f, .90f, 1.0f, .90f, 1.0f, .90f, 1.0f, .90f, 1.0f, .90f, 1.0f, .90f },
    { 1.0f, .85f, .95f, .85f, 1.0f, .85f, .95f, .85f, 1.0f, .85f, .95f, .85f, 1.0f, .85f, .95f, .85f },
    { 1.0f, .85f, .90f, .85f, 1.0f, .85f, .90f, .85f, 1.0f, .85f, .90f, .85f, 1.0f, .85f, .90f, .85f },
    { 1.0f, .70f, .90f, .75f, 1.0f, .70f, .90f, .80f, 1.0f, .70f, .90f, .75f, 1.0f, .70f, .95f, .80f },
    { .95f, .80f, 1.0f, .75f, .90f, 1.0f, .70f, .85f, 1.0f, .80f, .75f, .95f, .85f, 1.0f, .78f, .90f },
    { 1.0f, .85f, .90f, .85f, 1.0f, .85f, .90f, .85f, 1.0f, .85f, .90f, .85f, 1.0f, .85f, .90f, .85f },
    { 1.0f, .60f, .80f, .60f, 1.0f, .60f, .80f, .65f, 1.0f, .60f, .80f, .60f, 1.0f, .65f, .85f, .70f },
    { 1.0f, .55f, .75f, .55f, 1.0f, .55f, .75f, .55f, 1.0f, .55f, .75f, .55f, 1.0f, .55f, .75f, .55f },
    { 1.0f, .90f, .95f, .90f, .95f, .90f, 1.0f, .90f, 1.0f, .90f, .95f, .90f, .95f, .90f, 1.0f, .90f },
    { 1.0f, .80f, .90f, .70f, 1.0f, .80f, .90f, .70f, 1.0f, .80f, .90f, .70f, 1.0f, .80f, .90f, .70f },
};
static const pd_t GD[G_N] = {
    { "m_vol",    "Volume", 0, 0.80f },
    { "key",      "Key",    12, 5.0f },
    { "scale",    "Scale",  NSC, 0.0f },
    { "dly_time", "D Time", 5, 1.0f },
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
    { "dly_mode", "D Type", 3, 0.0f },
    { "dly_tone", "D Tone", 0, 0.50f },
    { "dly_wide", "D Wide", 0, 0.50f },
    { "rvb_mode", "R Type", 3, 0.0f },
    { "rvb_wide", "R Wide", 0, 0.50f },
    { "rvb_low",  "R Low",  0, 0.00f },
    { "m_tape",   "Tape",   0, 0.00f },
    { "m_hiss",   "Hiss",   0, 0.00f },
    { "m_wow",    "Wow",    0, 0.00f },
    { "m_grv",    "Groove", 11, 0.0f },
    { "m_gam",    "GrvAmt", 0, 0.70f },
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
enum { T_E_MODAL = 4000, T_E_FM, T_E_VOX, T_E_CHIP, T_E_MORPH, T_E_DRUM, T_E_ANA, T_E_WT, T_E_FM4, T_E_ADD, T_E_PAD, T_E_STR, T_E_FLD, T_E_AC, T_E_FXE, T_E_SPK };
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
/* ONE attack for every engine: the Attack knob alone sets the time (0 = instant, 1 = 2 s), the same on a plucked FM bell and on a pad. Only drum sounds keep the preset's own attack. */
static inline float att_unified(float a) { return 0.0005f + a * a * 2.0f; }
static inline float att_time(int p, float a, float tatt, float base, float k2) {
    if (p < NTRK) return att_unified(a);
    float g = a * 8.0f; if (g > 1.0f) g = 1.0f; float g2 = (a - 0.10f) * 10.0f; if (g2 < 0.0f) g2 = 0.0f; if (g2 > 1.0f) g2 = 1.0f;
    return base * (0.3f + 0.7f * g) + tatt * g + a * a * k2 * g2;
}
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
    float ks[1024]; int ksi, ksn; float ksz1, ksz2;                     /* string engine: delay line */
    float nx[64]; int ni[2];                                           /* NEW ENGINES: modal / FM / vocal / chip / morph state */
    int   sw, gk; float sdet, ssub, satt;                              /* synth family options */
    double sp; double ga, g0[2]; float gp[2]; int stch; float stg; int s0, s1, sidx, smode; float srate, sgain, satk_inc, srel_c, sgate_env;                   /* sample playback */
    float nze, trp;                  /* noise follower, tremolo / auto-pan phase */
    float xfd;       /* >0: fast fade-out of a cut-off note (1 -> 0), no click */
    float aenv, ac, hd;  /* generic attack ramp for struck / plucked sounds on synth tracks (Attack knob above its default) */
} vc_t;

typedef struct {
    int   on, held, n, base, key;     /* base = note offset of a held note pad, key = which pad runs the arp (-1 none) */
    float cnt, vel, life;
} ar_t;

#define GB_SIZE 4096
#define SH_SIZE 2048
typedef struct { float xph, xz[2][4], xbuf[2][XB_SIZE], xhold[2]; int xw, xcnt, scnt, sn;
    float gb[2][GB_SIZE]; int gw, gtim, gage[4], glen[4]; float gpos[4], ginc[4], gpl[4], gpr[4]; unsigned grng;
    int fi, fxw, fxi[12]; float xdl, fxf[8]; } xs_t;      /* fi: the delay / reverb memory this insert slot uses (0 = none), see fxm in inst_t */
#define FXM_N 49152
#define FXM_SLOTS 10
#define REP_N 8                                       /* note repeat: up to 8 pads repeat at the same time */
typedef struct {
    float pp[NLANE][PP_N];
    float fdk[NLANE];
    float g[G_N];
    int   focused;                  /* 1..32 */

    float sr, inv_sr;
    float tp_dl[2][2048], tp_lp[2], tp_hb[2], tp_hn[2], tp_ph1, tp_ph2, tp_dr, tp_drv; int tp_w; unsigned tp_rng;   /* master tape: wow / flutter delay, soft saturation, head bump, hiss */
    float root_hz;
    int mo_clk, mo_trk, mo_chn, mo_xp, mo_vp, mo_dn; float mclk; struct { int cnt; uint8_t st, d1; } mo_q[64];   /* MIDI out: clock + notes per track */
    int16_t *rec_buf; long rec_n, rec_len, rec_max; int rec_state, rec_bars, rec_src; char rec_last[48]; int rec_slot;   /* resampling (rec_src -2 = the Move's audio input, see mic_block) */
    int mic_mon, mic_clip, mic_ok, rfx, rnorm, rfx_cnt; float *rfx_mem; long rfx_cap, rec_in_n; int rfx_dln, rfx_dp, rfx_len[12], rfx_off[12], rfx_cp[12]; float rfx_lp[8]; float mic_gain, mic_thr, mic_peak, ramt, rfx_hx[2], rfx_hy[2], rfx_hold[2];   /* MIC take: monitor flag (level only while the MIC page is open), gain, start threshold, smoothed input peak */
    float sc_w[128], sc_lp[8], sc_env[8]; int sc_i, sc_c;
    float sc_lp2[16], sc_env2[16], sc_sl[64], sc_sr[64], sc_cll, sc_crr, sc_clr; int sc_si;   /* 16-band spectrum, stereo ring and phase correlation for the extra scope pages */            /* master scope ring (decimated) and 8 band levels for the UI scope page */
    float dk_env[NLANE], dk_c[NLANE], meter[NTRK];                 /* sidechain ducking (kick -> other tracks) and level meters */
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
    /* shared effects bus: ONE delay + ONE reverb for the whole mix; every track feeds them with its send amounts */
    float dlL[DLY_SIZE], dlR[DLY_SIZE];
    int   dw;
    float dly_z[2], dly_hp[2], dly_ph;
    float shb[SH_SIZE]; float sh_ph; int shw;
    float rv[NLINES][RV_SIZE];
    float rv_z[NLINES];
    int   rw;
    int   rv_len[NLINES];
    float rv_g[NLINES], rv_a;
    float rlc_z[2];
    float dt_buf[12][DT_SIZE]; int dt_w, dt_len[26]; float dt_bw, dt_zl, dt_zr, dt_lfo, dt_dec;     /* Dattorro plate */
    int   fx_act[NLANE], dr_act, dr_quiet;
    /* activity lists (rebuilt every 16 samples or when a voice / arp / ratchet starts): the per-sample loops only visit what is really sounding */
    int   act_dirty, vl_n, al_n, rl_n; short vl[NLANE + NGH], al[NLANE], rl[NLANE]; uint64_t fxm[2];
    float bt_l[NLANE], bt_r[NLANE], bt_sd[NLANE], bt_sr[NLANE]; int bt_act[NLANE];
    int   tgen[NLANE];                         /* trigger counter per lane (MIDI effect layers follow it) */
    struct { int cnt, lane, semi, follow; float vel; } eq[32];   /* delayed notes: echo + strum */
    /* per-track insert effect (10 types): lfo phase, filter states, delay buffers, sample-hold */
    xs_t xs[2][NLANE];
    float xmem[FXM_SLOTS][2][FXM_N]; unsigned xmem_used;     /* memory of the DELAY / REVERB insert effects: a pool, only the slots that use one take a block */
    float sbuf[NLANE][2][SB_SIZE];

    /* sequencer */
    uint8_t stp[NPAT][NLANE][MAXSTEP][7];   /* a = on<<7 | vel, b = probability 0..7, c = semi + 64, d = ratchet | gate<<2 | cond<<5, e = nudge 0..7, f = parameter lock value 0 (none) .. 127 */
    uint8_t plk_t[NLANE]; float plk_b[NLANE]; float cdet_z[2];   /* per-lane lock target (PP index, 0 = none), its base value, compressor detector HPF state */
    int   seqlen[NTRK], mute[NTRK]; uint8_t rechold[NTRK];
    int tph[NTRK];      /* a clip that was recorded from a bar line in the middle of the run starts at that bar line: its own step 0 = this global step */
    int rskip[NLANE];   /* a note recorded live must not sound a second time when its step is still ahead (the pad already played it): holds the absolute step + 1 */   /* rechold: a clip that is being recorded for the first time does not play back until the recording is finished */
    uint8_t lmute[NLANE];            /* per-sound mute (drum pads): the sequencer skips these lanes */
    int   llen[NLANE];                      /* own loop length of one lane (0 = follow the track) */
    int   playing, cur_pat, sel_track, gs;
    int   sub;                              /* tick inside the current 1/16 (0..5): the grid is 6 ticks per 1/16 so 1/32 and triplets are real steps */
    int   tsn[NTRK], tsd[NTRK];            /* track speed as a fraction (tsn/tsd, 0 = 1): 1/2 = half time, 3/2 = one and a half times as fast */
    int   tres[NTRK];                       /* ticks per step of a track's grid: 6 = 1/16 (default), 3 = 1/32, 4 = 1/16T, 8 = 1/8T, 12 = 1/8 */
    int   clip[NTRK], qclip[NTRK];   /* active clip per track (-1 = stopped), pending (-2 = none) */
    int   scq[NPAT], pend_on, pend_qm;   /* launch quantisation of one scene (0 = global setting) and the one used by the pending scene launch */
    float bpm, swing;
    float pump_ph, wob_ph;
    float mf_z[2][2], mf_a, comp_env; int mf_mode;
    float eq_c[NLANE][3][5], eq_z[NLANE][3][2][2], eq_g[NLANE][3], eq_p[NLANE][4], eq_og[NLANE], eq_ogp[NLANE]; int eq_on[NLANE];       /* per-sound 3-band EQ (low shelf, peak, high shelf) */
    float rm_buf[4][4096], rm_lp, rm_env, rm_hp; int rm_w, rm_act;       /* kick rumble bus: dark comb reverb, ducked by the kicks */
    float eq_lo[2], eq_hi[2], lim_g;                              /* master EQ band splitters + limiter gain */
    float met_env, met_ph, met_f, met_amp;
    int   ci_left, ci_t, ci_b;                 /* count-in: samples left, samples elapsed, samples per beat */
    uint8_t fol[NTRK][NPAT];                   /* follow action per clip: bits 0-2 action, bits 3-5 loops index */
    uint8_t fol_cnt[NTRK];                     /* loops played of the running clip */
    int pads_off, pad_oct, lay_s; float cpu_avg, cpu_peak;
    double tsamp, next_time, last_time;
    int   gate_left[NLANE];
    int   rt_n[NLANE], rt_semi[NLANE], rt_gate[NLANE]; float rt_cnt[NLANE], rt_int[NLANE], rt_vel[NLANE];   /* ratchet / nudge scheduler */
    float cpp[NPAT][NLANE][PP_N]; uint8_t cpp_set[NPAT][NLANE];   /* sound of every clip */
    int   np_trk[32];                      /* track sounding on each note pad */
    int   rep_idx, rp_on[REP_N], rp_t[REP_N], rp_k[REP_N], rp_rate[REP_N], rp_ramp[REP_N]; float rp_semi[REP_N], rp_vel[REP_N], rp_cnt[REP_N], rp_cap[REP_N];   /* note repeat */

    float lf_buf[2][131072]; float lf_dl[2][65536]; float lf_z[2][4], lf_eng, lf_rpos, lf_rate, lf_gate, lf_hold[2], lf_lvl;       /* LIVE FX on the master bus: one pad = one performance effect */
    struct { float buf[2][GRN_SZ], pos[GRN_G], inc[GRN_G], pl[GRN_G], pr[GRN_G]; int w, tim, age[GRN_G], len[GRN_G], back[GRN_G], lane; unsigned rng; float lpl, lpr; } gr[GRN_SLOTS]; int gr_slot[NLANE];     /* granulator: a few shared slots, a sound that uses it takes one */
    int   lf_w, lf_req, lf_mode, lf_t, lf_cap, lf_len, lf_n, lf_dw, lf_dact, lf_hc;
    int   in_use;
    int spk_wi[NLANE];                                             /* SPEECH, one word per note: the next word of every lane (back to the first at every loop start) */
    char spk_txt[NLANE][48];                                       /* SPEECH engine: the text of every lane */
} inst_t;
/* LIVE FX (master bus, held pad = effect on, release = fade back):
 * 1-4 REPEAT 1/4 1/8 1/16 1/32   5 ROLL (repeats get shorter)   6 TRIPLET   7 REVERSE   8 TAPE STOP   9-10 GATE 1/8 1/16
 * 11 LOW SWEEP   12 HIGH SWEEP   13 CRUSH   14 FREEZE   15 ECHO THROW (the tail rings on after release) */
/* BEADS granular per sound: windowed grains read from the last 3 s of the sound (short AND long grains). Size = grain length, Spread = how far back in time, Pitch = random octave / fifth grains.
 * Only GRN_SLOTS sounds can use it at once (a slot is taken while Grain > 0 and given back when it is turned down). */
static int gran_slot(inst_t *s, int t, float mix) {
    int sl = s->gr_slot[t] - 1;
    if (mix < 0.01f) { if (sl >= 0) { s->gr_slot[t] = 0; for (int g = 0; g < GRN_G; g++) s->gr[sl].age[g] = s->gr[sl].len[g]; } return -1; }
    if (sl >= 0) return sl;
    for (int k = 0; k < GRN_SLOTS; k++) {
        int used = 0; for (int q = 0; q < NLANE; q++) if (s->gr_slot[q] == k + 1) used = 1;
        if (used) continue;
        memset(&s->gr[k], 0, sizeof s->gr[k]); s->gr[k].lane = t; s->gr_slot[t] = k + 1; return k;
    }
    return -1;
}
static float g_win[5][257]; static int g_win_ok;
static void gwin_init(void) {                             /* grain windows: 0 rect (short fades), 1 triangle, 2 sine, 3 gauss, 4 hann (the original) */
    for (int k = 0; k <= 256; k++) {
        const float ph = (float)k / 256.0f, e = ph < 0.5f ? ph : 1.0f - ph;
        const float d = ph - 0.5f, hs = sinf(3.14159265f * ph);
        g_win[0][k] = e * 12.0f < 1.0f ? e * 12.0f : 1.0f;
        g_win[1][k] = 2.0f * e;
        g_win[2][k] = hs;
        g_win[3][k] = expf(-20.0f * d * d);
        g_win[4][k] = hs * hs;
    }
    g_win_ok = 1;
}
static void gran_proc(inst_t *s, int t, const float *q, float *Lp, float *Rp) {
    const float mix = q[PP_GRN];
    const int sl = gran_slot(s, t, mix);
    if (sl < 0) return;
    __typeof__(s->gr[0]) *G = &s->gr[sl];
    const int w = G->w;
    if (q[PP_GFZ] < 0.5f) { G->buf[0][w] = *Lp; G->buf[1][w] = *Rp; G->w = (w + 1) & (GRN_SZ - 1); }      /* Freeze: the buffer stops listening, the grains keep looping what it holds */
    if (!G->rng) G->rng = 2463534242u + (unsigned)t * 977u;
    /* BEADS: Size is exponential, 4 ms (micro grains, a buzzing cloud) up to 4.5 s (very long grains, a smeared time-stretch); Density = overlap, with a little random timing */
    const int glen = (int)(0.004f * powf(1125.0f, q[PP_GSZ] < 0.0f ? 0.0f : (q[PP_GSZ] > 1.0f ? 1.0f : q[PP_GSZ])) * s->sr);
    const float ov = 1.0f + q[PP_GDN] * 7.0f;
    if (--G->tim <= 0) {
        G->rng ^= G->rng << 13; G->rng ^= G->rng >> 17; G->rng ^= G->rng << 5;
        G->tim = (int)((float)glen / ov * (0.75f + 0.5f * (float)(G->rng & 1023) / 1023.0f)) + 1;
        for (int g = 0; g < GRN_G; g++) if (G->age[g] >= G->len[g]) {
            G->rng ^= G->rng << 13; G->rng ^= G->rng >> 17; G->rng ^= G->rng << 5;
            const float r1 = (float)(G->rng & 1023) / 1023.0f;
            G->rng ^= G->rng << 13; G->rng ^= G->rng >> 17; G->rng ^= G->rng << 5;
            const float r2 = (float)(G->rng & 1023) / 1023.0f;
            const float pi = q[PP_GPI];
            float ratio = 1.0f;
            if (r2 < pi) ratio = r2 < pi * 0.3f ? 2.0f : (r2 < pi * 0.6f ? 0.5f : (r2 < pi * 0.8f ? 1.5f : 0.75f));
            else if (pi > 0.05f) ratio = 1.0f + (r1 - 0.5f) * 0.04f * pi;
            if ((float)((G->rng >> 10) & 1023) / 1023.0f < q[PP_GRV]) ratio = -ratio;                    /* some grains play backwards */
            int back = glen + 64 + (int)(r1 * q[PP_GSP] * (float)(GRN_SZ - 2 * glen - 200));
            if (back > GRN_SZ - 64) back = GRN_SZ - 64;
            G->len[g] = glen; G->age[g] = 0; G->inc[g] = ratio; G->back[g] = back;
            G->pos[g] = (float)((w - back + GRN_SZ) & (GRN_SZ - 1));
            const float pn = (r2 * 2.0f - 1.0f) * (0.2f + 0.8f * q[PP_GSP]);
            G->pl[g] = 0.7071f * (1.0f - pn * 0.7f); G->pr[g] = 0.7071f * (1.0f + pn * 0.7f);
            break;
        }
    }
    if (!g_win_ok) gwin_init();
    const float wm = (q[PP_GTN] < 0.0f ? 0.0f : (q[PP_GTN] > 1.0f ? 1.0f : q[PP_GTN])) * 4.0f; int wi = (int)wm; if (wi > 3) wi = 3; const int wj = wi + 1; const float wf = wm - (float)wi;      /* Window: morphs between the five shapes */
    float gl = 0.0f, gr = 0.0f;
    for (int g = 0; g < GRN_G; g++) {
        if (G->age[g] >= G->len[g]) continue;
        const float ph = (float)G->age[g] / (float)G->len[g], pk = ph * 256.0f; const int pi_ = (int)pk; const float pf = pk - (float)pi_;
        const float wa = g_win[wi][pi_] * (1.0f - pf) + g_win[wi][pi_ + 1] * pf, wb = g_win[wj][pi_] * (1.0f - pf) + g_win[wj][pi_ + 1] * pf, w2 = wa * (1.0f - wf) + wb * wf;
        const int i0 = (int)G->pos[g] & (GRN_SZ - 1), i1 = (i0 + 1) & (GRN_SZ - 1); const float fr = G->pos[g] - floorf(G->pos[g]);
        gl += (G->buf[0][i0] * (1.0f - fr) + G->buf[0][i1] * fr) * w2 * G->pl[g];
        gr += (G->buf[1][i0] * (1.0f - fr) + G->buf[1][i1] * fr) * w2 * G->pr[g];
        G->pos[g] += G->inc[g]; if (G->pos[g] >= (float)GRN_SZ) G->pos[g] -= (float)GRN_SZ; else if (G->pos[g] < 0.0f) G->pos[g] += (float)GRN_SZ;
        G->age[g]++;
    }
    if (!(fabsf(gl) < 8.0f) || !(fabsf(gr) < 8.0f)) { gl = gr = 0.0f; memset(G->buf, 0, sizeof G->buf); }
    const float nrm = 1.4f / sqrtf(ov + 0.4f);                       /* dense clouds do not get louder */
    *Lp = *Lp * (1.0f - 0.55f * mix) + gl * 0.95f * nrm * mix; *Rp = *Rp * (1.0f - 0.55f * mix) + gr * 0.95f * nrm * mix;
}
#define LF_SIZE 131072
#define LF_MASK (LF_SIZE - 1)
static void lfx_proc(inst_t *s, float *Lp, float *Rp) {
    const float in0 = *Lp, in1 = *Rp;
    float o0 = in0, o1 = in1;
    const int req = s->lf_req;
    const float beat = 60.0f / s->bpm * s->sr;
    if (req != s->lf_mode && req > 0) {                                      /* a pad went down: remember the moment, the effect works on what was just played */
        s->lf_mode = req; s->lf_t = 0; s->lf_n = 0; s->lf_cap = s->lf_w; s->lf_rpos = 0.0f; s->lf_rate = 1.0f; s->lf_gate = 1.0f; s->lf_hc = 0;
        for (int c_ = 0; c_ < 2; c_++) for (int k_ = 0; k_ < 4; k_++) s->lf_z[c_][k_] = 0.0f;
        switch (req) {
            case 1: s->lf_len = (int)(beat); break;
            case 2: s->lf_len = (int)(beat * 0.5f); break;
            case 3: s->lf_len = (int)(beat * 0.25f); break;
            case 4: s->lf_len = (int)(beat * 0.125f); break;
            case 5: s->lf_len = (int)(beat * 0.5f); break;
            case 6: s->lf_len = (int)(beat / 3.0f); break;
            case 7: s->lf_len = (int)(beat * 0.5f); break;
            case 14: s->lf_len = (int)(0.07f * s->sr); break;
            case 17: s->lf_len = (int)(beat * 0.5f); break;
            case 29: s->lf_len = (int)(beat * 0.375f); break;
            default: s->lf_len = (int)beat; break;
        }
        if (s->lf_len < 64) s->lf_len = 64; if (s->lf_len > 60000) s->lf_len = 60000;
    }
    const int mode = s->lf_mode;
    const int on = req > 0 && mode > 0;
    const float tg = on ? 1.0f : 0.0f;
    s->lf_eng += (tg - s->lf_eng) * (on ? 0.012f : 0.006f);
    const int capmode = (mode >= 1 && mode <= 7) || mode == 14 || mode == 16 || mode == 17 || mode == 29;
    if (!(capmode && on)) { s->lf_buf[0][s->lf_w] = in0; s->lf_buf[1][s->lf_w] = in1; s->lf_w = (s->lf_w + 1) & LF_MASK; }
    if (mode == 15 || s->lf_dact > 0) {                                       /* ECHO THROW: dotted-eighth echo, input only while the pad is down */
        const int d = (int)(beat * 0.75f) < 60000 ? (int)(beat * 0.75f) : 60000;
        const int rp = (s->lf_dw - d + 65536) & 65535;
        const int held = (mode == 15 && on);
        if (held) s->lf_dact = (int)(4.0f * s->sr); else if (s->lf_dact > 0) s->lf_dact--;
        float y0 = s->lf_dl[0][rp], y1 = s->lf_dl[1][rp];
        s->lf_dl[0][s->lf_dw] = (held ? in0 : 0.0f) + y1 * 0.62f; s->lf_dl[1][s->lf_dw] = (held ? in1 : 0.0f) + y0 * 0.62f;      /* ping-pong */
        s->lf_dw = (s->lf_dw + 1) & 65535;
        o0 += y0 * 0.85f; o1 += y1 * 0.85f;
        if (s->lf_dact == 0) memset(s->lf_dl, 0, sizeof s->lf_dl);
        s->lf_t++;
        if (!on && s->lf_dact == 0) s->lf_mode = 0;
        *Lp = o0; *Rp = o1; return;
    }
    if (mode == 0) { *Lp = o0; *Rp = o1; return; }
    const int t = s->lf_t++;
    float w0 = in0, w1 = in1;
    if ((mode >= 1 && mode <= 7) || mode == 29) {
        int len = s->lf_len;
        int ph;
        if (mode == 5) {                                                          /* ROLL: every repeat is 15 % shorter, down to 1/64 */
            const int pos = s->lf_hc; ph = pos;
            if (ph >= len) { s->lf_hc = ph = 0; s->lf_n++; s->lf_len = len = (int)((float)len * 0.85f); const int mn = (int)(beat / 64.0f); if (len < mn) s->lf_len = len = mn; }
            s->lf_hc++;
        } else { ph = t % len; }
        int idx;
        if (mode == 7) idx = (s->lf_cap - 1 - ph) & LF_MASK; else idx = (s->lf_cap - len + ph) & LF_MASK;
        float g = 1.0f; const int fd = len < 400 ? len / 4 : 96;
        if (ph < fd) g = (float)ph / (float)fd; else if (len - ph < fd) g = (float)(len - ph) / (float)fd;
        w0 = s->lf_buf[0][idx] * g; w1 = s->lf_buf[1][idx] * g;
    } else if (mode == 8 || mode == 30) {                                         /* TAPE STOP (0.5 s) / BRAKE (0.15 s): the playback slows down like a stopping reel */
        float u = (float)t / ((mode == 8 ? 0.5f : 0.15f) * s->sr); if (u > 1.0f) u = 1.0f;
        const float r = (1.0f - u) * (1.0f - u);
        s->lf_rpos += r;
        const float fp = (float)(s->lf_cap + LF_SIZE - 1) + s->lf_rpos;                 /* starts at "now", falls behind as the speed drops */
        const int i0 = (int)fp & LF_MASK, i1 = (i0 + 1) & LF_MASK; const float fr = fp - floorf(fp);
        w0 = (s->lf_buf[0][i0] * (1.0f - fr) + s->lf_buf[0][i1] * fr) * (0.2f + 0.8f * r);
        w1 = (s->lf_buf[1][i0] * (1.0f - fr) + s->lf_buf[1][i1] * fr) * (0.2f + 0.8f * r);
        if (u >= 1.0f) { w0 = w1 = 0.0f; }
    } else if (mode == 9 || mode == 10) {                                         /* GATE 1/8, 1/16 */
        const float per = mode == 9 ? beat * 0.5f : beat * 0.25f;
        const float ph2 = fmodf((float)t, per) / per;
        const float tgt = ph2 < 0.5f ? 1.0f : 0.04f;
        s->lf_gate += 0.02f * (tgt - s->lf_gate);
        w0 = in0 * s->lf_gate; w1 = in1 * s->lf_gate;
    } else if (mode == 11 || mode == 12) {                                        /* LOW SWEEP: closes a low-pass; HIGH SWEEP: opens a high-pass (both over two beats) */
        float u = (float)t / (2.0f * beat); if (u > 1.0f) u = 1.0f;
        const float fc = mode == 11 ? 14000.0f * powf(150.0f / 14000.0f, u) : 40.0f * powf(3500.0f / 40.0f, u);
        const float a = 1.0f - expf(-6.2831853f * fc * s->inv_sr);
        float in2[2] = { in0, in1 };
        for (int c = 0; c < 2; c++) {
            float *z = s->lf_z[c];
            z[0] += a * (in2[c] - z[0]); z[1] += a * (z[0] - z[1]); z[2] += a * (z[1] - z[2]);
            in2[c] = mode == 11 ? z[2] * 1.2f : (in2[c] - z[2]) * 1.1f;
        }
        w0 = in2[0]; w1 = in2[1];
    } else if (mode == 13) {                                                      /* CRUSH: low sample rate, few bits */
        if (--s->lf_hc <= 0) { s->lf_hc = 10; s->lf_hold[0] = floorf(in0 * 24.0f + 0.5f) / 24.0f; s->lf_hold[1] = floorf(in1 * 24.0f + 0.5f) / 24.0f; }
        w0 = s->lf_hold[0] * 1.1f; w1 = s->lf_hold[1] * 1.1f;
    } else if (mode == 14) {                                                      /* FREEZE: the last 70 ms as a sustained cloud (two overlapping windowed readers) */
        const int len = s->lf_len;
        float ac0 = 0.0f, ac1 = 0.0f;
        for (int k = 0; k < 2; k++) {
            const int ph = (t + k * (len / 2)) % len;
            const float win = sinf(3.14159265f * (float)ph / (float)len);
            const int idx = (s->lf_cap - len + ph) & LF_MASK;
            ac0 += s->lf_buf[0][idx] * win * win; ac1 += s->lf_buf[1][idx] * win * win;
        }
        w0 = ac0; w1 = ac1;
    } else if (mode == 16 || mode == 17) {                                        /* HALF SPEED (an octave down) / DOUBLE SPEED (an octave up): the last beat loops at another speed */
        const int len = s->lf_len; const float rate = mode == 16 ? 0.5f : 2.0f;
        const float ph = fmodf((float)t * rate, (float)len), fp = (float)(s->lf_cap + LF_SIZE - len) + ph;
        const int i0 = (int)fp & LF_MASK, i1 = (i0 + 1) & LF_MASK; const float fr = fp - floorf(fp);
        float g = 1.0f; const float fd = 160.0f; if (ph < fd) g = ph / fd; else if ((float)len - ph < fd) g = ((float)len - ph) / fd;
        w0 = (s->lf_buf[0][i0] * (1.0f - fr) + s->lf_buf[0][i1] * fr) * g; w1 = (s->lf_buf[1][i0] * (1.0f - fr) + s->lf_buf[1][i1] * fr) * g;
    } else if (mode == 18 || mode == 19) {                                        /* WOBBLE: tape wow (pitch sways) / FLANGER: a short sweeping delay mixed with the dry sound */
        const float tt = (float)t * s->inv_sr;
        const float d = mode == 18 ? (0.010f + 0.007f * sin2pi(4.5f * tt)) * s->sr : (0.0008f + 0.0030f * (0.5f + 0.5f * sin2pi(0.3f * tt))) * s->sr;
        const float fp = (float)(s->lf_w + LF_SIZE - 1) - d;
        const int i0 = (int)fp & LF_MASK, i1 = (i0 + 1) & LF_MASK; const float fr = fp - floorf(fp);
        const float y0 = s->lf_buf[0][i0] * (1.0f - fr) + s->lf_buf[0][i1] * fr, y1 = s->lf_buf[1][i0] * (1.0f - fr) + s->lf_buf[1][i1] * fr;
        if (mode == 18) { w0 = y0; w1 = y1; } else { w0 = 0.6f * (in0 + y0); w1 = 0.6f * (in1 + y1); }
    } else if (mode == 20) {                                                      /* RING MOD: metallic, bell-like */
        const float o = sin2pi(233.0f * (float)t * s->inv_sr); w0 = in0 * o * 1.3f; w1 = in1 * o * 1.3f;
    } else if (mode == 21) {                                                      /* AUTO PAN: left <> right once per beat */
        const float p = 0.5f + 0.5f * sin2pi((float)t / beat); w0 = in0 * 1.6f * p; w1 = in1 * 1.6f * (1.0f - p);
    } else if (mode == 22) {                                                      /* TREMOLO: 1/16 notes */
        const float g = 0.12f + 0.88f * (0.5f + 0.5f * sin2pi((float)t / (beat * 0.25f))); w0 = in0 * g; w1 = in1 * g;
    } else if (mode == 23) {                                                      /* WAH SWEEP: a band-pass opens and closes once per two beats */
        const float fc = 450.0f + 2300.0f * (0.5f + 0.5f * sin2pi((float)t / (beat * 2.0f)));
        const float f = 2.0f * sinf(3.14159265f * fc * s->inv_sr); float in2[2] = { in0, in1 };
        for (int c = 0; c < 2; c++) { float *z = s->lf_z[c]; z[0] += f * z[1]; const float hp = in2[c] - z[0] - 0.22f * z[1]; z[1] += f * hp; in2[c] = z[1] * 0.25f; }
        w0 = in2[0]; w1 = in2[1];
    } else if (mode == 24 || mode == 25 || mode == 26) {                          /* BASS CUT (high-pass 450 Hz) / TELEPHONE (500 - 3000 Hz) / UNDERWATER (low-pass 350 Hz) */
        const float a1 = 1.0f - expf(-6.2831853f * (mode == 24 ? 450.0f : (mode == 25 ? 500.0f : 350.0f)) * s->inv_sr), a2 = 1.0f - expf(-6.2831853f * 3000.0f * s->inv_sr);
        float in2[2] = { in0, in1 };
        for (int c = 0; c < 2; c++) {
            float *z = s->lf_z[c]; z[0] += a1 * (in2[c] - z[0]);
            if (mode == 24) { z[1] += a1 * (z[0] - z[1]); in2[c] = in2[c] - z[1]; }
            else if (mode == 25) { const float hp = in2[c] - z[0]; z[1] += a2 * (hp - z[1]); in2[c] = z[1] * 1.5f; }
            else { z[1] += a1 * (z[0] - z[1]); z[2] += a1 * (z[1] - z[2]); in2[c] = z[2] * 1.6f; }
        }
        w0 = in2[0]; w1 = in2[1];
    } else if (mode == 27) {                                                      /* DISTORT: heavy soft clipping */
        w0 = tanhf(in0 * 7.0f) * 0.5f; w1 = tanhf(in1 * 7.0f) * 0.5f;
    } else if (mode == 28) {                                                      /* KILL: everything goes silent while the pad is down */
        w0 = w1 = 0.0f;
    } else if (mode == 31) {                                                      /* PUMP: the level ducks on every beat and swells back */
        const float ph2 = fmodf((float)t, beat) / beat, g = 0.1f + 0.9f * (1.0f - expf(-ph2 * 6.0f)); w0 = in0 * g; w1 = in1 * g;
    }
    if (!(fabsf(w0) < 8.0f) || !(fabsf(w1) < 8.0f)) { w0 = w1 = 0.0f; }
    const float e = s->lf_eng;
    o0 = in0 * (1.0f - e) + w0 * e; o1 = in1 * (1.0f - e) + w1 * e;
    if (!on && s->lf_eng < 0.002f) { s->lf_mode = 0; s->lf_eng = 0.0f; }
    *Lp = o0; *Rp = o1;
}
/* one audio insert slot (type 0 = off); two of them in a row per lane = the effect chain */
static void xfx_proc(inst_t *s, xs_t *X, float (*sbuf)[SB_SIZE], int xt, float am, float pa, float pb, float *lp, float *rp) {
    float l = *lp, r_ = *rp;
    if (X->fi && xt != 12 && xt != 13) { s->xmem_used &= ~(1u << (X->fi - 1)); X->fi = 0; }       /* the effect was changed: give its memory back */
    if ((xt == 12 || xt == 13) && !X->fi) {
        int got = 0;
        for (int i = 0; i < FXM_SLOTS; i++) if (!(s->xmem_used & (1u << i))) { s->xmem_used |= 1u << i; X->fi = i + 1; memset(s->xmem[i], 0, sizeof s->xmem[i]); memset(X->fxi, 0, sizeof X->fxi); memset(X->fxf, 0, sizeof X->fxf); X->fxw = 0; X->xdl = 0.0f; got = 1; break; }
        if (!got) xt = 0;                                          /* all ten delay / reverb inserts are in use: this one stays off */
    }
    if ((xt == 1 || xt == 2) && !sbuf) xt = 0;                     /* stutter / reverse need the capture buffer: slot 1 only */
                                if (xt > 0 && xt <= 13) {
                    float in2[2] = { l, r_ }, out2[2] = { l, r_ };
                    float *z = &X->xz[0][0];
                    const float wet2 = pb * 2.0f > 1.0f ? 1.0f : pb * 2.0f;
                    if (xt == 1 || xt == 2) {                                   /* 1 STUTTER / 2 REVERSE: capture a slice (1/8 .. 1/64 beat), then repeat it (or play it backwards) for some of every 4 slices */
                        static const float SD[4] = { 2.0f, 4.0f, 8.0f, 16.0f };
                        int di = (int)(pa * 3.99f); if (di < 0) di = 0; if (di > 3) di = 3;
                        int ls = (int)(60.0f / s->bpm * s->sr / SD[di]); if (ls < 64) ls = 64; if (ls > SB_SIZE) ls = SB_SIZE;
                        int cn = X->scnt; if (cn >= ls) { cn = 0; X->sn++; }
                        const int hold = (X->sn & 3) < (int)(pb * 3.0f + 0.5f);
                        if (hold) {
                            const int ri = xt == 1 ? cn : ls - 1 - cn;
                            float g = 1.0f; if (cn < 48) g = (float)cn / 48.0f; else if (ls - cn < 48) g = (float)(ls - cn) / 48.0f;
                            for (int c = 0; c < 2; c++) out2[c] = in2[c] * (1.0f - am) + sbuf[c][ri] * g * am;
                        } else {
                            for (int c = 0; c < 2; c++) sbuf[c][cn] = in2[c];
                        }
                        X->scnt = cn + 1;
                    } else if (xt == 3 || xt == 6) {                            /* FLANGER / COMB */
                        const int w = X->xw;
                        if (xt == 3) {
                            X->xph += (0.05f + pa * pa * 5.0f) * s->inv_sr; if (X->xph >= 1.0f) X->xph -= 1.0f;
                        }
                        for (int c = 0; c < 2; c++) {
                            float ph = X->xph + 0.25f * (float)c; if (ph >= 1.0f) ph -= 1.0f;
                            float d = xt == 3 ? (0.0005f + (0.5f + 0.5f * sin2pi(ph)) * 0.0035f) * s->sr : 22.0f + pa * 400.0f;
                            if (d > (float)(XB_SIZE - 4)) d = (float)(XB_SIZE - 4);
                            float pos = (float)w - d; if (pos < 0.0f) pos += (float)XB_SIZE;
                            int i0 = (int)pos; float fr = pos - (float)i0; i0 &= (XB_SIZE - 1); int i1 = (i0 + 1) & (XB_SIZE - 1);
                            float y = X->xbuf[c][i0] * (1.0f - fr) + X->xbuf[c][i1] * fr;
                            float fb = xt == 3 ? pb * 0.85f : 0.2f + pb * 0.7f;
                            X->xbuf[c][w] = in2[c] + y * fb;
                            out2[c] = xt == 3 ? in2[c] + y * am : in2[c] * (1.0f - 0.4f * am) + y * am * 0.7f;
                        }
                        X->xw = (w + 1) & (XB_SIZE - 1);
                    } else if (xt == 4 || xt == 5) {                            /* TREMOLO / AUTO-PAN */
                        X->xph += (0.5f + pa * pa * 14.0f) * s->inv_sr; if (X->xph >= 1.0f) X->xph -= 1.0f;
                        if (xt == 4) {
                            for (int c = 0; c < 2; c++) {
                                float ph = X->xph + 0.5f * pb * (float)c; if (ph >= 1.0f) ph -= 1.0f;
                                out2[c] = in2[c] * (1.0f - am * (0.5f + 0.5f * sin2pi(ph)));
                            }
                        } else {
                            float lfo = tanhf(sin2pi(X->xph) * (1.0f + pb * 8.0f)) * am;
                            out2[0] = in2[0] * (1.0f - (lfo > 0.0f ? lfo : 0.0f));
                            out2[1] = in2[1] * (1.0f + (lfo < 0.0f ? lfo : 0.0f));
                        }
                    } else if (xt == 7) {                                       /* AUTO-WAH (state variable band-pass) */
                        X->xph += (0.2f + pa * pa * 6.0f) * s->inv_sr; if (X->xph >= 1.0f) X->xph -= 1.0f;
                        const float fc = 300.0f + 2400.0f * (0.5f + 0.5f * sin2pi(X->xph));
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
                        X->xph += s->bpm / 60.0f * DIV[di] * s->inv_sr; if (X->xph >= 1.0f) X->xph -= 1.0f;
                        const float tgt = X->xph < 0.5f ? 1.0f : 1.0f - am, k = 0.003f + (1.0f - pb) * 0.2f;
                        X->xhold[0] += k * (tgt - X->xhold[0]);
                        out2[0] = in2[0] * X->xhold[0]; out2[1] = in2[1] * X->xhold[0];
                    } else if (xt == 9) {                                       /* LO-FI: sample-rate + bit reduction */
                        const int n = 1 + (int)(am * 15.0f);
                        const float lv = 8.0f + (1.0f - pa) * 2000.0f;
                        if (++X->xcnt >= n) { X->xcnt = 0; for (int c = 0; c < 2; c++) X->xhold[c] = floorf(in2[c] * lv + 0.5f) / lv; }
                        for (int c = 0; c < 2; c++) out2[c] = in2[c] * (1.0f - wet2) + X->xhold[c] * wet2;
                    } else if (xt == 11) {                                      /* GRANULATOR: short windowed grains of the last ~90 ms, scattered in time, pitch and pan */
                        const int w = X->gw;
                        X->gb[0][w] = in2[0]; X->gb[1][w] = in2[1];
                        X->gw = (w + 1) & (GB_SIZE - 1);
                        if (!X->grng) X->grng = 2463534242u;
                        const int glen = 441 + (int)(pa * pa * 1700.0f);                 /* 10 .. 49 ms */
                        if (--X->gtim <= 0) {
                            X->gtim = glen / 3 + 1;
                            for (int g = 0; g < 4; g++) if (X->gage[g] >= X->glen[g]) {
                                X->grng ^= X->grng << 13; X->grng ^= X->grng >> 17; X->grng ^= X->grng << 5;
                                const float r1 = (float)(X->grng & 1023) / 1023.0f;
                                X->grng ^= X->grng << 13; X->grng ^= X->grng >> 17; X->grng ^= X->grng << 5;
                                const float r2 = (float)(X->grng & 1023) / 1023.0f;
                                float ratio = 1.0f;
                                if (r2 < pb * 0.9f) ratio = r2 < pb * 0.3f ? 2.0f : (r2 < pb * 0.6f ? 0.5f : 1.5f);
                                int back = (int)((float)glen * ratio) + 8 + (int)(r1 * pb * 1500.0f);
                                if (back > GB_SIZE - 8) back = GB_SIZE - 8;
                                X->glen[g] = glen; X->gage[g] = 0; X->ginc[g] = ratio;
                                X->gpos[g] = (float)((X->gw - back + GB_SIZE) & (GB_SIZE - 1));
                                const float pn = (r1 * 2.0f - 1.0f) * pb;
                                X->gpl[g] = 0.7071f * (1.0f - pn * 0.8f); X->gpr[g] = 0.7071f * (1.0f + pn * 0.8f);
                                break;
                            }
                        }
                        float gl = 0.0f, gr = 0.0f;
                        for (int g = 0; g < 4; g++) {
                            if (X->gage[g] >= X->glen[g]) continue;
                            const float ph = (float)X->gage[g] / (float)X->glen[g], win = sinf(3.14159265f * ph);
                            const float win2 = win * win;
                            const int i0 = (int)X->gpos[g] & (GB_SIZE - 1), i1 = (i0 + 1) & (GB_SIZE - 1);
                            const float fr = X->gpos[g] - floorf(X->gpos[g]);
                            const float sm = (X->gb[0][i0] * (1.0f - fr) + X->gb[0][i1] * fr + X->gb[1][i0] * (1.0f - fr) + X->gb[1][i1] * fr) * 0.5f;
                            gl += sm * win2 * X->gpl[g]; gr += sm * win2 * X->gpr[g];
                            X->gpos[g] += X->ginc[g]; if (X->gpos[g] >= (float)GB_SIZE) X->gpos[g] -= (float)GB_SIZE;
                            X->gage[g]++;
                        }
                        const float wl = gl * 0.75f, wr = gr * 0.75f;
                        out2[0] = in2[0] * (1.0f - am * 0.8f) + wl * am * 1.6f;
                        out2[1] = in2[1] * (1.0f - am * 0.8f) + wr * am * 1.6f;
                    } else if (xt == 12) {                                      /* DELAY: ping-pong echo in time with the tempo. Amount = level, Rate = time (1/16 .. 1/2), Depth = feedback */
                        float (*B)[FXM_N] = s->xmem[X->fi - 1];
                        static const float DM[6] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
                        int di = (int)(pa * 5.99f); if (di < 0) di = 0; if (di > 5) di = 5;
                        float dsm = s->sr * 60.0f / (s->bpm > 20.0f ? s->bpm : 120.0f) * DM[di]; if (dsm > (float)(FXM_N - 4)) dsm = (float)(FXM_N - 4);
                        if (X->xdl < 1.0f) X->xdl = dsm; else X->xdl += (dsm - X->xdl) * 0.0006f;
                        const int w = X->fxw; float pos = (float)w - X->xdl; if (pos < 0.0f) pos += (float)FXM_N;
                        const int i0 = (int)pos; const float fr = pos - (float)i0; const int i1 = i0 + 1 >= FXM_N ? 0 : i0 + 1;
                        const float tl = B[0][i0] * (1.0f - fr) + B[0][i1] * fr, tr = B[1][i0] * (1.0f - fr) + B[1][i1] * fr, fb = pb * 0.85f;
                        B[0][w] = in2[0] + tr * fb; B[1][w] = in2[1] + tl * fb;
                        X->fxw = w + 1 >= FXM_N ? 0 : w + 1;
                        out2[0] = in2[0] + tl * am * 0.9f; out2[1] = in2[1] + tr * am * 0.9f;
                    } else if (xt == 13) {                                      /* REVERB: small room / hall (4 combs + 2 all-passes per side). Amount = level, Rate = size, Depth = damping */
                        float (*B)[FXM_N] = s->xmem[X->fi - 1];
                        static const int CLN[4] = { 1116, 1188, 1277, 1356 }, COF[4] = { 0, 1300, 2600, 4000 }, ALN[2] = { 556, 441 }, AOF[2] = { 5500, 6200 };
                        const float fb = 0.72f + pa * 0.26f, dp = 0.12f + pb * 0.7f, rin = (in2[0] + in2[1]) * 0.22f;
                        for (int c = 0; c < 2; c++) {
                            float sum = 0.0f; const int sp = c ? 23 : 0;
                            for (int i = 0; i < 4; i++) {
                                const int ln = CLN[i] + sp; int ix = X->fxi[c * 6 + i];
                                const float y = B[c][COF[i] + ix]; float *ls = &X->fxf[c * 4 + i];
                                *ls = y * (1.0f - dp) + *ls * dp; if (!(fabsf(*ls) < 8.0f)) *ls = 0.0f;
                                B[c][COF[i] + ix] = rin + *ls * fb; X->fxi[c * 6 + i] = ix + 1 >= ln ? 0 : ix + 1; sum += y;
                            }
                            float xa = sum;
                            for (int a = 0; a < 2; a++) {
                                const int ln = ALN[a] + sp; int ix = X->fxi[c * 6 + 4 + a];
                                const float bq = B[c][AOF[a] + ix], y = -xa + bq; B[c][AOF[a] + ix] = xa + bq * 0.5f; X->fxi[c * 6 + 4 + a] = ix + 1 >= ln ? 0 : ix + 1; xa = y;
                            }
                            out2[c] = in2[c] * (1.0f - am * 0.4f) + xa * am * 1.5f;
                        }
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
    *lp = l; *rp = r_;
}


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
static inline int clampi_(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
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
    float semis = (q[PP_TUNE] - 0.5f) * 48.0f;
    if (is_melodic(ty)) semis = roundf(semis);
    s->semi[p] = semis;
    {   /* decay 0 = a real snap / click (about 1/150 of the natural length), 0.5 = natural length, 1 = 3x: the lowest fifth of the knob ramps down into it (the rest keeps the old curve, so existing sounds stay as they were) */
        const float dv = q[PP_DECAY];
        float f = 1.0f; if (dv < 0.2f) { const float u = dv / 0.2f; f = 0.012f + 0.988f * u * u; }
        s->dm[p] = powf(3.0f, (dv - 0.5f) * 2.0f) * f;
    }
    s->a_tone[p] = q[PP_TONE] < 0.99f ? onepole_a(150.0f * powf(110.0f, q[PP_TONE]), s->sr) : 1.0f;
    s->fdk[p] = 1.5e-4f * exp2f((0.5f - q[PP_FDEC]) * 6.0f);
    float dg = 1.0f + q[PP_DRIVE] * 10.0f;
    s->dg[p] = dg;
    s->dnorm[p] = q[PP_DRIVE] > 0.01f ? 0.5f / fast_tanh(0.5f * dg) : 1.0f;
    s->gain[p] = q[PP_VOL] * q[PP_VOL] / 0.64f * TY[ty].trim * 0.85f;      /* sounds were a little too loud: -1.4 dB */
    float a = q[PP_PAN] * 1.57079633f;
    s->pl[p] = cosf(a) * 1.41421356f;
    s->pr[p] = sinf(a) * 1.41421356f;
    {
        float dtau = 0.03f + q[PP_DUCKR] * q[PP_DUCKR] * 0.55f;
        s->dk_c[p] = expf(-1.0f / (dtau * s->sr));          /* every sound (lane) has its own duck release */
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
    (void)key; s->root_hz = 32.7032f * powf(2.0f, 5.0f / 12.0f);        /* the sounds keep their pitch: KEY only tells the MIDI effects (scale snap, diatonic chords) where the scale starts */
    {                                                          /* the shared reverb: size = decay time, dark = damping */
        const float rt60 = 0.5f * powf(12.0f, s->g[G_RVSIZE]);
        static const int BASE_H[NLINES] = { 1033, 1327, 1759, 2143 }, BASE_E[NLINES] = { 1999, 2713, 3331, 3989 };
        const int *BASE = ((int)(s->g[G_RMODE] + 0.5f) == 1) ? BASE_E : BASE_H;
        for (int i = 0; i < NLINES; i++) {
            int d = (int)((float)BASE[i] * sr / 44100.0f + 0.5f);
            if (d > RV_SIZE - 2) d = RV_SIZE - 2;
            if (d < 8) d = 8;
            s->rv_len[i] = d;
            s->rv_g[i] = powf(10.0f, -3.0f * (float)d / (sr * rt60));
        }
        s->rv_a = onepole_a(6000.0f * powf(0.05f, s->g[G_RVDARK]), sr);
        float dec = powf(10.0f, -1.05f / rt60); if (dec > 0.93f) dec = 0.93f;
        s->dt_dec = dec;
        static const int DL[26] = { 142, 107, 379, 277, 672, 4453, 1800, 3720, 908, 4217, 2656, 3163, 266, 2974, 1913, 1996, 1990, 187, 1066, 353, 3627, 1228, 2673, 2111, 335, 121 };
        const float k = sr / 29761.0f;
        for (int i = 0; i < 26; i++) { int d = (int)((float)DL[i] * k + 0.5f); if (d > DT_SIZE - 40) d = DT_SIZE - 40; s->dt_len[i] = d; }
    }
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
    { T_KICK,0.5f,0.5f,0,1,0,0 },        { T_RUMBKICK,0.5f,0.5f,0,1,0,0 }, { T_CLAP,0.5f,0.5f,0,1,0,0 }, { T_SNARE,0.5f,0.5f,0,1,0,0 },
    { T_HATCL,0.5f,0.5f,0,1,0,0 },       { T_HATOP,0.5f,0.5f,0,1,0,0 },       { T_RIDE,0.5f,0.5f,0,1,0,0 },        { T_RIM,0.5f,0.5f,0,1,0,0 },
    { T_TOM,0.5f,0.5f,0,1,0,0 },         { T_PERC,0.5f,0.5f,0,1,0,0 },    { T_SUBBASS,0.5f,0.5f,0,1,0,0 },     { T_ROLLBASS,0.5f,0.5f,0,1,0,0 },
    { T_ACID,0.5f,0.5f,0,1,0,0 },     { T_PLUCK,0.5f,0.5f,0,1,0,0 },{ T_STAB,0.5f,0.5f,0,1,0,0 }, { T_PAD,0.5f,0.5f,0,1,0,0 },
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
/* KEY and SCALE belong to the track (0 = follow the master Key / Scale) */
static inline int eff_key(const inst_t *s, int p) {
    const int t = (p >= 0 && p < NLANE) ? lane_trk(s, p) : 0, v = (int)(s->pp[t][PP_TKEY] + 0.5f);
    return v > 0 && v <= 12 ? v - 1 : (int)(s->g[G_KEY] + 0.5f);
}
static inline int eff_scale(const inst_t *s, int p) {
    const int t = (p >= 0 && p < NLANE) ? lane_trk(s, p) : 0, v = (int)(s->pp[t][PP_TSCL] + 0.5f);
    return (v > 0 && v <= NSC ? v - 1 : (int)(s->g[G_SCALE] + 0.5f)) % NSC;
}
static inline int lane_on(const inst_t *s, int l) {
    if (l < NTRK) return !s->isdrum[l];
    return s->dtrk[(l - NTRK) >> 4] >= 0;
}
static int track_slot(const inst_t *s, int t) { for (int d = 0; d < 4; d++) if (s->dtrk[d] == t) return d; return -1; }

/* ---------------- voices ---------------- */

/* ---------------- ACOUSTIC drums + EFFECT sweeps (engines 13 and 14) ---------------- */
#define AC_SETS 9
static const float AC_R[AC_SETS][8] = {
    { 1.0f, 1.593f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.500f },     /* 0 drum head (circular membrane) */
    { 1.0f, 1.483f, 1.932f, 2.546f, 2.917f, 3.520f, 4.160f, 5.430f },     /* 1 cymbal */
    { 1.0f, 1.342f, 1.784f, 2.236f, 2.763f, 3.410f, 4.120f, 4.940f },     /* 2 hi-hat */
    { 1.0f, 2.310f, 4.050f, 0, 0, 0, 0, 0 },                              /* 3 wood block */
    { 1.0f, 1.504f, 2.956f, 4.120f, 0, 0, 0, 0 },                         /* 4 cowbell */
    { 1.0f, 2.760f, 5.404f, 8.933f, 13.34f, 0, 0, 0 },                    /* 5 triangle */
    { 1.0f, 2.150f, 3.870f, 0, 0, 0, 0, 0 },                              /* 6 rim / stick */
    { 1.0f, 1.210f, 1.470f, 1.790f, 2.130f, 2.570f, 0, 0 },               /* 7 tambourine jingles */
    { 1.0f, 1.590f, 2.140f, 0, 0, 0, 0, 0 },                              /* 8 bass drum shell */
};
static const float AC_A[AC_SETS][8] = {
    { 1.0f, 0.62f, 0.45f, 0.38f, 0.30f, 0.22f, 0.16f, 0.12f },
    { 0.8f, 0.7f, 0.8f, 0.65f, 0.6f, 0.5f, 0.45f, 0.4f },
    { 1.0f, 0.8f, 0.9f, 0.7f, 0.6f, 0.55f, 0.45f, 0.4f },
    { 1.0f, 0.5f, 0.25f, 0, 0, 0, 0, 0 },
    { 1.0f, 0.8f, 0.5f, 0.3f, 0, 0, 0, 0 },
    { 1.0f, 0.6f, 0.45f, 0.3f, 0.2f, 0, 0, 0 },
    { 1.0f, 0.6f, 0.3f, 0, 0, 0, 0, 0 },
    { 1.0f, 0.9f, 0.8f, 0.8f, 0.7f, 0.6f, 0, 0 },
    { 0.0f, 0.35f, 0.2f, 0, 0, 0, 0, 0 },
};
/* ring modes: nx[k] / nx[8+k] = state, nx[16+k] = a1, nx[24+k] = a2, nx[32+k] = gain; ni[0] = number of modes (0 = none) */
static void ac_modes(inst_t *s, vc_t *v, int set, float f0, float tau, float tilt, float stretch, int skip0, float ascale) {
    int nm = 0;
    for (int k = 0; k < 8; k++) {
        v->nx[k] = v->nx[8 + k] = v->nx[16 + k] = v->nx[24 + k] = v->nx[32 + k] = 0.0f;
        const float rt = AC_R[set][k], am = AC_A[set][k] * ascale;
        if (rt <= 0.0f || am <= 0.0f || (skip0 && k == 0)) continue;
        const float fk = f0 * rt * (1.0f + stretch * (rt - 1.0f));
        if (fk > 0.45f * s->sr || fk < 20.0f) continue;
        float tk = tau * (rt > 1.0f ? powf(rt, -tilt) : 1.0f); if (tk < 0.0015f) tk = 0.0015f;
        const float r = expf(-1.0f / (tk * s->sr)), w = TWO_PI * fk / s->sr;
        v->nx[16 + k] = 2.0f * r * cosf(w); v->nx[24 + k] = -r * r; v->nx[32 + k] = am * sinf(w); nm++;
    }
    v->ni[0] = nm;
}
#include "speech_lts.inc"
#include "speech.inc"
static int ac_hat(int ty) { const nx_t *n = &VAR4[ty - T_V4]; return (n->eng == 13 && (int)(n->p[0] + 0.5f) == 3) ? (int)(n->p[9] + 0.5f) : 0; }

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
    case 5:                                                         /* DRUM SYNTH */
        for (int k = 0; k < 24; k++) v->nx[k] = 0.0f;
        break;
    case 11: {                                                      /* STRING (Karplus-Strong): fill the delay line with a plucked burst. p: brightness, pick position, loop gain, excitation (0 noise .. 1 saw) */
        float N = s->sr / (f0 > 44.0f ? f0 : 44.0f) - 0.5f; if (N > 1020.0f) N = 1020.0f; if (N < 6.0f) N = 6.0f;
        const int n = (int)N; const float fr = N - (float)n;
        v->ksn = n; v->ksi = 0; v->ksz1 = 0.0f; v->ksz2 = 0.0f; v->nx[0] = (1.0f - fr) / (1.0f + fr);   /* all-pass tuning */
        const float ex = clampf(q[3] + (s->pp[p][PP_FDEC] - 0.5f), 0.0f, 1.0f), pk = clampf(q[1] + s->pp[p][PP_SUB], 0.02f, 0.5f);
        float lp = 0.0f; const float lpa = 0.25f + 0.75f * clampf(0.35f + v->vel * 0.65f, 0.0f, 1.0f);
        for (int i = 0; i < n; i++) { const float nz = noise(s), sw = 2.0f * (float)i / (float)n - 1.0f; const float e = nz * (1.0f - ex) + sw * ex; lp += lpa * (e - lp); v->ks[i] = lp; }
        const int k = (int)(pk * (float)n); if (k > 0) { float tmp[1024]; for (int i = 0; i < n; i++) tmp[i] = v->ks[i] - v->ks[(i + n - k) % n]; for (int i = 0; i < n; i++) v->ks[i] = tmp[i] * 0.7f; }
        break; }
    case 13: {                                                      /* ACOUSTIC: p = kind (0 kick, 1 snare, 2 tom, 3 hat, 4 cymbal, 5 rim, 6 clap, 7 shaker / tambourine, 8 block / bell),
                                                                       tone, drop, noise, click, body, noise Hz, extra, set / jingles, hat flag */
        const int kd = (int)(q[0] + 0.5f); const float tau = n->tau1 * dm;
        for (int k = 0; k < 64; k++) v->nx[k] = 0.0f;
        int set = -1, skip0 = 0; float tilt = 0.4f + q[5] * 1.2f, asc = 1.0f, burst = 2.5f;
        switch (kd) {
        case 0: set = 8; skip0 = 1; burst = 1.0f; break;
        case 1: set = 0; burst = 2.5f; tilt = 0.5f + q[5]; break;
        case 2: set = 0; skip0 = 1; burst = 3.0f; break;
        case 3: set = 2; tilt = 0.2f + q[5] * 0.5f; burst = 1.0f; break;
        case 4: set = 1; tilt = 0.15f + q[5] * 0.4f; burst = clampf(q[7], 1.0f, 400.0f); break;
        case 5: set = 6; burst = 0.6f; tilt = 0.3f + q[5]; break;
        case 7: set = q[8] > 0.5f ? 7 : -1; burst = 4.0f; tilt = 0.2f; break;
        case 8: set = 3 + clampi_((int)(q[8] + 0.5f), 0, 2); burst = 0.5f; tilt = 0.2f + q[5] * 0.6f; break;
        default: break;
        }
        if (set >= 0) ac_modes(s, v, set, f0, tau, tilt, 0.0f, skip0, asc); else v->ni[0] = 0;
        const float b = clampf(q[1] * (0.45f + 0.9f * v->vel), 0.02f, 1.0f);
        v->nx[43] = 0.03f + 0.97f * b * b * b;
        v->nx[42] = expf(-1.0f / (burst * 0.001f * sr));
        v->nx[41] = 1.0f; v->nx[44] = 1.0f; v->nx[46] = kd == 4 ? q[4] : (kd == 3 ? 0.35f * q[4] : 0.0f);
        v->nx[48] = onepole_a(clampf(q[6], 80.0f, 16000.0f), sr);       /* noise: high-pass corner */
        v->nx[49] = onepole_a(kd == 6 ? clampf(q[6] * 3.0f, 800.0f, 9000.0f) : (kd == 0 ? 320.0f : 11000.0f), sr);    /* noise: low-pass corner */
        v->ph = 0.0f; break; }
    case 15: spk_start(s, v, p, n); break;                         /* SPEECH */
    case 14: {                                                      /* EFFECT: p = length s, tone Hz start, tone Hz end, filter Hz start, filter Hz end, resonance, noise, tone level, peak position, filter type (0 LP, 1 BP, 2 HP) */
        for (int k = 0; k < 64; k++) v->nx[k] = 0.0f;
        const float L = clampf(q[0] * dm, 0.03f, 20.0f);
        v->nx[0] = L; v->nx[7] = clampf(q[1], 10.0f, 18000.0f); v->nx[8] = powf(clampf(q[2], 10.0f, 18000.0f) / v->nx[7], 1.0f / (L * sr));
        v->nx[9] = clampf(q[3], 20.0f, 18000.0f); v->nx[10] = powf(clampf(q[4], 20.0f, 18000.0f) / v->nx[9], 1.0f / (L * sr));
        v->ph = 0.0f; break; }
    case 12:                                                        /* FOLD */
        for (int k = 0; k < 24; k++) v->nx[k] = 0.0f; break;
    case 6: case 7: case 8: case 9: case 10:                        /* ANALOG / WAVETABLE / FM4 / ADDITIVE / PAD: phases start spread out so unison layers do not add up to a click */
        for (int k = 0; k < 64; k++) v->nx[k] = 0.0f;                  /* nx[24..] hold values that are only recomputed every 16 samples (nx[63] counts down) */
        for (int k = 0; k < 8; k++) v->nx[k] = (float)((k * 37) % 16) / 16.0f;
        if (n->eng == 10) for (int k = 8; k < 12; k++) v->nx[k] = (float)((k * 37) % 16) / 16.0f;
        break;
    default:                                                        /* MORPH */
        v->nx[0] = 0.0f; v->nx[1] = 0.37f; v->nx[2] = 0.71f; v->nx[3] = 0.0f; v->nx[4] = 0.0f; break;
    }
}
static float g_fxk = 1.0f;                      /* level share of each note while a MIDI effect plays several notes at once */
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
        s->vc[NLANE + gi] = *v; s->vown[NLANE + gi] = p; s->act_dirty = 1;
        if (!v->gk) s->vc[NLANE + gi].gate = 0;
    }
    if (p < NLANE) {                                          /* SIDECHAIN: every hit of pad 1 (bottom left) of the FIRST drum track ducks every sound that has Duck > 0 (Duck Src: PAD 1 / ALL = every hit of this track / OFF) */
        const int kt = lane_trk(s, p);
        const int scs = (int)(s->pp[p][PP_SCS] * 2.0f + 0.5f), scs_t = (int)(s->pp[kt][PP_SCS] * 2.0f + 0.5f);
        int bestT = 99, bestSl = -1; for (int sl = 0; sl < 4; sl++) { const int t = s->dtrk[sl]; if (t >= 0 && t < bestT) { bestT = t; bestSl = sl; } }
        const int first = (bestSl >= 0 && p == NTRK + bestSl * 16);
        if ((first && scs != 2 && scs_t != 2) || (!first && (scs == 1 || (scs_t == 1 && p < NTRK))))
        for (int t = 0; t < NLANE; t++) if (t != p && s->pp[t][PP_DUCK] > 0.01f) s->dk_env[t] = 1.0f;
    }
    if (s->pp[p][PP_POLY] <= 0.5f && v->active && (TY[v->type].sus || BASE_T[v->type] == T_E_SPK) && v->env > 0.003f) {   /* mono re-trigger of a held synth note: the old note fades out in a ghost instead of being cut (click) */
        int gi = -1;
        for (int i = 0; i < NGH; i++) if (!s->vc[NLANE + i].active) { gi = i; break; }
        if (gi < 0) { gi = s->gnext; s->gnext = (s->gnext + 1) % NGH; }
        s->vc[NLANE + gi] = *v; s->vown[NLANE + gi] = p; s->act_dirty = 1;
        s->vc[NLANE + gi].gate = 0; s->vc[NLANE + gi].fol = 0; s->vc[NLANE + gi].xfd = 1.0f;
    }
    memset(v, 0, sizeof(*v)); v->fenv = 1.0f;
    v->fkoff = s->pp[p][PP_FKEY] * 0.16f * log2f(fm > 0.05f ? fm : 0.05f);
    v->active = 1; v->gate = 1; v->type = ty; s->act_dirty = 1;
    v->aenv = 1.0f; v->ac = 0.0f; v->hd = 1.0f;
    if (p < NTRK && !TY[ty].sus && BASE_T[ty] != T_SAMPLE) {       /* Attack on every struck / plucked synth sound (FM, bells, ...): the same time as on the sustained ones */
        const float ak = s->pp[p][PP_ATT];
        if (ak > 0.004f) { v->aenv = 0.0f; v->ac = s->inv_sr / att_unified(ak); }
    }
    v->vel = (0.25f + 0.75f * vel) * g_fxk;       /* g_fxk < 1 while a MIDI effect stacks several notes: keeps the sum from overloading the output */
    { int w0 = (int)(s->pp[p][PP_WAVE] + 0.5f); v->sw = (w0 >= 1 && w0 <= 15) ? w0 : CHR[ty].wv; }   /* 0 = the sound's own oscillator */
    v->fm = fm; v->f0 = base * fm;
    if (BASE_T[ty] >= T_S0 && BASE_T[ty] < T_S0 + 8) {
        const sy_t *y = &SYT[ty];
        int w = (int)(s->pp[p][PP_WAVE] + 0.5f);
        v->sw = (w >= 1 && w <= 15) ? w : (CHR[ty].wv ? CHR[ty].wv : y->wave);
        v->sdet = y->det + s->pp[p][PP_DET] * 0.04f;
        v->ssub = clampf(y->sub + s->pp[p][PP_SUB], 0.0f, 1.0f);
        v->satt = att_time(p, s->pp[p][PP_ATT], T->att, 0.0f, 1.5f);
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
    v->third = powf(2.0f, (float)SCALE_TBL[eff_scale(s, p)][1] / 12.0f);
    if (BASE_T[ty] >= T_E_MODAL) nx_init(s, v, p, ty);
    if (BASE_T[ty] == T_HATCL || (BASE_T[ty] == T_E_AC && ac_hat(ty) == 1)) {                         /* choke every open hat */
        for (int i = 0; i < NLANE; i++)
            if (s->vc[i].active && lane_trk(s, i) == lane_trk(s, p) && (BASE_T[s->vc[i].type] == T_HATOP || (BASE_T[s->vc[i].type] == T_E_AC && ac_hat(s->vc[i].type) == 2))) s->vc[i].active = 0;
    }
}

/* ---------------- MIDI effects (per track, 10 types) ---------------- */
static const int MSN[NSC] = { 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 6, 5, 5, 5, 5, 6, 8, 8, 8, 7, 7, 7, 7 };      /* notes per scale (5 .. 8) */
static const int MS7[NSC][8] = { { 0, 2, 3, 5, 7, 8, 10, 0 }, { 0, 1, 3, 5, 7, 8, 10, 0 }, { 0, 2, 4, 5, 7, 9, 11, 0 }, { 0, 2, 3, 5, 7, 9, 10, 0 }, { 0, 2, 4, 6, 7, 9, 11, 0 }, { 0, 2, 4, 5, 7, 9, 10, 0 }, { 0, 1, 3, 5, 6, 8, 10, 0 }, { 0, 2, 3, 5, 7, 8, 11, 0 }, { 0, 2, 3, 5, 7, 9, 11, 0 }, { 0, 1, 4, 5, 7, 8, 10, 0 }, { 0, 1, 4, 5, 7, 8, 11, 0 }, { 0, 2, 3, 6, 7, 8, 11, 0 }, { 0, 2, 4, 7, 9, 0, 0, 0 }, { 0, 3, 5, 7, 10, 0, 0, 0 }, { 0, 3, 5, 6, 7, 10, 0, 0 }, { 0, 2, 3, 4, 7, 9, 0, 0 }, { 0, 2, 3, 7, 8, 0, 0, 0 }, { 0, 1, 5, 7, 10, 0, 0, 0 }, { 0, 1, 5, 6, 10, 0, 0, 0 }, { 0, 2, 5, 7, 10, 0, 0, 0 }, { 0, 2, 4, 6, 8, 10, 0, 0 }, { 0, 2, 3, 5, 6, 8, 9, 11 }, { 0, 1, 3, 4, 6, 7, 9, 10 }, { 0, 2, 4, 5, 7, 9, 10, 11 }, { 0, 1, 3, 5, 7, 8, 11, 0 }, { 0, 2, 4, 6, 7, 9, 10, 0 }, { 0, 1, 3, 4, 6, 8, 10, 0 }, { 0, 1, 4, 5, 6, 8, 11, 0 } };
static void ghost_put(inst_t *s, int p, const vc_t *lay) {
    int gi = -1;
    for (int i = 0; i < NGH; i++) if (!s->vc[NLANE + i].active) { gi = i; break; }
    if (gi < 0) { gi = s->gnext; s->gnext = (s->gnext + 1) % NGH; }
    s->vc[NLANE + gi] = *lay; s->vown[NLANE + gi] = p; s->act_dirty = 1;
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
#define EMIT(v, sm, fr) do { if (slot == 1) trig_fx(s, p, (v), (sm), 2); else trig_raw(s, p, (v), (sm), (fr)); } while (0)
static void trig_fx(inst_t *s, int p, float vel, int arp_semi, int slot);
static void trig_fx(inst_t *s, int p, float vel, int arp_semi, int slot) {      /* one MIDI effect slot: slot 1 runs first and hands its notes on to slot 2 */
    const int mf = (int)(s->pp[p][slot == 2 ? PP_M2FX : PP_MFX] + 0.5f);
    if (mf <= 0 || mf > 11) { if (slot == 1) trig_fx(s, p, vel, arp_semi, 2); else trig_raw(s, p, vel, arp_semi, 1); return; }
    const float am = s->pp[p][slot == 2 ? PP_M2AMT : PP_MAMT], pr = s->pp[p][slot == 2 ? PP_M2PRM : PP_MPRM];
    const int sc = eff_scale(s, p);
    const float lv = vel * (0.35f + 0.65f * am);
    const float L16 = s->sr * 60.0f / s->bpm / 4.0f;
    static const int CHD[8][4] = { {0,4,7,-99}, {0,3,7,-99}, {0,2,7,-99}, {0,5,7,-99}, {0,4,7,10}, {0,3,7,10}, {0,4,7,11}, {0,3,6,9} };
    const float k0 = g_fxk;
    switch (mf) {
    case 1: {                                                    /* OCTAVE: Param = which layers (-1 / +1 / both / +1 +2) */
        g_fxk = k0 * (pr < 0.5f ? 0.70f : 0.55f);
        EMIT(vel, arp_semi, 1);
        if (pr < 0.25f) trig_layer(s, p, lv, arp_semi - 12, 1);
        else if (pr < 0.5f) trig_layer(s, p, lv, arp_semi + 12, 1);
        else if (pr < 0.75f) { trig_layer(s, p, lv, arp_semi + 12, 1); trig_layer(s, p, lv, arp_semi - 12, 1); }
        else { trig_layer(s, p, lv, arp_semi + 12, 1); trig_layer(s, p, lv, arp_semi + 24, 1); }
        break; }
    case 2: {                                                    /* CHORD: Param = chord type (maj min sus2 sus4 7 m7 maj7 dim) */
        int ci = (int)(pr * 7.99f); if (ci < 0) ci = 0; if (ci > 7) ci = 7;
        g_fxk = k0 * (CHD[ci][3] > -90 ? 0.42f : 0.50f);
        EMIT(vel, arp_semi, 1);
        for (int k = 1; k < 4; k++) if (CHD[ci][k] > -90) trig_layer(s, p, lv, arp_semi + CHD[ci][k], 1);
        break; }
    case 3: {                                                    /* ARP: root, then the notes of the scale chord one by one (Param = rate 1/16, 1/8, 1/32) */
        const float st = pr < 0.33f ? 1.0f : (pr < 0.66f ? 2.0f : 0.5f);
        const int d = (int)(st * L16);
        EMIT(vel, arp_semi, 1);
        eq_push(s, p, lv, arp_semi + MS7[sc][2], d, 0);
        eq_push(s, p, lv, arp_semi + 7, 2 * d, 0);
        eq_push(s, p, lv, arp_semi + 12, 3 * d, 0);
        if (am > 0.5f) { eq_push(s, p, lv * 0.9f, arp_semi + MS7[sc][2] + 12, 4 * d, 0); eq_push(s, p, lv * 0.8f, arp_semi + 7 + 12, 5 * d, 0); }
        break; }
    case 4: {                                                    /* STRUM: root, third, fifth one after another (Param = speed) */
        const int d = (int)((0.006f + pr * 0.06f) * s->sr);
        EMIT(vel, arp_semi, 1);
        eq_push(s, p, lv, arp_semi + MS7[sc][2], d, 1);
        eq_push(s, p, lv, arp_semi + 7, 2 * d, 1);
        break; }
    case 5: {                                                    /* ECHO: Amount = feedback, Param = spacing */
        const int d = (int)((0.5f + pr * 3.5f) * L16); const float fb = 0.35f + 0.5f * am;
        EMIT(vel, arp_semi, 1);
        eq_push(s, p, vel * fb, arp_semi, d, 0);
        eq_push(s, p, vel * fb * fb, arp_semi, 2 * d, 0);
        if (am > 0.4f) eq_push(s, p, vel * fb * fb * fb, arp_semi, 3 * d, 0);
        break; }
    case 6: {                                                    /* RANDOM PITCH inside the scale, range grows with Amount */
        s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
        if (pr < 0.5f && (float)((s->rng >> 8) & 0xFFFF) / 65536.0f >= pr * 2.0f) { EMIT(vel, arp_semi, 1); break; }          /* Param = chance that the pitch is changed */
        int r = (int)((s->rng >> 4) % (unsigned)(2 + (int)(am * 12.0f)));
        int deg = MS7[sc][r % MSN[sc]] + 12 * (r / MSN[sc]);
        if (s->rng & 0x8000u) deg = -deg;
        EMIT(vel, arp_semi + deg, 1); break; }
    case 7: {                                                    /* RANDOM VELOCITY */
        s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
        float rr = (float)(s->rng & 0xFFFF) / 65536.0f;
        if (pr < 0.5f && (float)((s->rng >> 16) & 0xFFFF) / 65536.0f >= pr * 2.0f) rr = 0.0f;                                  /* Param = chance that the velocity is changed */
        EMIT(clampf(vel * (1.0f - am * 0.85f * rr), 0.05f, 1.0f), arp_semi, 1); break; }
    case 8: EMIT(clampf(vel + ((0.2f + 1.3f * pr > 1.0f ? 1.0f : 0.2f + 1.3f * pr) - vel) * am, 0.05f, 1.0f), arp_semi, 1); break;   /* Param = target velocity (default 0.85) */   /* VELOCITY COMPRESSOR */
    case 9: {                                                    /* SCALE: snap every note to the current key / scale */
        const int ks = 5 - eff_key(s, p), as = arp_semi + ks;      /* the scale starts at the KEY (the sounds' own root is F) */
        int o = as >= 0 ? as / 12 : -((-as + 11) / 12), r = as - o * 12, best = 0, bd = 99;
        for (int k = 0; k <= MSN[sc]; k++) { int dv = k < MSN[sc] ? MS7[sc][k] : 12, dd = dv > r ? dv - r : r - dv; if (dd < bd) { bd = dd; best = dv; } }
        EMIT(vel * (0.35f + 0.65f * am) / 0.805f > 1.0f ? 1.0f : vel * (0.35f + 0.65f * am) / 0.805f, o * 12 + best - ks + 12 * (int)((pr - 0.5f) * 4.0f + (pr >= 0.5f ? 0.5f : -0.5f)), 1); break; }      /* Amount = level, Param = octave shift */
    case 11: {                                                   /* DIATONIC CHORD: the chord that belongs to this scale degree (Amount = level, Param = triad / 7th / 9th / open) */
        const int ks = 5 - eff_key(s, p), as = arp_semi + ks;
        int o = as >= 0 ? as / 12 : -((-as + 11) / 12), r = as - o * 12, deg = 0, bd = 99;
        for (int k = 0; k < MSN[sc]; k++) { int dd = MS7[sc][k] > r ? MS7[sc][k] - r : r - MS7[sc][k]; if (dd < bd) { bd = dd; deg = k; } }
        const int base = o * 12 - ks, n = pr < 0.25f ? 3 : (pr < 0.5f ? 4 : (pr < 0.75f ? 5 : 3));
        const int step[5] = { 0, 2, 4, 6, 8 };
        g_fxk = k0 * (n >= 5 ? 0.38f : (n == 4 ? 0.42f : 0.50f));
        EMIT(vel, base + MS7[sc][deg], 1);
        for (int k = 1; k < n; k++) {
            const int g = deg + step[k]; int semi = base + MS7[sc][g % MSN[sc]] + 12 * (g / MSN[sc]);
            if (pr >= 0.75f && k == 1) semi += 12;                 /* open voicing: the third an octave up */
            trig_layer(s, p, lv, semi, 1);
        }
        break; }
    default:                                                     /* TRANSPOSE: Param = -24 .. +24 semitones */
        EMIT(vel, arp_semi + (int)((pr - 0.5f) * 48.0f + (pr >= 0.5f ? 0.5f : -0.5f)), 1); break;
    }
    g_fxk = k0;
}

static void trig(inst_t *s, int p, float vel, int arp_semi, int fresh) {
    {   /* note stage before the MIDI effects: chance, transpose, random octave, velocity bias / humanise / range */
        const float *q = s->pp[p];
        if (fresh && q[PP_MCH] < 0.995f) { s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5; if ((float)(s->rng & 0xFFFF) / 65536.0f >= q[PP_MCH]) return; }
        arp_semi += (int)((q[PP_TRN] - 0.5f) * 48.0f + (q[PP_TRN] >= 0.5f ? 0.5f : -0.5f));
        if (q[PP_ORN] > 0.01f) { s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5; if ((float)(s->rng & 0xFFFF) / 65536.0f < q[PP_ORN]) arp_semi += (s->rng & 0x10000u) ? 12 : -12; }
        vel += (q[PP_MVB] - 0.5f) * 0.8f;
        if (q[PP_VHM] > 0.01f) { s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5; vel *= 1.0f - q[PP_VHM] * 0.6f * ((float)(s->rng & 0xFFFF) / 65536.0f); }
        { float lo = 0.05f + q[PP_VMN] * 0.95f, hi = q[PP_VMX] < lo ? lo : q[PP_VMX]; if (hi < 0.05f) hi = 0.05f; if (vel < lo) vel = lo; if (vel > hi) vel = hi; if (vel > 1.0f) vel = 1.0f; if (vel < 0.02f) vel = 0.02f; }
    }
    if (!fresh) { trig_raw(s, p, vel, arp_semi, fresh); return; }
    trig_fx(s, p, vel, arp_semi, 1);
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
    case 4:  return sin2pi(ph);
    case 5:  return ph < 0.25f ? 1.0f : -1.0f;
    case 6:  return ph < 0.125f ? 1.0f : -1.0f;
    case 7:  return 1.0f - 2.0f * ph;
    case 8:  return 2.0f * fabsf(sin2pi(ph)) - 1.0f;
    case 9:  return (sin2pi(ph) + 0.6f * sinf(TWO_PI * 2.0f * ph) + 0.35f * sinf(TWO_PI * 4.0f * ph)) * 0.6f;
    case 10: return (sin2pi(ph) + 0.33f * sinf(TWO_PI * 3.0f * ph) + 0.2f * sinf(TWO_PI * 5.0f * ph)) * 0.75f;
    case 11: { float x = 1.9f * sin2pi(ph); if (x > 1.0f) x = 2.0f - x; if (x < -1.0f) x = -2.0f - x; return x; }
    case 12: return floorf(ph * 5.0f) * 0.5f - 1.0f;
    case 13: return floorf((4.0f * fabsf(ph - 0.5f) - 1.0f) * 3.5f + 0.5f) * (1.0f / 3.5f);
    case 14: return (sin2pi(ph) * 0.5f + sinf(TWO_PI * 3.0f * ph + 1.0f) * 0.5f + sinf(TWO_PI * 5.0f * ph + 2.0f) * 0.4f) * 0.8f;
    case 15: { float q = ph * 2.5f; q -= floorf(q); return 2.0f * q - 1.0f; }
    default: return 2.0f * ph - 1.0f;
    }
}
#define WRAP(x) do { if ((x) >= 1.0f) (x) -= 1.0f; } while (0)

static inline float bsin(const vc_t *v) { return v->sw >= 100 ? wt_at(v->ph, v->sw - 100) : sin2pi(v->ph); }      /* body oscillator: sine, or the preset's own wavetable */


/* polyBLEP: removes the aliasing step of saw / square edges */
static inline float blep(float t, float d) {
    if (t < d) { t /= d; return t + t - t * t - 1.0f; }
    if (t > 1.0f - d) { t = (t - 1.0f) / d; return t * t + t + t + 1.0f; }
    return 0.0f;
}
/* ---------------- NEW ENGINES: one sample ---------------- */
/* cheap sine for the oscillator banks: 2048-entry table with linear interpolation (error below -90 dB); the table is filled once at load */
static inline float fsin01(float ph) { const float x = ph * 2048.0f; const int i = (int)x; const float fr = x - (float)i; const float a = g_sintab[i & 2047]; return a + (g_sintab[(i & 2047) + 1] - a) * fr; }
static float nx_render(inst_t *s, vc_t *v, int p) {
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
        if (q[6] > 0.0f) { v->nx[45] += q[7] * dt; if (v->nx[45] >= 1.0f) v->nx[45] -= 1.0f; x *= 1.0f - q[6] * 0.5f * (1.0f - sin2pi(v->nx[45])); }
        v->env *= v->c1; if (v->env < 1e-3f) v->active = 0;
        break; }
    case 1: {                                                       /* FM: carrier, two modulators, feedback, three algorithms */
        const float f = v->f0, algo = q[8], fl = q[9];
        const float ie = fl + (1.0f - fl) * v->env2, vs = 1.0f + q[7] * (2.0f * v->vel - 1.0f) * 0.6f;
        v->ph += f * q[5] * dt; WRAP(v->ph);
        v->ph2 += f * q[0] * dt; WRAP(v->ph2);
        v->ph3 += f * q[2] * dt; WRAP(v->ph3);
        float out;
        if (algo > 2.5f) {                                          /* INIT: plain 2-OP FM, ratio / index / feedback live on the pad knobs (Detune, Sub, KeyTrack) */
            const float *pq = s->pp[p];
            const float rat = 0.5f * powf(16.0f, pq[PP_DET]), idx = pq[PP_SUB] * 8.0f, fbk = pq[PP_FKEY] * 1.2f;
            v->ph2 += f * (rat - q[0]) * dt; WRAP(v->ph2); if (v->ph2 < 0.0f) v->ph2 += 1.0f;
            const float sb = sin2pi(v->ph2 + (fbk * v->nx[2]) * 0.15915494f); v->nx[2] = sb;
            out = sin2pi(v->ph + (sb * idx * ie * vs) * 0.15915494f);
        } else if (algo < 0.5f) {                                   /* C -> B -> A */
            const float mc = sin2pi(v->ph3) * q[3] * ie * vs;
            const float sb = sin2pi(v->ph2 + (mc + q[4] * v->nx[2]) * 0.15915494f); v->nx[2] = sb;
            out = sin2pi(v->ph + (sb * q[1] * ie * vs) * 0.15915494f);
        } else if (algo < 1.5f) {                                   /* B + C -> A (C follows the amplitude) */
            const float sb = sin2pi(v->ph2 + (q[4] * v->nx[2]) * 0.15915494f); v->nx[2] = sb;
            const float sc = sin2pi(v->ph3);
            out = sin2pi(v->ph + (sb * q[1] * ie * vs + sc * q[3] * (0.3f + 0.7f * v->env) * vs) * 0.15915494f);
        } else {                                                    /* two FM pairs side by side */
            const float sb = sin2pi(v->ph2 + (q[4] * v->nx[2]) * 0.15915494f); v->nx[2] = sb;
            out = 0.65f * sin2pi(v->ph + (sb * q[1] * ie * vs) * 0.15915494f) + 0.35f * sin2pi(v->ph3 + (sb * q[3] * ie * vs) * 0.15915494f);
        }
        if (q[6] > 0.0f) { v->nx[3] += f * 0.5f * dt; if (v->nx[3] >= 1.0f) v->nx[3] -= 1.0f; out += q[6] * sin2pi(v->nx[3]); }
        if (T->sus) {
            const float at = att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f);
            if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        } else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        v->env2 *= v->c2;
        x = out * v->env * 0.8f;
        break; }
    case 2: {                                                       /* VOCAL: glottal pulse through three formant filters */
        v->nx[8] += q[5] * dt; if (v->nx[8] >= 1.0f) v->nx[8] -= 1.0f;
        const float vib = 1.0f + q[4] * sin2pi(v->nx[8]) * (v->t > 0.25f ? 1.0f : v->t * 4.0f);
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
        { const float at = att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f);
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
        if (q[3] > 0.0f && v->t > q[4]) { v->nx[5] += 6.0f * dt; if (v->nx[5] >= 1.0f) v->nx[5] -= 1.0f; vm += q[3] * sin2pi(v->nx[5]); }
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
            const float at = att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f);
            if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        } else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = o * v->env * 0.6f;
        if (q[5] > 0.5f) { const float L = (float)((1 << (int)(q[5] + 0.5f)) - 1) * 0.5f; x = floorf(x * L + 0.5f) / L; }
        break; }
    case 5: {                                                       /* DRUM SYNTH: kick, snare, hat, clap, tom, rim, metal -- p: kind, sweep, sweep ms, noise mix, click, drive, noise HP Hz, band Hz */
        const float *pq5 = s->pp[p]; const float qsw = fmaxf(0.0f, q[1] + (pq5[PP_DSW] - 0.5f) * 12.0f), qst = q[2] * exp2f((pq5[PP_DST] - 0.5f) * 3.0f), qnm = clampf(q[3] + (pq5[PP_DNZ] - 0.5f), 0.0f, 1.0f), qck = clampf(q[4] + (pq5[PP_DCK] - 0.5f) * 1.4f, 0.0f, 1.0f);      /* the engine knobs: Sweep, Sweep time, Noise, Click (50 = the sound as designed) */
        const int kd = (int)(q[0] + 0.5f); const float t = v->t, f = v->f0;
        const float nz = noise(s);
        { const float xa = TWO_PI * (q[6] > 20.0f ? q[6] : 20.0f) * dt; v->nx[4] += (xa / (1.0f + xa)) * (nz - v->nx[4]); }
        const float hp = nz - v->nx[4];
        float out = 0.0f;
        const float sw = 1.0f + qsw * expf(-t / (qst * 0.001f + 1e-5f));
        switch (kd) {
        case 0: case 4: {                                           /* KICK / TOM: sine with a falling pitch + a click */
            v->ph += f * sw * dt; WRAP(v->ph);
            out = sin2pi(v->ph) * v->env + nz * expf(-t / 0.0015f) * qck * 0.6f + hp * (v->env2 * q[3] + fmaxf(0.0f, pq5[PP_DNZ] - 0.5f) * 1.6f * v->env * expf(-t / 0.03f));      /* Noise on a kick / tom adds a short breath on top of the body */
            break; }
        case 1: {                                                   /* SNARE: two tuned shells + filtered noise */
            v->ph += f * sw * dt; WRAP(v->ph); v->ph2 += f * 1.68f * sw * dt; WRAP(v->ph2);
            out = (0.62f * sin2pi(v->ph) + 0.38f * sin2pi(v->ph2)) * v->env * (1.0f - 0.5f * qnm) + hp * v->env2 * qnm * 1.6f + nz * expf(-t / 0.002f) * qck * 0.4f;
            break; }
        case 2: {                                                   /* HAT: six detuned squares (the 808 recipe) + noise, high-passed */
            static const float R[6] = { 1.0f, 1.483f, 1.8f, 2.546f, 2.63f, 3.9f };
            float sq = 0.0f;
            for (int k = 0; k < 6; k++) { v->nx[10 + k] += f * R[k] * dt; if (v->nx[10 + k] >= 1.0f) v->nx[10 + k] -= 1.0f; sq += v->nx[10 + k] < 0.5f ? 1.0f : -1.0f; }
            { const float xa = TWO_PI * 7000.0f * dt; v->nx[5] += (xa / (1.0f + xa)) * (sq - v->nx[5]); }
            out = ((sq - v->nx[5]) * 0.16f * (1.0f - qnm) + hp * qnm * 1.2f) * v->env;
            break; }
        case 3: {                                                   /* CLAP: three quick bursts, then the tail, band-passed noise */
            float be = t < 0.03f ? expf(-fmodf(t, 0.01f) / 0.0016f) : 0.0f;
            { const float xa = TWO_PI * (q[7] > 100.0f ? q[7] : 100.0f) * dt; v->nx[5] += (xa / (1.0f + xa)) * (hp - v->nx[5]); }
            out = v->nx[5] * 3.0f * (be * 0.8f + v->env2 * (t >= 0.03f ? 1.0f : 0.0f)) * (0.5f + v->env);
            break; }
        case 5: {                                                   /* RIM / CLAVE: two stiff sines + a click */
            v->ph += f * dt; WRAP(v->ph); v->ph2 += f * 2.42f * dt; WRAP(v->ph2);
            out = (0.7f * sin2pi(v->ph) + 0.4f * sin2pi(v->ph2)) * v->env + nz * expf(-t / 0.001f) * qck * 0.5f;
            break; }
        default: {                                                  /* METAL / COWBELL: two squares through a low-pass */
            v->ph += f * dt; WRAP(v->ph); v->ph2 += f * 1.4983f * dt; WRAP(v->ph2);
            const float sq = (v->ph < 0.5f ? 1.0f : -1.0f) + (v->ph2 < 0.5f ? 1.0f : -1.0f);
            { const float xa = TWO_PI * (q[7] > 200.0f ? q[7] : 200.0f) * dt; v->nx[5] += (xa / (1.0f + xa)) * (sq - v->nx[5]); }
            out = v->nx[5] * 0.5f * v->env;
            break; }
        }
        if (q[5] > 0.0f) out = fast_tanh(out * (1.0f + q[5] * 5.0f)) * 0.85f;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f && v->env2 < 1e-3f) v->active = 0;
        x = out * 0.9f;
        break; }
    case 6: {                                                       /* ANALOG: two band-limited oscillators (+ hard sync), sub, pulse width.
                                                                       p: waveA, waveB(0 saw 1 square 2 tri 3 sine), detuneB (cents), subLevel, syncRatio, pulseWidth, mixB, driveInside
                                                                       knobs on the pad: Detune (+cents), Sub, Sync, Width */
        const float *pq = s->pp[p]; const float f = v->f0;
        const float det = q[2] + pq[PP_DET] * 40.0f, sub = clampf(q[3] + pq[PP_SUB], 0.0f, 1.0f);
        const float sy = q[4] + pq[PP_FKEY] * 3.0f, pw = clampf(q[5] + (pq[PP_FDEC] - 0.5f) * 0.8f, 0.05f, 0.95f);
        const float fa = f, fb = f * exp2f(det / 1200.0f), da = fa * dt, db = fb * dt;
        v->ph += da; int wrap = 0; if (v->ph >= 1.0f) { v->ph -= 1.0f; wrap = 1; }
        v->ph2 += db * (1.0f + sy); if (v->ph2 >= 1.0f) v->ph2 -= 1.0f;
        if (wrap && sy > 0.01f) v->ph2 = 0.0f;                       /* hard sync */
        float a, b;
        #define ANA_OSC(res, ph, d, w) do { switch ((int)(w)) { \
            case 0: (res) = 2.0f * (ph) - 1.0f - blep((ph), (d)); break; \
            case 1: (res) = ((ph) < pw ? 1.0f : -1.0f) + blep((ph), (d)) - blep(fmodf((ph) + 1.0f - pw, 1.0f), (d)); break; \
            case 2: (res) = 4.0f * fabsf((ph) - 0.5f) - 1.0f; break; \
            default: (res) = sin2pi((ph)); break; } } while (0)
        ANA_OSC(a, v->ph, da, q[0]);
        ANA_OSC(b, v->ph2, db * (1.0f + sy), q[1]);
        v->nx[9] += 0.5f * da; if (v->nx[9] >= 1.0f) v->nx[9] -= 1.0f;
        float o = a + q[6] * b + sub * (v->nx[9] < 0.5f ? 0.6f : -0.6f);
        o *= 1.0f / (1.0f + q[6] + sub * 0.5f);
        if (q[7] > 0.0f) o = fast_tanh(o * (1.0f + q[7] * 4.0f)) * 0.8f;
        { const float at = att_time(p, pq[PP_ATT], T->att, 0.0008f, 2.0f);
          if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
          else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
        v->env2 *= v->c2;
        x = o * v->env * 0.85f;
        #undef ANA_OSC
        break; }
    case 7: {                                                       /* WAVETABLE: a position knob scans a run of the 640 tables (dark to bright).
                                                                       p: firstTable, span, position, sweepAmt, warp, unison cents, subLevel, - ; knobs: Position, Sweep, Warp, Detune */
        const float *pq = s->pp[p]; const float f = v->f0;
        const int t0 = (int)q[0]; const float span = q[1];
        float pos = clampf(q[2] + pq[PP_DET] + (q[3] + pq[PP_SUB]) * v->env2, 0.0f, 1.0f) * span;
        int ia = (int)pos; float fr = pos - (float)ia; int ta = t0 + ia, tb = ta + 1;
        const float warp = clampf(q[4] + pq[PP_FKEY], 0.0f, 1.0f);
        const float dn = q[5] + (pq[PP_FDEC] - 0.5f) * 30.0f;
        v->ph += f * dt; WRAP(v->ph);
        float pa = v->ph; if (warp > 0.0f) { pa = v->ph + warp * 0.5f * sin2pi(v->ph) / TWO_PI * 2.0f; pa -= floorf(pa); }
        float o = (1.0f - fr) * wt_at(pa, ta) + fr * wt_at(pa, tb);
        if (fabsf(dn) > 0.5f) {
            const float k = exp2f(dn / 1200.0f);
            v->nx[0] += f * k * dt; WRAP(v->nx[0]); v->nx[1] += f / k * dt; WRAP(v->nx[1]);
            o = o * 0.5f + 0.25f * ((1.0f - fr) * wt_at(v->nx[0], ta) + fr * wt_at(v->nx[0], tb) + (1.0f - fr) * wt_at(v->nx[1], ta) + fr * wt_at(v->nx[1], tb));
        }
        if (q[6] > 0.0f) { v->nx[2] += f * 0.5f * dt; WRAP(v->nx[2]); o += q[6] * sin2pi(v->nx[2]); }
        { const float at = att_time(p, pq[PP_ATT], T->att, 0.0008f, 2.0f);
          if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
          else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
        v->env2 *= v->c2;
        x = o * v->env * 0.8f;
        break; }
    case 8: {                                                       /* FM4: four operators, four algorithms.
                                                                       p: ratio2, ratio3, ratio4, index base, feedback, algo, decay of indices, velocity; knobs: Index, Ratio, Feedback, Algo */
        const float *pq = s->pp[p]; const float f = v->f0;
        const float rs = exp2f(pq[PP_SUB] * 2.0f);                   /* ratio knob: all modulator ratios scale up to x4 */
        const float ix = (q[3] + pq[PP_DET] * 6.0f), fbk = clampf(q[4] + pq[PP_FKEY], 0.0f, 1.2f);
        int al = (int)(q[5] + (pq[PP_FDEC] - 0.5f) * 8.0f + 0.5f); if (al < 0) al = 0; if (al > 3) al = 3;
        const float ie = 0.25f + 0.75f * v->env2, vs = 1.0f + q[7] * (2.0f * v->vel - 1.0f) * 0.6f;
        v->ph += f * dt; WRAP(v->ph);
        v->ph2 += f * q[0] * rs * dt; WRAP(v->ph2);
        v->ph3 += f * q[1] * rs * dt; WRAP(v->ph3);
        v->nx[0] += f * q[2] * rs * dt; WRAP(v->nx[0]);
        const float s4 = sin2pi(v->nx[0] + (fbk * v->nx[3]) * 0.15915494f); v->nx[3] = s4;
        float out;
        switch (al) {
        case 0: { const float s3 = sin2pi(v->ph3 + (s4 * ix * ie * vs) * 0.15915494f), s2 = sin2pi(v->ph2 + (s3 * ix * ie * vs) * 0.15915494f);
                  out = sin2pi(v->ph + (s2 * ix * ie * vs) * 0.15915494f); break; }                       /* 4 > 3 > 2 > 1 */
        case 1: { const float s3 = sin2pi(v->ph3), s2 = sin2pi(v->ph2 + ((s3 + s4) * 0.7f * ix * ie * vs) * 0.15915494f);
                  out = sin2pi(v->ph + (s2 * ix * ie * vs) * 0.15915494f); break; }                       /* (4 + 3) > 2 > 1 */
        case 2: { const float s3 = sin2pi(v->ph3 + (s4 * ix * ie * vs) * 0.15915494f), s2 = sin2pi(v->ph2);
                  out = sin2pi(v->ph + ((s2 + s3) * 0.7f * ix * ie * vs) * 0.15915494f); break; }         /* 4 > 3, (3 + 2) > 1 */
        default: { const float s3 = sin2pi(v->ph3 + (s4 * ix * ie * vs) * 0.15915494f), s2 = sin2pi(v->ph2 + (s4 * ix * ie * vs * 0.6f) * 0.15915494f);
                   out = 0.5f * sin2pi(v->ph + (s2 * ix * ie * vs) * 0.15915494f) + 0.35f * s3; break; }  /* two stacks side by side */
        }
        { const float at = att_time(p, pq[PP_ATT], T->att, 0.0008f, 2.0f);
          if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
          else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
        v->env2 *= v->c2 > 0.0f ? v->c2 : 0.0f;
        x = out * v->env * 0.8f;
        break; }
    case 9: {                                                       /* ADDITIVE: 10 partials. p: tilt, odd/even, stretch, shimmer(high partials fade faster), tremolo depth, tremolo rate, -, -;
                                                                       knobs: Tilt, Odd/Even, Stretch, Shimmer */
        const float *pq = s->pp[p]; const float f = v->f0;
        const float tilt = clampf(q[0] + pq[PP_DET] * 2.0f, 0.0f, 3.0f), oe = clampf(q[1] + pq[PP_SUB], 0.0f, 1.0f);
        const float str = q[2] + pq[PP_FKEY] * 0.02f, shim = clampf(q[3] + (pq[PP_FDEC] - 0.5f), 0.0f, 1.0f);
        float o = 0.0f;
        if (v->nx[63] <= 0.0f) {                                    /* control rate (every 16 samples): partial increments, levels and the normalisation */
            float norm = 0.0f; int kmax = 0;
            for (int k = 1; k <= 10; k++) {
                const float fk = f * (float)k * (1.0f + str * (float)(k * k - 1) * 0.05f);
                if (fk > 0.45f * s->sr) break;
                const float pk = 1.0f / powf((float)k, tilt);
                float a = pk;
                if ((k & 1) == 0) a *= 1.0f - 0.9f * oe;                  /* odd / even: 1 = only odd partials (clarinet) */
                a *= powf(v->env2 + 0.001f, shim * (float)(k - 1) * 0.35f);   /* high partials die first */
                v->nx[23 + k] = fk * dt; v->nx[33 + k] = a; norm += pk; kmax = k;
            }
            v->nx[44] = 1.0f / (0.4f + 0.6f * norm); v->nx[62] = (float)kmax; v->nx[63] = 16.0f;
        }
        v->nx[63] -= 1.0f;
        for (int k = 1; k <= (int)v->nx[62]; k++) {
            v->nx[11 + k] += v->nx[23 + k]; if (v->nx[11 + k] >= 1.0f) v->nx[11 + k] -= 1.0f;
            o += v->nx[33 + k] * fsin01(v->nx[11 + k]);
        }
        o *= v->nx[44];
        if (q[4] > 0.0f) { v->nx[1] += q[5] * dt; WRAP(v->nx[1]); o *= 1.0f - q[4] * 0.5f * (1.0f - sin2pi(v->nx[1])); }
        { const float at = att_time(p, pq[PP_ATT], T->att, 0.0008f, 2.0f);
          if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
          else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
        v->env2 *= v->c2;
        x = o * v->env * 0.75f;
        break; }
    case 10: {                                                      /* PAD / STRINGS - additive: 8 harmonics, the lowest four doubled into a slowly drifting detuned pair (that beating is the lush part).
                                                                       Harmonic levels follow a saw (1/k) with a soft roll-off at "body" x the note; "PWM" thins the even harmonics (pulse colour).
                                                                       p: spread (cents), motion rate, motion depth, pulse mix, body (roll-off in harmonics), -, width; knobs: Detune, Spread, PWM, Motion */
        const float *pq = s->pp[p]; const float f = v->f0;
        v->nx[12] += q[1] * dt; WRAP(v->nx[12]);
        if (v->nx[63] <= 0.0f) {                                    /* control rate (every 16 samples): partial increments and levels */
            const float sp = (q[0] + pq[PP_DET] * 30.0f) * (0.4f + 0.6f * clampf(q[6] + pq[PP_SUB], 0.0f, 1.0f) * 1.6f);
            const float mo = clampf(q[2] + pq[PP_FDEC] - 0.5f, 0.0f, 1.0f);
            const float pm = clampf(q[3] + pq[PP_FKEY], 0.0f, 1.0f), br = fmaxf(q[4], 0.5f), ev = 1.0f - 0.85f * pm;
            for (int k = 1; k <= 8; k++) {
                const float fk = f * (float)k, x = (float)k / br;
                float a = 0.637f * (1.0f - 0.45f * pm) / ((float)k * (1.0f + x * x));
                if ((k & 1) == 0) a *= ev;
                if (fk > 0.45f * s->sr) a = 0.0f;
                if (k <= 4) {                                       /* detuned pair */
                    const float d = sp * (0.45f + 0.12f * (float)k) * (1.0f + mo * 0.5f * sin2pi((v->nx[12] + 0.19f * (float)k))) / 1200.0f;
                    v->nx[16 + 2 * (k - 1)] = fk * exp2f(d) * dt;  v->nx[28 + 2 * (k - 1)] = a * 0.5f;
                    v->nx[17 + 2 * (k - 1)] = fk * exp2f(-d) * dt; v->nx[29 + 2 * (k - 1)] = a * 0.5f;
                } else { v->nx[19 + k] = fk * dt; v->nx[31 + k] = a; }      /* the single partials 5..8 are oscillators 8..11 */
            }
            v->nx[63] = 16.0f;
        }
        v->nx[63] -= 1.0f;
        float o = 0.0f;
        for (int i = 0; i < 12; i++) {
            v->nx[i] += v->nx[16 + i]; if (v->nx[i] >= 1.0f) v->nx[i] -= 1.0f;
            o += v->nx[28 + i] * fsin01(v->nx[i]);
        }
        { const float at = att_time(p, pq[PP_ATT], T->att, 0.004f, 3.0f);
          if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
          else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
        v->env2 *= v->c2;
        x = o * v->env * 0.85f;
        break; }
    case 11: {                                                      /* STRING: delay line + loop filter + all-pass tuning. knobs: Bright, Pick, Damp, Excite */
        const float *pq = s->pp[p]; const int n = v->ksn; if (n < 4) { v->active = 0; break; }
        const float b0 = clampf(q[0] + pq[PP_DET] * 0.5f, 0.5f, 1.0f), g = clampf(q[2] - pq[PP_FKEY] * 0.06f, 0.85f, 0.99995f);
        const float cur = v->ks[v->ksi], nxt = v->ks[v->ksi + 1 >= n ? 0 : v->ksi + 1];
        const float lpv = b0 * cur + (1.0f - b0) * nxt;
        const float a = v->nx[0], y = a * lpv + v->ksz1 - a * v->ksz2; v->ksz1 = lpv; v->ksz2 = y;
        v->ks[v->ksi] = y * g; if (++v->ksi >= n) v->ksi = 0;
        v->env *= v->c1; if (v->env < 1e-3f) v->active = 0;
        x = cur * v->env * 0.55f;
        break; }
    case 12: {                                                      /* FOLD (west coast): sine / triangle -> bias -> sine wave-folder, fold amount swept by an envelope.
                                                                       p: wave(0 sine 1 tri), fold, sweep, bias, sub, FM amount, FM ratio; knobs: Fold, Sweep, Symmetry, Harmonics */
        const float *pq = s->pp[p]; const float f = v->f0;
        const float fold = q[1] + pq[PP_DET] * 5.0f, sw = q[2] + pq[PP_SUB] * 4.0f, bias = q[3] + pq[PP_FKEY] * 0.6f;
        const float fmA = clampf(q[5] + (pq[PP_FDEC] - 0.5f), 0.0f, 2.0f);
        v->ph += f * dt; WRAP(v->ph); v->ph2 += f * q[6] * dt; WRAP(v->ph2);
        float o = q[0] > 0.5f ? 4.0f * fabsf(v->ph - 0.5f) - 1.0f : sin2pi(v->ph);
        o += fmA * 0.5f * sin2pi(v->ph2);
        const float u = (o + bias) * (1.0f + fold + sw * v->env2);
        float y = sinf(u * 1.5707963f);
        v->nx[9] += f * 0.5f * dt; WRAP(v->nx[9]);
        y += q[4] * sin2pi(v->nx[9]);
        v->nx[10] += 0.002f * (y - v->nx[10]); y -= v->nx[10];            /* DC blocker (the bias leaves an offset) */
        { const float at = att_time(p, pq[PP_ATT], T->att, 0.0008f, 2.0f);
          if (T->sus) { if (v->gate) v->env += (1.0f - v->env) * (dt / at); else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
          else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; } }
        v->env2 *= v->c2;
        x = y * v->env * 0.7f;
        break; }
    case 13: {                                                      /* ACOUSTIC drums: ring modes (drum head, shell, metal) + noise parts (snare wires, hat / cymbal sizzle, clap, shaker) */
        const int kd = (int)(q[0] + 0.5f); const float t = v->t, f = v->f0;
        const float nz = noise(s);
        float e = v->nx[44]; v->nx[44] = 0.0f;
        e += nz * v->nx[46] * v->nx[41]; v->nx[41] *= v->nx[42];
        v->nx[40] += v->nx[43] * (e - v->nx[40]); e = v->nx[40];
        float body = 0.0f;
        for (int k = 0; k < 8; k++) if (v->nx[32 + k] != 0.0f) {
            const float y = v->nx[16 + k] * v->nx[k] + v->nx[24 + k] * v->nx[8 + k] + e * v->nx[32 + k];
            v->nx[8 + k] = v->nx[k]; v->nx[k] = y; body += y;
        }
        v->nx[47] += v->nx[48] * (nz - v->nx[47]);                    /* noise high-pass, then low-pass */
        const float hpn = nz - v->nx[47];
        v->nx[50] += v->nx[49] * (hpn - v->nx[50]);
        const float bn = v->nx[50];
        float out = 0.0f;
        switch (kd) {
        case 0: {                                                   /* BASS DRUM: head thump with a short pitch drop, beater click, shell knock */
            const float sw = 1.0f + q[2] * expf(-t / 0.032f);
            v->ph += f * sw * dt; WRAP(v->ph);
            float th = sin2pi(v->ph) * v->env;
            if (q[6] > 0.0f) th = fast_tanh(th * (1.0f + q[6] * 0.01f)) / (0.5f + 0.5f * fast_tanh(1.0f + q[6] * 0.01f));
            out = th * 0.85f + hpn * expf(-t / 0.0025f) * q[4] * 0.5f + v->nx[50] * expf(-t / 0.022f) * q[3] * 1.4f + body * q[5] * 0.5f;
            break; }
        case 1: {                                                   /* SNARE: two ringing heads + wires */
            const float wr = bn * v->env2 * q[3] * (t < 0.0012f ? t / 0.0012f : 1.0f);
            out = body * 0.9f + wr * 2.2f + hpn * expf(-t / 0.0018f) * q[4] * 0.45f;
            break; }
        case 2: {                                                   /* TOM: fundamental with a falling pitch + the head modes */
            const float sw = 1.0f + q[2] * expf(-t / 0.04f);
            v->ph += f * sw * dt; WRAP(v->ph);
            out = sin2pi(v->ph) * v->env * 0.8f + body * q[5] * 0.9f + hpn * expf(-t / 0.002f) * q[4] * 0.4f;
            break; }
        case 3: {                                                   /* HI-HAT: metal rings + sizzle */
            out = body * 0.5f + bn * v->env2 * q[3] * 1.3f;
            break; }
        case 4: {                                                   /* CYMBAL: modes kept alive by a long noise burst + a washy noise bed, optional swell */
            const float sw = q[8] > 0.001f ? clampf(t / q[8], 0.0f, 1.0f) : 1.0f;
            out = body * 0.42f * (0.35f + 0.65f * sw) + bn * v->env2 * q[3] * (0.25f + 0.75f * sw) * 1.2f;
            break; }
        case 5: {                                                   /* RIM / SIDE STICK */
            out = body * 0.9f + hpn * expf(-t / 0.0015f) * q[4] * 0.6f;
            break; }
        case 6: {                                                   /* CLAP: a few hands a hair apart, then the room */
            static const float OFF[4] = { 0.0f, 0.0105f, 0.0215f, 0.0310f };
            float be = 0.0f; if (t < 0.06f) for (int k = 0; k < 4; k++) if (t >= OFF[k]) be += expf(-(t - OFF[k]) / 0.0024f);
            const float tail = t >= 0.031f ? v->env2 : 0.0f;
            out = bn * (be * 0.7f + tail * q[3] * 0.9f) * 2.2f;
            break; }
        case 7: {                                                   /* SHAKER / TAMBOURINE: noise that swells in and falls away, + jingles */
            const float at = q[7] > 0.0005f ? q[7] : 0.004f, ra = t < at ? t / at : 1.0f;
            out = bn * v->env * ra * q[3] * 1.6f + body * q[4] * 0.35f;
            break; }
        default: {                                                  /* BLOCK / COWBELL / TRIANGLE */
            out = body * 0.9f + hpn * expf(-t / 0.001f) * q[4] * 0.3f;
            break; }
        }
        v->env *= v->c1; v->env2 *= v->c2;
        if (t > 0.005f && v->env < 1e-3f && v->env2 < 1e-3f) v->active = 0;
        x = out * 0.9f;
        break; }
    case 14: {                                                      /* EFFECT: a tone and a filtered noise, both swept; the level either blooms up to a peak or hits and falls */
        const float L = v->nx[0], u = v->t / L;
        if (u > 1.3f) { v->active = 0; break; }
        v->nx[7] *= (u < 1.0f ? v->nx[8] : 1.0f); v->nx[9] *= (u < 1.0f ? v->nx[10] : 1.0f);
        v->ph += v->nx[7] * dt; WRAP(v->ph);
        const float tone = fast_tanh(sin2pi(v->ph) * 1.6f) * 0.8f;
        const float nz = noise(s);
        const float g = tanf(3.14159265f * clampf(v->nx[9], 20.0f, 0.45f * s->sr) * dt), k2 = 2.0f - 1.94f * clampf(q[5], 0.0f, 1.0f);
        const float a1 = 1.0f / (1.0f + g * (g + k2)), a2 = g * a1, a3 = g * a2;
        const float v3 = nz - v->nx[6], v1 = a1 * v->nx[5] + a2 * v3, v2 = v->nx[6] + a2 * v->nx[5] + a3 * v3;
        v->nx[5] = 2.0f * v1 - v->nx[5]; v->nx[6] = 2.0f * v2 - v->nx[6];
        const int ft = (int)(q[9] + 0.5f);
        float fl = ft == 0 ? v2 : (ft == 1 ? v1 * (0.3f + 0.7f * q[5]) : nz - k2 * v1 - v2);
        const float pk = clampf(q[8], 0.0f, 0.98f);
        float env;
        if (pk < 0.02f) env = expf(-u * 5.0f);
        else if (u < pk) { const float r = u / pk; env = r * r; }
        else env = expf(-(u - pk) * 7.0f / (1.3f - pk));
        if (u < 0.002f) env *= u / 0.002f;
        x = (tone * q[7] + fl * q[6] * 1.5f) * env;
        break; }
    case 15: x = spk_render(s, v, p, n); break;                    /* SPEECH */
    default: {                                                      /* MORPH: two real wavetables crossfaded over time */
        const float f = v->f0;
        v->nx[3] += q[4] * dt; if (v->nx[3] >= 1.0f) v->nx[3] -= 1.0f;
        float m = q[3] + (q[2] - q[3]) * v->env2 + q[5] * sin2pi(v->nx[3]); m = clampf(m, 0.0f, 1.0f);
        const int ta = (int)q[0], tb = (int)q[1];
        v->ph += f * dt; WRAP(v->ph);
        float osc = (1.0f - m) * wt_at(v->ph, ta) + m * wt_at(v->ph, tb);
        if (q[6] > 0.0f) {
            v->nx[0] += f * (1.0f + q[6]) * dt; if (v->nx[0] >= 1.0f) v->nx[0] -= 1.0f;
            v->nx[1] += f * (1.0f - q[6]) * dt; if (v->nx[1] >= 1.0f) v->nx[1] -= 1.0f;
            osc = osc * 0.5f + 0.25f * ((1.0f - m) * wt_at(v->nx[0], ta) + m * wt_at(v->nx[0], tb) + (1.0f - m) * wt_at(v->nx[1], ta) + m * wt_at(v->nx[1], tb));
        }
        if (q[9] > 0.0f) { v->nx[2] += f * 0.5f * dt; if (v->nx[2] >= 1.0f) v->nx[2] -= 1.0f; osc += q[9] * sin2pi(v->nx[2]); }
        float fc = f * q[7] * (1.0f + q[8] * v->env2 * 4.0f); if (fc > 16000.0f) fc = 16000.0f; if (fc < 100.0f) fc = 100.0f;
        const float xx = TWO_PI * fc * dt, a = xx / (1.0f + xx);
        v->z1 += a * (osc - v->z1); v->z2 += a * (v->z1 - v->z2);
        if (T->sus) {
            const float at = att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f);
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
        float body = sin2pi(v->ph) * v->env;
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
        x = (sin2pi(v->ph) * 0.5f + sin2pi(v->ph2) * 0.4f) * v->env + (nz - v->z1) * v->env2 * 0.6f;
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
        x = m * v->env * 0.9f + sin2pi(v->ph) * v->env2 * 0.25f;
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
        x = sin2pi(v->ph + (3.0f * v->env2 * sin2pi(v->ph2)) * 0.15915494f) * v->env;
        v->env *= v->c1; v->env2 *= v->c2;
        if (v->env < 1e-3f) v->active = 0;
        break; }
    case T_ZAP: {
        float f = v->f0 + 4800.0f * fm * v->pe;
        v->pe *= v->c2;
        v->ph += f * dt; WRAP(v->ph);
        x = sin2pi(v->ph) * v->env;
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
        if (v->gate) v->env += (1.0f - v->env) * (dt / att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f));
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
        x = (v->z2 * 0.7f + sin2pi(v->ph) * 0.6f) * v->env;
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
        if (v->gate) v->env += (1.0f - v->env) * (dt / att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f));
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
        if (v->gate) v->env += (1.0f - v->env) * (dt / att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f));
        else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = v->z2 * v->env * 0.7f;
        break; }
    case T_LEAD: {
        v->ph  += v->f0 * dt;          WRAP(v->ph);
        v->ph2 += v->f0 * 1.005f * dt; WRAP(v->ph2);
        float osc = v->sw ? wf(v->ph, v->sw) * 0.6f + wf(v->ph2, v->sw) * 0.4f : (2.0f * v->ph - 1.0f) * 0.6f + (v->ph2 < 0.5f ? 0.4f : -0.4f);
        v->z1 += s->a_lead * (osc - v->z1);
        v->z2 += s->a_lead * (v->z1 - v->z2);
        if (v->gate) v->env += (1.0f - v->env) * (dt / att_time(p, s->pp[p][PP_ATT], T->att, 0.0005f, 2.0f));
        else { v->env *= v->c1; if (v->env < 1e-3f) v->active = 0; }
        x = v->z2 * v->env;
        break; }
    case T_BELL: {
        v->ph  += v->f0 * dt;        WRAP(v->ph);
        v->ph2 += v->f0 * 3.5f * dt; WRAP(v->ph2);
        x = sin2pi(v->ph + (4.0f * v->env2 * sin2pi(v->ph2)) * 0.15915494f) * v->env;
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
        x = sin2pi(v->ph) * v->env * 0.9f + v->z1 * v->env2 * 1.5f;
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
            osc = (wf(v->ph, v->sw) + wf(v->ph2, v->sw)) * 0.5f + sin2pi(v->ph3) * v->ssub;
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
            osc += sin2pi(v->ph3) * v->ssub;
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
            if (v->sw >= 100) { float pp_ = v->ph + idx * 0.15915f * sin2pi(v->ph2); pp_ -= floorf(pp_); osc = wt_at(pp_, v->sw - 100) * 1.2f + sin2pi(v->ph3) * v->ssub; }
            else osc = sin2pi(v->ph + (idx * sin2pi(v->ph2)) * 0.15915494f) + sin2pi(v->ph3) * v->ssub;
            osc *= 0.7f;
        } else {                                      /* organ: drawbars 16' 8' 4' 2 2/3' */
            v->ph  += f * dt;        WRAP(v->ph);
            v->ph2 += f * 2.0f * dt; WRAP(v->ph2);
            v->ph3 += f * 3.0f * dt; WRAP(v->ph3);
            v->z3  += f * 0.5f * dt; WRAP(v->z3);
            v->z4  += f * 4.0f * dt; WRAP(v->z4);
            osc = (v->sw >= 100 ? wt_at(v->ph, v->sw - 100) : sin2pi(v->ph)) * 0.7f + sin2pi(v->ph2) * 0.5f + sin2pi(v->ph3) * (0.2f + d * 8.0f)
                + sin2pi(v->z3) * v->ssub * 1.4f + sin2pi(v->z4) * 0.25f;
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
    case T_E_MODAL: case T_E_FM: case T_E_VOX: case T_E_CHIP: case T_E_MORPH: case T_E_DRUM: case T_E_ANA: case T_E_WT: case T_E_FM4: case T_E_ADD: case T_E_PAD: case T_E_STR: case T_E_FLD: case T_E_AC: case T_E_FXE: case T_E_SPK: x = nx_render(s, v, p); break;
    default: v->active = 0; break;
    }
    if (p < NTRK && v->gate && TY[v->type].sus) {                    /* Decay on sustained sounds: the same slope as the FM sound (time constant 0.8 s x the Decay knob law), so a pad and an FM voice answer the knob alike; the very top of the knob (>= 0.97) holds the note */
        const float dv = s->pp[p][PP_DECAY];
        if (dv < 0.97f) { float td = 0.8f * s->dm[p]; if (td < 0.002f) td = 0.002f; v->hd *= expf(-dt / td); x *= v->hd; }
    }
    if (v->aenv < 1.0f) { v->aenv += (1.0f - v->aenv) * v->ac * 3.0f; if (v->aenv > 0.995f) v->aenv = 1.0f; x *= v->aenv; }
    v->t += dt;
    return x;
}

/* ---------------- arp ---------------- */
static int arp_offset(inst_t *s, int p, int n) {
    int arp = (int)(s->pp[p][PP_ARP] + 0.5f);
    const int *tb = SCALE_TBL[eff_scale(s, p)];
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

static float rep_step_samples_r(inst_t *s, int r) { if (r < 0) r = s->rep_idx; if (r < 0 || r > 15) r = 8; return s->sr * 60.0f / s->bpm / RPT_SPB[r]; }
static float rep_step_samples(inst_t *s) { return rep_step_samples_r(s, -1); }

static void pad_on(inst_t *s, int p, int vel) {
    float v = (float)vel / 127.0f;
    ar_t *a = &s->ar[p];
    int arp = (int)(s->pp[p][PP_ARP] + 0.5f);
    a->held = 1; a->vel = v; a->base = 0; a->key = -1;
    if (arp > 0) {
        a->on = 1; a->n = 0; s->act_dirty = 1;
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
static inline int prob_idx(int b) { return (b & 0x80) ? (b & 0x1F) : ((((b & 7) << 1) | ((b >> 3) & 1)) << 1); }       /* 32 levels (3.1 % each); older saves stored 8 or 16 levels */

static int note_pad_semi(inst_t *s, int t, int d) {          /* d = 0..31 scale degree; 32..95 = chromatic semitone (d-32) */
    const int ksh = eff_key(s, t) - 5;       /* the KEY moves the pads (the sounds' own root is F = key 5) */
    if (d >= 32) return d - 32 - 12 + ksh;
    int sc = eff_scale(s, t);
    return 12 * (d / MSN[sc]) + MS7[sc][d % MSN[sc]] - 12 + ksh;
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
    s->clip[t] = c; s->tph[t] = 0; s->fol_cnt[t] = 0;
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
#ifdef PS_TRACE
    fprintf(stderr, "F %.0f %d\n", s->tsamp, t);
#endif
    if (!s->mute[lane_trk(s, t)]) {
        ar_t *ap = &s->ar[t];
        if (t < NTRK && s->rt_n[t] == 1 && (int)(s->pp[t][PP_ARP] + 0.5f) > 0) {      /* ARP on a sequenced step: it runs for the length of the note */
            s->act_dirty = 1; ap->on = 1; ap->n = 1; ap->base = s->rt_semi[t]; ap->key = -2; ap->vel = s->rt_vel[t]; ap->cnt = arp_step_samples(s, t);
            ap->life = (float)(s->rt_gate[t] > 64 ? s->rt_gate[t] : 64);
            trig(s, t, s->rt_vel[t], s->rt_semi[t] + arp_offset(s, t, 0), 1);
        } else {
            if (ap->key == -2) { ap->on = 0; ap->key = -1; }
            trig(s, t, s->rt_vel[t], s->rt_semi[t], 1);
        }
        { const int tk = lane_trk(s, t); if (s->mo_trk & (1 << tk)) mo_note(s, t, tk, s->rt_vel[t], s->rt_semi[t], s->rt_gate[t]); }
        if (TY[s->vc[t].type].sus) { s->gate_left[t] = s->rt_gate[t] > 0 ? s->rt_gate[t] : 1; s->act_dirty = 1; }
    }
    s->rt_vel[t] *= 0.88f;
    if (--s->rt_n[t] > 0) s->rt_cnt[t] += s->rt_int[t];
}

/* swing phase of step g: 0 = long (on-beat), 1 = short (off-beat), 2 = neutral. With an odd main loop length the groove restarts with every pass (last step neutral), so the loop sounds the same each time. */
static inline void trk_spd(const inst_t *s, int tk, int *n, int *d) { *n = s->tsn[tk] > 0 ? s->tsn[tk] : 1; *d = s->tsd[tk] > 0 ? s->tsd[tk] : 1; }
static int sw_ph(const inst_t *s, long g) {
    int M = 16; for (int t = 0; t < NTRK; t++) { const int lt = s->seqlen[t] * s->tres[t]; if (lt != 96 && lt > 0 && lt % 6 == 0) { M = lt / 6; break; } }
    if (M & 1) { long i = g % M; return i == M - 1 ? 2 : (int)(i & 1); }
    return (int)(g & 1);
}
static void seq_step(inst_t *s) {
    const float L = s->sr * 60.0f / s->bpm / 4.0f;
    const int sub = s->sub; const long gtk = (long)s->gs * 6 + sub;       /* tick counter: 6 per 1/16 */
    if (sub == 0) {   /* everything that lives on the 1/16 grid */
    {   /* follow actions: when a clip has played its loops, queue the next one */
        static const int LP[6] = { 1, 2, 3, 4, 8, 16 };
        if (s->gs > 0) for (int t = 0; t < NTRK; t++) {
            const int c = s->clip[t]; if (c < 0 || c >= NPAT || s->qclip[t] != -2) continue;
            const int act = s->fol[t][c] & 7; if (act == 0) continue;
            int len = s->seqlen[t]; if (len < 1) len = 1;
            { const long lt = (long)len * s->tres[t]; int sn, sd; trk_spd(s, t, &sn, &sd); long G = gtk - (long)s->tph[t] * 6; if (G < 0) G = 0;
              if (sn == sd) { if ((G % lt) != 0) continue; }
              else { const long a = (G * sn + sd - 1) / sd, b = ((G + 1) * sn + sd - 1) / sd; int hit = 0; for (long q = a; q < b; q++) if (q % lt == 0) hit = 1; if (!hit) continue; } }
            int li = (s->fol[t][c] >> 3) & 7; if (li > 5) li = 5;
            if (++s->fol_cnt[t] < LP[li]) continue;
            s->fol_cnt[t] = 0;
            int tgt = c;
            if (act == 5) tgt = -1;
            else {
                int cand[NPAT], n = 0;
                for (int k = 0; k < NPAT; k++) if (clip_has(s, k, t)) cand[n++] = k;
                if (n == 0) continue;
                if (act == 1 || act == 2) {
                    int d = (act == 1) ? 1 : NPAT - 1;
                    for (int k = 1; k <= NPAT; k++) { int q = (c + d * k) % NPAT; if (clip_has(s, q, t)) { tgt = q; break; } }
                } else if (act == 3) tgt = cand[0];
                else if (act == 4) {
                    s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
                    if (n > 1) { int pick = (int)((s->rng >> 8) % (unsigned)(n - 1)), k = 0; for (int i = 0; i < n; i++) { if (cand[i] == c) continue; if (k++ == pick) { tgt = cand[i]; break; } } }
                }
            }
            if (tgt != c) s->qclip[t] = tgt;
        }
    }
    {
        float qv = s->g[G_QNT];
        static const int QM[7] = { 0, 1, 3, 7, 15, 31, 63 };   /* 1/16 1/8 1/4 1/2 bar 2bar 4bar */
        int qi = (int)(qv * 7.0f); if (qi < 0) qi = 0; if (qi > 6) qi = 6;
        int qm = QM[qi];
        if (s->pend_on) qm = s->pend_qm;
        if ((s->gs & qm) == 0) { apply_pending(s); s->pend_on = 0; }
    }
    if (s->rec_state == 1 && (s->gs & 15) == 0) { s->rec_state = 2; s->rec_n = 0; s->rec_len = (long)((float)(s->rec_bars * 16) * L); if (s->rec_len > s->rec_max) s->rec_len = s->rec_max; }
    if (s->g[G_MET] > 0.01f && (s->gs & 3) == 0) { s->met_amp = s->g[G_MET]; s->met_env = 1.0f; s->met_ph = 0.0f; s->met_f = ((s->gs & 15) == 0) ? 1700.0f : 1100.0f; }
    }
    for (int t = 0; t < NLANE; t++) {                              /* t = lane */
        const int tk = lane_trk(s, t);
        const int res = s->tres[tk];
        int sn, sd; trk_spd(s, tk, &sn, &sd);
        if (res == 6 && sub != 0 && sn == sd) continue;
        if (!lane_on(s, t)) continue;
        int len = s->llen[t] > 0 ? s->llen[t] : s->seqlen[tk]; if (len < 1) len = 1; if (len > MAXSTEP) len = MAXSTEP;
        if (s->clip[tk] < 0) continue;
        long G = gtk - (long)s->tph[tk] * 6; if (G < 0) G = 0;
        long lt0 = G, lt1 = G + 1;                                  /* the ticks of this track's own clock that begin inside this global tick (speed ratio sn/sd) */
        if (sn != sd) { lt0 = (G * sn + sd - 1) / sd; lt1 = ((G + 1) * sn + sd - 1) / sd; }
        for (long gl6 = lt0; gl6 < lt1; gl6++) {
        if (gl6 % res) continue;                                   /* not a step of this track's grid */
        const int gl = (int)(gl6 / res);
        const float Ls = L * (float)res / 6.0f * (float)sd / (float)sn;      /* one step of this grid, in samples (a slower track has longer steps) */
        if ((gl % len) == 0) s->spk_wi[t] = 0;                      /* a speech sound that says one word per note starts again with the loop */
        const uint8_t *st = s->stp[s->clip[tk]][t][gl % len];
        if (s->rskip[t]) { const long ab = (gl6 + (long)s->tph[tk] * 6 * sn / sd) / res; if (s->rskip[t] == ab + 1) { s->rskip[t] = 0; continue; } if (s->rskip[t] < ab + 1) s->rskip[t] = 0; }      /* never leave an old skip behind */
        if (!(st[0] & 0x80) || s->mute[tk] || s->lmute[t] || s->rechold[tk]) continue;
        if (prob_idx(st[1]) > 0) {
            s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
            if ((float)(s->rng & 0xFFFF) / 65536.0f > 1.0f - 0.03125f * (float)prob_idx(st[1])) continue;
        }
        {   /* condition: play only on certain passes of the loop (1:2, 2:2, 1:3 ... 4:4) */
            static const int CN[8] = { 1, 2, 2, 3, 3, 3, 4, 4 }, CK[8] = { 0, 0, 1, 0, 1, 2, 0, 3 };
            int cd = (st[3] >> 5) & 7;
            if (cd && ((gl / len) % CN[cd]) != CK[cd]) continue;
        }
        if (s->plk_t[t]) { const int lv = st[5]; s->pp[t][s->plk_t[t]] = lv ? (float)(lv - 1) / 126.0f : s->plk_b[t]; }       /* parameter lock: this step's value, or back to the base */
        int semi = (int)st[2] - 64;
        float hv = (float)(st[0] & 0x7F) / 127.0f;
        if (s->g[G_HUM] > 0.01f) {
            s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
            hv *= 1.0f + s->g[G_HUM] * 0.6f * ((float)(s->rng & 0xFFFF) / 65536.0f - 0.5f);
            if (hv > 1.0f) hv = 1.0f; if (hv < 0.05f) hv = 0.05f;
        }
        int grv = (int)(s->g[G_GRV] + 0.5f); if (grv < 0 || grv > 10) grv = 0;
        const int gp = (int)(gl6 % 96); const int grv_on = grv > 0 && (gp % 6) == 0;
        if (grv_on) { hv *= 1.0f + (GRV_V[grv][gp / 6] - 1.0f) * s->g[G_GAM]; if (hv > 1.0f) hv = 1.0f; if (hv < 0.05f) hv = 0.05f; }
        {
            static const float GT[8] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 8.0f };
            int n = 1 + (st[3] & 3), gi = (st[3] >> 2) & 7;
            float iv = Ls / (float)n;
            float gs_ = gi == 0 ? 0.8f * iv : GT[gi] * Ls;
            if (st[6] > 0) gs_ = (float)st[6] * Ls - 1.0f;      /* held-step length: lasts exactly until the chosen step */
            if (n > 1 && gs_ > 0.9f * iv) gs_ = 0.9f * iv;
            s->act_dirty = 1; s->rt_n[t] = n; s->rt_int[t] = iv; s->rt_semi[t] = semi; s->rt_vel[t] = hv; s->rt_gate[t] = (int)gs_;
            s->rt_cnt[t] = (float)((st[4] & 0x40) ? (st[4] & 0x3F) : (st[4] & 7) * 4) * 0.025f * Ls;      /* nudge: delay up to 70 % of a step in 2.5 % steps (older saves: 10 % steps) */
            if (sn != sd) s->rt_cnt[t] += (float)(gl6 * sd - G * sn) / (float)sn * (L / 6.0f);      /* the exact moment inside the tick */
            if (grv_on) s->rt_cnt[t] += GRV_T[grv][gp / 6] * s->g[G_GAM] * L;      /* groove template: this sixteenth comes a little late */
            if (sw_ph(s, s->gs) == 1 && s->pp[tk][PP_TSW] > 0.52f) s->rt_cnt[t] += (s->pp[tk][PP_TSW] - 0.5f) * 0.5f * Ls;   /* per-track extra swing on the off-beat steps */
            if (s->rt_cnt[t] <= 0.0f) rt_fire(s, t);
        }
        }
    }
    float off = (2.0f * s->swing - 1.0f) * L;
    s->last_time = s->next_time;
    { const int ph = sw_ph(s, s->gs); s->next_time += (double)((L + (ph == 2 ? 0.0f : (ph == 0 ? off : -off))) / 6.0f); }
    if (++s->sub >= 6) { s->sub = 0; s->gs++; }
}

static void seq_start(inst_t *s) { memset(s->tph, 0, sizeof s->tph); if (s->mo_clk) { mo_send(0xF, 0xFA, 0, 0); s->mclk = 0.0f; } s->playing = 1; s->tsamp = 0.0; s->next_time = 0.0; s->last_time = 0.0; s->gs = 0; s->sub = 0; s->pump_ph = 0.0f; s->wob_ph = 0.0f; }
static void seq_stop(inst_t *s) { memset(s->rskip, 0, sizeof s->rskip); memset(s->tph, 0, sizeof s->tph);
    if (s->playing && s->mo_clk) mo_send(0xF, 0xFC, 0, 0);
    for (int i = 0; i < 64; i++) if (s->mo_q[i].cnt > 0) { mo_send(0x8, s->mo_q[i].st, s->mo_q[i].d1, 0); s->mo_q[i].cnt = 0; }
    s->playing = 0; s->gs = 0; s->sub = 0;
    for (int t = 0; t < NTRK; t++) if (s->qclip[t] != -2) { clip_activate(s, t, s->qclip[t]); s->qclip[t] = -2; }
    for (int t = 0; t < NLANE; t++) { s->gate_left[t] = 0; s->rt_n[t] = 0; s->vc[t].gate = 0; if (s->ar[t].key == -2) { s->ar[t].on = 0; s->ar[t].key = -1; } }
    for (int i = 0; i < 32; i++) s->eq[i].cnt = 0;
}

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0)); }
static void put_step(inst_t *s, int pat, int t, int st, const char *h, int w) {
    if (pat < 0 || pat >= NPAT || t < 0 || t >= NLANE || st < 0 || st >= MAXSTEP) return;
    const int bb = (hexv(h[2]) << 4) | hexv(h[3]), ee = (hexv(h[8]) << 4) | hexv(h[9]), ff = w >= 12 ? (hexv(h[10]) << 4) | hexv(h[11]) : 0;
    s->stp[pat][t][st][0] = (uint8_t)((hexv(h[0]) << 4) | hexv(h[1]));
    s->stp[pat][t][st][1] = (uint8_t)(bb & 0x9F);
    s->stp[pat][t][st][2] = (uint8_t)((hexv(h[4]) << 4) | hexv(h[5]));
    s->stp[pat][t][st][3] = (uint8_t)((hexv(h[6]) << 4) | hexv(h[7]));
    s->stp[pat][t][st][4] = (uint8_t)(ee & 0x7F);
    s->stp[pat][t][st][5] = (uint8_t)(ff & 0x7F);
    /* exact step length 1..15 steps (0 = use the gate setting): 4 spare bits - probability byte bits 5-6, nudge byte bit 7, lock byte bit 7 */
    s->stp[pat][t][st][6] = (bb & 0x80) ? (uint8_t)(((bb >> 5) & 1) | (((bb >> 6) & 1) << 1) | (((ee >> 7) & 1) << 2) | (((ff >> 7) & 1) << 3)) : 0;
}

/* a pad event from Move (or the UI): 0..15 = track sound, 16..31 = note pads for the selected track */
static void pad_event(inst_t *s, int idx, int vel) {
    if (idx < 0 || idx >= NPADS) return;
    if (idx < NTRK) { if (vel > 0) pad_on(s, idx, vel); else pad_off(s, idx); return; }
    int k = idx - NTRK;
    if (vel > 0) {
        int t = s->sel_track;
        s->np_trk[k] = t;
        trig(s, t, (float)vel / 127.0f, note_pad_semi(s, t, k) + s->pad_oct, 1);
        s->ar[t].held = 1;
    } else {
        int t = s->np_trk[k];
        if (t >= 0 && t < NLANE) {
            if (s->vc[t].gk == k + 1 || s->vc[t].gk == 0) s->vc[t].gate = 0;
            for (int i = 0; i < NGH; i++) if (s->vc[NLANE + i].active && s->vown[NLANE + i] == t && s->vc[NLANE + i].gk == k + 1) s->vc[NLANE + i].gate = 0;
        }
    }
}

static void rep_clear(inst_t *s) { for (int i = 0; i < REP_N; i++) s->rp_on[i] = 0; }
static void rep_stop(inst_t *s, int k) { for (int i = 0; i < REP_N; i++) if (s->rp_on[i] && s->rp_k[i] == k) s->rp_on[i] = 0; }
static int rep_start(inst_t *s, int t, int k, float semi, float vel) {
    int f = -1; for (int i = 0; i < REP_N; i++) { if (s->rp_on[i] && s->rp_k[i] == k) { f = i; break; } if (f < 0 && !s->rp_on[i]) f = i; }
    if (f < 0) f = 0;
    s->rp_on[f] = 1; s->rp_t[f] = t; s->rp_k[f] = k; s->rp_semi[f] = semi; s->rp_vel[f] = vel; s->rp_cnt[f] = rep_step_samples(s); s->rp_rate[f] = -1; s->rp_ramp[f] = 0; s->rp_cap[f] = vel;
    return f;
}
/* pad k (0..31) plays the selected track chromatically in the scale; repeat = retrigger while held at the track's rate */
static void sel_pad(inst_t *s, int k, int vel, int repeat) {
    if (k < 0 || k >= 96) return;
    const int dry = repeat == 2; if (dry) repeat = 0;                       /* 2 = a single note without the MIDI effects (the note half of the CHORD pads) */
    int kk = k & 31;
    if (vel > 0) {
        int t = s->sel_track; if (t < 0 || t >= NLANE) t = 0;
        s->np_trk[kk] = t;
        int sm = note_pad_semi(s, t, k) + s->pad_oct;
        ar_t *ap = &s->ar[t];
        if (!repeat && (int)(s->pp[t][PP_ARP] + 0.5f) > 0) {               /* ARP: a held note pad steps through the scale at the Rate */
            s->act_dirty = 1; ap->on = 1; ap->n = 1; ap->base = sm; ap->key = kk; ap->vel = (float)vel / 127.0f; ap->cnt = arp_step_samples(s, t);
            trig(s, t, (float)vel / 127.0f, sm + arp_offset(s, t, 0), 1);
        } else {
            if (ap->key >= 0) { ap->on = 0; ap->key = -1; }
            if (dry) trig_raw(s, t, (float)vel / 127.0f, sm, 1); else trig(s, t, (float)vel / 127.0f, sm, 1);
        }
        s->vc[t].gk = k + 1;
        s->ar[t].held = 1;
        if (repeat) rep_start(s, t, k, (float)sm, (float)vel / 127.0f);
    } else {
        rep_stop(s, k);
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
        /* repeat: 0 off, 1 = the Note Repeat speed, 2.. = a roll that fits the sound: (repeat - 2) & 15 = speed index, (repeat - 2) >> 4 = shape (1 build up gently, 2 build up strongly, 3 fade out) */
        const int ramp = repeat >= 2 ? ((repeat - 2) >> 4) & 3 : 0;
        const float v0 = (float)vel / 127.0f, vs = ramp == 1 ? v0 * 0.7f : (ramp == 2 ? v0 * 0.45f : v0);
        trig(s, l, vs, 0, 1);
        s->vc[l].gk = 99;
        if (repeat) { int f = rep_start(s, l, 100 + l, 0.0f, vs); if (repeat >= 2) { s->rp_rate[f] = (repeat - 2) & 15; s->rp_ramp[f] = ramp; s->rp_cap[f] = v0; s->rp_cnt[f] = rep_step_samples_r(s, s->rp_rate[f]); } }
    } else {
        rep_stop(s, 100 + l);
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
            s->tp_rng = 2463534242u;
            s->focused = 1;
            s->pend_pad = -1;
            s->rec_src = -1; s->rec_slot = -1;
            for (int k = 0; k < G_N; k++) s->g[k] = GD[k].def; s->lim_g = 1.0f;
            s->bpm = 120.0f; s->swing = 0.5f; s->rep_idx = 8;
            for (int t = 0; t < NTRK; t++) { s->seqlen[t] = 16; s->tg[t] = 1.0f; s->tres[t] = 6; s->tsn[t] = 1; s->tsd[t] = 1; }
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
    const int rcol = p & 7;
    int vel = (st == 0x90 && msg[2] > 0) ? msg[2] : 0;
    if (s->pads_off == 4 && msg[1] >= 68 && msg[1] <= 99) return;      /* LIVE FX: every pad belongs to the UI (effect pads on top, fill hits at the bottom) - no notes of the synth */
    if (msg[1] >= 68 && msg[1] <= 99) {                                 /* a Move pad: the same layout as the UI (MOVE rows 3 degrees apart, SEQ, OCT, CHR) */
        const int col = p & 7, row = p >> 3;
        if (s->pads_off == 5) {                                          /* CHORD mode: left 4 columns = single notes (no MIDI FX), right 4 columns = the chords of the same degrees */
            const int c4 = col & 3, w4 = 4;
            p = s->lay_s >= 3 ? 32 + c4 + (s->lay_s == 3 ? 5 : (s->lay_s == 4 ? 4 : 8)) * row : (s->lay_s == 0 ? c4 + 3 * row : (s->lay_s == 1 ? row * w4 + c4 : c4 + 7 * row));
            sel_pad(s, p, vel, col < 4 ? 2 : 0);
            return;
        }
        p = s->lay_s >= 3 ? 32 + col + (s->lay_s == 3 ? 5 : (s->lay_s == 4 ? 4 : 8)) * row : (s->lay_s == 0 ? col + 3 * row : (s->lay_s == 1 ? row * 8 + col : col + 7 * row));
    }
    if (s->pads_off == 1) { if (!vel) sel_pad(s, p, 0, 0); return; }     /* UI owns the pads (session, steps, menus) */
    sel_pad(s, p, vel, s->pads_off == 3 && rcol >= 4);      /* NOTE REPEAT: only the right 16 pads repeat (several at once), the left 16 play normally */
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
        else if (k == PP_XFX || k == PP_MFX || k == PP_X2FX || k == PP_M2FX)  { v = clampf((float)atof(val), 0.0f, (k == PP_XFX || k == PP_X2FX) ? 13.0f : 11.0f); }
        else if (k == PP_DMODE || k == PP_RMODE || k == PP_MONO || k == PP_DMD) { v = clampf((float)atof(val), 0.0f, 2.0f); }
        else if (k == PP_DLN) { v = clampf((float)atof(val), 0.0f, 6.0f); }
        else if (k == PP_TKEY) { v = clampf((float)atof(val), 0.0f, 12.0f); }
        else if (k == PP_TSCL) { v = clampf((float)atof(val), 0.0f, (float)NSC); }
        else v = clampf((float)atof(val), 0.0f, 1.0f);
        s->pp[p][k] = v;
        if (s->plk_t[p] == k) s->plk_b[p] = v;
        update_pad(s, p);
        return;
    }
}


/* ---- MIDI file export: "path|clip" (clip 0..7, or -1 = all clips one after another). Type 1 file, 480 ticks per quarter, one track per sound with steps.
 * Drum sounds use the Move drum rack notes 36..51, synth tracks use C3 (60) + semitones. Swing, nudge, ratchet and gate are written as they play; probability and conditions are ignored. */
typedef struct { uint8_t *b; int n, cap; } mb_t;
static void mb_put(mb_t *m, int v) { if (m->n >= m->cap) { m->cap = m->cap ? m->cap * 2 : 4096; m->b = (uint8_t *)realloc(m->b, (size_t)m->cap); } if (m->b) m->b[m->n++] = (uint8_t)v; }
static void mb_vlq(mb_t *m, long v) { uint8_t t[5]; int k = 0; t[k++] = (uint8_t)(v & 0x7F); while ((v >>= 7) > 0) t[k++] = (uint8_t)(0x80 | (v & 0x7F)); while (k) mb_put(m, t[--k]); }
typedef struct { long tick; int on, note, vel; } me_t;
static int me_cmp(const void *a, const void *b) { const me_t *x = (const me_t *)a, *y = (const me_t *)b; if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1; return x->on - y->on; }
static char g_mx_res[96] = "none";
static void midi_export(inst_t *s, const char *val) {
    char path[200]; int clip = 0;
    const char *bar = strchr(val, '|'); if (!bar || bar - val >= (int)sizeof path) { snprintf(g_mx_res, sizeof g_mx_res, "error,bad request"); return; }
    memcpy(path, val, (size_t)(bar - val)); path[bar - val] = 0; clip = atoi(bar + 1);
    if (clip < -1 || clip >= NPAT) { snprintf(g_mx_res, sizeof g_mx_res, "error,bad clip"); return; }
    const int c0 = clip < 0 ? 0 : clip, c1 = clip < 0 ? NPAT - 1 : clip, TQ = 120;      /* ticks per 1/16 step */
    long start[NPAT + 1]; long total = 0;
    for (int c = c0; c <= c1; c++) {                                  /* length of every clip section: the longest lane, rounded up to whole bars */
        int mx = 0;
        for (int l = 0; l < NLANE; l++) {
            if (!lane_on(s, l)) continue;
            int any = 0; for (int i = 0; i < MAXSTEP && !any; i++) if (s->stp[c][l][i][0] & 0x80) any = 1;
            if (!any) continue;
            const int tk = lane_trk(s, l); int len = s->llen[l] > 0 ? s->llen[l] : s->seqlen[tk]; if (len < 1) len = 1; if (len > MAXSTEP) len = MAXSTEP;
            if (len > mx) mx = len;
        }
        start[c] = total; if (mx > 0) total += (long)((mx + 15) / 16) * 16;
    }
    start[c1 + 1 > NPAT ? NPAT : c1 + 1] = total;
    if (total == 0) { snprintf(g_mx_res, sizeof g_mx_res, "error,nothing to export"); return; }
    mb_t file = { 0, 0, 0 }; int ntr = 0; mb_t tracks = { 0, 0, 0 };
    me_t *ev = (me_t *)malloc(sizeof(me_t) * 20000); if (!ev) { snprintf(g_mx_res, sizeof g_mx_res, "error,memory"); return; }
    /* tempo track */
    { mb_t t = { 0, 0, 0 }; long us = (long)(60000000.0f / s->bpm);
      mb_put(&t, 0); mb_put(&t, 0xFF); mb_put(&t, 0x51); mb_put(&t, 3); mb_put(&t, (int)((us >> 16) & 255)); mb_put(&t, (int)((us >> 8) & 255)); mb_put(&t, (int)(us & 255));
      mb_put(&t, 0); mb_put(&t, 0xFF); mb_put(&t, 0x58); mb_put(&t, 4); mb_put(&t, 4); mb_put(&t, 2); mb_put(&t, 24); mb_put(&t, 8);
      mb_put(&t, 0); mb_put(&t, 0xFF); mb_put(&t, 0x2F); mb_put(&t, 0);
      static const uint8_t H[] = { 'M','T','r','k' }; for (int i = 0; i < 4; i++) mb_put(&tracks, H[i]);
      mb_put(&tracks, (t.n >> 24) & 255); mb_put(&tracks, (t.n >> 16) & 255); mb_put(&tracks, (t.n >> 8) & 255); mb_put(&tracks, t.n & 255);
      for (int i = 0; i < t.n; i++) mb_put(&tracks, t.b[i]); free(t.b); ntr++; }
    static const float GT[8] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 8.0f };
    for (int l = 0; l < NLANE; l++) {
        int ne = 0; const int tk = lane_trk(s, l), drum = l >= NTRK;
        if (!lane_on(s, l)) continue;
        for (int c = c0; c <= c1 && ne < 19000; c++) {
            const long len0 = (c + 1 <= c1 ? start[c + 1] : total) - start[c]; if (len0 <= 0) continue;
            int len = s->llen[l] > 0 ? s->llen[l] : s->seqlen[tk]; if (len < 1) len = 1; if (len > MAXSTEP) len = MAXSTEP;
            const int rs = s->tres[tk]; int sn_, sd_; trk_spd(s, tk, &sn_, &sd_); const double rsf = (double)rs * sd_ / sn_;      /* a slower track: longer steps */
            for (long g = 0; (double)g * rsf < (double)len0 * 6 && ne < 19000; g++) {
                const uint8_t *st = s->stp[c][l][g % len]; if (!(st[0] & 0x80)) continue;
                const int semi = (int)st[2] - 64, vel = st[0] & 0x7F; int note = drum ? 36 + ((l - NTRK) & 15) : 60 + semi; if (note < 0) note = 0; if (note > 127) note = 127;
                const float sw = (sn_ == sd_ && (g * rs) % 6 == 0 && ((g * rs / 6) & 1)) ? (2.0f * s->swing - 1.0f) * TQ + (s->pp[tk][PP_TSW] > 0.52f ? (s->pp[tk][PP_TSW] - 0.5f) * 0.5f * TQ : 0.0f) : 0.0f;
                const float TS = (float)((double)TQ / 6.0 * rsf);
                const float nd = (float)((st[4] & 0x40) ? (st[4] & 0x3F) : (st[4] & 7) * 4) * 0.025f * TS;
                const int n = 1 + (st[3] & 3), gi = (st[3] >> 2) & 7; const float iv = TS / (float)n;
                float gl = gi == 0 ? 0.8f * iv : GT[gi] * TS; if (n > 1 && gl > 0.9f * iv) gl = 0.9f * iv;
                for (int r = 0; r < n && ne < 19998; r++) {
                    long t0 = start[c] * TQ + (long)((double)g * rsf * ((double)TQ / 6.0)) + (long)(sw + nd + iv * r), t1 = t0 + (long)(gl < 1.0f ? 1.0f : gl);
                    ev[ne].tick = t0; ev[ne].on = 1; ev[ne].note = note; ev[ne].vel = vel ? vel : 1; ne++;
                    ev[ne].tick = t1; ev[ne].on = 0; ev[ne].note = note; ev[ne].vel = 0; ne++;
                }
            }
        }
        if (!ne) continue;
        qsort(ev, (size_t)ne, sizeof(me_t), me_cmp);
        mb_t t = { 0, 0, 0 }; char nm[32]; int nl = snprintf(nm, sizeof nm, drum ? "Track %d pad %d" : "Track %d", tk + 1, ((l - NTRK) & 15) + 1);
        mb_put(&t, 0); mb_put(&t, 0xFF); mb_put(&t, 3); mb_put(&t, nl); for (int i = 0; i < nl; i++) mb_put(&t, nm[i]);
        long last = 0; const int ch = drum ? 9 : (tk & 7);
        for (int i = 0; i < ne; i++) { mb_vlq(&t, ev[i].tick - last); last = ev[i].tick; mb_put(&t, (ev[i].on ? 0x90 : 0x80) | ch); mb_put(&t, ev[i].note); mb_put(&t, ev[i].vel); }
        mb_vlq(&t, (long)total * TQ - last > 0 ? (long)total * TQ - last : 0); mb_put(&t, 0xFF); mb_put(&t, 0x2F); mb_put(&t, 0);
        static const uint8_t H[] = { 'M','T','r','k' }; for (int i = 0; i < 4; i++) mb_put(&tracks, H[i]);
        mb_put(&tracks, (t.n >> 24) & 255); mb_put(&tracks, (t.n >> 16) & 255); mb_put(&tracks, (t.n >> 8) & 255); mb_put(&tracks, t.n & 255);
        for (int i = 0; i < t.n; i++) mb_put(&tracks, t.b[i]); free(t.b); ntr++;
    }
    free(ev);
    if (ntr < 2) { free(tracks.b); snprintf(g_mx_res, sizeof g_mx_res, "error,nothing to export"); return; }
    static const uint8_t H[] = { 'M','T','h','d', 0,0,0,6, 0,1 };
    for (int i = 0; i < 10; i++) mb_put(&file, H[i]);
    mb_put(&file, (ntr >> 8) & 255); mb_put(&file, ntr & 255); mb_put(&file, (480 >> 8) & 255); mb_put(&file, 480 & 255);
    FILE *f = fopen(path, "wb"); int ok = 0;
    if (f) { ok = fwrite(file.b, 1, (size_t)file.n, f) == (size_t)file.n && fwrite(tracks.b, 1, (size_t)tracks.n, f) == (size_t)tracks.n; fclose(f); }
    if (ok) snprintf(g_mx_res, sizeof g_mx_res, "ok,%d,%ld", ntr - 1, (long)(file.n + tracks.n)); else snprintf(g_mx_res, sizeof g_mx_res, "error,cannot write file");
    free(file.b); free(tracks.b);
}

static void set_param(void *instance, const char *key, const char *val) {
    inst_t *s = (inst_t *)instance;
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
        s->rec_bars = bars; s->rec_src = src; s->rec_n = 0; s->rec_state = 1; s->rfx = 0; s->rnorm = 0;      /* a resample never takes over the effect of an earlier MIC take */
        return;
    }
    if (strcmp(key, "reccancel") == 0) { s->rec_state = 0; return; }
    if (strcmp(key, "micgain") == 0) { int gx = atoi(val); if (gx < 10) gx = 10; if (gx > 80) gx = 80; if (s->rec_state != 2) s->mic_gain = (float)gx * 0.1f; return; }
    if (strcmp(key, "micmon") == 0) { s->mic_mon = atoi(val) ? 1 : 0; s->mic_peak = 0.0f; s->mic_clip = 0; return; }
    if (strcmp(key, "micarm") == 0) {                /* "seconds,threshold %,gain x10,source,effect,amount %,normalize": source -2 = the Move's input, -1 = master, 0..15 = one track; threshold 0 = start at once, else wait for sound (input only) */
        if (s->rec_state == 1 || s->rec_state == 2 || s->rec_state == 5) return;
        int sec = 5, thr = 0, gx = 10, src = -2, fx = 0, am = 50, nm = 1;
        sscanf(val, "%d,%d,%d,%d,%d,%d,%d", &sec, &thr, &gx, &src, &fx, &am, &nm);
        if (sec < 1) sec = 1; if (sec > 30) sec = 30; if (thr < 0) thr = 0; if (thr > 90) thr = 90; if (gx < 10) gx = 10; if (gx > 80) gx = 80;
        if (src < -2 || src >= NTRK) src = -2; if (fx < 0 || fx > 5) fx = 0; if (am < 0) am = 0; if (am > 100) am = 100;
        s->rec_max = (long)(s->sr * 40.0f);
        if (!s->rec_buf) s->rec_buf = (int16_t *)malloc((size_t)s->rec_max * 4);
        if (!s->rec_buf) return;
        s->rec_src = src; s->rec_len = (long)(s->sr * (float)sec); if (s->rec_len > s->rec_max) s->rec_len = s->rec_max;
        s->rec_in_n = s->rec_len;
        if (fx >= 4) {                                  /* DELAY / REVERB: set up the small buffers once, and add a tail after the input so the echoes and the room can ring out */
            float sc = s->sr / 44100.0f; static const int bl[6] = { 1116, 1188, 1277, 1356, 556, 441 };
            int tot = 0; s->rfx_dln = (int)(s->sr * 0.30f); tot = 2 * s->rfx_dln;
            for (int i = 0; i < 12; i++) { s->rfx_len[i] = (int)(bl[i % 6] * sc) + (i >= 6 ? (int)(23 * sc) : 0); s->rfx_off[i] = tot; tot += s->rfx_len[i]; s->rfx_cp[i] = 0; }
            if (!s->rfx_mem || s->rfx_cap < tot) { free(s->rfx_mem); s->rfx_mem = (float *)calloc((size_t)tot, sizeof(float)); s->rfx_cap = s->rfx_mem ? tot : 0; }
            if (!s->rfx_mem) fx = 0; else { memset(s->rfx_mem, 0, (size_t)tot * sizeof(float)); s->rfx_dp = 0; for (int i = 0; i < 8; i++) s->rfx_lp[i] = 0.0f;
                long tail = (long)(s->sr * (fx == 4 ? 1.5f : 2.5f)); s->rec_len += tail; if (s->rec_len > s->rec_max) s->rec_len = s->rec_max; }
        }
        s->rec_n = 0; s->mic_gain = (float)gx * 0.1f; s->mic_thr = (float)thr * 0.01f; s->mic_clip = 0; s->rec_slot = -1;
        s->rfx = fx; s->ramt = (float)am * 0.01f; s->rnorm = nm ? 1 : 0; s->rfx_cnt = 0;
        s->rfx_hx[0] = s->rfx_hx[1] = s->rfx_hy[0] = s->rfx_hy[1] = s->rfx_hold[0] = s->rfx_hold[1] = 0.0f;
        s->rec_state = (src == -2 && thr > 0) ? 5 : 2;
        return;
    }
    if (strcmp(key, "micstop") == 0) {               /* stop a running take early: what was recorded so far is kept */
        if (s->rec_state == 2 && s->rec_n >= 64) { s->rec_len = s->rec_n; s->rec_state = 3; } else if (s->rec_state == 2 || s->rec_state == 5) s->rec_state = 0;
        return;
    }
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
        if (s->rec_src == -2) {                        /* mic take: the Move's built-in mic is mono (one side silent): use the louder side for both; short fades hide clicks */
            double eL = 0, eR = 0; for (long i = 0; i < s->rec_n; i++) { double a = s->rec_buf[2 * i], b = s->rec_buf[2 * i + 1]; eL += a * a; eR += b * b; }
            if (eL > 0 || eR > 0) { if (eR < eL * 0.02) { for (long i = 0; i < s->rec_n; i++) s->rec_buf[2 * i + 1] = s->rec_buf[2 * i]; } else if (eL < eR * 0.02) { for (long i = 0; i < s->rec_n; i++) s->rec_buf[2 * i] = s->rec_buf[2 * i + 1]; } }
            long fd = s->rec_n < 256 ? s->rec_n / 2 : 128;
            for (long i = 0; i < fd; i++) { float f = (float)i / (float)fd; s->rec_buf[2 * i] = (int16_t)(s->rec_buf[2 * i] * f); s->rec_buf[2 * i + 1] = (int16_t)(s->rec_buf[2 * i + 1] * f); long j = s->rec_n - 1 - i; s->rec_buf[2 * j] = (int16_t)(s->rec_buf[2 * j] * f); s->rec_buf[2 * j + 1] = (int16_t)(s->rec_buf[2 * j + 1] * f); }
        }
        if (s->rnorm) {                                 /* normalise: louder takes stay, quiet ones are brought up to about -1 dB (at most 24 dB, so silence is not turned into noise) */
            int pk = 0; for (long i = 0; i < 2 * s->rec_n; i++) { int a = s->rec_buf[i] < 0 ? -(int)s->rec_buf[i] : (int)s->rec_buf[i]; if (a > pk) pk = a; }
            if (pk > 64) { float g = 29000.0f / (float)pk; if (g > 16.0f) g = 16.0f; if (g > 1.02f) for (long i = 0; i < 2 * s->rec_n; i++) { int v = (int)(s->rec_buf[i] * g); s->rec_buf[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v)); } }
        }
        for (k = 1; k < 1000; k++) { snprintf(name, sizeof name, s->rec_src == -2 ? "mic_%03d.wav" : "resample_%03d.wav", k); snprintf(path, sizeof path, "%s/%s", SMP_DIR, name); FILE *t = fopen(path, "rb"); if (!t) break; fclose(t); }
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
    if (strcmp(key, "lfx") == 0) { int r = atoi(val); s->lf_req = r < 0 ? 0 : (r > 31 ? 31 : r); return; }
    if (strcmp(key, "reprate") == 0) { int r = atoi(val); s->rep_idx = r < 0 ? 0 : (r > 15 ? 15 : r); return; }
    if (strcmp(key, "laysyn") == 0) { int v = atoi(val); s->lay_s = v < 0 ? 0 : (v > 5 ? 5 : v); return; }
    if (strcmp(key, "padoct") == 0) { int o = atoi(val); s->pad_oct = o < -36 ? -36 : (o > 36 ? 36 : o); return; }
    if (strcmp(key, "padsoff") == 0) { s->pads_off = atoi(val); if (s->pads_off == 1) rep_clear(s); return; }
    if (strcmp(key, "npad") == 0) {                /* "k,vel,rep" from the UI */
        int k = atoi(val); const char *c = strchr(val, ','); int vel = c ? atoi(c + 1) : 100; const char *c2 = c ? strchr(c + 1, ',') : NULL;
        sel_pad(s, k, vel, c2 ? atoi(c2 + 1) : 0);
        return;
    }
    if (strcmp(key, "panic") == 0) { for (int i = 0; i < NLANE + NGH; i++) { s->vc[i].active = 0; s->vc[i].gate = 0; } for (int t = 0; t < NLANE; t++) { s->ar[t].on = 0; s->ar[t].key = -1; } rep_clear(s); s->lf_req = 0; return; }
    if (strcmp(key, "play") == 0) {
        if (val[0] == '2') { if (!s->playing && s->ci_left == 0) { s->ci_b = (int)(s->sr * 60.0f / s->bpm); if (s->ci_b < 1) s->ci_b = 1; s->ci_t = 0; s->ci_left = 4 * s->ci_b; } return; }
        s->ci_left = 0;
        if (val[0] == '1') seq_start(s); else seq_stop(s);
        return;
    }
    if (strcmp(key, "fol") == 0) {                 /* "track,clip,value" follow action of one clip */
        int t = atoi(val); const char *c = strchr(val, ','); const char *c2 = c ? strchr(c + 1, ',') : NULL;
        if (!c || !c2 || t < 0 || t >= NTRK) return;
        int cl = atoi(c + 1), v = atoi(c2 + 1); if (cl < 0 || cl >= NPAT || v < 0 || v > 63) return;
        s->fol[t][cl] = (uint8_t)v; return;
    }
    if (strcmp(key, "midiexp") == 0) { midi_export(s, val); return; }
    if (strcmp(key, "bpm") == 0)  { s->bpm = clampf((float)atof(val), 40.0f, 240.0f); return; }
    if (strcmp(key, "swing") == 0){ s->swing = clampf((float)atof(val), 0.5f, 0.75f); return; }
    if (strcmp(key, "pat") == 0)  { int n = atoi(val); if (n >= 0 && n < NPAT) s->cur_pat = n; return; }
    if (strcmp(key, "clip") == 0) {                /* "track,clip" clip -1 = stop; quantised to the bar */
        int t = atoi(val); const char *c = strchr(val, ','); if (!c || t < 0 || t >= NTRK) return;
        int cl = atoi(c + 1); if (cl < -1 || cl >= NPAT) return;
        s->pend_on = 0;
        if (s->playing) s->qclip[t] = cl; else { clip_activate(s, t, cl); s->qclip[t] = -2; }
        return;
    }
    if (strcmp(key, "sceneq") == 0) {              /* "scene,mode": 0 global, 1 now, 2 beat, 3 bar, 4 four bars */
        int c = atoi(val); const char *m = strchr(val, ','); if (!m || c < 0 || c >= NPAT) return;
        int md = atoi(m + 1); if (md < 0) md = 0; if (md > 4) md = 4; s->scq[c] = md; return;
    }
    if (strcmp(key, "scene") == 0) {               /* launch clip n on every track that has one, stop the rest */
        int cl = atoi(val); if (cl < 0 || cl >= NPAT) return;
        { static const int SQM[5] = { 0, 0, 3, 15, 63 }; s->pend_on = (s->playing && s->scq[cl] > 0); s->pend_qm = SQM[s->scq[cl]]; }
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
    if (strcmp(key, "sel") == 0)  { int n = atoi(val); if (n >= 0 && n < NLANE) { if (n != s->sel_track) rep_clear(s); s->sel_track = n; } return; }
    if (strcmp(key, "len") == 0)  {                /* "track,length" */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NTRK) { int l = atoi(c + 1); if (l >= 1 && l <= MAXSTEP) s->seqlen[t] = l; }
        return;
    }
    if (strcmp(key, "tres") == 0) {                /* "track,ticks per step" (6 = 1/16, 3 = 1/32, 4 = 1/16T, 8 = 1/8T, 12 = 1/8) */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NTRK) { int r = atoi(c + 1); if (r == 3 || r == 4 || r == 6 || r == 8 || r == 12) s->tres[t] = r; }
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
    if (strcmp(key, "tph") == 0) {                 /* "track,global step": the clip's own step 0 */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NTRK) { int g = atoi(c + 1); s->tph[t] = g < 0 ? 0 : g; }
        return;
    }
    if (strcmp(key, "rskip") == 0) {               /* "lane,step": skip that one step once (it was just played live by the pad) */
        int l = atoi(val); const char *c = strchr(val, ',');
        if (c && l >= 0 && l < NLANE) s->rskip[l] = atoi(c + 1) + 1;
        return;
    }
    if (strcmp(key, "rechold") == 0) {             /* "track,0|1" */
        int t = atoi(val); const char *c = strchr(val, ',');
        if (c && t >= 0 && t < NTRK) s->rechold[t] = atoi(c + 1) ? 1 : 0;
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
    if (strcmp(key, "tspd") == 0) {                /* "track,num,den": the track's speed as a fraction (1,1 = normal) */
        int t = atoi(val); const char *c1 = strchr(val, ','); const char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
        if (c2 && t >= 0 && t < NTRK) { int n = atoi(c1 + 1), d = atoi(c2 + 1); if (n >= 1 && d >= 1 && n <= 16 && d <= 16 && n * 4 <= d * 12 && d <= 8 * n) { s->tsn[t] = n; s->tsd[t] = d; } }
        return;
    }
    if (strcmp(key, "plkt") == 0) {                /* "lane,paramIndex": which lane parameter the step locks drive (0 = none) */
        int l = atoi(val); const char *c = strchr(val, ','); if (!c || l < 0 || l >= NLANE) return;
        int k = atoi(c + 1);
        if (s->plk_t[l] > 0) { s->pp[l][s->plk_t[l]] = s->plk_b[l]; update_pad(s, l); }       /* put the old target back to its base value */
        if (k < 1 || k >= PP_N || k == PP_TYPE || k == PP_ARP || k == PP_RATE || k == PP_WAVE || k == PP_POLY || k == PP_DTIME || k == PP_XFX || k == PP_MFX || k == PP_X2FX || k == PP_M2FX || k == PP_DMODE || k == PP_RMODE || k == PP_MONO || k == PP_DLN || k == PP_DMD || k == PP_TKEY || k == PP_TSCL) k = 0;
        s->plk_t[l] = (uint8_t)k; if (k) s->plk_b[l] = s->pp[l][k];
        return;
    }
    if (strcmp(key, "reset") == 0) {               /* "NN" 1-based */
        int n = atoi(val);
        if (n >= 1 && n <= NLANE) reset_pad(s, n - 1);
        return;
    }
    if (strncmp(key, "txt", 3) == 0 && key[3] >= '0' && key[3] <= '9') {    /* the text of the SPEECH engine for lane N */
        const int l = atoi(key + 3);
        if (l >= 0 && l < NLANE) { int k = 0; for (const char *c = val; *c && k < 40; c++) { char ch = *c; if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32); if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '.' || ch == ',' || ch == '?' || ch == '!' || ch == '-') s->spk_txt[l][k++] = ch; } while (k > 0 && s->spk_txt[l][k - 1] == ' ') k--; s->spk_txt[l][k] = 0; s->spk_wi[l] = 0; }
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
            else if (k == PP_XFX || k == PP_MFX || k == PP_X2FX || k == PP_M2FX) v = clampf(v, 0.0f, (k == PP_XFX || k == PP_X2FX) ? 13.0f : 11.0f);
            else if (k == PP_DMODE || k == PP_RMODE || k == PP_MONO || k == PP_DMD) v = clampf(v, 0.0f, 2.0f);
            else if (k == PP_DLN) v = clampf(v, 0.0f, 6.0f);
            else if (k == PP_TKEY) v = clampf(v, 0.0f, 12.0f);
            else if (k == PP_TSCL) v = clampf(v, 0.0f, (float)NSC);
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
            else if (k == PP_XFX || k == PP_MFX || k == PP_X2FX || k == PP_M2FX) v = clampf(v, 0.0f, (k == PP_XFX || k == PP_X2FX) ? 13.0f : 11.0f);
            else if (k == PP_DMODE || k == PP_RMODE || k == PP_MONO || k == PP_DMD) v = clampf(v, 0.0f, 2.0f);
            else if (k == PP_DLN) v = clampf(v, 0.0f, 6.0f);
            else if (k == PP_TKEY) v = clampf(v, 0.0f, 12.0f);
            else if (k == PP_TSCL) v = clampf(v, 0.0f, (float)NSC);
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
        else if (k == G_SCALE) { int e = enum_index(val, SCALE_NAME, NSC);  if (e < 0) return; v = (float)e; }
        else if (k == G_DLYT)  { int e = enum_index(val, DT_NAME, 5);   if (e < 0) return; v = (float)e; }
        else if (k == G_DMODE) { int e = enum_index(val, DMODE_NAME, 3); if (e < 0) return; v = (float)e; }
        else if (k == G_RMODE) { int e = enum_index(val, RMODE_NAME, 3); if (e < 0) return; v = (float)e; }
        else if (k == G_GRV)   { int e = enum_index(val, GRV_NAME, 11); if (e < 0) return; v = (float)e; }
        else v = clampf((float)atof(val), 0.0f, 1.0f);
        s->g[k] = v;
        update_global(s);
        if (k == G_KEY) for (int i = 0; i < NLANE; i++) update_pad(s, i);
        return;
    }
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
        int gsr = s->gs;
        if (s->playing && (s->gs > 0 || s->sub > 0)) {              /* old meaning: steps started so far; the tick in progress adds to the fraction */
            const long done = (long)s->gs * 6 + s->sub - 1; gsr = (int)(done / 6) + 1;
            fr = (int)(((done % 6) * 100 + fr) / 6);
        }
        return snprintf(buf, (size_t)buf_len, "%d,%d,%d,%d,%d", s->playing, gsr, s->cur_pat, fr, s->ci_left > 0 ? (s->ci_left + s->ci_b - 1) / s->ci_b : 0);
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
    if (strncmp(key, "smpz_", 5) == 0) {                /* zoomed waveform: "smpz_<sample>_<from permille>_<to permille>", 64 columns like smpw_ but only for that part of the sample */
        int i = 0, p0 = 0, p1 = 1000; if (sscanf(key + 5, "%d_%d_%d", &i, &p0, &p1) < 3) return -1;
        const smp_t *m = smp_get(i); if (!m || buf_len < 66) return -1;
        if (p0 < 0) p0 = 0; if (p1 > 1000) p1 = 1000; if (p1 <= p0) p1 = p0 + 1;
        long long b0 = (long long)m->n * p0 / 1000, bn = (long long)m->n * (p1 - p0) / 1000;
        for (int c = 0; c < 64; c++) {
            long long a0 = b0 + bn * c / 64, a1 = b0 + bn * (c + 1) / 64, mx = 0; if (a1 <= a0) a1 = a0 + 1; if (a1 > m->n) a1 = m->n;
            long long stp = (a1 - a0) / 64 + 1;
            for (long long k = a0; k < a1; k += stp) { int v = m->d[k * m->ch]; if (v < 0) v = -v; if (v > mx) mx = v; }
            int q = (int)(sqrtf((float)mx / 32768.0f) * 25.0f + 0.5f); if (q > 25) q = 25;
            buf[c] = (char)('a' + q);
        }
        buf[64] = 0; return 64;
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
    if (strcmp(key, "micpeak") == 0) {              /* "level 0..100, clipped, input found" */
        int v = (int)(s->mic_peak * 100.0f + 0.5f); if (v > 100) v = 100;
        return snprintf(buf, (size_t)buf_len, "%d,%d,%d", v, s->mic_clip, s->mic_ok);
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
    if (strcmp(key, "midiexp") == 0) { snprintf(buf, (size_t)buf_len, "%s", g_mx_res); return (int)strlen(buf); }
    if (strcmp(key, "scope") == 0) {                 /* 64 wave points (2 hex digits each, triggered on a rising zero crossing) + 8 band levels (2 hex digits each) */
        char tmp[420]; int n = 0, st = 0, o = s->sc_i;
        for (int i = 0; i < 60; i++) { float a = s->sc_w[(o + i) & 127], b = s->sc_w[(o + i + 1) & 127]; if (a < 0.0f && b >= 0.0f) { st = i + 1; break; } }
        for (int i = 0; i < 64; i++) { float v = s->sc_w[(o + st + i) & 127]; int q = (int)((v * 0.5f + 0.5f) * 255.0f + 0.5f); if (q < 0) q = 0; if (q > 255) q = 255; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02x", q); }
        for (int b = 0; b < 8; b++) { int q = (int)(sqrtf(s->sc_env[b]) * 400.0f); if (q > 255) q = 255; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02x", q); }
        for (int b = 0; b < 16; b++) { int q = (int)(sqrtf(s->sc_env2[b]) * 400.0f); if (q > 255) q = 255; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02x", q); }
        for (int i = 0; i < 40; i++) { int j = (s->sc_si + 24 + i) & 63; int ql = (int)((s->sc_sl[j] * 0.5f + 0.5f) * 255.0f + 0.5f), qr = (int)((s->sc_sr[j] * 0.5f + 0.5f) * 255.0f + 0.5f); if (ql < 0) ql = 0; if (ql > 255) ql = 255; if (qr < 0) qr = 0; if (qr > 255) qr = 255; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02x%02x", ql, qr); }
        { float dn = sqrtf(s->sc_cll * s->sc_crr); float cr = dn > 1e-7f ? s->sc_clr / dn : 1.0f; int q = (int)((cr * 0.5f + 0.5f) * 255.0f + 0.5f); if (q < 0) q = 0; if (q > 255) q = 255; n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%02x", q); }
        if (buf_len <= n) return -1;
        memcpy(buf, tmp, (size_t)n + 1); return n;
    }
    if (strncmp(key, "grnv_", 5) == 0) {             /* granulator display of one sound: 32 history levels (0-9 a-f) | grains: back (0-99), size (0-99), pitch (0-9), age (0-99), pan (0-9) ; ... */
        const int ln = atoi(key + 5) - 1; char tmp[300]; int n = 0;
        if (ln < 0 || ln >= NLANE || !s->gr_slot[ln]) { buf[0] = 0; return 0; }
        const int sl = s->gr_slot[ln] - 1;
        for (int k = 0; k < 32; k++) {
            float pk = 0.0f; const int base = (s->gr[sl].w - (32 - k) * (GRN_SZ / 32) + GRN_SZ) & (GRN_SZ - 1);
            for (int j = 0; j < GRN_SZ / 32; j += 64) { const float v = fabsf(s->gr[sl].buf[0][(base + j) & (GRN_SZ - 1)]); if (v > pk) pk = v; }
            int lv = (int)(sqrtf(pk) * 15.0f); if (lv > 15) lv = 15; tmp[n++] = "0123456789abcdef"[lv];
        }
        tmp[n++] = '|';
        for (int g = 0; g < GRN_G && n < 240; g++) if (s->gr[sl].age[g] < s->gr[sl].len[g]) {
            int back = (s->gr[sl].w - (int)s->gr[sl].pos[g] + GRN_SZ) & (GRN_SZ - 1);
            n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%d,%d,%d,%d,%d;", back * 99 / GRN_SZ, s->gr[sl].len[g] * 99 / (int)(1.3f * s->sr), (int)(s->gr[sl].inc[g] * 4.0f), s->gr[sl].age[g] * 99 / s->gr[sl].len[g], (int)(s->gr[sl].pr[g] / (s->gr[sl].pl[g] + s->gr[sl].pr[g]) * 9.0f));
        }
        if (buf_len <= n) return -1;
        tmp[n] = 0; memcpy(buf, tmp, (size_t)n + 1); return n;
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

static void eq_coef(float *c, int band, float gdb, float sr, float fx, float qx) {     /* RBJ biquad: band 0 low shelf 100 Hz, 1 peak 1 kHz, 2 high shelf 8 kHz; c = b0 b1 b2 a1 a2 (normalised) */
    float f = band == 0 ? 100.0f * powf(3.0f, (fx - 0.5f) * 2.0f) : (band == 1 ? 1000.0f * powf(8.0f, (fx - 0.5f) * 2.0f) : 8000.0f * powf(2.0f, (fx - 0.5f) * 2.0f));
    if (f > sr * 0.45f) f = sr * 0.45f;
    const float A = powf(10.0f, gdb / 40.0f), w0 = 6.2831853f * f / sr, cw = cosf(w0), sw = sinf(w0);
    float b0, b1, b2, a0, a1, a2;
    if (band == 1) {
        const float al = sw / (2.0f * 0.9f * powf(4.0f, (qx - 0.5f) * 2.0f));
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
static void act_rebuild(inst_t *s) {
    int n = 0; for (int i = 0; i < NLANE + NGH; i++) if (s->vc[i].active) s->vl[n++] = (short)i; s->vl_n = n;
    n = 0; for (int i = 0; i < NLANE; i++) if (s->ar[i].on) s->al[n++] = (short)i; s->al_n = n;
    n = 0; for (int i = 0; i < NLANE; i++) if (s->rt_n[i] > 0 || s->gate_left[i] > 0) s->rl[n++] = (short)i; s->rl_n = n;
    s->act_dirty = 0;
}
static void render_block_impl(void *instance, int16_t *out_lr, int frames);
/* Effects that are only recorded INTO a take (they run while the take is recorded, never while playing): 1 LOW CUT, 2 DRIVE, 3 LO-FI. Cheap: a few operations per sample. */
static inline void rec_fx(inst_t *s, float *l, float *r) {
    int t = s->rfx; if (t <= 0) return;
    float a = s->ramt;
    if (t == 1) {                                   /* LOW CUT: one-pole high pass, 40 .. 300 Hz */
        float k = 1.0f / (1.0f + 6.2831853f * (40.0f + a * 260.0f) * s->inv_sr), xl = *l, xr = *r;
        s->rfx_hy[0] = k * (s->rfx_hy[0] + xl - s->rfx_hx[0]); s->rfx_hx[0] = xl;
        s->rfx_hy[1] = k * (s->rfx_hy[1] + xr - s->rfx_hx[1]); s->rfx_hx[1] = xr;
        *l = s->rfx_hy[0]; *r = s->rfx_hy[1];
    } else if (t == 2) {                            /* DRIVE: more gain into a soft clipper, level made up a little */
        float g = 1.0f + a * 7.0f, c = 1.0f / (1.0f + a * 0.6f);
        *l = softclip(*l * g) * c; *r = softclip(*r * g) * c;
    } else if (t == 3) {                            /* LO-FI: every n-th sample held (n 1..8), 12 .. 6 bits */
        if (s->rfx_cnt <= 0) {
            float lv = (float)(1 << (int)(12.0f - a * 6.0f));
            s->rfx_hold[0] = floorf(*l * lv + 0.5f) / lv; s->rfx_hold[1] = floorf(*r * lv + 0.5f) / lv;
            s->rfx_cnt = 1 + (int)(a * 7.0f + 0.5f);
        }
        s->rfx_cnt--; *l = s->rfx_hold[0]; *r = s->rfx_hold[1];
    } else if (t == 4) {                            /* DELAY: 0.3 s ping-pong echo, amount = echo level and repeats */
        int n = s->rfx_dln, p = s->rfx_dp; float *dl = s->rfx_mem, *dr = s->rfx_mem + n;
        float fb = 0.25f + a * 0.5f, wet = 0.25f + a * 0.5f, xl = *l, xr = *r, yl = dl[p], yr = dr[p];
        dl[p] = xl + yr * fb; dr[p] = xr + yl * fb;
        *l = xl + yl * wet; *r = xr + yr * wet;
        if (++s->rfx_dp >= n) s->rfx_dp = 0;
    } else if (t == 5) {                            /* REVERB: four damped combs and two allpass filters per side (small room to hall) */
        float x = (*l + *r) * 0.5f, fb = 0.72f + a * 0.22f, wet = 0.35f + a * 0.65f, out[2];
        for (int ch = 0; ch < 2; ch++) {
            float sum = 0.0f;
            for (int c = 0; c < 4; c++) {
                int k = ch * 6 + c; float *b = s->rfx_mem + s->rfx_off[k]; int ix = s->rfx_cp[k];
                float y = b[ix], lp = y * 0.7f + s->rfx_lp[ch * 4 + c] * 0.3f; s->rfx_lp[ch * 4 + c] = lp;
                b[ix] = x * 0.3f + lp * fb; sum += y;
                if (++s->rfx_cp[k] >= s->rfx_len[k]) s->rfx_cp[k] = 0;
            }
            for (int q = 4; q < 6; q++) {
                int k = ch * 6 + q; float *b = s->rfx_mem + s->rfx_off[k]; int ix = s->rfx_cp[k];
                float bo = b[ix], o = -sum + bo; b[ix] = sum + bo * 0.5f; sum = o;
                if (++s->rfx_cp[k] >= s->rfx_len[k]) s->rfx_cp[k] = 0;
            }
            out[ch] = sum;
        }
        *l += out[0] * wet; *r += out[1] * wet;
    }
}
/* MIC: reads the Move's audio input block (host mailbox, 128 frames, stereo int16). It does nothing at all (one test) unless the MIC page is open
   (level meter) or a take is running, and then it only measures and copies samples: no filter, no effect, no allocation, nothing is played back. */
static void mic_block(inst_t *s, int frames) {
    int taking = (s->rec_src == -2 && (s->rec_state == 2 || s->rec_state == 5));
    if (!s->mic_mon && !taking) return;
    const int16_t *in = (g_host && g_host->mapped_memory && g_host->audio_in_offset > 0) ? (const int16_t *)(g_host->mapped_memory + g_host->audio_in_offset) : NULL;
    if (!in) { s->mic_ok = 0; s->mic_peak = 0.0f; return; }
    s->mic_ok = 1;
    int n = frames < 128 ? frames : 128;
    int pk = 0;
    for (int i = 0; i < 2 * n; i++) { int a = in[i] < 0 ? -(int)in[i] : (int)in[i]; if (a > pk) pk = a; }
    float g = s->mic_gain > 0.05f ? s->mic_gain : 1.0f, pv = (float)pk * g * (1.0f / 32768.0f);
    s->mic_peak = pv > s->mic_peak ? pv : s->mic_peak * 0.96f;
    if (pv >= 0.97f) s->mic_clip = 1;
    if (s->rec_state == 5 && s->rec_src == -2 && pv >= s->mic_thr) s->rec_state = 2;          /* waiting for sound: the take starts with the first loud block */
    if (s->rec_state == 2 && s->rec_src == -2 && s->rec_buf) {
        for (int i = 0; i < n && s->rec_n < s->rec_len; i++) {
            float l = in[2 * i] * g, r = in[2 * i + 1] * g;
            if (s->rfx >= 4 && s->rec_n >= s->rec_in_n) { l = 0.0f; r = 0.0f; }          /* tail of a DELAY / REVERB take: no more input, only the effect rings out */
            if (s->rfx > 0) { l *= (1.0f / 32768.0f); r *= (1.0f / 32768.0f); rec_fx(s, &l, &r); l *= 32768.0f; r *= 32768.0f; }
            if (l > 32767.0f) l = 32767.0f; if (l < -32768.0f) l = -32768.0f; if (r > 32767.0f) r = 32767.0f; if (r < -32768.0f) r = -32768.0f;
            s->rec_buf[2 * s->rec_n] = (int16_t)l; s->rec_buf[2 * s->rec_n + 1] = (int16_t)r; s->rec_n++;
        }
        if (s->rec_n >= s->rec_len) s->rec_state = 3;
    }
}
static void render_block(void *instance, int16_t *out_lr, int frames) {
    inst_t *s = (inst_t *)instance;
    if (!s) { memset(out_lr, 0, (size_t)frames * 2 * sizeof(int16_t)); return; }
    mic_block(s, frames);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
#if defined(__aarch64__)
    uint64_t fpcr_old; __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr_old));
    __asm__ volatile("msr fpcr, %0" :: "r"(fpcr_old | (1ull << 24)));          /* flush denormals to zero: decaying filter / reverb / delay tails no longer cost extra CPU (a cause of crackling) */
#elif defined(__SSE__)
    unsigned int mx_old = _mm_getcsr(); _mm_setcsr(mx_old | 0x8040u);
#endif
    render_block_impl(instance, out_lr, frames);
#if defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" :: "r"(fpcr_old));
#elif defined(__SSE__)
    _mm_setcsr(mx_old);
#endif
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double el = (double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec);
    float load = (float)(el / ((double)frames / (double)s->sr * 1e9));
    s->cpu_avg += 0.05f * (load - s->cpu_avg);
    s->cpu_peak = load > s->cpu_peak ? load : s->cpu_peak * 0.9995f;
}

/* ---- the shared effects bus ---- */
static void bus_clear(inst_t *s) {
    memset(s->dlL, 0, sizeof s->dlL); memset(s->dlR, 0, sizeof s->dlR); memset(s->rv, 0, sizeof s->rv); memset(s->rv_z, 0, sizeof s->rv_z);
    memset(s->shb, 0, sizeof s->shb); memset(s->dt_buf, 0, sizeof s->dt_buf);
    s->dly_z[0] = s->dly_z[1] = s->dly_hp[0] = s->dly_hp[1] = 0.0f; s->rlc_z[0] = s->rlc_z[1] = 0.0f;
    s->dt_bw = s->dt_zl = s->dt_zr = 0.0f;
}
static inline float dt_rd(const inst_t *s, int b, int off) { return s->dt_buf[b][(s->dt_w - off) & DT_MASK]; }
static inline float dt_rdf(const inst_t *s, int b, float off) {
    float pos = (float)s->dt_w - off; int i = (int)floorf(pos); const float f = pos - (float)i;
    return s->dt_buf[b][i & DT_MASK] * (1.0f - f) + s->dt_buf[b][(i + 1) & DT_MASK] * f;
}
/* delay: 0 PING-PONG, 1 TAPE (wow + flutter, dark, saturated), 2 DUB (dotted right side, band-limited feedback); reverb: 0 HALL (Dattorro plate), 1 ECHO (4 separate lines), 2 SHIMMER (FDN + octave-up feedback) */
static void bus_fx(inst_t *s, float sd, float sr_in, int dlen, float *outL, float *outR) {
    const float *g = s->g;
    float oL = 0.0f, oR = 0.0f;
    {
        const float dfb = g[G_DLYFB] * 0.9f, dtn = g[G_DTN];
        const int dm = (int)(g[G_DMODE] + 0.5f);
        float yl, yr;
        if (dm == 0) {
            int rp = (s->dw - dlen) & DLY_MASK;
            yl = s->dlL[rp]; yr = s->dlR[rp];
            { const float ta = fminf(1.0f, s->a_dly * (0.2f + 1.6f * dtn));
              s->dly_z[0] += ta * (yl - s->dly_z[0]);
              s->dly_z[1] += ta * (yr - s->dly_z[1]); }
            s->dlL[s->dw] = sd + s->dly_z[1] * dfb;
            s->dlR[s->dw] = s->dly_z[0] * dfb;
        } else {
            float dl_l = (float)dlen, dl_r = (float)dlen;
            if (dm == 1) {
                s->dly_ph += 0.7f * s->inv_sr; if (s->dly_ph >= 1.0f) s->dly_ph -= 1.0f;
                const float wob = (0.0007f * sin2pi(s->dly_ph) + 0.00025f * sinf(TWO_PI * 7.3f * s->dly_ph)) * s->sr;
                dl_l += wob; dl_r += wob * 0.8f;
            } else dl_r *= 0.75f;
            if (dl_l < 8.0f) dl_l = 8.0f; if (dl_r < 8.0f) dl_r = 8.0f;
            float pl = (float)s->dw - dl_l, pr = (float)s->dw - dl_r;
            int il = (int)floorf(pl), ir = (int)floorf(pr); const float fl = pl - (float)il, fr_ = pr - (float)ir;
            yl = s->dlL[il & DLY_MASK] * (1.0f - fl) + s->dlL[(il + 1) & DLY_MASK] * fl;
            yr = s->dlR[ir & DLY_MASK] * (1.0f - fr_) + s->dlR[(ir + 1) & DLY_MASK] * fr_;
            const float aa = fminf(1.0f, (dm == 1 ? s->a_dly * 0.45f : s->a_dly * 0.8f) * (0.2f + 1.6f * dtn));
            s->dly_z[0] += aa * (yl - s->dly_z[0]);
            s->dly_z[1] += aa * (yr - s->dly_z[1]);
            float fa = s->dly_z[0], fb = s->dly_z[1];
            if (dm == 2) {
                s->dly_hp[0] += 0.02f * (fa - s->dly_hp[0]); fa -= s->dly_hp[0];
                s->dly_hp[1] += 0.02f * (fb - s->dly_hp[1]); fb -= s->dly_hp[1];
            }
            fa = fast_tanh(fa * 1.4f) * 0.714f; fb = fast_tanh(fb * 1.4f) * 0.714f;
            s->dlL[s->dw] = sd + fb * dfb;
            s->dlR[s->dw] = fa * dfb;
        }
        s->dw = (s->dw + 1) & DLY_MASK;
        { const float wd = g[G_DWD] * 2.0f, md = 0.5f * (yl + yr), sdd = 0.5f * (yl - yr); yl = md + sdd * wd; yr = md - sdd * wd; }
        const float dret = g[G_DLYRET] * 1.2f;
        oL += yl * dret; oR += yr * dret;
    }
    {
        const int rm = (int)(g[G_RMODE] + 0.5f);
        const float rret = g[G_RVRET] * 1.6f;
        float rl, rr2;
        if (rm == 0) {
            const int *Ln = s->dt_len;
            float x = s->dt_bw += 0.75f * (sr_in * 0.5f - s->dt_bw);
            for (int i = 0; i < 4; i++) {
                const float gg = i < 2 ? 0.75f : 0.625f;
                const float v = dt_rd(s, i, Ln[i]), u = x - gg * v;
                s->dt_buf[i][s->dt_w] = u; x = v + gg * u;
            }
            const float fbL = dt_rd(s, 11, Ln[11]), fbR = dt_rd(s, 7, Ln[7]);
            s->dt_lfo += 0.8f * s->inv_sr; if (s->dt_lfo >= 1.0f) s->dt_lfo -= 1.0f;
            const float md = 7.0f * s->sr * (1.0f / 29761.0f);
            const float dec = s->dt_dec, g2 = fminf(0.5f, fmaxf(0.25f, dec + 0.15f));
            { /* left half */
                const float v = dt_rdf(s, 4, (float)Ln[4] + md * fsin01(s->dt_lfo)), u = (x + fbL) - 0.7f * v;
                s->dt_buf[4][s->dt_w] = u; const float y = v + 0.7f * u;
                const float d1 = dt_rd(s, 5, Ln[5]); s->dt_buf[5][s->dt_w] = y;
                s->dt_zl += s->rv_a * (d1 - s->dt_zl);
                const float z = s->dt_zl * dec, v2 = dt_rd(s, 6, Ln[6]), u2 = z - g2 * v2;
                s->dt_buf[6][s->dt_w] = u2; s->dt_buf[7][s->dt_w] = v2 + g2 * u2 + 1e-20f;
            }
            { /* right half */
                const float v = dt_rdf(s, 8, (float)Ln[8] + md * fsin01(s->dt_lfo + 0.25f >= 1.0f ? s->dt_lfo - 0.75f : s->dt_lfo + 0.25f)), u = (x + fbR) - 0.7f * v;
                s->dt_buf[8][s->dt_w] = u; const float y = v + 0.7f * u;
                const float d1 = dt_rd(s, 9, Ln[9]); s->dt_buf[9][s->dt_w] = y;
                s->dt_zr += s->rv_a * (d1 - s->dt_zr);
                const float z = s->dt_zr * dec, v2 = dt_rd(s, 10, Ln[10]), u2 = z - g2 * v2;
                s->dt_buf[10][s->dt_w] = u2; s->dt_buf[11][s->dt_w] = v2 + g2 * u2 - 1e-20f;
            }
            rl = dt_rd(s, 9, Ln[12]) + dt_rd(s, 9, Ln[13]) - dt_rd(s, 10, Ln[14]) + dt_rd(s, 11, Ln[15]) - dt_rd(s, 5, Ln[16]) - dt_rd(s, 6, Ln[17]) - dt_rd(s, 7, Ln[18]);
            rr2 = dt_rd(s, 5, Ln[19]) + dt_rd(s, 5, Ln[20]) - dt_rd(s, 6, Ln[21]) + dt_rd(s, 7, Ln[22]) - dt_rd(s, 9, Ln[23]) - dt_rd(s, 10, Ln[24]) - dt_rd(s, 11, Ln[25]);
            s->dt_w = (s->dt_w + 1) & DT_MASK;
            if (!(fabsf(rl) < 16.0f) || !(fabsf(rr2) < 16.0f)) { memset(s->dt_buf, 0, sizeof s->dt_buf); s->dt_zl = s->dt_zr = s->dt_bw = 0.0f; rl = rr2 = 0.0f; }
            rl *= 0.6f * rret; rr2 *= 0.6f * rret;
        } else {
            float o[NLINES];
            for (int i = 0; i < NLINES; i++) {
                int rr = (s->rw - s->rv_len[i]) & RV_MASK;
                float y = s->rv[i][rr];
                s->rv_z[i] += s->rv_a * (y - s->rv_z[i]);
                o[i] = s->rv_z[i] * s->rv_g[i];
            }
            float in = sr_in, shv = 0.0f;
            if (rm == 2) {
                const float m = 0.25f * (o[0] + o[1] + o[2] + o[3]);
                s->shb[s->shw] = m;
                s->sh_ph += 1.0f / 1024.0f; if (s->sh_ph >= 1.0f) s->sh_ph -= 1.0f;
                for (int k = 0; k < 2; k++) {
                    float ph = s->sh_ph + 0.5f * (float)k; if (ph >= 1.0f) ph -= 1.0f;
                    const float d = 8.0f + 1024.0f * (1.0f - ph), w = sinf(3.14159265f * ph);
                    float pp_ = (float)s->shw - d; if (pp_ < 0.0f) pp_ += (float)SH_SIZE;
                    const int i0 = (int)pp_ & (SH_SIZE - 1), i1 = (i0 + 1) & (SH_SIZE - 1); const float f = pp_ - floorf(pp_);
                    shv += (s->shb[i0] * (1.0f - f) + s->shb[i1] * f) * w * w;
                }
                s->shw = (s->shw + 1) & (SH_SIZE - 1);
                if (!(fabsf(shv) < 8.0f)) { shv = 0.0f; memset(s->shb, 0, sizeof s->shb); }
                in += fast_tanh(shv) * 0.14f;
            }
            if (rm == 1) {
                s->rv[0][s->rw] = o[0] * 0.92f + in + 1e-20f;
                s->rv[1][s->rw] = o[1] * 0.92f + in * 0.8f - 1e-20f;
                s->rv[2][s->rw] = o[2] * 0.92f + in * 0.6f + 1e-20f;
                s->rv[3][s->rw] = o[3] * 0.92f + in * 0.4f - 1e-20f;
            } else {
                float h0 = 0.5f * (o[0] + o[1] + o[2] + o[3]);
                float h1 = 0.5f * (o[0] - o[1] + o[2] - o[3]);
                float h2 = 0.5f * (o[0] + o[1] - o[2] - o[3]);
                float h3 = 0.5f * (o[0] - o[1] - o[2] + o[3]);
                s->rv[0][s->rw] = h0 + in + 1e-20f;
                s->rv[1][s->rw] = h1 + in - 1e-20f;
                s->rv[2][s->rw] = h2 + in + 1e-20f;
                s->rv[3][s->rw] = h3 + in - 1e-20f;
            }
            s->rw = (s->rw + 1) & RV_MASK;
            rl = (o[0] - o[2]) * 0.7f * rret + shv * 0.35f * rret; rr2 = (o[1] - o[3]) * 0.7f * rret + shv * 0.35f * rret;
        }
        { const float wd = g[G_RWD] * 2.0f, md = 0.5f * (rl + rr2), sdd = 0.5f * (rl - rr2); rl = md + sdd * wd; rr2 = md - sdd * wd; }
        if (g[G_RLC] > 0.01f) { const float ca = 0.002f + g[G_RLC] * g[G_RLC] * 0.12f; s->rlc_z[0] += ca * (rl - s->rlc_z[0]); s->rlc_z[1] += ca * (rr2 - s->rlc_z[1]); rl -= s->rlc_z[0]; rr2 -= s->rlc_z[1]; }
        oL += rl; oR += rr2;
    }
    *outL = oL; *outR = oR;
}
static void render_block_impl(void *instance, int16_t *out_lr, int frames) {
    inst_t *s = (inst_t *)instance;

    /* shared delay time for this block */
    int dlen_g;
    { int di = (int)(s->g[G_DLYT] + 0.5f); if (di < 0) di = 0; if (di > 4) di = 4;
      dlen_g = (int)(DT_BEATS[di] * 60.0f / s->bpm * s->sr);
      if (dlen_g < 64) dlen_g = 64; if (dlen_g > DLY_SIZE - 2) dlen_g = DLY_SIZE - 2; }
    const float mvol = s->g[G_VOL] * s->g[G_VOL] / 0.64f * 0.6f;
#define RM_GAIN 1.9f
    float rm_fb = 0.8f; const float rm_alp = 1.0f - expf(-6.2831853f * 130.0f * s->inv_sr);
    { float sw = 0.0f, st2 = 0.0f; for (int i = 0; i < NLANE; i++) { const float w = s->pp[i][PP_RMB]; if (w > 0.001f) { sw += w; st2 += w * s->pp[i][PP_RMT]; } }
      if (sw > 0.0f) rm_fb = 0.78f + 0.17f * (st2 / sw); }

    for (int n = 0; n < frames; n++) {
        s->clock++;
        float L = 0.0f, R = 0.0f;
        float rmb_in = 0.0f, rmd_in = 0.0f;
        float *const tl = s->bt_l, *const tr = s->bt_r, *const tsd = s->bt_sd, *const tsr = s->bt_sr; int *const tact = s->bt_act;      /* one bus per sound lane: a drum sound has its own effects, a synth track one set (kept at zero between samples, only the busy ones are cleared again) */
        uint64_t tmk[2] = { 0, 0 };
        if (s->act_dirty || (n & 15) == 0) act_rebuild(s);

        /* count-in: one bar of clicks, then the sequencer starts */
        if (s->ci_left > 0) {
            if (s->ci_b > 0 && s->ci_t % s->ci_b == 0) { s->met_amp = s->g[G_MET] > 0.6f ? s->g[G_MET] : 0.6f; s->met_env = 1.0f; s->met_ph = 0.0f; s->met_f = (s->ci_t == 0) ? 1700.0f : 1100.0f; }
            s->ci_t++;
            if (--s->ci_left == 0) seq_start(s);
        }

        /* sequencer */
        if (s->playing) {
            if (s->tsamp >= s->next_time) seq_step(s);
            s->tsamp += 1.0;
            if (s->act_dirty) act_rebuild(s);
            for (int q_ = 0; q_ < s->rl_n; q_++) {
                const int t = s->rl[q_];
                if (s->rt_n[t] > 0 && (s->rt_cnt[t] -= 1.0f) <= 0.0f) rt_fire(s, t);
                if (s->gate_left[t] > 0 && --s->gate_left[t] == 0) s->vc[t].gate = 0;
            }
        }

        if (s->playing) {
            if (s->mo_clk) { s->mclk += s->bpm * 0.4f * s->inv_sr; while (s->mclk >= 1.0f) { s->mclk -= 1.0f; mo_send(0xF, 0xF8, 0, 0); } }
            if (s->mo_trk) for (int i = 0; i < 64; i++) if (s->mo_q[i].cnt > 0 && --s->mo_q[i].cnt == 0) mo_send(0x8, s->mo_q[i].st, s->mo_q[i].d1, 0);
        }
        /* arps */
        if (s->act_dirty) act_rebuild(s);
        for (int q_ = 0; q_ < s->al_n; q_++) {
            const int p = s->al[q_];
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
        for (int ri = 0; ri < REP_N; ri++) if (s->rp_on[ri]) {
            s->rp_cnt[ri] -= 1.0f;
            if (s->rp_cnt[ri] <= 0.0f) {
                trig(s, s->rp_t[ri], s->rp_vel[ri], (int)s->rp_semi[ri], 1); s->rp_cnt[ri] += rep_step_samples_r(s, s->rp_rate[ri]);
                if (s->rp_ramp[ri] == 1 || s->rp_ramp[ri] == 2) { s->rp_vel[ri] *= s->rp_ramp[ri] == 1 ? 1.05f : 1.12f; if (s->rp_vel[ri] > s->rp_cap[ri]) s->rp_vel[ri] = s->rp_cap[ri]; }
                else if (s->rp_ramp[ri] == 3) { s->rp_vel[ri] *= 0.93f; if (s->rp_vel[ri] < 0.3f * s->rp_cap[ri]) s->rp_vel[ri] = 0.3f * s->rp_cap[ri]; }
            }
        }

        {
            float inc = s->bpm * (1.0f / 60.0f) * s->inv_sr;
            s->pump_ph += inc; if (s->pump_ph >= 1.0f) s->pump_ph -= 1.0f;
            s->wob_ph += inc * 0.5f; if (s->wob_ph >= 1.0f) s->wob_ph -= 1.0f;
        }
        /* voices */
        if (s->act_dirty) act_rebuild(s);
        for (int vq_ = 0; vq_ < s->vl_n; vq_++) {
            const int vi = s->vl[vq_];
            vc_t *v = &s->vc[vi];
            if (!v->active) continue;
            const int p = vi < NLANE ? vi : s->vown[vi];
            if (vi >= NLANE && v->fol && (v->fgen != s->tgen[p] || !s->vc[p].gate)) { v->gate = 0; v->fol = 0; }   /* MIDI-effect layer ends with its note */
            const int gi = lane_trk(s, p);
            tact[p] = 1; tmk[p >> 6] |= 1ull << (p & 63);
            float x = vc_render(s, v, p);
            if (v->type != T_SAMPLE && BASE_T[v->type] != T_E_SPK) {      /* tail trim: no sound rings on for ever. Cap = 2 s x decay factor (Decay 50 % = 2 s, 0 = 0.7 s, 100 % = 6 s), then a quick fade */
                const int sus = TY[v->type].sus;
                if (!v->gate) v->trel += s->inv_sr;
                const float tt = sus ? (v->gate ? 0.0f : v->trel) : v->t, cap = (sus ? 1.2f : 2.0f) * s->dm[p];
                if (tt > cap) { const float lim = 1.0f - (tt - cap) * (1.0f / 0.35f); if (lim <= 0.0f) { v->active = 0; continue; } x *= lim; }
            }
            if (v->xfd > 0.0f) { x *= v->xfd; v->xfd -= s->inv_sr * (1.0f / 0.010f); if (v->xfd <= 0.0f) { v->active = 0; continue; } }
            const ch_t *ch = &CHR[v->type];
            {
                const float at = s->a_tone[p] * ch->tm < 1.0f ? s->a_tone[p] * ch->tm : 1.0f;
                if (at < 1.0f) {
                    v->l1 += at * (x - v->l1);
                    v->l2 += at * (v->l1 - v->l2);
                    x = v->l2;
                }
            }
            {                                                              /* sound design 2: noise layer that follows the sound, wave folder */
                const float *q2 = s->pp[p];
                if (q2[PP_SNZ] > 0.01f) {
                    v->nze += 0.002f * (fabsf(x) - v->nze);
                    s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
                    x += ((float)(s->rng & 0xFFFF) * (1.0f / 32768.0f) - 1.0f) * q2[PP_SNZ] * 2.0f * v->nze;
                }
                if (q2[PP_SFLD] > 0.01f) { const float f = q2[PP_SFLD]; x = (1.0f - f) * x + f * sinf(x * (1.0f + f * 6.0f) * 1.5708f); }
            }
            {
                float cu = s->pp[p][PP_CUT] * ch->cm, fa = s->pp[p][PP_FENV] + ch->fenv, rs_ = s->pp[p][PP_RES] + ch->res;
                if (fa > 1.0f) fa = 1.0f; if (rs_ > 1.0f) rs_ = 1.0f;
                const float fmd = s->pp[p][PP_FMODE];
                if (cu < 0.995f || fa > 0.01f || rs_ > 0.01f || fmd > 0.01f) {
                    float ramp = 1.0f;
                    if (s->pp[p][PP_FATK] > 0.01f) { const float at_ = 0.001f + s->pp[p][PP_FATK] * s->pp[p][PP_FATK] * 1.5f; ramp = v->t < at_ ? v->t / at_ : 1.0f; }
                    float ce = cu + fa * v->fenv * ramp + v->fkoff + s->pp[p][PP_VFLT] * (v->vel - 0.6f) * 0.8f; if (ce > 1.0f) ce = 1.0f; if (ce < 0.0f) ce = 0.0f;
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
                    { const float rq = s->pp[p][PP_RFQ]; v->ring_ph += (rq > 0.01f ? 40.0f + rq * rq * 2000.0f : 80.0f + rg * 900.0f) * s->inv_sr; } if (v->ring_ph >= 1.0f) v->ring_ph -= 1.0f;
                    float tri = 4.0f * fabsf(v->ring_ph - 0.5f) - 1.0f;
                    float m = rg * 3.0f; if (m > 1.0f) m = 1.0f;
                    x = x * (1.0f - m) + x * tri * m;
                }
                float pm = s->pp[p][PP_PUMP];
                if (pm > 0.01f) { float q = 1.0f - s->pump_ph, e = q * q; const float pc = s->pp[p][PP_PCV]; if (pc > 0.01f) e = e * (1.0f - pc) + e * e * e * pc; x *= 1.0f - pm * e; }
            }
            float pgl = s->pl[p], pgr = s->pr[p];
            {                                                              /* chop (1/16 pulse), tremolo, auto-pan */
                const float *q2 = s->pp[p];
                if (q2[PP_SCHP] > 0.01f) { float ph = s->pump_ph * 4.0f; ph -= floorf(ph); x *= 1.0f - q2[PP_SCHP] * (0.5f - 0.5f * cosf(TWO_PI * ph)); }
                if (q2[PP_TREM] > 0.01f || q2[PP_APAN] > 0.01f) {
                    v->trp += (0.5f + q2[PP_TRAT] * q2[PP_TRAT] * 14.5f) * s->inv_sr; if (v->trp >= 1.0f) v->trp -= 1.0f;
                    if (q2[PP_TREM] > 0.01f) x *= 1.0f - q2[PP_TREM] * (0.5f - 0.5f * cosf(TWO_PI * v->trp));
                    if (q2[PP_APAN] > 0.01f) { const float lf = sin2pi(v->trp); pgl *= 1.0f - q2[PP_APAN] * 0.5f * (1.0f + lf); pgr *= 1.0f - q2[PP_APAN] * 0.5f * (1.0f - lf); }
                }
            }
            x *= v->vel * s->gain[p] * s->tg[gi] * ch->gn;
            float wb = s->pp[p][PP_WOB];
            if (wb > 0.01f) {
                float tw = 4.0f * fabsf(s->wob_ph - 0.5f) - 1.0f;
                tl[p] += x * pgl * (1.0f - wb * 0.5f * (1.0f + tw));
                tr[p] += x * pgr * (1.0f - wb * 0.5f * (1.0f - tw));
            } else {
                tl[p] += x * pgl;
                tr[p] += x * pgr;
            }
            tsd[p] += x * s->pp[p][PP_DLY];
            float rs = s->pp[p][PP_RVB];
            if (v->type == T_RUMBKICK && rs < 0.9f) rs = 0.9f;
            tsr[p] += x * rs;
            { const float rmv = s->pp[p][PP_RMB]; if (rmv > 0.001f) { rmb_in += x * rmv; rmd_in += x; } }
        }

        float rec_l = 0.0f, rec_r = 0.0f;
        /* per-bus effects (a drum sound = one bus, a synth track = one bus); ducking, meters and resampling work per track */
        float dkg[NLANE], mtr[NTRK];
        for (int k = 0; k < NTRK; k++) mtr[k] = 0.0f;
        for (int k = 0; k < NLANE; k++) {                              /* duck gain per sound (lane) */
            dkg[k] = 1.0f;
            if (s->dk_env[k] > 0.0005f) { dkg[k] = 1.0f - s->pp[k][PP_DUCK] * s->dk_env[k]; s->dk_env[k] *= s->dk_c[k]; } else s->dk_env[k] = 0.0f;
        }
        float sds = 0.0f, srs = 0.0f;                                 /* summed delay / reverb sends of all buses */
        int blist[NLANE], nb = 0;
        for (int w = 0; w < 2; w++) for (uint64_t bits = tmk[w] | s->fxm[w]; bits; bits &= bits - 1) blist[nb++] = w * 64 + __builtin_ctzll(bits);
        for (int bq_ = 0; bq_ < nb; bq_++) {
            const int t = blist[bq_];
            const float L0 = L, R0 = R;
            if (!tact[t] && s->fx_act[t] <= 0) continue;                  /* nothing sounding and no effect tail on this bus */
            const int tk = lane_trk(s, t);
            if (dkg[t] < 1.0f) { const float g = dkg[t]; tl[t] *= g; tr[t] *= g; tsd[t] *= g; tsr[t] *= g; }
            sds += tsd[t]; srs += tsr[t];
            mtr[tk] += fabsf(tl[t]) + fabsf(tr[t]);
            if (tact[t]) s->fx_act[t] = (int)(6.0f * s->sr);
            if (s->fx_act[t] <= 0) { L += tl[t]; R += tr[t]; if (s->rec_src == tk) { rec_l += tl[t]; rec_r += tr[t]; } continue; }
            s->fx_act[t]--;
            const float *q = s->pp[t];
            float l = tl[t], r_ = tr[t];
            {   /* audio effect chain: slot 1, then slot 2 */
                xfx_proc(s, &s->xs[0][t], s->sbuf[t], (int)(q[PP_XFX] + 0.5f), q[PP_XMIX], q[PP_XRT], q[PP_XDP], &l, &r_);
                xfx_proc(s, &s->xs[1][t], NULL, (int)(q[PP_X2FX] + 0.5f), q[PP_X2MIX], q[PP_X2RT], q[PP_X2DP], &l, &r_);
                if (q[PP_GRN] >= 0.01f || s->gr_slot[t]) gran_proc(s, t, q, &l, &r_);
            }
            {   /* per-sound EQ: low shelf 100 Hz, peak 1 kHz, high shelf 8 kHz, +-12 dB (skipped while flat) */
                const float gq[3] = { (q[PP_EQL] - 0.5f) * 24.0f, (q[PP_EQM] - 0.5f) * 24.0f, (q[PP_EQH] - 0.5f) * 24.0f };
                if (fabsf(gq[0]) > 0.15f || fabsf(gq[1]) > 0.15f || fabsf(gq[2]) > 0.15f) {
                    if (!s->eq_on[t]) { memset(s->eq_z[t], 0, sizeof s->eq_z[t]); for (int b = 0; b < 3; b++) s->eq_g[t][b] = 999.0f; s->eq_on[t] = 1; }
                    const float fq[4] = { q[PP_EQLF], q[PP_EQMF], q[PP_EQHF], q[PP_EQQ] };
                    if (fq[0] != s->eq_p[t][0] || fq[1] != s->eq_p[t][1] || fq[2] != s->eq_p[t][2] || fq[3] != s->eq_p[t][3]) { for (int b = 0; b < 3; b++) s->eq_g[t][b] = 999.0f; memcpy(s->eq_p[t], fq, sizeof fq); }
                    for (int b = 0; b < 3; b++) if (s->eq_g[t][b] != gq[b]) { eq_coef(s->eq_c[t][b], b, gq[b], s->sr, s->eq_p[t][b], s->eq_p[t][3]); s->eq_g[t][b] = gq[b]; }
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
                if (q[PP_EQO] != s->eq_ogp[t]) { s->eq_ogp[t] = q[PP_EQO]; s->eq_og[t] = fabsf(q[PP_EQO] - 0.5f) < 0.01f ? 1.0f : powf(10.0f, (q[PP_EQO] - 0.5f) * 1.2f); }
                if (s->eq_og[t] != 1.0f) { l *= s->eq_og[t]; r_ *= s->eq_og[t]; }
            }
            if (q[PP_MONO] > 1.5f) { const float mm = 0.5f * (l + r_); l = mm * s->pl[t]; r_ = mm * s->pr[t]; }      /* DRY: only the dry sound is mono, the reverb / delay returns stay stereo */
            L += l; R += r_;

            if (q[PP_MONO] > 0.5f && q[PP_MONO] < 1.5f) { const float mm = 0.5f * ((L - L0) + (R - R0)); L = L0 + mm * s->pl[t]; R = R0 + mm * s->pr[t]; }      /* MONO: the sound stays in the middle, no stereo from effects */
            if (s->rec_src == tk) { rec_l += L - L0; rec_r += R - R0; }
        }
        for (int bq_ = 0; bq_ < nb; bq_++) {                          /* back to zero for the next sample; remember which buses still have an effect tail */
            const int t = blist[bq_];
            tl[t] = tr[t] = tsd[t] = tsr[t] = 0.0f; tact[t] = 0;
            if (s->fx_act[t] > 0) s->fxm[t >> 6] |= 1ull << (t & 63); else s->fxm[t >> 6] &= ~(1ull << (t & 63));
        }
        if (sds != 0.0f || srs != 0.0f) s->dr_act = (int)(6.0f * s->sr);        /* the shared delay + reverb run only while something is sent to them (and for their tail) */
        if (s->dr_act > 0) {
            if (--s->dr_act == 0) bus_clear(s);
            else {
                float yl_, yr_; bus_fx(s, sds, srs, dlen_g, &yl_, &yr_); L += yl_; R += yr_;
                if (sds == 0.0f && srs == 0.0f && fabsf(yl_) + fabsf(yr_) < 2e-6f) { if (++s->dr_quiet > 8192 && (int)(s->g[G_RMODE] + 0.5f) != 2) { s->dr_act = 1; } }      /* the tail has died away: stop early */
                else s->dr_quiet = 0;
            }
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
            float cl = (4.0f * fabsf(s->met_ph - 0.5f) - 1.0f) * s->met_env * s->met_amp * 0.5f;
            s->met_env *= 0.9965f; L += cl; R += cl;
        }
        if (!(fabsf(L) < 16.0f) || !(fabsf(R) < 16.0f)) {            /* NaN / runaway: wipe the feedback buffers */
            L = 0.0f; R = 0.0f;
            bus_clear(s);
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
        lfx_proc(s, &L, &R);
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
        {   /* TAPE (last stage before the output): a gentle cassette colour - soft saturation, a little head bump and top roll-off, slow pitch drift (wow), hiss. Everything is 0 = off. */
            const float tp = s->g[G_TAPE], hs = s->g[G_HISS], ww = s->g[G_WOW];
            if (tp > 0.01f || hs > 0.01f || ww > 0.01f) {
                float io[2] = { L, R };
                float dly = 0.0f;
                if (ww > 0.01f) {                                          /* wow (0.9 Hz) + flutter (6 Hz) + a slow random drift: about +-2.5 % pitch at full = a clearly warped tape, a quarter of that at 25 % */
                    s->tp_ph1 += 0.9f * s->inv_sr; if (s->tp_ph1 >= 1.0f) s->tp_ph1 -= 1.0f;
                    s->tp_ph2 += 6.1f * s->inv_sr; if (s->tp_ph2 >= 1.0f) s->tp_ph2 -= 1.0f;
                    s->tp_rng ^= s->tp_rng << 13; s->tp_rng ^= s->tp_rng >> 17; s->tp_rng ^= s->tp_rng << 5;
                    s->tp_dr += 0.00005f * (((float)(s->tp_rng & 0xFFFF) / 32768.0f - 1.0f) - s->tp_dr);
                    dly = 180.0f + ww * (160.0f * sinf(6.2831853f * s->tp_ph1) + 8.0f * sinf(6.2831853f * s->tp_ph2) + 700.0f * s->tp_dr);
                    if (dly < 2.0f) dly = 2.0f; if (dly > 2040.0f) dly = 2040.0f;
                }
                const int w = s->tp_w;
                for (int c = 0; c < 2; c++) {
                    float x = io[c];
                    if (ww > 0.01f) {
                        s->tp_dl[c][w] = x;
                        float rp = (float)w - dly; if (rp < 0.0f) rp += 2048.0f;
                        const int i0 = (int)rp, i1 = (i0 + 1) & 2047; const float fr = rp - (float)i0;
                        x = s->tp_dl[c][i0 & 2047] * (1.0f - fr) + s->tp_dl[c][i1] * fr;
                    }
                    if (tp > 0.01f) {
                        const float dg = 1.0f + tp * 5.0f;                        /* soft tape saturation: quiet signals keep their level, loud ones are squashed and thickened */
                        x = fast_tanh(x * dg) / dg * (1.0f + tp * 0.25f);
                        s->tp_hb[c] += (1.0f - expf(-6.2831853f * 90.0f * s->inv_sr)) * (x - s->tp_hb[c]);
                        x += s->tp_hb[c] * 0.35f * tp;                            /* head bump: a warm touch around 90 Hz */
                        const float fc = 16000.0f - 11000.0f * tp;                /* top roll-off 16 kHz -> 5 kHz */
                        s->tp_lp[c] += (1.0f - expf(-6.2831853f * fc * s->inv_sr)) * (x - s->tp_lp[c]);
                        x = s->tp_lp[c];
                    }
                    if (hs > 0.01f) {
                        s->tp_rng ^= s->tp_rng << 13; s->tp_rng ^= s->tp_rng >> 17; s->tp_rng ^= s->tp_rng << 5;
                        const float nz = (float)(s->tp_rng & 0xFFFF) / 32768.0f - 1.0f;
                        s->tp_hn[c] += 0.35f * (nz - s->tp_hn[c]);
                        x += (nz - s->tp_hn[c]) * 0.016f * hs * sqrtf(hs);            /* the hiss lives in the highs; full = about -45 dB, 25 % is still just audible in quiet parts */
                    }
                    io[c] = x;
                }
                s->tp_w = (w + 1) & 2047;
                L = io[0]; R = io[1];
            }
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
        if (s->rec_state == 2 && s->rec_src != -2 && s->rec_buf) {
            float cl = s->rec_src < 0 ? L : softclip(rec_l * mvol), cr = s->rec_src < 0 ? R : softclip(rec_r * mvol);
            if (s->rfx >= 4 && s->rec_n >= s->rec_in_n) { cl = 0.0f; cr = 0.0f; }
            if (s->rfx > 0) rec_fx(s, &cl, &cr);
            int ci = (int)(cl * 32767.0f), cj = (int)(cr * 32767.0f);
            if (ci > 32767) ci = 32767; if (ci < -32768) ci = -32768; if (cj > 32767) cj = 32767; if (cj < -32768) cj = -32768;
            s->rec_buf[2 * s->rec_n] = (int16_t)ci; s->rec_buf[2 * s->rec_n + 1] = (int16_t)cj;
            if (++s->rec_n >= s->rec_len) s->rec_state = 3;
        }
        {   /* scope + 8-band level meter of the master output (only reads, never changes the audio) */
            static const float scf[8] = { 80.f, 160.f, 320.f, 640.f, 1280.f, 2560.f, 5120.f, 10240.f };
            float m = (L + R) * 0.5f, prev = 0.0f;
            if (++s->sc_c >= 24) { s->sc_c = 0; s->sc_w[s->sc_i] = m; s->sc_i = (s->sc_i + 1) & 127; }
            if (s->sc_c == 0) { s->sc_sl[s->sc_si] = L; s->sc_sr[s->sc_si] = R; s->sc_si = (s->sc_si + 1) & 63; }
            s->sc_cll += (L * L - s->sc_cll) * 0.0003f; s->sc_crr += (R * R - s->sc_crr) * 0.0003f; s->sc_clr += (L * R - s->sc_clr) * 0.0003f;
            {   float pv = 0.0f;
                for (int b = 0; b < 16; b++) {
                    static float sc16[16]; if (sc16[0] == 0.0f) for (int q = 0; q < 16; q++) sc16[q] = 60.0f * powf(200.0f, (float)q / 15.0f);
                    float a2 = 1.0f - expf(-6.2831853f * sc16[b] * s->inv_sr);
                    s->sc_lp2[b] += a2 * (m - s->sc_lp2[b]);
                    float bd = s->sc_lp2[b] - pv; pv = s->sc_lp2[b];
                    s->sc_env2[b] += (fabsf(bd) - s->sc_env2[b]) * (s->sc_env2[b] < fabsf(bd) ? 0.01f : 0.0006f);
                }
            }
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
    for (int i = 0; i <= 2048; i++) g_sintab[i] = sinf(6.283185307f * (float)i / 2048.0f);
    if (host) {
        if (host->sample_rate > 0) g_sr = host->sample_rate;
        g_get_beat = host->get_beat_position;
        g_get_bpm = host->get_bpm;
        g_host = host;
    }
    (void)g_get_beat;
    return &g_api;
}
