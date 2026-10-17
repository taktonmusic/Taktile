/*
 * Taktile - overtake groovebox for Ableton Move (Schwung)
 *
 * 16 tracks (synth or drum) x up to 128 steps x 8 clips per track, 240 sounds, arp, per-track filter, shared delay + reverb bus, master bus.
 *
 *  Pads 1-16 (bottom 2 rows)  select + play a track      Pads 17-32: scale keyboard      Shift + pad: select silently
 *  Shift + Up/Down            top 16 pads per sound: PITCH keys / CLIPS (scenes + clips) / STEPS (16-step sequencer)
 *  Step buttons               toggle steps (hold a step + knobs 1-3: velocity, pitch, probability)
 *  Play / Shift+Play          start-stop / stop + reset    Shift+Jog click: tap tempo
 *  Rec                        tap = arm live record, hold (stopped) = step record (Left/Right = back/rest)
 *  Capture                    keep the notes you just played (last 2 bars)
 *  Mute                       tap = mute track, hold = mute map on the steps, Shift+Mute = solo
 *  |||  (Note/Session)        CLIP VIEW: tap = latch, hold = peek. Pads = clips of 4 tracks, Up/Down = group,
 *                             steps = select track, Loop + step 1-8 = scenes, Shift+pad = stop track
 *  Jog turn                   presets of the track's sound category (Shift = next category)
 *  Jog click                  SEQ or SOUND page: opens the preset browser (folders > sounds). Shift + jog click = HOME tiles on every page (also Shift + step 3). Other pages: jog click = HOME / last tab. Jog = choose, click = open, Back = up one level
 *                               FX tiles:     DELAY REVERB   (one shared bus: every track only sends into it)
 *                               HOME also has TRACK (synth/drum, volume, pad mode, drum sound)
 *                               MASTER tiles: MIX BUS TEMPO QUANT OPTION MORE
 *  Undo / Shift+Undo          undo / redo         Back x2 or Shift+Back: exit
 *  DRUM TRACKS                Shift + track button (or HOME > TRACK) = switch the track between SYNTH and DRUM (max 4 drum tracks).
 *                             Drum track: left 16 pads = the 16 sounds (white = selected, Shift+pad = select silently, Delete+pad = clear that sound),
 *                             right 16 pads = PITCH / REPEAT / STEPS of the selected sound (track button / Left / Right = change mode)
 *  Loop + step button         loop length (drum track: of the selected sound; Shift = whole track; Delete = sound follows the track)
 *  Loop + jogwheel            loop length +/-1 (Shift: +/-16 and the whole track)
 *  Loop + knob 3              loop length +/-1 (Shift +16)
 *  Mute + track button        mute / unmute that track      Hold step or step-pad + knobs 1-7: Vel Pitch Prob Gate Ratchet Nudge Cond
 *  Shift + step (same symbols as the Move): 1 CLIPS 2 OPTIONS 3 HOME 5 TEMPO 6 METRONOME 7 SWING 8 PAD MODE (PITCH/REPEAT/STEPS)
 *                                 9 KEY+SCALE 10 FULL VEL 11 REPEAT RATE 15 DOUBLE 16 QUANT   (4, 12, 13, 14: unused)
 */

/* boot breadcrumbs: show how far the module got (file /data/UserData/schwung/taktile-boot.log) */
let bootLog = '', bootTick = false;
function crumb(s) { bootLog += s + ' '; try { if (typeof host_write_file === 'function') host_write_file('/data/UserData/schwung/taktile-boot.log', bootLog); } catch (e) { /* ignore */ } }
crumb('loaded');
/* No external imports: the module must load even if the shared helper file changes or is missing. */
const setLED = function (n, c) { if (typeof move_midi_internal_send === 'function') move_midi_internal_send([0x09, 0x90, n, c]); };
const setButtonLED = function (cc, c) { if (typeof move_midi_internal_send === 'function') move_midi_internal_send([0x0B, 0xB0, cc, c]); };
function decodeDelta(v) { if (v === 0) return 0; if (v >= 1 && v <= 63) return v; if (v >= 65 && v <= 127) return -(128 - v); return 0; }

const CC = { SHIFT: 49, JOG: 14, JOGCLICK: 3, LEFT: 62, RIGHT: 63, LEFT_: -62, RIGHT_: -63, UP: 55, DOWN: 54, COPY: 60, DELETE: 119, MUTE: 88, PLAY: 85, REC: 86, BACK: 51, LOOP: 58, MENU: 50, UNDO: 56, SAMPLE: 118, TRK1: 43, TRK4: 40, MASTERK: 79, CAPTURE: 52, KNOB1: 71, KNOB8: 78 };
const PAD_FIRST = 68, PAD_LAST = 99, STEP_FIRST = 16, STEP_LAST = 31;
const NTRK = 16, NPAT = 8, MAXSTEP = 128, NLANE = 80;     /* lanes 0-15 = the 16 tracks, lanes 16-79 = 4 drum kits x 16 sounds */
const STATE_FILE = '/data/UserData/schwung/taktile-state.json';
const OLD_STATE_FILE = '/data/UserData/schwung/takton-groove-state.json';

const TYPE_NAME = ['Kick','KickHrd','KickGoa','RumbKick','Sub808','Clap','Snare','Rim','HatCl','HatOp','Shaker','Crash','Ride','Tom','Conga','Cowbell','PercFM','Zap','Riser','SubBass','RollBass','Acid','Reese','Pluck','Stab','Pad','Lead','Bell','Sweep','Impact','KickDeep','KickClick','KickPsy','KickDub','Kick909','Sub808Lo','Sub808Hi','SnrTight','SnrFat','ClapLong','ClapShrt','HatTick','HatLong','ShakeLng','RimClick','TomLo','TomHi','CongaHi','CongaLo','PercHi','PercLo','ZapHi','ZapLo','Bass1','BassHi','AcidLo','AcidHi','ReeseLo','ReeseHi','SubLow','PluckLo','PluckHi','StabLo','StabHi','PadLo','PadSlow','LeadLo','LeadHi','BellLo','BellHi','SweepLng','ImpactLo','RiserLng','SynLead','SynBass','SuperSaw','EPiano','Organ','Strings','FMBass','Hoover','LeadSoft','LeadHi','SawBass','BassHi','SuperHi','SuperLo','KeysHi','KeysLo','OrganLo','OrganHi','StringLo','StringSlw','FMBassHi','HooverLo','HooverHi','Kick707','KickTight','KickBoom','KickSoft','KickPunch','KickGabba','KickFull','KickTrnc','RumbShrt','RumbHuge','Sub808Mid','Sub808Hrt','KickSnap','KickWarm','KickRave','Snr909','SnrDeep','SnrSnap','SnrRoll','SnrLoFi','SnrBig','ClapDry','ClapRoom','ClapTrap','ClapSnap','RimTick','RimWood','RimLong','HatClsd','HatMid','HatSoft','HatTrap','HatOp909','HatShort','HatSizzl','HatHalf','ShakeSht','ShakeMed','ShakeSft','CrashShr','CrashLng','Splash','RideShrt','RideBell','RideSoft','TomMid','TomFloor','TomRack','TomSynth','CongaMid','BongoHi','BongoLo','CowShort','CowLong','FMclick','FMtom','FMmetal','FMblip','ZapMid','ZapLaser','ZapDown','SweepShrt','SweepSlow','ImpactHi','ImpactBig','ImpactMid','ImpactDry','SubSoft','SubPunch','SubMid','SubLong','RollFast','RollLong','RollDeep','Roll909','Acid303','AcidSlide','AcidShort','AcidDeep','AcidRave','ReeseDark','ReeseWide','ReeseFast','ReeseHigh','SqrBass','TriBass','PulsBass','FatBass','SineBass','PluckBass','LongBass','FMBass2','FMGrowl','FMDeep','FMHard','LeadMid','LeadFast','LeadSlow','SqrLead','TriLead','PulseLead','SineLead','FatLead','TranceLd','SoloLead','LeadLow','HoovFat','HoovDeep','HoovMid','HoovSqr','PluckMid','PluckShrt','PluckLong','PluckDeep','PluckSoft','StabShrt','StabLong','StabRave','StabHoov','PadWarm','PadFast','PadHuge','PadAir','BellMid','BellLong','BellSoft','Glass','SuperSqr','SuperFat','SuperTite','SuperPls','SuperSub','EPsoft','EPbright','EPbell','EPtine','OrganSft','OrganFull','OrganStac','OrganJazz','StringSaw','StringSqr','StringPls','StrWide','Kick90s','KickDry','KickLow','KickMid','KickHigh','KickThmp','KickClik','KickLng','KickShrt','KickDub2','KickHard','KickMax','KickNu','KickAcid','808Boom','808Tight','808Long','808Mid','808Pop','808Deep','RumbTrnc','RumbDark','RumbPsy','RumbGoa','SnrDry','SnrWide','SnrTop','SnrLow','SnrCrck','SnrRoom','SnrAcst','SnrTrap','ClapTght','ClapWide','ClapHigh','ClapStk','ClapRev','RimDry','RimHi','RimBox','HatTight','HatDust','HatSnap','HatLoose','HatOpen','HatSwsh','HatSemi','HatAir','ShakTght','ShakLoos','CrashBig','CrashDry','RideTink','RideWash','TomDeep','TomHigh','TomFat','TomPunch','CongaSlp','CongaOpn','CowMute','CowRing','FMBlip','FMBell','FMClang','FMTick','ZapDive','ZapRise','ZapBeam','SweepUp','SweepDwn','ImpactHg','ImpactTg','SubSine','SubDeep','SubTight','RollDown','RollUp','RollGoa','AcidSlam','AcidLong','Acid303','ReeseSub','ReeseMid','ReeseAir','PluckHi','PluckLow','PluckHarp','PluckMute','StabHard','StabSoft','StabRave2','StabHouse','PadDark','PadChoir','PadSlow2','PadGlas','LeadAcid','LeadSwor','LeadFlut','LeadReso','BellDeep','BellTine','BellGong','SawLead','SqrLead2','PulsLead','DetSaw','HugeSaw','OctSaw','TrncSaw','FoldSaw','VowelPad','SyncLead','HollowKy','StairBas','DigiLead','HalfSin','RampBas','Sample','KckTch1','KckTch2','KckTch3','KckTch4','KckTch5','KckTch6','KckTch7','KckTch8','KckHse1','KckHse2','KckHse3','KckHse4','KckHse5','KckHse6','KckTrn1','KckTrn2','KckTrn3','KckTrn4','KckTrn5','KckTrn6','KckPsy1','KckPsy2','KckPsy3','KckPsy4','KckPsy5','KckPsy6','KckHrd1','KckHrd2','KckHrd3','KckHrd4','KckHrd5','KckHrd6','KckDub1','KckDub2','KckDub3','KckDub4','KckDub5','KckDub6','808Kck1','808Kck2','808Kck3','808Kck4','808Kck5','808Kck6','SnrTch1','SnrTch2','SnrTch3','SnrTch4','SnrTch5','SnrTch6','SnrTch7','SnrTch8','SnrBig1','SnrBig2','SnrBig3','SnrBig4','SnrBig5','SnrBig6','SnrLof1','SnrLof2','SnrLof3','SnrLof4','SnrLof5','SnrLof6','ClpDry1','ClpDry2','ClpDry3','ClpDry4','ClpDry5','ClpDry6','ClpRoom1','ClpRoom2','ClpRoom3','ClpRoom4','ClpRoom5','ClpRoom6','ClpTrp1','ClpTrp2','ClpTrp3','ClpTrp4','RimTk1','RimTk2','RimTk3','RimTk4','RimTk5','RimTk6','HatCl01','HatCl02','HatCl03','HatCl04','HatCl05','HatCl06','HatCl07','HatCl08','HatCl09','HatCl10','HatOp01','HatOp02','HatOp03','HatOp04','HatOp05','HatOp06','HatOp07','HatOp08','HatOp09','HatOp10','ShkGrv1','ShkGrv2','ShkGrv3','ShkGrv4','ShkGrv5','ShkGrv6','CrsCym1','CrsCym2','CrsCym3','CrsCym4','CrsCym5','CrsCym6','RdeCym1','RdeCym2','RdeCym3','RdeCym4','RdeCym5','RdeCym6','TomSet1','TomSet2','TomSet3','TomSet4','TomSet5','TomSet6','TomSet7','TomSet8','CngSet1','CngSet2','CngSet3','CngSet4','CngSet5','CngSet6','BngSet1','BngSet2','BngSet3','BngSet4','CowSet1','CowSet2','CowSet3','CowSet4','FMPerc1','FMPerc2','FMPerc3','FMPerc4','FMPerc5','FMPerc6','FMPerc7','FMPerc8','ZapSet1','ZapSet2','ZapSet3','ZapSet4','ZapSet5','ZapSet6','ZapSet7','ZapSet8','SweepX1','SweepX2','SweepX3','SweepX4','SweepX5','SweepX6','SweepX7','SweepX8','ImpactX1','ImpactX2','ImpactX3','ImpactX4','ImpactX5','ImpactX6','ImpactX7','ImpactX8','SubBs1','SubBs2','SubBs3','SubBs4','SubBs5','SubBs6','SubBs7','SubBs8','RollBs1','RollBs2','RollBs3','RollBs4','RollBs5','RollBs6','RollBs7','RollBs8','AcidBs01','AcidBs02','AcidBs03','AcidBs04','AcidBs05','AcidBs06','AcidBs07','AcidBs08','AcidBs09','AcidBs10','ReeseBs1','ReeseBs2','ReeseBs3','ReeseBs4','ReeseBs5','ReeseBs6','ReeseBs7','ReeseBs8','SynBs01','SynBs02','SynBs03','SynBs04','SynBs05','SynBs06','SynBs07','SynBs08','SynBs09','SynBs10','SynBs11','SynBs12','FMBs1','FMBs2','FMBs3','FMBs4','FMBs5','FMBs6','FMBs7','FMBs8','Pluck01','Pluck02','Pluck03','Pluck04','Pluck05','Pluck06','Pluck07','Pluck08','Pluck09','Pluck10','Pluck11','Pluck12','SynLd01','SynLd02','SynLd03','SynLd04','SynLd05','SynLd06','SynLd07','SynLd08','SynLd09','SynLd10','SynLd11','SynLd12','Hoov1','Hoov2','Hoov3','Hoov4','Hoov5','Hoov6','Hoov7','Hoov8','Lead1','Lead2','Lead3','Lead4','Lead5','Lead6','Lead7','Lead8','Stab01','Stab02','Stab03','Stab04','Stab05','Stab06','Stab07','Stab08','Stab09','Stab10','Stab11','Stab12','Pad01','Pad02','Pad03','Pad04','Pad05','Pad06','Pad07','Pad08','Pad09','Pad10','Pad11','Pad12','Pad13','Pad14','Bell01','Bell02','Bell03','Bell04','Bell05','Bell06','Bell07','Bell08','Bell09','Bell10','Super01','Super02','Super03','Super04','Super05','Super06','Super07','Super08','Super09','Super10','EPno1','EPno2','EPno3','EPno4','EPno5','EPno6','EPno7','EPno8','Organ1','Organ2','Organ3','Organ4','Organ5','Organ6','Strng1','Strng2','Strng3','Strng4','Strng5','Strng6','Strng7','Strng8','DeepBs1','DeepBs2','DeepBs3','DeepBs4','DeepBs5','DeepBs6','DeepBs7','DeepBs8','DubBs1','DubBs2','DubBs3','DubBs4','DubBs5','DubBs6','DubBs7','DubBs8','TechBs1','TechBs2','TechBs3','TechBs4','TechBs5','TechBs6','TechBs7','TechBs8','AcdSqr1','AcdSqr2','AcdSqr3','AcdSqr4','AcdSqr5','AcdSqr6','AcdSqr7','AcdSqr8','RvDark1','RvDark2','RvDark3','RvDark4','RvDark5','RvDark6','RvDark7','RvDark8','GoaBs1','GoaBs2','GoaBs3','GoaBs4','GoaBs5','GoaBs6','GoaBs7','GoaBs8','WobBs1','WobBs2','WobBs3','WobBs4','WobBs5','WobBs6','WobBs7','WobBs8','DnbBs1','DnbBs2','DnbBs3','DnbBs4','DnbBs5','DnbBs6','DnbBs7','DnbBs8','HseBs1','HseBs2','HseBs3','HseBs4','HseBs5','HseBs6','HseBs7','HseBs8','AcdLd1','AcdLd2','AcdLd3','AcdLd4','AcdLd5','AcdLd6','AcdLd7','AcdLd8','TrcLd1','TrcLd2','TrcLd3','TrcLd4','TrcLd5','TrcLd6','TrcLd7','TrcLd8','PsyLd1','PsyLd2','PsyLd3','PsyLd4','PsyLd5','PsyLd6','PsyLd7','PsyLd8','SqrLd1','SqrLd2','SqrLd3','SqrLd4','SqrLd5','SqrLd6','SqrLd7','SqrLd8','SftLd1','SftLd2','SftLd3','SftLd4','SftLd5','SftLd6','SftLd7','SftLd8','SyncLd1','SyncLd2','SyncLd3','SyncLd4','SyncLd5','SyncLd6','HoovRv1','HoovRv2','HoovRv3','HoovRv4','HoovRv5','HoovRv6','HoovRv7','HoovRv8','ArpPl01','ArpPl02','ArpPl03','ArpPl04','ArpPl05','ArpPl06','ArpPl07','ArpPl08','ArpPl09','ArpPl10','ArpPl11','ArpPl12','PlkHrp1','PlkHrp2','PlkHrp3','PlkHrp4','PlkHrp5','PlkHrp6','PlkHrp7','PlkHrp8','PlkMut1','PlkMut2','PlkMut3','PlkMut4','PlkMut5','PlkMut6','PlkMut7','PlkMut8','DubCrd1','DubCrd2','DubCrd3','DubCrd4','DubCrd5','DubCrd6','DubCrd7','DubCrd8','HseStb01','HseStb02','HseStb03','HseStb04','HseStb05','HseStb06','HseStb07','HseStb08','HseStb09','HseStb10','RvStb1','RvStb2','RvStb3','RvStb4','RvStb5','RvStb6','RvStb7','RvStb8','TrcChd1','TrcChd2','TrcChd3','TrcChd4','TrcChd5','TrcChd6','TrcChd7','TrcChd8','DrkPad1','DrkPad2','DrkPad3','DrkPad4','DrkPad5','DrkPad6','DrkPad7','DrkPad8','WrmPad1','WrmPad2','WrmPad3','WrmPad4','WrmPad5','WrmPad6','WrmPad7','WrmPad8','AtmPad1','AtmPad2','AtmPad3','AtmPad4','AtmPad5','AtmPad6','AtmPad7','AtmPad8','GlsPad1','GlsPad2','GlsPad3','GlsPad4','GlsPad5','GlsPad6','GlsPad7','GlsPad8','TrcSaw1','TrcSaw2','TrcSaw3','TrcSaw4','TrcSaw5','TrcSaw6','TrcSaw7','TrcSaw8','SawStk1','SawStk2','SawStk3','SawStk4','SawStk5','SawStk6','SawStk7','SawStk8','Rhodes1','Rhodes2','Rhodes3','Rhodes4','Rhodes5','Rhodes6','Rhodes7','Rhodes8','Clav1','Clav2','Clav3','Clav4','Clav5','Clav6','Clav7','Clav8','Wurli1','Wurli2','Wurli3','Wurli4','Wurli5','Wurli6','OrgJzz1','OrgJzz2','OrgJzz3','OrgJzz4','OrgJzz5','OrgJzz6','OrgRck1','OrgRck2','OrgRck3','OrgRck4','OrgRck5','OrgRck6','BellMlt1','BellMlt2','BellMlt3','BellMlt4','BellMlt5','BellMlt6','BellMlt7','BellMlt8','Chime1','Chime2','Chime3','Chime4','Chime5','Chime6','Chime7','Chime8','Gong1','Gong2','Gong3','Gong4','Gong5','Gong6','StrSlw1','StrSlw2','StrSlw3','StrSlw4','StrSlw5','StrSlw6','StrSlw7','StrSlw8','Choir1','Choir2','Choir3','Choir4','Choir5','Choir6','Choir7','Choir8','Brass1','Brass2','Brass3','Brass4','Brass5','Brass6','RiseX1','RiseX2','RiseX3','RiseX4','RiseX5','RiseX6','RiseX7','RiseX8','DownX1','DownX2','DownX3','DownX4','DownX5','DownX6','DownX7','DownX8','NoiseSw1','NoiseSw2','NoiseSw3','NoiseSw4','NoiseSw5','NoiseSw6','NoiseSw7','NoiseSw8','ImpHit1','ImpHit2','ImpHit3','ImpHit4','ImpHit5','ImpHit6','ImpHit7','ImpHit8','GlchX01','GlchX02','GlchX03','GlchX04','GlchX05','GlchX06','GlchX07','GlchX08','GlchX09','GlchX10','LaserX1','LaserX2','LaserX3','LaserX4','LaserX5','LaserX6','LaserX7','LaserX8','MarimLo','MarimMid','MarimHi','MarimHard','MarimSft','VibesMid','VibesLo','VibesFst','XyloMid','XyloHi','KalimLo','KalimHi','Celesta','GlockHi','Tubular','ChurchLo','ChurchHi','SteelLo','SteelHi','Crystal','GlassTap','BowlLo','BowlHi','GamelanL','GamelanH','PlateLo','PlateHi','HarpLo','HarpMid','HarpHi','NylonLo','NylonHi','Koto','Hollow','TimpLo','TimpHi','Taiko','TablaLo','TablaHi','Bongo','TomModLo','TomModMd','TomModHi','WoodLo','WoodHi','Clave','MetalHit','Anvil','TinCan','PipeHit','EpTine','EpWarm','EpBright','EpSoft','FmClav','FmOrgan','FmOrgan2','FmMarim','FmVibe','FmBell','FmChime','FmGlass','FmPluck','FmHarp','FmKalim','FmSlap','FmSub','FmGrowl','FmWobble','FmReedBs','FmBrass','FmLead','FmSaw','FmPad','FmChoir','FmTom','FmZap','FmMetal','FmHat','FmClick','ChoirAh','ChoirOh','ChoirEe','ChoirOo','MaleAh','FemaleAh','VoxWow','VoxYoi','Talkbox','Robot','Whisper','GhostCh','Gospel','VoxBass','Pulse12','Pulse25','Pulse50','PwmSweep','Crunch4','ChipBass','ChipBs12','ChipSiren','ArpMaj','ArpMin','ArpOct','ArpPower','ArpFifth','Coin','PowerUp','ChipLaser','NoiseHat','NoiseOpen','NoiseSnr','NoiseMet','ChipKick','ChipTom','MBassDp','MBassGr','MBassWb','MBassCl','MBassSn','MBassSb','MBassRs','MLeadBr','MLeadSw','MLeadGl','MLeadSt','MLeadAr','MLeadHm','MLeadDk','MLeadAc','MPadDrk','MPadAir','MPadGls','MPadSwl','MPadWrm','MPadEvo','MPadOrg','MPadDst','MKeyPlk','MKeyStb','MKeyBel','MKeyWrm','MKeyArp','MKeyTr','MKeyGlw','MKeyOrg','FmInit','DKick808','DKickTch','DKickHse','DKickDub','DKickHrd','DKickPsy','DKickSub','DKickPnc','DKickTrp','DKickLof','DSnr909','DSnrTght','DSnrFat','DSnrDeep','DSnrCrck','DSnrBig','DSnrLoFi','DSnrElec','DHatCl','DHatTght','DHatSoft','DHatSizz','DHatOp','DHatOp2','DHatPed','DHatLoFi','DClap','DClapTgt','DClapLng','DClapTrp','DTomLo','DTomMid','DTomHi','DTomElec','DRim','DRimHi','DClave','DCowbell','DCowHi','DMetal','AnaInit','AnaSawLd','AnaPulse','AnaSync','AnaSyncHi','AnaBass','AnaSubBs','AnaAcid','AnaPwm','AnaPluck','AnaStab','AnaBrass','AnaPad','AnaTri','AnaHoov','WtInit','WtSweep','WtDark','WtBright','WtGlass','WtBass','WtPad','WtPluck','WtVox','WtMetal','WtWarp','WtEvolve','Fm4Init','Fm4Bell','Fm4EP','Fm4Bass','Fm4Brass','Fm4Pluck','Fm4Organ','Fm4Pad','Fm4Metal','Fm4Lead','Fm4Chime','Fm4Slap','AddInit','AddOrgan','AddClar','AddBell','AddGlass','AddPad','AddVox','AddSoft','AddBright','AddBow','AddHarp','AddTrem','PadInit','PadWarmth','PadSwarm','PadStr','PadSoft','PadPwm','PadSweep','PadDusk','PadBright','PadCrawl','PadEns','PadDrone','StrInit','StrNylon','StrSteel','StrHarp','StrMuted','StrBass','StrKoto','StrBanjo','StrPiano','StrHarsh','StrGlass','StrDrone','FoldInit','FoldBass','FoldLead','FoldPluck','FoldPad','FoldBell','FoldSub','FoldGlass','FoldDrone','FoldSeq','FoldAcid','FoldBuzz','AcKickRck','AcKickJzz','AcKickDp','AcKickTgt','AcKickPnc','AcKickVin','AcKickBtr','AcKickFlt','AcKickSub','AcKickLof','AcSnrRock','AcSnrTght','AcSnrFat','AcSnrJazz','AcSnrBrsh','AcSnrPicc','AcSnrRimS','AcSnrDry','AcSnrCrk','AcSnrDeep','AcSnrLoos','AcTomFlr','AcTomLo','AcTomMid','AcTomHi','AcTomRck','AcTomDry','AcTomDpFl','AcTomRoto','AcHatTgt','AcHatCls','AcHatSft','AcHatPdl','AcHatHlf','AcHatOpn','AcHatWid','AcHatSzl','AcCrash','AcCrshLng','AcCrshDry','AcSplash','AcChina','AcCrshSwl','AcRide','AcRideBel','AcRideSft','AcRideTnk','AcRim','AcStick','AcRimHi','AcRimLo','AcClick','AcClap','AcClapTgt','AcClapRm','AcClapSnp','AcClapHal','AcShaker','AcShkLng','AcEgg','AcCabasa','AcTamb','AcTambHit','AcWood','AcWoodHi','AcWoodLo','AcCowbell','AcCowLo','AcCowHi','AcTriang','AcTriMute','FxImpact','FxImpBig','FxImpDry','FxImpHi','FxSubDrp','FxThundr','FxBoom','FxRiser','FxRisHi','FxRisLo','FxRisNz','FxRisTn','FxRevCym','FxRevSnr','FxFall','FxFallNz','FxWhoosh','FxWhshLo','FxSweepUp','FxSweepDn','FxLaser','FxLaserLo','FxZapUp','FxBlip','FxBubble','FxNzHit','FxNzSnap','FxStatic','TnSub','TnSubPnc','TnBass','TnBassDub','TnStab','TnStabHi','TnPluck','TnStrPlk','TnMarim','TnKalim','TnVibes','TnGlock','TnBell','TnSteel','TnFmPluk','TnFmBass','TnFmBell','TnFoldPl','TnFoldBs','DKickThmp','DKickClk','DKickGrit','DKickBoom','DKickRnd'];
const BASE_T = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,0,1,2,3,0,4,4,6,6,5,5,8,9,10,7,13,13,14,14,16,16,17,17,20,20,21,21,22,22,19,23,23,24,24,25,25,26,26,27,27,28,29,18,73,74,75,76,77,78,79,80,73,73,74,74,75,75,76,76,77,77,78,78,79,80,80,0,0,0,0,1,1,2,2,3,3,4,4,0,0,2,6,6,6,6,6,6,5,5,5,5,7,7,7,8,8,8,8,9,9,9,9,10,10,10,11,11,11,12,12,12,13,13,13,13,14,14,14,15,15,16,16,16,16,17,17,17,28,28,29,29,29,29,19,19,19,19,20,20,20,20,21,21,21,21,21,22,22,22,22,74,74,74,74,74,74,74,79,79,79,79,26,26,26,73,73,73,73,73,73,73,73,80,80,80,80,23,23,23,23,23,24,24,24,24,25,25,25,25,27,27,27,27,75,75,75,75,75,76,76,76,76,77,77,77,77,78,78,78,78,0,0,0,0,0,1,1,2,2,0,1,1,2,0,4,4,4,4,4,4,3,3,3,3,6,6,6,6,6,6,6,6,5,5,5,5,5,7,7,7,8,8,8,8,9,9,9,9,10,10,11,11,12,12,13,13,13,13,14,14,15,15,16,16,16,16,17,17,17,28,28,29,29,19,19,19,20,20,20,21,21,21,22,22,22,23,23,23,23,24,24,24,24,25,25,25,25,26,26,26,26,27,27,27,73,73,73,75,75,75,75,74,78,73,76,74,73,77,74,359,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,3,3,3,3,3,3,1,1,1,1,1,1,0,0,0,0,0,0,4,4,4,4,4,4,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,11,11,11,11,11,11,12,12,12,12,12,12,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,15,15,15,15,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,19,19,19,19,19,19,19,19,20,20,20,20,20,20,20,20,21,21,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,74,74,74,74,74,74,74,74,74,74,74,74,79,79,79,79,79,79,79,79,23,23,23,23,23,23,23,23,23,23,23,23,73,73,73,73,73,73,73,73,73,73,73,73,80,80,80,80,80,80,80,80,26,26,26,26,26,26,26,26,24,24,24,24,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,25,25,25,25,25,25,27,27,27,27,27,27,27,27,27,27,75,75,75,75,75,75,75,75,75,75,76,76,76,76,76,76,76,76,77,77,77,77,77,77,78,78,78,78,78,78,78,78,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,74,74,74,74,74,74,74,74,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,20,20,20,20,20,20,20,20,79,79,79,79,79,79,79,79,74,74,74,74,74,74,74,74,74,74,74,74,74,74,74,74,26,26,26,26,26,26,26,26,73,73,73,73,73,73,73,73,26,26,26,26,26,26,26,26,73,73,73,73,73,73,73,73,73,73,73,73,73,73,73,73,26,26,26,26,26,26,80,80,80,80,80,80,80,80,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,75,75,75,75,75,75,75,75,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,75,75,75,75,75,75,75,75,75,75,75,75,75,75,75,75,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,76,77,77,77,77,77,77,77,77,77,77,77,77,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,75,75,75,75,75,75,18,18,18,18,18,18,18,18,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,23,23,23,23,23,27,27,27,23,23,23,23,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,23,23,23,23,23,23,23,14,14,13,14,14,14,13,13,13,7,7,7,15,15,7,15,76,76,76,76,76,77,77,23,27,27,27,27,23,23,23,74,74,79,79,74,73,73,73,25,78,13,17,16,8,7,78,78,78,78,25,25,26,26,26,26,25,25,78,74,26,26,26,26,26,74,74,26,23,23,23,23,23,23,17,17,8,9,6,7,0,13,74,74,79,74,74,19,22,73,26,26,26,26,26,26,26,25,25,25,25,25,25,25,25,23,24,27,76,23,24,27,77,76,0,0,0,0,0,0,0,0,0,0,5,5,5,5,5,5,5,5,8,8,8,8,9,9,8,8,6,6,6,6,13,13,13,13,6,6,14,14,14,14,26,26,26,26,26,74,19,21,26,23,24,73,25,26,80,26,26,74,26,27,74,25,23,25,24,26,25,76,27,76,79,73,23,77,25,27,26,27,79,26,77,24,27,27,25,25,26,26,78,23,25,25,25,25,78,25,25,25,25,25,25,78,25,23,23,23,23,23,74,23,23,76,26,27,25,26,74,26,23,25,27,19,27,25,23,21,26,0,0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,13,13,13,13,13,13,13,13,8,8,8,8,9,9,9,9,11,11,11,11,11,11,12,12,12,12,7,7,7,7,7,5,5,5,5,5,10,10,10,10,10,10,14,14,14,15,15,15,15,15,29,29,29,29,29,29,29,18,18,18,18,18,18,18,28,28,28,28,28,28,17,17,17,17,17,4,4,4,19,19,74,74,24,24,23,23,23,23,27,27,27,27,23,74,27,23,74,0,0,0,0,0];
const ENUMS = {
    type: TYPE_NAME,
    wave: ['Auto','Saw','Square','Tri','Sine','Pulse','Pulse12','RampDn','HalfSin','Organ','Hollow','Fold','Stair','Digi','Vowel','Sync'],
    poly: ['Mono','Poly'],
    mfx: ['OFF','OCTV','CHRD','ARP','STRM','ECHO','RNDP','RNDV','COMP','SCAL','TRNS','DIAT'],
    xfx: ['OFF','STUT','REV','FLNG','TREM','APAN','COMB','WAH','GATE','LOFI','DJ','GRAN'],
    dmode: ['PING','TAPE','DUB'],
    rmode: ['HALL','ECHO','SHIM'],
    mono: ['STEREO','MONO','DRY'],
    dtime: ['1/16','1/8','3/16','1/4','1/2'],
    arp: ['Off','Up','Down','UpDn','Rand','Roll'],
    rate: ['1/8','1/16','1/16T','1/32'],
    key: ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'],
    scale: ['Minor','Phryg','Major'],
    dly_time: ['1/16','1/8','3/16','1/4','1/2'],
};
ENUMS.m2fx = ENUMS.mfx; ENUMS.x2fx = ENUMS.xfx; ENUMS.dly_mode = ENUMS.dmode; ENUMS.rvb_mode = ENUMS.rmode;
const PK = ['type','vol','pan','tune','decay','tone','drive','dly','rvb','arp','rate','crush','locut','ring','pump','wob','cut','res','fenv','wave','det','sub','att','poly','dtime','dfb','rsize','rdark','chor','chrt','chdp','phas','phrt','phfb','xfx','xmix','xrt','xdp','mfx','mamt','mprm','smp','sst','sen','smd','duck','duckr','sgn','satk','srel','sstr','scs','tsw','fdec','fkey','fmode','rmb','rmt','eql','eqm','eqh','x2fx','x2mix','x2rt','x2dp','m2fx','m2amt','m2prm','dmode','rmode','mono','snz','fatk','sfld','schp','trem','trat','apan','vflt','grn','gsz','gsp','gpi','mch','mvb','trn','vhm','vmn','vmx','orn','eqlf','eqmf','eqhf','eqq','eqo','pcv','rfq','dtn','dwd','rwd','rlc','gdn','grv','gfz','gtn'];
const PDEF = { 97: 0.5, 98: 0.5, 99: 0.5, 101: 0.4, 104: 1, 83: 1, 84: 0.5, 85: 0.5, 88: 1, 90: 0.5, 91: 0.5, 92: 0.5, 93: 0.5, 94: 0.5, 80: 0.4, 81: 0.5, 76: 0.4, 70: 1, 57: 0.5, 58: 0.5, 59: 0.5, 60: 0.5, 16: 1, 24: 1, 25: 0.4, 26: 0.55, 27: 0.5, 29: 0.3, 30: 0.5, 32: 0.3, 33: 0.5, 35: 0.5, 36: 0.5, 37: 0.5, 39: 0.5, 40: 0.5, 43: 1, 46: 0.4, 47: 0.5, 50: 0.5, 52: 0.5, 53: 0.5, 62: 0.5, 63: 0.5, 64: 0.5, 66: 0.5, 67: 0.5 };
function defOf(i) { return PDEF[i] !== undefined ? PDEF[i] : 0; }
const MK = ['m_vol','key','scale','dly_time','dly_fb','dly_ret','rvb_size','rvb_dark','rvb_ret','m_filt','m_sat','m_wid','m_comp','m_hum','m_met','m_qnt','m_eql','m_eqm','m_eqh','m_lim','m_chp','dly_mode','dly_tone','dly_wide','rvb_mode','rvb_wide','rvb_low'];
const PI = {}; PK.forEach((k, i) => PI[k] = i);
const FAV_MAP = { dtime: 'dly_time', dfb: 'dly_fb', rsize: 'rvb_size', rdark: 'rvb_dark', dmode: 'dly_mode', rmode: 'rvb_mode', dtn: 'dly_tone', dwd: 'dly_wide', rwd: 'rvb_wide', rlc: 'rvb_low', chor: 'dly', phas: 'rvb', chrt: 'dly_time', chdp: 'dly_fb', phrt: 'rvb_size', phfb: 'rvb_dark' };     /* favourites saved before the shared FX bus */
const MI = {}; MK.forEach((k, i) => MI[k] = i);
const SCALE7 = [[0,2,3,5,7,8,10],[0,1,3,5,7,8,10],[0,2,4,5,7,9,11]];
const PROB_PCT = Array.from({ length: 32 }, (_, i) => Math.round(100 - i * 100 / 32));   /* 32 levels */
const NUD_MAX = 28;                                                       /* nudge 0..28 = 0..70 % of a step in 2.5 % steps */
function nudTxt(n) { return '+' + (n * 2.5).toFixed(n % 2 ? 1 : 0) + '%'; }

/* default sounds for the 16 tracks: type, tune, decay, arp, rate, dly, rvb (mirrors the DSP) */
const LAYOUT = [
    [0,.5,.5,0,1,0,0],[3,.5,.5,0,1,0,0],[5,.5,.5,0,1,0,0],[6,.5,.5,0,1,0,0],
    [8,.5,.5,0,1,0,0],[9,.5,.5,0,1,0,0],[12,.5,.5,0,1,0,0],[7,.5,.5,0,1,0,0],
    [13,.5,.5,0,1,0,0],[16,.5,.5,0,1,0,0],[19,.5,.5,0,1,0,0],[20,.5,.5,0,1,0,0],
    [21,.5,.5,0,1,0,0],[23,.5,.5,0,1,0,0],[24,.5,.5,0,1,0,0],[25,.5,.5,0,1,0,0],
];
const DRUM_LAYOUT = [
    [0,.5,.5,0,1,0,0],[1,.5,.5,0,1,0,0],[6,.5,.5,0,1,0,.12],[5,.5,.5,0,1,.1,.15],
    [7,.5,.5,0,1,0,0],[8,.5,.5,0,1,0,0],[9,.5,.5,0,1,0,0],[12,.5,.5,0,1,0,0],
    [11,.5,.5,0,1,0,.2],[13,.35,.5,0,1,0,0],[13,.7,.5,0,1,0,0],[14,.5,.5,0,1,0,0],
    [15,.5,.5,0,1,0,0],[10,.5,.5,0,1,0,0],[16,.5,.5,0,1,.15,0],[17,.5,.5,0,1,.3,0],
];
const MASTER_DEF = [0.8, 5, 0, 1, 0.4, 0.5, 0.6, 0.5, 0.5, 0.5, 0, 0.5, 0, 0, 0, 0.64, 0.5, 0.5, 0.5, 0, 0, 0, 0.5, 0.5, 0, 0.5, 0];
const VIEWS = ['SEQ', 'SOUND', 'FILTER', 'MOD/ARP', 'BUS', 'DELAY', 'REVERB', 'CLIPS', 'OPTIONS', 'SYNTH', 'TEMPO', 'METRO', 'GROOVE', 'FULL VEL', 'CHORUS', 'QUANT', 'HOME', 'FX', 'MASTER', 'PHASER', 'MIX', 'TRACK', 'TEMPO', 'SWING', 'MULTI', 'KEY', 'REPEAT', 'MIDI FX', 'SAMPLE', 'SLICE', 'DUCK', 'SONG', 'ENV', 'RESAMPLE', 'KIT', 'PROJECT', 'EUCLID', 'MIDI OUT', 'FX SEND', 'FX TIME', 'HISTORY', 'MASTER EQ', 'SCOPE', 'TRACK EQ'];
const TAB_SYM = { 50: 'GRAIN', 43: 'EQ', 1: 'SOUND', 9: 'SYNTH', 28: 'SAMPLE', 2: 'FILTER', 3: 'MOD', 38: 'SEND', 39: 'BUS', 27: 'MIDI', 36: 'EUCLID', 46: 'BUS' };
const TAB_NAME = { 44: 'CHORD', 50: 'GRANULAR', 43: 'SOUND EQ', 1: 'PRESET', 9: 'SYNTH ENGINE', 28: 'SAMPLE', 2: 'FILTER', 3: 'MOD + DUCK', 38: 'TRACK FX', 39: 'FX BUS 2', 27: 'MIDI FX', 36: 'EUCLID', 46: 'FX BUS' };
function stepWinView() { return view === 0 || (isTab(view) && view !== 36 && !(view === 28 && broOpen)); }   /* a held step opens its edit window on the SEQ page and on the sound tabs */
function isTab(v) { return v === 43 || v === 1 || v === 9 || v === 28 || v === 2 || v === 3 || v === 38 || v === 39 || v === 46 || v === 27 || v === 36 || v === 50; }
function trackTabs() { const smp = Math.round(pads[ln()][0]) === 359, syn = !kind[sel] && !smp; const tb = syn ? [1, 9, 3, 2, 38, 46, 36, 27] : [1, 3, 2, 38, 46, 36, 27, 28]; if (chordOn()) tb.unshift(44); return tb; }      /* SOUND, FILTER, MOD, FX, FX 2, EUCLID, MIDI FX, SYNTH/SAMPLE last */
const CAT_NAME = ['KICK', 'SNARE', 'HATS', 'PERC', 'FX', 'BASS', 'LEAD', 'CHORD', 'SAMPLE'];
function catOf(t) {
    const b = BASE_T[t];
    if (b === 359) return 8;
    if (b <= 4) return 0; if (b <= 7) return 1; if (b <= 12) return 2; if (b <= 17) return 3;
    if (b === 18 || b === 28 || b === 29) return 4;
    if ((b >= 19 && b <= 22) || b === 74 || b === 79) return 5;
    if (b === 26 || b === 73 || b === 80 || b === 23) return 6;
    return 7;
}
function catList(c) { const l = []; for (let t = 0; t < TYPE_NAME.length; t++) if (catOf(t) === c) l.push(t); return l; }
let brw = null, brwTicks = 0;
function drawBrowse() {                                    /* preset list: 5 rows, current in the middle */
    const b = brw; clear_screen();
    titleBar((b.title || CAT_NAME[b.c]) + ' ' + (b.i + 1) + '/' + b.l.length, 'T' + p2(sel + 1));
    for (let q = -2; q <= 2; q++) {
        const k = b.i + q, y = 12 + (q + 2) * 10;
        if (k < 0 || k >= b.l.length) continue;
        const nm = b.names ? (b.names[b.l[k]] || '') : TYPE_NAME[b.l[k]];
        const fv = (!b.names && favs.indexOf(b.l[k]) >= 0) ? '*' : ' ';
        if (q === 0) { fill_rect(0, y, 128, 10, 1); txt(4, y + 1, '>' + fv + mq(nm, 19), 0); } else txt(4, y + 1, (' ' + fv + nm).slice(0, 20), 1);
    }
}
function sampleList(d) {                                   /* sample file knob: the same top-to-bottom list */
    selectSample(d, false);
    const l = smpLoaded(); if (!l.length) return;
    brw = { c: 8, l: l, i: Math.max(0, l.indexOf(smpIdx(ln()))), title: 'SAMPLES', names: smpNames }; brwTicks = 55; needsRedraw = true;
}
function browseSound(dl) {                                 /* jog: presets inside the category of the track's sound (Shift = next category) */
    const L = ln(), cur = Math.round(pads[L][0]);
    let c = catOf(cur), l = catList(c), i = l.indexOf(cur), nt;
    if (shiftHeld) { fbOpen(); return; }
    else if (favOnly && favs.length) { l = favs.slice().sort((a, b) => a - b); const at = l.indexOf(cur); i = at >= 0 ? clamp(at + (dl > 0 ? 1 : -1), 0, l.length - 1) : (dl > 0 ? l.findIndex(x => x > cur) : l.length - 1 - l.slice().reverse().findIndex(x => x < cur)); if (i < 0 || i >= l.length) i = dl > 0 ? l.length - 1 : 0; nt = l[i]; c = catOf(nt); }
    else if (browseAll) { l = TYPE_NAME.map((x, k) => k); i = clamp(cur + (dl > 0 ? 1 : -1), 0, l.length - 1); nt = i; c = catOf(nt); }
    else {                                                  /* one long list: past the end of a category the next one continues */
        const cmin = kind[sel] ? 0 : 5;                     /* a synth track stays with the synth sounds (BASS, LEAD, CHORD); drum sounds are only reached through the browser */
        let ni = i + (dl > 0 ? 1 : -1);
        if (c < cmin) { c = cmin; l = catList(c); ni = dl > 0 ? 0 : l.length - 1; }
        else if (ni < 0 && c > cmin) { c--; l = catList(c); ni = l.length - 1; } else if (ni >= l.length && c < 7) { c++; l = catList(c); ni = 0; }
        i = clamp(ni, 0, l.length - 1); nt = l[i];
    }
    brw = { c: c, l: l, i: Math.max(0, l.indexOf(nt)) }; brwTicks = 55; needsRedraw = true;
    if (nt === cur) return;
    pads[L][0] = nt; if (liveOk(sel)) enqueue('p' + L + 'type', 'p' + p2(L + 1) + '_type', String(nt)); storeCs(L);
    markEdited(); needsRedraw = true;
}
function jogFirst(dl) {
    const items = VIEW_ITEMS[view]; if (!items || !items.length) return;
    const key = items[0][0], delta = Math.sign(dl) * Math.min(Math.abs(dl), 3), ts0 = toastSeq;
    if (editParam(scopeOf(key), key, delta, 0)) {
        markEdited(); if (toastSeq !== ts0) return;
        if (!bigOn && !CURVE_KEYS[key]) { needsRedraw = true; return; }
        const v2 = getVal(scopeOf(key), key); curveKey = key; showToast(fullLabel(view, 0, items).toUpperCase(), fmtVal(key, v2), fracVal(key, v2)); if (toast) toast.nm = 1;
    }
}
function padView(v) { return v === 50 || v === 43 || v === 30 || v === 38 || v === 39 || v === 46 || v === 36 || (v >= 1 && v <= 3) || v === 5 || v === 6 || v === 9 || v === 14 || v === 19 || v === 24 || v === 26 || v === 27; }
function isHub(v) { return v >= 16 && v <= 18; }
/* every parameter lives on exactly ONE page */
const VIEW_ITEMS = {
    1: [['type','Type'],['tune','Tune'],['decay','Decay','Dcy'],['tone','Tone'],['drive','Drive','Drv'],['pan','Pan'],['poly','Voices','Poly'],['vol','Vol']],
    2: [['cut','LPCut','LPF'],['res','Reso'],['locut','HPCut','HPF'],['fenv','FltEnv','FEnv'],['fdec','FltDecay','FDec'],['fkey','KeyTrack','FKey'],['fmode','Mode'],['crush','Crush','Crsh']],
    3: [['ring','Ring'],['rfq','RingFrq','RFrq'],['pump','Pump'],['wob','Wobble','Wob'],['duck','Duck'],['duckr','DuckRel','DRel'],['scs','DuckSrc','DSrc'],['pcv','PumpCrv','PCrv']],       /* synth tracks; drum tracks add the kick rumble (viewItemsFix) */
    38: [['dly','DlySend','DSnd'],['rvb','RvbSend','RSnd'],['xfx','Effect','FX'],['xmix','Amount','Amt'],['xrt','Rate'],['xdp','Depth','Dpth'],['mono','Output','Out'],['morph','Morph A>B','Mrph']],
    46: [['dly_time','DlyTime','DTim'],['dly_fb','DlyFdbk','DFb'],['dly_mode','DlyType','DTyp'],['dly_tone','DlyTone','DTon'],['rvb_size','RvbSize','RSiz'],['rvb_dark','RvbDark','RDrk'],['rvb_mode','RvbType','RTyp'],['rvb_ret','RvbRet','RRet']],
    50: [['grn','Grain','Grn'],['gsz','Size'],['gsp','Spread','Sprd'],['gpi','Pitch','Ptch'],['gdn','Density','Dens'],['grv','Reverse','Rev'],['gfz','Freeze','Frz'],['gtn','Window','Win']],
    39: [['dly_ret','DlyRet','DRet'],['dly_wide','DlyWide','DWid'],['rvb_wide','RvbWide','RWid'],['rvb_low','RvbLow','RLow']],
    41: [['m_eql','Low'],['m_eqm','Mid'],['m_eqh','High'],['m_lim','Limiter','Lim'],['m_vol','Vol'],['m_filt','Filter','Flt'],['m_sat','Satur','Sat'],['m_comp','Comp']],
    4: [['m_vol','Vol'],['m_filt','Filter','Flt'],['m_sat','Satur','Sat'],['m_wid','Width','Wid'],['m_comp','Comp'],['dly_ret','DlyRet','DRet'],['rvb_ret','RvbRet','RRet'],['m_lim','Limiter','Lim']],
    5: [['dly','Send'],['dly_time','Time'],['dly_fb','Fdbk'],['dly_mode','Type'],['dly_tone','Tone'],['dly_wide','Wide'],['dly_ret','Return','Ret']],
    6: [['rvb','Send'],['rvb_size','Size'],['rvb_dark','Dark'],['rvb_mode','Type'],['rvb_wide','Wide'],['rvb_low','LowCut','Low'],['rvb_ret','Return','Ret']],
    8: [['browse','Jog'],['fullvel','FullVel','FVel'],['rqoff','RecSnap','Snap'],['rdens','RndDens','Dens'],['rvar','RndVel','RVel'],['rkey','RndKey','RKey'],['rrat','RndRatch','RRat'],['m_met','Metro','Met']],
    10: [['bpm','BPM'],['swing','Swing','Swg'],['m_hum','Human','Hum'],['m_met','Metro','Met'],['m_qnt','Launch','Lnch'],['key','Key'],['scale','Scale','Scl'],['qscale','ScaleQ','ScQ']],
    15: [['qgrid','Grid'],['qstr','Strength','Str'],['rqoff','RecSnap','Snap'],['m_qnt','Launch','Lnch'],['lay','Layout','Lay'],['key','Key'],['scale','Scale','Scl']],
    24: [['xfx','Effect','FX'],['xmix','Amount','Amt'],['xrt','Rate'],['xdp','Depth','Dpth']],
    49: [['m2fx','MIDI FX 2','FX'],['m2amt','Amount','Amt'],['m2prm','Param','Prm'],['trn','Transp','Xpos'],['mvb','VelBias','VelB'],['vhm','VelHum','VHum'],['vmn','VelMin','VMin'],['vmx','VelMax','VMax']],
    27: [['mfx','MIDI FX','Effect'],['mamt','Amount','Amt'],['mprm','Param','Prm'],['key','Key'],['arp','Arp'],['rate','ArpRate','Rate'],['scale','Scale','Scl'],['mch','Prob']],
    25: [['key','Key'],['scale','Scale','Scl'],['qscale','ScaleQ','ScQ'],['m_qnt','Launch','Lnch'],['bpm','BPM'],['swing','Swing','Swg'],['m_hum','Human','Hum'],['m_met','Metro','Met']],
    26: [['rrate','Rate'],['repmode','Repeat','Rep'],['fullvel','FullVel','FVel'],['rqoff','RecSnap','Snap'],['qscale','ScaleQ','ScQ'],['bpm','BPM'],['swing','Swing','Swg']],
    22: [['bpm','BPM'],['swing','Swing','Swg'],['m_hum','Human','Hum'],['m_met','Metro','Met'],['m_qnt','Launch','Lnch'],['key','Key'],['scale','Scale','Scl'],['qscale','ScaleQ','ScQ']],
    23: [['swing','Swing','Swg'],['bpm','BPM'],['m_hum','Human','Hum'],['m_met','Metro','Met'],['tsw','Swing+','Swg+'],['m_qnt','Launch','Lnch'],['key','Key'],['scale','Scale','Scl']],
    30: [['duck','Amount','Amt'],['duckr','Release','Rel'],['scs','Source','Src'],['pump','Pump'],['wob','Wobble','Wob'],['ring','Ring'],['crush','Crush','Crsh'],['locut','HPCut','HPF']],
    32: [['sgn','Gain'],['satk','Attack','Atk'],['srel','Release','Rel'],['sstr','Stretch','Strc'],['smd','Mode'],['sst','Start','Strt'],['sen','End'],['vol','Vol']],
    33: [['rsrc','Source','Src'],['rbar','Bars'],['rgo','Record','Rec'],['rst','State','Stat'],['m_vol','Master','Vol'],['m_lim','Limiter','Lim'],['bpm','BPM'],['m_met','Metro','Met']],
    34: [['kslot','Slot'],['ksave','Save'],['kload','Load'],['kinf','Info'],['kprev','Listen','Hear'],['dsel','Sound','Snd'],['vol','Vol'],['pan','Pan']],
    35: [['pslot','Slot'],['psave','Save'],['pload','Load'],['pinf','Info'],['pnew','New'],['prst','Revert','Rvrt'],['bpm','BPM'],['m_vol','Master','Vol']],
    43: [['eql','Low'],['eqm','Mid'],['eqh','High'],['eqlf','LowFreq','LowF'],['eqmf','MidFreq','MidF'],['eqhf','HighFreq','HiF'],['eqq','MidWidth','MidQ'],['eqo','EQ Out','Out']],
    36: [['ehit','Hits'],['erot','Rotate','Rot'],['elen','Length','Len'],['emut','Mutate','Mut']],
    37: [['moclk','Clock','Clk'],['motn','Track','Trk'],['mont','Notes','Note'],['moall','All'],['mochn','Channel','Chan'],['moxp','Transpose','Xpos'],['movel','Velocity','Vel'],['modn','Drum note','Drum']],
    31: [['sgsl','Slot'],['sgcl','Scene','Scn'],['sgbr','Bars'],['sgrp','Repeat','Rep'],['sgtp','Tempo','Tmp'],['sgtr','Change','Chg'],['sgtg','Name'],['sgon','Song']],
    29: [['slm','Mark'],['slp','Pos'],['sln','Count','Cnt'],['sla','Auto'],['sladd','Add'],['sldel','Delete','Del'],['slsn','Snap'],['slg','To Pads','Pads']],
    48: [['smfit','Fit Tempo','Fit'],['smhf','Half'],['smdb','Double','Dbl'],['smnx','Move'],['smrn','Random','Rand'],['smrv','Reverse','Rev'],['smnm','Normal','Norm'],['smrs','Reset','Rst']],
    28: [['smp','File'],['sst','Start','Strt'],['sen','End'],['smd','Mode'],['tune','Tune'],['sgn','Gain'],['pan','Pan'],['vol','Vol']],
    21: [['kind','Kind'],['tvol','Volume','Vol'],['pmode','Pads'],['dsel','Sound','Snd'],['lay','Layout','Lay'],['tcol','Color','Col'],['tsw','Swing+','Swg+'],['pan','Pan']],
    47: [['snz','Noise','Nois'],['fatk','FltAtk','FAtk'],['sfld','Fold'],['schp','Chop'],['trem','Tremolo','Trem'],['trat','TremRate','TRt'],['apan','AutoPan','APan'],['vflt','VelFilt','VFlt']],
    9: [['wave','Wave'],['det','Detune','Det'],['sub','Sub'],['drive','Drive','Drv'],['cut','LPCut','LPF'],['res','Reso','Res'],['fenv','FltEnv','FEnv'],['poly','Voices','Poly']],
};
/* home / fx / master hubs: tiles you pick with the jogwheel (turn = choose, click = open, Back = up one level) */
const HUBS = {
    16: ['HOME', [['SOUND', 1], ['TRACK', 21], ['MIX', 20], ['BUS', 4], ['OPTIONS', -1], ['MORE', 18]]],
    17: ['FX', [['DELAY', 5], ['REVERB', 6], ['MULTI', 24], ['MIDI', 27], ['DUCK', 30]]],
    18: ['MORE', [['TEMPO', 10], ['SONG', 31], ['PROJ', 35], ['KIT', 34], ['RESAMP', 33], ['SYNC', 37], ['OPTION', 8], ['UNDO', 40], ['EQ', 41], ['SCOPE', 42]]],
};
let hubSel = { 16: 0, 17: 0, 18: 0 }, navStack = [], hubReturn = 0, mixPage = 0;

/* ---- state ---- */
let pads = [], master = MASTER_DEF.slice();
let steps = [];                 /* [pat][trk][step] = [on, vel, semi, prob] */
const growTrk = new Array(NTRK).fill(false), growG0 = new Array(NTRK).fill(0);            /* auto length: a clip recorded from empty grows by 16 steps whenever you play into its last beat */
let lens = new Array(NTRK).fill(16), mutes = new Array(NTRK).fill(0), lmutes = new Array(NLANE).fill(0), lmSent = new Array(NLANE).fill(0);   /* lmutes = per-sound mute of the drum pads */
let bpm = 120, swing = 0;
let ed = new Array(16).fill(0);           /* edited/launched clip per track */
let clipsCur = new Array(16).fill(0), clipsQ = new Array(16).fill(-2), bank = 0, copySrc = null, prevView = 0, clipInit = false, clipPoll = 0;
let sel = 0, view = 0, stepPage = 0, followHold = 0;
let kind = new Array(NTRK).fill(0), slot = new Array(NTRK).fill(-1), dsel = new Array(NTRK).fill(0), tv = new Array(NTRK).fill(0.67);   /* kind: 0 synth / 1 drum, slot = drum kit 0..3 (kept while the track is a synth again) */
let llen = new Array(NLANE).fill(0);                                 /* own loop length of a lane (drum sound); 0 = follows the track */
function effLen(l) { return llen[l] || lens[laneTrk(l)]; }
function LL() { return effLen(ln()); }
let chosen = new Array(NTRK).fill(0), askKind = -1, askSel = 0;       /* chosen[t] = the Synth/Drum question was answered for that track */
function lanesOf(t) { if (!kind[t]) return [t]; const a = []; for (let k = 0; k < 16; k++) a.push(NTRK + slot[t] * 16 + k); return a; }
function laneOf(t, k) { return kind[t] ? NTRK + slot[t] * 16 + k : t; }
function ln() { return kind[sel] ? NTRK + slot[sel] * 16 + dsel[sel] : sel; }                  /* the lane (sound + steps) being edited */
function laneTrk(l) { if (l < NTRK) return l; const d = (l - NTRK) >> 4; for (let t = 0; t < NTRK; t++) if (kind[t] && slot[t] === d) return t; for (let t = 0; t < NTRK; t++) if (slot[t] === d) return t; return 0; }
function keyLane(k) { return (PI[k] === 45 || PI[k] === 46 || PI[k] === 51 || PI[k] === 52) ? sel : ln(); }                                         /* effects belong to the track, sounds to the lane */
function laneName() { return kind[sel] ? 'D' + (dsel[sel] + 1) + ' ' + TYPE_NAME[Math.round(pads[ln()][0])] : TYPE_NAME[Math.round(pads[sel][0])]; }
function trkName(t) { return kind[t] ? 'DRUM ' + (slot[t] + 1) : 'SYNTH'; }
function getTV(t) { return kind[t] ? tv[t] : pads[t][PI.vol]; }
function setTV(t, v) { if (kind[t]) { tv[t] = v; enqueue('tv' + t, 'tvol', t + ',' + (v * 1.5).toFixed(3)); } else { pads[t][PI.vol] = v; enqueue('p' + t + 'vol', 'p' + p2(t + 1) + '_vol', v.toFixed(3)); } }
const REP_NAME = ['1/2', '1/4.', '1/4', '1/8.', '1/4T', '1/8', '1/16.', '1/8T', '1/16', '1/32.', '1/16T', '1/32', '1/32T', '1/64', '1/64T', '1/128'];
let repRate = 8;                                            /* note repeat speed 0..15 (slow to fast), set on the right pads or on the REPEAT page */
function setRepRate(r, toast) { repRate = clamp(r, 0, 15); enqueue('reprate', 'reprate', String(repRate)); markEdited(); if (toast !== false) showToast('NOTE REPEAT', REP_NAME[repRate], repRate / 15); needsRedraw = true; }
let octS = new Array(16).fill(0);                           /* pad octave per track (Up / Down) */
function oct() { return 12 * octS[sel]; }
function syncSel() { enqueue('sel', 'sel', String(ln())); enqueue('padoct', 'padoct', String(oct())); }
let menuGrp = -1, menuSub = 0;
let menuOpen = false, menuIdx = 0, browseAll = false, rqOff = 0.5, trkHeld = -1, trkUsed = false;
let topModes = new Array(16).fill(0), sessDown = 0, sessPeek = false, hitLog = [], muteUsed = false, soloAtPress = false, recDown = false, recStepUsed = false, recHead = 0, fullVel = false, loopHeld = false, euK = new Array(16).fill(4), euR = new Array(16).fill(0), cpuAvg = 0, cpuPeak = 0, padsOffSent = -1, padMenu = false, prevPM = 0, backTicks = 0, ledAge = 0, ledRot = 0, resyncT = 0, resyncTrk = 0, resyncSub = 0, tapT = [];
let playing = false, gs = 0, recArmed = false, phase = 0;
let recSwallow = false, shiftHeld = false, copyHeld = false, deleteHeld = false, muteHeld = false;
let delAge = 0;
let fb = null, fbT = 0, fbOff = null, fbPend = 0;                      /* sound / kit folder browser */
let heldStep = -1, heldWasOn = false, heldEdited = false, heldGrp = [];      /* heldGrp: more steps held together with the first one: knobs edit all of them */
let acc = [0,0,0,0,0,0,0,0];
let needsRedraw = true, toast = null;
let queue = [];                 /* [uniqueKey, dspKey, value] */
let ledCache = {}, buttonCache = {};
let ticksSinceEdit = 100000, dirty = false, healTicks = 0, initialPush = 0, posFail = 0;

const p2 = n => (n < 10 ? '0' : '') + n;
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

/* ---- data helpers ---- */
function newSteps() {
    const out = [];
    for (let p = 0; p < NPAT; p++) { const tr = []; for (let t = 0; t < NTRK; t++) tr.push(blankLane()); out.push(tr); }
    return out;
}
function blankLane() { const st = []; for (let s = 0; s < MAXSTEP; s++) st.push([0, 100, 0, 0]); return st; }
function ensureSlot(d) { for (let p = 0; p < NPAT; p++) for (let k = 0; k < 16; k++) if (!steps[p][NTRK + d * 16 + k]) steps[p][NTRK + d * 16 + k] = blankLane(); }
function laneHas(p, l) { return !!steps[p][l] && steps[p][l].some(s => s[0]); }
function activeLanes() { const a = []; for (let t = 0; t < NTRK; t++) a.push(t); for (let t = 0; t < NTRK; t++) if (kind[t]) for (const l of lanesOf(t)) a.push(l); return a; }
function defaultPads() {
    const lay = []; for (let l = 0; l < NLANE; l++) lay.push(l < NTRK ? LAYOUT[l] : DRUM_LAYOUT[(l - NTRK) & 15]);
    return lay.map(l => { const a = [l[0], 0.8, 0.5, l[1], l[2], 1.0, 0.0, l[5], l[6], l[3], l[4], 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]; while (a.length < PK.length) a.push(defOf(a.length)); return a; });
}
function hex2(n) { return (n < 16 ? '0' : '') + n.toString(16); }
function encStep(st) {                                    /* 12 hex chars: on+vel, prob, semi+64, ratchet|gate<<2|cond<<5, nudge, lock. The 4 spare bits (prob bits 5-6, nudge bit 7, lock bit 7) hold the exact step length 1..15 */
    const x = ((st[4] | 0) & 3) | (((st[5] | 0) & 7) << 2) | (((st[7] | 0) & 7) << 5), g = clamp(st[9] | 0, 0, 15);
    return hex2((st[0] ? 0x80 : 0) | (st[1] & 0x7F)) + hex2(0x80 | (clamp(st[3] | 0, 0, 31) & 0x1F) | ((g & 1) << 5) | (((g >> 1) & 1) << 6)) + hex2(clamp(st[2], -64, 63) + 64) + hex2(x) + hex2(0x40 | (clamp(st[6] | 0, 0, NUD_MAX) & 0x3F) | (((g >> 2) & 1) << 7)) + hex2(((st[8] | 0) & 0x7F) | (((g >> 3) & 1) << 7));
}
function decProb(b) { return (b & 0x80) ? (b & 0x1F) : ((((b & 7) << 1) | ((b >> 3) & 1)) << 1); }
function decStep(h) {
    const a = parseInt(h.substr(0, 2), 16), b = parseInt(h.substr(2, 2), 16), c = parseInt(h.substr(4, 2), 16);
    if (h.length >= 10) {
        const x = parseInt(h.substr(6, 2), 16) || 0, n = parseInt(h.substr(8, 2), 16) || 0;
        const r = [(a & 0x80) ? 1 : 0, (a & 0x7F) || 100, (c || 0) - 64, decProb(b), x & 3, (x >> 2) & 7, (n & 0x40) ? (n & 0x3F) : (n & 7) * 4, (x >> 5) & 7];
        const lkb = h.length >= 12 ? (parseInt(h.substr(10, 2), 16) || 0) : 0, lk = lkb & 0x7F;
        if (lk) r.push(lk);
        if (b & 0x80) { const gl = ((b >> 5) & 1) | (((b >> 6) & 1) << 1) | (((n >> 7) & 1) << 2) | (((lkb >> 7) & 1) << 3); if (gl) { while (r.length < 9) r.push(0); r.push(gl); } }
        return r;
    }
    return [(a & 0x80) ? 1 : 0, (a & 0x7F) || 100, (c || 0) - 64, decProb(b)];
}
function decStepOld(h) {                                  /* states saved before v1.16.2: 4 hex chars, 5-bit semitone */
    const a = parseInt(h.substr(0, 2), 16), b = parseInt(h.substr(2, 2), 16);
    return [(a & 0x80) ? 1 : 0, (a & 0x7F) || 100, (b & 31) - 16, (b >> 5) * 4];
}
function encTrack(p, t) { return steps[p][t].map(encStep).join(''); }
function trackHasSteps(p, t) { if (kind[t]) { for (const l of lanesOf(t)) if (laneHas(p, l)) return true; return false; } return laneHas(p, t); }
function stepOnAt(p, t, s) { for (const l of lanesOf(t)) if (steps[p][l][s][0]) return true; return false; }
const LOCK_KEYS = ['cut', 'res', 'fenv', 'tune', 'decay', 'tone', 'drive', 'pan', 'vol', 'locut'], LOCK_NAME = ['LPCut', 'Reso', 'FltEnv', 'Tune', 'Decay', 'Tone', 'Drive', 'Pan', 'Vol', 'HPCut'];
let ciOn = true, ciBeats = 0;                                   /* count-in before recording starts */
const FOL_NAME = ['OFF', 'NEXT', 'PREV', 'FIRST', 'RANDOM', 'STOP'], FOL_LOOPS = [1, 2, 3, 4, 8, 16];
const fol = Array.from({ length: NTRK }, () => new Array(NPAT).fill(0)), folSent = Array.from({ length: NTRK }, () => new Array(NPAT).fill(0));   /* follow action per clip: bits 0-2 action, 3-5 loops */
let sceneNames = new Array(NPAT).fill(''), sceneQ = new Array(NPAT).fill(0); const SQ_NAME = ['GLOBAL', 'NOW', 'BEAT', 'BAR', '4 BARS'];
let abSlot = null, morphV = 0, morphBase = null, monoChk = false, monoSaved = 0.5, bakT = Date.now();
function qFol(t, c) { enqueue('fol' + t + '_' + c, 'fol', t + ',' + c + ',' + fol[t][c]); folSent[t][c] = fol[t][c]; }
function folCycle(which) {
    const t = sel, c = ed[t]; let a = fol[t][c] & 7, li = (fol[t][c] >> 3) & 7; if (li >= FOL_LOOPS.length) li = 0;
    if (which === 0) a = (a + 1) % FOL_NAME.length; else li = (li + 1) % FOL_LOOPS.length;
    fol[t][c] = a | (li << 3); qFol(t, c); markEdited(); needsRedraw = true;
    showToast('FOLLOW CLIP ' + (c + 1), a ? FOL_NAME[a] + ' AFTER ' + FOL_LOOPS[li] + 'X' : 'OFF', a / 5);
}
let recent = [], lockT = new Array(NLANE).fill(0), favs = [], favOnly = false, cpuWarnT = 0, smpMem = 0;
function qLockT(l) { enqueue('lt' + l, 'plkt', l + ',' + (lockT[l] ? PI[LOCK_KEYS[lockT[l]]] : (laneAnyLock(l) ? PI[LOCK_KEYS[0]] : 0))); }
function laneAnyLock(l) { for (let p = 0; p < NPAT; p++) for (let s = 0; s < MAXSTEP; s++) { const st = steps[p] && steps[p][l] && steps[p][l][s]; if (st && st[8]) return true; } return false; }
function stepAt(s) { const st = steps[ed[sel]][ln()][s]; while (st.length < 10) st.push(0); return st; }
function gateTxt(st) { return st[9] > 0 ? st[9] + ' STEPS' : GATE_NAME[st[5]]; }
const GATE_NAME = ['AUTO','1/4','1/2','1','2','4','8','8'], COND_NAME = ['ALL','1:2','2:2','1:3','2:3','3:3','1:4','4:4'];

/* ---- DSP bridge ---- */
function dspSet(key, val) { if (typeof host_module_set_param === 'function') host_module_set_param(key, String(val)); }
function midiExport(all) {                                  /* writes a standard MIDI file (one track per sound) next to the project: the DSP does the writing */
    menuOpen = false; needsRedraw = true;
    for (let i = 0; i < 2000 && queue.length; i++) flushQueue(50);      /* every step must be in the engine first */
    const clip = all ? -1 : ed[sel], path = '/data/UserData/schwung/taktile-' + (all ? 'all-clips' : 'clip-' + (clip + 1)) + '.mid';
    dspSet('midiexp', path + '|' + clip);
    const r = String(dspGet('midiexp') || ''), a = r.split(',');
    if (a[0] === 'ok') showToast('MIDI EXPORT', a[1] + ' TRACKS', 1);
    else showToast('MIDI EXPORT', a[0] === 'error' ? (a[1] || 'FAILED').toUpperCase() : 'FAILED', 0);
}
function dspGet(key) { return (typeof host_module_get_param === 'function') ? host_module_get_param(key) : null; }
function enqueue(uk, dk, val) {
    for (let i = 0; i < queue.length; i++) if (queue[i][0] === uk) { queue[i][2] = val; return; }
    queue.push([uk, dk, val]);
}
function flushPad(l) {                                    /* a pad that is hit must already have its new sound: send its waiting sound data first */
    for (let i = 0; i < queue.length; i++) if (queue[i][0] === 'setpad' + l) { const q = queue.splice(i, 1)[0]; dspSet(q[1], q[2]); break; }
}
function padStr(v) { return v.map((x, k) => (k === 0 || k === 9 || k === 10 || k === 19) ? String(Math.round(x)) : String(+x.toFixed(3))).join(','); }
/* every clip keeps its own sound: csnd[clip][track] = full pad row (volume + pan stay track-wide) */
let csnd = null;
function ensureCs() {
    if (csnd) return;
    csnd = []; for (let c = 0; c < NPAT; c++) { const row = []; for (let t = 0; t < NLANE; t++) row.push(pads[t].slice()); csnd.push(row); }
}
function sendCs(c, t) { enqueue('cs' + c + '_' + t, 'csnd', c + ',' + t + ':' + padStr(csnd[c][t])); }
function storeCs(l) { ensureCs(); const t = laneTrk(l); csnd[ed[t]][l] = pads[l].slice(); sendCs(ed[t], l); }
function liveOk(t) { return !playing || clipsCur[t] < 0 || clipsCur[t] === ed[t]; }      /* edits reach the engine only while that clip is the one sounding */
function applyClipSound(t, c) {                         /* UI follows the clip: its sound, track volume + pan kept */
    ensureCs();
    for (const l of lanesOf(t)) { const v = pads[l][1], p = pads[l][2]; pads[l] = csnd[c][l].slice(); pads[l][1] = v; pads[l][2] = p; }
}
function qPadLive(i) { enqueue('setpad' + i, 'setpad', (i + 1) + ':' + padStr(pads[i])); }
function qPad(i) { storeCs(i); if (liveOk(laneTrk(i))) qPadLive(i); }
function qLay() { enqueue('laysyn', 'laysyn', String(layS)); }      /* the engine plays the pads of the inbound MIDI route with the same layout as the sequencer records them */
function qMaster() { MK.forEach((k, i) => enqueue('m' + k, k, (i >= 1 && i <= 3) || i === 21 || i === 24 ? String(Math.round(master[i])) : master[i].toFixed(3))); }
function qTransport() {
    enqueue('bpm', 'bpm', String(bpm)); enqueue('swing', 'swing', (0.5 + swing / 100).toFixed(3));
    for (let t = 0; t < NTRK; t++) { enqueue('kind' + t, 'kind', t + ',' + (kind[t] ? slot[t] : -1)); enqueue('tv' + t, 'tvol', t + ',' + (tv[t] * 1.5).toFixed(3)); }
    syncSel();
    if (!clipInit) { clipInit = true; for (let t = 0; t < NTRK; t++) enqueue('clip' + t, 'clip', t + ',' + ed[t]); }
    for (let t = 0; t < NTRK; t++) { enqueue('len' + t, 'len', t + ',' + lens[t]); enqueue('mute' + t, 'mute', t + ',' + mutes[t]); }
}
function qSeq(p, t) {                                      /* long patterns go out in chunks of 32 steps */
    const h = encTrack(p, t);
    for (let o = 0; o < MAXSTEP; o += 16) enqueue('seq' + p + '_' + t + '_' + o, 'seqd', p + ',' + t + ',' + o + ':' + h.substr(12 * o, 192));
}
function setLen(n, wholeTrack) {                           /* loop length: a drum sound has its own, otherwise (or with wholeTrack) the track's */
    growTrk[sel] = false;
    n = clamp(n, 1, MAXSTEP); const L = ln();
    { const top = undoStack.length ? undoStack[undoStack.length - 1] : null, now = Date.now(); if (!(top && !top.k && top.lt && top.t === L && top.p === ed[sel] && now - top.lt < 1500)) { snap(ed[sel], L); undoStack[undoStack.length - 1].lt = now; } else top.lt = now; }      /* a loop length change is an undo step (turning the wheel merges) */
    if (kind[sel] && !wholeTrack) { llen[L] = n; enqueue('llen' + L, 'llen', L + ',' + n); }
    else { lens[sel] = n; enqueue('len' + sel, 'len', sel + ',' + n); }
    stepPage = Math.min(stepPage, pageCount() - 1);
    showToast(kind[sel] && !wholeTrack ? 'SOUND LOOP' : 'TRACK LOOP', n + ' STEPS', n / MAXSTEP); markEdited();
}
function pageCount() { return Math.max(1, Math.ceil(LL() / 16)); }
function qSeqT(p, t) { for (const l of lanesOf(t)) qSeq(p, l); }
function qStep(p, t, s) { enqueue('st' + p + '_' + t + '_' + s, 'st', p + ',' + t + ',' + s + ',' + encStep(steps[p][t][s])); }
function qEverything() {
    for (let l = 0; l < NLANE; l++) if (lmutes[l] || lmSent[l]) { enqueue('lmute' + l, 'lmute', l + ',' + lmutes[l]); lmSent[l] = lmutes[l]; }
    qMaster(); qTransport(); qLay(); enqueue('reprate', 'reprate', String(repRate));
    if (moClk || moMask) enqueue('midiout', 'midiout', moStr());
    ensureCs();
    const al = activeLanes();
    for (const i of al) { qPadLive(i); for (let c = 0; c < NPAT; c++) sendCs(c, i); }
    for (const l of al) if (llen[l]) enqueue('llen' + l, 'llen', l + ',' + llen[l]);
    for (const l of al) qLockT(l);
    for (let t = 0; t < NTRK; t++) for (let c = 0; c < NPAT; c++) if (fol[t][c] !== folSent[t][c]) qFol(t, c);
    for (let c = 0; c < NPAT; c++) if (sceneQ[c]) enqueue('sq' + c, 'sceneq', c + ',' + sceneQ[c]);
    for (let p = 0; p < NPAT; p++) for (const l of al) if (laneHas(p, l)) qSeq(p, l);
}
function flushQueue(n) { for (let i = 0; i < n && queue.length; i++) { const q = queue.shift(); dspSet(q[1], q[2]); } }

/* ---- persistence ---- */
function saveState() {
    try {
        if (typeof host_write_file !== 'function') return;
        const seqHex = [];
        for (let p = 0; p < NPAT; p++) { const row = []; for (let l = 0; l < NLANE; l++) row.push(laneHas(p, l) ? encTrack(p, l) : ''); seqHex.push(row); }
        const keep = {}; for (let t = 0; t < NTRK; t++) { keep[t] = 1; if (slot[t] >= 0) for (let k = 0; k < 16; k++) keep[NTRK + slot[t] * 16 + k] = 1; }
        const cs = csnd.map(row => row.map((r, l) => keep[l] ? r : null));
        host_write_file(STATE_FILE, JSON.stringify({ v: 2, mg: 1, qg: [qGrid, qStr], fx: 2, pf: 2, sf: 12, lm: lmutes, rr: repRate, pads, master: (monoChk ? master.map((v, i) => i === MI.m_wid ? monoSaved : v) : master), seq: seqHex, lens, mutes, bpm, swing, ed, pm: topModes, cb: chBak, ba: browseAll, rq: rqOff, cs: cs, kd: kind, ch: chosen, ll: llen, sl: slot, ds: dsel, tv: tv, ly: [layS, layD], sm: slm, au: AUTO, sg: { on: songOn, l: songList, lp: songLoop ? 1 : 0 }, mo: [moClk, moMask, moChn, moXp, moVp, moDn], eu: [euV], ep: euP, bp: broPath, tc: tcolO, rn: [rndDens, rndVar, rndKey, rndRat], lt: lockT, fk: favK, fv: favs, rc: recent, fo: favOnly ? 1 : 0, ui: [splashOn ? 1 : 0, idleOn ? 1 : 0, dispInv ? 1 : 0, repAllowed ? 1 : 0, slowKnobs ? 1 : 0, knobLedsOn ? 1 : 0, uiTheme, bigOn ? 1 : 0, slideOn ? 1 : 0, ciOn ? 1 : 0, knobLvlOn ? 1 : 0, 1, btnBright ? 1 : 0, lang], fl: fol, sn: sceneNames, sq: sceneQ, se: sel }));
        dirty = false; saveFlash = 30; needsRedraw = true;
    } catch (e) { /* ignore */ }
}
let saveFlash = 0;
let freshState = false;
function loadState() {
    miscLast = null; pads = defaultPads(); steps = newSteps();
    try {
        if (typeof host_read_file !== 'function') return;
        let txt = host_read_file(STATE_FILE);
        if (!txt) txt = host_read_file(OLD_STATE_FILE);
        if (!txt) { freshState = true; return; }
        const o = JSON.parse(txt);
        if (!o || o.v !== 2) { freshState = true; return; }
        if (Array.isArray(o.pads) && (o.pads.length === NTRK || o.pads.length === NLANE)) {
            const loaded = o.pads.map(r => { const a = r.map(Number); while (a.length < PK.length) a.push(defOf(a.length)); return a; });
            for (let l = 0; l < loaded.length; l++) pads[l] = loaded[l];
        }
        if (Array.isArray(o.ly) && o.ly.length === 2) { layS = clamp(Number(o.ly[0]) | 0, 0, 5); layD = clamp(Number(o.ly[1]) | 0, 0, 1); }
        if (Array.isArray(o.mo) && o.mo.length >= 2) { moClk = o.mo[0] ? 1 : 0; moMask = (Number(o.mo[1]) | 0) & 65535; moChn = clamp(Number(o.mo[2]) | 0, 0, 15); moXp = clamp(Number(o.mo[3]) | 0, -24, 24); moVp = clamp(Number(o.mo[4]) | 0, -75, 50); moDn = clamp(Number(o.mo[5]) | 0, -12, 36); }
        if (Array.isArray(o.eu) && o.eu.length === 1) euV = clamp(Number(o.eu[0]) | 0, 1, 127);
        if (Array.isArray(o.tc) && o.tc.length === NTRK) tcolO = o.tc.map(x => clamp(Number(x) | 0, 0, 26));
        for (let t = 0; t < NTRK; t++) for (let c = 0; c < NPAT; c++) fol[t][c] = (Array.isArray(o.fl) && Array.isArray(o.fl[t])) ? clamp(Number(o.fl[t][c]) | 0, 0, 63) : 0;
        lockT = new Array(NLANE).fill(0); if (Array.isArray(o.lt)) for (let l = 0; l < NLANE && l < o.lt.length; l++) lockT[l] = clamp(Number(o.lt[l]) | 0, 0, LOCK_KEYS.length - 1);
        if (Array.isArray(o.fk) && o.fk.length === 8) favK = o.fk.map(String).map(k => FAV_MAP[k] || k).filter(k => PI[k] !== undefined || MI[k] !== undefined); if (favK.length !== 8) favK = ['cut', 'res', 'decay', 'tone', 'drive', 'dly', 'rvb', 'm_filt']; favBuild(); favs = Array.isArray(o.fv) ? o.fv.map(Number).filter(x => x >= 0 && x < TYPE_NAME.length) : []; favOnly = !!o.fo; if (Array.isArray(o.ui)) { splashOn = o.ui[0] !== 0; idleOn = o.ui[1] === 1; dispInv = o.ui[2] === 1; repAllowed = o.ui[3] !== 0; slowKnobs = o.ui[4] === 1; knobLedsOn = o.ui[5] !== 0; uiTheme = o.ui[11] === 1 ? clamp(Math.round(Number(o.ui[6]) || 0), 0, 9) : 9; uiMin = uiTheme !== 0; bigOn = o.ui[7] === 1; slideOn = o.ui[8] === 1; ciOn = o.ui[9] !== 0; knobLvlOn = o.ui[10] !== 0; btnBright = o.ui[12] === 1; lang = clamp(Math.round(Number(o.ui[13]) || 0), 0, LANGS.length - 1); if (!repAllowed) for (let t = 0; t < NTRK; t++) if (topModes[t] === 1) topModes[t] = 0; } recent = Array.isArray(o.rc) ? o.rc.map(Number).filter(x => x >= 0 && x < TYPE_NAME.length).slice(0, 12) : [];
        if (Array.isArray(o.rn) && o.rn.length === 4) { rndDens = clamp(Number(o.rn[0]) || 0.28, 0.02, 0.9); rndVar = clamp(Number(o.rn[1]) || 0.5, 0, 1); rndKey = o.rn[2] ? 1 : 0; rndRat = clamp(Number(o.rn[3]) || 0, 0, 1); }
        if (typeof o.bp === 'string' && o.bp.charAt(0) === '/' && o.bp.length < 200) broPath = o.bp;
        if (Array.isArray(o.ep)) for (let i = 0; i < NLANE && i < o.ep.length; i++) { const q = o.ep[i] || {}, d = euDef(); euP[i] = { eo: clamp(q.eo | 0, 0, 2), n0: clamp(q.n0 | 0, -24, 24), alt: clamp(q.alt | 0, -12, 12), an: clamp(q.an | 0, -12, 12), wk: clamp(q.wk | 0, -4, 4), wl: clamp(q.wl | 0, 1, 16) || 4, rn: clamp(q.rn | 0, 0, 12), ou: clamp(q.ou | 0, 0, 100), od: clamp(q.od | 0, 0, 100), keep: q.keep ? 1 : 0, rev: q.rev ? 1 : 0, inv: q.inv ? 1 : 0, acc: clamp(q.acc | 0, 0, 60), hum: clamp(q.hum | 0, 0, 40), mut: clamp(q.mut | 0, 0, 100), nud: clamp((q.nud | 0) * (o.pf === 2 ? 1 : 4), 0, NUD_MAX), prb: clamp((q.prb | 0) * (o.pf === 2 ? 1 : (o.pf === 1 ? 2 : 4)), 0, 31), gat: clamp(q.gat | 0, 0, 6), rat: clamp(q.rat | 0, 0, 3), cond: clamp(q.cond | 0, 0, 7), seed: clamp(q.seed | 0, 1, 9972) || d.seed, q: Array.isArray(q.q) && q.q.length === 4 ? q.q.map((z, k) => { z = z || {}; const ln0 = clamp(z.len | 0, 1, 64) || EU_LDEF[k][0]; return { on: z.on ? 1 : 0, len: ln0, hit: clamp(z.hit | 0, 0, ln0), rot: clamp(z.rot | 0, 0, ln0 - 1), not: clamp(z.not | 0, -24, 24), prb: clamp(z.prb | 0, 0, 31), hum: clamp(z.hum | 0, 0, 40), acc: clamp(z.acc | 0, 0, 60) }; }) : null }; }
        if (o.au && typeof o.au === 'object') { AUTO = {}; for (const id in o.au) if (/^\d+\|\d+\|[a-z]+$/.test(id) && o.au[id] && typeof o.au[id] === 'object') { const m = {}; for (const k in o.au[id]) { const v = Number(o.au[id][k]); if (isFinite(v)) m[k] = clamp(v, 0, 12); } AUTO[id] = m; } }
        if (o.sg && Array.isArray(o.sg.l) && o.sg.l.length >= 1 && o.sg.l.length <= 32) { songList = o.sg.l.map(songNorm); songOn = !!o.sg.on; songLoop = o.sg.lp === undefined ? true : !!o.sg.lp; songSlot = 0; songBlank = o.sg.l.length === 1 && o.sg.l[0][0] === 0 && o.sg.l[0][1] === 4 && !songOn; }
        if (Array.isArray(o.sm) && o.sm.length >= 1 && o.sm.length <= 16) { slm = o.sm.map(x => clamp(Number(x) || 0, 0, 0.998)).sort((a, b) => a - b); slN = slm.length; slSel = 0; }
        if (Array.isArray(o.ll) && o.ll.length === NLANE) llen = o.ll.map(x => clamp(Number(x) | 0, 0, MAXSTEP));
        chosen = (Array.isArray(o.ch) && o.ch.length === NTRK) ? o.ch.map(x => x ? 1 : 0) : new Array(NTRK).fill(1);
        if (Array.isArray(o.kd) && o.kd.length === NTRK && Array.isArray(o.sl) && o.sl.length === NTRK) {
            kind = o.kd.map(x => x ? 1 : 0); slot = o.sl.map(x => clamp(Number(x) | 0, -1, 3));
            for (let t = 0; t < NTRK; t++) if (kind[t] && slot[t] < 0) kind[t] = 0;
            if (Array.isArray(o.ds) && o.ds.length === NTRK) dsel = o.ds.map(x => clamp(Number(x) | 0, 0, 15));
            if (Array.isArray(o.tv) && o.tv.length === NTRK) tv = o.tv.map(x => clamp(Number(x) || 0, 0, 1));
            for (let t = 0; t < NTRK; t++) if (slot[t] >= 0) ensureSlot(slot[t]);
        }
        if (Array.isArray(o.master) && o.master.length >= 9) { master = o.master.map(Number); while (master.length < MASTER_DEF.length) master.push(MASTER_DEF[master.length]); }
        if (Array.isArray(o.seq)) for (let p = 0; p < NPAT; p++) for (let t = 0; t < NLANE; t++) {
            const h = o.seq[p] && o.seq[p][t];
            if (h && steps[p][t]) { const w = o.sf === 12 ? 12 : (o.sf === 10 ? 10 : (o.sf === 6 ? 6 : 4)); for (let s = 0; s < Math.min(MAXSTEP, Math.floor(h.length / w)); s++) steps[p][t][s] = w >= 10 ? decStep(h.substr(w * s, w)) : (w === 6 ? decStep(h.substr(6 * s, 6)) : decStepOld(h.substr(4 * s, 4))); }
        }
        if (Array.isArray(o.cs) && o.cs.length === NPAT && o.cs.every(row => Array.isArray(row) && (row.length === NTRK || row.length === NLANE))) {
            csnd = o.cs.map(row => { const out = []; for (let l = 0; l < NLANE; l++) { const a = row[l]; if (Array.isArray(a)) { const b = a.map(Number); while (b.length < PK.length) b.push(defOf(b.length)); out.push(b); } else out.push(pads[l].slice()); } return out; });
        }
        if (o.fx !== 2) {                                                    /* older saves: effects were per track; every pad of a drum track starts with a copy */
            for (let t = 0; t < NTRK; t++) if (kind[t] && slot[t] >= 0) for (let k = 0; k < 16; k++) {
                const l = NTRK + slot[t] * 16 + k;
                for (let i = o.fx ? PI.mfx : PI.dtime; i <= PI.mprm; i++) pads[l][i] = pads[t][i];
                if (Array.isArray(csnd)) for (const row of csnd) if (row && row[t] && row[l]) for (let i = o.fx ? PI.mfx : PI.dtime; i <= PI.mprm; i++) row[l][i] = row[t][i];
            }
        }
        if (o.mg !== 1) {                                                    /* 5.65.3: MIDI effects and Euclid lanes start OFF everywhere (once, for older saves) */
            for (const r of pads) { r[PI.mfx] = 0; r[PI.m2fx] = 0; }
            if (Array.isArray(csnd)) for (const row of csnd) if (row) for (const r of row) if (r) { r[PI.mfx] = 0; r[PI.m2fx] = 0; }
            for (const P of euP) if (P && Array.isArray(P.q)) for (const l of P.q) l.on = 0;
        }
        if (typeof o.rr === 'number') repRate = clamp(o.rr | 0, 0, 15);
        if (Array.isArray(o.sn)) sceneNames = new Array(NPAT).fill('').map((_, i) => typeof o.sn[i] === 'string' ? o.sn[i].slice(0, 12) : '');
        if (Array.isArray(o.sq)) sceneQ = new Array(NPAT).fill(0).map((_, i) => clamp(Number(o.sq[i]) | 0, 0, 4));
        if (typeof o.se === 'number') sel = clamp(o.se | 0, 0, NTRK - 1);
        if (Array.isArray(o.qg)) { qGrid = clamp(o.qg[0] | 0, 0, 5); qStr = clamp(o.qg[1] | 0, 0, 3); } browseAll = !!o.ba; if (typeof o.rq === 'number') rqOff = clamp(o.rq, 0, 1);
        if (Array.isArray(o.pm) && o.pm.length === NTRK) topModes = o.pm.map(x => clamp(Number(x) || 0, 0, 4));
        if (!repAllowed) for (let t = 0; t < NTRK; t++) if (topModes[t] === 1) topModes[t] = 0;
        chBak = (Array.isArray(o.cb) && o.cb.length === NTRK) ? o.cb.map(x => (Array.isArray(x) && x.length === 3) ? x.map(Number) : null) : new Array(NTRK).fill(null); for (let t = 0; t < NTRK; t++) { if (kind[t] || topModes[t] !== 4) { topModes[t] = (topModes[t] === 4 ? 0 : topModes[t]); chBak[t] = null; } chOn[t] = topModes[t] === 4 ? 1 : 0; }
        if (Array.isArray(o.lens)) lens = o.lens.map(Number);
        if (Array.isArray(o.mutes)) mutes = o.mutes.map(Number);
        lmutes = new Array(NLANE).fill(0); if (Array.isArray(o.lm)) for (let l = 0; l < NLANE && l < o.lm.length; l++) lmutes[l] = o.lm[l] ? 1 : 0;
        bpm = clamp(Number(o.bpm) || 120, 40, 240); swing = clamp(Number(o.swing) || 0, 0, 25); if (Array.isArray(o.ed) && o.ed.length === NTRK) ed = o.ed.map(x => clamp(Number(x) || 0, 0, NPAT - 1));
    } catch (e) { /* defaults */ }
}
function markEdited() { needsRedraw = true; dirty = true; ticksSinceEdit = 0; healTicks = 40; }

/* ---- value helpers ---- */
function scopeOf(key) { return PI[key] !== undefined ? 'pad' : (MI[key] !== undefined ? 'master' : 'sys'); }
function getVal(scope, key) {
    if (scope === 'sys') { if (key === 'qgrid') return qGrid / 5; if (key === 'qstr') return qStr / 3; if (key === 'qtime') return 0; if (key === 'morph') return morphV; if (key === 'bpm') return (bpm - 40) / 200; if (key === 'swing') return swing / 25; if (key === 'fullvel') return fullVel ? 1 : 0; if (key === 'repmode') return topModes[sel] === 1 ? 1 : 0; if (key === 'browse') return browseAll ? 1 : 0; if (key === 'rqoff') return rqOff; if (key === 'rrate') return repRate / 15; if (key === 'kind') return kind[sel]; if (key === 'tvol') return getTV(sel); if (key === 'pmode') return Math.min(1, topModes[sel] / 3); if (key === 'dsel') return dsel[sel] / 15; if (key === 'lay') return kind[sel] ? layD : layS / 3; if (key === 'snorm') return 0; if (key === 'rsrc') return rsSrc / 16; if (key === 'rbar') return rsBarI / 4; if (key === 'rgo') return rsState ? 1 : 0; if (key === 'rst') return rsPct / 100;
        if (key === 'kslot') return kitSlot / (15 + FACTORY.length); if (key === 'ksave' || key === 'kload' || key === 'psave' || key === 'pload') return 0; if (key === 'kinf') return kitInfo() === 'EMPTY' ? 0 : 1;
        if (key === 'rdens') return rndDens; if (key === 'rvar') return rndVar; if (key === 'favo') return favOnly ? 1 : 0; if (key === 'kprev') return 0; if (key === 'rkey') return rndKey; if (key === 'rrat') return rndRat; if (key === 'tcol') return (tcolO[sel] || 0) / 26; if (key === 'pnew' || key === 'prst') return 0; if (key === 'pslot') return projSlot / 31; if (key === 'pinf') return projUsed[projSlot] ? 1 : 0;
        if (EU_KEYS[key]) return euGet(key)[0];
        if (key === 'moclk') return moClk; if (key === 'motn') return moTrk / 15; if (key === 'mont') return (moMask >> moTrk) & 1; if (key === 'moall') return moMask === 65535 ? 1 : 0; if (key === 'mochn') return moChn / 15; if (key === 'moxp') return (moXp + 24) / 48; if (key === 'movel') return (moVp + 75) / 125; if (key === 'modn') return (moDn + 12) / 48;
        if (key === 'sgsl') return songSlot / 15; if (key === 'sgcl') return songList[songSlot] ? songList[songSlot][0] / 7 : 0; if (key === 'sgbr') return songList[songSlot] ? songList[songSlot][1] / 32 : 0; if (key === 'sgrp') return songList[songSlot] ? (songList[songSlot][2] - 1) / 15 : 0; if (key === 'sgtp') return songList[songSlot] ? songList[songSlot][3] / 240 : 0; if (key === 'sgtr') return songList[songSlot] ? songList[songSlot][4] / 2 : 0; if (key === 'sgtg') return songList[songSlot] ? songList[songSlot][5] / 7 : 0; if (key === 'sgon') return songOn ? (songLoop ? 1 : 0.5) : 0; if (key === 'sglp') return songLoop ? 1 : 0; if (key === 'sgin' || key === 'sgdl' || key === 'sgmv') return 0; if (key === 'slm') return slSel / 15; if (key === 'slp') return slm[slSel]; if (key === 'sln') return (slN - 1) / 15; if (key === 'sla') return 0; return 0; }
    return scope === 'pad' ? pads[keyLane(key)][PI[key]] : master[MI[key]];
}
function fmtVal(key, v) {
    if (key === 'bpm') return String(Math.round(40 + v * 200));
    if (key === 'swing') return String(50 + Math.round(v * 25));
    if (key === 'fullvel' || key === 'repmode') return v > 0.5 ? 'ON' : 'OFF';
    if (key === 'browse') return favOnly ? 'FAV' : (v > 0.5 ? 'ALL' : 'CATEG');
    if (key === 'm_met') return v > 0.05 ? String(Math.round(v * 100)) : 'OFF';
    if (key === 'm_qnt') return ['1/16', '1/8', '1/4', '1/2', 'BAR', '2 BAR', '4 BAR'][Math.min(6, Math.floor(v * 7))];
    if (key === 'kind') return v > 0.5 ? 'DRUM' : 'SYN';
    if (key === 'gtn') return ['RECT', 'TRI', 'SINE', 'GAUSS', 'HANN'][clamp(Math.round(v * 4), 0, 4)];
    if (key === 'dtn' || key === 'dly_tone' || key === 'gdn' || key === 'grv') return Math.round(v * 100) + '%';
    if (key === 'dwd' || key === 'rwd' || key === 'dly_wide' || key === 'rvb_wide') return v < 0.03 ? 'MONO' : Math.round(v * 200) + '%';
    if (key === 'rlc' || key === 'rvb_low') return v < 0.02 ? 'OFF' : Math.round(30 + v * v * 600) + 'Hz';
    if (key === 'gfz') return v > 0.5 ? 'FREEZE' : 'LIVE';
    if (key === 'trn') { const st = Math.round((v - 0.5) * 48); return (st > 0 ? '+' : '') + st; }
    if (key === 'mch' || key === 'orn' || key === 'vhm' || key === 'vmn' || key === 'vmx' || key === 'pcv') return Math.round(v * 100) + '%';
    if (key === 'mvb') { const d = Math.round((v - 0.5) * 80); return (d > 0 ? '+' : '') + d; }
    if (key === 'eqlf') return String(Math.round(100 * Math.pow(3, (v - 0.5) * 2)));
    if (key === 'eqmf') { const f = 1000 * Math.pow(8, (v - 0.5) * 2); return f >= 1000 ? (f / 1000).toFixed(1) + 'k' : String(Math.round(f)); }
    if (key === 'eqhf') return (8 * Math.pow(2, (v - 0.5) * 2)).toFixed(1) + 'k';
    if (key === 'eqq') return Math.pow(4, (v - 0.5) * 2).toFixed(1);
    if (key === 'eqo') { const d = Math.round((v - 0.5) * 24); return (d > 0 ? '+' : '') + d + 'dB'; }
    if (key === 'rfq') return v < 0.01 ? 'AUTO' : Math.round(40 + v * v * 2000) + 'Hz';
    if (key === 'mprm' || key === 'm2prm') { const t = Math.round(pads[ln()][key === 'mprm' ? PI.mfx : PI.m2fx]); if (t === 1) return OCT_NAME[Math.min(3, Math.floor(v * 4))]; if (t === 2) return CHORD_NAME[Math.min(7, Math.floor(v * 8))]; if (t === 3) return ARP_RATE[v < 0.33 ? 0 : (v < 0.66 ? 1 : 2)]; if (t === 11) return DIA_NAME[Math.min(3, Math.floor(v * 4))]; if (t === 10) { const st = Math.round((v - 0.5) * 48); return (st > 0 ? '+' : '') + st; } if (t === 6 || t === 7) return Math.round(Math.min(1, v * 2) * 100) + '%'; if (t === 8) return Math.round(Math.min(1, 0.2 + 1.3 * v) * 100) + '%'; if (t === 9) { const o = Math.round((v - 0.5) * 4); return (o > 0 ? '+' : '') + o + 'OCT'; } }
    if (key === 'pmode') return PM_SHORT[Math.round(v * 3)];
    if (key === 'sgn') return (((v - 0.5) * 6) * 6.02).toFixed(0) + 'dB';
    if (key === 'satk' || key === 'srel') return String(Math.round(v * 100));
    if (key === 'snorm') return 'TURN >';
    if (key === 'sstr') return Math.abs(v - 0.5) < 0.012 ? 'OFF' : Math.pow(2, (v - 0.5) * 2).toFixed(2) + 'x';
    if (key === 'scs') return ['AUTO', 'ALL', 'OFF'][Math.round(v * 2)];
    if (key === 'm_eql' || key === 'm_eqm' || key === 'm_eqh' || key === 'eql' || key === 'eqm' || key === 'eqh') { const d = Math.round((v - 0.5) * 24); return (d > 0 ? '+' : '') + d + 'dB'; }
    if (key === 'tsw') return v < 0.52 ? 'OFF' : '+' + Math.round((v - 0.5) * 2 * 25) + '%';
    if (key === 'gsz') { const ms = 4 * Math.pow(300, v); return ms < 1000 ? Math.round(ms) + 'ms' : (ms / 1000).toFixed(1) + 's'; }
    if (key === 'grn') return v < 0.01 ? 'OFF' : Math.round(v * 100) + '%';
    if (key === 'm_chp') return v < 0.02 ? 'OFF' : String(Math.round(40 + 360 * v));
    if (key === 'favo') return favOnly ? 'ON' : 'OFF';
    if (key === 'kprev') return kitBackup ? '< UNDO >' : 'TURN >';
    if (key === 'm_lim') return v < 0.02 ? 'OFF' : String(Math.round(v * 100));
    if (key === 'morph') return abSlot && abSlot.L === ln() ? Math.round(v * 100) + '%' : 'NO A';
    if (key === 'rsrc') return rsSrc === 0 ? 'MASTER' : 'T' + rsSrc;
    if (key === 'rbar') return String(RS_BARS[rsBarI]);
    if (key === 'rgo') return rsState === 0 ? 'TURN >' : (rsState === 1 ? 'WAIT' : 'REC');
    if (key === 'rst') return rsState === 2 ? rsPct + '%' : (rsState === 1 ? 'ARMED' : 'IDLE');
    if (key === 'kslot') return kitSlot >= 16 ? FACTORY[kitSlot - 16][0] : String(kitSlot + 1);
    if (key === 'ksave') return 'TURN >'; if (key === 'kload') return 'TURN >'; if (key === 'kinf') return kitInfo();
    if (key === 'pslot') return String(projSlot + 1); if (key === 'psave' || key === 'pload') return 'TURN >'; if (key === 'rdens') return Math.round(rndDens * 100) + '%'; if (key === 'rvar') return Math.round(rndVar * 100) + '%'; if (key === 'rkey') return rndKey ? 'IN KEY' : 'FREE'; if (key === 'rrat') return Math.round(rndRat * 100) + '%'; if (key === 'tcol') return tcolO[sel] ? 'C' + tcolO[sel] : 'AUTO'; if (key === 'pnew') return 'TURN >>'; if (key === 'prst') return 'TURN >'; if (key === 'pinf') return projUsed[projSlot] ? 'USED' : 'EMPTY';
    if (EU_KEYS[key]) return euGet(key)[1];
    if (key === 'moclk') return moClk ? 'ON' : 'OFF'; if (key === 'motn') return 'T' + (moTrk + 1); if (key === 'mont') return ((moMask >> moTrk) & 1) ? 'ON' : 'OFF'; if (key === 'moall') return moMask === 65535 ? 'ALL ON' : (moMask ? 'SOME' : 'NONE'); if (key === 'mochn') return 'T1=' + (moChn + 1); if (key === 'moxp') return (moXp > 0 ? '+' : '') + moXp; if (key === 'movel') return (100 + moVp) + '%'; if (key === 'modn') return moNoteName(36 + moDn);
    if (key === 'sgsl') return (songSlot + 1) + '/' + songList.length;
    if (key === 'sgcl') return songList[songSlot] ? 'C' + (songList[songSlot][0] + 1) : '--';
    if (key === 'sgbr') return songList[songSlot] ? String(songList[songSlot][1]) : '--';
    if (key === 'sgrp') return songList[songSlot] ? songList[songSlot][2] + 'x' : '--';
    if (key === 'sgtp') return songList[songSlot] ? (songList[songSlot][3] ? String(songList[songSlot][3]) : 'KEEP') : '--';
    if (key === 'sgtr') return songList[songSlot] ? SONG_TRANS[songList[songSlot][4]] : '--';
    if (key === 'sgtg') return songList[songSlot] ? SONG_TAG[songList[songSlot][5]] : '--';
    if (key === 'sgon') return songOn ? (songLoop ? 'LOOP' : 'ONCE') : 'OFF';
    if (key === 'sglp') return songLoop ? 'LOOP' : 'ONCE';
    if (key === 'sgin') return 'TURN >'; if (key === 'sgdl') return 'TURN >'; if (key === 'sgmv') return 'UP/DOWN';
    if (key === 'slm') return (slSel + 1) + '/' + slN;
    if (key === 'slp') return (v * 100).toFixed(1);
    if (key === 'sln') return String(slN);
    if (key === 'sla' || key === 'slg' || key === 'sladd' || key === 'sldel' || key === 'slsn' || key === 'smfit' || key === 'smhf' || key === 'smdb' || key === 'smrn' || key === 'smnm' || key === 'smrs') return 'TURN >';
    if (key === 'smnx') return '< TURN >';
    if (key === 'smrv') return pads[ln()][PI.smd] > 0.25 && pads[ln()][PI.smd] < 0.75 ? 'ON' : 'OFF';
    if (key === 'smp') { const i = Math.round(v * 63); return smpInfo[i] && smpInfo[i][0] ? String(i + 1) : '--'; }
    if (key === 'sst' || key === 'sen') return String(Math.round(v * 100));
    if (key === 'smd') return ['ONE', 'REV', 'LOOP'][Math.min(2, Math.round(v * 2))];
    if (key === 'fmode') return ['LP', 'BP', 'HP'][Math.min(2, Math.round(v * 2))];
    if (key === 'lay') return kind[sel] ? LAYD_NAME[layD] : LAYS_NAME[layS];
    if (key === 'dsel') return kind[sel] ? String(Math.round(v * 15) + 1) : '-';
    if (key === 'rqoff') return v < 0.4 ? 'EARLY' : (v > 0.6 ? 'LATE' : 'EXACT');
    if (key === 'rrate') return REP_NAME[repRate];
    if (key === 'qscale' || key === 'qtime') return 'TURN >';
    if (key === 'qgrid') return Q_GRID[qGrid][0];
    if (key === 'qstr') return [25, 50, 75, 100][qStr] + '%';
    if (ENUMS[key]) return String(ENUMS[key][clamp(Math.round(v), 0, ENUMS[key].length - 1)]);
    if (key === 'tune') { const s = Math.round((v - 0.5) * 24); return (s > 0 ? '+' : '') + s + 'st'; }
    if (key === 'pan') { const d = Math.round((v - 0.5) * 200); return d === 0 ? 'C' : (d < 0 ? 'L' + (-d) : 'R' + d); }
    return String(Math.round(v * 100));
}
function fracVal(key, v) { return ENUMS[key] ? Math.round(v) / Math.max(1, ENUMS[key].length - 1) : clamp(v, 0, 1); }
const PM_SHORT = ['KEYS', 'REPT', 'STEP', 'LIVE'];
let toastSeq = 0;

/* ---- curve preview: while a knob is turned, a small box shows the envelope / filter / duck / drive shape the value makes ---- */
let curveKey = '';
const CURVE_KEYS = {};                              /* the little curve window above the page was removed: the knob cell shows the value */
function curveVal(k) { const v = getVal(scopeOf(k), k); return clamp(Number(v) || 0, 0, 1); }
function drawCurveBox(key) {
    const X = 84, Y = 14, W = 42, H = 24, n = W - 4, bx = X + 2, by = Y + H - 3, hh = H - 6;
    fill_rect(X, Y, W, H, 0); draw_rect(X, Y, W, H, 1);
    const v = curveVal(key);
    const plot = f => { let py = -1; for (let i = 0; i < n; i++) { const y = clamp(Math.round(f(i / (n - 1)) * hh), 0, hh), p = py < 0 ? y : py; const a = Math.min(y, p), b = Math.max(y, p); for (let q = a; q <= b; q++) set_pixel(bx + i, by - q, 1); py = y; } };
    if (key === 'decay') plot(t => Math.exp(-t / (0.04 + v * v * 0.9)));
    else if (key === 'att') { const a = 0.02 + v * 0.55; plot(t => t < a ? t / a : Math.exp(-(t - a) * 1.6)); }
    else if (key === 'cut' || key === 'res') {
        const c = key === 'cut' ? v : curveVal('cut'), r = key === 'res' ? v : curveVal('res');
        plot(t => { const d = t - c; const base = d <= 0 ? 0.75 : 0.75 * Math.exp(-d * 9); return base + 0.22 * r * Math.exp(-d * d * 260) ; });
    }
    else if (key === 'fenv') { const c = curveVal('cut'); plot(t => 0.12 + (0.85 - 0.12) * Math.exp(-t * 4) * (0.15 + v * 0.85) * (1 - c * 0.5)); }
    else if (key === 'duck' || key === 'duckr') {
        const d = key === 'duck' ? v : curveVal('duck'), rl = key === 'duckr' ? v : curveVal('duckr'), t0 = 0.12, tau = 0.04 + rl * rl * 0.9;
        plot(t => t < t0 ? 1 : 1 - d * Math.exp(-(t - t0) / tau));
    }
    else if (key === 'drive') plot(t => { const x = t * 2 - 1, k = 1 + v * 9; return 0.5 + 0.5 * Math.tanh(k * x) / Math.tanh(k); });
}
function showToast(label, text, f) {
    toastSeq++; label = tr(label); text = tr(text); toast = { label, text, f, ticks: slowKnobs ? 160 : 70, ck: curveKey, nm: 1 }; curveKey = ''; needsRedraw = true;
    const e = undoStack.length ? undoStack[undoStack.length - 1] : null; if (e && (!e.lbl || e.k) && label !== 'UNDO' && label !== 'REDO') e.lbl = (String(label) + ' ' + String(text)).substring(0, 20);
}

const FX_TYPE_KEY = { xfx: 1, x2fx: 1, mfx: 1, m2fx: 1, dmode: 1, rmode: 1, dly_mode: 1, rvb_mode: 1, mono: 1 };
function editParam(scope, key, delta, idx) {
    if (scope === 'sys') {
        if (key === 'bpm' || key === 'swing') pSnap('sys', 0, key);
        if (key === 'bpm') { bpm = clamp(bpm + delta * (shiftHeld ? 2 : 1), 40, 240); enqueue('bpm', 'bpm', String(bpm)); }
        else if (key === 'swing') { swing = clamp(swing + delta, 0, 25); enqueue('swing', 'swing', (0.5 + swing / 100).toFixed(3)); }
        else if (key === 'fullvel') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; acc[idx] = 0; fullVel = !fullVel; }
        else if (key === 'rqoff') { rqOff = clamp(rqOff + delta * 0.02, 0, 1); }
        else if (key === 'repmode') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; acc[idx] = 0; if (!repAllowed) { showToast('NOTE REPEAT', 'OFF IN OPTIONS', 0); return false; } toggleTop(1, true); }
        else if (key === 'rrate') { acc[idx] += delta; if (Math.abs(acc[idx]) < 5) return false; setRepRate(repRate + (acc[idx] > 0 ? 1 : -1), false); acc[idx] = 0; }
        else if (key === 'qscale') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; opScaleQ(); }
        else if (key === 'qtime') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; opTimeQ(); }
        else if (key === 'qgrid') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; qGrid = clamp(qGrid + (acc[idx] > 0 ? 1 : -1), 0, 5); acc[idx] = 0; markEdited(); }
        else if (key === 'qstr') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; qStr = clamp(qStr + (acc[idx] > 0 ? 1 : -1), 0, 3); acc[idx] = 0; markEdited(); }
        else if (key === 'kind') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const d = acc[idx] > 0 ? 1 : 0; acc[idx] = 0; setKind(sel, d); }
        else if (key === 'tvol') { setTV(sel, clamp(getTV(sel) + delta * (shiftHeld ? 0.016 : 0.0032), 0, 1)); }
        else if (key === 'pmode') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; { const dr = acc[idx] > 0 ? 1 : -1; let m = clamp(topModes[sel] + dr, 0, 3); if (m === 1 && (!repAllowed || !kind[sel])) m = clamp(topModes[sel] + 2 * dr, 0, 3); topModes[sel] = m; } acc[idx] = 0; heldStep = -1; heldPad = -1; }
        else if (key === 'morph') { morphTurn(delta * (shiftHeld ? 4 : 1)); }
        else if (key === 'rsrc') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; rsSrc = clamp(rsSrc + (acc[idx] > 0 ? 1 : -1), 0, 16); acc[idx] = 0; }
        else if (key === 'rbar') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; rsBarI = clamp(rsBarI + (acc[idx] > 0 ? 1 : -1), 0, 4); acc[idx] = 0; }
        else if (key === 'rgo') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const d = acc[idx] > 0; acc[idx] = 0; if (d) rsArm(); else rsCancel(); }
        else if (key === 'snorm') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; sampleNormalize(); }
        else if (key === 'kslot') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; kitSlot = clamp(kitSlot + (acc[idx] > 0 ? 1 : -1), 0, 15 + FACTORY.length); acc[idx] = 0; }
        else if (key === 'ksave') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; kitSave(); return false; }
        else if (key === 'kload') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; kitLoad(); kitBackup = null; return false; }
        else if (key === 'rdens') { rndDens = clamp(rndDens + delta * 0.01, 0.02, 0.9); }
        else if (key === 'rvar') { rndVar = clamp(rndVar + delta * 0.02, 0, 1); }
        else if (key === 'rrat') { rndRat = clamp(rndRat + delta * 0.02, 0, 1); }
        else if (key === 'favo') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; acc[idx] = 0; favOnly = !favOnly; markEdited(); }
        else if (key === 'kprev') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const b = acc[idx] < 0; acc[idx] = 0; kitPreview(b); }
        else if (key === 'rkey') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; acc[idx] = 0; rndKey ^= 1; }
        else if (key === 'tcol') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; tcolO[sel] = clamp(tcolO[sel] + d, 0, 26); markEdited(); }
        else if (key === 'prst') { acc[idx] += delta; if (acc[idx] < 4) return false; acc[idx] = 0; restoreBackup(); return false; }
        else if (key === 'pnew') { acc[idx] += delta; if (acc[idx] < 5) return false; acc[idx] = 0; newProject(); return false; }
        else if (key === 'pslot') { acc[idx] += delta; if (Math.abs(acc[idx]) < 1) return false; projSlot = clamp(projSlot + (acc[idx] > 0 ? 1 : -1), 0, 31); acc[idx] = 0; }
        else if (key === 'psave') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; projOpen(0); return false; }
        else if (key === 'pload') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; projOpen(0); return false; }
        else if (EU_KEYS[key]) { if (!euEdit(key, delta, idx)) return false; }
        else if (key === 'moclk') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; acc[idx] = 0; moClk ^= 1; moPush(); }
        else if (key === 'motn') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; moTrk = clamp(moTrk + (acc[idx] > 0 ? 1 : -1), 0, 15); acc[idx] = 0; }
        else if (key === 'mont') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; acc[idx] = 0; moMask ^= (1 << moTrk); moPush(); }
        else if (key === 'mochn') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; moChn = clamp(moChn + (acc[idx] > 0 ? 1 : -1), 0, 15); acc[idx] = 0; moPush(); }
        else if (key === 'moxp') { acc[idx] += delta; if (Math.abs(acc[idx]) < 1) return false; moXp = clamp(moXp + (acc[idx] > 0 ? 1 : -1) * (shiftHeld ? 12 : 1), -24, 24); acc[idx] = 0; moPush(); }
        else if (key === 'movel') { moVp = clamp(moVp + (delta > 0 ? 1 : -1) * (shiftHeld ? 1 : 5), -75, 50); moPush(); }
        else if (key === 'modn') { acc[idx] += delta; if (Math.abs(acc[idx]) < 1) return false; moDn = clamp(moDn + (acc[idx] > 0 ? 1 : -1) * (shiftHeld ? 12 : 1), -12, 36); acc[idx] = 0; moPush(); }
        else if (key === 'moall') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; moMask = acc[idx] > 0 ? 65535 : 0; acc[idx] = 0; moPush(); }
        else if (key === 'sgsl') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; if (shiftHeld) { songMove(d); return true; } songSlot = clamp(songSlot + d, 0, songList.length - 1); dirty = true; needsRedraw = true; }
        else if (key === 'sgin') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; songIns(); }
        else if (key === 'sgdl') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; songDel(); }
        else if (key === 'sgmv') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; songMove(d); }
        else if (key === 'sglp') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; acc[idx] = 0; songLoop = !songLoop; showToast('SONG', songLoop ? 'LOOPS' : 'PLAYS ONCE', songLoop ? 1 : 0); dirty = true; }
        else if (key === 'sgcl') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; songList[songSlot][0] = clamp(songList[songSlot][0] + (acc[idx] > 0 ? 1 : -1), 0, NPAT - 1); acc[idx] = 0; dirty = true; }
        else if (key === 'sgbr') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; songList[songSlot][1] = clamp(songList[songSlot][1] + (acc[idx] > 0 ? 1 : -1), 1, 32); acc[idx] = 0; dirty = true; }
        else if (key === 'sgrp') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; songList[songSlot][2] = clamp(songList[songSlot][2] + (acc[idx] > 0 ? 1 : -1), 1, 16); acc[idx] = 0; dirty = true; }
        else if (key === 'sgtp') { acc[idx] += delta; if (Math.abs(acc[idx]) < 1) return false; const e = songList[songSlot], st = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; let v = e[3] ? e[3] + st : (st > 0 ? bpm : 0); if (e[3] && v < 40) v = 0; e[3] = v ? clamp(v, 40, 240) : 0; songSent = songSent; dirty = true; }
        else if (key === 'sgtr') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; songList[songSlot][4] = clamp(songList[songSlot][4] + (acc[idx] > 0 ? 1 : -1), 0, 2); acc[idx] = 0; dirty = true; }
        else if (key === 'sgtg') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; songList[songSlot][5] = clamp(songList[songSlot][5] + (acc[idx] > 0 ? 1 : -1), 0, 7); acc[idx] = 0; dirty = true; }
        else if (key === 'sgon') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const stt = songOn ? (songLoop ? 1 : 2) : 0, nx = (stt + (acc[idx] > 0 ? 1 : 2)) % 3; acc[idx] = 0; songOn = nx > 0; songLoop = nx !== 2; songSent = -1; songLaunched = -1; songOff = 0; songNext = -1; if (!songOn) songRestore(); showToast('SONG', nx === 0 ? 'OFF' : (nx === 1 ? 'ON - LOOP' : 'ON - ONCE'), nx ? 1 : 0); }
        else if (key === 'slm') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; slSel = clamp(slSel + (acc[idx] > 0 ? 1 : -1), 0, slN - 1); acc[idx] = 0; }
        else if (key === 'slp') { const lo = slSel > 0 ? slm[slSel - 1] + 0.001 : 0, hi = slSel < slN - 1 ? slm[slSel + 1] - 0.001 : 0.998; slm[slSel] = clamp(slm[slSel] + delta * (shiftHeld ? 0.0005 : 0.004), lo, Math.max(lo, hi)); }
        else if (key === 'sln') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; slN = clamp(slN + (acc[idx] > 0 ? 1 : -1), 1, 16); acc[idx] = 0; slEqual(slN); }
        else if (key === 'slg') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; sliceApply(); }
        else if (key === 'sla') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; sliceAuto(); }
        else if (key === 'sladd') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; sliceAdd(); }
        else if (key === 'sldel') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; sliceDel(); }
        else if (key === 'slsn') { acc[idx] += delta; if (acc[idx] < 3) return false; acc[idx] = 0; sliceSnap(); }
        else if (key === 'smnx') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; smpTool('next', d); }
        else if (key === 'smfit' || key === 'smhf' || key === 'smdb' || key === 'smrn' || key === 'smrv' || key === 'smnm' || key === 'smrs') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; acc[idx] = 0; smpTool(key); }
        else if (key === 'lay') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; if (kind[sel]) layD = clamp(layD + d, 0, 1); else layS = clamp(layS + d, 0, 5); qLay(); heldStep = -1; heldPad = -1; markEdited(); }
        else if (key === 'dsel') { if (!kind[sel]) return false; acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; dsel[sel] = clamp(dsel[sel] + (acc[idx] > 0 ? 1 : -1), 0, 15); acc[idx] = 0; heldStep = -1; heldPad = -1; syncSel(); }
        else if (key === 'browse') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; acc[idx] = 0; if (!browseAll && !favOnly) browseAll = true; else if (browseAll) { browseAll = false; favOnly = favs.length > 0; } else favOnly = false; showToast('JOG SOUNDS', browseAll ? 'ALL SOUNDS' : 'CATEGORY ONLY', browseAll ? 1 : 0); }
        return { v: getVal('sys', key) };
    }
    if (key === 'smp') { selectSample(delta > 0 ? 1 : -1, true); return { v: getVal(scope, key) }; }
    if (key === 'smd') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; setLaneP(ln(), 'smd', clamp(Math.round(pads[ln()][PI.smd] * 2) + d, 0, 2) / 2); return { v: pads[ln()][PI.smd] }; }
    if (key === 'fmode') { acc[idx] += delta; if (Math.abs(acc[idx]) < 3) return false; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; setLaneP(ln(), 'fmode', clamp(Math.round(pads[ln()][PI.fmode] * 2) + d, 0, 2) / 2); return { v: pads[ln()][PI.fmode] }; }
    let v = getVal(scope, key);
    if (ENUMS[key]) {
        acc[idx] += delta;
        if (Math.abs(acc[idx]) < (FX_TYPE_KEY[key] ? 14 : 6)) return false;                  /* effect types turn even slower; a list value (arp rate, wave ...) needs a longer turn per step, so it no longer jumps */
        v = clamp(Math.round(v) + (acc[idx] > 0 ? 1 : -1), 0, ENUMS[key].length - 1); acc[idx] = 0;
    } else v = clamp(v + delta * (shiftHeld ? 0.016 : 0.0032), 0, 1);
    if (scope === 'pad') {
        const L = keyLane(key);
        if (deleteHeld) { autoClear(L, key); return { v: pads[L][PI[key]] }; }
        pSnap('pad', L, key); pads[L][PI[key]] = v;
        if (recArmed && playing && PI[key] !== 0) autoRec(L, key, v);
        if (liveOk(sel) || key === 'vol' || key === 'pan') enqueue('p' + L + key, 'p' + p2(L + 1) + '_' + key, ENUMS[key] ? String(Math.round(v)) : v.toFixed(3));
        if (key !== 'vol' && key !== 'pan' && true) storeCs(L);
    } else {
        pSnap('master', 0, key); master[MI[key]] = v;
        enqueue('m' + key, key, ENUMS[key] ? String(Math.round(v)) : v.toFixed(3));
    }
    return { v: v };
}

/* sequencer-view knobs */
const SEQ_DRM = ['tune', 'decay', 'tone', 'drive', 'cut', 'res', 'pan', 'vol'], SEQ_DRM_S = ['TUN', 'DCY', 'TON', 'DRV', 'CUT', 'RES', 'PAN', 'VOL'];
const SEQ_SYN = ['tune', 'att', 'decay', 'tone', 'cut', 'res', 'pan', 'vol'], SEQ_SYN_S = ['TUN', 'ATK', 'DCY', 'TON', 'CUT', 'RES', 'PAN', 'VOL'];      /* synth tracks get their own knob row (attack instead of drive), drum tracks keep theirs */
function seqSnd() { return kind[sel] ? SEQ_DRM : SEQ_SYN; }
function seqShort() { return kind[sel] ? SEQ_DRM_S : SEQ_SYN_S; }
function seqKnob(i, delta) {
    if (delta === 0) return;
    const big = shiftHeld ? 5 : 1;
    if (!shiftHeld || i > 3) { const k = seqSnd()[i], r = editParam('pad', k, delta, i); if (r) showToast(k.toUpperCase(), fmtVal(k, r.v), fracVal(k, r.v)); markEdited(); return; }
    switch (i) {
    case 0: pSnap('sys', 0, 'bpm'); bpm = clamp(bpm + delta * big, 40, 240); enqueue('bpm', 'bpm', String(bpm)); break;
    case 1: pSnap('sys', 0, 'swing'); swing = clamp(swing + delta, 0, 25); enqueue('swing', 'swing', (0.5 + swing / 100).toFixed(3)); break;
    case 2: setLen(lens[sel] + delta * (shiftHeld ? 16 : 1), true); stepPage = Math.min(stepPage, pageCount() - 1); break;
    case 3: acc[3] += delta; if (Math.abs(acc[3]) >= 2) { setClip(sel, clamp(ed[sel] + (acc[3] > 0 ? 1 : -1), 0, NPAT - 1)); acc[3] = 0; } break;
    default: {
        const k = ['tune', 'decay', 'tone', 'vol'][i - 4];
        const r = editParam('pad', k, delta, i);
        if (r) showToast(k.toUpperCase(), fmtVal(k, r.v), fracVal(k, r.v));
    }
    }
    markEdited();
}
function launchClip(t, c) {                       /* c = -1 stops the track */
    if (c >= 0) { ed[t] = c; applyClipSound(t, c); }
    dspSet('clip', t + ',' + c);
    clipsQ[t] = playing ? c : -2; if (!playing) clipsCur[t] = c;
    if (c >= 0 && !playing) { playing = true; gs = 0; dspSet('play', '1'); }
    needsRedraw = true; dirty = true;
}
function setClip(t, c) {
    ed[t] = c; launchClip(t, c); showToast('CLIP', 'T' + p2(t + 1) + ' C' + (c + 1), c / (NPAT - 1)); markEdited();
}
function launchScene(c, quiet) {
    for (let t = 0; t < NTRK; t++) if (trackHasSteps(c, t)) { ed[t] = c; applyClipSound(t, c); }
    dspSet('scene', String(c));
    for (let t = 0; t < NTRK; t++) { const tgt = trackHasSteps(c, t) ? c : -1; clipsQ[t] = playing ? tgt : -2; if (!playing) clipsCur[t] = tgt; }
    if (!playing) { playing = true; gs = 0; dspSet('play', '1'); }
    if (!quiet) showToast('SCENE', sceneNames[c] || String(c + 1), c / (NPAT - 1)); needsRedraw = true; dirty = true;
}
function stopAll() { for (let t = 0; t < NTRK; t++) { dspSet('clip', t + ',-1'); clipsQ[t] = playing ? -1 : -2; if (!playing) clipsCur[t] = -1; } showToast('CLIPS', 'STOP ALL', 0); needsRedraw = true; }
function clipsPad(idx) {
    const row = idx >> 3, c = idx & 7, t = bank * 4 + (3 - row);
    selectTrack(t);
    clipAction(t, c);
}
function clipAction(t, c) {
    if (muteHeld) { muteUsed = true; toggleMute(t); return; }
    if (deleteHeld) { snapAll(c, t); autoClearClip(t, c); for (const l of lanesOf(t)) { for (let s = 0; s < MAXSTEP; s++) steps[c][l][s] = [0, 100, 0, 0]; qSeq(c, l); } resetLen(t); showToast('CLIP', 'CLEARED', 0); markEdited(); return; }
    if (copyHeld) {
        if (!copySrc) { copySrc = [t, c]; showToast('COPY', 'PICK TARGET', 0.5); return; }
        const st0 = copySrc[0], sc = copySrc[1];
        if (!!kind[st0] !== !!kind[t]) { copySrc = null; showToast('COPY', 'SYNTH<>DRUM', 0); return; }
        snapAll(c, t); ensureCs();
        const dl = lanesOf(t), sl = lanesOf(st0);
        for (let i = 0; i < dl.length; i++) {
            for (let s = 0; s < MAXSTEP; s++) steps[c][dl[i]][s] = steps[sc][sl[i]][s].slice();
            csnd[c][dl[i]] = csnd[sc][sl[i]].slice(); sendCs(c, dl[i]); qSeq(c, dl[i]);
        }
        if (ed[t] === c) applyClipSound(t, c);
        copySrc = null; showToast('COPY', 'DONE', 1); markEdited(); return;
    }
    copySrc = null;
    if (shiftHeld) { dspSet('clip', t + ',-1'); clipsQ[t] = playing ? -1 : -2; if (!playing) clipsCur[t] = -1; showToast('TRACK ' + p2(t + 1), 'STOP', 0); needsRedraw = true; return; }
    launchClip(t, c);
}
const TOP_NAMES = ['NOTE', 'REPEAT', 'STEPS', 'LIVE FX', 'CHORD'];
let repAllowed = true;                                     /* off: Note Repeat is skipped when switching the pad mode */
function nextTopMode(cur, d, trackBtn) {                              /* the Track button walks PITCH > REPEAT > STEPS (> CHORD on synth tracks); LIVE FX has its own switch */
    const ring = kind[sel] ? [0, 1, 2] : (trackBtn ? [0, 2] : [0, 4]);            /* drum tracks: PITCH > REPEAT > STEPS; synth tracks: Shift + step 8 = note layout <> CHORD, the Track button = note layout <> STEPS (never CHORD) */
    let i = ring.indexOf(cur); if (i < 0) return d > 0 ? ring[0] : ring[ring.length - 1];
    for (let n = 0; n < ring.length; n++) { i = (i + d + ring.length) % ring.length; if (ring[i] !== 1 || repAllowed) return ring[i]; }
    return 0;
}      /* the Track button only walks PITCH > REPEAT > STEPS; LIVE FX has its own switch (menu) */
function canTop() { return !(menuOpen || fb || askKind >= 0 || exitDlg >= 0 || view === 7 || view === 31 || view === 20 || isHub(view) || SYM_VIEW_SET[view]); }      /* pad modes only change on the track's own screens, never in clips / song / menus */
function cycleTop(d, trackBtn) {
    if (!canTop()) { showToast('PAD MODE', 'OPEN THE TRACK FIRST', 0); return; }
    liveFxAllOff(); topModes[sel] = nextTopMode(topModes[sel], d, trackBtn);
    showToast('TRACK ' + p2(sel + 1) + ' PADS', TOP_NAMES[topModes[sel]], topModes[sel] / 4); if (topModes[sel] === 4) chordReconcile(); markEdited(); needsRedraw = true;
}
let layS = 0, layD = 0;                                    /* pad layouts: synth 0 MOVE (in key, rows 3 degrees apart) 1 SEQ 2 OCT 3 CHR (chromatic, rows a fourth apart); drum 0 sounds left 1 sounds right */
const LAYS_NAME = ['KEY', 'SEQ', 'OCT', '4THS', '3RDS', 'ROW8'], LAYD_NAME = ['SND L', 'SND R'];
function soundArea(cc) { return (layD === 0) === (cc < 4); }
function degOf(col, row, w) { return layS >= 3 ? 32 + col + [5, 4, 8][layS - 3] * row : layS === 0 ? col + 3 * row : (layS === 1 ? row * w + col : col + 7 * row); }
function padStepOf(i) { return (3 - (i >> 3)) * 8 + (i & 7); }       /* step 1 = top left ... step 32 = bottom right */
function padStepIdx(i) {                                  /* step number of a pad in STEPS mode (drum track: right 4x4 block = 16 steps), -1 = no step pad */
    if (kind[sel]) { const c = i & 7; return soundArea(c) ? -1 : stepPage * 16 + (3 - (i >> 3)) * 4 + (c & 3); }
    return (stepPage >> 1) * 32 + padStepOf(i);
}
let heldPad = -1;
const lastSemi = new Array(16).fill(null);               /* the pitch of the last pad you played on each synth track: a step you switch on afterwards gets it too */
function stepsPad(idx, vel) {                             /* STEPS mode: 32 pads = 32 steps; hold a pad + turn knobs 1-7 = edit that step */
    const s = padStepIdx(idx);
    if (s < 0 || s >= MAXSTEP) return;
    const c = ed[sel], L = ln();
    snap(c, L);
    if (s >= LL()) {                                  /* a pad beyond the loop end extends the loop (in blocks of 16) */
        setLen(Math.min(MAXSTEP, Math.ceil((s + 1) / 16) * 16), false);
    }
    if (deleteHeld) { steps[c][L][s] = [0, 100, 0, 0]; qStep(c, L, s); markEdited(); return; }
    const st = stepAt(s);
    heldStep = s; heldPad = idx; heldEdited = false; heldWasOn = !!st[0];
    if (!heldWasOn) { st[0] = 1; st[1] = fullVel ? 127 : clamp(vel, 1, 127); qStep(c, L, s); markEdited(); }
    needsRedraw = true;
}
function stepsPadUp(idx) {                                // release: a plain tap on a lit step switches it off
    if (heldPad !== idx || heldStep < 0) return;
    const s = heldStep, c = ed[sel], L = ln();
    if (heldWasOn && !heldEdited) { steps[c][L][s] = [0, 100, 0, 0]; qStep(c, L, s); markEdited(); }
    heldStep = -1; heldPad = -1; needsRedraw = true;
}
function clipTrig(t, c) {                                  /* does the playing clip of track t fire a note at this very step? */
    if (!playing) return false;
    for (const l of lanesOf(t)) { const n = effLen(l), row = steps[c] && steps[c][l]; if (!row) continue; const cur = (gs - 1 + 1024 * n) % n; if (row[cur] && row[cur][0] && !lmutes[l]) return true; }
    return false;
}
function pulseLow() { const p = playing ? (((gs - 1) % 4) + 4) % 4 : Math.floor(Date.now() / 250) & 3; return p >= 2; }      /* beat-locked blink phase of the pulsing pads */
function padLedBase(i) {                                 /* LED colour of pad i (0..31) of the selected track */
    const dr = kind[sel], m = topModes[sel], tc = trkCol(sel), cc = i & 7, rr = i >> 3;
    if (m === 3 && fillPad(i)) { const fl = fillLanes(); return fl && fl[i & 7] ? ((i >> 3) ? rootCol(tc) : trackColor(pads[fl[i & 7]][0])) : 0; }
    if (m === 3 && lfxPad(i) >= 0) { const n = lfxPad(i); return n < 7 ? tc : (n < 10 ? rootCol(tc) : 120); }
    if (dr && soundArea(cc)) {                                   /* drum track, left block: the 16 sounds (white = selected, colour = sound family) */
        const k = rr * 4 + (cc & 3);
        if (lmutes[laneOf(sel, k)]) return k === dsel[sel] ? 124 : dimCol(trackColor(pads[laneOf(sel, k)][0]));                   /* muted sound: dark (faint if selected) */
        const fc = trackColor(pads[laneOf(sel, k)][0]);                          /* family colour: kick red, snare, hats yellow ... */
        return k === dsel[sel] ? 120 : fc;
    }
    if (dr && m === 1) { const ri = rr * 4 + (cc & 3); return ri === repRate ? 120 : (ri >= 8 ? tc : dimCol(tc)); }      /* speed pads: chosen = white, fast half bright, slow half dim */
    if (m === 2) {
        const si = padStepIdx(i);
        if (si >= LL()) return 124;                   /* beyond the loop: faint, press to extend */
        const cur = playing ? ((gs - 1 + 1024 * LL()) % LL()) : -1;
        if (si === cur) return 120;
        return steps[ed[sel]][ln()][si][0] ? tc : 96;
    }
    if (m === 4 && !dr) {                                  /* CHORD pads: left 4 x 4 = single notes, right 4 x 4 = the chords of the same notes (other colour, tonic white) */
        const j4 = degOf(cc & 3, rr, 4), chd = cc >= 4, root = j4 >= 32 ? (j4 - 32) % 12 === 0 : j4 % 7 === 0;
        if (j4 >= 32 && !root && SCALE7[Math.round(master[2]) % 3].indexOf((j4 - 32) % 12) < 0) return 124;
        return chd ? (root ? 120 : usedCol(tc)) : (root ? rootCol(tc) : tc);
    }
    const j = dr ? degOf(cc & 3, rr, 4) : degOf(cc, rr, 8);
    if (j >= 32) {                                         /* chromatic: root bright, scale notes normal, others dim */
        const pc = (j - 32) % 12;
        if (pc === 0) return m === 1 ? 120 : rootCol(tc);
        return SCALE7[Math.round(master[2]) % 3].indexOf(pc) >= 0 ? tc : (m === 1 ? dimCol(tc) : 124);
    }
    if (j % 7 === 0) return m === 1 ? 120 : rootCol(tc);   /* PITCH: tonic in the darker shade of the track colour, the rest full colour; REPEAT: tonic white */
    return tc;
}

/* what the sequencer plays: pads of used notes / used drum sounds light up, the one playing right now flashes white */
let seqUse = { semis: {}, play: null, lanes: [], lplay: [] };
function seqUseUpdate() {
    const c = ed[sel], su = { semis: {}, play: null, lanes: [], lplay: [] };
    const cur = (l) => { const n = effLen(l); return playing ? ((gs - 1 + 1024 * n) % n) : -1; };
    if (kind[sel]) {
        for (let k = 0; k < 16; k++) { const l = laneOf(sel, k), n = effLen(l), row = steps[c][l]; let has = false; for (let s2 = 0; s2 < n; s2++) if (row[s2][0]) { has = true; break; } su.lanes.push(has); const cs = cur(l); su.lplay.push(cs >= 0 && !!row[cs][0]); }
    }
    const L = ln(), n = effLen(L), row = steps[c][L], cs = cur(L);
    for (let s2 = 0; s2 < n; s2++) if (row[s2][0]) su.semis[row[s2][2]] = 1;
    if (cs >= 0 && row[cs][0]) su.play = row[cs][2];
    seqUse = su;
}
function padLed(i) {
    if (view === 31) return songLed(i);
    const c = padLedBase(i);
    if (askKind >= 0 || view === 7 || view === 20) return c;
    const dr = kind[sel], m = topModes[sel], cc = i & 7, rr = i >> 3;
    if (m === 3 && fillPad(i)) return fillHeld[i] ? 126 : c;
    if (m === 3 && lfxPad(i) >= 0) return lfxHeld[i] ? 126 : c;
    if (dr && soundArea(cc)) {                                       /* drum sounds: used = bright, empty = dim, hit right now = flash */
        const k = rr * 4 + (cc & 3);
        if (lmutes[laneOf(sel, k)]) return c;
        if (k === dsel[sel]) return seqUse.lplay[k] ? trackColor(pads[laneOf(sel, k)][0]) : c;      /* selected = white, pulses in its colour on a hit */
        return seqUse.lplay[k] ? 126 : c;                        /* steady family colour, pulse (white) only while a note triggers */                            /* every sound pad has the track colour; sequenced ones pulse */
    }
    if (m === 3 && fillPad(i)) return fillHeld[i] ? 126 : c;
    if (m === 3 && lfxPad(i) >= 0) return lfxHeld[i] ? 126 : c;
    if (m === 2 || (dr && m === 1)) return c;
    const j = (dr || m === 4) ? degOf(cc & 3, rr, 4) : degOf(cc, rr, 8), sm = notePadSemi(j) + oct();
    if (seqUse.play === sm) return 126;
    if (heldStep >= 0 && !recDown) { const hs = stepAt(heldStep); if (hs && hs[0] && hs[2] === sm) return 126; }      /* step held: the pad of its note lights up white */
    return c;
}
function logHit(track, semi, vel) {
    if (!playing || recArmed) return;
    const r = dspGet('pos'); if (typeof r !== 'string') return;
    const a = r.split(','), g = (parseInt(a[1]) || 0) - ((parseInt(a[3]) || 0) >= 50 ? 0 : 1);
    hitLog.push({ t: track, semi: semi, vel: vel, g: g }); if (hitLog.length > 64) hitLog.shift();
}
const HELP = {
    0: ['Step + 2. Step: Laenge', '2+ Steps: Knopf = alle', 'Copy+Step, Ziel: einfuegen'],
    1: ['Preset: Klang waehlen', 'Jog: Reiter wechseln', 'Shift+Jog: Presets'],
    2: ['Filter: LP, HP, Crush', 'Jog: Reiter wechseln', 'Jog klick: letzter Reiter'],
    3: ['Mod: Ring, Pump, Wobble', 'Duck = Sidechain Tiefe', 'Jog: Reiter wechseln'],
    9: ['Engine: Wave, Sub, Filter', 'Jog klick: Seite 2 (Noise, Fold, Tremolo)', 'Jog: Reiter wechseln'],
    28: ['Sample: Datei=Knopf 1. Shift+Jog klick: Browser. Dort Shift+Klick auf Ordner = Kit aus 16 WAVs. Jog klick: Slice/ENV/Tools. Tools: Fit, Half, Move. Slice: Add, Del, Snap.'],
    27: ['MIDI-Effekt + Arp', 'Jog klick: MIDI FX 2', 'Jog: Reiter wechseln'],
    36: ['Euklid: 7 Seiten', 'Jog klick: Seite', 'Aendert das Pattern'],
    38: ['FX SENDS: Delay + Reverb', 'Send, Effekt, Output', 'Jog klick: Grain'],
    39: ['FX BUS 2: Return,', 'Breite, Low Cut', 'fuer alle Spuren'],
    46: ['FX BUS: ein Delay und', 'ein Reverb fuer alle', 'Spuren (Sends)', 'Jog klick: Seite 2'],
    50: ['Granulator: Grain = Mix', 'Size 4ms-1.2s kurz/lang, Dens', 'Reverse, Freeze, Window', 'Bild unten: Grains live'],
    16: ['SOUND = Track-Seiten', 'EDIT = Aktionen', 'MORE = Projekt, Song'],
    18: ['Tempo, Song, Projekt', 'Kits, Resample, Sync', 'Optionen, History'],
    20: ['Pegel aller Tracks', 'Jog klick: Vol/Dly/Rvb', 'Shift+Knopf: Pan'],
    21: ['Track: Typ, Volume', 'Pads, Sound, Layout', 'Color = Farbe'],
    31: ['Steps 1-8 = Szene anhaengen', 'Steps 9-14 = Takte, 15 = Kopie', 'Step 16 / Play = Song an'],
    35: ['32 Projekt-Slots', 'New = leeres Projekt', 'Revert = Sicherung'],
    34: ['Kits: 16 Pad-Sounds. Slot 17+ = fertige Kits. Preview: rechts drehen = anhoeren, links = zurueck. Load = behalten.'],
    33: ['Resample nimmt Master', 'oder Track auf', 'Ergebnis = Sample'],
    37: ['MIDI Clock + Noten', 'ueber USB-A', 'Track + Ton einstellen'],
    8: ['Optionen + Zufall', 'RndDens = Dichte', 'RndKey = in Tonart'],
    10: ['Tempo, Swing, Human', 'Launch-Quantisierung', 'Key + Scale'],
    7: ['Clips: Pad = starten', 'Jog: Tracks scrollen', 'Copy+Pad = kopieren'],
    44: ['Deine 8 Favoriten', 'Menu: Pin last knob', 'Reset favorites = Standard'],
    41: ['Master EQ 3 Baender', 'Low / Mid / High +-12dB', 'Limiter = Obergrenze'],
    43: ['EQ pro Sound/Track', 'Low 100Hz Mid 1k High 8k', '+-12 dB, Mitte = aus'],
    42: ['Scope + 8 Baender', 'Jog klick: Ansicht, auch Tiere', 'Master-Ausgang live'],
    51: ['CPU-Auslastung + Spitze', 'Verlauf, Sounds, Effekte', 'Back = zurueck'],
    15: ['Grid, Strength: Quantize', 'Layout: Pad-Anordnung', 'Shift+Step 16 = jetzt'],
    40: ['Letzte Aenderungen', 'Jog waehlen', 'Klick = zurueck bis dort'],
    4: ['Master: Vol, Filter', 'Satur, Width, Comp', 'EQ + Limiter'],
};
const HELP_EN = {
    0: ['Step + 2nd step: length', '2+ steps: knob = all', 'Copy+step, target: paste'],
    1: ['Preset: choose a sound', 'Jog: change tab', 'Shift+Jog: presets'],
    2: ['Filter: LP, HP, Crush', 'Jog: change tab', 'Jog click: last tab'],
    3: ['Mod: Ring, Pump, Wobble', 'Duck = sidechain depth', 'Jog: change tab'],
    9: ['Engine: Wave, Sub, Filter', 'Jog click: page 2 (Noise, Fold, Tremolo)', 'Jog: change tab'],
    28: ['Sample: File = knob 1. Shift+Jog click: browser. There Shift+click on a folder = kit from 16 WAVs. Jog click: Slice/ENV/Tools. Tools: Fit, Half, Move. Slice: Add, Del, Snap.'],
    27: ['MIDI effect + Arp', 'Jog click: MIDI FX 2', 'Jog: change tab'],
    36: ['Euclid: 7 pages', 'Jog click: page', 'Changes the pattern'],
    38: ['FX SENDS: Delay + Reverb', 'send, effect, output', 'Jog click: Grain'],
    39: ['FX BUS 2: returns,', 'width, low cut', 'shared by all tracks'],
    46: ['FX BUS: one delay and', 'one reverb for all', 'tracks (sends)', 'Jog click: page 2'],
    50: ['Granulator: Grain = mix', 'Size 4ms-1.2s short/long, Dens', 'Reverse, Freeze, Window', 'Picture below: live grains'],
    16: ['SOUND = track pages', 'EDIT = actions', 'MORE = project, song'],
    18: ['Tempo, song, project', 'Kits, resample, sync', 'Options, history'],
    20: ['Level of all tracks', 'Jog click: Vol/Dly/Rvb', 'Shift+knob: pan'],
    21: ['Track: type, volume', 'Pads, sound, layout', 'Color'],
    31: ['Steps 1-8 = add scene', 'Steps 9-14 = bars, 15 = copy', 'Step 16 / Play = song on'],
    35: ['32 project slots', 'New = empty project', 'Revert = backup'],
    34: ['Kits: 16 pad sounds. Slot 17+ = ready-made kits. Preview: turn right = listen, left = back. Load = keep.'],
    33: ['Resample records master', 'or a track', 'Result = sample'],
    37: ['MIDI clock + notes', 'over USB-A', 'Set track + tone'],
    8: ['Options + random', 'RndDens = density', 'RndKey = in key'],
    10: ['Tempo, swing, human', 'Launch quantize', 'Key + scale'],
    7: ['Clips: pad = launch', 'Jog: scroll tracks', 'Copy+pad = copy'],
    44: ['Your 8 favorites', 'Menu: pin last knob', 'Reset favorites = default'],
    41: ['Master EQ 3 bands', 'Low / Mid / High +-12dB', 'Limiter = ceiling'],
    43: ['EQ per sound/track', 'Frequencies adjustable', '+-12 dB, middle = off'],
    42: ['Scope + 8 bands', 'Jog click: view, also animals', 'Master output live'],
    51: ['CPU load + peak', 'History, sounds, effects', 'Back = return'],
    15: ['Grid, strength: quantize', 'Layout: pad arrangement', 'Shift+step 16 = apply now'],
    40: ['Latest changes', 'Jog to choose', 'Click = go back to there'],
    4: ['Master: Vol, Filter', 'Satur, Width, Comp', 'EQ + limiter'],
};
function wrapTxt(s, n) { const out = []; let cur = ''; for (const w of s.split(' ')) { if ((cur + ' ' + w).trim().length > n) { if (cur) out.push(cur); cur = w; } else cur = (cur + ' ' + w).trim(); } if (cur) out.push(cur); return out; }
function drawHelp() {
    fill_rect(0, 0, 128, 64, 0); draw_rect(0, 0, 128, 64, 1);
    txt(4, 2, (lang === 1 ? 'HILFE ' : 'HELP ') + (VIEWS[view] || ''), 1); fill_rect(4, 11, 120, 1, 1);
    const h = (lang === 1 ? HELP : HELP_EN)[view] || [lang === 1 ? 'Jog drehen / klicken. Back = zurueck. Handbuch: PDF' : 'Turn / click the jog wheel. Back = return. Manual: PDF'];
    const L = wrapTxt(h.join('. '), 19);
    for (let i = 0; i < L.length && i < 5; i++) txt(5, 14 + i * 8, L[i], 1);
    txt(5, 54, lang === 1 ? 'Taste = zu' : 'Any key = close', 1);
}
let helpOpen = false;
let histSel = 0;
function drawHist() {
    titleBar('HISTORY', undoStack.length + ' STEPS');
    if (!undoStack.length) { txt(4, 24, 'NOTHING TO UNDO', 1); return; }
    const top = clamp(histSel - 2, 0, Math.max(0, undoStack.length - 5));
    for (let r = 0; r < 5; r++) {
        const k = top + r; if (k >= undoStack.length) break;
        const e = undoStack[undoStack.length - 1 - k], y = 12 + r * 10, on = k === histSel;
        if (on) fill_rect(0, y - 1, 128, 10, 1);
        txt(2, y, fit(e.k ? ((e.k === 'pad' ? 'T' + p2(laneTrk(e.t) + 1) + ' ' : '') + (e.lbl || e.k)) : 'T' + p2(laneTrk(e.t) + 1) + ' C' + (e.p + 1) + ' ' + (e.lbl || 'edit'), 122), on ? 0 : 1);
    }
}
function histApply() { const n = histSel + 1; for (let i = 0; i < n; i++) doUndo(); histSel = 0; view = 0; navStack = []; needsRedraw = true; showToast('HISTORY', n + ' STEP(S) UNDONE', 1); }
/* ---- fill: one bar of roll on the last quarter of the loop, then everything goes back by itself ---- */
let fillChg = [], fillPos = -1;
function fillBar(quiet) {
    if (!playing) { if (!quiet) showToast('FILL', 'PRESS PLAY FIRST', 0); return; }
    if (fillChg.length && !quiet) { showToast('FILL', 'ALREADY ON', 0); return; }
    const t = sel, c = ed[t];
    for (const l of lanesOf(t)) {
        const n = effLen(l); if (n < 8 || !laneHas(c, l)) continue;
        let src = null; for (let s = 0; s < n; s++) if (steps[c][l][s][0]) { src = steps[c][l][s]; break; }
        if (kind[t] && BASE_T[Math.round(pads[l][0])] <= 4) continue;                 /* kicks keep their pattern */
        for (let s = n - 4; s < n; s++) {
            const st = steps[c][l][s], orig = st.slice(), v = clamp(60 + (s - (n - 4)) * 22, 1, 127);
            const nv = [1, v, kind[t] ? 0 : ((src && src[2]) || 0), 0, kind[t] ? 1 : 0, 0, 0, 0];
            if (st[0] && st[1] > v) nv[1] = st[1];
            fillChg.push({ c: c, l: l, s: s, orig: orig, fill: nv.slice() }); steps[c][l][s] = nv;
        }
        qSeq(c, l);
    }
    if (!fillChg.length) { if (!quiet) showToast('FILL', 'NOTHING TO FILL', 0); return; }
    fillPos = -1; if (!quiet) showToast('FILL', '1 BAR', 1); markEdited();
}
function fillRestore() {
    const done = {};
    for (const e of fillChg) { if (JSON.stringify(steps[e.c][e.l][e.s]) === JSON.stringify(e.fill)) steps[e.c][e.l][e.s] = e.orig; done[e.c + '|' + e.l] = [e.c, e.l]; }
    for (const k in done) qSeq(done[k][0], done[k][1]);
    fillChg = []; fillPos = -1; needsRedraw = true; markEdited();
}
function fillTick() {
    if (!fillChg.length) return;
    if (!playing) { fillRestore(); return; }
    const n = effLen(fillChg[0].l), pos = (((gs - 1) % n) + n) % n;
    if (fillPos >= 0 && pos < fillPos) { fillRestore(); return; }
    fillPos = pos;
}
function doCapture() {                                  /* keep what was just played (last 2 bars) */
    if (!playing || !hitLog.length) { showToast('CAPTURE', 'NOTHING', 0); return; }
    let n = 0;
    for (const h of hitLog) {
        if (gs - h.g > 32) continue;
        const tk = laneTrk(h.t), len = effLen(h.t), tgt = ((h.g % len) + len) % len, c = ed[tk];
        if (n === 0) snap(c, h.t);
        const st = steps[c][h.t][tgt]; st[0] = 1; st[1] = clamp(h.vel, 1, 127); st[2] = clamp(h.semi, -64, 63);
        qStep(c, h.t, tgt); n++;
    }
    hitLog = []; showToast('CAPTURED', n + ' NOTES', 1); markEdited();
}
function toggleLaneMute(l) {
    lmutes[l] ^= 1; enqueue('lmute' + l, 'lmute', l + ',' + lmutes[l]); lmSent[l] = lmutes[l];
    showToast('SOUND ' + ((l - NTRK) % 16 + 1) + ' ' + (TYPE_NAME[Math.round(pads[l][0])] || ''), lmutes[l] ? 'MUTED' : 'ON', lmutes[l] ? 0 : 1); markEdited();
}
function toggleMute(t) {
    pSnap('mute', 0, 'm' + t); mutes[t] ^= 1; enqueue('mute' + t, 'mute', t + ',' + mutes[t]);
    showToast('TRACK ' + p2(t + 1), mutes[t] ? 'MUTED' : 'ON', mutes[t] ? 0 : 1); markEdited();
}
function soloTrack(t) {
    let othersMuted = true;
    for (let i = 0; i < NTRK; i++) if (i !== t && !mutes[i]) othersMuted = false;
    const on = !(othersMuted && !mutes[t]);
    for (let i = 0; i < NTRK; i++) { mutes[i] = on ? (i === t ? 0 : 1) : 0; enqueue('mute' + i, 'mute', i + ',' + mutes[i]); }
    showToast('SOLO ' + p2(t + 1), on ? 'ON' : 'OFF', on ? 1 : 0); markEdited();
}
/* ---- undo ---- */
let undoStack = [], snapHold = false, snapEu = false;
let redoStack = [];
let undoGroup = 0;
function snap(p, l, g) {                                 /* l = lane; g = group id: a whole drum clip is one undo step */
    redoStack = [];
    undoStack.push({ p: p, t: l, st: steps[p][l].map(x => x.slice()), len: lens[laneTrk(l)], ll: llen[l] || 0, g: g || 0, cs: (csnd && csnd[p] && csnd[p][l]) ? csnd[p][l].slice() : null });
    if (undoStack.length > 240) undoStack.shift();
}
function snapAll(p, t) { undoGroup++; for (const l of lanesOf(t)) snap(p, l, undoGroup); }
const UNDO_LBL = { copy: 'COPY SOUND', rnd: 'RANDOM SOUND', chordmode: 'CHORD MODE', bpm: 'TEMPO', swing: 'SWING' };
function undoLbl(kind, key) {                                 /* what the toast / history shows: the name of the knob that was turned */
    if (kind === 'mute') return 'MUTE'; if (kind === 'auto') return 'AUTOMATION';
    if (UNDO_LBL[key]) return UNDO_LBL[key];
    try { const n = keyName(key); return String(LONG_NAME[key] || n[0] || key).toUpperCase(); } catch (e) { return String(key).toUpperCase(); }
}
function pSnap(kind, L, key) {                              /* undo for sounds, knobs, tempo and mutes: one knob turn = one step (same knob within 1.5 s is merged) */
    const now = Date.now(), id = kind + ':' + L + ':' + key, top = undoStack.length ? undoStack[undoStack.length - 1] : null;
    if (top && top.k && top.id === id && now - top.ts < 1500) { top.ts = now; return; }
    const e = { k: kind, id: id, ts: now, L: L, t: kind === 'pad' ? L : 0, p: ed[sel], g: 0, lbl: undoLbl(kind, key) };
    if (kind === 'pad' && top && top.k === 'pad' && top.L !== L && now - top.ts < 300) { if (!top.g) top.g = ++undoGroup; e.g = top.g; top.ts = now; }       /* one gesture on several sounds (slice, chop, kit) = one step */
    if (kind === 'pad') e.v = pads[L].slice(); else if (kind === 'master') e.v = master.slice(); else if (kind === 'sys') e.v = [bpm, swing]; else if (kind === 'mute') e.v = [mutes.slice(), lmutes.slice()]; else if (kind === 'auto') e.v = JSON.stringify(AUTO);
    redoStack = []; undoStack.push(e); if (undoStack.length > 240) undoStack.shift();
}
/* undo for everything else (song, scenes, follow actions, pad modes): changes are noticed on their own and the old state is kept */
let miscLast = null, miscLastO = null, lfxBusy = 0;
const MISC_LBL = { sl: 'SONG', so: 'SONG', sp: 'SONG', sf: 'SONG', st: 'SONG', sn: 'SCENE NAME', sq: 'SCENE LAUNCH', fo: 'FOLLOW ACTION', tm: 'PAD MODE', rr: 'REPEAT RATE', tv: 'TRACK VOLUME' };
function miscGet() { return JSON.stringify({ sl: songList, so: songOn, sp: songLoop, sf: songOff, st: songTempoOn, sn: sceneNames, sq: sceneQ, fo: fol, tm: topModes, rr: repRate, tv: tv }); }
function miscSet(str) {
    const o = JSON.parse(str);
    songList = o.sl.map(songNorm); songOn = !!o.so; songLoop = !!o.sp; songOff = o.sf | 0; songTempoOn = !!o.st; songSlot = clamp(songSlot, 0, songList.length - 1);
    songSent = -1; songLaunched = -1; songNext = -1; if (!songOn) songRestore();
    sceneNames = o.sn.slice(); for (let c = 0; c < NPAT; c++) { if (sceneQ[c] !== o.sq[c]) enqueue('sq' + c, 'sceneq', c + ',' + o.sq[c]); sceneQ[c] = o.sq[c]; }
    for (let t = 0; t < NTRK; t++) for (let c = 0; c < NPAT; c++) if (fol[t][c] !== o.fo[t][c]) { fol[t][c] = o.fo[t][c]; qFol(t, c); }
    for (let t = 0; t < NTRK; t++) topModes[t] = o.tm[t]; if (repRate !== o.rr) setRepRate(o.rr, false);
    if (o.tv) { for (let t = 0; t < NTRK; t++) if (tv[t] !== o.tv[t]) { tv[t] = o.tv[t]; enqueue('tv' + t, 'tvol', t + ',' + (tv[t] * 1.5).toFixed(3)); } }
    needsRedraw = true;
}
function miscCheck() {
    const s = miscGet();
    if (miscLast === null) { miscLast = s; miscLastO = JSON.parse(s); return; }
    if (s === miscLast) return;
    const now = Date.now(), top = undoStack.length ? undoStack[undoStack.length - 1] : null;
    if (!(top && top.k === 'misc' && now - top.ts < 1500)) {
        const cur = JSON.parse(s); let lbl = 'EDIT';
        for (const k in cur) if (JSON.stringify(cur[k]) !== JSON.stringify(miscLastO[k])) { lbl = MISC_LBL[k] || lbl; break; }
        redoStack = []; undoStack.push({ k: 'misc', id: 'misc', ts: now, L: 0, t: 0, p: ed[sel], g: 0, v: miscLast, lbl: lbl });
        if (undoStack.length > 240) undoStack.shift();
    } else top.ts = now;
    miscLast = s; miscLastO = JSON.parse(s);
}
function swapParam(e, to) {
    const n = { k: e.k, id: e.id, ts: 0, L: e.L, t: e.t, p: e.p, g: e.g || 0, lbl: e.lbl };
    if (e.k === 'pad') { n.v = pads[e.L].slice(); pads[e.L] = e.v.slice(); qPad(e.L); enqueue('p' + e.L + 'vol', 'p' + p2(e.L + 1) + '_vol', (+pads[e.L][PI.vol]).toFixed(3)); enqueue('p' + e.L + 'pan', 'p' + p2(e.L + 1) + '_pan', (+pads[e.L][PI.pan]).toFixed(3)); }
    else if (e.k === 'master') { n.v = master.slice(); master = e.v.slice(); qMaster(); }
    else if (e.k === 'auto') { n.v = JSON.stringify(AUTO); AUTO = JSON.parse(e.v); autoLast = {}; dirty = true; }
    else if (e.k === 'misc') { n.v = miscGet(); miscSet(e.v); miscLast = miscGet(); miscLastO = JSON.parse(miscLast); }
    else if (e.k === 'sys') { n.v = [bpm, swing]; bpm = e.v[0]; swing = e.v[1]; enqueue('bpm', 'bpm', String(bpm)); enqueue('swing', 'swing', (0.5 + swing / 100).toFixed(3)); }
    else if (e.k === 'mute') { n.v = [mutes.slice(), lmutes.slice()]; mutes = e.v[0].slice(); lmutes = e.v[1].slice(); for (let t = 0; t < NTRK; t++) enqueue('mute' + t, 'mute', t + ',' + mutes[t]); for (let l = 0; l < NLANE; l++) { enqueue('lmute' + l, 'lmute', l + ',' + lmutes[l]); lmSent[l] = lmutes[l]; } }
    to.push(n); needsRedraw = true;
}
function swapEntry(e, to) {
    if (e.k) { swapParam(e, to); return; }
    const tk = laneTrk(e.t);
    to.push({ p: e.p, t: e.t, st: steps[e.p][e.t].map(x => x.slice()), len: lens[tk], ll: llen[e.t] || 0, g: e.g, cs: (csnd && csnd[e.p] && csnd[e.p][e.t]) ? csnd[e.p][e.t].slice() : null });
    steps[e.p][e.t] = e.st; lens[tk] = e.len;
    if (e.cs && csnd && csnd[e.p]) { csnd[e.p][e.t] = e.cs.slice(); sendCs(e.p, e.t); if (ed[tk] === e.p) { const v = pads[e.t][1], pn = pads[e.t][2]; pads[e.t] = e.cs.slice(); pads[e.t][1] = v; pads[e.t][2] = pn; qPadLive(e.t); } }       /* the sound a clip carries (copy / paste) comes back with its steps */
    enqueue('len' + tk, 'len', tk + ',' + e.len);
    if (e.ll !== undefined && (llen[e.t] || 0) !== e.ll) { llen[e.t] = e.ll; enqueue('llen' + e.t, 'llen', e.t + ',' + e.ll); }
    qSeq(e.p, e.t);
}
function doRedo() {
    const e = redoStack.pop();
    if (!e) { showToast('REDO', 'NOTHING', 0); return; }
    swapEntry(e, undoStack); miscLast = miscGet(); miscLastO = JSON.parse(miscLast);
    while (e.g && redoStack.length && redoStack[redoStack.length - 1].g === e.g) swapEntry(redoStack.pop(), undoStack);
    showToast('REDO', e.k ? ((e.k === 'pad' ? 'T' + p2(laneTrk(e.t) + 1) + ' ' : '') + String(e.lbl || e.k).toUpperCase()).substring(0, 18) : 'T' + p2(laneTrk(e.t) + 1) + ' C' + (e.p + 1), 1); markEdited();
}
function doUndo() {
    const e = undoStack.pop();
    if (!e) { showToast('UNDO', 'NOTHING (STEPS)', 0); return; }
    swapEntry(e, redoStack); miscLast = miscGet(); miscLastO = JSON.parse(miscLast);
    while (e.g && undoStack.length && undoStack[undoStack.length - 1].g === e.g) swapEntry(undoStack.pop(), redoStack);
    showToast('UNDO', e.k ? ((e.k === 'pad' ? 'T' + p2(laneTrk(e.t) + 1) + ' ' : '') + String(e.lbl || e.k).toUpperCase()).substring(0, 18) : 'T' + p2(laneTrk(e.t) + 1) + ' C' + (e.p + 1), 1); markEdited();
}
let rndDens = 0.28, rndVar = 0.5, rndKey = 0, rndRat = 0;

/* ---- sound randomizer: nudges the tone parameters of the current sound (never volume, pan, sends, type); "Undo sound" brings the old sound back ---- */
let soundBak = null;
function randomSound(wild) {
    const L = ln(), a = pads[L]; if (!a) return;
    pSnap('pad', L, 'rnd'); soundBak = { L: L, v: a.slice() };
    const drum = !!kind[sel] || L >= 0 && !!kind[laneTrk(L)];
    const set = drum ? [['tune', 0.22], ['decay', 0.35], ['tone', 0.5], ['drive', 0.3], ['crush', 0.15], ['ring', 0.12]]
                     : [['cut', 0.5], ['res', 0.35], ['fenv', 0.5], ['det', 0.3], ['sub', 0.3], ['att', 0.12], ['decay', 0.3], ['tone', 0.4], ['drive', 0.25], ['wave', 0.4]];
    const k = wild ? 1.6 : 0.8;
    for (const e of set) {
        const i = PK.indexOf(e[0]); if (i < 0) continue;
        const cur = +a[i] || 0;
        a[i] = clamp(Math.round((cur + (Math.random() - 0.5) * 2 * e[1] * k) * 100) / 100, 0, 1);
    }
    qPad(L); markEdited(); needsRedraw = true;
    showToast(wild ? 'RANDOM SOUND' : 'VARY SOUND', wild ? 'WILD' : 'MILD', 1);
}
function undoSound() {
    if (!soundBak || !pads[soundBak.L]) { showToast('UNDO SOUND', 'NOTHING', 0); return; }
    pads[soundBak.L] = soundBak.v.slice(); qPad(soundBak.L); markEdited(); soundBak = null; needsRedraw = true;
    showToast('UNDO SOUND', 'RESTORED', 1);
}
function allLoops16() {                                   /* every track and every drum sound back to the standard 16 steps (recorded steps beyond step 16 stay stored, just not played) */
    for (let t = 0; t < NTRK; t++) if (lens[t] !== 16) { lens[t] = 16; enqueue('len' + t, 'len', t + ',16'); }
    for (let l = 0; l < NLANE; l++) if (llen[l]) { llen[l] = 0; enqueue('llen' + l, 'llen', l + ',0'); }
    stepPage = 0; markEdited(); needsRedraw = true; showToast('ALL LOOPS', '16 STEPS', 1);
}
function randomize(wild) {
    const L = ln(), c = ed[sel], n = LL(), bt = BASE_T[Math.round(pads[L][0])], mel = (bt >= 19 && bt <= 27) || (bt >= 73 && bt <= 80);
    snap(c, L);
    const dens = wild ? Math.min(0.9, rndDens * 1.8) : rndDens, pit = [0, 0, 3, 5, 7, 12, -12];
    for (let s = 0; s < MAXSTEP; s++) {
        if (s < n && Math.random() < dens) {
            const semi = !mel ? 0 : (rndKey ? notePadSemi(Math.floor(Math.random() * 9)) : pit[Math.floor(Math.random() * pit.length)]);
            const v = clamp(Math.round(127 - Math.random() * rndVar * 80), 20, 127);
            steps[c][L][s] = [1, v, semi, 0, Math.random() < rndRat ? 1 : 0, 0, 0, 0];
        } else steps[c][L][s] = [0, 100, 0, 0];
    }
    qSeq(c, L); showToast('RANDOM', (wild ? 'WILD ' : 'SPARSE ') + Math.round(dens * 100) + '%', dens); markEdited();
}
function trackVol(t, dl0) {
    if (!dl0) return;
    const v = clamp(getTV(t) + Math.sign(dl0) * Math.min(Math.abs(dl0), 3) * 0.02, 0, 1);
    setTV(t, v);
    trkUsed = true; showToast('VOLUME', String(Math.round(v * 100)), v); markEdited();
}
function suppressMasterVol(v) {                           /* while a track button is held the module (not Move) owns the volume knob */
    try { if (typeof shadow_set_overtake_suppress_master_volume === 'function') shadow_set_overtake_suppress_master_volume(v); } catch (e) { /* ignore */ }
}
function octMove(d) {
    const o = clamp(octS[sel] + d, -3, 3);
    if (o === octS[sel]) { showToast('OCTAVE', (o > 0 ? '+' : '') + o + (d > 0 ? ' (MAX)' : ' (MIN)'), (o + 3) / 6); return; }
    octS[sel] = o; enqueue('padoct', 'padoct', String(oct())); heldPad = -1; needsRedraw = true;
    showToast('OCTAVE', (o > 0 ? '+' : '') + o, (o + 3) / 6);
}
function stepTrack(d) {                                   /* Up/Down = track above / below, the 4 track buttons follow */
    const t = (sel + d + NTRK) % NTRK;
    selectTrack(t);
    showToast('TRACK ' + p2(t + 1), trkName(t), t / (NTRK - 1));
}
function goBank(b, tr) {
    bank = clamp(b, 0, 3);
    selectTrack(bank * 4 + (view === 7 && tr !== undefined ? tr : 0));
    showToast(view === 7 ? 'BANK' : 'TRACK', view === 7 ? 'T' + p2(bank * 4 + 1) + '-' + p2(bank * 4 + 4) : p2(sel + 1), bank / 3);
    needsRedraw = true;
}
function sessionKnob(i, delta) {
    const t = bank * 4 + (i & 3), k = i >= 4 ? 'pan' : 'vol';
    trackKnob(t, k, delta * 0.025);
}
function trackKnob(t, k, d) {                               /* volume / pan of a track (drum tracks: volume of the whole kit, pan stays per sound) */
    if (k === 'pan' && kind[t]) { showToast('PAN', 'PER SOUND', 0); return; }
    const v = clamp((k === 'pan' ? pads[t][PI.pan] : getTV(t)) + d, 0, 1);
    if (k === 'pan') { pads[t][PI.pan] = v; enqueue('p' + t + 'pan', 'p' + p2(t + 1) + '_pan', v.toFixed(3)); } else setTV(t, v);
    showToast(k === 'pan' ? 'PAN' : 'VOLUME', fmtVal(k, v), v); markEdited();
}

function clipOpDone(label, f, all) { if (all) qSeqT(ed[sel], sel); else qSeq(ed[sel], ln()); enqueue('len' + sel, 'len', sel + ',' + lens[sel]); stepPage = Math.min(stepPage, pageCount() - 1); showToast(label, '', f === undefined ? 1 : f); markEdited(); }
function copySound(a, b) {
    const Ls = kind[a] ? laneOf(a, dsel[a]) : a, Ld = kind[b] ? laneOf(b, dsel[b]) : b;
    if (Ls === Ld) return;
    pSnap('pad', Ld, 'copy'); pads[Ld] = pads[Ls].slice(); qPad(Ld); markEdited();
    showToast('SOUND COPIED', 'T' + p2(a + 1) + ' > T' + p2(b + 1), 1);
}
function copyTrackFull(a, b) {                              /* Shift + Copy + track button: the whole track (sounds, every clip, length) goes to the other track */
    if (a === b) return;
    if (!!kind[a] !== !!kind[b]) { showToast('COPY TRACK', 'SYNTH<>DRUM', 0); return; }
    const la = lanesOf(a), lb = lanesOf(b);
    undoGroup++;
    for (let j = 0; j < la.length && j < lb.length; j++) {
        pSnap('pad', lb[j], 'copy'); pads[lb[j]] = pads[la[j]].slice(); qPad(lb[j]);
        for (let c = 0; c < NPAT; c++) { snap(c, lb[j], undoGroup); steps[c][lb[j]] = steps[c][la[j]].map(x => x.slice()); qSeq(c, lb[j]); }
        llen[lb[j]] = llen[la[j]]; enqueue('llen' + lb[j], 'llen', lb[j] + ',' + llen[lb[j]]);
    }
    lens[b] = lens[a]; enqueue('len' + b, 'len', b + ',' + lens[b]);
    showToast('TRACK COPIED', 'T' + p2(a + 1) + ' > T' + p2(b + 1), 1); markEdited(); needsRedraw = true;
}
function opVary() {                                        /* a variation of the clip: a few steps appear, a few vanish, some get a different velocity - the groove stays recognisable */
    const L = ln(), c = ed[sel], n = LL(); snap(c, L);
    const on = []; for (let s = 0; s < n; s++) if (steps[c][L][s][0]) on.push(s);
    if (!on.length) { showToast('VARIATION', 'CLIP IS EMPTY', 0); return; }
    let ch = 0;
    for (let s = 0; s < n; s++) {
        const st = steps[c][L][s], r = Math.random();
        if (st[0]) { if (r < 0.1 && s !== on[0]) { steps[c][L][s] = [0, 100, 0, 0]; ch++; } else if (r < 0.3) { st[1] = clamp(st[1] + Math.round((Math.random() - 0.5) * 40), 25, 127); ch++; } }
        else if (r < 0.07) { const src = steps[c][L][on[Math.floor(Math.random() * on.length)]]; steps[c][L][s] = [1, clamp(Math.round(src[1] * 0.75), 25, 127), src[2], 0, 0, 0, 0, 0]; ch++; }
    }
    qSeq(c, L); showToast('VARIATION', ch + ' CHANGES', Math.min(1, ch / 8)); markEdited(); needsRedraw = true;
}
function clearTrack(t) {                                   /* Delete + track button: every clip of the track (all 16 sounds of a drum track), incl. recorded motion */
    undoGroup++; let n = 0; const ls = lanesOf(t);
    for (let c = 0; c < NPAT; c++) for (const l of ls) if (laneHas(c, l)) { snap(c, l, undoGroup); for (let s = 0; s < MAXSTEP; s++) steps[c][l][s] = [0, 100, 0, 0]; qSeq(c, l); n++; }
    for (const id in AUTO) { const L = parseInt(id.split('|')[1]); if (ls.indexOf(L) >= 0) { delete AUTO[id]; delete autoLast[id]; } }
    showToast('TRACK ' + p2(t + 1) + ' CLEARED', n ? n + ' CLIPS/SOUNDS' : 'WAS EMPTY', n ? 1 : 0); resetLen(t); chosen[t] = 0; markEdited(); needsRedraw = true;      /* a cleared track asks Synth or Drum again */
}
function opClear() { const c = ed[sel], L = ln(); snap(c, L); for (let s = 0; s < MAXSTEP; s++) steps[c][L][s] = [0, 100, 0, 0]; if (kind[sel] && llen[L]) { llen[L] = 0; enqueue('llen' + L, 'llen', L + ',0'); } else if (!trackHasSteps(c, sel)) resetLen(sel); clipOpDone('CLEARED'); }
function opReverse() { const c = ed[sel], L = ln(), n = LL(); snap(c, L); const a = steps[c][L].slice(0, n).reverse(); for (let s = 0; s < n; s++) steps[c][L][s] = a[s]; clipOpDone('REVERSE'); }
function opRotate(d) { const c = ed[sel], L = ln(), n = LL(); snap(c, L); const a = steps[c][L].slice(0, n); for (let s = 0; s < n; s++) steps[c][L][s] = a[((s - d) % n + n) % n]; clipOpDone(d > 0 ? 'ROTATE >' : 'ROTATE <'); }
function opDouble() { const c = ed[sel], n = lens[sel]; if (n * 2 > MAXSTEP) { showToast('DOUBLE', 'MAX ' + MAXSTEP, 1); return; } snapAll(c, sel); for (const l of lanesOf(sel)) { const m = effLen(l); if (m * 2 > MAXSTEP) continue; for (let s = 0; s < m; s++) steps[c][l][s + m] = steps[c][l][s].slice(); if (llen[l]) { llen[l] = m * 2; enqueue('llen' + l, 'llen', l + ',' + llen[l]); } } lens[sel] = n * 2; clipOpDone('DOUBLE x2', 1, true); }
const Q_GRID = [['1/4', 4], ['1/8', 2], ['1/8T', 4 / 3], ['1/16', 1], ['1/16T', 2 / 3], ['1/32', 0.5]];
let qGrid = 3, qStr = 3;                                     /* time quantize: grid (in steps) and strength 25/50/75/100 % */
function qSnapPos(p) { const g = Q_GRID[qGrid][1]; return Math.round(p / g) * g; }
function opTimeQ() {                                        /* move every note of this sound toward the grid (1/4 ... 1/32, triplets), the offset inside a step is kept as nudge */
    const c = ed[sel], L = ln(), n = LL(), str = [0.25, 0.5, 0.75, 1][qStr]; snap(c, L);
    const row = steps[c][L], out = []; let moved = 0, lost = 0;
    for (let s = 0; s < n; s++) { const st = row[s]; if (!st || !st[0]) continue; const pos = s + (st[6] | 0) * 0.025; let np = pos + (qSnapPos(pos) - pos) * str; let ns = Math.floor(np + 1e-6), fr = np - ns, nd = Math.round(fr / 0.025); if (nd > NUD_MAX) { ns++; nd = 0; } out.push({ s, st, ns: ((ns % n) + n) % n, nd }); }
    const fresh = () => [0, 100, 0, 0, 0, 0, 0, 0], res = new Array(n).fill(null);
    for (const o of out) { if (!res[o.ns]) { res[o.ns] = o; } else if (!res[o.s]) { res[o.s] = { s: o.s, st: o.st, ns: o.s, nd: o.st[6] | 0 }; lost++; } else lost++; }
    for (let s = 0; s < n; s++) { const o = res[s]; if (o) { const st = o.st.slice(); st[6] = o.nd; if (o.ns !== o.s || o.nd !== (o.st[6] | 0)) moved++; row[s] = st; } else if (row[s] && row[s][0]) row[s] = fresh(); }
    clipOpDone('QUANT ' + Q_GRID[qGrid][0], 1); showToast('QUANT ' + Q_GRID[qGrid][0], moved + ' MOVED ' + [25, 50, 75, 100][qStr] + '%', 1);
}
function opScaleQ() {                                      /* snap every note of the clip to the current key + scale */
    const c = ed[sel], L = ln(), set = SCALE7[Math.round(master[MI.scale]) % 3].concat([12]); let n = 0;
    snap(c, L);
    for (let s = 0; s < LL(); s++) {
        const st = steps[c][L][s]; if (!st[0]) continue;
        const o = Math.floor(st[2] / 12), r = ((st[2] % 12) + 12) % 12; let best = set[0], bd = 99;
        for (const d of set) { const dd = Math.abs(d - r); if (dd < bd) { bd = dd; best = d; } }
        const ns = clamp(o * 12 + best, -64, 63); if (ns !== st[2]) { st[2] = ns; n++; }
    }
    clipOpDone('SCALE Q', 1); showToast('SCALE Q', n + ' NOTES', 1);
}
function opHalf() { const c = ed[sel], L = ln(), n = LL(); if (n < 2) return; snap(c, L); if (llen[L]) { llen[L] = n >> 1; enqueue('llen' + L, 'llen', L + ',' + llen[L]); } else lens[sel] = n >> 1; clipOpDone('HALF /2'); }
function opProb(mode) {                                   /* probability of all notes of the clip: 0 random, 1 curve (on-beats sure, off-beats lower), 2 everything 100% */
    const c = ed[sel], L = ln(), n = LL(); snap(c, L); let k = 0;
    for (let s = 0; s < n; s++) {
        const st = stepAt(s); if (!st[0]) continue;
        st[3] = mode === 2 ? 0 : mode === 1 ? (s % 4 === 0 ? 0 : (s % 4 === 2 ? 6 : 14)) : (s % 4 === 0 ? Math.floor(Math.random() * 4) : 2 + Math.floor(Math.random() * 18));
        k++;
    }
    if (!k) { showToast('PROBABILITY', 'NO NOTES', 0); return; }
    clipOpDone(mode === 2 ? 'PROB 100%' : mode === 1 ? 'PROB CURVE' : 'PROB RANDOM');
}
function opPump() {                                        /* sidechain preset: the sound ducks on every kick, short release */
    const L = ln(); pSnap('pad', L, 'pump'); soundBak = { L: L, v: pads[L].slice() }; setLaneP(L, 'duck', 0.7); setLaneP(L, 'duckr', 0.35); setLaneP(L, 'scs', 0);
    markEdited(); needsRedraw = true; showToast('PUMP ON KICK', 'DUCK 70% REL 35%', 0.7);
}
let stepClip = null;
function stepCopyKey(s) {                                  /* Copy held + step: first press = pick, next presses = paste there (with every value) */
    const c = ed[sel], L = ln();
    if (s >= LL()) return;
    if (!stepClip || (stepClip.c === c && stepClip.L === L && stepClip.s === s)) { stepClip = { c: c, L: L, s: s, d: stepAt(s).slice() }; showToast('STEP ' + (s + 1), 'COPIED - PRESS TARGET', 0.5); return; }
    snap(c, L); steps[c][L][s] = stepClip.d.slice(); qStep(c, L, s); markEdited(); showToast('STEP ' + (stepClip.s + 1) + ' > ' + (s + 1), 'PASTED', 1); needsRedraw = true;
}
function loadCounts() {                                    /* what runs at once: sounds with notes, sounds with effects, reverb / delay sends - helps to find the cause of crackling */
    let ns = 0, nf = 0, nr = 0, nd = 0;
    for (let l = 0; l < NLANE; l++) {
        let has = false; for (let p = 0; p < NPAT && !has; p++) if (laneHas(p, l)) has = true;
        if (!has) continue; ns++; const q = pads[l];
        if (q[PI.rvb] > 0.02) nr++; if (q[PI.dly] > 0.02) nd++;
        if (Math.round(q[PI.xfx]) > 0 || Math.round(q[PI.mfx]) > 0 || Math.round(q[PI.x2fx]) > 0) nf++;
    }
    return [ns, nf, nr, nd];
}
let cpuHist = [], cpuHT = 0;
function loadTick() { if (view !== 51 || menuOpen) return; if (++cpuHT % 6) return; cpuHist.push(cpuAvg); if (cpuHist.length > 64) cpuHist.shift(); needsRedraw = true; }
function drawLoad() {
    titleBar('CPU', cpuAvg >= 75 ? 'HIGH' : 'OK');
    txtH(4, 14, cpuAvg + '%', 1); txt(60, 14, 'PEAK ' + cpuPeak + '%', 1);
    draw_rect(2, 24, 124, 7, 1); fill_rect(4, 26, Math.round(120 * clamp(cpuAvg / 100, 0, 1)), 3, 1);
    const pk = Math.round(120 * clamp(cpuPeak / 100, 0, 1)); fill_rect(3 + pk, 22, 1, 11, 1);
    fill_rect(2, 44, 64, 1, 1); fill_rect(2, 34, 1, 11, 1);
    for (let i = 0; i < cpuHist.length; i++) { const h = Math.round(10 * clamp(cpuHist[i] / 100, 0, 1)); if (h > 0) fill_rect(3 + i, 44 - h, 1, h, 1); }
    const c = loadCounts(); txt(70, 36, c[0] + ' SOUNDS', 1); txt(70, 45, c[1] + ' FX ' + c[2] + ' RV', 1);
    txt(2, 54, c[3] + ' DLY   ' + (playing ? 'PLAYING' : 'STOPPED'), 1);
}
function opAB() {                                         /* first press stores the sound as A; every next press swaps A and the sound you edited (B) */
    const L = ln();
    if (!abSlot || abSlot.L !== L) { abSlot = { L: L, v: pads[L].slice() }; morphBase = null; morphV = 0; showToast('A STORED', 'EDIT, THEN A/B', 0.5); return; }
    pSnap('pad', L, 'ab'); soundBak = { L: L, v: pads[L].slice() };
    const t = pads[L].slice(); pads[L] = abSlot.v.slice(); abSlot.v = t; morphBase = null; morphV = 0;
    qPad(L); markEdited(); needsRedraw = true; showToast('A/B', 'SWAPPED', 1);
}
function morphTurn(delta) {                                /* Morph knob: blends the sound between the stored A and what it is now; steps (types, modes) switch at the middle */
    const L = ln();
    if (!abSlot || abSlot.L !== L) { showToast('MORPH', 'STORE A FIRST (A/B)', 0); return; }
    if (!morphBase || morphBase.L !== L) { morphBase = { L: L, v: pads[L].slice() }; morphV = 0; pSnap('pad', L, 'morph'); soundBak = { L: L, v: pads[L].slice() }; }
    morphV = clamp(morphV + delta * 0.01, 0, 1);
    const DISC = { type: 1, arp: 1, rate: 1, wave: 1, poly: 1, dtime: 1, xfx: 1, mfx: 1, x2fx: 1, m2fx: 1, smp: 1, scs: 1, fmode: 1, smd: 1, tsw: 1, fkey: 1 };
    for (let i = 0; i < PK.length; i++) {
        const k = PK[i], a = morphBase.v[i], b = abSlot.v[i];
        if (a === undefined || b === undefined) continue;
        pads[L][i] = (DISC[k] || ENUMS[k]) ? (morphV >= 0.5 ? b : a) : a + (b - a) * morphV;
    }
    qPad(L); markEdited(); needsRedraw = true;
}
function monoCheck() {                                     /* master width to 0 and back: hear what is left in mono (not saved that way) */
    monoChk = !monoChk;
    if (monoChk) { monoSaved = master[MI.m_wid]; master[MI.m_wid] = 0; } else master[MI.m_wid] = monoSaved;
    enqueue('mm_wid', 'm_wid', master[MI.m_wid].toFixed(3)); needsRedraw = true; showToast('MONO CHECK', monoChk ? 'ON' : 'OFF', monoChk ? 1 : 0);
}
function sceneNameStart(c) {
    projNamesLoad(); const nm = (sceneNames[c] || 'SCENE ' + (c + 1)).toUpperCase().slice(0, PNAME_LEN).split(''); while (nm.length < PNAME_LEN) nm.push(' ');
    projUI = { i: c, mode: 2, a: 0, nm: nm.map(ch => Math.max(0, PCHARS.indexOf(ch))), cur: 0, then: 0, scn: true }; needsRedraw = true;
}
function sceneLaunchCycle() { const c = ed[sel]; sceneQ[c] = (sceneQ[c] + 1) % SQ_NAME.length; enqueue('sq' + c, 'sceneq', c + ',' + sceneQ[c]); markEdited(); showToast('SCENE ' + (c + 1) + ' LAUNCH', SQ_NAME[sceneQ[c]], sceneQ[c] / 4); }
const FOL_PRE = [['OFF', 0, 0], ['NEXT 4X', 1, 3], ['NEXT 8X', 1, 4], ['FIRST 4X', 3, 3], ['RANDOM 2X', 4, 1], ['STOP 1X', 5, 0]];
function folPreCur() { const v = fol[sel][ed[sel]]; const i = FOL_PRE.findIndex(p => (p[1] | (p[2] << 3)) === v || (p[1] === 0 && (v & 7) === 0)); return i; }
function folPreset() { const t = sel, c = ed[t], i = (folPreCur() + 1) % FOL_PRE.length, p = FOL_PRE[i]; fol[t][c] = p[1] | (p[2] << 3); qFol(t, c); markEdited(); needsRedraw = true; showToast('FOLLOW CLIP ' + (c + 1), p[0], i / 5); }
function opHuman() { const c = ed[sel], L = ln(); snap(c, L); for (let s = 0; s < lens[sel]; s++) { const st = steps[c][L][s]; if (st[0]) st[1] = clamp(st[1] + Math.round((Math.random() - 0.5) * 40), 20, 127); } clipOpDone('VEL RANDOM'); }
const SHIFT_OPS = ['CLIP1','CLIP2','CLIP3','CLIP4','CLIP5','CLIP6','CLIP7','CLIP8','RND','CLEAR','REV','ROT<','ROT>','x2','/2','VEL~'];
const SYM_VIEW = { 1: 16, 2: 15, 4: 22, 6: 23, 8: 25, 10: 26, 12: 26 };       /* shift + step i+1 opens this view: one symbol, one function */
let symOpen = -1, symPrev = 0;
function symLit(i) {
    if (symOpen !== i) return false;
    if (i === 3) return padMenu;
    return view === SYM_VIEW[i];
}
function closeSym() {
    const was = symOpen;
    symOpen = -1; menuOpen = false;
    if (was === 3 && padMenu) { padMenu = false; view = prevPM; } else { padMenu = false; view = symPrev; }
    acc.fill(0); needsRedraw = true;
}
function anySymLit() { return symOpen >= 0 && symLit(symOpen); }
const SHIFT_OK = { 3: 1, 0: 1, 1: 1, 2: 1, 4: 1, 5: 1, 6: 1, 7: 1, 8: 1, 13: 1, 9: 1, 10: 1, 11: 1, 12: 1, 14: 1, 15: 1 };      /* same symbols as the Move itself; steps 12, 13, 14 pick the pad mode directly (the current one is bright), step 4 has no function and stays dark */
const topPrev = new Array(NTRK).fill(0);                     /* the pad mode before REPEAT / STEPS was switched on */
/* ---- LIVE FX pads: hold a pad = effect on, let go = back to the old setting (synth: top 2 rows, drum: right block) ---- */
const LFX = [['REPEAT 1/4', 1], ['REPEAT 1/8', 2], ['REPEAT 1/16', 3], ['REPEAT 1/32', 4], ['ROLL', 5], ['TRIPLET', 6], ['REVERSE', 7], ['TAPE STOP', 8],
    ['GATE 1/8', 9], ['GATE 1/16', 10], ['LOW SWEEP', 11], ['HIGH SWEEP', 12], ['CRUSH', 13], ['FREEZE', 14], ['ECHO THROW', 15], ['REVERB WASH', 'rvb', 0.9]];     /* 0-14: master bus effects in the engine (hold = on), 15: reverb send of the track */
const lfxHeld = {}, lfxBase = {}; let lfxStack = [];
function lfxPad(idx) { const rr = idx >> 3; return rr >= 2 ? (rr - 2) * 8 + (idx & 7) : -1; }      /* full screen on every track: the top two rows are the 16 effects, the bottom two rows the fills / rolls */
function lfxSet(L, k, v) { const b = L + k; if (!lfxBase[b]) lfxBase[b] = { old: pads[L][PI[k]], n: 0 }; lfxBase[b].n++; lfxBusy++; setLaneP(L, k, v); lfxBusy--; }
/* FILL pads (Live FX mode on a synth track): the two bottom rows play drum sounds of the first drum track, sorted hats > snare/clap > perc > kick.
 * bottom row = single hits, row above = ROLL (held = repeats at the note repeat speed). On a drum track the 16 sound pads roll while held. */
const fillHeld = {};
function fillPad(idx) { return (idx >> 3) < 2; }
function fillLanes() {
    let t = kind[sel] ? sel : -1; if (t < 0) for (let q = 0; q < NTRK; q++) if (kind[q] && chosen[q]) { t = q; break; }      /* the fills always play the sounds of the first loaded drum track, whichever track is selected */
    if (t < 0) for (let q = 0; q < NTRK; q++) if (kind[q]) { t = q; break; }
    if (t < 0) return null;
    const out = [], quota = [[2, 3], [1, 2], [0, 1], [3, 2], [2, 8], [1, 8], [3, 8], [4, 8], [5, 8], [6, 8], [7, 8], [0, 8]];      /* hats (3), snares (2), kick, perc (2), then whatever is left */
    for (const [c, q] of quota) { let n = 0; for (let k = 0; k < 16 && out.length < 8 && n < q; k++) { const l = laneOf(t, k); if (out.indexOf(l) < 0 && catOf(Math.round(pads[l][0])) === c) { out.push(l); n++; } } }
    return out;
}
const ROLL_BY_CAT = { 0: [5, 0], 1: [8, 2], 2: [11, 1], 3: [10, 1] };      /* family -> [speed index (REP_NAME), shape]: kick 1/8 steady, snare / clap 1/16 building up, hats 1/32 gently building, perc 1/16 triplets */
function rollCode(l) { const r = ROLL_BY_CAT[catOf(Math.round(pads[l][0]))]; return r ? 2 + r[0] + 16 * r[1] : 1; }
function fillHit(idx, on, vel) {
    if (!on) { const h = fillHeld[idx]; if (h) { delete fillHeld[idx]; dspSet('dhit', h.l + ',0,0'); needsRedraw = true; } return; }
    const fl = fillLanes(); if (!fl) { showToast('FILL PADS', 'NO DRUM TRACK', 0); return; }
    const l = fl[idx & 7]; if (l === undefined) return;
    const roll = (idx >> 3) === 1, v = fullVel ? 127 : Math.max(1, vel);
    if (fillHeld[idx]) dspSet('dhit', fillHeld[idx].l + ',0,0');
    fillHeld[idx] = { l: l }; recordHit(l, 0, v); logHit(l, 0, v); flushPad(l);
    dspSet('dhit', l + ',' + v + ',' + (roll ? rollCode(l) : 0));
    showToast(roll ? 'ROLL' : 'FILL', TYPE_NAME[Math.round(pads[l][0])] || '', (idx & 7) / 7); needsRedraw = true;
}
function lfxSend() { let m = 0; for (let k = lfxStack.length - 1; k >= 0; k--) { const h = lfxHeld[lfxStack[k]]; if (h && h.m) { m = h.m; break; } } if (m !== lfxMode) { lfxMode = m; dspSet('lfx', String(m)); } }
let lfxMode = 0;
function liveFxOn(idx, n) {
    if (lfxHeld[idx]) liveFxOff(idx);
    const f = LFX[n], L = ln();
    if (f.length === 2) { lfxHeld[idx] = { m: f[1], n: n }; lfxStack.push(idx); lfxSend(); }
    else { lfxSet(L, f[1], f[2]); lfxHeld[idx] = { L: L, keys: [f[1]], n: n }; }
    showToast('LIVE FX', f[0], n / 15); needsRedraw = true;
}
function liveFxOff(idx) {
    const h = lfxHeld[idx]; if (!h) return; delete lfxHeld[idx];
    if (h.m) { lfxStack = lfxStack.filter(x => x !== idx); lfxSend(); }
    else for (const k of h.keys) { const b = lfxBase[h.L + k]; if (b && --b.n <= 0) { lfxBusy++; setLaneP(h.L, k, b.old); lfxBusy--; delete lfxBase[h.L + k]; } }
    needsRedraw = true;
}
function liveFxAllOff() { for (const k of Object.keys(fillHeld)) fillHit(+k, false, 0); for (const k of Object.keys(lfxHeld)) liveFxOff(+k); lfxStack = []; lfxSend(); }
function toggleTop(m, force) {
    if (!force && !canTop()) { showToast('PAD MODE', 'OPEN THE TRACK FIRST', 0); return; }                                    /* Shift + step 11 = note repeat on/off, Shift + step 12 = step grid on/off; off goes back to where you were (PITCH or the other mode) */
    if (m === 1 && !repAllowed) { showToast('NOTE REPEAT', 'OFF IN MENU', 0); return; }
    if (m === 4 && kind[sel]) { showToast('CHORD PADS', 'SYNTH TRACKS ONLY', 0); return; }
    if (m === 1 && !kind[sel]) { showToast('NOTE REPEAT', 'DRUM TRACKS ONLY', 0); return; }
    liveFxAllOff(); const cur = topModes[sel], name = m === 1 ? 'NOTE REPEAT' : (m === 3 ? 'LIVE FX' : (m === 4 ? 'CHORD PADS' : 'STEP GRID'));
    if (cur === m) { const back = topPrev[sel] !== m ? topPrev[sel] : 0; topModes[sel] = back; heldStep = -1; heldPad = -1; showToast(name, 'OFF  > ' + TOP_NAMES[back], 0); }
    else { topPrev[sel] = cur; topModes[sel] = m; heldStep = -1; heldPad = -1; showToast(name, 'ON', 1); }
    chordReconcile(); markEdited(); needsRedraw = true;
}
function openPresets() {
    if (askKind >= 0 || exitDlg >= 0 || fb) return;
    menuOpen = false; symOpen = -1; padMenu = false; helpOpen = false; if (view === 28 && broOpen) { broOpen = false; dspSet('brstop', '1'); }
    if (view === 7 || isHub(view) || view === 40 || view === 42 || view === 51 || SYM_VIEW_SET[view]) view = 0;
    fbOpen(); acc.fill(0); needsRedraw = true;
}
function openEffects() {
    if (askKind >= 0 || exitDlg >= 0 || fb) return;
    menuOpen = false; symOpen = -1; padMenu = false; helpOpen = false;
    if (view === 7 || isHub(view) || view === 40 || view === 42 || view === 51 || SYM_VIEW_SET[view]) view = 0;
    fbOpen(); const root = fb.st[0].node, ei = root.kids.findIndex(k => k.n === 'AUDIO FX');
    fb.st = [{ node: root, i: Math.max(0, ei) }];
    if (ei >= 0) { const A = root.kids[ei]; fb.st.push({ node: A, i: Math.max(0, A.l.indexOf(Math.round(pads[ln()][PI.xfx]))) }); }
    acc.fill(0); needsRedraw = true;
}
/* ---- DEVICES: the chain of a track / pad - MIDI FX 1 > MIDI FX 2 > SOUND > AUDIO FX 1 > AUDIO FX 2, every one its own module ---- */
let devRow = 0, devPrev = 0, fbWantFx = false;
const DEV_PAR = { mfx: ['mamt', 'mprm'], m2fx: ['m2amt', 'm2prm'], snd: ['tune', 'decay', 'tone', 'drive'], xfx: ['xmix', 'xrt', 'xdp'], x2fx: ['x2mix', 'x2rt', 'x2dp'] };
const DEV_SHORT = { mamt: 'AMT', mprm: 'PRM', m2amt: 'AMT', m2prm: 'PRM', tune: 'TUNE', decay: 'DCY', tone: 'TONE', drive: 'DRV', xmix: 'AMT', xrt: 'RATE', xdp: 'DPTH', x2mix: 'AMT', x2rt: 'RATE', x2dp: 'DPTH' };
function devOn(k) { return Math.round(pads[ln()][PI[k]]) > 0; }
function devList() {                                           /* only what is really there: MIDI devices first, then the sound, then the audio devices; a "+" row offers a free slot */
    const r = [], mi = ['mfx', 'm2fx'], au = ['xfx', 'x2fx'], fm = mi.find(k => !devOn(k)), fa = au.find(k => !devOn(k));
    if (fm) r.push({ add: fm, kind: 'MIDI' });
    for (const k of mi) if (devOn(k)) r.push({ k: k, kind: 'MIDI' });
    r.push({ k: 'snd', kind: 'SOUND' });
    for (const k of au) if (devOn(k)) r.push({ k: k, kind: 'AUDIO' });
    if (fa) r.push({ add: fa, kind: 'AUDIO' });
    return r;
}
function devCur() { const l = devList(); devRow = clamp(devRow, 0, l.length - 1); return l[devRow]; }
function devItems() { const d = devCur(), ks = d.k ? DEV_PAR[d.k] : []; return ks.map(k => [k, LONG_NAME[k] || k, DEV_SHORT[k] || k]); }
function devSync() { VIEW_ITEMS[45] = devItems(); }
function devName(d) { if (d.add) return 'ADD ...'; const L = ln(); if (d.k === 'snd') return TYPE_NAME[Math.round(pads[L][0])] || '?'; return FX_LABEL[d.k][Math.round(pads[L][PI[d.k]])]; }
function openDevices() {
    if (view === 45) { view = devPrev; fb = null; acc.fill(0); needsRedraw = true; return; }
    if (askKind >= 0 || exitDlg >= 0 || fb) return;
    menuOpen = false; symOpen = -1; padMenu = false; helpOpen = false; navStack = [];
    devPrev = (view === 7 || view === 31 || isHub(view) || SYM_VIEW_SET[view] || view === 40 || view === 42 || view === 51) ? 0 : view;
    view = 45; devRow = devList().findIndex(d => d.k === 'snd'); devSync(); acc.fill(0); showToast('DEVICES', 'TURN = PICK  CLICK = CHANGE', 0.5); needsRedraw = true;
}
function devMove(dl) { devRow += dl > 0 ? 1 : -1; devCur(); devSync(); acc.fill(0); needsRedraw = true; }
function devClick() {
    const d = devCur();
    if (d.k === 'snd') { fbOpen(); acc.fill(0); needsRedraw = true; return; }
    const k = d.add || d.k;
    fbWantFx = true; fbOpen(); fbWantFx = false;
    const root = fb.st[0].node, ei = root.kids.findIndex(q => q.n === (k === 'xfx' || k === 'x2fx' ? 'AUDIO FX' : 'MIDI FX'));
    if (ei < 0) { fb = null; return; }
    const A = root.kids[ei]; A.fx = k; fb.dev = true;
    fb.st = [{ node: root, i: ei }, { node: A, i: Math.max(0, A.l.indexOf(Math.round(pads[ln()][PI[k]]))) }];
    acc.fill(0); needsRedraw = true;
}
function devClear() {
    const d = devCur(); if (!d.k || d.k === 'snd') { showToast(d.k ? 'SOUND' : 'DEVICES', d.k ? 'CANNOT BE REMOVED' : 'CLICK TO ADD', 0); return; }
    setLaneP(ln(), d.k, 0); showToast(d.kind, 'REMOVED', 0); devCur(); devSync(); needsRedraw = true;
}
function drawDevices() {
    titleBar('DEVICES', p2(sel + 1) + ' ' + (kind[sel] ? 'D' + ((dsel[sel] | 0) + 1) : 'SYNTH'));
    const l = devList(); devCur();
    for (let r = 0; r < l.length; r++) {
        const d = l[r], y = 10 + r * 9, on = r === devRow;
        if (on) fill_rect(0, y, 128, 9, 1);
        const c = on ? 0 : 1;
        txt(3, y + 1, d.kind, c);
        txt(40, y + 1, devName(d).substring(0, 14), c);
        if (d.k && d.k !== 'snd') fill_rect(121, y + 3, 4, 4, c);
    }
    const items = devItems();
    fill_rect(0, 55, 128, 1, 1);
    items.forEach((it, i) => { const cw = 32, v = getVal('pad', it[0]), x = i * cw, hotE = hot.t > 0 && hot.v === 45 && hot.i === i; const cc = 1; txt(x + 2, 56, String(hotE ? fmtVal(it[0], v) : it[2]).substring(0, 5), cc); const w = Math.round((cw - 4) * clamp(fracVal(it[0], v), 0, 1)); if (!hotE && w > 0) fill_rect(x + 2, 63, w, 1, 1); });
    if (!items.length) txt(3, 56, 'CLICK = CHOOSE', 1);
}
function shiftStep(i) {
    if (!SHIFT_OK[i]) return;                                       /* only symbols with a real function light up */
    if (i === 0 && view === 7) { songOpen(true); return; }          /* step 1 inside the clips: on to the SONG */
    if (i === 0 && view === 31 && songFromClips) { view = 7; songFromClips = false; acc.fill(0); needsRedraw = true; return; }
    if (i === 13) { openFav(); return; }                              /* step 14: your favorite knobs */
    if (i === 0) { projOpen(); return; }                              /* step 1: PROJECT (slots, new, revert) */
    if ((i === 3 || i === 7 || i === 11) && view === 7 && !menuOpen) { view = prevView; if (SYM_VIEW_SET[view] || view === 7) view = 0; symOpen = -1; acc.fill(0); needsRedraw = true; }      /* from the clip view: leave it for the track's own pads, then switch their mode (Live FX, pad mode, Repeat, Steps) */
    if (i === 3) { if (!canTop()) { showToast('LIVE FX', 'OPEN THE TRACK FIRST', 0); return; } toggleTop(3); return; }      /* step 4: LIVE FX pads on / off */
    if (i === 7) { cycleTop(1); return; }                             /* step 8: LAYOUT = the three pad modes PITCH > REPEAT > STEPS */
    if (i === 11) { toggleTop(2); return; }                          /* step 12: step grid on / off (the 16 pads become steps) */
    if (i === 5) { const v = cycleMaster('m_met', [0, 0.6]); showToast('METRONOME', v > 0 ? 'ON' : 'OFF', v); return; }
    if (i === 9) { fullVel = !fullVel; showToast('FULL VELOCITY', fullVel ? 'ON' : 'OFF', fullVel ? 1 : 0); markEdited(); return; }
    if (i === 14) { opDouble(); return; }
    if (i === 15) { opTimeQ(); return; }                            /* step 16: QUANT = quantize the timing right now with the grid + strength of the QUANT tile (Shift + step 3) */
    if (symLit(i)) { closeSym(); return; }                          /* same symbol again = close its menu */
    navStack = [];
    if (!anySymLit()) symPrev = (view === 7 || SYM_VIEW_SET[view]) ? 0 : (padMenu ? prevPM : view);
    padMenu = false; menuOpen = false;
    symOpen = i;
    if (i === 1) { navStack = []; hubReturn = 0; hubSel[16] = 0; view = 16; }
    else if (i === 3) { prevPM = padView(view) ? 0 : (SYM_VIEW_SET[view] ? symPrev : view); padMenu = true; view = 1; showToast('TRACK ' + p2(sel + 1), 'KNOBS = SOUND', 0); }
    else { view = SYM_VIEW[i]; if (i === 4) showToast('TEMPO', bpm + ' BPM', (bpm - 40) / 200); }
    acc.fill(0); needsRedraw = true;
}
const SYM_VIEW_SET = {}; Object.keys(SYM_VIEW).forEach(k => SYM_VIEW_SET[SYM_VIEW[k]] = true);

function setView(v) { if (v === 39) { v = 38; fxPage = 1; } view = v; acc.fill(0); needsRedraw = true; }
/* ---- FAVORITES: 8 knobs you choose yourself (any parameter from any page) ---- */
let favK = ['cut', 'res', 'decay', 'tone', 'drive', 'dly', 'rvb', 'm_filt'], favNext = 0, lastKnob = '';
let KEYNAME = null;
function keyName(k) {
    if (!KEYNAME) { KEYNAME = {}; for (const v in VIEW_ITEMS) for (const it of VIEW_ITEMS[v]) if (it && it[0] && !KEYNAME[it[0]]) KEYNAME[it[0]] = [it[1] || it[0], it[2] || it[1] || it[0]]; }
    return KEYNAME[k] || [k, k];
}
/* CHORD pads (synth tracks): the pads play chords through the track's MIDI effect, and while this mode is on the favorite knobs become the chord options */
const CH_PG = [['mfx', 'Chord Mode', 'Mode'], ['mprm', 'Chord', 'Chrd'], ['mamt', 'Level', 'Lvl'], ['m2fx', 'Layer FX', 'Lyr'], ['m2amt', 'Layer Amt', 'LAmt'], ['m2prm', 'Layer Prm', 'LPrm'], ['mch', 'Prob'], ['mvb', 'VelBias', 'VelB']];
let chBak = new Array(16).fill(null), chOn = new Array(16).fill(0), favSig = 0;
function chordOn() { return !kind[sel] && topModes[sel] === 4; }
function chordReconcile() {                                   /* switching the mode on sets the track's MIDI effect to CHORD (the old one is remembered), switching it off brings the old one back */
    for (let t = 0; t < NTRK; t++) {
        const want = !kind[t] && topModes[t] === 4 ? 1 : 0;
        if (kind[t] && topModes[t] === 4) topModes[t] = 0;
        if (!kind[t] && topModes[t] === 1) topModes[t] = 0;
        if (want === chOn[t]) continue;
        chOn[t] = want; const a = pads[t]; if (!a) continue;
        pSnap('pad', t, 'chordmode');
        if (want) {
            const m = Math.round(a[PI.mfx]);
            if (m !== 2 && m !== 11) { chBak[t] = [a[PI.mfx], a[PI.mamt], a[PI.mprm]]; a[PI.mfx] = 2; a[PI.mamt] = 0.7; a[PI.mprm] = 0; } else chBak[t] = null;
        } else if (chBak[t]) { a[PI.mfx] = chBak[t][0]; a[PI.mamt] = chBak[t][1]; a[PI.mprm] = chBak[t][2]; chBak[t] = null; }
        qPad(t); markEdited(); needsRedraw = true;
        if (t === sel) { favBuild(); if (want) showToast('CHORD MODE', 'LEFT = NOTES, RIGHT = CHORDS', 1); else if (view === 44) tabEnter(1); }      /* the page stays where it is: Shift + step 8 only switches the mode */
    }
    if (favSig !== (chordOn() ? 1 : 0)) favBuild();
}
function favBuild() { favSig = chordOn() ? 1 : 0; VIEW_ITEMS[44] = favSig ? CH_PG : favK.map(k => { const n = keyName(k); return [k, LONG_NAME[k] || n[0], n[1]]; }); }
function favPin() {
    if (!lastKnob || scopeOf(lastKnob) === 'sys') { showToast('PIN KNOB', 'TURN A KNOB FIRST', 0); return; }
    let at = favK.indexOf(lastKnob); if (at < 0) { at = favNext; favNext = (favNext + 1) & 7; favK[at] = lastKnob; }
    favBuild(); markEdited(); showToast('PINNED ' + keyName(lastKnob)[1].toUpperCase(), 'SLOT ' + (at + 1) + ' OF 8', 1);
}
function favReset() { favK = ['cut', 'res', 'decay', 'tone', 'drive', 'dly', 'rvb', 'm_filt']; favNext = 0; favBuild(); markEdited(); showToast('FAVORITES', 'RESET', 0); }
function openFav() { if (askKind >= 0 || exitDlg >= 0 || fb) return; menuOpen = false; symOpen = -1; padMenu = false; helpOpen = false; favBuild(); setView(44); }

function cycleMaster(key, vals) {
    const cur = master[MI[key]]; let k = vals.findIndex(x => Math.abs(x - cur) < 0.05); k = (k + 1) % vals.length;
    master[MI[key]] = vals[k]; enqueue('m' + key, key, vals[k].toFixed(3)); markEdited(); return vals[k];
}
const THEME_NAME = ['CLASSIC', 'MINIMAL', 'SYMBOLS', 'NATURE', 'KIDS', 'TECH', 'SPACE', 'RETRO', 'ANALOG', 'KNOBS'];
/* ---- languages: ENGLISH plus five more. Names, menus and pages are looked up in this table when they are drawn (ASCII only, short forms: the screen is small) ---- */
const LANGS = ['ENGLISH', 'DEUTSCH', 'FRANCAIS', 'ESPANOL', 'ITALIANO', 'PORTUGUES'];
let lang = 0, langBefore = 0;
const TRD = {};
`type=Typ|Type|Tipo|Tipo|Tipo
tune=Stimm|Accord|Afinar|Accord.|Afinar
tone=Klang|Timbre|Tono|Timbro|Tom
voices=Stimmen|Voix|Voces|Voci|Vozes
keytrack=Tastatur|Suivi|Seguim|Inseg|Seguim
mode=Modus|Mode|Modo|Modo|Modo
filter=Filter|Filtre|Filtro|Filtro|Filtro
satur=Saett|Satur|Satur|Satur|Satur
width=Breite|Largeur|Ancho|Ampiez|Largura
limiter=Limiter|Limiteur|Limit|Limit|Limit
send=Send|Envoi|Envio|Send|Envio
time=Zeit|Temps|Tiempo|Tempo|Tempo
size=Groesse|Taille|Tamano|Dimens|Tamanho
dark=Dunkel|Sombre|Oscuro|Scuro|Escuro
attack=Attack|Attaque|Ataque|Attacco|Ataque
human=Human|Humain|Humano|Umano|Humano
launch=Start|Lancer|Lanzar|Lancio|Lancar
key=Tonart|Tonalite|Tonalid|Tonalita|Tonalid
scale=Skala|Gamme|Escala|Scala|Escala
scaleq=SkalaQ|GammeQ|EscalaQ|ScalaQ|EscalaQ
rate=Rate|Vitesse|Veloc|Veloc|Veloc
depth=Tiefe|Profond|Profund|Profond|Profund
kind=Art|Genre|Tipo|Tipo|Tipo
volume=Lautstaerke|Volume|Volumen|Volume|Volume
sound=Klang|Son|Sonido|Suono|Som
color=Farbe|Couleur|Color|Colore|Cor
effect=Effekt|Effet|Efecto|Effetto|Efeito
amount=Menge|Quantite|Cantid|Quantita|Quantid
prob=Wahrsch|Prob|Prob|Prob|Prob
file=Datei|Fichier|Archivo|File|Arquivo
start=Start|Debut|Inicio|Inizio|Inicio
end=Ende|Fin|Final|Fine|Fim
mark=Marke|Marque|Marca|Marca|Marca
count=Anzahl|Nombre|Cuenta|Numero|Conta
add=Neu|Ajout|Anadir|Aggiung|Adicion
delete=Loesch|Suppr|Borrar|Elimin|Apagar
to pads=Zu Pads|Vers Pad|A Pads|Ai Pad|P/ Pads
source=Quelle|Source|Fuente|Sorgent|Fonte
scene=Szene|Scene|Escena|Scena|Cena
bars=Takte|Mesures|Compas|Battute|Compas
repeat=Wdh|Repet|Repet|Ripet|Repet
change=Wechsel|Change|Cambio|Cambio|Mudanca
name=Name|Nom|Nombre|Nome|Nome
stretch=Dehnen|Etirer|Estirar|Allung|Esticar
record=Aufnahme|Enreg|Grabar|Registra|Gravar
state=Status|Etat|Estado|Stato|Estado
save=Sichern|Sauver|Guardar|Salva|Salvar
load=Laden|Charger|Cargar|Carica|Carregar
listen=Hoeren|Ecouter|Escuchar|Ascolta|Ouvir
new=Neu|Nouveau|Nuevo|Nuovo|Novo
revert=Zurueck|Annuler|Revertir|Ripristina|Reverter
hits=Treffer|Coups|Golpes|Colpi|Batidas
rotate=Drehen|Rotation|Rotar|Ruota|Rodar
length=Laenge|Longueur|Largo|Lunghez|Compr
mutate=Mutieren|Muter|Mutar|Muta|Mutar
clock=Takt|Horloge|Reloj|Clock|Relogio
track=Spur|Piste|Pista|Traccia|Faixa
notes=Noten|Notes|Notas|Note|Notas
all=Alle|Tout|Todo|Tutto|Tudo
channel=Kanal|Canal|Canal|Canale|Canal
velocity=Anschlag|Velocite|Velocid|Velocita|Velocid
drum note=Drumnote|Note batt|Nota bat|Nota bat|Nota bat
reverb=Hall|Reverb|Reverb|Reverbero|Reverb
dlytime=DlyZeit|DlyTemps|DlyTiemp|DlyTempo|DlyTempo
low=Tiefen|Graves|Graves|Bassi|Graves
mid=Mitten|Medios|Medios|Medi|Medios
high=Hoehen|Aigus|Agudos|Alti|Agudos
lowfreq=TiefenF|GravesF|GravesF|BassiF|GravesF
midfreq=MittenF|MediosF|MediosF|MediF|MediosF
highfreq=HoehenF|AigusF|AgudosF|AltiF|AgudosF
midwidth=MittenQ|MediosQ|MediosQ|MediQ|MediosQ
eq out=EQ Aus|EQ Sortie|EQ Salida|EQ Uscita|EQ Saida
dlytype=DlyTyp|DlyType|DlyTipo|DlyTipo|DlyTipo
rvbtype=HallTyp|RvbType|RvbTipo|RvbTipo|RvbTipo
output=Ausgang|Sortie|Salida|Uscita|Saida
dlytone=DlyKlang|DlyTimbre|DlyTono|DlyTimbro|DlyTom
dlywide=DlyBreit|DlyLarge|DlyAncho|DlyAmpio|DlyLarg
rvbwide=HallBreit|RvbLarge|RvbAncho|RvbAmpio|RvbLarg
rvblow=HallTief|RvbGrave|RvbGrave|RvbBasso|RvbGrave
noise=Rauschen|Bruit|Ruido|Rumore|Ruido
fit tempo=Tempo anp|Ajust|Ajustar|Adatta|Ajustar
half=Halb|Moitie|Mitad|Meta|Metade
double=Doppelt|Double|Doble|Doppio|Dobro
move=Schieben|Deplac|Mover|Sposta|Mover
random=Zufall|Hasard|Azar|Casuale|Aleat
reverse=Rueckw|Inverse|Invert|Inverti|Inverter
grain=Koerner|Grain|Grano|Grano|Grao
spread=Streuung|Dispers|Dispers|Dispers|Dispers
pitch=Tonhoeh|Hauteur|Altura|Altezza|Altura
density=Dichte|Densite|Densid|Densita|Densid
freeze=Frost|Gel|Congel|Blocca|Congel
wave=Welle|Onde|Onda|Onda|Onda
detune=Verstimm|Desacc|Desafin|Detune|Desafin
odd/even=Ung/Ger|Imp/Pair|Imp/Par|Disp/Pari|Imp/Par
motion=Bewegung|Mouvem|Movim|Moto|Movim
bright=Hell|Clair|Brillo|Luminos|Brilho
damp=Daempf|Amort|Amort|Smorz|Amort
excite=Anreg|Excit|Excit|Eccit|Excit
symmetry=Symm|Symetr|Simetr|Simmet|Simetr
harmonics=Obert|Harmon|Armon|Armon|Harmon
quick=SCHNELL|RAPIDE|RAPIDO|RAPIDO|RAPIDO
find=SUCHEN|CHERCHER|BUSCAR|CERCA|BUSCAR
themes=THEMEN|THEMES|TEMAS|TEMI|TEMAS
language=SPRACHE|LANGUE|IDIOMA|LINGUA|IDIOMA
design=DESIGN|DESIGN|DISENO|DESIGN|DESIGN
expert=EXPERTE|EXPERT|EXPERTO|ESPERTO|AVANCADO
play=PLAY|LECTURE|REPRODUC|PLAY|REPROD
edit clip=CLIP EDIT|EDIT CLIP|EDIT CLIP|MODIFICA CLIP|EDIT CLIP
random + feel=ZUFALL+FEEL|HASARD+FEEL|AZAR+FEEL|CASUALE+FEEL|ALEAT+FEEL
go to page=SEITE|ALLER A|IR A|VAI A|IR PARA
record on/off=Aufnahme|Enregistrer|Grabar|Registra|Gravar
undo=Rueckgaengig|Annuler|Deshacer|Annulla|Desfazer
undo history=Verlauf|Historique|Historial|Cronologia|Historico
favorite sound=Favorit|Favori|Favorito|Preferito|Favorito
fill (1 bar)=Fill (1 Takt)|Fill (1 mes.)|Fill (1 comp.)|Fill (1 batt.)|Fill (1 comp.)
follow action=Folgeaktion|Action suiv.|Accion sig.|Azione succ.|Acao seg.
follow after=Folgen nach|Suivre apres|Seguir tras|Segui dopo|Seguir apos
count-in on/off=Vorzaehler|Decompte|Cuenta atras|Conteggio|Contagem
normalize sample=Sample normal.|Normaliser|Normalizar|Normalizza|Normalizar
redo=Wiederholen|Refaire|Rehacer|Ripeti|Refazer
random clip=Zufalls-Clip|Clip hasard|Clip azar|Clip casuale|Clip aleat.
random wild=Zufall wild|Hasard fou|Azar loco|Casuale wild|Aleat. wild
random sound=Zufalls-Klang|Son hasard|Sonido azar|Suono casuale|Som aleat.
random sound wild=Klang wild|Son fou|Sonido loco|Suono wild|Som wild
undo sound=Klang zurueck|Annuler son|Deshacer son|Annulla suono|Desfazer som
vary pattern=Pattern variier|Varier patt.|Variar patron|Varia pattern|Variar padrao
clear clip=Clip leeren|Vider clip|Borrar clip|Svuota clip|Limpar clip
reverse clip=Clip umkehren|Inverser clip|Invertir clip|Inverti clip|Inverter clip
rotate left=Links drehen|Rotation G|Rotar izq|Ruota sx|Rodar esq
rotate right=Rechts drehen|Rotation D|Rotar der|Ruota dx|Rodar dir
all loops 16=Alle Loops 16|Loops a 16|Loops a 16|Tutti loop 16|Loops em 16
double length=Laenge x2|Longueur x2|Largo x2|Lunghezza x2|Compr. x2
half length=Laenge /2|Longueur /2|Largo /2|Lunghezza /2|Compr. /2
randomize vel=Vel zufaellig|Vel hasard|Vel azar|Vel casuale|Vel aleat.
prob random=Prob Zufall|Prob hasard|Prob azar|Prob casuale|Prob aleat.
prob curve=Prob Kurve|Prob courbe|Prob curva|Prob curva|Prob curva
pump on kick=Pump auf Kick|Pump sur kick|Pump en kick|Pump su kick|Pump no kick
idle screen on/off=Ruhebild|Ecran veille|Salvapant.|Screensaver|Prot. tela
slow knobs on/off=Langs. Regler|Reglage lent|Reg. lentos|Knob lenti|Knob lentos
design theme=Design|Theme|Tema|Tema|Tema
big values on/off=Grosse Werte|Grandes val.|Valores grand.|Valori grandi|Valores grand.
page slide on/off=Seiten-Slide|Glisse page|Desliz. pag.|Slide pagina|Slide pagina
knob lights on/off=Regler-Licht|Leds boutons|Luz knobs|Luci knob|Luz knobs
button lights=Tasten-Licht|Leds touches|Luz botones|Luci tasti|Luz botoes
invert display=Invertiert|Inverser|Invertir|Inverti|Inverter
metronome=Metronom|Metronome|Metronomo|Metronomo|Metronomo
humanize=Humanize|Humaniser|Humanizar|Umanizza|Humanizar
launch quant=Start-Quant|Quant lanc.|Cuant. lanz.|Quant. lancio|Quant. lanc.
pad mode (pitch/repeat/steps)=Pad Modus|Mode pad|Modo pad|Modo pad|Modo pad
device chain=Geraete-Kette|Chaine|Cadena|Catena|Cadeia
stop all clips=Alle Clips stop|Stop tous clips|Parar clips|Stop tutti clip|Parar clips
sound / kit browser=Klang-Browser|Nav. sons|Nav. sonidos|Browser suoni|Nav. sons
reset pad sound=Pad-Klang reset|Reset son pad|Reset sonido|Reset suono pad|Reset som pad
track: synth / drum=Spurtyp|Type piste|Tipo pista|Tipo traccia|Tipo faixa
view track=Spur|Piste|Pista|Traccia|Faixa
view sound=Klang|Son|Sonido|Suono|Som
view reverb=Hall|Reverb|Reverb|Reverbero|Reverb
view session=Session|Session|Sesion|Sessione|Sessao
view options=Optionen|Options|Opciones|Opzioni|Opcoes
view favorites=Favoriten|Favoris|Favoritos|Preferiti|Favoritos
pin last knob to fav=Regler zu FAV|Knob vers FAV|Knob a FAV|Knob in FAV|Knob p/ FAV
reset favorites=Favoriten reset|Reset favoris|Reset favoritos|Reset preferiti|Reset favoritos
export clip (midi)=MIDI Export Clip|Export clip MIDI|Export clip MIDI|Esporta clip MIDI|Export clip MIDI
export all clips (midi)=MIDI Export alle|Export tout MIDI|Export todo MIDI|Esporta tutto MIDI|Export tudo MIDI
load info=Lade-Info|Info charge|Info carga|Info carico|Info carga
mono check=Mono-Test|Test mono|Test mono|Test mono|Teste mono
name scene=Szene benennen|Nommer scene|Nombrar escena|Nomina scena|Nomear cena
scene launch=Szenen-Start|Lancer scene|Lanzar escena|Lancia scena|Lancar cena
follow preset=Folge-Preset|Preset suiv.|Preset sig.|Preset succ.|Preset seg.
save now=Jetzt sichern|Sauver|Guardar ya|Salva ora|Salvar agora
exit taktile=Taktile beenden|Quitter Taktile|Salir Taktile|Esci Taktile|Sair Taktile
theme classic=KLASSISCH|CLASSIQUE|CLASICO|CLASSICO|CLASSICO
theme minimal=MINIMAL|MINIMAL|MINIMO|MINIMO|MINIMO
theme symbols=SYMBOLE|SYMBOLES|SIMBOLOS|SIMBOLI|SIMBOLOS
theme nature=NATUR|NATURE|NATURA|NATURA|NATUREZA
theme kids=KINDER|ENFANTS|NINOS|BAMBINI|CRIANCAS
theme space=WELTALL|ESPACE|ESPACIO|SPAZIO|ESPACO
theme knobs=KNOEPFE|BOUTONS|KNOBS|MANOPOLE|BOTOES
engine=Engine|Moteur|Motor|Motore|Motor
fx type=FX TYP|FX TYPE|FX TIPO|FX TIPO|FX TIPO
fx time=FX ZEIT|FX TEMPS|FX TIEMPO|FX TEMPO|FX TEMPO
level=Pegel|Niveau|Nivel|Livello|Nivel
layers=Lagen|Couches|Capas|Strati|Camadas
chord=Akkord|Accord|Acorde|Accordo|Acorde
speed=Tempo|Vitesse|Veloc|Veloc|Veloc
space=Raum|Espace|Espacio|Spazio|Espaco
range=Bereich|Plage|Rango|Gamma|Faixa
target=Ziel|Cible|Objetivo|Target|Alvo
octave=Oktave|Octave|Octava|Ottava|Oitava
semis=Halbt|Demi-t|Semit|Semit|Semit
hold=Halten|Tenir|Mant|Tieni|Segurar
wide=Breit|Large|Ancho|Ampio|Largo
shape=Form|Forme|Forma|Forma|Forma
soft=Weich|Doux|Suave|Morbido|Suave
on=AN|ON|SI|ON|LIG
off=AUS|OFF|NO|OFF|DESL
bright=Hell|Clair|Brillo|Luminos|Brilho
inverted=Invertiert|Inverse|Invertido|Invertito|Invertido
turn >=Drehen >|Tourner >|Girar >|Gira >|Girar >
live=Live|Direct|Directo|Live|Ao vivo
keep=Behalt|Garder|Mant.|Tieni|Manter
up=Hoch|Haut|Arriba|Su|Cima
down=Runter|Bas|Abajo|Giu|Baixo
accent=Akzent|Accent|Acento|Accento|Acento
bias=Versatz|Decalage|Sesgo|Bias|Vies
return=Return|Retour|Retorno|Ritorno|Retorno
cutoff=Cutoff|Coupure|Corte|Taglio|Corte
compressor=Kompressor|Compresseur|Compresor|Compressore|Compressor
oscillator=Oszillator|Oscillateur|Oscilador|Oscillatore|Oscilador
waveform=Wellenform|Onde|Onda|Onda|Onda
frequency=Frequenz|Frequence|Frecuencia|Frequenza|Frequencia
curve=Kurve|Courbe|Curva|Curva|Curva
insert=Insert|Insert|Insert|Insert|Insert
audio=Audio|Audio|Audio|Audio|Audio
pad=Pad|Pad|Pad|Pad|Pad
layout=Layout|Disposition|Disposic|Layout|Layout
drum=Drum|Batterie|Bateria|Batteria|Bateria
slices=Slices|Tranches|Cortes|Tagli|Cortes
swing=Swing|Swing|Swing|Swing|Swing
release=Release|Release|Release|Release|Release
min=Min|Min|Min|Min|Min
max=Max|Max|Max|Max|Max
tempo=Tempo|Tempo|Tempo|Tempo|Tempo`.split('\n').forEach(l => { const i = l.indexOf('='); if (i > 0) TRD[l.substring(0, i)] = l.substring(i + 1).split('|'); });
function trk(s) { if (!lang) return null; const e = TRD[String(s).toLowerCase()]; return e && e[lang - 1] ? e[lang - 1] : null; }
function tr(s) {
    if (!lang) return s; s = String(s); if (!s) return s;
    const up = x => x === x.toUpperCase() && /[A-Z]/.test(x);
    const e = TRD[s.toLowerCase()];
    if (e) { const v = e[lang - 1]; return v ? (up(s) ? v.toUpperCase() : v) : s; }
    if (s.indexOf(' ') > 0 && s.length < 40) {                 /* several words: translate the words we know when at least half of them are known */
        const w = s.split(' '); if (w.length > 4) return s; let n = 0;
        const o = w.map(x => { const q = TRD[x.toLowerCase()]; if (q && q[lang - 1]) { n++; return up(x) ? q[lang - 1].toUpperCase() : q[lang - 1]; } return x; });
        if (n * 2 >= w.length) return o.join(' ');
    }
    return s;
}
function themeNm(i) { const t = trk('Theme ' + THEME_NAME[i]); return t !== null ? t : THEME_NAME[i]; }
const MENU_ITEMS = [
    ['Play / Stop', () => { playing = !playing; gs = 0; dspSet('play', playing ? '1' : '0'); }],
    ['Stop + Reset', () => { playing = false; gs = 0; dspSet('play', '0'); }],
    ['Record on/off', () => { if (recArmed) recStopLen(); recArmed = !recArmed; growTrk.fill(false); showToast('REC', recArmed ? 'ARMED' : 'OFF', recArmed ? 1 : 0); }],
    ['Undo', () => doUndo()],
    ['Undo history', () => { menuOpen = false; navStack.push(16); histSel = 0; view = 40; }],
    ['Favorite sound', () => { menuOpen = false; const t = Math.round(pads[ln()][0]), i = favs.indexOf(t); if (i >= 0) favs.splice(i, 1); else favs.push(t); markEdited(); showToast(TYPE_NAME[t], i >= 0 ? 'FAV OFF' : 'FAVORITE *', i >= 0 ? 0 : 1); }],
    ['Fill (1 bar)', () => { menuOpen = false; fillBar(); }],
    ['Follow action', () => folCycle(0)],
    ['Follow after', () => folCycle(1)],
    ['Count-in on/off', () => { ciOn = !ciOn; markEdited(); needsRedraw = true; showToast('COUNT-IN', ciOn ? 'ON (1 BAR)' : 'OFF', ciOn ? 1 : 0); }],
    ['Normalize sample', () => { menuOpen = false; sampleNormalize(); }],
    ['Redo', () => doRedo()],
    ['Random clip', () => randomize(false)],
    ['Random wild', () => randomize(true)],
    ['Random sound', () => randomSound(false)],
    ['Random sound wild', () => randomSound(true)],
    ['Undo sound', () => undoSound()],
    ['Vary pattern', () => opVary()],
    ['Clear clip', () => opClear()],
    ['Reverse clip', () => opReverse()],
    ['Rotate left', () => opRotate(-1)],
    ['Rotate right', () => opRotate(1)],
    ['All loops 16', () => allLoops16()],
    ['Double length', () => opDouble()],
    ['Half length', () => opHalf()],
    ['Randomize vel', () => opHuman()],
    ['Prob random', () => opProb(0)],
    ['Prob curve', () => opProb(1)],
    ['Prob 100%', () => opProb(2)],
    ['Pump on kick', () => opPump()],
    ['Start animation on/off', () => { splashOn = !splashOn; markEdited(); showToast('START ANIMATION', splashOn ? 'ON' : 'OFF', splashOn ? 1 : 0); }],
    ['Idle screen on/off', () => { idleOn = !idleOn; noteInput(); markEdited(); showToast('IDLE SCREEN', idleOn ? 'ON 5 MIN' : 'OFF', idleOn ? 1 : 0); }],
    ['Note repeat on/off', () => { repAllowed = !repAllowed; if (!repAllowed) { for (let t = 0; t < NTRK; t++) if (topModes[t] === 1) topModes[t] = 0; padsOffSent = -1; } markEdited(); needsRedraw = true; showToast('NOTE REPEAT', repAllowed ? 'AVAILABLE' : 'OFF', repAllowed ? 1 : 0); }],
    ['Slow knobs on/off', () => { slowKnobs = !slowKnobs; slowAcc = {}; markEdited(); showToast('SLOW KNOBS', slowKnobs ? 'ON (HALF SPEED)' : 'OFF', slowKnobs ? 1 : 0); }],
    ['Design theme', () => { themeOpenPicker(); }],
    ['Big values on/off', () => { bigOn = !bigOn; markEdited(); needsRedraw = true; showToast('BIG VALUES', bigOn ? 'ON' : 'OFF', bigOn ? 1 : 0); }],
    ['Page slide on/off', () => { slideOn = !slideOn; markEdited(); needsRedraw = true; showToast('PAGE SLIDE', slideOn ? 'ON' : 'OFF', slideOn ? 1 : 0); }],
    ['Knob lights on/off', () => { if (knobLedsOn && knobLvlOn) knobLvlOn = false; else if (knobLedsOn) knobLedsOn = false; else { knobLedsOn = true; knobLvlOn = true; } markEdited(); needsRedraw = true; showToast('KNOB LIGHTS', knobLedsOn ? (knobLvlOn ? 'BRIGHTNESS = VALUE' : 'ON (FLAT)') : 'OFF', knobLedsOn ? (knobLvlOn ? 1 : 0.5) : 0); }],
    ['Button lights', () => { btnBright = !btnBright; markEdited(); needsRedraw = true; showToast('BUTTON LIGHTS', btnBright ? 'BRIGHT' : 'NORMAL', btnBright ? 1 : 0); }],
    ['Invert display', () => { dispInv = !dispInv; markEdited(); needsRedraw = true; showToast('DISPLAY', dispInv ? 'INVERTED' : 'NORMAL', dispInv ? 1 : 0); }],
    ['MIDI monitor', () => { menuOpen = false; monOpen = true; monLog = []; needsRedraw = true; }],
    ['Scope', () => { menuOpen = false; navStack.push(18); view = 42; needsRedraw = true; }],
    ['Metronome', () => { const v = cycleMaster('m_met', [0, 0.6]); showToast('METRONOME', v > 0 ? 'ON' : 'OFF', v); }],
    ['Humanize', () => { const v = cycleMaster('m_hum', [0, 0.3, 0.6, 1]); showToast('HUMANIZE', String(Math.round(v * 100)), v); }],
    ['Launch quant', () => { const v = cycleMaster('m_qnt', [0.07, 0.21, 0.36, 0.5, 0.64, 0.79, 0.93]); showToast('LAUNCH', fmtVal('m_qnt', v), v); }],
    ['Tap tempo', () => tapTempo()],
    ['Pad mode (pitch/repeat/steps)', () => cycleTop(1)],
    ['Live FX pads on/off', () => { menuOpen = false; toggleTop(3); }],
    ['Chord pads on/off', () => { menuOpen = false; toggleTop(4); }],
    ['Device chain', () => { menuOpen = false; openDevices(); }],
    ['Stop all clips', () => stopAll()],
    ['Sound / kit browser', () => { menuOpen = false; fbOpen(); }],
    ['Reset pad sound', () => { const L = ln(); pSnap('pad', L, 'rst'); soundBak = { L: L, v: pads[L].slice() }; pads[L] = defaultPads()[L]; qPad(L); markEdited(); showToast('RESET', 'T' + p2(sel + 1), 1); }],
    ['Track: Synth / Drum', () => setKind(sel, kind[sel] ? 0 : 1)],
    ['View TRACK', () => setView(21)],
    ['View SEQ', () => setView(0)],
    ['View SOUND', () => setView(1)],
    ['View FILTER FX', () => setView(2)],
    ['View MOD/ARP', () => setView(3)],
    ['View MASTER', () => setView(4)],
    ['View DELAY', () => setView(5)],
    ['View REVERB', () => setView(6)],
    ['View SESSION', () => setView(7)],
    ['View OPTIONS', () => setView(8)],
    ['View FAVORITES', () => openFav()],
    ['Pin last knob to FAV', () => favPin()],
    ['Reset favorites', () => favReset()],
    ['Export clip (MIDI)', () => midiExport(false)],
    ['Export all clips (MIDI)', () => midiExport(true)],
    ['Quantize now', () => opTimeQ()],
    ['Load info', () => { menuOpen = false; navStack.push(18); view = 51; needsRedraw = true; }],
    ['CPU meter', () => { menuOpen = false; navStack.push(18); view = 51; needsRedraw = true; }],
    ['Sound A/B', () => opAB()],
    ['Mono check', () => monoCheck()],
    ['Name scene', () => { menuOpen = false; sceneNameStart(ed[sel]); }],
    ['Scene launch', () => sceneLaunchCycle()],
    ['Follow preset', () => folPreset()],
    ['Save now', () => { saveState(); showToast('SAVED', '', 1); }],
    ['Exit Taktile', () => exitModule()],
];
function knobTouch(i) {
    if (menuOpen) return;
    if (view === 0 && heldStep < 0) {
        if (shiftHeld && i < 4) { const L = [['BPM', String(bpm), (bpm - 40) / 200], ['SWING', String(50 + swing), swing / 25], ['LEN', String(LL()), LL() / MAXSTEP], ['CLIP', String(ed[sel] + 1), ed[sel] / 7]]; showToast(L[i][0], L[i][1], L[i][2]); return; }
        const k = seqSnd()[i], v = pads[ln()][PI[k]]; showToast((LONG_NAME[k] || k).toUpperCase(), fmtVal(k, v), fracVal(k, v)); return;
    }
    if (view === 7) { const t = bank * 4 + (i & 3), k = i >= 4 ? 'pan' : 'vol', tvv = k === 'pan' ? pads[t][PI.pan] : getTV(t); showToast(k === 'pan' ? 'PAN' : 'VOLUME', kind[t] && k === 'pan' ? 'PER SOUND' : fmtVal(k, tvv), tvv); return; }
    const items = VIEW_ITEMS[itemsView()]; if (!items || i >= items.length) return;
    const scope = scopeOf(items[i][0]), v = getVal(scope, items[i][0]);
    if (!bigOn && !CURVE_KEYS[items[i][0]]) { needsRedraw = true; return; }
    curveKey = items[i][0]; showToast(fullLabel(view, i, items).toUpperCase(), fmtVal(items[i][0], v), fracVal(items[i][0], v)); if (toast) toast.nm = 1;
}
const LONG_NAME = { dly_time: 'Delay Time', dly_fb: 'Delay Feedback', dly_mode: 'Delay Type', dly_tone: 'Delay Tone', dly_wide: 'Delay Width', dly_ret: 'Delay Return', rvb_size: 'Reverb Size', rvb_dark: 'Reverb Damping', rvb_mode: 'Reverb Type', rvb_wide: 'Reverb Width', rvb_low: 'Reverb Low Cut', dmode: 'Delay Type', rmode: 'Reverb Type', dtime: 'Delay Time', dfb: 'Delay Feedback', rsize: 'Reverb Size', rdark: 'Reverb Damping', chrt: 'Chorus Rate', chdp: 'Chorus Depth', phrt: 'Phaser Rate', phfb: 'Phaser Feedback',
    dly: 'Delay Send', rvb: 'Reverb Send', cut: 'Lowpass Cutoff', locut: 'Highpass Cutoff', res: 'Resonance', fenv: 'Filter Envelope', crush: 'Bit Crusher', ring: 'Ring Modulator', pump: 'Pump', wob: 'Wobble',
    arp: 'Arpeggiator', rate: 'Arp Rate', duck: 'Sidechain Duck', duckr: 'Duck Release', scs: 'Duck Source', m_vol: 'Master Volume', m_filt: 'Master Filter', m_sat: 'Saturation', m_wid: 'Stereo Width',
    m_comp: 'Compressor', dly_ret: 'Delay Return', rvb_ret: 'Reverb Return', eql: 'Sound EQ Low', eqm: 'Sound EQ Mid', eqh: 'Sound EQ High', m_eql: 'EQ Low', m_eqm: 'EQ Mid', m_eqh: 'EQ High', m_lim: 'Limiter', poly: 'Voices', wave: 'Waveform', det: 'Detune',
    sub: 'Sub Oscillator', att: 'Attack', vol: 'Volume', tvol: 'Track Volume', pmode: 'Pad Mode', dsel: 'Drum Sound', lay: 'Pad Layout', tcol: 'Track Color', tsw: 'Track Swing', rdens: 'Random Density',
    rvar: 'Random Velocity', rkey: 'Random In Key', rrat: 'Random Ratchet', rqoff: 'Record Snap', fullvel: 'Full Velocity', m_hum: 'Humanize', m_met: 'Metronome', m_qnt: 'Launch Quantize',
    qscale: 'Quantize To Scale', sst: 'Sample Start', sen: 'Sample End', smd: 'Sample Mode', smp: 'Sample File', sgn: 'Sample Gain', satk: 'Sample Attack', srel: 'Sample Release', sstr: 'Time Stretch',
    mch: 'Note Chance', mvb: 'Velocity Bias', trn: 'Transpose', orn: 'Random Octave', vhm: 'Velocity Humanize', vmn: 'Velocity Min', vmx: 'Velocity Max', eqlf: 'EQ Low Freq', eqmf: 'EQ Mid Freq', eqhf: 'EQ High Freq', eqq: 'EQ Mid Width', eqo: 'EQ Output', pcv: 'Pump Curve', rfq: 'Ring Frequency',
    dtn: 'Delay Tone', dwd: 'Delay Width', rwd: 'Reverb Width', rlc: 'Reverb Low Cut', gdn: 'Grain Density', grv: 'Grain Reverse', gfz: 'Grain Freeze', gtn: 'Grain Window',
    enot: 'Euclid Note', ealt: 'Euclid Alt Note', eacn: 'Euclid Accent Note', ewlk: 'Euclid Walk', ewln: 'Euclid Walk Length', ernd: 'Euclid Random Note', eoup: 'Euclid Octave Up', eodn: 'Euclid Octave Down', eon: 'Euclid Mode', qgrid: 'Quantize Grid', qstr: 'Quantize Strength', qtime: 'Quantize Now',
    rmb: 'Kick Rumble', rmt: 'Rumble Tail', ehit: 'Euclid Hits', erot: 'Euclid Rotate', elen: 'Euclid Length', emut: 'Euclid Mutate', mamt: 'MIDI FX Amount', mprm: 'MIDI FX Param', mfx: 'MIDI Effect', xfx: 'Insert Effect', xmix: 'Insert Amount', m2fx: 'MIDI Effect 2', m2amt: 'MIDI 2 Amount', m2prm: 'MIDI 2 Param', x2fx: 'Audio Effect 2', x2mix: 'Audio 2 Amount', x2rt: 'Audio 2 Rate', x2dp: 'Audio 2 Depth',
    xrt: 'Insert Rate', xdp: 'Insert Depth', fdec: 'Filter Env Decay', fkey: 'Filter Key Track', fmode: 'Filter Mode', slg: 'Slices To Pads' };
function fullLabel(v, i, items) {                          /* the whole parameter name (the grid uses short names) */
    const k = items[i][0];
    if (v === 28 && smpPage) return items[i][1];
    if ((v === 27 || v === 24) && i > 0) return itemLabel(v, i);
    if (v === 14 && k === 'chor') return 'Chorus Mix';
    if (v === 19 && k === 'phas') return 'Phaser Mix';
    if (v === 38 && k === 'chor') return 'Chorus Send';
    if (v === 38 && k === 'phas') return 'Phaser Send';
    if (v === 6 && k === 'rvb') return 'Reverb Send';
    return LONG_NAME[k] || items[i][1] || items[i][2];
}
THEME_NAME.forEach((n, i) => MENU_ITEMS.push(['Theme ' + n, () => { uiTheme = i; uiMin = i !== 0; themeBefore = i; markEdited(); needsRedraw = true; showToast('DESIGN THEME', n, (i + 1) / THEME_NAME.length); }]));
let themeBefore = 0;                                       /* the theme that was active when the picker opened (Back restores it) */
function themeApply(i) { uiTheme = i; uiMin = i !== 0; }
LANGS.forEach((n, i) => MENU_ITEMS.push(['Language ' + n, () => { lang = i; langBefore = i; markEdited(); needsRedraw = true; showToast('LANGUAGE', n, (i + 1) / LANGS.length); }]));
MENU_ITEMS.push(['Language', () => { langOpenPicker(); }]);
function inLang() { return menuOpen && menuGrp >= 0 && MENU_GROUPS[menuGrp][0] === 'LANGUAGE'; }
function langOpenPicker() { langBefore = lang; menuOpen = true; menuGrp = MENU_GROUPS.findIndex(g => g[0] === 'LANGUAGE'); menuIdx = menuGrp; menuSub = lang; needsRedraw = true; }
function inThemes() { return menuOpen && menuGrp >= 0 && MENU_GROUPS[menuGrp][0] === 'THEMES'; }
function themeRestore() { if (inThemes()) themeApply(themeBefore); else if (inLang()) { lang = langBefore; needsRedraw = true; } }
function themeOpenPicker() { themeBefore = uiTheme; menuOpen = true; menuGrp = MENU_GROUPS.findIndex(g => g[0] === 'THEMES'); menuIdx = menuGrp; menuSub = uiTheme; needsRedraw = true; }
const MENU_SHORT = { 'Design theme': 'Theme', 'Invert display': 'Invert', 'Start animation on/off': 'Intro', 'Export clip (MIDI)': 'MIDI export clip', 'Export all clips (MIDI)': 'MIDI export all', 'Pad mode (pitch/repeat/steps)': 'Pad mode', 'Live FX pads on/off': 'Live FX', 'Track: Synth / Drum': 'Track type', 'Sound / kit browser': 'Sound browser' };
function menuVal(n) {                                      /* the current state of a menu item, shown on its right */
    const o = v => v ? 'ON' : 'OFF';
    switch (n) {
        case 'Design theme': return themeNm(uiTheme);
        case 'Language': return LANGS[lang];
        case 'Big values on/off': return o(bigOn);
        case 'Page slide on/off': return o(slideOn);
        case 'Invert display': return o(dispInv);
        case 'Button lights': return btnBright ? 'BRIGHT' : 'NORMAL';
        case 'Start animation on/off': return o(splashOn);
        case 'Idle screen on/off': return o(idleOn);
        case 'Knob lights on/off': return knobLedsOn ? (knobLvlOn ? 'LEVEL' : 'ON') : 'OFF';
        case 'Count-in on/off': return o(ciOn);
        case 'Note repeat on/off': return o(repAllowed);
        case 'Slow knobs on/off': return o(slowKnobs);
        case 'Record on/off': return o(recArmed);
        case 'Metronome': return o(master[MI.m_met] > 0.01);
        case 'Mono check': return o(monoChk);
        case 'Scene launch': return SQ_NAME[sceneQ[ed[sel]]];
        case 'Name scene': return sceneNames[ed[sel]] ? fit(sceneNames[ed[sel]], 54) : '';
        case 'Follow preset': { const i = folPreCur(); return i >= 0 ? FOL_PRE[i][0] : 'CUSTOM'; }
        case 'Follow action': return FOL_NAME[fol[sel][ed[sel]] & 7];
        case 'Follow after': return FOL_LOOPS[Math.min(5, (fol[sel][ed[sel]] >> 3) & 7)] + 'x';
        case 'Pad mode (pitch/repeat/steps)': return TOP_NAMES[topModes[sel]];
        case 'Live FX pads on/off': return topModes[sel] === 3 ? 'ON' : 'OFF';
        case 'Chord pads on/off': return topModes[sel] === 4 ? 'ON' : 'OFF';
    }
    return '';
}
const MENU_GROUPS = [
    ['THEMES', THEME_NAME.map(n => 'Theme ' + n)],
    ['LANGUAGE', LANGS.map(n => 'Language ' + n)],
    ['DESIGN', ['Design theme', 'Language', 'Big values on/off', 'Page slide on/off', 'Invert display', 'Button lights', 'Start animation on/off', 'Idle screen on/off', 'Knob lights on/off']],
    ['GLOBAL', ['Count-in on/off', 'Note repeat on/off', 'Slow knobs on/off', 'Save now', 'Exit Taktile']],
    ['EXPERT', ['CPU meter', 'Load info', 'MIDI monitor', 'Scope', 'Export clip (MIDI)', 'Export all clips (MIDI)']],
    ['PLAY', ['Play / Stop', 'Stop + Reset', 'Record on/off', 'Metronome', 'Mono check', 'Tap tempo', 'Stop all clips']],
    ['EDIT CLIP', ['Quantize now', 'Undo', 'Redo', 'Undo history', 'Clear clip', 'Reverse clip', 'Rotate left', 'Rotate right', 'All loops 16', 'Double length', 'Half length', 'Fill (1 bar)', 'Follow action', 'Follow after', 'Follow preset', 'Name scene', 'Scene launch']],
    ['RANDOM + FEEL', ['Random clip', 'Random wild', 'Random sound', 'Random sound wild', 'Undo sound', 'Vary pattern', 'Randomize vel', 'Prob random', 'Prob curve', 'Prob 100%', 'Humanize', 'Launch quant']],
    ['SOUND', ['Sound / kit browser', 'Favorite sound', 'Reset pad sound', 'Sound A/B', 'Pump on kick', 'Normalize sample', 'Track: Synth / Drum', 'Pad mode (pitch/repeat/steps)', 'Live FX pads on/off', 'Device chain']],
    ['GO TO PAGE', ['View TRACK', 'View SEQ', 'View SOUND', 'View FILTER FX', 'View MOD/ARP', 'View MASTER', 'View DELAY', 'View REVERB', 'View SESSION', 'View OPTIONS', 'View FAVORITES', 'Pin last knob to FAV', 'Reset favorites']],
].map(g => [g[0], g[1].map(n => MENU_ITEMS.find(m => m[0] === n)).filter(Boolean)]);
MENU_GROUPS.unshift(['QUICK', []]); MENU_GROUPS.splice(1, 0, ['FIND', []]);
const MENU_FILE = '/data/UserData/schwung/taktile-menu.json';
let menuPins = [], menuRecent = [], menuPinLoaded = false, findCh = '';
function gName() { return menuGrp >= 0 ? MENU_GROUPS[menuGrp][0] : ''; }
function quickRebuild() {                                  /* QUICK = pinned entries (Shift + click on any entry) first, then the last used ones */
    if (!menuPinLoaded) { menuPinLoaded = true; try { const t = host_read_file(MENU_FILE); const o = t ? JSON.parse(t) : null; if (o && Array.isArray(o.pins)) menuPins = o.pins.filter(x => typeof x === 'string'); } catch (e) { menuPins = []; } }
    const q = MENU_GROUPS[0][1]; q.length = 0; const seen = {};
    const add = n => { const it = MENU_ITEMS.find(m => m[0] === n); if (it && !seen[n] && q.length < 10) { seen[n] = 1; q.push(it); } };
    menuPins.forEach(add); menuRecent.forEach(add);
    if (!q.length) ['Undo', 'Play / Stop', 'Vary pattern', 'Save now'].forEach(add);
}
function menuPinToggle(n) {
    const i = menuPins.indexOf(n); if (i >= 0) menuPins.splice(i, 1); else menuPins.push(n);
    try { host_write_file(MENU_FILE, JSON.stringify({ pins: menuPins })); } catch (e) { /* ignore */ }
    quickRebuild(); showToast(i >= 0 ? 'UNPINNED' : 'PINNED', fit(n, 100), i >= 0 ? 0 : 1);
    if (menuGrp >= 0) menuSub = clamp(menuSub, 0, Math.max(0, menuLen() - 1));
}
function findRebuild() {                                   /* FIND: all entries A-Z; Shift + turn = only entries with a word starting with this letter */
    const f = MENU_GROUPS[1][1]; f.length = 0;
    const lab = it => (MENU_SHORT[it[0]] || it[0]).toUpperCase();
    MENU_ITEMS.filter(it => !/^Theme /.test(it[0]) && (!findCh || lab(it).split(/[^A-Z0-9]+/).some(w => w[0] === findCh))).sort((a, b) => lab(a) < lab(b) ? -1 : 1).forEach(it => f.push(it));
}
function findStep(d) { const A = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'; let i = findCh ? A.indexOf(findCh) : -1; i = clamp(i + (d > 0 ? 1 : -1), -1, 25); findCh = i < 0 ? '' : A[i]; findRebuild(); menuSub = 0; needsRedraw = true; }
function menuCtxGroup(v) {                                 /* the menu opens at the group that fits the page you came from */
    const SOUNDV = [1, 2, 3, 9, 27, 28, 30, 38, 39, 46, 50];
    const nm = v === 0 ? 'EDIT CLIP' : (SOUNDV.indexOf(v) >= 0 ? 'SOUND' : 'QUICK');
    return Math.max(0, MENU_GROUPS.findIndex(g => g[0] === nm));
}
quickRebuild(); findRebuild();
function menuLen() { return menuGrp < 0 ? MENU_GROUPS.length : MENU_GROUPS[menuGrp][1].length; }
function menuMove(d) { if (menuGrp < 0) menuIdx = clamp(menuIdx + (d > 0 ? 1 : -1), 0, MENU_GROUPS.length - 1); else { menuSub = clamp(menuSub + (d > 0 ? 1 : -1), 0, menuLen() - 1); if (inThemes()) themeApply(menuSub); else if (inLang()) { lang = menuSub; needsRedraw = true; } } needsRedraw = true; }      /* in the THEMES group the theme changes live while you scroll */
const MENU_STAY = {};                                          /* switches and cyclers keep the menu open: you stay where you are and see the value change */
['Record on/off', 'Undo', 'Redo', 'Follow action', 'Follow after', 'Count-in on/off', 'Start animation on/off', 'Idle screen on/off', 'Note repeat on/off', 'Slow knobs on/off', 'Big values on/off', 'Page slide on/off', 'Knob lights on/off', 'Button lights', 'Invert display', 'Metronome', 'Humanize', 'Launch quant', 'Pad mode (pitch/repeat/steps)', 'Track: Synth / Drum', 'Random clip', 'Random wild', 'Random sound', 'Random sound wild', 'Undo sound', 'Vary pattern', 'Reverse clip', 'Rotate left', 'Rotate right', 'Double length', 'Half length', 'Randomize vel', 'Prob random', 'Prob curve', 'Prob 100%', 'Pump on kick', 'Reset pad sound', 'Tap tempo', 'Stop all clips'].forEach(k => { MENU_STAY[k] = 1; });
function menuSelect() { if (menuGrp < 0) { menuGrp = menuIdx; menuSub = 0; if (gName() === 'FIND') { findCh = ''; findRebuild(); showToast('FIND', 'SHIFT+TURN = LETTER', 0.5); } if (gName() === 'QUICK') quickRebuild(); if (MENU_GROUPS[menuGrp][0] === 'THEMES') { themeBefore = uiTheme; menuSub = uiTheme; } if (MENU_GROUPS[menuGrp][0] === 'LANGUAGE') { langBefore = lang; menuSub = lang; } needsRedraw = true; return; } const it = MENU_GROUPS[menuGrp][1][menuSub]; if (!it) return; if (shiftHeld && gName() !== 'THEMES') { menuPinToggle(it[0]); needsRedraw = true; return; } if (gName() !== 'THEMES' && it[0] !== 'Exit Taktile') { const r = menuRecent.indexOf(it[0]); if (r >= 0) menuRecent.splice(r, 1); menuRecent.unshift(it[0]); if (menuRecent.length > 6) menuRecent.length = 6; quickRebuild(); } if (!MENU_STAY[it[0]]) menuOpen = false; it[1](); needsRedraw = true; }
function menuBack() { themeRestore(); if (menuGrp >= 0) { menuGrp = -1; needsRedraw = true; } else { menuOpen = false; needsRedraw = true; } }
let menuKA = 0, mnPos = 0, mnT0 = 0, mnDir = 1, mnGrpPrev = -2, mnLast = 0, menuAnim = false;
/* thin small font for the menu: 3x5 capitals and digits, one pixel strokes, drawn pixel by pixel */
const SF = {
    'A': '.#.#.#####.##.#',
    'B': '##.#.###.#.###.',
    'C': '.###..#..#...##',
    'D': '##.#.##.##.###.',
    'E': '####..##.#..###',
    'F': '####..##.#..#..',
    'G': '.###..#.##.#.##',
    'H': '#.##.#####.##.#',
    'I': '###.#..#..#.###',
    'J': '..#..#..##.#.#.',
    'K': '#.##.###.#.##.#',
    'L': '#..#..#..#..###',
    'M': '#.########.##.#',
    'N': '##.#.##.##.##.#',
    'O': '.#.#.##.##.#.#.',
    'P': '##.#.###.#..#..',
    'Q': '.#.#.##.####..#',
    'R': '##.#.###.#.##.#',
    'S': '.###...#...###.',
    'T': '###.#..#..#..#.',
    'U': '#.##.##.##.####',
    'V': '#.##.##.##.#.#.',
    'W': '#.##.########.#',
    'X': '#.##.#.#.#.##.#',
    'Y': '#.##.#.#..#..#.',
    'Z': '###..#.#.#..###',
    '0': '####.##.##.####',
    '1': '.#.##..#..#.###',
    '2': '##...#.#.#..###',
    '3': '##...#.#...###.',
    '4': '#.##.####..#..#',
    '5': '####..##...###.',
    '6': '.###..####.####',
    '7': '###..#.#..#..#.',
    '8': '####.#####.####',
    '9': '####.####..###.',
    '/': '..#..#.#.#..#..',
    '-': '......###......',
    '+': '....#.###.#....',
    '.': '.............#.',
    ':': '....#.....#....',
    '>': '#...#...#.#.#..',
    '<': '..#.#.#...#...#',
    '*': '...#.#.#.#.#...',
    '(': '.#.#..#..#...#.',
    ')': '.#...#..#..#.#.',
    '%': '#.#..#.#.#..#.#',
    '_': '............###',
    '\'': '.#..#..........',
    '!': '.#..#..#.....#.',
    '?': '##...#.#.....#.',
    ' ': '...............',
    ',': '..........#.#..',
    '=': '...###...###...',
    '#': '#.#####.#####.#',
};
function sfW(s) { return Math.max(0, String(s).length * 4 - 1); }
function txtS(x, y, s, c) {
    if (NH && c === 0) NH.note('s', x, y, s);
    s = tr(String(s)).toUpperCase();
    for (let i = 0; i < s.length; i++) {
        let g = SF[s[i]]; if (g === undefined) g = SF['?'];
        if (g.length < 15) g = (g + '...............').substring(0, 15);
        for (let r = 0; r < 5; r++) for (let q = 0; q < 3; q++) if (g[r * 3 + q] === '#') set_pixel(x + i * 4 + q, y + r, c === undefined ? 1 : c);
    }
}
function menuHeader(left, right, ox) {                       /* minimal: small thin title, count on the right, one hairline */
    hdrDrawn = true;
    const th = uiTheme, ic = (uiMin && th >= 2 && TH_GLYPH[th]) ? 11 : 0;
    if (ic) bmp(2 + ox, 1, TH_GLYPH[th], 1);
    txt(3 + ic + ox, 1, left, 1); txt(125 - tw(right), 1, right, 1);
    if (uiMin) thLine(10); else fill_rect(0, 10, 128, 1, 1);
}
function drawMenu() {
    const inG = menuGrp >= 0, cur = inG ? menuSub : menuIdx, n = menuLen(), now = Date.now(), V = 5, RH = 10;
    if (menuGrp !== mnGrpPrev) { mnGrpPrev = menuGrp; mnT0 = now; mnDir = inG ? 1 : -1; mnPos = cur; }
    const dt = Math.min(80, now - (mnLast || now)); mnLast = now;
    if (mnPos !== cur) { mnPos += (cur - mnPos) * (1 - Math.exp(-dt / 55)); if (Math.abs(cur - mnPos) < 0.04) mnPos = cur; }
    const p = clamp((now - mnT0) / 150, 0, 1), ox = Math.round(Math.pow(1 - p, 3) * 30 * mnDir);
    menuAnim = p < 1 || mnPos !== cur;
    const topF = clamp(mnPos - 2.5, 0, Math.max(0, n - V)), k0 = Math.floor(topF);
    for (let k = Math.max(0, k0 - 1); k <= Math.min(n - 1, k0 + V); k++) {
        const y = 13 + Math.round((k - topF) * RH); if (y > 56 || y < (menuAnim ? 8 : 12)) continue;
        const on = Math.abs(k - mnPos) < 0.5;
        const raw = inG ? MENU_GROUPS[menuGrp][1][k][0] : '', val = inG ? menuVal(raw) : '';
        const tk = inG ? trk(raw) : null;
        let lab = inG ? (tk !== null ? tk : (MENU_SHORT[raw] || raw.replace(/^View /, '').replace(/^Theme /, '').replace(/^Language /, '').replace(/ on\/off$/, ''))) : tr(MENU_GROUPS[k][0]);
        if (inThemes() && k === themeBefore) lab += ' *';
        else if (inLang() && k === langBefore) lab += ' *';
        else if (inG && menuPins.indexOf(raw) >= 0 && !inThemes()) lab += ' *';
        const room = Math.floor((val ? 106 - tw(val) : 102) / 6); if (lab.length > room) lab = on ? mq(lab, room) : lab.slice(0, room);
        if (on) { fill_rect(2 + ox, y - 1, 120, 10, 1); set_pixel(2 + ox, y - 1, 0); set_pixel(121 + ox, y - 1, 0); set_pixel(2 + ox, y + 8, 0); set_pixel(121 + ox, y + 8, 0); }      /* the chosen row is a solid bar: easy to see in every design */
        const tc = on ? 0 : 1;
        txt(6 + ox, y, lab, tc);
        if (val) txt(118 - tw(val) + ox, y, val, tc);
        else if (!inG) txt(112 + ox, y, '>', tc);
    }
    if (n > V) {                                                 /* slim scroll bar on the right edge */
        const th = Math.max(5, Math.round(44 * V / n)), ty = 13 + Math.round((44 - th) * (topF / Math.max(1, n - V)));
        for (let yy = 13; yy < 57; yy += 2) set_pixel(126 + ox, yy, 1); fill_rect(125 + ox, ty, 3, th, 1);
    }
    fill_rect(0, 0, 128, 11, 0);
    menuHeader((inG ? MENU_GROUPS[menuGrp][0] + (MENU_GROUPS[menuGrp][0] === 'FIND' ? ' ' + (findCh || 'ALL') : '') : 'OPTIONS').slice(0, 12), (n ? cur + 1 : 0) + '/' + n, ox);
}

/* euclid generator: hold Loop + knob 1 (hits) / knob 2 (rotation) */
function euclidKnob(i, delta) {
    const n = LL();
    if (i === 0) euK[sel] = clamp(euK[sel] + (delta > 0 ? 1 : -1), 0, n);
    else euR[sel] = clamp((((euR[sel] % n) + n) % n) + (delta > 0 ? 1 : -1), 0, n - 1);
    const L = ln();
    if (!snapEu) { snap(ed[sel], L); snapEu = true; }
    const k = euK[sel], r = euR[sel], c = ed[sel];
    for (let s = 0; s < MAXSTEP; s++) {
        const idx = ((s - r) % n + n) % n;
        const hit = s < n && k > 0 && ((idx * k) % n) < k;
        if (hit) steps[c][L][s][0] = 1; else steps[c][L][s] = [0, 100, 0, 0];
    }
    qSeq(c, L); showToast('EUCLID ' + k + '/' + n, 'ROT ' + r, k / n); markEdited();
}

/* step-held knobs */
function stepKnob(i, delta) {
    if (delta === 0 || i > 7) return;
    const st = stepAt(heldStep), k = (i === 0 && shiftHeld) ? 4 : 1;
    if (i === 7) {
        if (shiftHeld) { acc[7] += delta; if (Math.abs(acc[7]) < 4) return; const d = acc[7] > 0 ? 1 : -1; acc[7] = 0; const L = ln(); lockT[L] = (lockT[L] + d + LOCK_KEYS.length) % LOCK_KEYS.length; qLockT(L); showToast('LOCK TARGET', LOCK_NAME[lockT[L]], lockT[L] / 9); markEdited(); return; }
        if (!st[8]) { if (delta < 0) return; st[8] = 64; } else { acc[7] += delta; if (Math.abs(acc[7]) < 5) return; st[8] = clamp(st[8] + (acc[7] > 0 ? 1 : -1) * 2, 0, 127); acc[7] = 0; }
        showToast('LOCK ' + LOCK_NAME[lockT[ln()]], st[8] ? String(Math.round((st[8] - 1) / 126 * 100)) : 'OFF', st[8] / 127);
        heldEdited = true; grpSync(7); qStep(ed[sel], ln(), heldStep); markEdited(); return;
    }
    if (i === 0) { acc[0] += delta; if (Math.abs(acc[0]) < 4) return; const dv = acc[0] > 0 ? 1 : -1; acc[0] = 0; st[1] = clamp(st[1] + dv * k, 1, 127); showToast('VELOCITY', String(st[1]), st[1] / 127); }
    else if (i === 1) { acc[1] += delta; if (Math.abs(acc[1]) < 8) return; const dp = acc[1] > 0 ? 1 : -1; acc[1] = 0; st[2] = clamp(st[2] + dp, -64, 63); showToast('PITCH', (st[2] > 0 ? '+' : '') + st[2] + 'st', (st[2] + 64) / 127); }
    else {
        acc[i] += delta; if (Math.abs(acc[i]) < ((i === 2 || i === 5) ? 4 : 10)) return;      /* probability and nudge: fine steps (4 clicks); gate, ratchet, condition: ten clicks per step */
        const d = acc[i] > 0 ? 1 : -1; acc[i] = 0;
        if (i === 2) { st[3] = clamp(st[3] - d, 0, 31); showToast('PROBABILITY', PROB_PCT[st[3]] + '%', PROB_PCT[st[3]] / 100); }
        else if (i === 3) { st[5] = clamp(st[5] + d, 0, 6); st[9] = 0; showToast('GATE', GATE_NAME[st[5]], st[5] / 6); }
        else if (i === 4) { st[4] = clamp(st[4] + d, 0, 3); showToast('RATCHET', (st[4] + 1) + 'x', st[4] / 3); }
        else if (i === 5) { st[6] = clamp(st[6] + d, 0, NUD_MAX); showToast('NUDGE', nudTxt(st[6]), st[6] / NUD_MAX); }
        else { st[7] = clamp(st[7] + d, 0, 7); showToast('CONDITION', COND_NAME[st[7]], st[7] / 7); }
    }
    heldEdited = true; grpSync(i); qStep(ed[sel], ln(), heldStep); markEdited();
}
const STEP_F = [1, 2, 3, 5, 4, 6, 7, 8];
function grpSync(i) { if (!heldGrp.length || heldStep < 0) return; const st = stepAt(heldStep), f = STEP_F[i], L = ln(); for (const g of heldGrp) { const o = stepAt(g); o[f] = st[f]; if (i === 3) o[9] = 0; qStep(ed[sel], L, g); } }
function drawStepEdit() {
    const st = stepAt(heldStep);
    titleBar('STEP ' + (heldStep + 1) + (heldGrp.length ? ' x' + (heldGrp.length + 1) : '') + '  ' + (kind[sel] ? 'D' + (dsel[sel] + 1) : p2(sel + 1)), 'CLIP ' + (ed[sel] + 1));
    const L = [['Vel', String(st[1]), st[1] / 127], ['Pitch', (st[2] > 0 ? '+' : '') + st[2], (st[2] + 64) / 127], ['Prob', PROB_PCT[st[3]] + '%', PROB_PCT[st[3]] / 100],
               ['Gate', gateTxt(st), st[9] > 0 ? Math.min(1, st[9] / 16) : st[5] / 6], ['Ratch', (st[4] + 1) + 'x', st[4] / 3], ['Nudge', nudTxt(st[6]), st[6] / NUD_MAX], ['Cond', COND_NAME[st[7]], st[7] / 7], [LOCK_NAME[lockT[ln()]], st[8] ? String(Math.round((st[8] - 1) / 126 * 100)) : 'OFF', st[8] / 127]];
    for (let i = 0; i < L.length; i++) cell((i % 4) * 32, 13 + ((i / 4) | 0) * 26, L[i][0], L[i][1], L[i][2], 32);
}

/* ---- pads / steps ---- */
const TRK_COL = [1, 3, 8, 9, 11, 13, 14, 16, 17, 21, 22, 23, 25, 26, 24, 4];      /* one colour per track, all have dim variants */
function accent(tc, pref) { for (const c of pref) if (c !== tc && c !== dimCol(tc)) return c; return 19; }      /* a clearly different hue than the track colour */
function yellowish(c) { return c === 8 || c === 19 || Math.abs(c - 8) === 1 || Math.abs(c - 19) === 1; }      /* 8 and 19 both look yellow: a yellow track needs a different accent */
function rootCol(tc) { return yellowish(tc) ? 120 : accent(tc, [19, 8, 11]); }
function usedCol(tc) { return yellowish(tc) ? 120 : accent(tc, [11, 3, 8]); }
function dimCol(c) { return c >= 1 && c <= 26 ? 65 + 2 * (c - 1) : 124; }
let tcolO = new Array(NTRK).fill(0);
function trkCol(t) { return tcolO[t & 15] || TRK_COL[t & 15]; }
function trackColor(t) {
    t = BASE_T[Math.round(t)];
    if (t <= 4) return 1; if (t <= 7) return 3; if (t <= 12) return 8; if (t <= 17) return 11;
    if (t === 18 || t >= 28) return 19; if (t <= 22) return 16; return 19;
}
function drumCol(t, k) {                                  /* colour of drum sound k: its family colour, later members of the same family in the kit get the darker shade */
    const col = trackColor(pads[laneOf(t, k)][0]); let n = 0;
    for (let j = 0; j < k; j++) if (trackColor(pads[laneOf(t, j)][0]) === col) n++;
    return (n & 1) ? dimCol(col) : col;
}
function inboundActive() { return (typeof shadow_inbound_pad_midi_active === 'function') && shadow_inbound_pad_midi_active(); }
function selectTrack(t) { drumHeldK = []; padHold = {}; growTrk.fill(false); bank = t >> 2; if (sel !== t) { for (let q = 0; q < NTRK; q++) if (topModes[q] === 3) { liveFxAllOff(); topModes[q] = topPrev[q] !== 3 ? topPrev[q] : 0; markEdited(); }      /* Live FX is a visit: another track = gone until you switch it on again */
        fb = null; kitBackup = null; sel = t; stepPage = 0; heldStep = -1; heldPad = -1; acc.fill(0); syncSel(); if (view === 44 && !chordOn()) tabEnter(1); } needsRedraw = true; }
function wipeSlot(d) {                                    /* a kit is handed to another track: back to the default kit */
    ensureCs(); const def = defaultPads();
    for (let k = 0; k < 16; k++) {
        const l = NTRK + d * 16 + k; pads[l] = def[l];
        for (let c = 0; c < NPAT; c++) csnd[c][l] = def[l].slice();
        for (let p = 0; p < NPAT; p++) steps[p][l] = blankLane();
    }
    undoStack = []; redoStack = []; miscLast = null;
}
function pushLanes(t, full) {                             /* send a drum track's 16 sounds + patterns to the engine */
    ensureCs();
    for (const l of lanesOf(t)) { qPadLive(l); for (let c = 0; c < NPAT; c++) sendCs(c, l); if (llen[l]) enqueue('llen' + l, 'llen', l + ',' + llen[l]); }
    for (let p = 0; p < NPAT; p++) for (const l of lanesOf(t)) if (full || laneHas(p, l)) qSeq(p, l);
    if (clipsCur[t] >= 0 || !playing) enqueue('clip' + t, 'clip', t + ',' + ed[t]);
}
function setKind(t, k) {                                  /* every track is a synth or a drum track (max 4 drum tracks, 16 sounds each) */
    k = k ? 1 : 0;
    if (kind[t] === k) { showToast('TRACK ' + p2(t + 1), k ? 'DRUM' : 'SYNTH', k); return; }
    if (k === 1) {
        let n = 0; for (let i = 0; i < NTRK; i++) if (kind[i]) n++;
        if (n >= 4) { showToast('MAX 4 DRUM', 'TRACKS', 1); return; }
        let full = false;
        if (slot[t] < 0) {
            full = true;                                      /* a freshly taken kit slot: send blank patterns so no old notes survive in the engine */
            const used = [false, false, false, false]; for (let i = 0; i < NTRK; i++) if (slot[i] >= 0) used[slot[i]] = true;
            let d = used.indexOf(false);
            if (d < 0) { for (let i = 0; i < NTRK; i++) if (slot[i] >= 0 && !kind[i]) { d = slot[i]; slot[i] = -1; break; } ensureSlot(d); wipeSlot(d); full = true; }
            slot[t] = d; ensureSlot(d);
        }
        kind[t] = 1; dsel[t] = clamp(dsel[t], 0, 15);
        for (const l of lanesOf(t)) if (!llen[l]) { let far = false; for (let p = 0; p < NPAT && !far; p++) for (let s2 = 16; s2 < MAXSTEP; s2++) if (steps[p][l][s2][0]) { far = true; break; } if (!far) llen[l] = 16; }      /* every drum sound starts with its own 16 steps */
        enqueue('kind' + t, 'kind', t + ',' + slot[t]); pushLanes(t, full);
    } else {
        kind[t] = 0; enqueue('kind' + t, 'kind', t + ',-1');
        let empty = FMI > 0; for (let p = 0; p < NPAT && empty; p++) for (let s2 = 0; s2 < MAXSTEP; s2++) if (steps[p][t][s2][0]) { empty = false; break; }
        if (empty) { initSound(t); pads[t][PI.det] = 0.25; pads[t][PI.sub] = 0.3; pads[t][PI.fkey] = 0; pads[t][PI.decay] = 0.45; fltSet(t, FLT_OPEN); pads[t][0] = FMI; qPad(t); storeCs(t); }      /* a fresh synth track starts as the plain INIT FM, not a preset (only when it has no notes yet) */
    }
    heldStep = -1; heldPad = -1; stepPage = 0; acc.fill(0);
    if (t === sel) syncSel();
    showToast('TRACK ' + p2(t + 1), k ? 'DRUM' : 'SYNTH', k); markEdited();
}
function notePadSemi(d) { if (d >= 32) return d - 44; return 12 * ((d / 7) | 0) + SCALE7[Math.round(master[2]) % 3][d % 7] - 12; }

function copyTrack(src, dst) {                              /* (unused helper, synth tracks only) */
    pads[dst] = pads[src].slice(); qPad(dst);
    for (let s = 0; s < MAXSTEP; s++) steps[ed[dst]][dst][s] = steps[ed[src]][src][s].slice();
    lens[dst] = lens[src]; enqueue('len' + dst, 'len', dst + ',' + lens[dst]); qSeq(ed[dst], dst);
}
function stepRecord(lane, semi, vel) {                 /* Rec held + stopped: pads write notes at the record head */
    const track = laneTrk(lane), c = ed[track], n = effLen(lane);
    snap(c, lane);
    const st = steps[c][lane][recHead % n]; st[0] = 1; st[1] = fullVel ? 127 : clamp(vel, 1, 127); st[2] = clamp(semi, -64, 63);
    qStep(c, lane, recHead % n); recStepUsed = true;
    showToast('STEP REC', 'T' + p2(track + 1) + ' S' + (recHead % n + 1), (recHead % n) / MAXSTEP);
    recHead = (recHead + 1) % n; markEdited();
}
function resetLen(t) {                                    /* an emptied clip goes back to the standard length (16 steps), drum sounds follow the track again */
    if (lens[t] !== 16) { lens[t] = 16; enqueue('len' + t, 'len', t + ',16'); }
    for (const l of lanesOf(t)) if (llen[l]) { llen[l] = 0; enqueue('llen' + l, 'llen', l + ',0'); }
    growTrk[t] = false;
}
function recStopLen() {                                   /* Rec pressed the second time: the clip is as long as you played (16, 32, 48 ... steps, rounded up) */
    let gpos = gs - 1; { const r = dspGet('pos'); if (typeof r === 'string') { const a = r.split(','), g = parseInt(a[1]) || 0; if (g > 0) gpos = g - 1 + (parseInt(a[3]) || 0) / 100; } }      /* the DSP's own step counter, the same one the recorded hits use */
    for (let t = 0; t < NTRK; t++) {
        if (!growTrk[t]) continue;
        const el = Math.max(0.01, gpos - growG0[t]), n = clamp(Math.ceil(el / 16) * 16, 16, MAXSTEP);
        if (lens[t] !== n) { reMap(t, lens[t], n); lens[t] = n; enqueue('len' + t, 'len', t + ',' + n); }
        showToast('REC LENGTH', n + ' STEPS', n / MAXSTEP);
    }
}
function growTick() {                                     /* recording into an empty clip: the loop follows how long you play (16 steps, 32, 48 ...); stopping before the end of the bar keeps 16 */
    if (!recArmed || !playing) return;
    for (let t = 0; t < NTRK; t++) {
        if (!growTrk[t]) continue;
        const len = lens[t]; if (len >= MAXSTEP) continue;
        if (gs - 1 >= 0 && (gs - 1) % len === len - 1 && gs - 1 < len * 2) growLoop(t, len + 16);
    }
}
function reMap(t, oldLen, n) {                           /* a clip that changes length keeps every note on the same beat of the shared grid: notes recorded from a bar line that is not a multiple of the new length move along, a shorter clip folds back */
    const c = ed[t], sh = ((growG0[t] % n) + n) % n;
    for (const l of lanesOf(t)) {
        const a = steps[c][l], tmp = a.slice(0, Math.max(oldLen, n)).map(x => x.slice());
        let ch = false;
        if (n > oldLen) {
            if (!sh) continue;
            const out = []; for (let i = 0; i < n; i++) out.push([0, 100, 0, 0]);
            for (let i = oldLen; i < n; i++) if (tmp[i][0]) out[i] = tmp[i];
            for (let i = 0; i < oldLen; i++) out[(i + sh) % n] = tmp[i];
            for (let i = 0; i < n; i++) a[i] = out[i];
            ch = true;
        } else if (n < oldLen) {
            for (let i = n; i < oldLen; i++) if (tmp[i][0]) { if (!a[i % n][0]) a[i % n] = tmp[i]; a[i] = [0, 100, 0, 0]; ch = true; }
        }
        if (ch) qSeq(c, l);
    }
}
function growLoop(lane, n) {
    const t = laneTrk(lane); n = clamp(n, 1, MAXSTEP);
    if (kind[t] && llen[lane]) { llen[lane] = n; enqueue('llen' + lane, 'llen', lane + ',' + n); }
    else { if (growTrk[t] && !(kind[t] && llen[lane])) reMap(t, lens[t], n); lens[t] = n; enqueue('len' + t, 'len', t + ',' + n); }
    showToast('LOOP GROWS', n + ' STEPS', n / MAXSTEP);
}
let recSnap = {}, recGrp = 0, recLastT = 0;
function recUndo(track, lane) {                           /* one live take = one undo step per lane (the first hit of a lane in this clip stores the state before the take) */
    const now = Date.now();
    if (now - recLastT > 8000) { recSnap = {}; recGrp = ++undoGroup; }
    recLastT = now;
    const key = ed[track] * 128 + lane;
    if (!recSnap[key]) { recSnap[key] = 1; snap(ed[track], lane, recGrp); }
}
let lastRec = null; const recHeldPad = {};
let padHold = {};                                          /* pads held down right now: a step button pressed meanwhile gets exactly that note */
function recRelease(key, lane) {                           /* the recorded note lasts as long as the pad was held (long = step length up to 15 steps, very short = short gate) */
    const h = recHeldPad[key]; delete recHeldPad[key]; if (!h || !pads[lane]) return;
    let dur = (Date.now() - h.t) / (15000 / bpm);                      /* fallback: the clock of the UI */
    if (playing && h.g !== undefined) { const r = dspGet('pos'); if (typeof r === 'string') { const a = r.split(','), g = parseInt(a[1]) || 0, f = parseInt(a[3]) || 0; const d2 = (g - h.g) + (f - h.f) / 100; if (d2 >= 0 && d2 < 4096) dur = d2; } }
    const st = steps[h.c][h.l][h.i]; if (!st[0]) return;
    for (let q = st.length; q < 10; q++) st[q] = 0;
    const arp = Math.round(pads[lane][PI.arp]) > 0;
    if (dur >= (arp ? 1.5 : 1.4)) st[9] = clamp(Math.round(dur), 2, 15);
    else if (arp) return;
    else if (dur < 0.35) st[5] = 1; else if (dur < 0.65) st[5] = 2; else return;
    qStep(h.c, h.l, h.i); markEdited();
}
function recHit(lane, semi, vel, key) { lastRec = null; recordHit(lane, semi, vel); if (lastRec) recHeldPad[key] = { c: lastRec.c, l: lastRec.l, i: lastRec.i, g: lastRec.g, f: lastRec.f, t: Date.now() }; }
function recordHit(lane, semi, vel) {
    const track = laneTrk(lane);
    if (recDown && !playing) { stepRecord(lane, semi, vel); return; }
    if (fullVel) vel = 127;
    if (!recArmed) return;
    if (!playing) {                                   /* arm + first hit starts the clip recorder */
        recUndo(track, lane);
        playing = true; gs = 0; dspSet('play', '1'); growTrk[track] = !trackHasSteps(ed[track], track); growG0[track] = 0;
        const st0 = steps[ed[track]][lane][0]; st0[0] = 1; st0[1] = clamp(vel, 1, 127); st0[2] = clamp(semi, -64, 63);
        qStep(ed[track], lane, 0); lastRec = { c: ed[track], l: lane, i: 0, g: 1, f: 0 }; showToast('REC', 'T' + p2(track + 1) + ' C' + (ed[track] + 1), 1); markEdited(); return;
    }
    recUndo(track, lane);
    const r = dspGet('pos');
    if (typeof r !== 'string') return;
    const a = r.split(','), g = parseInt(a[1]) || 0, fr = parseInt(a[3]) || 0;
    let len = effLen(lane);
    if (!growTrk[track] && !trackHasSteps(ed[track], track)) { growTrk[track] = true; { const g1 = g - 1, l1 = Math.max(1, len); growG0[track] = g1 - (((g1 % l1) + l1) % l1); } }
    if (growTrk[track] && len < MAXSTEP && gs - 1 >= len && ((g % len) + len) % len >= len - 2) {      /* from the second pass on, a hit in the last two steps means: keep going */ growLoop(lane, len < 16 ? 16 : len * 2); len = effLen(lane); }
    const pRaw = (g - 1) + fr / 100 + 0.5 - Math.round(90 - rqOff * 80) / 100, pSn = qSnapPos(pRaw);      /* the hit's exact position, snapped to the QUANT grid (1/16 = whole steps, finer grids keep the rest as nudge) */
    let tS = Math.floor(pSn + 1e-6), tN = Math.round((pSn - tS) / 0.025); if (tN > NUD_MAX) { tS++; tN = 0; }
    const target = ((tS % len) + len) % len;
    const st = steps[ed[track]][lane][target]; st[6] = tN; for (let q = st.length; q < 10; q++) st[q] = 0; st[5] = 0; st[9] = 0;
    st[0] = 1; st[1] = clamp(vel, 1, 127); st[2] = clamp(semi, -64, 63);
    qStep(ed[track], lane, target); lastRec = { c: ed[track], l: lane, i: target, g: g, f: fr }; markEdited();
}
let cpPad = -1, cpTrk = -1;
function padCopy(k) {                                       /* Copy + pad = pick the source, Copy + pad again = copy there (Shift + Copy + pad = swap) */
    if (cpPad < 0 || cpTrk !== sel || cpPad === k) { cpPad = k; cpTrk = sel; showToast('SOUND ' + (k + 1), 'PICK TARGET', 0.5); return; }
    const a = laneOf(sel, cpPad), b = laneOf(sel, k);
    if (shiftHeld) { const t = pads[a]; pads[a] = pads[b]; pads[b] = t; qPad(a); qPad(b); showToast('SWAPPED', (cpPad + 1) + ' <> ' + (k + 1), 1); }
    else { pads[b] = pads[a].slice(); qPad(b); showToast('COPIED', (cpPad + 1) + ' > ' + (k + 1), 1); }
    cpPad = -1; markEdited();
}
let drumHeldK = [];                                        /* drum sound pads that are held down right now: the master knob then sets the volume of that sound */
function drumHeldSet(k, on) { drumHeldK = drumHeldK.filter(x => x !== k); if (on) drumHeldK.push(k); suppressMasterVol(drumHeldK.length || trkHeld >= 0 ? 1 : 0); }
function drumPad(idx, on, vel) {                          /* drum track: left 4x4 = the 16 sounds, right 4x4 = PITCH / REPEAT / STEPS of the selected sound */
    const cc = idx & 7, rr = idx >> 3, m = topModes[sel];
    if (soundArea(cc)) {
        const k = rr * 4 + (cc & 3), l = laneOf(sel, k);
        if (on && copyHeld) { padCopy(k); return; }
        if (on) {
            if (k !== dsel[sel]) { dsel[sel] = k; heldStep = -1; heldPad = -1; syncSel(); needsRedraw = true; markEdited(); }
            if (shiftHeld && !copyHeld) { showToast('SOUND ' + (k + 1), TYPE_NAME[Math.round(pads[l][0])], k / 15); return; }          /* Shift + pad = select silently */
            if (deleteHeld) {
                const c = ed[sel]; snap(c, l); let had = false; for (let s = 0; s < MAXSTEP; s++) { if (steps[c][l][s][0]) had = true; steps[c][l][s] = [0, 100, 0, 0]; }
                if (had) { qSeq(c, l); showToast('SOUND ' + (k + 1), 'STEPS CLEARED', 0); }
                else { const d = defaultPads()[l]; for (let i = 0; i < PK.length; i++) if (i !== PI.vol && i !== PI.pan) pads[l][i] = d[i]; qPad(l); showToast('SOUND ' + (k + 1), 'RESET', 0); }
                markEdited(); return;
            }
            const v = fullVel ? 127 : Math.max(1, vel);
            recHit(l, 0, v, 'd' + idx); logHit(l, 0, v); padHold['d' + idx] = { lane: l, semi: 0, vel: v };
            drumHeldSet(k, true);
            flushPad(l); dspSet('dhit', l + ',' + v + ',' + ((m === 1 || m === 3) ? 1 : 0));        /* Note Repeat: the sound pads repeat at the speed chosen on the right block */
        } else { drumHeldSet(k, false); dspSet('dhit', l + ',0,0'); recRelease('d' + idx, l); delete padHold['d' + idx]; }
        return;
    }
    if (m === 2) { if (on) stepsPad(idx, vel); else stepsPadUp(idx); return; }
    if (m === 1) {                                         /* Note Repeat: the 16 pads on the right are the speeds - pressing one repeats the selected sound at that speed right away (and you hear it) */
        const L = ln(), key = 'r' + idx;
        if (on) {
            const ri = rr * 4 + (cc & 3), v = fullVel ? 127 : Math.max(1, vel);
            setRepRate(ri); dspSet('reprate', String(repRate));
            if (!deleteHeld && !shiftHeld) { recHit(L, 0, v, key); logHit(L, 0, v); padHold[key] = { lane: L, semi: 0, vel: v }; flushPad(L); dspSet('dhit', L + ',' + v + ',1'); }
        } else if (padHold[key]) { dspSet('dhit', L + ',0,0'); recRelease(key, L); delete padHold[key]; }
        return;
    }
    if (shiftHeld) return;
    const j = degOf(cc & 3, rr, 4), L = ln();
    if (on && deleteHeld) { delNote(L, notePadSemi(j) + oct()); return; }
    if (on && heldStep < 0) { recHit(L, notePadSemi(j) + oct(), vel, 'p' + idx); logHit(L, notePadSemi(j) + oct(), vel); }
    if (!on) { recRelease('p' + idx, L); delete padHold['p' + idx]; } else padHold['p' + idx] = { lane: L, semi: clamp(notePadSemi(j) + oct(), -64, 63), vel: fullVel ? 127 : Math.max(1, vel) };
    if (on && heldStep >= 0) {
        const st = stepAt(heldStep); st[2] = clamp(notePadSemi(j) + oct(), -64, 63);
        heldEdited = true; qStep(ed[sel], L, heldStep); showToast('PITCH', (st[2] > 0 ? '+' : '') + st[2] + 'st', (st[2] + 64) / 127); markEdited();
    }
    if (on) flushPad(L);
    dspSet('npad', j + ',' + (on ? Math.max(1, vel) : 0) + ',' + (m === 1 ? 1 : 0));
}
function delNote(L, semi) {                                 /* Delete + note pad: removes every step of that pitch */
    const c = ed[sel]; let n = 0; snap(c, L);
    for (let k = 0; k < MAXSTEP; k++) { const st = steps[c][L][k]; if (st[0] && st[2] === semi) { steps[c][L][k] = [0, 100, 0, 0]; n++; } }
    if (n) { qSeq(c, L); markEdited(); }
    showToast(n ? 'NOTE DELETED' : 'NO NOTE HERE', n ? n + ' STEPS' : '', 0);
}
function onPad(idx, on, vel) {
    if (askKind >= 0) return;                                 /* the Synth / Drum question is open: pads must not play notes */
    if (muteHeld || view === 20) {                            /* MUTE + pad (session / mixer: just the pad) = mute without leaving the page */
        if (on) {
            if (view === 7) { muteUsed = true; const t = bank * 4 + (3 - (idx >> 3)); if (t >= 0 && t < NTRK) { if (shiftHeld) soloTrack(t); else toggleMute(t); } }
            else if (view === 20) { if (idx < NTRK) toggleMute(idx); }
            else if (kind[sel] && soundArea(idx & 7)) { muteUsed = true; toggleLaneMute(laneOf(sel, (idx >> 3) * 4 + ((idx & 7) & 3))); }
            else if (muteHeld) { muteUsed = true; toggleMute(sel); }
        }
        return;
    }
    if (view === 31) { if (on) songPad(songPadK(idx)); return; }      /* song: pad = step */
    if (view === 7) { if (on) clipsPad(idx); return; }       /* session: pick track + launch clip */
    if (!on && lfxHeld[idx]) { liveFxOff(idx); return; }
    if (!on && fillHeld[idx]) { fillHit(idx, false, 0); return; }
    if (topModes[sel] === 3 && fillPad(idx)) { fillHit(idx, on, vel); return; }
    if (topModes[sel] === 3) { const fn = lfxPad(idx); if (fn >= 0) { if (on) liveFxOn(idx, fn); return; } }
    if (kind[sel]) { drumPad(idx, on, vel); return; }
    const m = topModes[sel];
    if (m === 2) { if (on) stepsPad(idx, vel); else stepsPadUp(idx); return; }
    const dg = m === 4 ? degOf(idx & 3, idx >> 3, 4) : degOf(idx & 7, idx >> 3, 8);
    if (on && deleteHeld) { delNote(sel, notePadSemi(dg) + oct()); return; }
    if (on) lastSemi[sel] = clamp(notePadSemi(dg) + oct(), -64, 63);
    if (on) padHold[idx] = { lane: sel, semi: clamp(notePadSemi(dg) + oct(), -64, 63), vel: fullVel ? 127 : Math.max(1, vel) }; else delete padHold[idx];
    if (on && heldStep < 0) { recHit(sel, notePadSemi(dg) + oct(), vel, idx); logHit(sel, notePadSemi(dg) + oct(), vel); }
    if (!on) recRelease(idx, sel);
    if (on && heldStep >= 0) {                                       /* hold step + pad = set pitch */
        const st = stepAt(heldStep); st[2] = clamp(notePadSemi(dg) + oct(), -64, 63);
        heldEdited = true; qStep(ed[sel], sel, heldStep); showToast('PITCH', (st[2] > 0 ? '+' : '') + st[2] + 'st', (st[2] + 64) / 127); markEdited();
    }
    if (!inboundActive()) dspSet('npad', dg + ',' + (on ? Math.max(1, vel) : 0) + ',' + (m === 1 ? 1 : (m === 4 && (idx & 7) < 4 ? 2 : 0)));
}
function onStep(i, on) {
    const s = stepPage * 16 + i;
    if (on && muteHeld) { muteUsed = true; if (shiftHeld) soloTrack(i); else toggleMute(i); return; }
    if (on && recDown && !playing) {
        recStepUsed = true; recHead = s; const c0 = ed[sel], L0 = ln(); if (s < LL()) { snap(c0, L0); steps[c0][L0][s] = [0, 100, 0, 0]; qStep(c0, L0, s); }
        showToast('REC HEAD', String(s + 1), s / MAXSTEP); markEdited(); return;
    }
    if (on) {
        if (loopHeld && view === 0) { if (deleteHeld) { if (kind[sel]) { llen[ln()] = 0; enqueue('llen' + ln(), 'llen', ln() + ',0'); showToast('SOUND LOOP', 'FOLLOWS TRACK', 0); markEdited(); } } else setLen(s + 1, shiftHeld); return; }
        if (copyHeld && view === 0 && !shiftHeld) { stepCopyKey(s); return; }
        if (shiftHeld && heldStep >= 0 && s !== heldStep && s < LL() && view === 0) {      /* hold a step, then Shift + more steps: edit them together */
            if (!heldGrp.includes(s)) { const L = ln(); snap(ed[sel], L); heldGrp.push(s); heldEdited = true; const g = stepAt(s); if (!g[0]) { g[0] = 1; if (fullVel) g[1] = 127; } qStep(ed[sel], L, s); markEdited(); showToast('STEPS', (heldGrp.length + 1) + ' TOGETHER', Math.min(1, (heldGrp.length + 1) / 8)); needsRedraw = true; }
            return;
        }
        if (shiftHeld) { shiftStep(i); return; }
        if (view === 31) { songStepKey(i); return; }
        if (view === 7) { if (loopHeld && i < NPAT) launchScene(i); else if (!loopHeld) selectTrack(i); return; }
        if (s >= LL()) return;
        const L = ln();
        if (deleteHeld) { snap(ed[sel], L); steps[ed[sel]][L][s] = [0, 100, 0, 0]; qStep(ed[sel], L, s); markEdited(); return; }
        const ph = Object.keys(padHold); if (ph.length && heldStep < 0 && view !== 7) {      /* a pad is held: this step gets exactly its note (and how hard it was hit) */
            const h = padHold[ph[ph.length - 1]], c = ed[sel]; snap(c, h.lane); const st = steps[c][h.lane][s]; for (let q = st.length; q < 10; q++) st[q] = 0;
            st[0] = 1; st[1] = clamp(h.vel, 1, 127); st[2] = h.semi; qStep(c, h.lane, s); markEdited(); showToast('STEP ' + (s + 1), kind[sel] ? 'SOUND SET' : (h.semi > 0 ? '+' : '') + h.semi + ' ST', 1); needsRedraw = true; return;
        }
        if (heldStep >= 0 && heldStep !== s && !heldGrp.includes(s) && s < LL()) {
            /* a second step while one is held: (1) the first step lasts until here, like on the Move; (2) the step is also held with the first one, so as long as you keep both down the knobs edit both (probability ...) */
            if (s > heldStep && !heldGrp.length) {
                const n = Math.min(15, s - heldStep + 1), st0 = stepAt(heldStep); snap(ed[sel], L); st0[0] = 1; st0[9] = n; qStep(ed[sel], L, heldStep);
                showToast('STEP LENGTH', n + ' STEPS', Math.min(1, n / 16));
            }
            heldGrp.push(s); heldEdited = true; markEdited(); needsRedraw = true;
            return;
        }
        snap(ed[sel], L); heldStep = s; heldEdited = false; heldGrp = [];
        const st = stepAt(s); heldWasOn = !!st[0];
        if (!heldWasOn) { st[0] = 1; if (fullVel) st[1] = 127; if (!kind[sel] && lastSemi[sel] !== null && view === 0 && topModes[sel] !== 2) st[2] = lastSemi[sel]; qStep(ed[sel], L, s); markEdited(); if (!playing && view === 0) launchClip(sel, ed[sel]); }      /* first step set while stopped: playback starts */
    } else if (heldGrp.includes(s)) { heldGrp.splice(heldGrp.indexOf(s), 1); needsRedraw = true;
    } else if (heldStep === s) {
        const L = ln(); heldGrp = [];
        if (heldWasOn && !heldEdited) { steps[ed[sel]][L][s] = [0, 100, 0, 0]; qStep(ed[sel], L, s); markEdited(); }
        heldStep = -1; needsRedraw = true;
    }
}

/* ---- drawing ---- */
let labFull = null, labW = 28;                                 /* parameter tile label: when it does not fit in the normal font it is drawn complete in the small font */
function txt(x, y, s, c) {
    if (labFull !== null && s.length && labFull.indexOf(s) === 0) { const f = labFull; labFull = null; if (s !== f) { const n = Math.floor(labW / 4); txtS(x, y + 1, f.length <= n ? f : f.substring(0, n), c); return; } }
    print(x, y, tr(s), c === undefined ? 1 : c);
}
function txtB(x, y, s, c) { print(x, y, tr(s), c === undefined ? 1 : c); }          /* values: normal weight again */
const HFONT = {"A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"], "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."], "C": [".####", "#....", "#....", "#....", "#....", "#....", ".####"], "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."], "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"], "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."], "G": [".####", "#....", "#....", "#..##", "#...#", "#...#", ".####"], "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"], "I": [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."], "J": ["....#", "....#", "....#", "....#", "....#", "#...#", ".###."], "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"], "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"], "M": ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"], "N": ["#...#", "##..#", "#.#.#", "#.#.#", "#..##", "#...#", "#...#"], "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."], "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."], "Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"], "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"], "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."], "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."], "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."], "V": ["#...#", "#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."], "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"], "X": ["#...#", ".#.#.", "..#..", "..#..", "..#..", ".#.#.", "#...#"], "Y": ["#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..", "..#.."], "Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"], "0": [".###.", "#..##", "#.#.#", "#.#.#", "##..#", "#...#", ".###."], "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."], "2": [".###.", "#...#", "....#", "..##.", ".#...", "#....", "#####"], "3": [".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."], "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."], "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."], "6": [".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."], "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."], "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."], "9": [".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."], " ": ["...", "...", "...", "...", "...", "...", "..."], "-": [".....", ".....", ".....", ".###.", ".....", ".....", "....."], ".": ["..", "..", "..", "..", "..", "##", "##"], ":": ["..", "##", "##", "..", "##", "##", ".."], "/": ["....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...."], "%": ["##..#", "##.#.", "...#.", "..#..", ".#.##", "#..##", "....."], "+": [".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."], "<": ["...#", "..#.", ".#..", "#...", ".#..", "..#.", "...#"], ">": ["#...", ".#..", "..#.", "...#", "..#.", ".#..", "#..."], "*": [".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."], "#": [".#.#.", "#####", ".#.#.", ".#.#.", "#####", ".#.#.", "....."], "?": [".###.", "#...#", "....#", "..##.", "..#..", ".....", "..#.."], "!": ["#", "#", "#", "#", "#", ".", ".", "#"], "_": [".....", ".....", ".....", ".....", ".....", ".....", "#####"], "'": ["#", "#", "#", ".", ".", ".", ".", "."], "[": ["###", "#..", "#..", "#..", "#..", "#..", "###"], "]": ["###", "..#", "..#", "..#", "..#", "..#", "###"]};
function hfW(s) { let w = 0; s = tr(String(s)).toUpperCase(); for (let i = 0; i < s.length; i++) { const g = HFONT[s[i]] || HFONT['?']; w += g[0].length + 1; } return Math.max(0, w - 1); }
function hfFit(s, w) { s = tr(String(s)); while (s.length > 1 && hfW(s) > w) s = s.substring(0, s.length - 1); return s; }
function txtHL(x, y, label, w, c) {                         /* parameter name in the header letters; too long = complete in the small letters instead of cut off */
    label = String(label); if (hfW(label) <= w) { txtH(x, y, label, c); return; }
    const n = Math.floor(w / 4); txtS(x, y + 1, label.length <= n ? label : label.substring(0, n), c);
}
function txtH(x, y, s, c) {                                  /* header font: squared 5x7 capitals, drawn pixel by pixel */
    c = c === undefined ? 1 : c; if (NH && c === 0) NH.note('h', x, y, s); s = tr(String(s)).toUpperCase();
    for (let i = 0; i < s.length; i++) {
        const g = HFONT[s[i]] || HFONT['?'], gw = g[0].length;
        for (let r = 0; r < 7; r++) { const row = g[r]; let a = -1; for (let q = 0; q <= gw; q++) { const on = q < gw && row[q] === '#'; if (on && a < 0) a = q; else if (!on && a >= 0) { fill_rect(x + a, y + r, q - a, 1, c); a = -1; } } }
        x += gw + 1;
    }
}
let touchK = -1, touchT = 0;                              /* knob under your finger: its name / value stays on screen until you let go */
let hot = { v: -1, i: -1, t: 0 };                       /* the knob you touched last: its cell lights up for a moment */
function tw(s) { s = tr(s); return (typeof text_width === 'function') ? text_width(s) : s.length * 6; }
let tabStrip = false;
const GW = { MIDI: 19, FX: 18, SAMPLE: 18, EUCLID: 17, DELAY: 16 };
function lastTabId() { return trackTabs().indexOf(9) >= 0 ? 9 : 28; }      /* SYNTH ENGINE (second tab of a synth track) or SAMPLE (last tab of the others) */
const TAB_PARENT = { 43: 2, 50: 38, 39: 46, 47: 9, 49: 27 };      /* second pages belong to a tab: EQ > FILTER, GRAIN > FX, FX BUS 2 > FX BUS */
function tabIndex(tabs) { return tabs.indexOf(view === 9 ? lastTabId() : (view === 38 && fxPage ? 46 : (TAB_PARENT[view] || view))); }
function tabPageName(t) { return view === 43 ? 'EQ' : (view === 50 ? 'GRAIN' : (view === 38 && fxPage) ? 'FX BUS 2' : (t === 9 && synPage) ? 'ENGINE 2' : (t === 27 && fxPage) ? 'MIDI FX 2' : TAB_SHORT[t]); }
function tabPageNum() { return (view === 43 || view === 50 || (view === 38 && fxPage) || (view === 9 && synPage) || (view === 27 && fxPage)) ? '.2' : ''; }
let synPage = 0, fxPage = 0;
function tabPg(t) {                                        /* [page, pages] inside the current tile, always shown: tile name top left, page top right */
    if (t === 36) return [euPage + 1, EU_NPG];
    if (t === 28) return [smpPage + 1, 4];
    const two = (t === 9 || t === 2 || t === 38 || t === 46 || t === 27);
    return [tabPageNum() ? 2 : 1, two ? 2 : 1];
}
function pg2(v) { return v === 9 && synPage ? 47 : (v === 38 && fxPage ? 39 : (v === 27 && fxPage ? 49 : v)); }      /* second pages (Jog click): ENGINE 2, FX TIME, MIDI FX 2 */
function itemsView() { return view === 28 && smpPage ? (smpPage === 1 ? 29 : (smpPage === 3 ? 48 : 32)) : pg2(view); }
let exitDlg = -1, suspended = false, selfTestT = 0;
function selfTest() {                                       /* once after start: can Taktile really save, and is the sample folder there? */
    let bad = '';
    try { const f = '/data/UserData/schwung/taktile-selftest.txt'; host_write_file(f, 'ok' + Date.now()); const r = host_read_file(f); if (!r || r.indexOf('ok') !== 0) bad = 'CANNOT SAVE'; } catch (e) { bad = 'STORAGE ERROR'; }
    if (!bad) { const n = parseInt(dspGet('smpn')); if (isNaN(n)) bad = 'DSP NOT READY'; }
    if (bad) { showToast('SELF TEST', bad, 0); toast.ticks = 120; }
}
let lastTab = new Array(NTRK).fill(0), prevTabView = 1;
const TAB_SHORT = { 50: 'GRAIN', 43: 'EQ', 1: 'PRESET', 9: 'ENGINE', 28: 'SAMPLE', 2: 'FILTER', 3: 'MOD', 38: 'TRK FX', 39: 'BUS 2', 46: 'FX BUS', 27: 'MIDI', 36: 'EUCLID' };
function drawTabs() {                                      /* header font: previous page | current page (white block) | next page */
    const tabs = trackTabs(), cur = Math.max(0, tabIndex(tabs));
    const nm = TAB_SHORT[tabs[cur]], num = String(cur + 1), nw = num.length * 4 + 3, pg = tabPg(tabs[cur]), pt = pg[0] + '/' + pg[1], pw = pt.length * 4 + 3, w = hfW(nm) + 8 + nw + pw, x = 64 - Math.floor(w / 2);
    fill_rect(x, 0, w, 10, 1); txtS(x + 3, 2, num, 0); txtH(x + 4 + nw, 1, nm, 0); txtS(x + w - pw, 2, pt, 0);
    if (cur > 0) txtH(1, 1, hfFit('<' + TAB_SHORT[tabs[cur - 1]], x - 4), 1);
    if (cur < tabs.length - 1) { const t = hfFit(TAB_SHORT[tabs[cur + 1]] + '>', 124 - (x + w)); txtH(127 - hfW(t), 1, t, 1); }
    fill_rect(0, 9, 128, 2, 1);
}
function tabEnter(v) {
    if (isTab(view) && view !== v) prevTabView = view;
    view = v; acc.fill(0); needsRedraw = true; if (v !== 9) synPage = 0; fxPage = 0;
    if (v === 28) { smpPage = 0; broOpen = false; smpRefresh(); }
    if (v === 36) euSetPage(euPage);
}
function tabMove(dl) {
    const tabs = trackTabs(), i = tabIndex(tabs), ni = clamp((i < 0 ? 0 : i) + (dl > 0 ? 1 : -1), 0, tabs.length - 1);
    if (ni === i) return;
    tabEnter(tabs[ni]); showToast((ni + 1) + '/' + tabs.length, '', 0); toast.ticks = Math.min(toast.ticks, 40);
}
function tabNormalize() {
    if (view !== 9) return;
    const w = lastTabId(); if (view !== w) tabEnter(w);
}

/* ---- big value display, kind icons, arc knobs, minimal header ---- */
function hfBigW(s, sc) { return hfW(s) * sc + Math.max(0, String(s).length - 1) * 0; }
function hfBig(x, y, s, sc, c) {                              /* the header font scaled up: used for the big value while a knob is turned */
    s = String(s).toUpperCase();
    for (let i = 0; i < s.length; i++) {
        const g = HFONT[s[i]] || HFONT['?'], gw = g[0].length;
        for (let r = 0; r < 7; r++) for (let q = 0; q < gw; q++) if (g[r][q] === '#') fill_rect(x + q * sc, y + r * sc, sc, sc, c);
        x += (gw + 1) * sc;
    }
}
function bigW(s, sc) { s = String(s).toUpperCase(); let w = 0; for (let i = 0; i < s.length; i++) { const g = HFONT[s[i]] || HFONT['?']; w += (g[0].length + 1) * sc; } return Math.max(0, w - sc); }
function drawBigValue(t) {
    const avail = (t.ck && CURVE_KEYS[t.ck]) ? 82 : 128;
    fill_rect(0, 11, 128, 53, 0);
    const vt = String(t.text).toUpperCase(), m = vt.match(/^[-+]?[0-9.]+/), main = m ? m[0] : vt, unit = m ? vt.substring(m[0].length).trim() : '';
    let sc = 3; while (sc > 1 && bigW(main, sc) > avail - 6) sc--;
    const mm = bigW(main, sc) > avail - 6 ? hfFit(main, Math.floor((avail - 6))) : main;
    const w = bigW(mm, sc), x = Math.floor((avail - w) / 2), y = 20 + Math.floor((21 - 7 * sc) / 2);
    hfBig(x, y, mm, sc, 1);
    if (unit) { const u = hfFit(unit, avail - 6); txtH(Math.floor((avail - hfW(u)) / 2), y + 7 * sc + 5, u, 1); }
    const bw = avail - 12; fill_rect(6, 57, bw, 1, 1); const f = Math.round(bw * clamp(t.f, 0, 1)); if (f > 0) fill_rect(6, 56, f, 3, 1);
}
function drawKindIcon(x, y, drum, muted) {                     /* 7x7: synth = sine wave, drum = ring with a dot */
    if (drum) { draw_rect(x, y, 7, 7, 1); fill_rect(x + 2, y + 2, 3, 3, 1); fill_rect(x, y, 1, 1, 0); fill_rect(x + 6, y, 1, 1, 0); fill_rect(x, y + 6, 1, 1, 0); fill_rect(x + 6, y + 6, 1, 1, 0); }
    else { const ys = [3, 1, 0, 1, 3, 5, 6, 5, 3]; for (let i = 0; i < 7; i++) { const a = ys[i], b = ys[i + 1] !== undefined ? ys[i + 1] : a; set_pixel(x + i, y + a, 1); if (Math.abs(b - a) > 1) set_pixel(x + i, y + Math.round((a + b) / 2), 1); } }
    if (muted) for (let i = 0; i < 7; i++) set_pixel(x + i, y + 6 - i, 1);
}
function cellArc(x, y, label, value, f, cw, hotE) {            /* round knob: 270 degree ring filled up to the value; the name stays complete in the clear header letters, touched = the full value replaces it */
    const vs = String(value);
    if (hotE) { fill_rect(x + 1, y - 1, cw - 2, 9, 1); txt(x + 3, y, fit(vs, cw - 6), 0); }
    else txtHL(x + 2, y, label, cw - 3, 1);
    const cx = x + 10, cy = y + 17, r = 8, P = (q, t) => set_pixel(Math.round(cx + q * Math.cos(t)), Math.round(cy + q * Math.sin(t)), 1);
    for (let a = 135; a <= 405; a += 15) P(r, a * Math.PI / 180);
    const end = 135 + 270 * clamp(f, 0, 1);
    for (let a = 135; a <= end + 0.01; a += 3) { const t = a * Math.PI / 180; P(r, t); P(r - 1, t); }
    { const t = end * Math.PI / 180; for (let q = 2; q <= r + 1; q += 0.5) P(q, t); const tx = Math.round(cx + (r + 1) * Math.cos(t)), ty = Math.round(cy + (r + 1) * Math.sin(t)); fill_rect(tx - 1, ty - 1, 2, 2, 1); }
    if (!hotE && tw(vs) <= cw - 20) txtB(x + 20, y + 13, vs, 1);
}
function bmp(x, y, rows, c) { c = c === undefined ? 1 : c; if (NH && c === 0) NH.note('b', x, y, rows); for (let r = 0; r < rows.length; r++) for (let q = 0; q < rows[r].length; q++) if (rows[r][q] === '#') set_pixel(x + q, y + r, c); }
const TH_GLYPH = {
    3: ['...##..', '..####.', '.######', '.######', '..####.', '...#...', '...#...'],          /* leaf-ish bud on a stem */
    4: ['...#...', '...#...', '#######', '.#####.', '..###..', '.##.##.', '.#...#.'],            /* star */
    5: ['#.....#', '.#...#.', '..#.#..', '...#...', '..#.#..', '.#...#.', '#.....#'],            /* cross */
    2: ['#......', '##.....', '###....', '####...', '###....', '##.....', '#......'],             /* play arrow */
    6: ['...#...', '...#...', '..###..', '#######', '..###..', '...#...', '...#...'],            /* sparkle */
    7: ['.#...#.', '..#.#..', '.#####.', '##.#.##', '#######', '#.###.#', '#.#.#.#'],            /* invader */
    8: ['.#####.', '#.#.#.#', '#..#..#', '#.#.#.#', '#..#...', '.#####.', '.......']             /* gauge */
};
const ICONS = {
    vol: ['...#...', '..##.#.', '###..#.', '###..#.', '###..#.', '..##.#.', '...#...'],
    pan: ['.......', '#.....#', '##...##', '#######', '##...##', '#.....#', '.......'],
    tune: ['...###.', '...#..#', '...#...', '...#...', '.###...', '####...', '.##....'],
    decay: ['#......', '.#.....', '..#....', '...##..', '.....##', '.......', '.......'],
    att: ['......#', '.....#.', '....#..', '..##...', '##.....', '.......', '.......'],
    tone: ['...#...', '#..#..#', '.#####.', '.#####.', '.#####.', '#..#..#', '...#...'],
    drive: ['....##.', '...##..', '..##...', '..####.', '...##..', '..##...', '.##....'],
    cut: ['#####..', '.....#.', '......#', '.......', '.......', '.......', '.......'],
    locut: ['.......', '.......', '.......', '......#', '.....#.', '#####..', '.......'],
    res: ['...#...', '..#.#..', '..#.#..', '.#...#.', '#.....#', '.......', '.......'],
    dly: ['#......', '#..#...', '#..#.#.', '#..#.#.', '#..#.#.', '.......', '.......'],
    rvb: ['...###.', '..#...#', '.#..#..', '#..#...', '.##....', '.......', '.......'],
    arp: ['......#', '....#.#', '..#.#.#', '#.#.#.#', '.......', '.......', '.......'],
    rate: ['.#####.', '#..#..#', '#..#..#', '#..###.', '#.....#', '.#####.', '.......'],
    poly: ['#######', '.......', '#####..', '.......', '###....', '.......', '.......'],
    type: ['.##....', '#..#...', '...#..#', '....##.', '.......', '.......', '.......'],
    wave: ['.##....', '#..#...', '...#..#', '....##.', '.......', '.......', '.......'],
    duck: ['#####.#', '#####.#', '#####.#', '#.....#', '##...##', '.......', '.......'],
    crush: ['....###', '....#..', '..###..', '..#....', '###....', '.......', '.......'],
    ring: ['.##.##.', '#..#..#', '#..#..#', '.##.##.', '.......', '.......', '.......'],
    pump: ['#.#.#.#', '#.#.#.#', '#######', '#######', '.......', '.......', '.......'],
    wob: ['.#...#.', '#.#.#.#', '#.#.#.#', '..#.#..', '.......', '.......', '.......'],
    swing: ['#.....#', '.#...#.', '..#.#..', '...#...', '.......', '.......', '.......'],
    bpm: ['...#...', '..###..', '..###..', '.#####.', '.#####.', '#######', '.......'],
    fenv: ['...#...', '..#.#..', '.#...#.', '#.....#', '.......', '.......', '.......']
};
const ICON_DEF = ['.#####.', '#.....#', '#..#..#', '#..#..#', '#.....#', '.#####.', '.......'];
function thLine(y) {                                           /* the line under the header (and in the menus), one look per theme */
    if ((uiTheme === 3 || uiTheme === 4 || uiTheme >= 6) && view === 0 && y < 12) fill_rect(0, y, 128, 1, 1);      /* the sequencer page writes right under the header: plain line */
    else if (uiTheme === 3) { for (let x = 0; x < 128; x++) { set_pixel(x, y + Math.round(Math.sin(x * 0.45)), 1); } }
    else if (uiTheme === 4) { const pr = [0, 1, 2, 2, 2, 2, 1, 0]; for (let x = 0; x < 128; x++) set_pixel(x, y + pr[x & 7] - 1, 1); }
    else if (uiTheme === 6) { for (let x = 0; x < 128; x += 2) set_pixel(x, y, 1); for (let x = 0; x < 128; x++) { const h = (x * 37 + 11) % 17; if (h === 0) set_pixel(x, y - 1, 1); else if (h === 7) set_pixel(x, y + 1, 1); else if (h === 12 && y > 12) set_pixel(x, y - 2, 1); } }
    else if (uiTheme === 7) { for (let x = 0; x < 128; x++) set_pixel(x, y - ((x >> 2) & 1), 1); }
    else if (uiTheme === 8) { fill_rect(0, y, 128, 1, 1); for (let x = 0; x < 128; x += 8) fill_rect(x, y + 1, 1, (x & 31) === 0 ? 2 : 1, 1); }
    else if (uiTheme === 5) { for (let x = 0; x < 128; x++) if ((x & 3) !== 3) set_pixel(x, y, 1); fill_rect(0, y - 1, 1, 3, 1); fill_rect(127, y - 1, 1, 3, 1); }
    else fill_rect(0, y, 128, 1, 1);
}

function iconFor(key) { return ICONS[key] || ICON_DEF; }
function cellSym(x, y, label, value, f, cw, hotE, key) {         /* symbols: name, value, then a small picture next to the bar */
    txtHL(x + 2, y, label, cw - 3, 1);
    const vs = fit(String(value), cw - 5);
    if (hotE) { fill_rect(x + 1, y + 8, cw - 2, 9, 1); txtB(x + 3, y + 8, vs, 0); } else txtB(x + 3, y + 8, vs, 1);
    bmp(x + 2, y + 17, iconFor(key), 1);
    const bx = x + 11, bw = cw - 14; fill_rect(bx, y + 21, bw, 1, 1); const w = Math.round(bw * clamp(f, 0, 1)); if (w > 0) fill_rect(bx, y + 20, w, 3, 1);
}
function cellNature(x, y, label, value, f, cw, hotE) {            /* nature: the value grows like grass */
    txtHL(x + 2, y, label, cw - 3, 1);
    const vs = fit(String(value), cw - 5);
    if (hotE) { fill_rect(x + 1, y + 8, cw - 2, 9, 1); txtB(x + 3, y + 8, vs, 0); } else txtB(x + 3, y + 8, vs, 1);
    const base = y + 22, w = Math.round((cw - 5) * clamp(f, 0, 1));
    for (let i = 0; i < cw - 5; i++) { if (i < w) { const h = 2 + ((i * 7 + 3) % 4); fill_rect(x + 2 + i, base - h, 1, h + 1, 1); } else if ((i & 1) === 0) set_pixel(x + 2 + i, base, 1); }
}
function cellKids(x, y, label, value, f, cw, hotE) {              /* kids: chunky rounded box, big dots instead of a bar */
    draw_rect(x + 1, y, cw - 2, 24, 1); draw_rect(x + 2, y + 1, cw - 4, 22, 1);
    for (const [cx, cy] of [[x + 1, y], [x + cw - 2, y], [x + 1, y + 23], [x + cw - 2, y + 23]]) set_pixel(cx, cy, 0);
    if (hotE) fill_rect(x + 3, y + 2, cw - 6, 20, 1);
    const c = hotE ? 0 : 1;
    txtHL(x + 4, y + 3, label, cw - 7, c); txtB(x + 4, y + 11, fit(String(value), cw - 7), c);
    const n = Math.max(4, Math.floor((cw - 8) / 4)), on = Math.round(clamp(f, 0, 1) * n);
    for (let i = 0; i < n; i++) { if (i < on) fill_rect(x + 4 + i * 4, y + 19, 3, 2, c); else set_pixel(x + 5 + i * 4, y + 20, c); }
}
function cellTech(x, y, label, value, f, cw, hotE) {              /* tech: corner marks, a segmented bar */
    fill_rect(x, y, 3, 1, 1); fill_rect(x, y, 1, 3, 1); fill_rect(x + cw - 4, y, 3, 1, 1); fill_rect(x + cw - 2, y, 1, 3, 1);
    fill_rect(x, y + 23, 3, 1, 1); fill_rect(x, y + 21, 1, 3, 1); fill_rect(x + cw - 4, y + 23, 3, 1, 1); fill_rect(x + cw - 2, y + 21, 1, 3, 1);
    txtHL(x + 3, y + 2, label, cw - 6, 1);
    const vs = fit(String(value), cw - 7);
    if (hotE) { fill_rect(x + 2, y + 10, cw - 5, 9, 1); txtB(x + 3, y + 10, vs, 0); } else txtB(x + 3, y + 10, vs, 1);
    const n = Math.floor((cw - 7) / 4), on = Math.round(clamp(f, 0, 1) * n);
    for (let i = 0; i < n; i++) { if (i < on) fill_rect(x + 3 + i * 4, y + 20, 3, 2, 1); else fill_rect(x + 3 + i * 4, y + 21, 3, 1, 1); }
}
function cellSpace(x, y, label, value, f, cw, hotE) {              /* space: the value is a planet on its orbit, a few stars around */
    txtHL(x + 2, y, label, cw - 3, 1);
    const vs = fit(String(value), cw - 5), vw = tw(vs);
    if (hotE) { fill_rect(x + 1, y + 8, cw - 2, 9, 1); txtB(x + 3, y + 9, vs, 0); } else txtB(x + 3, y + 9, vs, 1);
    for (let i = 0; i < 4; i++) { const sx = x + 5 + vw + ((i * 11 + x) % Math.max(4, cw - 8 - vw)), sy = y + 9 + ((i * 5 + y) % 8); if (!hotE && sx < x + cw - 2) set_pixel(sx, sy, 1); }
    for (let i = 0; i < cw - 5; i += 2) set_pixel(x + 2 + i, y + 20, 1);
    const px = x + 2 + Math.round((cw - 9) * clamp(f, 0, 1));
    bmp(px, y + 18, ['.###.', '#####', '#####', '#####', '.###.'], 1);
    set_pixel(px + 1, y + 19, 0);
}
function cellRetro(x, y, label, value, f, cw, hotE) {              /* retro: notched arcade box, a bar of blocks */
    draw_rect(x + 1, y, cw - 2, 24, 1);
    for (const [cx, cy] of [[x + 1, y], [x + cw - 2, y], [x + 1, y + 23], [x + cw - 2, y + 23]]) set_pixel(cx, cy, 0);
    if (hotE) fill_rect(x + 2, y + 1, cw - 4, 22, 1);
    const c = hotE ? 0 : 1;
    txtHL(x + 3, y + 2, label, cw - 5, c); txtB(x + 4, y + 10, fit(String(value), cw - 8), c);
    const n = Math.floor((cw - 8) / 5), on = Math.round(clamp(f, 0, 1) * n);
    for (let i = 0; i < n; i++) { if (i < on) fill_rect(x + 4 + i * 5, y + 18, 4, 3, c); else draw_rect(x + 4 + i * 5, y + 18, 4, 3, c); }
}
function drawPresetBox(x, y, cw, name) {                           /* the first page: a small window with the preset name instead of the TYPE knob (turn = list, as before) */
    const w = cw - 2, bx = x + 1, by = y, bh = 25, per = Math.max(3, Math.floor((w - 3) / 4)), lines = [];
    let s = String(name).toUpperCase();
    while (s.length && lines.length < 3) {
        if (s.length <= per) { lines.push(s); s = ''; break; }
        let cut = per; for (let k = per; k >= 3; k--) { const nx = name.charAt(name.length - s.length + k); if (nx && nx === nx.toUpperCase() && nx !== nx.toLowerCase() && s.charAt(k - 1) !== ' ') { cut = k; break; } }
        lines.push(s.substring(0, cut)); s = s.substring(cut);
    }
    if (s.length) lines[2] = lines[2].substring(0, per);
    fill_rect(bx, by, w, 1, 1); fill_rect(bx, by + bh - 1, w, 1, 1); fill_rect(bx, by, 1, bh, 1); fill_rect(bx + w - 1, by, 1, bh, 1);
    const ty = by + 1 + ((bh - 2 - lines.length * 7 + 2) >> 1);
    for (let k = 0; k < lines.length; k++) txtS(bx + 2, ty + k * 7, lines[k], 1);
}
function cellKnob(x, y, label, value, f, cw, hotE) {              /* KNOBS (standard): a round knob, 270 degrees from bottom-left over the top to bottom-right; name below, the value replaces it while you turn */
    const up = Math.max(0, y + 26 - 64), ly = y + 17 - up, cx = x + (cw >> 1), cy = y + 8 - up, r = 8, v = clamp(f, 0, 1), A0 = 135, SW = 270;   /* up: lift everything when the cell sits at the bottom edge */
    const P = (q, d, c) => set_pixel(Math.round(cx + q * Math.cos(d * Math.PI / 180)), Math.round(cy + q * Math.sin(d * Math.PI / 180)), c);
    for (let d = 0; d <= SW; d += 5) P(r, A0 + d, 1);                                   /* the track: a dotted ring */
    const end = SW * v; for (let d = 0; d <= end; d += 2) { P(r, A0 + d, 1); P(r - 1, A0 + d, 1); }          /* the value: a solid double arc */
    for (let q = 0; q <= r - 1; q += 0.5) { P(q, A0 + end, 1); P(q + 0.25, A0 + end + 4, 1); }                            /* pointer, two pixels wide */
    fill_rect(cx - 1, cy - 1, 2, 2, 1);
    P(r + 1, A0, 1); P(r + 1, A0 + SW, 1);                                                /* end marks of the scale */
    let s = String(hotE ? value : label).toUpperCase();
    if (!hotE && tw(s) > cw - 2) { const n = Math.floor((cw - 2) / 4); if (s.length > n) s = s.substring(0, n); txtS(x + ((cw - s.length * 4 + 1) >> 1), ly + 1, s, 1); return; }
    while (s.length > 1 && tw(s) > cw - 2) s = s.substring(0, s.length - 1);
    txt(x + ((cw - tw(s)) >> 1), ly, s, 1);                  /* no white bar: just white text, the value shows instead of the name while you turn */
}
function cellAnalog(x, y, label, value, f, cw, hotE) {             /* analog: name on top, a half-round gauge, the value under the pivot */
    const c = hotE ? 0 : 1;
    if (hotE) fill_rect(x + 1, y - 1, cw - 2, 8 + 18, 1);
    txtHL(x + 2, y, label, cw - 3, c);
    const vs = fit(String(value), cw - 3), cx = x + (cw >> 1), cy = y + 15, r = 7, P = (q, t) => set_pixel(Math.round(cx + q * Math.cos(t)), Math.round(cy + q * Math.sin(t)), c);
    for (let a = 180; a <= 360; a += 15) P(r, a * Math.PI / 180);
    for (let a = 180; a <= 360; a += 45) { const t = a * Math.PI / 180; P(r + 1, t); }
    const t = (180 + 180 * clamp(f, 0, 1)) * Math.PI / 180; for (let q = 1; q <= r - 1; q += 0.5) P(q, t);
    txtB(cx - (tw(vs) >> 1), y + 16, vs, c);
}
function themedCell(x, y, label, value, f, cw, hotE, key) {
    if (uiTheme === 6) { cellSpace(x, y, label, value, f, cw, hotE); return; }
    if (uiTheme === 7) { cellRetro(x, y, label, value, f, cw, hotE); return; }
    if (uiTheme === 8) { cellAnalog(x, y, label, value, f, cw, hotE); return; }
    if (uiTheme === 9) { cellKnob(x, y, label, value, f, cw, hotE); return; }
    if (uiTheme === 2) cellSym(x, y, label, value, f, cw, hotE, key);
    else if (uiTheme === 3) cellNature(x, y, label, value, f, cw, hotE);
    else if (uiTheme === 4) cellKids(x, y, label, value, f, cw, hotE);
    else if (uiTheme === 5) cellTech(x, y, label, value, f, cw, hotE);
    else cellArc(x, y, label, value, f, cw, hotE);
}
function drawTabsMin() {                                       /* themed tab header: page name, a mark for the position */
    const tabs = trackTabs(), cur = Math.max(0, tabIndex(tabs)), th = uiTheme;
    let nm = TAB_SHORT[tabs[cur]];
    if (tabs[cur] === 1) nm = TYPE_NAME[Math.round(pads[ln()][0])] || nm;           /* first page: the title is the preset that is loaded right now */
    const num = String(cur + 1), nw = num.length * 4 + 3, pg = tabPg(tabs[cur]), pt = pg[0] + '/' + pg[1], pw = pt.length * 4;
    nm = hfFit(th === 5 ? '[' + nm + ']' : nm, 126 - (tabs.length * 6 - 3) - 6 - nw - pw - 3);
    txtS(2, 2, num, 1); txtH(2 + nw, 1, nm, 1); txtS(126 - (tabs.length * 6 - 3) - 4 - pw, 2, pt, 1);
    const n = tabs.length; let x = 126 - (n * 6 - 3);
    for (let i = 0; i < n; i++) {
        const X = x + i * 6;
        if (i === cur) {
            if (th === 4) bmp(X - 1, 1, ['..#..', '..#..', '#####', '.###.', '.#.#.'], 1);
            else if (th === 3) bmp(X - 1, 2, ['..#..', '.###.', '#####', '.###.', '..#..'], 1);
            else if (th === 5) fill_rect(X - 1, 2, 5, 5, 1);
            else if (th === 6) bmp(X - 1, 2, ['..#..', '..#..', '#####', '..#..', '..#..'], 1);
            else if (th === 7) { fill_rect(X - 1, 2, 5, 5, 1); set_pixel(X + 1, 4, 0); }
            else if (th === 8) bmp(X - 1, 2, ['..#..', '.###.', '#####', '..#..', '..#..'], 1);
            else fill_rect(X, 3, 3, 3, 1);
        } else if (th === 5 || th === 8) fill_rect(X + 1, 3, 1, 3, 1);
        else if (th === 7) fill_rect(X, 4, 2, 2, 1);
        else if (th === 4 || th === 3) { set_pixel(X + 1, 4, 1); set_pixel(X + 1, 5, 1); }
        else set_pixel(X + 1, 4, 1);
    }
    thLine(10);
}
const VIEW_BADGE = {};
let hdrDrawn = false;
function titleBar(left, right) {                            /* black header, white text, a thin line below */
    hdrDrawn = true;
    if (uiMin) {
        if (tabStrip) { drawTabsMin(); return; }
        const pv = padView(view) || view === 36 || view === 21, th = uiTheme, ic = (pv || th >= 3) ? 11 : 0;
        if (pv) drawKindIcon(2, 1, !!kind[sel], !!mutes[sel]); else if (th >= 2 && TH_GLYPH[th]) bmp(2, 1, TH_GLYPH[th], 1);
        let l2 = String(left).replace(/^\d\d /, ''); if (th === 5) l2 = '[' + l2 + ']';
        txtH(2 + ic, 1, hfFit(l2, 124 - ic - hfW(right) - 8), 1); txtH(126 - hfW(right), 1, right, 1);
        thLine(10);
        if (cpuAvg >= 75 && (ledAge & 32)) { fill_rect(84, 0, 44, 9, 1); txt(87, 1, 'CPU ' + cpuAvg + '!', 0); }
        return;
    }
    if (tabStrip) { drawTabs(); return; }
    const bg = VIEW_BADGE[view];
    if (bg) { fill_rect(0, 0, 24, 9, 1); drawGlyphAt(bg, 3, 0, 0); txt(27, 1, fit(left, 124 - 27 - tw(right) - 4), 1); }       /* white badge with the page's symbol */
    else txtH(2, 1, hfFit(left, 124 - hfW(right) - 8), 1);
    txtH(126 - hfW(right), 1, right, 1);
    fill_rect(0, 9, 128, 2, 1);
    if (cpuAvg >= 75 && (ledAge & 32)) { fill_rect(84, 0, 44, 9, 1); txt(87, 1, 'CPU ' + cpuAvg + '!', 0); }      /* load warning: blinks in every header while the engine is close to its limit */
}
let mqS = '', mqT0 = 0;
function mq(s, n) {                                          /* a long text on the chosen line scrolls sideways (short pause, then slowly to the end and back) */
    s = String(s); if (s.length <= n) return s;
    const now = Date.now(); if (s !== mqS) { mqS = s; mqT0 = now; }
    const span = s.length - n, t = now - mqT0 - 900, per = span * 260 + 1800;
    needsRedraw = true;
    if (t < 0) return s.substring(0, n);
    const p = t % per, off = Math.min(span, Math.floor(p / 260));
    return s.substring(off, off + n);
}
function fit(s, w) { s = tr(String(s)); while (s.length > 1 && tw(s) > w) s = s.substring(0, s.length - 1); return s; }
/* switches (types, modes, mono / stereo ...) are not knobs: a window with the chosen name and one dot per choice. Only continuous values keep the round knob / bar. */
function selInfo(key, v) {                                  /* [number of choices, chosen index] for a switch, null for a knob */
    if (key === 'type') return null;
    const E = ENUMS[key] || (key === 'x2fx' ? ENUMS.xfx : (key === 'm2fx' ? ENUMS.mfx : null));
    if (E) return [E.length, clamp(Math.round(v), 0, E.length - 1)];
    switch (key) {
        case 'kind': case 'fullvel': case 'repmode': case 'favo': return [2, v > 0.5 ? 1 : 0];
        case 'pmode': return [4, clamp(Math.round(v * 3), 0, 3)];
        case 'scs': return [3, clamp(Math.round(v * 2), 0, 2)];
        case 'm_qnt': return [7, Math.min(6, Math.floor(v * 7))];
        case 'qgrid': return [6, qGrid];
        case 'qstr': return [4, qStr];
    }
    return null;
}
function cellSel(x, y, label, value, n, idx, cw, hotE, small) {
    cw = cw || 32; const iw = cw - 4;
    if (small) txtS(x + 2, y + 2, label, 1); else txt(x + 2, y, fit(label, iw), 1);
    const bw = cw - 3;
    draw_rect(x + 1, y + 9, bw, 11, 1); if (hotE) draw_rect(x + 2, y + 10, bw - 2, 9, 1);      /* white text on black with a frame (a double frame while it is being turned) */
    const vt = fit(value, bw - 4);
    txt(x + 1 + Math.floor((bw - tw(vt)) / 2), y + 10, vt, 1);
    const w = bw - 4, x0 = x + 3;
    if (n <= 8) {
        for (let k = 0; k < n; k++) {
            const px = x0 + (n === 1 ? 0 : Math.round(k * (w - 3) / (n - 1)));
            if (k === idx) fill_rect(px, y + 22, 3, 3, 1); else set_pixel(px + 1, y + 23, 1);
        }
    } else {
        fill_rect(x0, y + 23, w, 1, 1);
        const px = x0 + Math.round(idx * (w - 3) / (n - 1)); fill_rect(px, y + 21, 3, 5, 1);
    }
}
function cell(x, y, label, value, f, cw) {
    cw = cw || 32; const iw = cw - 4;
    txt(x + 2, y, fit(label, iw), 1);                    /* label never wider than its cell: no overlap with the neighbour */
    txtB(x + 2, y + 9, fit(value, iw - 1), 1);
    draw_rect(x + 1, y + 18, cw - 3, 5, 1);
    const w = Math.round((cw - 7) * clamp(f, 0, 1)); if (w > 0) fill_rect(x + 3, y + 20, w, 1, 1);
    if (w > 0) fill_rect(x + 3, y + 19, w, 3, 1);
}
/* ---- page looks: every kind of page has its own style so they are not all alike ---- */
const PAGE_STYLE = { 41: 3, 43: 3 };   /* 0 outline (default), 1 inverted tile, 2 list, 3 vertical fader, 4 thick bar */
function cellSmall(x, y, label, value, f, cw, hot) {          /* MIDI FX: small complete names above the value */
    const iw = cw - 4, c = hot ? 0 : 1; if (hot) fill_rect(x + 1, y, cw - 2, 24, 1);
    txtS(x + 3, y + 2, label, c); txtB(x + 3, y + 9, fit(value, iw - 1), c);
    fill_rect(x + 3, y + 19, cw - 6, 1, c); const w = Math.round((cw - 6) * clamp(f, 0, 1)); if (w > 0) fill_rect(x + 3, y + 18, w, 3, c);
}
function cellInv(x, y, label, value, f, cw) {                /* white tile, black text, black bar */
    const iw = cw - 6; fill_rect(x + 1, y, cw - 2, 24, 1);
    txt(x + 3, y + 1, fit(label, iw), 0); txtB(x + 3, y + 10, fit(value, iw - 1), 0);
    fill_rect(x + 3, y + 19, cw - 6, 1, 0); const w = Math.round((cw - 6) * clamp(f, 0, 1)); if (w > 0) fill_rect(x + 3, y + 18, w, 3, 0);
}
function cellThick(x, y, label, value, f, cw) {              /* label, solid thick bar, value underneath */
    const iw = cw - 4; txt(x + 2, y, fit(label, iw), 1);
    draw_rect(x + 1, y + 8, cw - 3, 7, 1); const w = Math.round((cw - 7) * clamp(f, 0, 1)); if (w > 0) fill_rect(x + 3, y + 10, w, 3, 1);
    txtB(x + 2, y + 15, fit(value, iw - 1), 1);
}
function cellList(x, y, label, value, f, cw, rh, inv) {      /* one text line "label ..... value" with a thin bar below; inv = lit up */
    const iw = cw - 6, vs = fit(value, Math.floor(iw / 2)), c = inv ? 0 : 1; if (inv) fill_rect(x + 1, y - 2, cw - 2, 14, 1);
    txt(x + 3, y, fit(label, iw - tw(vs) - 4), c); txtB(x + cw - 4 - tw(vs), y, vs, c);
    fill_rect(x + 3, y + 9, cw - 6, 1, c); const w = Math.round((cw - 6) * clamp(f, 0, 1)); if (w > 0) fill_rect(x + 3, y + 8, w, 3, c);
}
function cellVert(x, y, label, value, f, cw) {               /* fader: value on top, vertical bar, label at the bottom */
    const vs = fit(value, cw - 3); txtB(x + Math.floor((cw - tw(vs)) / 2), y, vs, 1);
    const bx = x + Math.floor(cw / 2) - 6, by = y + 10, bh = 30; draw_rect(bx, by, 13, bh, 1);
    const h = Math.round((bh - 4) * clamp(f, 0, 1)); if (h > 0) fill_rect(bx + 2, by + bh - 2 - h, 9, h, 1);
    fill_rect(bx - 3, by + Math.floor(bh / 2), 3, 1, 1); fill_rect(bx + 13, by + Math.floor(bh / 2), 3, 1, 1);      /* centre tick marks */
    const ls = fit(label, cw - 2); txt(x + Math.floor((cw - tw(ls)) / 2), by + bh + 3, ls, 1);
}
/* ---- EQ: the real frequency response of the three bands (low shelf 100 Hz, bell 1 kHz, high shelf 8 kHz) ---- */
function eqCoef(band, gdb) {                                 /* the same RBJ biquads as the engine, 44.1 kHz */
    const f = band === 0 ? 100 : (band === 1 ? 1000 : 8000), A = Math.pow(10, gdb / 40), w0 = 2 * Math.PI * f / 44100, cw = Math.cos(w0), sw = Math.sin(w0);
    let b0, b1, b2, a0, a1, a2;
    if (band === 1) { const al = sw / (2 * 0.9); b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; }
    else {
        const al = sw * Math.SQRT1_2, sa = 2 * Math.sqrt(A) * al;
        if (band === 0) { b0 = A * ((A + 1) - (A - 1) * cw + sa); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sa); a0 = (A + 1) + (A - 1) * cw + sa; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - sa; }
        else { b0 = A * ((A + 1) + (A - 1) * cw + sa); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sa); a0 = (A + 1) - (A - 1) * cw + sa; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sa; }
    }
    return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0];
}
function eqDb(c, f) {
    const w = 2 * Math.PI * f / 44100, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
    const nr = c[0] + c[1] * c1 + c[2] * c2, ni = -(c[1] * s1 + c[2] * s2), dr = 1 + c[3] * c1 + c[4] * c2, di = -(c[3] * s1 + c[4] * s2);
    return 10 * Math.log10(Math.max(1e-9, (nr * nr + ni * ni) / Math.max(1e-9, dr * dr + di * di)));
}
function drawEqGraph(g, x0, y0, W, H, hotBand) {             /* g = [low, mid, high] in dB */
    const co = [0, 1, 2].map(b => eqCoef(b, g[b])), R = 14;
    const yOf = db => y0 + Math.round((R - clamp(db, -R, R)) / (2 * R) * (H - 1)), yz = yOf(0);
    const fx = f => Math.round((W - 1) * Math.log(f / 20) / Math.log(1000));
    for (const f of [100, 1000, 10000]) { const x = x0 + fx(f); for (let j = y0; j < y0 + H; j += 4) set_pixel(x, j, 1); }      /* faint grid: 100 Hz, 1 kHz, 10 kHz */
    for (let i = 0; i < W; i += 2) set_pixel(x0 + i, yz, 1);                                                                       /* 0 dB line */
    let py = yz;
    for (let i = 0; i < W; i++) {
        const f = 20 * Math.pow(1000, i / (W - 1)), db = eqDb(co[0], f) + eqDb(co[1], f) + eqDb(co[2], f), y = yOf(db);
        const a = Math.min(y, yz), b = Math.max(y, yz);
        for (let j = a; j <= b; j++) if (((i + j) & 1) === 0) set_pixel(x0 + i, j, 1);                                            /* hatching between the curve and 0 dB */
        const lo = Math.min(y, py), hi = Math.max(y, py);
        for (let j = lo; j <= hi; j++) set_pixel(x0 + i, j, 1);                                                                    /* the curve (joined, no gaps) */
        set_pixel(x0 + i, Math.min(y0 + H - 1, y + 1), 1); py = y;
    }
    [100, 1000, 8000].forEach((f, b) => {
        const x = x0 + fx(f), db = g[b], y = yOf(eqDb(co[0], f) + eqDb(co[1], f) + eqDb(co[2], f));
        if (hotBand === b) { fill_rect(x - 2, y - 2, 5, 5, 1); fill_rect(x - 1, y - 1, 3, 3, 0); fill_rect(x, y, 1, 1, 1); }
        else if (Math.abs(db) > 0.4) fill_rect(x - 1, y - 1, 3, 3, 1);
    });
}
function eqCellH(x, y, label, value, f, cw, hotE) {        /* bipolar bar: the fill grows from the centre */
    const vs = fit(value, cw - 3), ls = fit(label, cw - 3);
    txt(x + Math.floor((cw - tw(ls)) / 2), y, ls, 1); txtB(x + Math.floor((cw - tw(vs)) / 2), y + 9, vs, 1);
    const bx = x + 3, bw = cw - 7, by = y + 20; draw_rect(bx, by, bw, 5, 1);
    const mid = bx + Math.floor(bw / 2), n = Math.round((bw - 4) / 2 * (clamp(f, 0, 1) - 0.5) * 2);
    if (n > 0) fill_rect(mid, by + 1, n + 1, 3, 1); else if (n < 0) fill_rect(mid + n, by + 1, -n + 1, 3, 1);
    fill_rect(mid, by - 2, 1, 2, 1); fill_rect(mid, by + 5, 1, 2, 1);
    if (hotE) draw_rect(x, y - 2, cw - 1, 31, 1);
}
function cellVertS(x, y, label, value, f, cw, bh) {          /* short fader (master EQ page) */
    const vs = fit(value, cw - 3); txtB(x + Math.floor((cw - tw(vs)) / 2), y, vs, 1);
    const bx = x + Math.floor(cw / 2) - 5, by = y + 10; draw_rect(bx, by, 11, bh, 1);
    const h = Math.round((bh - 4) * clamp(f, 0, 1)); if (h > 0) fill_rect(bx + 2, by + bh - 2 - h, 7, h, 1);
    fill_rect(bx - 2, by + Math.floor(bh / 2), 2, 1, 1); fill_rect(bx + 11, by + Math.floor(bh / 2), 2, 1, 1);
    const ls = fit(label, cw - 2); txt(x + Math.floor((cw - tw(ls)) / 2), by + bh + 2, ls, 1);
}
function drawFilterCurve() {                                 /* the filter's frequency response, drawn from Cutoff, Reso and HPCut */
    const cut = pads[ln()][PI.cut], res = pads[ln()][PI.res], hp = pads[ln()][PI.locut];
    const fc = 30 * Math.pow(16000 / 30, cut), fh = 20 * Math.pow(2000 / 20, hp), Q = 0.7 + res * 7;
    const X0 = 2, W = 124, Y0 = 12, H = 12, md = Math.min(2, Math.round(pads[ln()][PI.fmode] * 2));
    for (let i = 0; i < W; i++) {
        const f = 20 * Math.pow(1000, i / (W - 1));
        const rl = f / fc, a1 = 1 - rl * rl, dn = Math.sqrt(a1 * a1 + (rl / Q) * (rl / Q)), mL = md === 0 ? 1 / dn : (md === 1 ? (rl / Q) / dn * 1.4 : rl * rl / dn);
        let m = mL; if (hp > 0.02) { const rh = fh / f, b1 = 1 - rh * rh; m *= 1 / Math.sqrt(b1 * b1 + (rh / 0.7) * (rh / 0.7)); }
        const db = clamp(20 * Math.log10(Math.max(m, 1e-4)), -24, 12), y = Y0 + Math.round((12 - db) / 36 * (H - 1));
        for (let j = y; j < Y0 + H; j++) if (((i + j) & 1) === 0) set_pixel(X0 + i, j, 1);        /* checker fill under the curve */
        set_pixel(X0 + i, y, 1); set_pixel(X0 + i, Math.min(Y0 + H - 1, y + 1), 1);
    }
    fill_rect(0, Y0 + H, 128, 1, 1);
}
/* ---- sample bank (WAV files in /data/UserData/schwung/taktile-samples) ---- */
let smpInfo = [], smpNames = [], smpWave = {};
function smpRefresh() {
    smpInfo = []; smpNames = []; smpWave = {};
    const n = parseInt(dspGet('smpn')) || 0; smpMem = parseInt(dspGet('smpmem')) || 0;
    for (let i = 0; i < n; i++) {
        const r = dspGet('smp_' + i), a = typeof r === 'string' ? r.split('|') : [];
        smpNames.push(a[0] || ''); smpInfo.push([parseInt(a[1]) || 0, parseInt(a[2]) || 0]);
    }
}
function smpLoaded() { const l = []; for (let i = 0; i < smpInfo.length; i++) if (smpInfo[i][0] > 0) l.push(i); return l; }
function smpIdx(L) { return Math.round(pads[L][PI.smp] * 63); }
function setLaneP(L, key, v) {
    if (!lfxBusy) { const tp = undoStack.length ? undoStack[undoStack.length - 1] : null; if (!(tp && tp.k === 'pad' && tp.L === L && Date.now() - tp.ts < 1500)) pSnap('pad', L, 'set'); else tp.ts = Date.now(); }
    pads[L][PI[key]] = v;
    enqueue('p' + L + key, 'p' + p2(L + 1) + '_' + key, key === 'type' ? String(Math.round(v)) : v.toFixed(3));
    storeCs(L); markEdited();
}
function ensureSampleType() {
    const L = ln();
    if (Math.round(pads[L][0]) !== 359) setLaneP(L, 'type', 359);
    const l = smpLoaded();
    if (l.length && l.indexOf(smpIdx(L)) < 0) setLaneP(L, 'smp', l[0] / 63);
}
function selectSample(d, toast) {
    const L = ln(), l = smpLoaded();
    if (!l.length) { showToast('NO SAMPLES', 'FOLDER taktile-samples', 0); return; }
    if (Math.round(pads[L][0]) !== 359) setLaneP(L, 'type', 359);
    let p = l.indexOf(smpIdx(L)); if (p < 0) p = d > 0 ? -1 : l.length;
    p = clamp(p + d, 0, l.length - 1);
    setLaneP(L, 'smp', l[p] / 63); setLaneP(L, 'sst', 0); setLaneP(L, 'sen', 1);
    if (toast !== false) showToast('SAMPLE ' + (l[p] + 1) + '/' + l.length, smpNames[l[p]], p / Math.max(1, l.length - 1));
}
/* ---- file browser (any folder of the Move) ---- */
let broOpen = false, broPath = '/data/UserData', broList = [], broSel = 0;
let broErr = 0;
function broLoad(path) {
    dspSet('brdir', path); let e = String(dspGet('brerr') || '0,0,0').split(',').map(x => parseInt(x) || 0);
    while (e[0] && path !== '/' && path !== '') { path = path.lastIndexOf('/') <= 0 ? '/' : path.substring(0, path.lastIndexOf('/')); showToast(e[0] === 13 ? 'NO ACCESS' : 'FOLDER GONE', 'ONE LEVEL UP', 0); dspSet('brdir', path); e = String(dspGet('brerr') || '0,0,0').split(',').map(x => parseInt(x) || 0); }
    broErr = e[0]; if (!broErr && (e[1] || e[2])) showToast('FOLDER', '+' + (e[1] + e[2]) + ' NOT SHOWN (LONG NAME/LIMIT)', 0);
    const p = dspGet('brpath'); broPath = typeof p === 'string' && p ? p : path;
    const n = parseInt(dspGet('brn')) || 0; broList = [['d', '..']];
    for (let i = 0; i < n; i++) { const r = dspGet('br_' + i); if (typeof r === 'string') { const k = r.indexOf('|'); broList.push([r.substring(0, k), r.substring(k + 1), i]); } }
    broSel = 0; needsRedraw = true;
}
function broOpenNow() { broOpen = true; broLoad(broPath); showToast('BROWSE', 'PLAY = LISTEN', 0.5); }
function broPreview() {
    const e = broList[broSel]; if (!e || e[0] === 'd') { showToast('LISTEN', 'SELECT A WAV', 0); return; }
    dspSet('brprev', String(e[2])); const r = parseInt(dspGet('brres'));
    showToast(r === 1 ? 'LISTENING' : 'TOO BIG', r === 1 ? e[1] : 'MAX 12 MB (LOAD WORKS)', r === 1 ? 1 : 0);
}
function broUp() { const i = broPath.lastIndexOf('/'); return i <= 0 ? '/' : broPath.substring(0, i); }
function broFolderKit(path) {                              /* Shift + click on a folder: its first 16 WAV files become the 16 pads of the drum track */
    if (!kind[sel]) { showToast('FOLDER KIT', 'DRUM TRACK ONLY', 0); return; }
    broLoad(path); const files = broList.filter(x => x[0] === 'f').slice(0, 16);
    if (!files.length) { showToast('FOLDER KIT', 'NO WAV FILES', 0); return; }
    let n = 0; dspSet('brstop', '1');
    for (let k = 0; k < files.length; k++) {
        dspSet('brimport', String(files[k][2])); const r = parseInt(dspGet('brres')); if (r < 0) continue;
        const l = laneOf(sel, k);
        setLaneP(l, 'type', 359); setLaneP(l, 'smp', r / 63); setLaneP(l, 'sst', 0); setLaneP(l, 'sen', 1); setLaneP(l, 'smd', 0); n++;
    }
    smpRefresh(); broOpen = false; showToast('FOLDER KIT', n + ' PADS' + (smpMem >= 85 ? ' MEM ' + smpMem + '%' : ''), n / 16); needsRedraw = true;
}
function broClick() {
    const e = broList[broSel]; if (!e) return;
    if (e[0] === 'd' && shiftHeld && e[1] !== '..') { broFolderKit((broPath === '/' ? '' : broPath) + '/' + e[1]); return; }
    if (e[0] === 'd') { broLoad(e[1] === '..' ? broUp() : (broPath === '/' ? '' : broPath) + '/' + e[1]); return; }
    showToast('LOADING', e[1], 0.5);
    dspSet('brstop', '1'); dspSet('brimport', String(e[2])); const r = parseInt(dspGet('brres'));
    if (r >= 0) {
        smpRefresh(); const L = ln();
        setLaneP(L, 'type', 359); setLaneP(L, 'smp', r / 63); setLaneP(L, 'sst', 0); setLaneP(L, 'sen', 1);
        broOpen = false; showToast('SAMPLE ' + (r + 1), smpNames[r], 1); if (smpMem >= 85) showToast('MEMORY ' + smpMem + '%', 'BANK ALMOST FULL', 1);
    } else showToast('FAILED', smpMem >= 95 ? 'MEMORY FULL ' + smpMem + '%' : 'WAV 16/24/32-BIT <40MB', 0);
    needsRedraw = true;
}
function drawBro() {
    titleBar('BROWSE', fit(broPath, 90));
    const top = clamp(broSel - 2, 0, Math.max(0, broList.length - 5));
    for (let k = 0; k < 5; k++) {
        const i = top + k; if (i >= broList.length) break;
        const e = broList[i], y = 11 + k * 10, on = i === broSel;
        if (on) fill_rect(0, y - 1, 128, 10, 1);
        txt(2, y, fit((e[0] === 'd' ? (e[1] === '..' ? '< back' : '/' + e[1]) : e[1]), 122), on ? 0 : 1);
    }
    if (broList.length <= 1) txt(2, 30, broErr ? (broErr === 13 ? 'NO ACCESS' : 'CANNOT OPEN') : '(no WAV files)', 1);
}
function sampleRescan() { dspSet('smprescan', '1'); smpRefresh(); const n = smpLoaded().length; showToast('SAMPLES', n + ' FOUND', n / 64); needsRedraw = true; }
function sampleSlice() {                                   /* Shift + jog click: drum track = 16 slices over the 16 pads, synth track = whole sample */
    const L0 = ln(), i = smpIdx(L0);
    if (!smpInfo[i] || !smpInfo[i][0]) { showToast('NO SAMPLE', 'SELECT FIRST', 0); return; }
    if (kind[sel]) {
        for (let k = 0; k < 16; k++) { const l = laneOf(sel, k); setLaneP(l, 'type', 359); setLaneP(l, 'smp', i / 63); setLaneP(l, 'sst', k / 16); setLaneP(l, 'sen', (k + 1) / 16); setLaneP(l, 'smd', 0); }
        showToast('SLICED', '16 PADS', 1);
    } else { setLaneP(L0, 'sst', 0); setLaneP(L0, 'sen', 1); showToast('SAMPLE', 'FULL', 1); }
    needsRedraw = true;
}
/* ---- own slices: up to 16 start markers (0..1); Jog click applies them to the drum pads ---- */
/* ---- resampling: record the master (or one track) for N bars into a new sample ---- */
const RS_BARS = [1, 2, 4, 8, 16];
let rsSrc = 0, rsBarI = 1, rsState = 0, rsPct = 0, rsPoll = 0, rsLane = 0;
function rsArm() {
    if (rsState) return;
    dspSet('recarm', (rsSrc - 1) + ',' + RS_BARS[rsBarI]);
    if (!playing) { dspSet('play', '1'); playing = true; gs = 0; }
    rsState = 1; rsPct = 0; rsLane = ln(); needsRedraw = true; showToast('RESAMPLE', 'WAITING FOR BAR 1', 0);
}
function rsCancel() { if (!rsState) return; dspSet('reccancel', '1'); rsState = 0; rsPct = 0; showToast('RESAMPLE', 'CANCELLED', 0); needsRedraw = true; }
function rsTick() {
    if (!rsState || ++rsPoll % 5) return;
    const r = dspGet('recstate'); if (typeof r !== 'string') return;
    const a = r.split(',').map(Number), st = a[0] | 0;
    const np = a[1] | 0; if (np !== rsPct) { rsPct = np; needsRedraw = true; }
    if (st === 2 && rsState !== 2) { rsState = 2; needsRedraw = true; }
    if (st === 3) {
        dspSet('recsave', '1');
        const r2 = dspGet('recstate'), slotN = typeof r2 === 'string' ? (parseInt(r2.split(',')[2]) | 0) : -1;
        rsState = 0; smpRefresh();
        if (slotN >= 0) { const L = rsLane; setLaneP(L, 'type', 359); setLaneP(L, 'smp', slotN / 63); setLaneP(L, 'sst', 0); setLaneP(L, 'sen', 1); setLaneP(L, 'smd', 0); showToast('RESAMPLED', 'SAMPLE ' + (slotN + 1) + ' ON PAD', 1); }
        else showToast('RESAMPLE', 'SAVE FAILED', 0);
        needsRedraw = true;
    } else if (st === 0 && rsState) { rsState = 0; needsRedraw = true; }
}
/* ---- kits: sounds of a drum track (16 pads) or a synth sound, stored in slots ---- */
const KIT_FILE = '/data/UserData/schwung/taktile-kits.json';
let kitSlot = 0, kits = null;
function kitsLoad() { if (kits) return; kits = {}; try { const t = host_read_file(KIT_FILE); if (t) { const o = JSON.parse(t); if (o && typeof o === 'object') kits = o; } } catch (e) { kits = {}; } }
function kitsSave() { try { host_write_file(KIT_FILE, JSON.stringify(kits)); } catch (e) { /* ignore */ } }
function soundIdx(i) { return i < 24 || (i >= 41 && i <= 44) || (i >= 47 && i < 61); }
const FACTORY = [
    ['TECHNO', 'KckTch1 RumbKick SnrTch1 ClpDry1 HatCl01 HatOp01 RimTk1 TomSet1 ShkGrv1 CrsCym1 RdeCym1 TomSet2 ZapSet1 FMPerc1 CowSet1 GlchX01'.split(' ')],
    ['HOUSE', 'KckHse1 Kick707 SnrSnap ClpRoom1 HatCl05 HatOp05 RimWood TomMid ShakeMed CrsCym3 RdeCym3 TomFloor CngSet1 BngSet1 CowSet2 FMclick'.split(' ')],
    ['TRANCE', 'KckTrn1 KckTrn3 Snr909 ClapLong HatCl06 HatOp06 RimClick TomRack ShakeLng Splash RideBell TomSynth CongaHi CongaLo CowLong ZapRise'.split(' ')],
    ['PSY', 'KckPsy1 RumbPsy SnrDeep ClapTrap HatCl08 HatOp08 RimLong FMtom ShakeSft CrashLng RideShrt TomLo ZapSet3 LaserX1 ZapLo GlchX03'.split(' ')],
    ['DUB', 'KckDub1 KickDub SnrLof1 ClpRoom4 HatSoft HatHalf RimTk4 TomFloor ShakeSft RideSoft AcRideSft TomDeep CongaLo BongoLo CowShort Sub808Mid'.split(' ')],
    ['TRAP', '808Kck1 Sub808Hrt SnrTrap ClpTrp1 HatTrap HatShort RimTick TomSynth ShakeSht HatSizzl HatTick Sub808Hi PercHi FMBlip ZapHi Sub808Lo'.split(' ')],
    ['GABBA', 'KickGabba KckHrd3 SnrBig3 ClapSnap HatCl09 HatSizzl RimHi TomPunch ShakTght CrashBig CrsCym5 TomHigh PercLo FMClang ZapSet5 ImpactHi'.split(' ')],
    ['LOFI', 'KickSoft AcKickLof SnrLoFi ClapRoom HatDust AcHatSft RimBox TomFat ShakLoos AcCrshDry AcRideTnk TomModMd CongaSlp BongoHi CowMute FMTick'.split(' ')],
    ['ELECTRO', 'Kick909 KickSnap SnrCrck ClapStk HatTick HatOp909 RimDry FMtom ShakeSht CrashShr RideShrt FMmetal FMclick FMblip Cowbell ZapLaser'.split(' ')],
    ['ACID', 'KickAcid KckTch5 Snr909 ClpDry5 HatClsd HatOp02 RimClick TomLo ShkGrv3 CrsCym4 RdeCym4 TomHi CngSet2 ZapDive CowSet3 LaserX2'.split(' ')],
    ['MINIMAL', 'KickTight KickClick SnrDry ClapShrt HatTight HatSoft RimTk6 TomMid ShakTght CrashDry RideTink TomSet6 CongaMid BongoHi CowMute GlchX05'.split(' ')],
    ['DNB', 'KickPunch Kick90s SnrTch6 ClapHigh HatSnap HatLoose RimTk5 TomRack ShkGrv4 CrsCym6 RdeCym6 TomHigh ZapDown ImpHit2 PercHi LaserX4'.split(' ')],
    ['HIPHOP', 'KickWarm Kick90s SnrFat AcClapRm HatCl03 AcHatSft RimBox TomFat ShakeSft AcCrash AcRideSft TomFloor CongaSlp BongoLo AcCowLo SweepShrt'.split(' ')],
    ['INDUST', 'KickHard KickMax SnrBig1 ClapWide HatCl10 HatOpen RimDry TomDeep ShakLoos CrashBig RideWash TomSet8 FMClang MetalHit Anvil ImpactBig'.split(' ')],
    ['808', '808Boom 808Tight SnrTop ClapLong HatShort HatHalf RimClick TomLo ShakeSft CrashShr RideShrt TomHi CongaOpn CongaLo DCowbell 808Pop'.split(' ')],
    ['RAVE', 'KickRave KckTrn5 SnrBig4 ClpDry4 HatOp09 HatSizzl RimTk3 TomSet7 ShakeLng HatOp10 RideBell TomMid ZapBeam ZapRise CowRing ImpactX3'.split(' ')],
    ['WORLD', 'Taiko TimpLo TablaLo TablaHi AcShaker AcShkLng Bongo TomModLo AcCabasa GamelanL AcTamb TomModMd CongaMid CongaHi AcWoodHi Clave'.split(' ')],
    ['TABLA', 'TimpLo TomModLo TablaLo TablaHi AcShaker AcShkLng Bongo TomModMd AcCabasa BowlLo AcTamb TomModHi AcWoodLo AcWoodHi Clave GlassTap'.split(' ')],
    ['ORCHESTRA', 'Taiko TimpLo TimpHi TomModHi AcTamb ShakeMed Anvil TomModLo AcTriang PlateLo ShakeSft TomModMd TnGlock TnBell SteelHi TnVibes'.split(' ')],
    ['FOUND', 'Taiko TomModLo TinCan PipeHit Clave WoodHi Anvil TomModMd MetalHit PlateLo WoodLo TomModHi SteelHi GlassTap BowlHi Crystal'.split(' ')],
    ['ORGANIC', 'TimpLo Taiko AcWoodLo AcWoodHi AcEgg AcShaker Clave TomModLo AcCabasa GamelanH AcTamb TomModMd TnMarim TnKalim XyloHi Bongo'.split(' ')],
    ['GAMELAN', 'Taiko TimpLo GlassTap WoodHi ShakeSht ShakeSft Clave TomModMd ShakeMed GamelanL ShakeLng TomModHi GamelanH SteelLo BowlHi PlateHi'.split(' ')],
    ['CHIPKIT', 'ChipKick ChipTom NoiseSnr ChipLaser NoiseHat NoiseOpen NoiseMet ChipTom NoiseHat NoiseMet NoiseOpen ChipTom ChipLaser FmClick FmZap FmHat'.split(' ')],
    ['SYNTHKIT', 'DKick808 DKickHse DSnr909 DClap DHatCl DHatOp DRim DTomMid DHatSoft DHatOp2 DHatTght DTomLo DCowbell DTomHi DClave DCowHi'.split(' ')],
    ['SYNTHHARD', 'DKickHrd DKickTch DSnrCrck DClapTgt DHatTght DHatSizz DRimHi DTomElec DHatPed DHatOp2 DHatCl DTomHi DMetal DCowHi DClave DKickPnc'.split(' ')],
    ['SYNTHSOFT', 'DKickDub DKickSub DSnrFat DClapLng DHatSoft DHatOp DRim DTomLo DHatLoFi DHatOp2 DHatSoft DTomMid DCowbell DClave DTomHi DKickTrp'.split(' ')],
    ['HARDTECH', 'KckHrd1 KckTch4 SnrTch4 ClpDry3 HatCl04 HatOp04 RimTk2 TomSet3 ShkGrv2 CrsCym2 RdeCym2 TomSet4 ZapSet2 PercHi FMPerc3 ImpactX2'.split(' ')],
    ['DUBTECH', 'KckDub2 KckDub4 SnrLof2 ClpRoom3 HatCl07 HatOp07 RimTk5 TomSet5 ShkGrv5 CrsCym5 RdeCym5 TomSet3 CngSet2 FMPerc6 SweepX2 ImpactX5'.split(' ')],
    ['DEEPHOUSE', 'KckHse3 KickWarm SnrLof3 ClpRoom2 HatSoft HatOp03 RimTk6 TomSet6 ShkGrv6 RdeCym5 HatAir TomFloor CngSet3 BngSet2 CowSet4 FMPerc2'.split(' ')],
    ['DISCO', 'KckHse5 KickDry SnrTch2 ClpRoom5 HatCl02 HatOp02 RimTk1 TomMid ShakeMed CrashShr RdeCym1 TomSet1 CngSet4 BngSet4 CowSet1 FMPerc7'.split(' ')],
    ['BREAKS', 'KickPunch KckHse6 SnrBig2 ClpDry6 HatCl10 HatLoose RimBox TomPunch ShkGrv1 AcCrshDry AcRideTnk TomFat CngSet5 BngSet3 CowMute ZapSet4'.split(' ')],
    ['GARAGE', 'KickSnap KckHse2 SnrTch7 ClpTrp2 HatCl09 HatOp09 RimTk4 TomSet7 ShkGrv4 AcSplash RdeCym4 TomSet8 BngSet3 CngSet6 CowSet3 FMPerc4'.split(' ')],
    ['FULLON', 'KckPsy3 KckPsy5 SnrBig5 ClpDry2 HatCl06 HatOp06 RimTk6 TomSet2 ShkGrv1 CrsCym2 RdeCym2 TomSet4 ZapSet5 ZapSet1 FMPerc5 ImpactX4'.split(' ')],
    ['AMBIENT', 'KickDeep KickLow SnrRoom ClpRoom6 HatAir HatSwsh RimLong TomModLo ShakeSft CrsCym6 RideWash TomModMd BowlLo GlassTap Crystal SweepX4'.split(' ')],
    ['AC ROCK', 'AcKickRck AcKickDp AcSnrRock AcSnrFat AcHatCls AcHatOpn AcRim AcTomMid AcHatTgt AcCrash AcHatPdl AcTomFlr AcRide AcCowbell AcTamb AcSplash'.split(' ')],
    ['JAZZ', 'AcKickJzz AcKickSub AcSnrJazz AcSnrBrsh AcHatCls AcHatHlf AcStick AcTomHi AcHatSft AcRideSft AcHatPdl AcTomLo AcRide AcWood AcShaker AcTriMute'.split(' ')],
    ['STUDIO', 'AcKickTgt AcKickPnc AcSnrTght AcClapTgt AcHatTgt AcHatSzl AcRimHi AcTomRck AcHatCls AcCrshDry AcHatPdl AcTomDry AcClick AcCowHi AcShkLng AcSplash'.split(' ')],
    ['AC LOFI', 'AcKickVin AcKickLof AcSnrLoos AcClapRm AcHatSft AcHatWid AcRimLo AcTomDpFl AcHatCls AcRideTnk AcHatHlf AcTomRoto AcEgg AcWoodLo AcCabasa AcTambHit'.split(' ')],
    ['PERC LAB', 'AcWood AcWoodHi AcWoodLo AcCowbell AcCowLo AcCowHi AcTriang AcTriMute AcShaker AcEgg AcCabasa AcTamb AcTambHit AcTomRoto AcStick AcClick'.split(' ')],
    ['CINEMA', 'FxImpact FxBoom FxImpDry FxImpHi FxRiser FxRisHi FxRevCym FxRevSnr FxWhoosh FxFall FxSweepDn FxThundr FxSubDrp FxImpBig FxRisNz FxSweepUp'.split(' ')],
    ['FX PLAY', 'FxLaser FxLaserLo FxZapUp FxBlip FxBubble FxNzHit FxNzSnap FxStatic FxFallNz FxWhshLo FxRisTn FxRisLo FxImpHi FxRevSnr FxNzHit FxBlip'.split(' ')],
    ['TONAL', 'TnMarim TnKalim TnVibes TnGlock TnBell TnSteel TnPluck TnStrPlk TnFmPluk TnFmBell TnBass TnSub TnStab TnFoldPl TnFmBass TnBassDub'.split(' ')],
];
/* the character of the factory kits: per role (K kick, S snare/clap/rim, H hats + cymbals, C crash, T toms, P percussion) the sound parameters */
const KIT_ROLE = ['K', 'K', 'S', 'S', 'H', 'H', 'S', 'T', 'H', 'C', 'H', 'T', 'P', 'P', 'P', 'P'];
const KIT_STYLE = {
    TECHNO:  { K: { tune: .48, decay: .5, tone: .75, drive: .38, rmb: .3 }, S: { decay: .42, tone: .72, drive: .22 }, H: { decay: .36, tone: .82, locut: .25 }, C: { decay: .45, rvb: .15 }, T: { decay: .5, drive: .15 }, P: { rvb: .12, tone: .8 } },
    HOUSE:   { K: { tune: .5, decay: .55, tone: .55, drive: .08 }, S: { decay: .52, tone: .65, rvb: .16 }, H: { decay: .5, tone: .72, locut: .1 }, C: { decay: .6, rvb: .2 }, T: { decay: .55 }, P: { rvb: .1, dly: .08 } },
    TRANCE:  { K: { tune: .5, decay: .62, tone: .7, drive: .22 }, S: { decay: .6, tone: .75, rvb: .22 }, H: { decay: .55, tone: .85 }, C: { decay: .75, rvb: .28 }, T: { decay: .6 }, P: { rvb: .15, dly: .12 } },
    PSY:     { K: { tune: .42, decay: .44, tone: .62, drive: .3, rmb: .25 }, S: { decay: .4, tone: .55, rvb: .14, ring: .06 }, H: { decay: .3, tone: .9, locut: .15 }, C: { decay: .55, rvb: .3 }, T: { tune: .4, decay: .45 }, P: { ring: .12, rvb: .25, dly: .18 } },
    DUB:     { K: { tune: .45, decay: .68, tone: .38 }, S: { decay: .58, tone: .45, rvb: .42, dly: .3 }, H: { decay: .5, tone: .5, dly: .15 }, C: { decay: .7, rvb: .4 }, T: { decay: .62, rvb: .25 }, P: { dly: .38, rvb: .3 } },
    TRAP:    { K: { tune: .38, decay: .88, tone: .45, drive: .15 }, S: { decay: .3, tone: .85, drive: .1 }, H: { decay: .24, tone: .92, locut: .3 }, C: { decay: .4 }, T: { tune: .35, decay: .6 }, P: { decay: .3, tone: .85 } },
    GABBA:   { K: { tune: .58, decay: .5, tone: .8, drive: .85, crush: .12 }, S: { decay: .4, tone: .8, drive: .55 }, H: { decay: .3, drive: .35 }, C: { decay: .5, drive: .3 }, T: { drive: .5 }, P: { drive: .4, crush: .08 } },
    LOFI:    { K: { tune: .46, decay: .6, tone: .34, crush: .22 }, S: { decay: .5, tone: .35, crush: .25, rvb: .1 }, H: { decay: .38, tone: .38, crush: .2, locut: .2 }, C: { decay: .55, tone: .35, crush: .2 }, T: { tone: .35, crush: .2 }, P: { tone: .35, crush: .2, dly: .1 } },
    ELECTRO: { K: { tune: .52, decay: .34, tone: .78, drive: .2 }, S: { decay: .3, tone: .85, crush: .1 }, H: { decay: .28, tone: .9 }, C: { decay: .4 }, T: { decay: .35, tone: .8, ring: .1 }, P: { ring: .18, dly: .1 } },
    ACID:    { K: { tune: .48, decay: .52, tone: .7, drive: .45 }, S: { decay: .45, tone: .75, drive: .2 }, H: { decay: .46, tone: .8 }, C: { decay: .5 }, T: { drive: .25 }, P: { drive: .25, dly: .12 } },
    MINIMAL: { K: { tune: .5, decay: .3, tone: .6 }, S: { decay: .24, tone: .55 }, H: { decay: .2, tone: .72, locut: .3 }, C: { decay: .3 }, T: { decay: .3 }, P: { decay: .28, tone: .6 } },
    DNB:     { K: { tune: .54, decay: .3, tone: .85, drive: .15 }, S: { tune: .56, decay: .36, tone: .9, drive: .3 }, H: { decay: .3, tone: .92, locut: .2 }, C: { decay: .45 }, T: { decay: .35 }, P: { decay: .3, tone: .85 } },
    HIPHOP:  { K: { tune: .44, decay: .56, tone: .45, drive: .1 }, S: { decay: .46, tone: .5, rvb: .12, crush: .08 }, H: { decay: .35, tone: .5, crush: .1 }, C: { decay: .5, tone: .5 }, T: { decay: .5, tone: .5 }, P: { tone: .5, crush: .08, dly: .08 } },
    INDUST:  { K: { tune: .5, decay: .48, tone: .85, drive: .72, crush: .15 }, S: { decay: .45, tone: .9, drive: .65, crush: .2 }, H: { decay: .4, tone: .9, drive: .45, ring: .1 }, C: { decay: .6, drive: .5 }, T: { drive: .55 }, P: { ring: .3, crush: .2, drive: .4 } },
    '808':   { K: { tune: .45, decay: .78, tone: .5 }, S: { decay: .4, tone: .6 }, H: { decay: .3, tone: .7 }, C: { decay: .5 }, T: { decay: .62, tone: .55 }, P: { decay: .4, tone: .6 } },
    WORLD:   { P: { rvb: .12 }, C: { rvb: .2 } },
    TABLA:   { P: { rvb: .1 }, C: { rvb: .25 } },
    ORCHESTRA: { K: { rvb: .2 }, T: { rvb: .22 }, C: { rvb: .3 }, P: { rvb: .25 } },
    FOUND:   { S: { rvb: .12 }, P: { rvb: .18, dly: .1 } },
    ORGANIC: { P: { rvb: .14 }, C: { rvb: .2 } },
    GAMELAN: { C: { rvb: .3 }, P: { rvb: .22, dly: .08 } },
    CHIPKIT: { P: { dly: .06 } },
    HARDTECH: { K: { tune: .46, decay: .5, tone: .8, drive: .6, rmb: .4 }, S: { decay: .4, tone: .8, drive: .4 }, H: { decay: .34, tone: .85, locut: .25 }, C: { decay: .5, rvb: .18 }, T: { decay: .45, drive: .3 }, P: { rvb: .15, drive: .2 } },
    DUBTECH: { K: { tune: .44, decay: .6, tone: .42, drive: .1 }, S: { decay: .5, tone: .45, rvb: .38, dly: .28 }, H: { decay: .42, tone: .5, dly: .2, locut: .2 }, C: { decay: .7, rvb: .45 }, T: { decay: .55, rvb: .3 }, P: { dly: .35, rvb: .3 } },
    DEEPHOUSE: { K: { tune: .47, decay: .6, tone: .45, drive: .06 }, S: { decay: .5, tone: .55, rvb: .2 }, H: { decay: .45, tone: .6, locut: .15 }, C: { decay: .55, rvb: .22 }, T: { decay: .55 }, P: { rvb: .14, dly: .1, tone: .55 } },
    DISCO:   { K: { tune: .5, decay: .5, tone: .6, drive: .05 }, S: { decay: .5, tone: .7, rvb: .15 }, H: { decay: .5, tone: .8 }, C: { decay: .6, rvb: .18 }, T: { decay: .55 }, P: { rvb: .1, tone: .75 } },
    BREAKS:  { K: { tune: .5, decay: .42, tone: .72, drive: .3 }, S: { decay: .45, tone: .8, drive: .3, crush: .06 }, H: { decay: .36, tone: .78 }, C: { decay: .5 }, T: { decay: .45, drive: .2 }, P: { tone: .7, crush: .05 } },
    GARAGE:  { K: { tune: .5, decay: .42, tone: .7, drive: .1 }, S: { decay: .36, tone: .82, rvb: .12 }, H: { decay: .3, tone: .88, locut: .2 }, C: { decay: .45 }, T: { decay: .4 }, P: { tone: .8, dly: .1 } },
    FULLON:  { K: { tune: .46, decay: .5, tone: .68, drive: .35, rmb: .3 }, S: { decay: .45, tone: .7, rvb: .2 }, H: { decay: .3, tone: .9, locut: .15 }, C: { decay: .55, rvb: .3 }, T: { decay: .5 }, P: { ring: .1, rvb: .2, dly: .15 } },
    AMBIENT: { K: { tune: .44, decay: .62, tone: .35, rvb: .25 }, S: { decay: .6, tone: .4, rvb: .5 }, H: { decay: .5, tone: .45, rvb: .3 }, C: { decay: .8, rvb: .6 }, T: { decay: .65, rvb: .4 }, P: { rvb: .5, dly: .3 } },
    SYNTHKIT: { K: { decay: .5 }, S: { decay: .5 }, H: { decay: .5 }, C: { decay: .55, rvb: .15 }, T: { decay: .5 }, P: { rvb: .1 } },
    SYNTHHARD: { K: { decay: .48, drive: .25 }, S: { decay: .45, drive: .2 }, H: { decay: .4 }, C: { decay: .5, rvb: .15 }, T: { decay: .45 }, P: { rvb: .1 } },
    SYNTHSOFT: { K: { decay: .55 }, S: { decay: .55, rvb: .2 }, H: { decay: .5, tone: .65 }, C: { decay: .6, rvb: .3 }, T: { decay: .55, rvb: .15 }, P: { rvb: .2, dly: .1 } },
    'AC ROCK': { K: { rvb: .06 }, S: { rvb: .14 }, H: { rvb: .04 }, C: { rvb: .2 }, T: { rvb: .12 }, P: { rvb: .1 } },
    JAZZ:    { K: { rvb: .1 }, S: { rvb: .16 }, H: { rvb: .08 }, C: { rvb: .26 }, T: { rvb: .16 }, P: { rvb: .14 } },
    STUDIO:  { K: { rvb: .03 }, S: { rvb: .08 }, H: { rvb: .02 }, C: { rvb: .12 }, T: { rvb: .06 }, P: { rvb: .06 } },
    'AC LOFI': { K: { rvb: .12, crush: .1 }, S: { rvb: .2, crush: .12 }, H: { rvb: .08, crush: .1 }, C: { rvb: .24, crush: .1 }, T: { rvb: .14, crush: .1 }, P: { rvb: .12, crush: .08 } },
    'PERC LAB': { P: { rvb: .14 }, T: { rvb: .12 }, S: { rvb: .1 } },
    CINEMA:  { K: { rvb: .25 }, S: { rvb: .3 }, H: { rvb: .3 }, C: { rvb: .3 }, T: { rvb: .3 }, P: { rvb: .3 } },
    'FX PLAY':  { P: { dly: .12 }, T: { dly: .1 } },
    'TONAL': { P: { rvb: .18, dly: .08 }, T: { rvb: .15 }, K: { rvb: .1 } },
    RAVE:    { K: { tune: .5, decay: .52, tone: .75, drive: .48 }, S: { decay: .5, tone: .8, rvb: .3 }, H: { decay: .5, tone: .85 }, C: { decay: .78, rvb: .42 }, T: { decay: .5, drive: .2 }, P: { ring: .1, dly: .22, rvb: .2 } },
};
function kitName(s) { return s >= 16 ? FACTORY[s - 16][0] : 'USER ' + (s + 1); }
function kitGet(s) {
    if (s < 16) return kits[s];
    const f = FACTORY[s - 16], ls = lanesOf(sel), rows = [], st = KIT_STYLE[f[0]] || {}, base = defaultPads()[NTRK];
    for (let j = 0; j < ls.length; j++) {
        const r = base.slice(), ti = TYPE_NAME.indexOf(f[1][j % 16]); if (ti >= 0) r[0] = ti;
        r[1] = pads[ls[j]][1]; r[2] = pads[ls[j]][2];                       /* Volume and Pan stay as they are */
        const role = KIT_ROLE[j % 16], mod = st[role];                       /* every kit shapes its sounds: drive, tone, decay, crush, room ... per role */
        if (mod) for (const k in mod) r[PI[k]] = mod[k];
        rows.push(r);
    }
    return { d: 1, r: rows };
}
function kitSave() {
    kitsLoad();
    if (kitSlot >= 16) { showToast('FACTORY KIT', 'READ ONLY', 0); return; }
    const rows = (kind[sel] ? lanesOf(sel) : [sel]).map(l => pads[l].map(x => Math.round(x * 1000) / 1000));
    kits[kitSlot] = { d: kind[sel] ? 1 : 0, r: rows }; kitsSave();
    showToast('KIT ' + (kitSlot + 1), 'SAVED', 1);
}
function kitLoad() {
    kitsLoad(); if (kitBackup && !kitPrevQ.length && false) kitBackup = null;
    const k = kitSlot >= 16 && !kind[sel] ? { d: 1, r: [] } : kitGet(kitSlot);
    if (!k || !Array.isArray(k.r)) { showToast('KIT ' + kitName(kitSlot), 'EMPTY', 0); return; }
    if ((k.d ? 1 : 0) !== (kind[sel] ? 1 : 0)) { showToast('KIT ' + kitName(kitSlot), k.d ? 'IS A DRUM KIT' : 'IS A SYNTH SOUND', 0); return; }
    const ls = kind[sel] ? lanesOf(sel) : [sel];
    for (let j = 0; j < ls.length && j < k.r.length; j++) {
        const row = k.r[j]; if (!Array.isArray(row)) continue;
        for (let i = 0; i < PK.length && i < row.length; i++) if (soundIdx(i)) pads[ls[j]][i] = clamp(Number(row[i]) || 0, 0, i === 0 ? TYPE_NAME.length : 12);
        qPad(ls[j]); flushPad(ls[j]);                                 /* the whole kit reaches the engine at once, not 3 pads per tick */
    }
    markEdited(); showToast('KIT ' + kitName(kitSlot), 'LOADED', 1); needsRedraw = true;
}
let kitPrevQ = [], kitPrevT = 0;
let kitBackup = null;
function kitPreview(back) {                                /* turn right: load the kit and play the first pads; turn left: put the old sounds back */
    if (!kind[sel]) { showToast('KIT PREVIEW', 'DRUM TRACK ONLY', 0); return; }
    const ls = lanesOf(sel);
    if (back) { if (!kitBackup) { showToast('KIT PREVIEW', 'NOTHING TO RESTORE', 0); return; } ls.forEach((l, j) => { pads[l] = kitBackup[j].slice(); qPad(l); storeCs(l); }); kitBackup = null; markEdited(); showToast('KIT PREVIEW', 'RESTORED', 1); return; }
    if (!kitBackup) kitBackup = ls.map(l => pads[l].slice());
    kitLoad(); kitPrevQ = [0, 1, 2, 4, 5, 6].map(k => laneOf(sel, k)); kitPrevT = 14;
}
function kitPrevTick() {
    if (!kitPrevQ.length) return;
    if (--kitPrevT > 0) return;
    const l = kitPrevQ.shift(); flushPad(l); dspSet('dhit', l + ',100,0'); dspSet('dhit', l + ',0,0'); kitPrevT = 11;
}
function kitInfo() { kitsLoad(); if (kitSlot >= 16) return FACTORY[kitSlot - 16][0]; const k = kits[kitSlot]; return k ? (k.d ? 'DRUM' : 'SYN') : 'EMPTY'; }

/* ---- sound / kit folder browser: Shift + jog (SEQ / SOUND) or EDIT menu. Synth: folders > sub-folders > sounds, drum track: KITS (styles) first ---- */
const FB_SUB = {
    0: [['ACOUSTIC', /^AcKick/], ['DRUM SYNTH', /^D(Kick|Snr|Hat|Clap|Tom|Rim|Clave|Cow|Metal)/], ['TECHNO', /^KckTch/], ['HOUSE', /^KckHse/], ['TRANCE', /^KckTrn/], ['PSY + GOA', /^KckPsy/], ['HARD', /^KckHrd/], ['DUB', /^KckDub/], ['808 + SUB', /^(Sub808|808)/], ['FX HITS', /^Fx/], ['CLASSIC', /./]],
    1: [['AC SNARES', /^AcSnr/], ['AC CLAPS', /^AcClap/], ['AC RIMS', /^Ac/], ['DRUM SYNTH', /^D(Kick|Snr|Hat|Clap|Tom|Rim|Clave|Cow|Metal)/], ['SNARE', /^(Snr|Snare)/], ['CLAP', /^(Clap|Clp)/], ['RIM', /^Rim/], ['OTHER', /./]],
    2: [['AC HATS', /^AcHat/], ['AC CYMBALS', /^Ac(Cr|Spl|Chi|Ride)/], ['AC SHAKERS', /^Ac/], ['DRUM SYNTH', /^D(Kick|Snr|Hat|Clap|Tom|Rim|Clave|Cow|Metal)/], ['CLOSED HATS', /^Hat(Cl|Tick|Mid|Soft|Trap|Tight|Dust|Snap)/], ['OPEN HATS', /^Hat/], ['SHAKER', /^Sh[ak]/], ['CYMBALS', /./]],
    3: [['AC TOMS', /^AcTom/], ['AC WOOD + BELLS', /^Ac/], ['FX BLIPS + ZAPS', /^Fx/], ['DRUM SYNTH', /^D(Kick|Snr|Hat|Clap|Tom|Rim|Clave|Cow|Metal)/], ['TOMS', /^Tom/], ['CONGA + BONGO', /^(Cong|Cng|Bong|Bng)/], ['COWBELL', /^Cow/], ['FM PERC', /^FM/], ['GLITCH', /^Glch/], ['ZAPS + LASERS', /^(Zap|Laser)/], ['OTHER', /./]],
    4: [['FX IMPACTS', /^Fx(Imp|Sub|Thu|Boom)/], ['FX RISERS', /^Fx(Ris|Rev)/], ['FX SWEEPS + FALLS', /^Fx(Fall|Whoosh|Whsh|Sweep)/], ['RISER', /^Ris/], ['SWEEP + WHOOSH', /^(Sweep|NoiseSw)/], ['DOWNLIFTER', /^Down/], ['IMPACT + HIT', /^(Impact|ImpHit)/], ['OTHER', /./]],
    5: [['TONE PARTS', /^Tn/], ['SUB', /^(Sub|808)/], ['ROLL', /^Roll/], ['ACID', /^Acid/], ['REESE', /^Reese/], ['FM', /^FM/], ['SYNTH BASS', /./]],
    6: [['TONE PARTS', /^Tn/], ['PLUCK', /^Pluck/], ['HOOVER', /^Hoov/], ['LEAD', /./]],
    7: [['TONE PARTS', /^Tn/], ['STAB', /^Stab/], ['PAD', /^(Pad|Vowel|Hollow|Half)/], ['BELL', /^(Bell|Glass)/], ['SUPERSAW', /^(Super|Det|Huge|Oct|Trnc)/], ['KEYS + STRINGS', /./]],
};
const CLAP_RE = /^(Clap|Clp|AcClap|DClap)/, CYM_RE = /(Cym|Crash|Ride|Splash|Chi|Spl|Crs)/;
const isClapT = t => CLAP_RE.test(TYPE_NAME[t]), isCymT = t => CYM_RE.test(TYPE_NAME[t]);
function fbSub(name, c, pred) {
    const kids = FB_SUB[c].map(x => ({ n: x[0], l: [] }));
    for (const t of catList(c)) { if (pred && !pred(t)) continue; for (let k = 0; k < kids.length; k++) if (FB_SUB[c][k][1].test(TYPE_NAME[t])) { kids[k].l.push(t); break; } }
    return { n: name, kids: kids.filter(k => k.l.length) };
}
/* synth sounds: ordered rules decide the folder (group > sub folder); anything unmatched lands in the default folder of its family */
const SYN_RULES = [
    ['ARP', 'ARP SHORT', /^ArpPl/], ['PLUCK', 'HARP + LONG', /^(PlkHrp|PluckLong|PluckHarp|PluckDeep|PluckSoft)/], ['PLUCK', 'MUTED', /^(PlkMut|PluckMute)/], ['PLUCK', 'PLUCK', /^Pl(uck|k)/],
    ['CHORDS', 'DUB CHORDS', /^DubCrd/], ['STABS', 'HOUSE STABS', /^(HseStb|StabHouse)/], ['STABS', 'RAVE STABS', /^(RvStb|StabRave|StabHoov)/], ['CHORDS', 'TRANCE CHORDS', /^TrcChd/], ['STABS', 'STABS', /^Stab/],
    ['PADS', 'DARK', /^(DrkPad|PadDark|PadHuge)/], ['PADS', 'WARM', /^(WrmPad|PadWarm)/], ['ATMOS', 'ATMOSPHERE', /^(AtmPad|PadAir|PadSlow|PadLo)/], ['PADS', 'GLASS + VOWEL', /^(GlsPad|PadGlas|Vowel|Hollow|Half)/], ['PADS', 'PADS', /^Pad/],
    ['SUPERSAW', 'TRANCE SAW', /^(TrcSaw|TrncSaw)/], ['SUPERSAW', 'SAW STACKS', /^(SawStk|Super|Det|Huge|Oct|Fold)/],
    ['KEYS', 'ELECTRIC PIANO', /^(EP|Rhodes|Wurli|Keys)/], ['KEYS', 'CLAV', /^Clav/], ['ORGAN', 'ORGAN', /^Org/],
    ['MALLET', 'MALLET + CHIME', /^(BellMlt|Chime)/], ['BELLS', 'GONG', /^Gong/], ['BELLS', 'BELLS', /^(Bell|Glass)/],
    ['CHOIR', 'CHOIR', /^Choir/], ['BRASS', 'BRASS', /^Brass/], ['STRINGS', 'STRINGS', /^(Str|String)/],
    ['BASS', 'WOBBLE', /^Wob/], ['BASS', 'DEEP + DUB', /^(DeepBs|DubBs|SubDeep|SubSine|SubLow)/], ['BASS', 'SUB', /^(Sub|SineBass|Bass1)/], ['BASS', 'ROLL + GOA', /^(Roll|GoaBs)/], ['BASS', 'ACID', /^(Acid|Acd)/], ['BASS', 'REESE', /^(Reese|Rv)/], ['BASS', 'FM', /^FM/], ['BASS', 'TECHNO + HOUSE', /^(TechBs|HseBs)/], ['BASS', 'DNB', /^DnbBs/],
    ['LEAD', 'HOOVER', /^Hoov/], ['LEAD', 'ACID LEAD', /^(AcdLd|LeadAcid)/], ['LEAD', 'TRANCE LEAD', /^(TrcLd|TranceLd)/], ['LEAD', 'PSY LEAD', /^PsyLd/], ['LEAD', 'RETRO', /^(SqrLd|SqrLead|PulsLead|PulseLead|TriLead|SqrLead2)/], ['LEAD', 'SOFT', /^(SftLd|LeadSoft|SineLead)/], ['LEAD', 'SYNC', /^SyncL/],
];
/*NXEBEGIN*/const NX_ENG = [[1252,1266,6],[1267,1278,7],[1279,1290,8],[1291,1302,9],[1303,1314,10],[1315,1326,11],[1327,1338,12]];/*NXEEND*/
/*NXBEGIN*/const NX_GROUPS = [["MALLETS",[1064,1065,1066,1067,1068,1069,1070,1071,1072,1073,1074,1075,1076]],["BELLS + GONGS",[1077,1078,1079,1080,1081,1082,1083,1084,1085,1086,1087,1088,1089,1090]],["STRINGS",[1091,1092,1093,1094,1095,1096,1097]],["MODAL PERC",[1098,1099,1100,1101,1102,1103,1104,1105,1106,1107,1108,1109,1110,1111,1112,1113]],["FM KEYS",[1114,1115,1116,1117,1118,1119,1120]],["FM BELLS + PLUCK",[1121,1122,1123,1124,1125,1126,1127,1128]],["FM BASS",[1129,1130,1131,1132,1133]],["FM LEAD + PAD",[1134,1135,1136,1137,1138]],["FM PERC + FX",[1139,1140,1141,1142,1143]],["VOCAL",[1144,1145,1146,1147,1148,1149,1150,1151,1152,1153,1154,1155,1156,1157]],["CHIP LEADS",[1158,1159,1160,1161,1162,1163,1164,1165]],["CHIP ARPS",[1166,1167,1168,1169,1170,1171,1172]],["CHIP DRUMS",[1173,1174,1175,1176,1177,1178,1179]],["MORPH BASS",[1180,1181,1182,1183,1184,1185,1186]],["MORPH LEAD",[1187,1188,1189,1190,1191,1192,1193,1194]],["MORPH PAD",[1195,1196,1197,1198,1199,1200,1201,1202]],["MORPH KEYS",[1203,1204,1205,1206,1207,1208,1209,1210]],["FM INIT",[1211]],["DRUM SYNTH",[1212,1213,1214,1215,1216,1217,1218,1219,1220,1221,1222,1223,1224,1225,1226,1227,1228,1229,1230,1231,1232,1233,1234,1235,1236,1237,1238,1239,1240,1241,1242,1243,1244,1245,1246,1247,1248,1249,1250,1251,1457,1458,1459,1460,1461]],["ANALOG",[1252,1253,1254,1255,1256,1257,1258,1259,1260,1261,1262,1263,1264,1265,1266]],["WAVETABLE",[1267,1268,1269,1270,1271,1272,1273,1274,1275,1276,1277,1278]],["FM 4-OP",[1279,1280,1281,1282,1283,1284,1285,1286,1287,1288,1289,1290]],["ADDITIVE",[1291,1292,1293,1294,1295,1296,1297,1298,1299,1300,1301,1302]],["PAD + STRINGS",[1303,1304,1305,1306,1307,1308,1309,1310,1311,1312,1313,1314]],["STRING + PLUCK",[1315,1316,1317,1318,1319,1320,1321,1322,1323,1324,1325,1326]],["FOLD",[1327,1328,1329,1330,1331,1332,1333,1334,1335,1336,1337,1338]],["ACOUSTIC KICKS",[1339,1340,1341,1342,1343,1344,1345,1346,1347,1348]],["ACOUSTIC SNARES",[1349,1350,1351,1352,1353,1354,1355,1356,1357,1358,1359]],["ACOUSTIC TOMS",[1360,1361,1362,1363,1364,1365,1366,1367]],["ACOUSTIC HATS",[1368,1369,1370,1371,1372,1373,1374,1375]],["ACOUSTIC CYMBALS",[1376,1377,1378,1379,1380,1381,1382,1383,1384,1385]],["ACOUSTIC PERC",[1386,1387,1388,1389,1390,1391,1392,1393,1394,1395,1396,1397,1398,1399,1400,1401,1402,1403,1404,1405,1406,1407,1408,1409]],["FX IMPACTS",[1410,1411,1412,1413,1414,1415,1416]],["FX RISERS",[1417,1418,1419,1420,1421,1422,1423]],["FX SWEEPS",[1424,1425,1426,1427,1428,1429]],["FX LASERS + HITS",[1430,1431,1432,1433,1434,1435,1436,1437]],["TONE PARTS",[1438,1439,1440,1441,1442,1443,1444,1445,1446,1447,1448,1449,1450,1451,1452,1453,1454,1455,1456]]];/*NXEND*/
const SYN_ORDER = ['BASS', 'LEAD', 'ARP', 'PLUCK', 'STABS', 'CHORDS', 'PADS', 'ATMOS', 'SUPERSAW', 'KEYS', 'ORGAN', 'BELLS', 'MALLET', 'STRINGS', 'CHOIR', 'BRASS'];
let synTreeCache = null, synTreeN = 0;
function fbSynGroups() {
    if (synTreeCache && synTreeN === TYPE_NAME.length) return synTreeCache;
    const g = {}; SYN_ORDER.forEach(n => g[n] = {});
    const put = (grp, sub, t) => { (g[grp][sub] = g[grp][sub] || []).push(t); };
    for (let t = 0; t < TYPE_NAME.length; t++) {
        if (t >= 1064) continue;                                   /* the new engines have their own folder */
        const c = catOf(t); if (c < 5 || c > 7) continue;
        const n = TYPE_NAME[t]; let done = false;
        for (const r of SYN_RULES) if (r[2].test(n) && ((r[0] === 'BASS') === (c === 5)) && ((r[0] === 'LEAD' || r[0] === 'ARP' || r[0] === 'PLUCK') === (c === 6) || (c === 7 && r[0] !== 'BASS' && r[0] !== 'LEAD' && r[0] !== 'ARP' && r[0] !== 'PLUCK'))) { put(r[0], r[1], t); done = true; break; }
        if (!done) { if (c === 5) put('BASS', 'SYNTH BASS', t); else if (c === 6) put('LEAD', 'LEAD', t); else put('CHORDS', 'OTHER', t); }
    }
    synTreeCache = SYN_ORDER.map(n => ({ n: n, kids: Object.keys(g[n]).map(k => ({ n: k, l: g[n][k] })) })).filter(x => x.kids.length);
    synTreeN = TYPE_NAME.length; return synTreeCache;
}
function fbKitSlots() { kitsLoad(); const a = []; for (let i = 0; i < FACTORY.length; i++) a.push(16 + i); for (let i = 0; i < 16; i++) if (kits[i] && kits[i].d) a.push(i); return a; }
/* ---- by style / genre: the same sounds sorted the way you think while making a track ---- */
let nameIdx = null;
function tIdx(n) { if (!nameIdx) { nameIdx = {}; TYPE_NAME.forEach((x, i) => { if (nameIdx[x] === undefined) nameIdx[x] = i; }); } return nameIdx[n]; }
const KIT_ROLE_FOLDER = { K: 'KICK', S: 'SNARE', H: 'HATS', C: 'CYMBALS', T: 'PERCUSSION', P: 'PERCUSSION' };
const STYLE_X = {
    TECHNO: [/^(SnrTch|ClpDry|Snr909|ClapDry|ClapTght|RimTk|RimTick)/, /^(HatCl0|HatOp0[1-5]|HatClsd|HatOp909|HatTight|ShkGrv|CrsCym)/, /^(TomSet|ZapSet|FMPerc|GlchX|TomLo|TomHi)/],
    HOUSE: [/^(ClpRoom|ClapRoom|SnrSnap|RimWood|SnrRoom)/, /^(HatMid|HatSoft|HatCl0[5-9]|HatOp0[5-9]|ShakeMed|ShkGrv|RdeCym)/, /^(Cng|Bng|Cow|Conga|Bongo)/],
    TRANCE: [/^(ClapLong|Snr909|SnrBig|ClpDry|RimClick)/, /^(HatTick|HatLong|HatOp|Splash|RideBell)/, /^(TomSynth|TomRack|Zap|Conga)/],
    PSY: [/^(SnrDeep|ClapTrap|ClpTrp|RimLong)/, /^(HatTrap|HatSizzl|CrashLng)/, /^(FM|Zap|Glch|Laser)/],
    DUB: [/^(SnrLoFi|SnrLof|ClapDry|RimWood)/, /^(HatSoft|HatHalf|ShakeSft|RideSoft)/, /^(Conga|Bongo|TomFloor|TomMid|Cow)/],
    TRAP: [/^(SnrTrap|SnrTight|ClapTrap|ClpTrp)/, /^(HatTrap|HatShort|HatSizzl|HatCl)/, /^(FM|Zap)/],
    GABBA: [/^(SnrBig|ClapSnap)/, /^(HatShort|HatSizzl|Crash)/, /^(TomRack|Zap|PercHi|PercLo)/],
};
function fbStyles() {                                      /* drums: STYLE > KICK / SNARE / HATS / PERCUSSION, sounds picked for that style */
    const kids = [];
    FACTORY.forEach(f => {
        const g = {}; ['KICK', 'SNARE', 'CLAP', 'HATS', 'CYMBALS', 'PERCUSSION'].forEach(n => g[n] = []);
        const fold = (fo, t) => fo === 'SNARE' ? (isClapT(t) ? 'CLAP' : 'SNARE') : (fo === 'HATS' ? (isCymT(t) ? 'CYMBALS' : 'HATS') : fo);
        f[1].forEach((nm, p) => { const t = tIdx(nm); if (t === undefined) return; const fo = fold(KIT_ROLE_FOLDER[KIT_ROLE[p]], t); if (g[fo].indexOf(t) < 0) g[fo].push(t); });
        const pre = { TECHNO: /^KckTch/, HOUSE: /^KckHse/, TRANCE: /^KckTrn/, PSY: /^KckPsy/, GABBA: /^KckHrd/, DUB: /^KckDub/, HARDTECH: /^(KckHrd|KckTch)/, DUBTECH: /^KckDub/, DEEPHOUSE: /^KckHse/, DISCO: /^KckHse/, GARAGE: /^KckHse/, FULLON: /^KckPsy/, '808': /^(Sub808|808)/, TRAP: /^(Sub808|808)/ }[f[0]];
        if (pre) catList(0).forEach(t => { if (pre.test(TYPE_NAME[t]) && g.KICK.indexOf(t) < 0) g.KICK.push(t); });
        const X = STYLE_X[f[0]];
        if (X) [['SNARE', 1, X[0]], ['HATS', 2, X[1]], ['PERCUSSION', 3, X[2]]].forEach(q => catList(q[1]).forEach(t => { const fo = fold(q[0], t); if (q[2].test(TYPE_NAME[t]) && g[fo].indexOf(t) < 0) g[fo].push(t); }));
        const sub = Object.keys(g).filter(n => g[n].length).map(n => ({ n: n, l: g[n] }));
        if (sub.length) kids.push({ n: f[0], kids: sub });
    });
    return { n: 'BY STYLE', kids: kids };
}
const GENRE_RULES = [
    ['TECHNO', /^(Tch|Tech|Rumb|Rmb|Warehouse|Wh)/], ['ACID', /^(Acid|Acd|AcdLd|LeadAcid)/], ['TRANCE', /^(Trc|Trnc|Trance|Trn)/], ['PSY + GOA', /^(Psy|Goa)/], ['HOUSE', /^(Hse|House)/],
    ['DUB', /^(Dub|DeepBs|SubDeep|SubSine)/], ['RAVE + HOOVER', /^(Rv|Rave|Hoov)/], ['AMBIENT', /^(Atm|PadAir|PadSlow|DrkPad|WrmPad|GlsPad|Bell|Glass|Choir|Vowel)/], ['RETRO', /^(Sqr|Puls|Tri|Org|EP|Clav)/],
];
function fbGenres() {                                      /* synth: sounds sorted by genre */
    const kids = GENRE_RULES.map(r => ({ n: r[0], l: [] }));
    for (let t = 0; t < TYPE_NAME.length && t < 1064; t++) { const c = catOf(t); if (c < 5 || c > 7) continue; for (let k = 0; k < GENRE_RULES.length; k++) if (GENRE_RULES[k][1].test(TYPE_NAME[t])) { kids[k].l.push(t); break; } }
    return { n: 'BY GENRE', kids: kids.filter(k => k.l.length) };
}
function fbTree() {
    const fav = [{ n: 'FAVORITES', l: favs.slice().sort((a, b) => a - b) }];
    const smp = smpLoaded().length ? [{ n: 'MY SAMPLES', smp: true }] : [];
    const rec = [{ n: 'RECENT', l: recent.slice() }];
    const nx = [{ n: 'NEW ENGINES', kids: NX_GROUPS.map(g => ({ n: g[0], l: g[1].slice() })) }];
    const cat = (n, c, pr) => fbSub(n, c, pr);
    const iniL = [...new Set(BASE_T)].filter(b => b !== 359 && (kind[sel] ? catOf(b) <= 4 : catOf(b) >= 5)).sort((a, b) => a - b);
    if (!kind[sel] && FMI > 0) iniL.unshift(FMI);                  /* the simple FM first: a sound you build yourself, not a preset */
    const ini = iniL.length ? [{ n: 'INIT PATCHES', l: iniL, init: true }] : [];      /* one clean starting patch per engine: default tone, decay, drive and effects */
    const my = fav;                                                 /* favourites on top (no recent list) */
    const named = a => a.map(tIdx).filter(t => t !== undefined);
    const insts = { n: 'ENGINES', kids: [      /* every family opens with ONE clean INIT sound, then its presets */
        { n: 'ANALOG', l: nxList(6), initFirst: true },
        { n: 'WAVETABLE', l: nxList(7).concat(named(['MLeadSw', 'MLeadBr', 'MPadAir', 'MPadDrk', 'MBassDp', 'MBassGr', 'MKeyPlk', 'MKeyWrm'])), initFirst: true },
        { n: 'FM 4-OP', l: nxList(8), initFirst: true },
        { n: 'FM 2-OP', l: named(['FmInit', 'EPiano', 'FMBass', 'FmPluck', 'FmLead', 'FmSaw', 'FmPad', 'FmBrass', 'FmSub', 'FmBell']), initFirst: true },
        { n: 'FOLD', l: nxList(12), initFirst: true },
        { n: 'STRING', l: nxList(11), initFirst: true },
        { n: 'ADDITIVE', l: nxList(9), initFirst: true },
        { n: 'PAD + STRINGS', l: nxList(10).concat(named(['Pad', 'Strings'])), initFirst: true },
        { n: 'CLASSIC', l: named(['SynLead', 'SynBass', 'SuperSaw', 'Lead', 'Pluck', 'Stab', 'Acid', 'Reese', 'SubBass', 'RollBass', 'Hoover', 'Organ']), initFirst: true },
        { n: 'MODAL', l: named(['MarimMid', 'VibesMid', 'HarpMid', 'KalimHi', 'Celesta', 'GlockHi', 'Tubular', 'BowlLo']), initFirst: true },
        { n: 'VOCAL', l: named(['ChoirAh', 'ChoirOh', 'MaleAh', 'FemaleAh']), initFirst: true },
        { n: 'CHIP', l: named(['Pulse50', 'Pulse25', 'PwmSweep', 'ArpMaj', 'ChipBass']), initFirst: true } ] };
    const byRole = { n: 'BY ROLE', kids: [['BASS', [19, 20, 21, 22, 74, 79]], ['LEAD', [26, 73, 80]], ['PLUCK', [23]], ['PAD + STRINGS', [25, 78]], ['KEYS + ORGAN', [24, 76, 77]], ['BELLS', [27]]].map(r => ({ n: r[0], l: (() => { const l = []; for (let t = 1064; t < TYPE_NAME.length; t++) if (r[1].indexOf(BASE_T[t]) >= 0) l.push(t); return l; })() })) };
    const dsy = (re) => { const l = []; for (let t = 1064; t < TYPE_NAME.length; t++) if (re.test(TYPE_NAME[t])) l.push(t); return l; };
    const dsyn = { n: 'DRUM SYNTHS', kids: [['KICKS', /^DKick/], ['SNARES', /^DSnr/], ['HATS', /^DHat/], ['CLAPS', /^DClap/], ['TOMS', /^DTom/], ['RIM + METAL', /^D(Rim|Clave|Cow|Metal)/]].map(x => ({ n: x[0], l: dsy(x[1]), init: true })) };
    const fxA = { n: 'AUDIO FX', fx: 'xfx', l: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] };       /* one flat list: turn = you hear it on the sound, click = keep */
    const fxM = { n: 'MIDI FX', fx: 'mfx', l: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] };
    const fxK = fbWantFx ? [fxA, fxM] : [];
    const drums = { n: 'DRUMS', kindSw: 1, kids: (kitFresh[sel] ? [{ n: 'KITS', kits: true }] : []).concat([fbStyles(), { n: 'BY TYPE', kids: [cat('KICK', 0), cat('SNARE', 1, t => !isClapT(t)), cat('CLAP', 1, isClapT), cat('HATS', 2, t => !isCymT(t)), cat('CYMBALS', 2, isCymT), cat('PERCUSSION', 3), cat('FX', 4)].concat(nx) }, dsyn]) };
    const all = [{ n: 'ALL SOUNDS', l: TYPE_NAME.map((x, k) => k).filter(k => BASE_T[k] !== 359) }];
    const roleMap = { 'BASS': 'BASS', 'LEAD': 'LEAD', 'PLUCK': 'PLUCK', 'PAD + STRINGS': 'PADS', 'KEYS + ORGAN': 'KEYS', 'BELLS': 'BELLS' };
    const typed = fbSynGroups().map(g => ({ n: g.n, kids: g.kids.slice() }));      /* sorted by sound type (pad, lead, bass ...), not by genre: the new engines join each type as a NEW folder */
    byRole.kids.forEach(r => { const tg = typed.find(g => g.n === roleMap[r.n]); if (tg && r.l.length) tg.kids.push({ n: 'NEW ENGINES', l: r.l }); else if (r.l.length) typed.push({ n: r.n, kids: [{ n: 'NEW ENGINES', l: r.l }] }); });
    const sounds = { n: 'SOUNDS', kindSw: 0, kids: typed.concat([{ n: 'MORE', kids: nx.concat([cat('FX', 4), cat('PERCUSSION', 3), cat('KICK', 0), cat('SNARE', 1, t => !isClapT(t)), cat('CLAP', 1, isClapT), cat('HATS', 2, t => !isCymT(t)), cat('CYMBALS', 2, isCymT)], all) }]) };
    insts.n = 'SYNTHS'; insts.kindSw = 0;
    /* the same start for every track, like the question for a new track: SYNTHS | DRUMS | SOUNDS, each opens its list; picking the other kind switches the track */
    return { n: 'BROWSE', kids: [sounds, drums, insts].concat(my, fxK, smp) };
}
const FX_LABEL = { x2fx: null, m2fx: null, xfx: ['NONE', 'STUTTER', 'REVERSE', 'FLANGER', 'TREMOLO', 'AUTO PAN', 'COMB', 'WAH', 'GATE', 'LO-FI', 'DJ FILTER', 'GRANULATOR'], mfx: ['NONE', 'OCTAVER', 'CHORD', 'ARPEGGIO', 'STRUM', 'NOTE ECHO', 'RANDOM PITCH', 'RANDOM VEL', 'COMPRESS', 'SCALE', 'TRANSPOSE', 'DIATONIC CHORD'] };
FX_LABEL.x2fx = FX_LABEL.xfx; FX_LABEL.m2fx = FX_LABEL.mfx;
const FX_AMT = { xfx: 'xmix', x2fx: 'x2mix', mfx: 'mamt', m2fx: 'm2amt' }, FX_AMT_DEF = { xfx: 0.5, x2fx: 0.5, mfx: 0.7, m2fx: 0.7 };
function fbItems(node) {
    if (node.kids) return node.kids.map(k => ({ label: k.n, kid: k, cnt: fbCount(k) }));
    if (node.kits) return fbKitSlots().map(sl => ({ label: kitName(sl), val: sl }));
    if (node.smp) return smpLoaded().map(i => ({ label: smpNames[i], val: i }));
    if (node.fx) return node.l.map(t => ({ label: FX_LABEL[node.fx][t], val: t }));
    if (node.initFirst) return node.l.map((t, k) => ({ label: k === 0 ? 'INIT' : TYPE_NAME[t], val: t, init: k === 0 }));
    return node.l.map(t => ({ label: (node.init ? 'INIT ' : '') + TYPE_NAME[t], val: t }));
}
function fbCount(n) { return n.kids ? n.kids.reduce((a, k) => a + fbCount(k), 0) : (n.kits ? fbKitSlots().length : (n.smp ? smpLoaded().length : n.l.length)); }
function fbOpen() {
    const L = ln();
    if (padMenu) { padMenu = false; }
    fb = { st: [{ node: fbTree(), i: 0 }], orig: { t: Math.round(pads[L][0]), smp: pads[L][PI.smp], sst: pads[L][PI.sst], sen: pads[L][PI.sen], flt: FLT_KEYS.map(k => pads[L][PI[k]]) }, origAll: pads[L].slice() };
    if (!fbWantFx && !kind[sel] && synFresh[sel]) {            /* new synth track: land in the SYNTHS family list (Analog, Wavetable, FM, Fold ...) */
        const root = fb.st[0].node, ii = root.kids ? root.kids.findIndex(k => k.n === 'SYNTHS') : -1;
        if (ii >= 0 && root.kids[ii].kids && root.kids[ii].kids.length) { const sy = root.kids[ii]; fb.st = [{ node: root, i: ii }, { node: sy, i: 0 }]; needsRedraw = true; return; }
    }
    if (!fbWantFx && !kind[sel] && fbGoMem()) { needsRedraw = true; return; }
    if (kind[sel] && !padMenu) fbDive(fb.orig.t); else if (!kind[sel] && !fbWantFx) fbDiveAny(fb.orig.t);
    needsRedraw = true;
}
const synFresh = {};                                        /* a brand-new synth track: the browser opens in the INIT patches first */
const kitFresh = {};                                        /* KITS are offered only for a brand-new drum track (until its first sound / kit is chosen) */
const fbMem = [null, null];                                 /* the folder + entry of the last sound you chose, for synth and drum tracks */
function fbGoMem() {
    const m = fbMem[kind[sel] ? 1 : 0]; if (!m) return false;
    let node = fb.st[0].node; const st = [{ node, i: 0 }];
    for (const nm of m.names) { const j = node.kids ? node.kids.findIndex(k => k.n === nm) : -1; if (j < 0) return false; st[st.length - 1].i = j; node = node.kids[j]; st.push({ node, i: 0 }); }
    if (node.kids) return false;
    const its = fbItems(node), f = its.findIndex(x => x.val === m.val);
    if (f < 0) return false;
    if (!node.kits && !node.smp && !node.fx && m.val !== fb.orig.t) return false;       /* the sound was changed some other way since: use the normal route */
    st[st.length - 1].i = f; fb.st = st; return true;
}
function fbDiveAny(t) {                                     /* synth tracks without a memory: open the folder that holds the current sound (engines first) */
    const root = fb.st[0].node, seen = [];
    const find = (n, path) => {
        if (n.fx || n.smp || n.kits || n.kindSw === 1) return null;
        if (n.kids) { for (let j = 0; j < n.kids.length; j++) { const k = n.kids[j]; if (k.n === 'FAVORITES') continue; const r = find(k, path.concat([[n, j]])); if (r) return r; } return null; }
        return n.l && n.l.indexOf(t) >= 0 ? { path, node: n } : null;
    };
    const r = find(root, []); if (!r) return;
    const st = []; for (const pj of r.path) { st.push({ node: pj[0], i: pj[1] }); }
    st.push({ node: r.node, i: Math.max(0, r.node.l.indexOf(t)) }); fb.st = st;
}
const FB_ROLE_CAT = { K: 0, S: 1, H: 2, C: 2, T: 3, P: 3 }, FB_CAT_NAME = ['KICK', 'SNARE + CLAP', 'HATS + CYMBALS', 'PERCUSSION', 'FX'];
function fbHas(n, t) { return n.kids ? n.kids.some(k => fbHas(k, t)) : (n.l && !n.fx ? n.l.indexOf(t) >= 0 : false); }
function fbDive(t) {                                       /* a drum pad opens the browser in its own family: kick pad > KICK, hat pad > HATS ..., straight to the folder and the sound that is on the pad */
    const root = fb.st[0].node; let c = t === 359 ? -1 : catOf(t);
    if (!(c >= 0 && c <= 4)) c = FB_ROLE_CAT[KIT_ROLE[dsel[sel] & 15]];
    if (c === undefined || c < 0) return;
    const si = root.kids.findIndex(k => k.n === 'DRUMS'); if (si < 0) return;
    const bti = root.kids[si].kids.findIndex(k => k.n === 'BY TYPE'); if (bti < 0) return;
    const bt = root.kids[si].kids[bti];
    const cn = [['KICK'], ['SNARE', 'CLAP'], ['HATS', 'CYMBALS'], ['PERCUSSION'], ['FX']][c] || []; let ci = bt.kids.findIndex(k => cn.indexOf(k.n) >= 0 && fbHas(k, t)); if (ci < 0) ci = bt.kids.findIndex(k => cn.indexOf(k.n) >= 0); if (ci < 0) return;
    fb.st[0].i = si; fb.st.push({ node: root.kids[si], i: bti }); fb.st.push({ node: bt, i: ci }); let node = bt.kids[ci];
    fb.st.push({ node, i: 0 });
    while (node.kids) {
        const j = node.kids.findIndex(k => fbHas(k, t)); if (j < 0) break;
        fb.st[fb.st.length - 1].i = j; node = node.kids[j]; fb.st.push({ node, i: 0 });
    }
    if (node.l) { const f = node.l.indexOf(t); if (f >= 0) fb.st[fb.st.length - 1].i = f; }
}
function fbSetType(nt) {
    const L = ln(); if (Math.round(pads[L][0]) === nt) return;
    pads[L][0] = nt; if (liveOk(sel)) enqueue('p' + L + 'type', 'p' + p2(L + 1) + '_type', String(nt)); storeCs(L); markEdited();
}
function fbAudition() {
    if (fbOff) { dspSet(fbOff[0], fbOff[1]); fbOff = null; }
    if (kind[sel]) { const l = ln(); flushPad(l); dspSet('dhit', l + ',100,0'); fbOff = ['dhit', l + ',0,0']; } else { flushPad(sel); dspSet('npad', '0,100,0'); fbOff = ['npad', '0,0,0']; }
    fbT = 12;
}
const FLT_KEYS = ['cut', 'locut', 'res', 'fenv'], FLT_OPEN = [1, 0, 0, 0];
function fltSet(L, vals) { for (let i = 0; i < FLT_KEYS.length; i++) if (Math.abs(pads[L][PI[FLT_KEYS[i]]] - vals[i]) > 1e-6) setLaneP(L, FLT_KEYS[i], vals[i]); }
function recentAdd(v) { const i = recent.indexOf(v); if (i >= 0) recent.splice(i, 1); recent.unshift(v); if (recent.length > 12) recent.length = 12; markEdited(); }
function fbApply() {
    synFresh[sel] = false;
    const top = fb.st[fb.st.length - 1], its = fbItems(top.node), it = its[top.i]; if (!it) return;
    const L = ln();
    if (!top.node.kits) pSnap('pad', L, 'browse');
    if (top.node.kits) { kitSlot = it.val; kitPreview(false); kitPrevQ = []; }      /* no audition hits: just load, play the pads to listen */
    else if (top.node.smp) { if (Math.round(pads[L][0]) !== 359) setLaneP(L, 'type', 359); setLaneP(L, 'smp', it.val / 63); setLaneP(L, 'sst', 0); setLaneP(L, 'sen', 1); }
    else if (top.node.fx) { const k = top.node.fx; setLaneP(L, k, it.val); if (it.val > 0 && FX_AMT[k] && pads[L][PI[FX_AMT[k]]] < 0.05) setLaneP(L, FX_AMT[k], FX_AMT_DEF[k]); }
    else { if (fb.origAll) { pads[L] = fb.origAll.slice(); qPad(L); }      /* every sound starts from the same state (the pad as it was when the browser opened), so the same entry always sounds the same, whatever was heard before */
        if (top.node.initFirst && !it.init && nxEng(it.val)) { pads[L][PI.det] = 0; pads[L][PI.sub] = 0; pads[L][PI.fkey] = 0; pads[L][PI.fdec] = 0.5; qPad(L); }
        if (top.node.init || it.init) { initSound(L); if (it.val === FMI) { pads[L][PI.det] = 0.25; pads[L][PI.sub] = 0.3; pads[L][PI.fkey] = 0; pads[L][PI.decay] = 0.45; qPad(L); } } fbSetType(it.val); fltSet(L, FLT_OPEN); qPad(L); }      /* last: the whole row (with the NEW type) goes out as one, so the reset above can never overwrite it with the old type */      /* a new sound starts with the filter wide open: LP fully up, HP at 0 */
    needsRedraw = true;
}
function initSound(L) {                                    /* every sound parameter back to its default (volume, pan and type stay) */
    const d = defaultPads()[L]; if (!d) return;
    for (let i = 0; i < PK.length; i++) if (i !== PI.vol && i !== PI.pan && i !== 0) pads[L][i] = d[i];
    pads[L][PI.dly] = 0; pads[L][PI.rvb] = 0;                  /* an INIT sound is dry: no delay or reverb send (set them per track / per drum pad on the FX tile) */
    qPad(L);
}
function fbRestore() {
    const top = fb.st[fb.st.length - 1], L = ln();
    if ((top.node.init || top.node.l || top.node.smp) && fb.origAll) { pads[L] = fb.origAll.slice(); qPad(L); needsRedraw = true; return; }
    if (top.node.kits) { if (kitBackup) kitPreview(true); }
    else if (top.node.smp || top.node.l) {
        fbSetType(fb.orig.t); fltSet(L, fb.orig.flt);
        if (fb.orig.t === 359) { setLaneP(L, 'smp', fb.orig.smp); setLaneP(L, 'sst', fb.orig.sst); setLaneP(L, 'sen', fb.orig.sen); }
    }
}
function fbIsLeaf(n) { return !n.kids; }
let fbLast = 0;
let fbAcc = 0;
function fbMove(dl, big) {
    const top = fb.st[fb.st.length - 1], n = fbItems(top.node).length; if (!n) return;
    const now = Date.now(), gap = now - fbLast; fbLast = now;
    let st = big || 1; if (!big && n > 30) st = gap < 35 ? 8 : (gap < 75 ? 3 : 1);      /* a long list: turning the jog fast skips ahead, slow = one by one; knob 1 always jumps by 10 */
    top.i = clamp(top.i + (dl > 0 ? st : -st), 0, n - 1);      /* turning moves the highlight; stopped = you hear it (preview, Back restores), playing/recording = silent, the click chooses */
    const it = fbItems(top.node)[top.i];
    fbPend = (it && !it.kid && !playing && !recArmed) ? (top.node.kits ? 14 : 5) : 0;      /* preview only after the jog rests a moment */
    needsRedraw = true;
}
function fbClick() {
    const top = fb.st[fb.st.length - 1], its = fbItems(top.node), it = its[top.i];
    if (!it) { fb = null; needsRedraw = true; return; }
    if (it.kid && it.kid.kindSw !== undefined && it.kid.kindSw !== kind[sel] && fb.st.length === 1) {      /* the other kind: the track turns into a synth / drum track and the list opens */
        const nm = it.kid.n; setKind(sel, it.kid.kindSw);
        if (kind[sel] !== it.kid.kindSw) { fb = null; needsRedraw = true; return; }      /* e.g. no free drum track */
        fb = null; fbOpen(); fb.st = [fb.st[0]]; const j = fb.st[0].node.kids.findIndex(k => k.n === nm);
        if (j >= 0) { fb.st[0].i = j; fb.st.push({ node: fb.st[0].node.kids[j], i: 0 }); }
        needsRedraw = true; return;
    }
    if (it.kid) {
        const leaf = fbIsLeaf(it.kid); let i0 = 0;
        if (leaf) { const li = fbItems(it.kid); const cur = it.kid.fx ? Math.round(pads[ln()][PI[it.kid.fx]]) : it.kid.kits ? -1 : (it.kid.smp ? (Math.round(pads[ln()][0]) === 359 ? smpIdx(ln()) : -1) : Math.round(pads[ln()][0])); const f = li.findIndex(x => x.val === cur); i0 = f >= 0 ? f : 0; }
        fb.st.push({ node: it.kid, i: i0 }); needsRedraw = true; return;
    }
    fbApply();                                                /* the click confirms the sound */
    delete kitFresh[sel];
    if (!top.node.fx && !top.node.smp && typeof it.val === 'number') fbMem[kind[sel] ? 1 : 0] = { names: fb.st.slice(1).map(q => q.node.n), val: it.val };
    if (top.node.kits) { kitBackup = null; showToast('KIT ' + it.label, 'LOADED', 1); }
    else { showToast(top.node.smp ? 'SAMPLE' : (top.node.fx === 'xfx' || top.node.fx === 'x2fx' ? 'AUDIO FX' : (top.node.fx === 'mfx' || top.node.fx === 'm2fx' ? 'MIDI FX' : 'SOUND')), it.label, 1); if (!top.node.smp && !top.node.fx && typeof it.val === 'number') recentAdd(it.val); }
    fb = null; needsRedraw = true;
}
function fbFav() {                                         /* Shift + click in the browser: star on / off for the highlighted sound */
    const top = fb.st[fb.st.length - 1], it = fbItems(top.node)[top.i];
    if (!it || it.kid || top.node.kits || top.node.smp || top.node.fx || typeof it.val !== 'number') { showToast('FAVORITE', 'SOUNDS ONLY', 0); return; }
    const i = favs.indexOf(it.val); if (i >= 0) favs.splice(i, 1); else favs.push(it.val);
    markEdited(); showToast(it.label, i >= 0 ? 'FAV OFF' : 'FAVORITE *', i >= 0 ? 0 : 1); needsRedraw = true;
}
function fbIsCur(it, node) {                               /* is this entry (or the folder holding it) the sound that was on the pad when the browser opened? */
    const o = fb.orig; if (!o) return false;
    if (it.kid) return !it.kid.kits && !it.kid.smp && !it.kid.fx && o.t !== 359 && fbHas(it.kid, o.t);
    if (node.kits) return false;
    if (node.fx) return Math.round(fb.origAll[PI[node.fx]]) === it.val;
    if (node.smp) return o.t === 359 && it.val === Math.round(o.smp * 63);
    return typeof it.val === 'number' && it.val === o.t;
}
function fbBack() {
    if (fb.st.length > 1) { if (fbIsLeaf(fb.st[fb.st.length - 1].node)) fbRestore(); if (fb.dev && fb.st.length === 2) fb = null; else fb.st.pop(); } else fb = null;
    needsRedraw = true;
}
function drawFB() {
    clear_screen();
    const top = fb.st[fb.st.length - 1], its = fbItems(top.node);
    const path = fb.st.slice(1).map(s => s.node.n).join(' > ') || fb.st[0].node.n;
    titleBar((path.length > 15 ? fb.st[fb.st.length - 1].node.n : path).slice(0, 15), uiMin ? '' : (its.length ? (top.i + 1) + '/' + its.length : '0'));
    if (!its.length) { txt(4, 30, 'EMPTY', 1); return; }
    const favOn = it => (!it.kid && !top.node.kits && !top.node.smp && !top.node.fx && favs.indexOf(it.val) >= 0);
    if (uiMin) {                                           /* minimal: a quiet list in the small font, the chosen line inverted, no counts */
        const rows = 5, first = clamp(top.i - 2, 0, Math.max(0, its.length - rows));
        for (let r = 0; r < rows; r++) {
            const k = first + r; if (k >= its.length) break;
            const it = its[k], on = k === top.i, y = 12 + r * 10, c = on ? 0 : 1;
            if (on) fill_rect(0, y, 126, 10, 1);
            let lab = (favOn(it) ? '* ' : '') + String(it.label || '').toUpperCase();
            const mw = 108 - (it.kid ? 6 : 0); if (on) lab = mq(lab, Math.floor(mw / 6)); while (lab.length > 1 && tw(lab) > mw) lab = lab.substring(0, lab.length - 1);
            txt(3, y + 1, lab, c);
            if (it.kid) txt(113, y + 1, '>', c);
            if (fbIsCur(it, top.node)) fill_rect(121, y + 3, 3, 3, c);
        }
        const th = Math.max(3, Math.floor(50 / its.length)), ty = 12 + Math.floor((50 - th) * top.i / Math.max(1, its.length - 1));
        fill_rect(127, 12, 1, 50, 0); fill_rect(127, ty, 1, th, 1);
        return;
    }
    for (let q = -2; q <= 2; q++) {
        const k = top.i + q, y = 12 + (q + 2) * 10;
        if (k < 0 || k >= its.length) continue;
        const it = its[k], fv = favOn(it) ? '*' : ' ';
        const lw = it.kid ? Math.floor((97 - String(it.cnt).length * 6) / 6) : 17, lab = q === 0 ? mq(it.label || '', lw) : (it.label || '').slice(0, lw), cur = fbIsCur(it, top.node);
        if (q === 0) { fill_rect(0, y, 128, 10, 1); txt(3, y + 1, fv + lab, 0); if (it.kid) txt(104, y + 1, '>', 0); } else { txt(3, y + 1, fv + lab, 1); if (it.kid) txt(104, y + 1, '>', 1); }
        if (it.kid) { const c = String(it.cnt); txt(100 - c.length * 6, y + 1, c, q === 0 ? 0 : 1); }
        if (cur) fill_rect(118, y + 3, 4, 4, q === 0 ? 0 : 1);      /* the sound that was on the pad when the browser opened (or the folder it is in) */
    }
    const th = Math.max(3, Math.floor(50 / its.length)), ty = 12 + Math.floor((50 - th) * top.i / Math.max(1, its.length - 1));      /* scroll bar */
    fill_rect(126, 12, 2, 50, 0); fill_rect(127, 12, 1, 50, 1); fill_rect(126, ty, 2, th, 1);
}
/* ---- projects: 32 slots, each a full copy of the state ---- */
let projSlot = 0, projUsed = [], projNames = [], projUI = null;
const LASTP_FILE = '/data/UserData/schwung/taktile-lastproj.txt';
try { const lp = host_read_file(LASTP_FILE); if (lp) projSlot = clamp(parseInt(lp) || 0, 0, 31); } catch (e) { /* ignore */ }
function projRemember() { try { host_write_file(LASTP_FILE, String(projSlot)); } catch (e) { /* ignore */ } }
const PNAME_FILE = '/data/UserData/schwung/taktile-proj-names.json', PCHARS = ' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-', PNAME_LEN = 12;
function projNamesLoad() { projNames = []; try { const t = host_read_file(PNAME_FILE); const a = t ? JSON.parse(t) : []; for (let n = 0; n < 32; n++) projNames.push(typeof a[n] === 'string' ? a[n].slice(0, PNAME_LEN) : ''); } catch (e) { projNames = new Array(32).fill(''); } }
function projNamesSave() { try { host_write_file(PNAME_FILE, JSON.stringify(projNames)); } catch (e) { /* ignore */ } }
function projOpen(slot) { projScan(); projNamesLoad(); if (slot !== undefined && projSlot > 31) projSlot = 0; projUI = { i: clamp(projSlot, 0, 31), mode: 0, a: 0, nm: [], cur: 0, then: 0 }; needsRedraw = true; }
function projNm(n) { return projNames[n] || (projUsed[n] ? 'PROJECT ' + (n + 1) : ''); }
function projNameStart(n, then) { const nm = (projNames[n] || 'PROJECT ' + (n + 1)).toUpperCase().slice(0, PNAME_LEN).split(''); while (nm.length < PNAME_LEN) nm.push(' '); projUI.nm = nm.map(c => Math.max(0, PCHARS.indexOf(c))); projUI.cur = 0; projUI.mode = 2; projUI.then = then; }
function projNameStr() { return projUI.nm.map(i => PCHARS[i]).join('').replace(/\s+$/, ''); }
function projInput(type, d1, d2) {
    const u = projUI; if (type !== 0xB0) return false;
    if (d1 === CC.JOG) { const dl = decodeDelta(d2); if (!dl) return true; const dd = dl > 0 ? 1 : -1;
        if (u.mode === 0) u.i = clamp(u.i + dd, 0, 31); else if (u.mode === 1) u.a = clamp(u.a + dd, 0, 4); else if (u.mode === 3) { /* confirm: only click / back */ } else u.nm[u.cur] = (u.nm[u.cur] + dd + PCHARS.length) % PCHARS.length;
        needsRedraw = true; return true; }
    if (d1 === CC.JOGCLICK && d2 > 0) {
        if (u.mode === 0) { u.mode = 1; u.a = projUsed[u.i] ? 0 : 1; }
        else if (u.mode === 1) { projSlot = u.i;
            if (u.a === 0) { if (!projUsed[u.i]) showToast('PROJECT ' + (u.i + 1), 'EMPTY', 0); else { projUI = null; projLoad(); } }
            else if (u.a === 3) { if (projUsed[u.i]) { u.mode = 3; u.ck = 3; } else projNewHere(u.i); }
            else if (u.a === 4) { if (!projUsed[u.i]) showToast('PROJECT ' + (u.i + 1), 'EMPTY', 0); else { u.mode = 3; u.ck = 4; } }
            else projNameStart(u.i, u.a === 1 ? 1 : 2); }
        else if (u.mode === 3) { const k = u.ck, n = u.i; u.mode = 0; if (k === 3) projNewHere(n); else projDelete(n); }
        else { if (shiftHeld) u.cur = Math.max(0, u.cur - 1); else if (u.cur < PNAME_LEN - 1) u.cur++; }
        needsRedraw = true; return true; }
    if (u.mode === 2 && d1 === CC.BACK && d2 > 0 && !shiftHeld) {      /* naming: Back = delete a letter (Shift + Back = cancel) */
        if (u.nm[u.cur] === 0 && u.cur > 0) u.cur--; u.nm[u.cur] = 0; needsRedraw = true; return true; }
    if ((d1 === CC.LEFT || d1 === CC.RIGHT) && d2 > 0) {                /* arrows: through the letters of the name, or through the project slots (5 at a time) */
        const dd = d1 === CC.RIGHT ? 1 : -1;
        if (u.mode === 2) u.cur = clamp(u.cur + dd, 0, PNAME_LEN - 1); else if (u.mode === 0) u.i = clamp(u.i + dd * 5, 0, 31);
        needsRedraw = true; return true; }
    if (d1 === CC.MENU && d2 > 0 && u.mode === 2 && u.scn) { sceneNames[u.i] = projNameStr(); markEdited(); showToast('SCENE ' + (u.i + 1), sceneNames[u.i] || 'NAME CLEARED', 1); projUI = null; needsRedraw = true; return true; }
    if (d1 === CC.BACK && d2 > 0 && u.scn) { projUI = null; needsRedraw = true; return true; }
    if (d1 === CC.MENU && d2 > 0 && u.mode === 2) {                    /* Menu = OK: save (or just rename) under this name */
        projNames[u.i] = projNameStr(); projNamesSave(); projSlot = u.i;
        if (u.then === 1) { projSave(); } else showToast('PROJECT ' + (u.i + 1), 'RENAMED', 1);
        u.mode = 0; needsRedraw = true; return true; }
    if (d1 === CC.BACK && d2 > 0) { if (u.mode === 0) projUI = null; else u.mode = (u.mode === 2 || u.mode === 3) ? 1 : 0; needsRedraw = true; return true; }
    return d1 !== CC.SHIFT && d1 !== CC.PLAY;
}
function drawProj() {
    const u = projUI;
    if (u.mode === 2) {
        titleBar(u.scn ? 'SCENE ' + (u.i + 1) : 'NAME ' + p2(u.i + 1), u.scn ? 'NAME' : (u.then === 1 ? 'SAVE' : 'RENAME'));
        const nm = projNameStr(); txt(4, 12, 'TURN:LETTER', 1); txt(4, 20, 'CLICK:NEXT BACK:DEL', 1); txt(4, 28, '<> MOVE  MENU:OK', 1);
        for (let k = 0; k < PNAME_LEN; k++) { const x = 4 + k * 10, ch = PCHARS[u.nm[k]]; txt(x + 1, 40, ch === ' ' ? '' : ch, 1); fill_rect(x, 52, 8, k === u.cur ? 2 : 1, 1); }
        return;
    }
    if (u.mode === 3) {
        titleBar('PROJECT ' + p2(u.i + 1), fit(projNm(u.i) || 'EMPTY', 70));
        txt(4, 18, u.ck === 4 ? 'DELETE THIS PROJECT?' : 'REPLACE THIS PROJECT', 1); if (u.ck === 3) txt(4, 28, 'WITH A NEW EMPTY ONE?', 1);
        txt(4, 42, 'CLICK = YES', 1); txt(4, 52, 'BACK = NO', 1);
        return;
    }
    if (u.mode === 1) {
        titleBar('PROJECT ' + p2(u.i + 1), fit(projNm(u.i) || 'EMPTY', 70));
        const L = ['LOAD', 'SAVE HERE', 'RENAME', 'NEW EMPTY', 'DELETE'];
        for (let k = 0; k < 5; k++) { const y = 12 + k * 10, on = u.a === k, dim = (k === 0 || k === 4) && !projUsed[u.i]; if (on) fill_rect(2, y, 124, 9, 1); txt(8, y + 1, L[k] + (dim ? ' (EMPTY)' : ''), on ? 0 : 1); }
        return;
    }
    titleBar('PROJECTS', (u.i + 1) + '/32');
    const st = clamp(u.i - 2, 0, 27);
    for (let k = 0; k < 5; k++) { const n = st + k, y = 12 + k * 10, on = n === u.i, c = on ? 0 : 1; if (on) fill_rect(0, y, 128, 10, 1); txt(3, y + 1, p2(n + 1) + ' ' + (fit(projNm(n), 90) || '-'), c); if (n === projSlot && projUsed[n]) txt(118, y + 1, '*', c); }
}
function projNewHere(n) {                                   /* a new empty project in this slot: it is saved there right away and is the one you work on */
    projSlot = n; projUI = null; newProject(); projNames[n] = ''; projNamesSave(); projSave(); showToast('NEW PROJECT', 'IN SLOT ' + (n + 1), 1);
}
function projDelete(n) {                                    /* remove the saved project from this slot (the project you are working on stays as it is) */
    try { host_write_file(projFile(n), ''); } catch (e) { showToast('PROJECT', 'DELETE FAILED', 0); return; }
    projUsed[n] = false; projNames[n] = ''; projNamesSave(); showToast('PROJECT ' + (n + 1), 'DELETED', 1); needsRedraw = true;
}
function projFile(n) { return '/data/UserData/schwung/taktile-proj-' + (n + 1) + '.json'; }
function projScan() { projUsed = []; for (let n = 0; n < 32; n++) { let t = null; try { t = host_read_file(projFile(n)); } catch (e) { t = null; } projUsed.push(!!t); } }
function projSave() { projRemember(); saveState(); try { const t = host_read_file(STATE_FILE); if (t) { host_write_file(projFile(projSlot), t); projUsed[projSlot] = true; showToast('PROJECT ' + (projSlot + 1), 'SAVED', 1); return; } } catch (e) { /* ignore */ } showToast('PROJECT', 'SAVE FAILED', 0); }
let DEF = null, clrPend = { t: -1, ticks: 0 };
function firstDrum() {                                     /* a new project: track 1 is a Drum track (default kit), so the beat can start right away */
    chosen[0] = 1; setKind(0, 1); kitFresh[0] = true;
}
function firstSynth() {                                    /* a new project: track 1 is always a Synth with the first (plain FM) sound, so it plays right away */
    chosen[0] = 1; kind[0] = 0; slot[0] = -1;
    if (FMI > 0) { initSound(0); pads[0][PI.det] = 0.25; pads[0][PI.sub] = 0.3; pads[0][PI.fkey] = 0; pads[0][PI.decay] = 0.45; fltSet(0, FLT_OPEN); pads[0][0] = FMI; qPad(0); storeCs(0); }
}
function newProject() {                                    /* empty project: one drum track, every other track asks Synth/Drum when first selected (the old state is kept in the undo slot) */
    saveState(); backupState(); try { const cur = host_read_file(STATE_FILE); if (cur) host_write_file('/data/UserData/schwung/taktile-proj-undo.json', cur); } catch (e) { /* ignore */ }
    if (playing) { dspSet('play', '0'); playing = false; gs = 0; }
    if (DEF) { chOn.fill(0); chBak.fill(null); lens = DEF.lens.slice(); mutes = DEF.mutes.slice(); kind = DEF.kind.slice(); slot = DEF.slot.slice(); dsel = DEF.dsel.slice(); tv = DEF.tv.slice(); llen = DEF.llen.slice(); ed = DEF.ed.slice(); topModes = DEF.topModes.slice(); master = DEF.master.slice(); bpm = DEF.bpm; swing = DEF.swing; }
    lmutes = new Array(NLANE).fill(0); chosen = new Array(NTRK).fill(0); AUTO = {}; autoLast = {}; songOn = false; songLoop = true; songList = [[0, 4, 1, 0, 0, 0]]; songOff = 0; songNext = -1; songMuted = false; songTempoOn = false; songSlot = 0; layS = 0; layD = 0; moClk = 0; moMask = 0; moChn = 0; moXp = 0; moVp = 0; moDn = 0;
    euKL.fill(4); euRL.fill(0); euV = 100; for (let i = 0; i < NLANE; i++) euP[i] = euDef();
    pads = defaultPads(); steps = newSteps(); csnd = null; undoStack = []; redoStack = []; miscLast = null; fillChg = []; tcolO.fill(0); lockT.fill(0); for (const r of fol) r.fill(0);
    ensureCs(); for (let tt = 0; tt < NTRK; tt++) applyClipSound(tt, ed[tt]); firstDrum();
    queue = []; qEverything(); initialPush = 100; sel = 0; stepPage = 0; heldStep = -1; heldPad = -1; navStack = []; view = 0; askKind = -1;
    markEdited(); saveState(); showToast('NEW PROJECT', 'TRACK 1 = DRUM', 1); needsRedraw = true;
}
const BAK_IDX = '/data/UserData/schwung/taktile-backup.idx', BAK_N = 6;
function bakFile(n) { return '/data/UserData/schwung/taktile-backup-' + (((n % BAK_N) + BAK_N) % BAK_N) + '.json'; }
function backupState() {                                   /* ring of 6 automatic copies: at start, at exit and before NEW */
    try {
        const t = host_read_file(STATE_FILE); if (!t) return;
        let n = parseInt(host_read_file(BAK_IDX)) || 0;
        host_write_file(bakFile(n), t); host_write_file(BAK_IDX, String(n + 1));
    } catch (e) { /* ignore */ }
}
function restoreBackup() {
    let n = 0, t = null; try { n = parseInt(host_read_file(BAK_IDX)) || 0; t = n > 0 ? host_read_file(bakFile(n - 1)) : null; } catch (e) { t = null; }
    if (!t) { showToast('RESTORE', 'NO BACKUP', 0); return; }
    saveState(); backupState(); applyStateText(t, 'BACKUP RESTORED');
}
function applyStateText(t, msg) {
    host_write_file(STATE_FILE, t);
    if (playing) { dspSet('play', '0'); playing = false; gs = 0; }
    AUTO = {}; autoLast = {}; fillChg = [];
    loadState(); ensureCs(); for (let tt = 0; tt < NTRK; tt++) applyClipSound(tt, ed[tt]);
    queue = []; qEverything(); initialPush = 100; sel = 0; stepPage = 0; heldStep = -1; heldPad = -1; navStack = []; view = 0;
    showToast(msg, 'DONE', 1); needsRedraw = true;
}
function projLoad() {
    projRemember();
    let t = null; try { t = host_read_file(projFile(projSlot)); } catch (e) { t = null; }
    if (!t) { showToast('PROJECT ' + (projSlot + 1), 'EMPTY', 0); return; }
    saveState(); try { const cur = host_read_file(STATE_FILE); if (cur) host_write_file('/data/UserData/schwung/taktile-proj-undo.json', cur); } catch (e) { /* ignore */ }
    host_write_file(STATE_FILE, t);
    if (playing) { dspSet('play', '0'); playing = false; gs = 0; }
    AUTO = {}; autoLast = {};
    loadState(); ensureCs(); for (let tt = 0; tt < NTRK; tt++) applyClipSound(tt, ed[tt]);
    queue = []; qEverything(); initialPush = 100; sel = 0; stepPage = 0; heldStep = -1; heldPad = -1; navStack = []; view = 0;
    { let nt = 0; for (let t = 0; t < NTRK; t++) { for (const l of lanesOf(t)) { let h = false; for (let p = 0; p < NPAT && !h; p++) if (laneHas(p, l)) h = true; if (h) { nt++; break; } } } showToast('LOADED ' + (projNames[projSlot] || ('PROJECT ' + (projSlot + 1))), bpm + ' BPM  ' + nt + ' TRACKS', 1); } needsRedraw = true;
}
/* ---- euclid page (per sound) ---- */
let euKL = new Array(NLANE).fill(4), euRL = new Array(NLANE).fill(0), euV = 100, euPage = 0;
const EU_KEYS = { eon: 1, q0len: 1, q0hit: 1, q0rot: 1, q0not: 1, q0prb: 1, q0hum: 1, q0acc: 1, q0on: 1, q1len: 1, q1hit: 1, q1rot: 1, q1not: 1, q1prb: 1, q1hum: 1, q1acc: 1, q1on: 1, q2len: 1, q2hit: 1, q2rot: 1, q2not: 1, q2prb: 1, q2hum: 1, q2acc: 1, q2on: 1, q3len: 1, q3hit: 1, q3rot: 1, q3not: 1, q3prb: 1, q3hum: 1, q3acc: 1, q3on: 1, ekeep: 1, erev: 1, einv: 1, eclr: 1, ehit: 1, erot: 1, elen: 1, emut: 1, evel: 1, eacc: 1, ehum: 1, enud: 1, eprb: 1, egat: 1, erat: 1, econd: 1 };
for (const k of ['enot', 'ealt', 'eacn', 'ewlk', 'ewln', 'ernd', 'eoup', 'eodn']) EU_KEYS[k] = 1;
const EU_PAGES = [
    [['eon', 'Euclid', 'Mode'], ['ehit', 'Hits'], ['erot', 'Rotate', 'Rot'], ['elen', 'Length', 'Len'], ['emut', 'Mutate', 'Mut'], ['evel', 'Velocity', 'Vel'], ['eacc', 'Accent', 'Acc'], ['ehum', 'Human', 'Hum']],
    [['eprb', 'Prob'], ['egat', 'Gate'], ['erat', 'Ratchet', 'Rat'], ['econd', 'Cond'], ['enud', 'Nudge', 'Nud'], ['erev', 'Reverse', 'Rev'], ['einv', 'Invert', 'Inv'], ['eclr', 'Clear', 'Clr']]];
const EU_PGNAME = ['PATTERN + LEVEL', 'TRIGGER + EXTRA', 'NOTE PITCH', 'LANE 1', 'LANE 2', 'LANE 3', 'LANE 4'], EU_NPG = 7;
/* four euclid lanes per sound: every lane has its own length, hits, rotation, note, probability, human and accent; they play together and the pattern repeats after their common length */
const EU_LF = [['len', 'Length', 'Len'], ['hit', 'Hits', 'Hit'], ['rot', 'Rotate', 'Rot'], ['not', 'Note', 'Note'], ['prb', 'Prob', 'Prob'], ['hum', 'Human', 'Hum'], ['acc', 'Accent', 'Acc'], ['on', 'Lane on', 'On']];
const EU_LDEF = [[16, 4], [12, 3], [8, 3], [6, 2]];
EU_PAGES.push([['enot', 'Note'], ['ealt', 'Alt'], ['eacn', 'AccNote'], ['ewlk', 'Walk'], ['ewln', 'WalkLen'], ['ernd', 'Random'], ['eoup', 'OctUp'], ['eodn', 'OctDown']]);
for (let q = 0; q < 4; q++) EU_PAGES.push(EU_LF.map((f, i) => ['q' + q + f[0], i === 0 ? (q + 1) + ' ' + f[1] : f[1], i === 0 ? (q + 1) + ' ' + f[2] : f[2]]));
function euQ(L) { const P = euP[L]; if (!Array.isArray(P.q)) P.q = EU_LDEF.map(d => ({ on: 0, len: d[0], hit: d[1], rot: 0, not: 0, prb: 0, hum: 0, acc: 0 })); return P.q; }
function euLanesOn(L) { const q = euP[L].q; return Array.isArray(q) && q.some(l => l.on); }
function euGcd(a, b) { while (b) { const t = a % b; a = b; b = t; } return a; }
function euDef() { return { eo: 0, n0: 0, alt: 0, an: 0, wk: 0, wl: 4, rn: 0, ou: 0, od: 0, q: null, keep: 0, rev: 0, inv: 0, acc: 0, hum: 0, mut: 0, nud: 0, prb: 0, gat: 0, rat: 0, cond: 0, seed: 1 }; }
let euP = []; for (let i = 0; i < NLANE; i++) euP.push(euDef());
function euSetPage(p) { euPage = ((p % EU_NPG) + EU_NPG) % EU_NPG; VIEW_ITEMS[36] = EU_PAGES[euPage]; acc.fill(0); needsRedraw = true; }
function euRnd(L, s) { let x = ((euP[L].seed * 9301 + s * 49297 + 233280) >>> 0) % 233280; x = (x * 9301 + 49297) % 233280; return x / 233280; }
function euGetQ(key) {
    const l = euQ(ln())[key.charCodeAt(1) - 48], f = key.substring(2);
    switch (f) {
        case 'len': return [l.len / 64, String(l.len)];
        case 'hit': return [l.hit / l.len, String(l.hit)];
        case 'rot': return [l.rot / l.len, String(l.rot)];
        case 'not': return [(l.not + 24) / 48, (l.not > 0 ? '+' : '') + l.not];
        case 'prb': return [PROB_PCT[l.prb] / 100, PROB_PCT[l.prb] + '%'];
        case 'hum': return [l.hum / 40, l.hum + '%'];
        case 'acc': return [l.acc / 60, '+' + l.acc];
        default: return [l.on ? 1 : 0, l.on ? 'ON' : 'OFF'];
    }
}
function euEditQ(key, delta, idx) {
    const L = ln(), l = euQ(L)[key.charCodeAt(1) - 48], f = key.substring(2), up = delta > 0 ? 1 : -1;
    if (f === 'len') { l.len = clamp(l.len + up * (shiftHeld ? 4 : 1), 1, 64); l.hit = Math.min(l.hit, l.len); l.rot = l.rot % l.len; }
    else if (f === 'hit') l.hit = clamp(l.hit + up, 0, l.len);
    else if (f === 'rot') l.rot = clamp((l.rot % l.len) + up, 0, l.len - 1);
    else if (f === 'not') l.not = clamp(l.not + up * (shiftHeld ? 12 : 1), -24, 24);
    else if (f === 'prb') { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; l.prb = clamp(l.prb - (acc[idx] > 0 ? 1 : -1), 0, 31); acc[idx] = 0; }
    else if (f === 'hum') l.hum = clamp(l.hum + delta * 2, 0, 40);
    else if (f === 'acc') l.acc = clamp(l.acc + delta * (shiftHeld ? 1 : 4), 0, 60);
    else { acc[idx] += delta; if (Math.abs(acc[idx]) < 2) return false; l.on = acc[idx] > 0 ? 1 : 0; acc[idx] = 0; }
    if (f !== 'on') l.on = 1;                                  /* touching a lane's value switches the lane on, so it is heard and shown at once */
    euP[L].seed = (euP[L].seed * 31 + 13) % 9973;
    euApplyLanes(); return true;
}
function euNote(L, s, hi) {                                 /* pitch of one euclid hit: base note, every second hit, accents, a walk up / down, random, octave jumps */
    const P = euP[L]; let n = P.n0;
    if (hi & 1) n += P.alt;
    if ((s & 3) === 0) n += P.an;
    if (P.wk) n += P.wk * (hi % Math.max(1, P.wl));
    if (P.rn > 0) n += Math.round((euRnd(L, s + 300) * 2 - 1) * P.rn);
    if (P.ou > 0 && euRnd(L, s + 600) * 100 < P.ou) n += 12;
    if (P.od > 0 && euRnd(L, s + 900) * 100 < P.od) n -= 12;
    return clamp(n, -24, 24);
}
function euApplyLanes() {
    if (!euP[ln()].eo) return;                                  /* merge the lanes that are on into the sound's steps */
    const L = ln(), c = ed[sel], P = euP[L], ql = euQ(L).filter(l => l.on);
    snap(c, L);
    const off = () => [0, 100, 0, 0, 0, 0, 0, 0];
    let tot = 1, cut = false; for (const l of ql) { tot = tot / euGcd(tot, l.len) * l.len; if (tot > MAXSTEP) { tot = MAXSTEP; cut = true; break; } }
    if (!ql.length) { qSeq(c, L); markEdited(); return; }
    setLen(tot, false);
    let hi = 0;
    for (let s = 0; s < MAXSTEP; s++) {
        let best = null;
        if (s < tot) for (let i = 0; i < ql.length; i++) {
            const l = ql[i], pos = s % l.len, idx = ((pos - l.rot) % l.len + l.len) % l.len;
            if (!(l.hit > 0 && ((idx * l.hit) % l.len) < l.hit)) continue;
            const v = clamp(euV + ((pos & 3) === 0 ? l.acc : 0) - Math.floor(euRnd(L, s * 4 + i) * l.hum * 1.5), 1, 127);
            if (!best || v >= best[1]) best = [1, v, l.not, l.prb, P.rat, P.gat, P.nud, P.cond];
        }
        if (best) { best[2] = clamp(best[2] + euNote(L, s, hi), -24, 24); hi++; }
        steps[c][L][s] = best || off();
    }
    qSeq(c, L); markEdited();
    if (cut) showToast('EUCLID', 'LONGER THAN ' + MAXSTEP, 0);
}
function euGet(key) {                                     /* [fraction 0..1, text] */
    if (key.charCodeAt(0) === 113) return euGetQ(key);
    const L = ln(), n = Math.max(1, LL()), P = euP[L];
    switch (key) {
        case 'ehit': return [euKL[L] / n, String(euKL[L])];
        case 'erot': return [(((euRL[L] % n) + n) % n) / n, String(((euRL[L] % n) + n) % n)];
        case 'elen': return [n / MAXSTEP, String(n)];
        case 'emut': return [P.mut / 100, P.mut + '%'];
        case 'enot': return [(P.n0 + 24) / 48, (P.n0 > 0 ? '+' : '') + P.n0];
        case 'ealt': return [(P.alt + 12) / 24, (P.alt > 0 ? '+' : '') + P.alt];
        case 'eacn': return [(P.an + 12) / 24, (P.an > 0 ? '+' : '') + P.an];
        case 'ewlk': return [(P.wk + 4) / 8, (P.wk > 0 ? '+' : '') + P.wk];
        case 'ewln': return [P.wl / 16, String(P.wl)];
        case 'ernd': return [P.rn / 12, P.rn ? '+-' + P.rn : 'OFF'];
        case 'eoup': return [P.ou / 100, P.ou + '%'];
        case 'eodn': return [P.od / 100, P.od + '%'];
        case 'eon': return [P.eo / 2, ['OFF', 'ON', 'ADD'][P.eo]];
        case 'ekeep': return [P.keep, P.keep ? 'ON' : 'OFF'];
        case 'erev': return [P.rev, P.rev ? 'ON' : 'OFF'];
        case 'einv': return [P.inv, P.inv ? 'ON' : 'OFF'];
        case 'eclr': return [0, 'TURN >'];
        case 'evel': return [euV / 127, String(euV)];
        case 'eacc': return [P.acc / 60, '+' + P.acc];
        case 'ehum': return [P.hum / 40, P.hum + '%'];
        case 'enud': return [P.nud / NUD_MAX, nudTxt(P.nud)];
        case 'eprb': return [PROB_PCT[P.prb] / 100, PROB_PCT[P.prb] + '%'];
        case 'egat': return [P.gat / 6, GATE_NAME[P.gat]];
        case 'erat': return [P.rat / 3, (P.rat + 1) + 'x'];
        case 'econd': return [P.cond / 7, COND_NAME[P.cond]];
    }
    return [0, ''];
}
function euEdit(key, delta, idx) {
    if (key.charCodeAt(0) === 113) return euEditQ(key, delta, idx);
    const L = ln(), n = Math.max(1, LL()), P = euP[L], big = shiftHeld ? 1 : 4;
    const stepD = (m) => { acc[idx] += delta; if (Math.abs(acc[idx]) < m) return 0; const d = acc[idx] > 0 ? 1 : -1; acc[idx] = 0; return d; };
    let mut = false;
    if (key === 'ehit') euKL[L] = clamp(euKL[L] + (delta > 0 ? 1 : -1), 0, n);
    else if (key === 'erot') euRL[L] = clamp((((euRL[L] % n) + n) % n) + (delta > 0 ? 1 : -1), 0, n - 1);
    else if (key === 'enot') P.n0 = clamp(P.n0 + (delta > 0 ? 1 : -1) * (shiftHeld ? 12 : 1), -24, 24);
    else if (key === 'ealt') P.alt = clamp(P.alt + (delta > 0 ? 1 : -1), -12, 12);
    else if (key === 'eacn') P.an = clamp(P.an + (delta > 0 ? 1 : -1), -12, 12);
    else if (key === 'ewlk') P.wk = clamp(P.wk + (delta > 0 ? 1 : -1), -4, 4);
    else if (key === 'ewln') P.wl = clamp(P.wl + (delta > 0 ? 1 : -1), 1, 16);
    else if (key === 'ernd') P.rn = clamp(P.rn + (delta > 0 ? 1 : -1), 0, 12);
    else if (key === 'eoup') P.ou = clamp(P.ou + (delta > 0 ? 5 : -5), 0, 100);
    else if (key === 'eodn') P.od = clamp(P.od + (delta > 0 ? 5 : -5), 0, 100);
    else if (key === 'elen') setLen(LL() + (delta > 0 ? 1 : -1) * (shiftHeld ? 4 : 1), false);
    else if (key === 'emut') { P.mut = clamp(P.mut + (delta > 0 ? 5 : -5), 0, 100); P.seed = (P.seed * 31 + 7) % 9973; mut = true; }
    else if (key === 'evel') euV = clamp(euV + delta * big, 1, 127);
    else if (key === 'eacc') P.acc = clamp(P.acc + delta * big, 0, 60);
    else if (key === 'ehum') { P.hum = clamp(P.hum + delta * 2, 0, 40); P.seed = (P.seed * 31 + 11) % 9973; }
    else { const d = stepD(2); if (!d) return false;
        if (key === 'eon') { P.eo = clamp(P.eo + d, 0, 2); P.keep = P.eo === 2 ? 1 : 0; }
        else if (key === 'ekeep') P.keep = d > 0 ? 1 : 0;
        else if (key === 'erev') P.rev = d > 0 ? 1 : 0;
        else if (key === 'einv') P.inv = d > 0 ? 1 : 0;
        else if (key === 'eclr') { if (d < 0) return true; const c = ed[sel]; snap(c, L); if (Array.isArray(P.q)) P.q.forEach(l => { l.on = 0; }); for (let s = 0; s < MAXSTEP; s++) steps[c][L][s] = [0, 100, 0, 0, 0, 0, 0, 0]; qSeq(c, L); markEdited(); showToast('EUCLID', 'PATTERN CLEARED', 0); return true; }
        else if (key === 'enud') P.nud = clamp(P.nud + d, 0, NUD_MAX);
        else if (key === 'eprb') P.prb = clamp(P.prb - d, 0, 31);
        else if (key === 'egat') P.gat = clamp(P.gat + d, 0, 6);
        else if (key === 'erat') P.rat = clamp(P.rat + d, 0, 3);
        else if (key === 'econd') P.cond = clamp(P.cond + d, 0, 7); }
    euApply(); return true;
}
function euApply() {
    if (!euP[ln()].eo) return;                              /* Euclid is off: nothing is written into the steps */
    if (euLanesOn(ln())) { euApplyLanes(); return; }
    const L = ln(), n = LL(), c = ed[sel], P = euP[L], k = clamp(euKL[L], 0, n), r = ((euRL[L] % n) + n) % n;
    euKL[L] = k; snap(c, L);
    const off = () => [0, 100, 0, 0, 0, 0, 0, 0];
    let hi = 0;
    for (let s = 0; s < MAXSTEP; s++) {
        const pp = P.rev ? n - 1 - s : s, idx = ((pp - r) % n + n) % n; let hit = s < n && k > 0 && ((idx * k) % n) < k; if (P.inv && s < n) hit = !hit;
        if (hit && P.keep && steps[c][L][s][0]) continue;
        if (hit) { const v = clamp(euV + ((s & 3) === 0 ? P.acc : 0) - Math.floor(euRnd(L, s) * P.hum * 1.5), 1, 127); steps[c][L][s] = [1, v, euNote(L, s, hi), P.prb, P.rat, P.gat, P.nud, P.cond]; hi++; }
        else steps[c][L][s] = off();
    }
    if (P.mut > 0 && n > 1) {                                  /* mutate: a seeded share of the steps flips (new seed with every turn of the knob) */
        const f = Math.max(1, Math.round(n * P.mut / 200));
        for (let i = 0; i < f; i++) {
            const s = Math.floor(euRnd(L, 100 + i) * n), st = steps[c][L][s];
            if (st[0]) steps[c][L][s] = off(); else steps[c][L][s] = [1, clamp(euV - 15, 1, 127), euNote(L, s, 0), P.prb, P.rat, P.gat, P.nud, P.cond];
        }
    }
    qSeq(c, L); markEdited();
}
/* ---- MIDI out (USB-A): clock + notes per track on channel = track number ---- */
let moClk = 0, moMask = 0, moTrk = 0, moChn = 0, moXp = 0, moVp = 0, moDn = 0;
function moStr() { return moClk + ',' + moMask + ',' + moChn + ',' + moXp + ',' + moVp + ',' + moDn; }
function moPush() { dspSet('midiout', moStr()); dirty = true; }
const MO_NOTE = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
function moNoteName(n) { return MO_NOTE[n % 12] + (Math.floor(n / 12) - 2); }
/* ---- mixer: meters + sends ---- */
let mixMode = 0, meterV = new Array(16).fill(0), meterPoll = 0;
function meterTick() {
    if (view !== 20 || ++meterPoll % 3) return;
    const r = dspGet('meters'); if (typeof r !== 'string' || r.length < 32) return;
    let ch = false; for (let t = 0; t < 16; t++) { const v = parseInt(r.substr(t * 2, 2)) || 0; if (v !== meterV[t]) { meterV[t] = v; ch = true; } }
    if (ch) needsRedraw = true;
}
/* ---- song mode: chain of clips (scenes), each for N bars ---- */
let songMutePend = false, songMuteBar = -1, songOn = false, songLoop = true, songList = [[0, 4, 1, 0, 0, 0]], songSlot = 0, songSent = -1, songLaunched = -1, songOff = 0, songNext = -1, songFillBar = -1, songMuted = false, songTempoOn = false;
const SONG_MAX = 32, SONG_TAG = ['--', 'INTRO', 'BUILD', 'DROP', 'BREAK', 'VERSE', 'CHORUS', 'OUTRO'], SONG_TRANS = ['HARD', 'FILL', 'DRUMS 1ST'], TAG_COL = [124, 12, 20, 1, 40, 60, 85, 100];
function songNorm(e) { e = Array.isArray(e) ? e : []; return [clamp(Number(e[0]) | 0, 0, NPAT - 1), clamp(Number(e[1]) | 0 || 1, 1, 32), clamp(Number(e[2]) | 0 || 1, 1, 16), (Number(e[3]) | 0) >= 40 ? clamp(Number(e[3]) | 0, 40, 240) : 0, clamp(Number(e[4]) | 0, 0, 2), clamp(Number(e[5]) | 0, 0, 7)]; }
function sceneHas(c) { for (let t = 0; t < NTRK; t++) if (trackHasSteps(c, t)) return true; return false; }
function songLen(e) { return e[1] * (e[2] || 1); }
function songIns() { if (songList.length >= SONG_MAX) { showToast('SONG', 'FULL', 1); return; } const e = songList[songSlot] || [0, 4, 1, 0, 0, 0]; songList.splice(songSlot + 1, 0, e.slice()); songSlot++; songSent = -1; songOff = 0; songNext = -1; showToast('SONG', 'STEP ' + (songSlot + 1) + ' ADDED', songSlot / (SONG_MAX - 1)); dirty = true; needsRedraw = true; }
function songDel() { if (songList.length <= 1) { songList = [[0, 4, 1, 0, 0, 0]]; songSlot = 0; songBlank = true; showToast('SONG', 'CLEARED', 0); } else { songList.splice(songSlot, 1); songSlot = clamp(songSlot, 0, songList.length - 1); showToast('SONG', 'STEP DELETED', 0); } songSent = -1; songOff = 0; songNext = -1; dirty = true; needsRedraw = true; }
function songMove(d) { const j = songSlot + d; if (j < 0 || j >= songList.length) return; const t = songList[j]; songList[j] = songList[songSlot]; songList[songSlot] = t; songSlot = j; songSent = -1; songOff = 0; songNext = -1; showToast('SONG', 'STEP ' + (j + 1), j / (SONG_MAX - 1)); dirty = true; needsRedraw = true; }
function songTotal() { let t = 0; for (const e of songList) t += songLen(e); return Math.max(1, t); }
function songStart(k) { let a = 0; for (let i = 0; i < k && i < songList.length; i++) a += songLen(songList[i]); return a; }
function songEntryAt(bar) { const tot = songTotal(); let b = (((bar + songOff) % tot) + tot) % tot, a = 0; for (let i = 0; i < songList.length; i++) { if (b < a + songLen(songList[i])) return [i, b === a]; a += songLen(songList[i]); } return [0, true]; }
function songRestore() {                                       /* tempo and mutes the song changed go back to the project's own */
    if (songTempoOn) { songTempoOn = false; dspSet('bpm', String(bpm)); }
    if (songMuted) { songMuted = false; for (let t = 0; t < NTRK; t++) dspSet('mute', t + ',' + mutes[t]); }
}
function songEnter(k) {                                         /* an entry begins: its scene, its tempo, its way in */
    const e = songList[k];
    if (songMuted) { songMuted = false; for (let t = 0; t < NTRK; t++) dspSet('mute', t + ',' + mutes[t]); }
    if (e[3]) { songTempoOn = true; dspSet('bpm', String(e[3])); } else if (songTempoOn) { songTempoOn = false; dspSet('bpm', String(bpm)); }
    if (e[0] !== songLaunched || true) { songLaunched = e[0]; launchScene(e[0], true); }
    songMutePend = e[4] === 2;
}
function songFill() { const keep = sel; for (let t = 0; t < NTRK; t++) if (kind[t]) { sel = t; fillBar(true); } sel = keep; }
function songTick() {
    if (!songOn || !playing) { if (!playing) { songSent = -1; songLaunched = -1; songOff = 0; songFillBar = -1; } songMutePend = false; if (songMuted || songTempoOn) songRestore(); return; }
    const g = Math.max(0, gs - 1), bar = Math.floor(g / 16), st = g % 16;
    if (songSent < 0 && g <= 1) { songSent = 0; songOff = 0; songNext = -1; songEnter(0); return; }
    if (songSent < 0) return;
    const nr = songEntryAt(bar + 1), cur = songEntryAt(bar);
    if (st < 12 && songFillBar !== bar) {                         /* the last bar before an entry with FILL: a roll on the drums */
        songFillBar = bar;
        const nxt = songNext >= 0 ? songNext : nr[0];
        const first = songNext >= 0 || nr[1];
        if (first && nxt !== cur[0] && songList[nxt] && songList[nxt][4] === 1) songFill();
        if (songMutePend) { songMutePend = false; songMuted = true; songMuteBar = bar; for (let t = 0; t < NTRK; t++) if (!kind[t]) dspSet('mute', t + ',1'); }
        else if (songMuted && bar >= songMuteBar + 2) { songMuted = false; for (let t = 0; t < NTRK; t++) dspSet('mute', t + ',' + mutes[t]); }
    }
    if (st >= 12) {
        const nb = bar + 1;
        if (songSent === nb) return;
        songSent = nb;
        if (songNext >= 0) {                                       /* a pad jump waits for the end of the bar */
            const k = songNext; songNext = -1;
            songOff = ((songStart(k) - nb) % songTotal() + songTotal()) % songTotal();
            songEnter(k); return;
        }
        if (!songLoop && nb >= songTotal()) { songOn = false; songSent = -1; songRestore(); showToast('SONG', 'END', 0); dirty = true; return; }
        const r = songEntryAt(nb);
        if (r[1]) songEnter(r[0]);
    }
}
let songFromClips = false, songBlank = true, songDelOn = false, songDelUsed = false, songCopySrc = -1;
function songOpen(fromClips) { symOpen = -1; navStack = []; padMenu = false; menuOpen = false; songFromClips = !!fromClips; view = 31; acc.fill(0); showToast('SONG', 'STEPS 1-8 = ADD SCENE', 0.5); needsRedraw = true; }
const SONG_BARS = [1, 2, 4, 8, 16, 32];
function songStepKey(i) {                                       /* in the song: steps 1-8 add scene 1-8, 9-14 = bars, 15 = copy, 16 = song on / off */
    const e = songList[songSlot]; if (!e) return;
    if (i < 8) {
        const bars = e[1], rep = e[2];
        if (songBlank) { songBlank = false; e[0] = i; e[5] = 0; }
        else { if (songList.length >= SONG_MAX) { showToast('SONG', 'FULL', 1); return; } songList.splice(songSlot + 1, 0, [i, bars, rep, 0, 0, 0]); songSlot++; }
        songSent = -1; songOff = 0; songNext = -1; showToast('SONG ' + p2(songSlot + 1), 'SCENE ' + (i + 1) + ' x ' + songList[songSlot][1] + ' BARS', i / 7);
    } else if (i < 14) { e[1] = SONG_BARS[i - 8]; songBlank = false; songSent = -1; showToast('SONG ' + p2(songSlot + 1), e[1] + ' BARS', (i - 8) / 5); }
    else if (i === 14) { songIns(); }
    else { songOn = !songOn; songLoop = true; songSent = -1; songLaunched = -1; songOff = 0; songNext = -1; if (!songOn) songRestore(); showToast('SONG', songOn ? 'ON - LOOP' : 'OFF', songOn ? 1 : 0); }
    dirty = true; needsRedraw = true;
}
function songPad(k) {                                           /* pad k = song step k: select, or (song running) jump there at the end of the bar */
    const L = songList.length;
    if (deleteHeld && k < L) { songDelUsed = true; songSlot = k; songDel(); return; }
    if (copyHeld && k <= L) {                                      /* Copy + step pad: pick a step, then Copy + another pad pastes a copy there (the others move along) */
        if (songCopySrc < 0 || songCopySrc >= L) { if (k < L) { songCopySrc = k; showToast('SONG', 'COPY STEP ' + (k + 1) + ' > PICK SPOT', 0.5); } return; }
        if (L >= SONG_MAX) { showToast('SONG', 'FULL', 1); return; }
        songList.splice(k, 0, songList[songCopySrc].slice()); songSlot = k; songSent = -1; songOff = 0; songNext = -1; songBlank = false; dirty = true; needsRedraw = true;
        showToast('SONG', 'PASTED AT STEP ' + (k + 1), k / (SONG_MAX - 1)); return;
    }
    if (shiftHeld && k < L) { songSlot = k; songIns(); return; }
    if (k < L) {
        songSlot = k; needsRedraw = true;
        if (songOn && playing) { songNext = k; showToast('SONG', 'NEXT: STEP ' + (k + 1), k / (SONG_MAX - 1)); }
        else showToast('SONG', 'STEP ' + (k + 1) + (songList[k][5] ? ' ' + SONG_TAG[songList[k][5]] : ''), k / (SONG_MAX - 1));
    } else if (k === L && L < SONG_MAX) { songSlot = L - 1; songIns(); }
}
function songPadK(i) { return (3 - (i >> 3)) * 8 + (i & 7); }
function songLed(i) {
    const k = songPadK(i), L = songList.length;
    if (k > L || k >= SONG_MAX) return 0;
    if (k === L) return 124;
    const pl = songPlayIdx(), e = songList[k], c = TAG_COL[e[5]];
    if (k === songNext) return playing && (ledAge & 4) ? 126 : c;
    if (k === pl) return 126;
    return k === songSlot ? 120 : c;
}
/* ---- motion recording: knob moves are stored per clip / lane / parameter in quarter steps and replayed ---- */
let AUTO = {}, autoLast = {}, autoHold = {};
function autoSub() { return Math.floor(((posFr || 0)) / 25) & 3; }
let posFr = 0;
function autoRec(L, key, v) {
    const c = ed[laneTrk(L)], id = c + '|' + L + '|' + key, len = effLen(L);
    const p = ((gs - 1 + 1024 * len) % len) * 4 + autoSub();
    pSnap('auto', 0, 'rec');
    if (!AUTO[id]) AUTO[id] = {};
    AUTO[id][p] = Math.round(v * 1000) / 1000; autoHold[id] = 25; dirty = true;
}
function autoClear(L, key) { const c = ed[laneTrk(L)], id = c + '|' + L + '|' + key; if (AUTO[id]) { pSnap('auto', 0, 'clr'); delete AUTO[id]; dirty = true; showToast('AUTOMATION', 'CLEARED', 0); } else showToast('AUTOMATION', 'NONE HERE', 0); }
function autoClearClip(t, c) { pSnap('auto', 0, 'clr'); for (const id in AUTO) { const a = id.split('|'); if (+a[0] === c && laneTrk(+a[1]) === t) delete AUTO[id]; } }
function autoPlay() {
    if (!playing) { autoLast = {}; return; }
    for (const id in AUTO) {
        const a = id.split('|'), c = +a[0], L = +a[1], key = a[2];
        if (ed[laneTrk(L)] !== c) continue;
        if (autoHold[id] > 0) { autoHold[id]--; continue; }
        const len = effLen(L), n = len * 4, cur = (((gs - 1 + 1024 * len) % len) * 4 + autoSub()) % n;
        const last = autoLast[id] === undefined ? cur : autoLast[id];
        if (last === cur) { if (autoLast[id] === undefined) autoLast[id] = cur; continue; }
        let hit = null, j = last;
        for (let k = 0; k < n; k++) { j = (j + 1) % n; if (AUTO[id][j] !== undefined) hit = AUTO[id][j]; if (j === cur) break; }
        autoLast[id] = cur;
        if (hit !== null && PI[key] !== undefined) { pads[L][PI[key]] = hit; enqueue('p' + L + key, 'p' + p2(L + 1) + '_' + key, hit.toFixed(3)); needsRedraw = true; }
    }
}
let slm = [], slN = 16, slSel = 0, smpPage = 0;       /* smpPage 0 = SAMPLE edit, 1 = SLICE markers, 2 = ENV (same editor, Jog click cycles) */
function slEqual(n) { slN = n; slm = []; for (let k = 0; k < n; k++) slm.push(k / n); slSel = clamp(slSel, 0, n - 1); }
slEqual(16);
function sliceAuto() {
    const i = smpIdx(ln()), r = (smpInfo[i] && smpInfo[i][0]) ? dspGet('smpo_' + i) : null;
    if (typeof r !== 'string' || !r.length) { showToast('AUTO SLICE', 'NO HITS FOUND', 0); return; }
    const p = r.split(',').map(x => (parseInt(x) || 0) / 1000).filter(x => x > 0.002 && x < 0.998).slice(0, 15);
    slm = [0].concat(p); slN = slm.length; slSel = 0;
    showToast('AUTO SLICE', slN + ' SLICES', slN / 16);
}
function sliceOnsets() { const i = smpIdx(ln()), r = (smpInfo[i] && smpInfo[i][0]) ? dspGet('smpo_' + i) : null; return (typeof r === 'string' && r.length) ? r.split(',').map(x => (parseInt(x) || 0) / 1000).filter(x => x > 0.002 && x < 0.998) : []; }
function sliceAdd() {                                      /* new marker in the middle between the selected one and the next */
    if (slN >= 16) { showToast('SLICE', 'MAX 16', 0); return; }
    const a = slm[slSel], b = slSel < slN - 1 ? slm[slSel + 1] : 1;
    slm.splice(slSel + 1, 0, (a + b) / 2); slN = slm.length; slSel++; showToast('MARKER ADDED', slN + ' SLICES', slN / 16); needsRedraw = true;
}
function sliceDel() {
    if (slN <= 1) { showToast('SLICE', 'LAST ONE STAYS', 0); return; }
    if (slSel === 0) { slm.splice(0, 1); slm[0] = 0; } else slm.splice(slSel, 1);
    slN = slm.length; slSel = clamp(slSel, 0, slN - 1); showToast('MARKER DELETED', slN + ' SLICES', slN / 16); needsRedraw = true;
}
function sliceSnap() {                                     /* marker jumps to the nearest hit of the sample (or the 1/16 grid when no hits were found) */
    if (slSel === 0) { slm[0] = 0; showToast('SNAP', 'START = 0', 0); needsRedraw = true; return; }
    const lo = slm[slSel - 1] + 0.002, hi = slSel < slN - 1 ? slm[slSel + 1] - 0.002 : 0.998;
    const on = sliceOnsets().filter(x => x >= lo && x <= hi); let best = -1, bd = 1;
    for (const x of on) { const d = Math.abs(x - slm[slSel]); if (d < bd) { bd = d; best = x; } }
    if (best >= 0 && bd < 0.12) { slm[slSel] = best; showToast('SNAP', 'TO HIT', 1); }
    else { const g = clamp(Math.round(slm[slSel] * 16) / 16, lo, Math.max(lo, hi)); slm[slSel] = g; showToast('SNAP', 'TO GRID', 0.5); }
    needsRedraw = true;
}
function smpTool(what, d) {                                /* tools page: quick things to do with the region of the sample */
    const L = ln(), i = smpIdx(L);
    if (!smpInfo[i] || !smpInfo[i][0]) { showToast('NO SAMPLE', 'PICK IN SAMPLE', 0); return; }
    if (Math.round(pads[L][0]) !== 359) setLaneP(L, 'type', 359);
    const st = pads[L][PI.sst], en = pads[L][PI.sen], len = Math.max(0.002, en - st), total = smpInfo[i][1] ? smpInfo[i][0] / smpInfo[i][1] : 0;
    if (what === 'smhf') { setLaneP(L, 'sen', st + len / 2); showToast('REGION', 'HALF', 0.5); }
    else if (what === 'smdb') { const nl = Math.min(1, len * 2), ns = clamp(st, 0, 1 - nl); setLaneP(L, 'sst', ns); setLaneP(L, 'sen', ns + nl); showToast('REGION', 'DOUBLE', 1); }
    else if (what === 'next') { const ns = clamp(st + d * len, 0, 1 - len); setLaneP(L, 'sst', ns); setLaneP(L, 'sen', ns + len); showToast('REGION', Math.round(ns * 100) + ' - ' + Math.round((ns + len) * 100), ns); }
    else if (what === 'smrn') { const ns = Math.random() * (1 - len); setLaneP(L, 'sst', ns); setLaneP(L, 'sen', ns + len); showToast('REGION', 'RANDOM ' + Math.round(ns * 100), ns); }
    else if (what === 'smrv') { const on = pads[L][PI.smd] > 0.25 && pads[L][PI.smd] < 0.75; setLaneP(L, 'smd', on ? 0 : 0.5); showToast('REVERSE', on ? 'OFF' : 'ON', on ? 0 : 1); }
    else if (what === 'smnm') sampleNormalize();
    else if (what === 'smrs') { setLaneP(L, 'sst', 0); setLaneP(L, 'sen', 1); setLaneP(L, 'sstr', 0.5); setLaneP(L, 'smd', 0); showToast('SAMPLE', 'RESET', 0); }
    else if (what === 'smfit') {
        const secs = len * total; if (secs < 0.02) { showToast('FIT', 'REGION TOO SHORT', 0); return; }
        let bn = 0, bf = 0, be = 9;
        for (const n of [0.25, 0.5, 1, 2, 4, 8, 16]) { const f = (n * 60 / bpm) / secs, e = Math.abs(Math.log2(f)); if (f >= 0.5 && f <= 2 && e < be) { be = e; bn = n; bf = f; } }
        if (!bn) { showToast('FIT', 'TOO FAR: HALF/DOUBLE', 0); return; }
        setLaneP(L, 'sstr', clamp(0.5 + Math.log2(bf) / 2, 0, 1)); showToast('FIT ' + bn + ' BEAT' + (bn === 1 ? '' : 'S'), bf.toFixed(2) + 'x AT ' + bpm, 1);
    }
    needsRedraw = true;
}
function sliceApply() {
    const L0 = ln(), i = smpIdx(L0);
    if (!smpInfo[i] || !smpInfo[i][0]) { showToast('NO SAMPLE', 'PICK IN SAMPLE', 0); return; }
    if (kind[sel]) {
        for (let k = 0; k < slN; k++) { const l = laneOf(sel, k); setLaneP(l, 'type', 359); setLaneP(l, 'smp', i / 63); setLaneP(l, 'sst', slm[k]); setLaneP(l, 'sen', k < slN - 1 ? slm[k + 1] : 1); setLaneP(l, 'smd', 0); }
        showToast('SLICES SET', slN + ' PADS', slN / 16);
    } else {
        setLaneP(L0, 'type', 359); setLaneP(L0, 'sst', slm[slSel]); setLaneP(L0, 'sen', slSel < slN - 1 ? slm[slSel + 1] : 1);
        showToast('SLICE ' + (slSel + 1), 'ON THIS TRACK', slSel / 15);
    }
    needsRedraw = true;
}
function drawSlice() {
    const L = ln(), i = smpIdx(L), ok = smpInfo[i] && smpInfo[i][0] > 0;
    titleBar(p2(sel + 1) + ' ' + laneName(), 'SLICE');
    { const sc = ok && smpInfo[i][1] ? (smpInfo[i][0] / smpInfo[i][1]) : 0, tg = ok ? (slSel + 1) + '/' + slN + ' ' + (sc * ((slSel < slN - 1 ? slm[slSel + 1] : 1) - slm[slSel]) * 1000).toFixed(0) + 'ms' : '';
      txt(1, 10, fit(ok ? smpNames[i] : 'NO SAMPLE (SAMPLE PAGE)', 124 - tw(tg) - 2), 1); if (tg) txt(127 - tw(tg), 10, tg, 1); }
    draw_rect(0, 19, 128, 13, 1);
    if (ok) {
        if (smpWave[i] === undefined) { const w = dspGet('smpw_' + i); smpWave[i] = typeof w === 'string' ? w : ''; }
        const w = smpWave[i];
        for (let c = 0; c < w.length && c < 64; c++) { const h = Math.round((w.charCodeAt(c) - 97) / 25 * 5); fill_rect(c * 2, 25 - h, 2, 2 * h + 1, 1); }
    }
    for (let k = 0; k < slN; k++) {
        const x = clamp(Math.round(slm[k] * 127), 0, 126);
        if (k === slSel) { fill_rect(x, 19, 3, 13, 1); } else fill_rect(x, 19, 1, 13, 1);
    }
    miniGrid(VIEW_ITEMS[29], 34, (key, v) => key === 'slp' ? v : (key === 'slm' ? slSel / 15 : (key === 'sln' ? (slN - 1) / 15 : fracVal(key, v))));
}
function sampleNormalize() {
    const L = ln(), i = smpIdx(L), r = (smpInfo[i] && smpInfo[i][0]) ? dspGet('smpp_' + i) : null, pk = typeof r === 'string' ? (parseInt(r) || 0) : 0;
    if (pk < 50) { showToast('NORMALIZE', 'NO SAMPLE', 0); return; }
    const g = 0.9 * 32768 / pk, v = clamp(0.5 + Math.log2(g) / 6, 0, 1);
    setLaneP(L, 'sgn', v); showToast('NORMALIZE', (20 * Math.log10(g)).toFixed(1) + ' dB', v);
}
function miniGrid(items, y0, frFn) {                         /* up to 8 knobs in two rows of four (label, value, bar) */
    for (let k = 0; k < items.length && k < 8; k++) {
        const key = items[k][0], v = getVal(scopeOf(key), key), fr = frFn(key, v);
        const cx = (k & 3) * 32 + 1, cy = y0 + (k >> 2) * 16;
        txt(cx + 1, cy, fit(items[k][2] || items[k][1], 30), 1); txtB(cx + 1, cy + 7, fit(fmtVal(key, v), 30), 1);
        fill_rect(cx + 1, cy + 14, 28, 1, 1); const w = Math.round(28 * clamp(fr, 0, 1)); if (w > 0) fill_rect(cx + 1, cy + 13, w, 2, 1);
    }
}
function drawSample() {
    const L = ln(), i = smpIdx(L), ok = smpInfo[i] && smpInfo[i][0] > 0;
    titleBar(p2(sel + 1) + ' ' + laneName(), (smpPage === 2 ? 'ENV ' : (smpPage === 3 ? 'TOOLS ' : 'SAMPLE ')) + smpMem + '%');
    const isS = Math.round(pads[L][0]) === 359, nm = ok ? (isS ? smpNames[i] : smpNames[i] + ' (off)') : (smpLoaded().length ? '(Jog: pick sample)' : 'NO WAV FILES');
    const sst = pads[L][PI.sst], sen = pads[L][PI.sen];
    let tag = ''; if (ok) { const sc = smpInfo[i][1] ? (smpInfo[i][0] / smpInfo[i][1]) : 0, md = Math.round(pads[L][PI.smd] * 2); tag = (sc * (sen - sst)).toFixed(2) + 's ' + ['', 'REV', 'LOOP'][md] + (Math.abs(pads[L][PI.sstr] - 0.5) > 0.012 ? ' ST' : ''); }
    txt(1, 10, fit(nm, 124 - tw(tag) - 2), 1); if (tag) txt(127 - tw(tag), 10, tag, 1);
    draw_rect(0, 19, 128, 13, 1);
    if (ok) {
        if (smpWave[i] === undefined) { const w = dspGet('smpw_' + i); smpWave[i] = typeof w === 'string' ? w : ''; }
        const w = smpWave[i], xs = Math.round(sst * 127), xe = Math.round(sen * 127);
        for (let c = 0; c < w.length && c < 64; c++) {
            const h = Math.round((w.charCodeAt(c) - 97) / 25 * 5), x = c * 2;
            if (x >= xs && x <= xe) fill_rect(x, 25 - h, 2, 2 * h + 1, 1); else fill_rect(x, 25, 2, 1, 1);
        }
        fill_rect(xs, 19, 1, 13, 1); fill_rect(xe, 19, 1, 13, 1);
    }
    const items = VIEW_ITEMS[smpPage === 2 ? 32 : (smpPage === 3 ? 48 : 28)];
    miniGrid(items, 34, (key, v) => key === 'smp' ? (smpLoaded().length ? smpLoaded().indexOf(i) / Math.max(1, smpLoaded().length - 1) : 0) : fracVal(key, v));
}
function drawSeq() {
    if (heldStep >= 0 && !recDown) { drawStepEdit(); return; }
    titleBar('C' + (ed[sel] + 1) + '  ' + bpm + ' BPM', recArmed ? (ciBeats > 0 ? 'COUNT ' + ciBeats : (playing ? 'REC' : 'REC ARM')) : ((topModes[sel] === 1 ? 'RPT ' + REP_NAME[repRate] : TOP_NAMES[topModes[sel]]) + (playing ? ' >' : '')));
    const star = llen[ln()] ? '*' : '', pgTxt = LL() > 16 ? (stepPage + 1) + '/' + pageCount() + ' L' + LL() + star : 'L' + LL() + star;
    txt(0, 11, fit(p2(sel + 1) + ' ' + laneName() + (mutes[sel] ? ' M' : ''), 120 - tw(pgTxt)), 1);
    txt(126 - tw(pgTxt), 11, pgTxt, 1);
    const cur = playing ? ((gs - 1 + 1024 * LL()) % LL()) : -1;
    for (let i = 0; i < 16; i++) {
        const s = stepPage * 16 + i, x = i * 8;
        if (s >= LL()) { set_pixel(x + 3, 30, 1); continue; }
        const st = steps[ed[sel]][ln()][s];
        if (st[0]) { const h = 5 + Math.round(st[1] / 127 * 12); fill_rect(x, 33 - h + 1, 7, h, 1); if (st[3] > 0) { fill_rect(x + 2, 33 - h + 3, 3, 1, 0); } }
        else { fill_rect(x, 32, 7, 2, 1); }
        if (st[0] && st[9] > 0) fill_rect(x, 38, Math.min(st[9] * 8 - 1, (16 - i) * 8 - 1), 2, 1);
        else if (st[0] && st[5]) fill_rect(x, 38, [0, 2, 3, 4, 6, 7, 7, 7][st[5]], 2, 1);      /* gate length of the step (nothing = auto) */
        if (s === cur) fill_rect(x, 36, 7, 2, 1);
        else if (i % 4 === 0) set_pixel(x + 3, 37, 1);
        if (heldGrp.includes(s)) draw_rect(x > 0 ? x - 1 : 0, 15, x > 0 ? 9 : 8, 21, 1);
        if (s === heldStep || (recDown && !playing && s === recHead % LL())) draw_rect(x > 0 ? x - 1 : 0, 15, x > 0 ? 9 : 8, 21, 1);
    }
    if (false) {
        txt(0, 40, '1-8 CLIP  9 RND 10 CLR', 1);
        txt(0, 49, '11 REV 12< 13> 14 x2', 1);
        txt(0, 58, '15 /2  16 VEL~', 1);
    } else if (heldStep >= 0) {
        const st = stepAt(heldStep);
        cell(0, 40, 'Vel', String(st[1]), st[1] / 127);
        cell(32, 40, 'Pitch', (st[2] > 0 ? '+' : '') + st[2], (st[2] + 64) / 127);
        cell(64, 40, 'Prob', PROB_PCT[st[3]] + '%', PROB_PCT[st[3]] / 100);
        txt(100, 40, 'S' + (heldStep + 1), 1);
    } else if (copyHeld || deleteHeld || loopHeld) {
        txt(0, 42, copyHeld ? (stepClip ? 'COPY: STEP ' + (stepClip.s + 1) + ' PICKED' : 'COPY: PICK A STEP') : (deleteHeld ? 'DELETE HELD' : 'LOOP HELD'), 1);
        txt(0, 53, copyHeld ? (stepClip ? 'PRESS TARGET STEPS' : 'THEN PRESS TARGET') : (deleteHeld ? 'STEP = CLEAR IT' : 'STEP = LOOP END'), 1);
    } else if (!shiftHeld) {
        for (let i = 0; i < 8; i++) {
            const k = seqSnd()[i], v = pads[ln()][PI[k]], cx = i * 16 + 1;
            txtS(cx, 41, seqShort()[i], 1); txtS(cx, 49, fit(fmtVal(k, v), 15).toUpperCase(), 1);
            fill_rect(cx, 58, 14, 1, 1); const w = Math.round(14 * clamp(fracVal(k, v), 0, 1)); if (w > 0) fill_rect(cx, 57, w, 3, 1);
        }
    } else {
        const sc = (x, l, v, f, k) => { if (uiTheme >= 2) themedCell(x, 40, l, v, f, 32, false, k); else cell(x, 40, l, v, f); };
        sc(0, 'BPM', String(bpm), (bpm - 40) / 200, 'bpm');
        sc(32, 'Swg', String(50 + swing), swing / 25, 'swing');
        sc(64, 'Len', String(LL()), LL() / MAXSTEP, 'len');
        sc(96, 'Clip', String(ed[sel] + 1), ed[sel] / (NPAT - 1), 'clip');
    }
}
function drawExit() {
    titleBar('LEAVE TAKTILE', playing ? 'PLAYING' : 'STOPPED'); txt(4, 57, dirty ? 'SAVES ON LEAVE' : 'ALL SAVED', 1);
    const L = [['BACKGROUND', 'plays on'], ['CLOSE', 'stops']];
    for (let i = 0; i < 2; i++) {
        const y = 14 + i * 22, on = exitDlg === i;
        if (on) fill_rect(2, y, 124, 19, 1); else draw_rect(2, y, 124, 19, 1);
        txt(8, y + 6, L[i][0], on ? 0 : 1); txt(122 - tw(L[i][1]), y + 6, L[i][1], on ? 0 : 1);
    }
}
function drawAsk() {
    titleBar('TRACK ' + p2(askKind + 1), 'NEW');
    txt(4, 13, 'What kind of track?', 1);
    for (let i = 0; i < 2; i++) {
        const x = 4 + i * 62, on = askSel === i, c = on ? 0 : 1;
        if (on) fill_rect(x, 26, 58, 20, 1); else draw_rect(x, 26, 58, 20, 1);
        txt(x + 29 - Math.floor(tw(i ? 'DRUM' : 'SYNTH') / 2), 32, i ? 'DRUM' : 'SYNTH', c);
    }
    txt(2, 52, 'Jog+Click or pad L/R', 1);
}
function askDone(k) {                                       /* the answer opens the next window: drum = kit styles, synth = sound categories > presets */
    const t = askKind; askKind = -1; chosen[t] = 1; if (k >= 0) setKind(t, k); if (k === 1) kitFresh[t] = true; if (k === 0) synFresh[t] = true; markEdited(); needsRedraw = true;
    if (k >= 0 && t === sel) { fbOpen(); if (kind[sel]) { const ki = fb.st[0].node.kids.findIndex(q => q.n === 'DRUMS'); if (ki >= 0) { const rt = fb.st[0].node; fb.st = [{ node: rt, i: ki }, { node: rt.kids[ki], i: 0 }]; } } }
}
function clipBars(c, t, x, y) {                            /* nine little bars: how much happens in each ninth of the clip */
    const n = lens[t], ls = lanesOf(t);
    for (let i = 0; i < 9; i++) {
        const s0 = Math.floor(i * n / 9), s1 = Math.max(s0 + 1, Math.floor((i + 1) * n / 9)); let cnt = 0;
        for (const l of ls) for (let s = s0; s < s1 && s < MAXSTEP; s++) if (steps[c][l][s] && steps[c][l][s][0]) cnt++;
        if (cnt) { const h = Math.min(7, 2 + cnt); fill_rect(x + 1 + i, y + 9 - h, 1, h - 1 > 0 ? h - 1 : 1, 1); }
    }
}
function drawClips() {
    titleBar('CLIPS ' + (bank * 4 + 1) + '-' + (bank * 4 + 4), sceneNames[ed[sel]] ? 'C' + (ed[sel] + 1) + ' ' + sceneNames[ed[sel]] : (playing ? 'PLAY' : 'STOP'));
    for (let r = 0; r < 4; r++) {
        const t = bank * 4 + r, y = 11 + r * 13, isSel = (t === sel);
        const nm = kind[t] ? 'DRM' + (slot[t] + 1) : 'S' + p2(t + 1);
        if (isSel) { fill_rect(0, y, 25, 11, 1); txt(1, y + 1, nm, 0); } else txt(1, y + 1, nm, 1);
        if (mutes[t]) fill_rect(0, y + 5, 25, 1, isSel ? 0 : 1);
        for (let c = 0; c < NPAT; c++) {
            const x = 27 + c * 12, has = trackHasSteps(c, t), cur = clipsCur[t] === c, q = clipsQ[t] === c;
            if (cur) {
                fill_rect(x, y, 11, 10, 1);
                if (playing) { const len = lens[t], prog = Math.round(9 * (((gs % len) + len) % len) / len); fill_rect(x + 1 + prog, y + 8, 1, 2, 0); }
                else { set_pixel(x + 4, y + 3, 0); set_pixel(x + 4, y + 4, 0); set_pixel(x + 4, y + 5, 0); set_pixel(x + 5, y + 4, 0); }
            } else if (q) { draw_rect(x, y, 11, 10, 1); fill_rect(x + 3, y + 3, 5, 4, 1); }
            else if (has) { draw_rect(x, y, 11, 10, 1); clipBars(c, t, x, y); }
            else { set_pixel(x + 5, y + 4, 1); set_pixel(x + 5, y + 5, 1); }
            if (fol[t][c] & 7) { const mc = cur ? 0 : 1; fill_rect(x + 7, y + 1, 3, 1, mc); set_pixel(x + 9, y + 2, mc); }
            if (isSel && ed[t] === c) fill_rect(x + 2, y + 11, 7, 1, 1);
        }
    }
}

function openHub(v) { navStack = []; hubReturn = isHub(view) ? hubReturn : (view === 7 ? 0 : view); padMenu = false; menuOpen = false; symOpen = -1; view = v || 16; acc.fill(0); needsRedraw = true; }
function hubMove(dl) { const n = HUBS[view][1].length; hubSel[view] = clamp(hubSel[view] + (dl > 0 ? 1 : -1), 0, n - 1); needsRedraw = true; }
function hubEnter() {
    const t = HUBS[view][1][hubSel[view]];
    if (t[1] === -1) { menuOpen = true; quickRebuild(); menuIdx = 0; menuGrp = -1; menuSub = 0; needsRedraw = true; return; }
    navStack.push(view); view = t[1]; acc.fill(0); needsRedraw = true;
    { const tt = trackTabs(), lt = tt.indexOf(lastTab[sel]) >= 0 ? lastTab[sel] : (tt.indexOf(1) >= 0 ? 1 : tt[0]); if (view === 1 && lt !== 1) { const w = lt === 9 ? lastTabId() : lt; tabEnter(w); } }
    if (view === 28) { smpPage = 0; broOpen = false; sampleRescan(); }
    if (view === 36) euSetPage(euPage);
    if (view === 35) projScan();
    if (view === 34) kitsLoad();
}
function hubBack() {                                       /* one level up; from the top hub back to where you came from */
    if (navStack.length) view = navStack.pop(); else { view = hubReturn; symOpen = -1; }
    acc.fill(0); needsRedraw = true;
}
const SHORTL = { Depth: 'Dpth', Shape: 'Shp', Pitch: 'Ptch', Level: 'Lvl', Layers: 'Lyrs', Chord: 'Chrd', Speed: 'Spd', Feedbk: 'Fdbk', Space: 'Spc', Range: 'Rng', Chance: 'Chnc', Amount: 'Amt', Target: 'Trgt', Octave: 'Oct', Semis: 'Semi', Voicing: 'Voic', Effect: 'FX', Param: 'Prm', VelBias: 'VelB', ArpRate: 'Rate' };      /* names on the effect tiles: always the normal letters */
const XL = [null, ['Mix', 'Div', 'Hold'], ['Mix', 'Div', 'Hold'], ['Mix', 'Rate', 'Fdbk'], ['Depth', 'Rate', 'Wide'], ['Depth', 'Rate', 'Shape'], ['Mix', 'Pitch', 'Fdbk'], ['Mix', 'Rate', 'Reso'], ['Depth', 'Div', 'Soft'], ['Rate', 'Bits', 'Mix'], ['Reso', 'Pos', '-']];
const ML = [null, ['Level', 'Layers'], ['Level', 'Chord'], ['Level', 'Rate'], ['Level', 'Speed'], ['Feedbk', 'Space'], ['Range', 'Chance'], ['Range', 'Chance'], ['Amount', 'Target'], ['Level', 'Octave'], ['Level', 'Semis'], ['Level', 'Voicing']];
const CHORD_NAME = ['MAJ', 'MIN', 'SUS2', 'SUS4', '7', 'MIN7', 'MAJ7', 'DIM'], OCT_NAME = ['-1', '+1', '+/-1', '+1+2'], ARP_RATE = ['1/16', '1/8', '1/32'], DIA_NAME = ['TRIAD', '7TH', '9TH', 'OPEN'];
function itemLabel(v, i, nn) {                                 /* MULTI / MIDI pages: labels follow the chosen effect */
    const it = VIEW_ITEMS[pg2(v)][i];
    if (v === 27 && i > 0 && i < 3) { const t = Math.round(pads[ln()][fxPage ? PI.m2fx : PI.mfx]), n = (ML[t] && ML[t][i - 1]) || ['Amt', 'Prm'][i - 1]; return SHORTL[n] || n; }
    if (v === 27) { const n = i === 0 ? (it[2] || it[1]) : (it[2] || it[1]); return SHORTL[n] || n; }
    if (v !== 24 || i === 0 || i > 3) return String(it[1]).length <= (nn || 7) ? it[1] : (it[2] || it[1]);
    const t = Math.round(pads[ln()][PI.xfx]), n = (XL[t] && XL[t][i - 1]) || ['Amt', 'Rate', 'Dpth'][i - 1] || '-';
    return SHORTL[n] || n;
}
let gSc = 1, gX0 = 0, gY0 = 0, gMeas = null, GMC = {};
function glyphBox(n) { if (GMC[n]) return GMC[n]; gMeas = { a: 999, b: -999 }; drawGlyph(n, 0, 0, 1); const r = [gMeas.a, gMeas.b]; gMeas = null; GMC[n] = r; return r; }
function drawGlyphAt(n, x, y, c) { const b = glyphBox(n); drawGlyph(n, x - b[0], y, c); }
function GF(x, y, w, h, c) { if (gMeas) { gMeas.a = Math.min(gMeas.a, x); gMeas.b = Math.max(gMeas.b, x + w - 1); return; } if (gSc === 1) { fill_rect_h(x, y, w, h, c); return; } fill_rect_h(gX0 + (x - gX0) * gSc, gY0 + (y - gY0) * gSc, w * gSc, h * gSc, c); }
function GP(x, y, c) { if (gMeas) { gMeas.a = Math.min(gMeas.a, x); gMeas.b = Math.max(gMeas.b, x); return; } if (gSc === 1) { set_pixel_h(x, y, c); return; } fill_rect_h(gX0 + (x - gX0) * gSc, gY0 + (y - gY0) * gSc, gSc, gSc, c); }
function GD(x, y, w, h, c) { if (gMeas) { gMeas.a = Math.min(gMeas.a, x); gMeas.b = Math.max(gMeas.b, x + w - 1); return; } if (gSc === 1) { draw_rect_h(x, y, w, h, c); return; } const X = gX0 + (x - gX0) * gSc, Y = gY0 + (y - gY0) * gSc, W = w * gSc, H = h * gSc; draw_rect_h(X, Y, W, H, c); draw_rect_h(X + 1, Y + 1, W - 2, H - 2, c); }
const fill_rect_h = (x, y, w, h, c) => fill_rect(x, y, w, h, c), set_pixel_h = (x, y, c) => set_pixel(x, y, c), draw_rect_h = (x, y, w, h, c) => draw_rect(x, y, w, h, c);
function drawGlyph(n, x, y, c) {
    if (n === 'UNDO') n = 'HISTORY';
    if (n === 'SCOPE') { for (let j = 0; j < 18; j++) set_pixel_h(x + j, y + 4 + Math.round(3.5 * Math.sin(j * 0.75)), c); return; }
    const fill_rect = GF, set_pixel = GP, draw_rect = GD; gX0 = x; gY0 = y;
    if (n === 'OPTIONS') { draw_rect(x + 5, y + 1, 7, 7, c); fill_rect(x + 8, y + 4, 1, 1, c); fill_rect(x + 7, y - 1, 3, 2, c); fill_rect(x + 7, y + 8, 3, 2, c); fill_rect(x + 3, y + 3, 2, 3, c); fill_rect(x + 12, y + 3, 2, 3, c); set_pixel(x + 4, y, c); set_pixel(x + 12, y, c); set_pixel(x + 4, y + 8, c); set_pixel(x + 12, y + 8, c); return; }
    if (n === 'HISTORY') { draw_rect(x + 3, y, 11, 9, c); fill_rect(x + 8, y + 2, 1, 4, c); fill_rect(x + 8, y + 5, 3, 1, c); return; }
    if (n === 'TRACK') { for (let j = 0; j < 3; j++) { fill_rect(x + 1, y + j * 3 + 1, 16, 1, c); fill_rect(x + 2 + ((j * 7) % 11), y + j * 3, 3, 3, c); } return; }
    if (n === 'SAMPLE') { for (let j = 0; j < 8; j++) { const hh = Math.round(4 * Math.abs(Math.sin(j * 1.1 + 0.4))) + 1; fill_rect(x + 1 + j * 2, y + 4 - hh + 1, 1, hh * 2 - 1, c); } fill_rect(x, y, 1, 9, c); fill_rect(x + 17, y, 1, 9, c); return; }
    if (n === 'EUCLID') { const P = [0, 1, 0, 1, 0, 0, 1, 0]; for (let j = 0; j < 8; j++) { if (P[j]) fill_rect(x + 1 + j * 2, y + 2, 2, 5, c); else fill_rect(x + 1 + j * 2, y + 4, 1, 1, c); } return; }
    if (n === 'MFX') { fill_rect(x + 3, y, 1, 9, c); fill_rect(x + 9, y, 1, 9, c); fill_rect(x + 15, y, 1, 9, c); fill_rect(x + 1, y + 2, 5, 2, c); fill_rect(x + 7, y + 5, 5, 2, c); fill_rect(x + 13, y + 1, 5, 2, c); return; }
    if (n === 'DUCK') { for (let j = 0; j < 18; j++) { const v = j < 3 ? 8 : Math.round(8 - 7 * Math.exp(-(j - 3) / 5)) ; set_pixel(x + j, y + 8 - (j < 3 ? 1 : 8 - v) , c); } fill_rect(x + 3, y, 1, 9, c); return; }
    if (n === 'SONG') { fill_rect(x + 1, y + 4, 4, 5, c); fill_rect(x + 6, y + 1, 4, 8, c); fill_rect(x + 11, y + 5, 3, 4, c); fill_rect(x + 15, y + 2, 3, 7, c); return; }
    if (n === 'RESAMP') { draw_rect(x + 2, y + 1, 13, 7, c); fill_rect(x + 12, y, 5, 3, 1 - c); fill_rect(x + 13, y + 1, 4, 1, c); fill_rect(x + 15, y, 1, 3, c); return; }
    if (n === 'KIT') { for (let a = 0; a < 2; a++) for (let b = 0; b < 3; b++) fill_rect(x + 2 + b * 5, y + a * 5, 4, 4, c); return; }
    if (n === 'PROJ') { draw_rect(x + 2, y + 2, 14, 7, c); fill_rect(x + 2, y, 6, 2, c); return; }
    if (n === 'SYNC') { for (let j = 0; j < 4; j++) { set_pixel(x + 2 + j, y + 1 + j, c); set_pixel(x + 2 + j, y + 7 - j, c); set_pixel(x + 9 + j, y + 1 + j, c); set_pixel(x + 9 + j, y + 7 - j, c); } fill_rect(x + 14, y + 1, 2, 7, c); return; }
    if (n === 'MIDI') { for (let j = 0; j < 4; j++) { fill_rect(x + 1 + j * 5, y + 7 - j * 2, 3, 2 + j * 2, c); } return; }
    if (n === 'EQ') { const H = [3, 7, 5, 8, 2]; for (let j = 0; j < 5; j++) fill_rect(x + 1 + j * 3, y + 9 - H[j], 2, H[j], c); return; }
    if (n === 'MULTI') { for (let j = 0; j < 3; j++) { fill_rect(x + 3 + j * 5, y + 1, 1, 8, c); fill_rect(x + 1 + j * 5, y + 2 + ((j * 3) % 5), 5, 2, c); } return; }
    if (n === 'SOUND') { for (let j = 0; j < 8; j++) fill_rect(x + j * 2, y + 7 - Math.round(5 * Math.abs(Math.sin(j * 0.9))) - 1, 1, Math.round(5 * Math.abs(Math.sin(j * 0.9))) + 1, c); }
    else if (n === 'FILTER') { fill_rect(x, y + 3, 9, 1, c); for (let j = 0; j < 7; j++) set_pixel(x + 9 + j, y + 3 + j, c); }
    else if (n === 'MOD') { for (let j = 0; j < 16; j++) set_pixel(x + j, y + 5 + Math.round(3 * Math.sin(j * 0.8)), c); }
    else if (n === 'SYNTH') { for (let j = 0; j < 16; j++) set_pixel(x + j, y + 8 - ((j * 2) % 9), c); }
    else if (n === 'FX') { for (let j = 0; j < 3; j++) fill_rect(x + 2 + j * 5, y + 7 - 2 - j * 2, 1, 2 + j * 2, c); fill_rect(x, y + 8, 18, 1, c); }
    else if (n === 'MASTER') { for (let j = 0; j < 4; j++) fill_rect(x + 2 + j * 4, y + 8 - (2 + j * 2), 3, 2 + j * 2, c); }
    else if (n === 'DELAY') { for (let j = 0; j < 4; j++) fill_rect(x + 2 + j * 5, y + 8 - (7 - j * 2), 2, 7 - j * 2, c); }
    else if (n === 'REVERB') { for (let j = 0; j < 3; j++) fill_rect(x + 2 + j * 5, y + 3 + j, 1, 5 - j, c); fill_rect(x, y + 8, 20, 1, c); }
    else if (n === 'CHORUS') { for (let j = 0; j < 16; j++) { set_pixel(x + j, y + 3 + Math.round(2 * Math.sin(j * 0.7)), c); set_pixel(x + j, y + 7 + Math.round(2 * Math.sin(j * 0.7 + 1.5)), c); } }
    else if (n === 'PHASER') { for (let j = 0; j < 4; j++) draw_rect(x + 2 + j * 4, y + 2 + (j % 2) * 2, 3, 5, c); }
    else if (n === 'MIX') { for (let j = 0; j < 4; j++) fill_rect(x + 2 + j * 4, y + 8 - (2 + ((j * 3) % 5) * 1), 3, 2 + ((j * 3) % 5), c); }
    else if (n === 'BUS') { fill_rect(x, y + 3, 7, 1, c); for (let j = 0; j < 4; j++) set_pixel(x + 7 + j, y + 3 + j, c); fill_rect(x + 11, y + 7, 7, 1, c); }
    else if (n === 'TEMPO') { for (let j = 0; j < 7; j++) set_pixel(x + 8 - Math.floor(j / 2), y + 7 - j, c); fill_rect(x + 8, y + 7, 7, 1, c); }
    else if (n === 'QUANT') { for (let j = 0; j < 4; j++) fill_rect(x + 2 + j * 4, y + 3 + (j % 2) * 3, 3, 3, c); }
    else if (n === 'OPTION') { for (let j = 0; j < 3; j++) fill_rect(x + 2, y + 2 + j * 3, 17, 1, c); }
    else if (n === 'MORE') { for (let j = 0; j < 3; j++) fill_rect(x + 4 + j * 5, y + 5, 2, 2, c); }
}
function drawHub() {
    const h = HUBS[view], n = h[1].length, pages = Math.ceil(n / 6), pg = Math.floor(hubSel[view] / 6);
    titleBar(p2(sel + 1) + ' ' + laneName(), h[0] + (pages > 1 ? ' ' + (pg + 1) + '/' + pages : ''));
    for (let k = 0; k < 6; k++) {                            /* 6 tiles per page: small picture on top, name below */
        const i = pg * 6 + k; if (i >= n) break;
        const nm = h[1][i][0], x = 2 + (k % 3) * 42, y = 10 + ((k / 3) | 0) * 27, on = i === hubSel[view], c = on ? 0 : 1, gb = glyphBox(nm), gw = gb[1] - gb[0] + 1;
        if (on) fill_rect(x, y, 40, 26, 1); else draw_rect(x, y, 40, 26, 1);
        drawGlyph(nm, x + Math.floor((40 - gw) / 2) - gb[0], y + 4, c);
        txt(x + 20 - Math.floor(tw(nm) / 2), y + 16, nm, c);
    }
    if (pages > 1) { for (let p = 0; p < pages; p++) { const dx = 64 - pages * 3 + p * 6; if (p === pg) fill_rect(dx, 62, 4, 2, 1); else draw_rect(dx, 62, 4, 2, 1); } }
}
function mixKnob(i, delta) {
    const t = mixPage * 8 + i;
    if (mixMode > 0) {
        const L = kind[t] ? laneOf(t, dsel[t]) : t, key = mixMode === 1 ? 'dly' : 'rvb';
        const v = clamp(pads[L][PI[key]] + delta * 0.02, 0, 1);
        pads[L][PI[key]] = v; enqueue('p' + L + key, 'p' + p2(L + 1) + '_' + key, v.toFixed(3)); storeCs(L); markEdited();
        showToast((mixMode === 1 ? 'DLY SEND T' : 'RVB SEND T') + p2(t + 1), String(Math.round(v * 100)), v); return;
    }
    const k = shiftHeld ? 'pan' : 'vol';
    trackKnob(t, k, delta * 0.02);
}
function drawMix() {
    titleBar('MIX ' + ['VOL', 'DLY', 'RVB'][mixMode], 'T' + (mixPage * 8 + 1) + '-' + (mixPage * 8 + 8));
    for (let i = 0; i < 8; i++) {
        const t = mixPage * 8 + i, x = i * 16, L2 = kind[t] ? laneOf(t, dsel[t]) : t, v = mixMode === 0 ? getTV(t) : pads[L2][PI[mixMode === 1 ? 'dly' : 'rvb']], h = Math.round(30 * v);
        draw_rect(x + 4, 12, 8, 32, 1);
        if (h > 0) fill_rect(x + 6, 12 + 30 - h + 1, 4, h, 1);
        const mh = Math.round(30 * clamp(meterV[t] / 99, 0, 1)); if (mh > 0) fill_rect(x + 13, 12 + 31 - mh + 1, 2, mh, 1);
        if (t === sel) fill_rect(x + 3, 46, 10, 9, 1);
        txt(x + 8 - Math.floor(tw(mutes[t] ? 'M' : String(t + 1)) / 2), 47, mutes[t] ? 'M' : String(t + 1), t === sel ? 0 : 1);
    }
    txt(1, 57, 'Jog click=Vol/Dly/Rvb', 1);
}

function songPlayIdx() { if (!songOn || !playing || songSent < 0) return -1; return songEntryAt(Math.max(0, Math.floor((gs - 1) / 16)))[0]; }
function drawSong() {
    const L = songList.length;
    titleBar('SONG ' + (songOn ? (songLoop ? 'LOOP' : 'ONCE') : 'OFF'), songTotal() + ' BARS');
    const rows = 4, st = clamp(songSlot - 2, 0, Math.max(0, L - rows)), pl = songPlayIdx(), tot = songTotal();
    for (let i = 0; i < rows; i++) {
        const k = st + i; if (k >= L) break;
        const y = 12 + i * 9, on = k === songSlot, e = songList[k];
        if (on) fill_rect(0, y, 122, 9, 1);
        const c = on ? 0 : 1;
        txt(2, y + 1, (k === pl ? '>' : (k === songNext ? '+' : ' ')) + p2(k + 1), c); txt(24, y + 1, 'S' + (e[0] + 1), c);
        const bs = (e[2] > 1 ? e[1] + 'x' + e[2] : e[1] + 'B') + (e[3] ? ' ' + e[3] : '') + (e[4] === 1 ? ' F' : (e[4] === 2 ? ' D' : ''));
        if (e[5] && 122 - 3 - tw(bs) - 40 > 8) txt(38, y + 1, fit(SONG_TAG[e[5]], 122 - 3 - tw(bs) - 42), c);
        txt(122 - 3 - tw(bs), y + 1, bs, c);
    }
    if (songBlank && L === 1) { txt(2, 26, 'STEP 1-8 = ADD SCENE', 1); txt(2, 36, 'STEP 9-14 = BARS', 1); txt(2, 46, 'PLAY = START SONG', 1); }
    if (L > rows) { const h = Math.max(4, Math.round(36 * rows / L)), y = 12 + Math.round(36 * (st / L)); fill_rect(125, 12, 1, 36, 1); fill_rect(124, y, 3, h, 1); }
    { const cur = songList[songSlot] ? songList[songSlot][0] : -1;           /* every scene you built: filled = has notes, frame = empty, thick frame = the selected step's scene (steps 1-8 add it) */
      for (let c = 0; c < 8; c++) { const x = 1 + c * 15, has = sceneHas(c); if (has) fill_rect(x, 49, 13, 7, 1); else draw_rect(x, 49, 13, 7, 1); txtS(x + 5, 50, String(c + 1), has ? 0 : 1); if (c === cur) draw_rect(x - 1, 48, 15, 9, 1); } }
    fill_rect(0, 58, 128, 1, 1);
    let bw = 0; for (let k = 0; k < L; k++) { const w = Math.max(1, Math.round(126 * songLen(songList[k]) / tot)); if (k === songSlot) fill_rect(1 + bw, 60, w, 3, 1); else fill_rect(1 + bw, 61, w, 1, 1); bw += w; if (bw > 126) break; }
}
let grains = [], grEnv = '', grT = 0;
function grainPoll() {                                     /* live picture of the granulator of the selected sound (only asked for while the page is open) */
    const r = dspGet('grnv_' + (ln() + 1)); if (typeof r !== 'string') return;
    const pt = r.split('|'); grEnv = pt[0] || ''; grains = (pt[1] || '').split(';').filter(x => x.length).map(x => x.split(',').map(Number));
}
const GWIN = [ph => Math.min(1, Math.min(ph, 1 - ph) * 12), ph => 2 * Math.min(ph, 1 - ph), ph => Math.sin(Math.PI * ph), ph => Math.exp(-20 * (ph - 0.5) * (ph - 0.5)), ph => { const s = Math.sin(Math.PI * ph); return s * s; }];
function gWin(ph, v) { const m = clamp(v, 0, 1) * 4, i = Math.min(3, Math.floor(m)), f = m - i; return GWIN[i](ph) * (1 - f) + GWIN[i + 1](ph) * f; }
function drawGrains() {                                    /* left = a while ago, right = now: the sound's level as a soft mirrored wave, every grain drawn with the real WINDOW shape (width = size, height = pitch) */
    const y0 = 49, h = 15, yc = y0 + 7, on = pads[ln()][PI.grn] >= 0.01, frz = pads[ln()][PI.gfz] > 0.5, wv = pads[ln()][PI.gtn];
    draw_rect(0, y0, 128, h, 1);
    if (!on) { txtS(28, y0 + 5, 'GRAIN OFF  TURN KNOB 1', 1); return; }
    if (frz) { txtS(3, y0 + 2, 'FRZ', 1); for (let x = 2; x < 126; x += 4) set_pixel(x, yc, 1); }
    for (let k = 0; k < 32 && k < grEnv.length; k++) {                       /* history: thin mirrored bars, rounded to a wave */
        const a = parseInt(grEnv[k], 16) / 15 || 0, hh = Math.round(a * 5), x = 3 + Math.round(k * 3.75);
        if (hh > 0) fill_rect(x, yc - hh, 1, hh * 2 + 1, 1); else set_pixel(x, yc, 1);
    }
    const big = frz ? 0 : 1;
    for (let k = 0; k < grains.length; k++) {
        const g = grains[k], rv = g[2] < 0, w = clamp(Math.round(6 + g[1] * 0.45), 6, 40), x0 = clamp(Math.round(124 - g[0] * 1.15), 2, 124 - w), base = clamp(y0 + h - 3 - Math.round((Math.abs(g[2]) - 4) * 0.9), y0 + 7, y0 + h - 3), ah = 6;
        let py = -1;
        for (let i = 0; i < w; i++) {                                         /* the window shape as an outline arch (backwards grains lean the other way) */
            const ph = (rv ? w - 1 - i : i) / (w - 1), yy = base - Math.round(gWin(ph, wv) * ah);
            set_pixel(x0 + i, yy, 1);
            if (py >= 0 && Math.abs(yy - py) > 1) { const lo = Math.min(yy, py), hi = Math.max(yy, py); for (let q = lo; q <= hi; q++) set_pixel(x0 + i, q, 1); }
            py = yy;
        }
        fill_rect(x0, base, w, 1, 1);                                          /* baseline under the arch */
        if (g[3] < 35) fill_rect(x0, base - ah, 1, ah + 1, 1);                 /* fresh grain: start tick */
    }
    fill_rect(126, y0 + 1, 1, h - 2, 1);
    if (big) fill_rect(125, yc, 3, 1, 1);
}
const BUS_TITLE = { 46: ['FX BUS', 'ALL TRACKS'], 39: ['FX BUS 2', 'ALL TRACKS'] };      /* shared by every track: the header must not say TRACK 03 */
function drawParams() {
    const items = VIEW_ITEMS[pg2(view)], scope = padView(view) ? 'pad' : 'master';
    if (BUS_TITLE[view]) titleBar(BUS_TITLE[view][0], BUS_TITLE[view][1]); else
    titleBar(scope === 'pad' || view === 36 ? (p2(sel + 1) + ' ' + laneName() + (view === 36 && euPage >= 3 ? ' L' + (euPage - 2) : '')) : (view === 21 ? p2(sel + 1) + ' ' + (kind[sel] ? 'DRUM ' + (slot[sel] + 1) : 'SYNTH') : (view === 8 ? 'OPT' : (view === 44 ? 'FAVORITES' : 'MASTER'))), view === 8 ? 'CPU ' + cpuAvg + '%' : (view === 44 ? 'ALL PAGES' : VIEWS[view]));
    const ps = PAGE_STYLE[view] || 0;
    if (view === 2) drawFilterCurve();
    if (view === 41 || view === 43) { const g = [0, 1, 2].map(b => { const k = items[b][0]; return (getVal(scopeOf(k), k) - 0.5) * 24; }); drawEqGraph(g, 2, 11, 124, view === 43 ? 13 : 17, (hot.t > 0 && hot.v === view && hot.i < 3) ? hot.i : -1); }
    for (let i = 0; i < items.length; i++) {
        let cw = items.length <= 2 ? 64 : (items.length === 3 || items.length === 5 || items.length === 6 ? 42 : 32), per = Math.floor(128 / cw);
        let x = (i % per) * cw, y = 13 + ((i / per) | 0) * 26, rh = 0;
        const v = getVal(scopeOf(items[i][0]), items[i][0]);
        const lab = itemLabel(view, i, Math.floor((cw - 6) / 6)), vs = fmtVal(items[i][0], v), fr = fracVal(items[i][0], v); labFull = String(tr(lab)); labW = cw - 3;
        if (view === 36 || view === 43 || view === 50) {         /* compact tiles (8 knobs) so the pattern / the EQ curve / the grains still fit */
            const cx = (i & 3) * 32, cy = (view === 43 ? 26 : 12) + (i >> 2) * 19, hotE = hot.t > 0 && hot.v === view && hot.i === i;
            const cc = 1;
            txt(cx + 3, cy + 1, fit(lab, 28), cc); txtB(cx + 3, cy + 9, fit(vs, 28), cc);
            const w = Math.round(28 * clamp(fr, 0, 1)); fill_rect(cx + 2, cy + 17, 28, 1, cc); if (w > 0) fill_rect(cx + 2, cy + 16, w, 2, cc);
            continue;
        }
        if (view === 2) { const cx = (i & 3) * 32 + 1, cy = 25 + (i >> 2) * 20; txt(cx + 1, cy, fit(lab, 30), 1); txtB(cx + 1, cy + 9, fit(vs, 30), 1); fill_rect(cx + 1, cy + 18, 28, 1, 1); const w = Math.round(28 * clamp(fr, 0, 1)); if (w > 0) fill_rect(cx + 1, cy + 17, w, 2, 1); continue; }
        const isHot = hot.t > 0 && hot.v === view && hot.i === i;
        if (view === 1 && items[i][0] === 'wave' && cw >= 32) { drawPresetBox(x, y, cw, (ENUMS.wave[Math.round(v)] || vs)); continue; }
        { const si = (ps !== 3 && cw >= 32) ? selInfo(items[i][0], v) : null; if (si) { cellSel(x, y, lab, vs, si[0], si[1], cw, isHot, false); continue; } }
                if (view === 1 && items[i][0] === 'type' && cw >= 32) { drawPresetBox(x, y, cw, TYPE_NAME[Math.round(v)] || vs); continue; }
        if (uiMin && cw >= 32 && (ps === 0 || ps === 1 || ps === 4) && view !== 24 && view !== 27) { themedCell(x, y, lab, vs, fr, cw, isHot, items[i][0]); continue; }
        if (ps === 2) { const n = items.length; per = n > 4 ? 2 : 1; cw = 128 / per; rh = Math.min(15, Math.floor(50 / Math.ceil(n / per))); x = (i % per) * cw; y = 14 + ((i / per) | 0) * rh; cellList(x, y, lab, vs, fr, cw, rh, isHot); }
        else if (ps === 1) { if (isHot) cell(x, y, lab, vs, fr, cw); else cellInv(x, y, lab, vs, fr, cw); }
        else if (ps === 3 && view === 43) { cw = 42; eqCellH(i * 42 + 1, 40, lab, vs, fr, cw, isHot); }
        else if (ps === 3 && view === 41) { cw = Math.floor(128 / items.length); x = i * cw + ((128 - cw * items.length) >> 1); cellVertS(x, 29, lab, vs, fr, cw, 19); }
        else if (ps === 3) { cw = Math.floor(128 / items.length); x = i * cw + ((128 - cw * items.length) >> 1); cellVert(x, 13, lab, vs, fr, cw); }
        else if (ps === 4) { if (isHot) cellInv(x, y, lab, vs, fr, cw); else cellThick(x, y, lab, vs, fr, cw); }
        else if (isHot) cellInv(x, y, lab, vs, fr, cw);
        else cell(x, y, lab, vs, fr, cw);
    }
    if (view === 36) {
        const L = ln(), N = LL(), n = Math.min(N, 64), c = ed[sel], two = n > 32, bw = two ? 4 : (n > 16 ? 4 : 7);
        const x0 = two ? 0 : Math.floor((128 - n * bw) / 2), cur = playing ? (((gs - 1) % N) + N) % N : -1;
        for (let st = 0; st < n; st++) {
            const rowI = two ? (st >> 5) : 0, x = x0 + (st % 32) * bw, base = two ? 55 + rowI * 8 : 62, mh = two ? 5 : 9, S = steps[c][L][st];
            if (S[0]) { const h = 2 + Math.round((mh - 2) * S[1] / 127); fill_rect(x, base - h + 1, bw - 1, h, 1); }
            else fill_rect(x + ((bw - 1) >> 1), base, 1, 1, 1);
            if (st === cur) fill_rect(x, two ? 56 + rowI * 8 : 63, bw - 1, 1, 1);
        }
        for (let p = 0; p < EU_NPG; p++) { if (p === euPage) fill_rect(102 + p * 4, 50, 3, 3, 1); else draw_rect(102 + p * 4, 50, 3, 3, 1); }
        
    }
    if (view === 50) drawGrains();
    if (view === 33 && rsState === 2) { draw_rect(1, 44, 126, 6, 1); const w = Math.round(122 * rsPct / 100); if (w > 0) fill_rect(3, 46, w, 2, 1); }
    if (view === 8 && lastErr) { fill_rect(0, 38, 128, 26, 0); txt(1, 41, 'ERR ' + errCount, 1); txt(1, 50, lastErr.substring(0, 21), 1); txt(1, 58, lastErr.substring(21, 42), 1); }
}
const MOD_SYN = [['ring','Ring'],['rfq','RingFrq','RFrq'],['pump','Pump'],['wob','Wobble','Wob'],['duck','Duck'],['duckr','DuckRel','DRel'],['scs','DuckSrc','DSrc'],['pcv','PumpCrv','PCrv']];
const MOD_DRM = [['ring','Ring'],['rfq','RingFrq','RFrq'],['pump','Pump'],['rmb','Rumble','Rmbl'],['duck','Duck'],['duckr','DuckRel','DRel'],['scs','DuckSrc','DSrc'],['rmt','RumbleTail','RTail']];
const FILT_BASE = VIEW_ITEMS[2], P47_BASE = VIEW_ITEMS[47], SYN_PG = VIEW_ITEMS[9], FMI_PG = [['det','Ratio','Rat'],['sub','Index','Idx'],['fkey','Feedback','Fdbk'],['drive','Drive','Drv'],['cut','LPCut','LPF'],['res','Reso','Res'],['fenv','FltEnv','FEnv'],['poly','Voices','Poly']];
const FMI = TYPE_NAME.indexOf('FmInit');                       /* the plain 2-operator FM INIT sound: Ratio, Index, Feedback (+ Decay on the sound page) */
const ENG_KN = { 6: ['Detune', 'Sub', 'Sync', 'Width'], 7: ['Position', 'Sweep', 'Warp', 'Detune'], 8: ['Index', 'Ratio', 'Feedback', 'Algo'], 9: ['Tilt', 'Odd/Even', 'Stretch', 'Shimmer'], 10: ['Detune', 'Spread', 'Hollow', 'Motion'], 11: ['Bright', 'Pick', 'Damp', 'Excite'], 12: ['Fold', 'Sweep', 'Symmetry', 'Harmonics'] };
const ENG_ABBR = { Detune: 'Det', Position: 'Pos', Sweep: 'Swp', Width: 'Wid', Feedback: 'Fdbk', 'Odd/Even': 'O/E', Stretch: 'Strc', Shimmer: 'Shim', Spread: 'Sprd', Hollow: 'Holo', Motion: 'Mot', Bright: 'Brt', Excite: 'Exc', Symmetry: 'Sym', Harmonics: 'Harm', Index: 'Idx', Ratio: 'Rat' };      /* every knob name fits its tile in the normal letters */
const ENG_PG = {}; for (const e in ENG_KN) ENG_PG[e] = [['det', ENG_KN[e][0], ENG_ABBR[ENG_KN[e][0]]], ['sub', ENG_KN[e][1], ENG_ABBR[ENG_KN[e][1]]], ['fkey', ENG_KN[e][2], ENG_ABBR[ENG_KN[e][2]]], ['fdec', ENG_KN[e][3], ENG_ABBR[ENG_KN[e][3]]], ['drive', 'Drive', 'Drv'], ['cut', 'LPCut', 'LPF'], ['res', 'Reso', 'Res'], ['poly', 'Voices', 'Poly']];
function nxEng(t) { for (let i = 0; i < NX_ENG.length; i++) if (t >= NX_ENG[i][0] && t <= NX_ENG[i][1]) return NX_ENG[i][2]; return 0; }
function nxList(e) { const l = []; for (let i = 0; i < NX_ENG.length; i++) if (NX_ENG[i][2] === e) for (let t = NX_ENG[i][0]; t <= NX_ENG[i][1]; t++) l.push(t); return l; }
const PG47_ENG = VIEW_ITEMS[47], PG47_SYN = PG47_ENG.map(it => it[0] === 'schp' ? ['drive', 'Drive', 'Drv'] : it);
const SND_ALL = VIEW_ITEMS[1], SND_DRM = SND_ALL.map(it => it[0] === 'poly' ? ['dly', 'Delay', 'Dly'] : it);
const SND_SYN = [['type', 'Engine'], ['att', 'Attack', 'Atk'], ['decay', 'Decay', 'Dcy'], ['tone', 'Tone'], ['tune', 'Tune'], ['pan', 'Pan'], ['poly', 'Voices', 'Poly'], ['vol', 'Vol']];      /* synth: engine + the shaping knobs first */       /* synth: Drive lives on the 2nd SYNTH page; the wave (Saw, Square ...) sits here as a small preset-style box */
function viewItemsFix() {
    if (favSig !== (chordOn() ? 1 : 0)) favBuild();
    VIEW_ITEMS[3] = kind[sel] ? MOD_DRM : MOD_SYN;
    const t = kind[sel] ? 0 : Math.round(pads[sel][0]), e = nxEng(t);
    VIEW_ITEMS[1] = kind[sel] ? SND_DRM : SND_SYN;      /* a drum sound has no voices: the delay send takes that place. Synths: the wave window only where the wave has an effect (classic synths), else the delay send */
    VIEW_ITEMS[9] = (FMI > 0 && t === FMI) ? FMI_PG : (e ? ENG_PG[e] : SYN_PG);
}

/* ---- scope page: master output wave (triggered) and 8 band levels, polled from the DSP ---- */
let scopeStr = '', scopeMode = 0, scopePoll = 0, animT = 0;
function scopeTick() {
    if (view !== 42 || menuOpen) return;
    if (scopeMode >= 3) { animT++; if (animT % 2 === 0) needsRedraw = true; }
    if (++scopePoll % 2) return;
    const r = dspGet('scope'); if (typeof r === 'string' && r.length >= 144 && r !== scopeStr) { scopeStr = r; needsRedraw = true; }
}

/* ---- MIDI monitor: shows every raw message the module receives (to find out which number a button really sends). Back leaves it. ---- */
let monOpen = false, monLog = [];
function drawMon() {
    titleBar('MIDI MONITOR', 'BACK = EXIT');
    if (!monLog.length) { txt(4, 22, 'PRESS A BUTTON', 1); txt(4, 34, 'OR TURN A KNOB', 1); return; }
    for (let i = 0; i < monLog.length && i < 5; i++) txt(4, 14 + i * 10, monLog[monLog.length - 1 - i], 1);
}
function monPush(d) {
    const h = v => ('0' + (v | 0).toString(16).toUpperCase()).slice(-2);
    const st = d[0] | 0, ty = st & 0xF0, nm = ty === 0xB0 ? 'CC' : (ty === 0x90 ? 'NOTE ON' : (ty === 0x80 ? 'NOTE OFF' : 'MSG'));
    monLog.push(nm + ' ' + (d[1] | 0) + ' VAL ' + (d[2] | 0) + ' (' + h(st) + ')'); if (monLog.length > 8) monLog.shift(); needsRedraw = true;
}

const SPR_CAT = ["##.....##", "###...###", "#########", "#.#####.#", "#########", "###.#.###", ".#######.", "..#####..", ["..#...#..", ".#.....#."]];
const SPR_BUN = [".##...##.", ".##...##.", ".#######.", "#########", "##.###.##", "#########", "###.#.###", ".#######.", ["..#...#..", ".#.....#."]];
const SPR_FRG = [".##...##.", "#.#####.#", "#########", "#########", "#.#####.#", ".#.....#.", ".#######.", "#.#####.#", ["#.#...#.#", ".#.....#."]];
function spr(x, y, S, fr, sc) {
    for (let r = 0; r < 9; r++) { const row = r === 8 ? S[8][fr] : S[r]; for (let q = 0; q < 9; q++) if (row[q] === '#') fill_rect(x + q * sc, y + r * sc, sc, sc, 1); }
}
function drawScopeAnim() {
    const b = []; for (let i = 0; i < 8; i++) b.push(scopeStr.length >= 144 ? parseInt(scopeStr.substr(128 + i * 2, 2), 16) / 255 : 0);
    const bass = Math.max(b[0], b[1]), mid = Math.max(b[2], b[3], b[4]), hi = Math.max(b[5], b[6], b[7]), lvl = Math.max(bass, mid, hi);
    const t = animT, beat = playing ? (gs & 1) : (t >> 3) & 1;
    if (scopeMode === 3) {
        const X = [14, 55, 96], S = [SPR_CAT, SPR_BUN, SPR_FRG], E = [bass, mid, hi];
        for (let k = 0; k < 3; k++) {
            const up = Math.round(clamp(E[k] * 2, 0, 1) * 14) + (((t >> 2) + k) & 1 ? 2 : 0), fr = ((t >> 3) + k) & 1;
            const sh = Math.round(Math.sin(t * 0.25 + k * 2) * 2 * clamp(lvl * 3, 0, 1));
            spr(X[k] + sh, 62 - 18 - up, S[k], fr, 2);
            fill_rect(X[k] + 2 + sh, 63, 14, 1, 1);
            if (E[k] > 0.35) { const ny = 14 + ((t + k * 7) % 20); bmp(X[k] + 12, 62 - 22 - up - (ny % 6), ['..#', '..#', '##.', '##.'], 1); }
        }
        fill_rect(0, 62, 128, 1, 1);
    } else if (scopeMode === 4) {
        const sp = 1 + Math.round(bass * 5) + Math.round(hi * 2);
        for (let i = 0; i < 28; i++) {
            const a = i * 2.399, base = ((i * 37 + t * sp * 0.6) % 70) + 1, r = base * base / 60, x = Math.round(64 + Math.cos(a) * r * 1.6), y = Math.round(36 + Math.sin(a) * r * 0.9);
            if (x < 0 || x > 127 || y < 11 || y > 63) continue; set_pixel(x, y, 1); if (r > 28 && bass > 0.3) { set_pixel(x + 1, y, 1); }
        }
    } else {
        for (let k = 0; k < 4; k++) {
            const r = ((t * (1 + bass * 3) * 0.5 + k * 9) % 36) + 2, cx = 64, cy = 38;
            for (let a = 0; a < 48; a++) { const an = a * 0.1309, x = Math.round(cx + Math.cos(an) * r * 1.7), y = Math.round(cy + Math.sin(an) * r * 0.9); if (x >= 0 && x < 128 && y >= 11 && y < 64) set_pixel(x, y, 1); }
        }
        const hh = Math.round(4 + lvl * 12); fill_rect(60, 38 - (hh >> 1), 8, hh, 1);
    }
}
function drawScope() {
    titleBar('SCOPE', ['WAVE + BANDS', 'WAVE', 'BANDS', 'DANCE', 'STARS', 'RINGS'][scopeMode]);
    if (scopeMode >= 3) { drawScopeAnim(); return; }
    if (scopeStr.length < 144) { txt(4, 28, 'NO SIGNAL DATA', 1); return; }
    const w = [], b = [];
    for (let i = 0; i < 64; i++) w.push((parseInt(scopeStr.substr(i * 2, 2), 16) / 255) * 2 - 1);
    for (let i = 0; i < 8; i++) b.push(parseInt(scopeStr.substr(128 + i * 2, 2), 16) / 255);
    let pk = 0; for (const v of w) pk = Math.max(pk, Math.abs(v));
    const wy0 = 12, wh = scopeMode === 1 ? 50 : (scopeMode === 0 ? 26 : 0);
    if (wh) {
        const cy = wy0 + (wh >> 1); fill_rect(0, cy, 128, 1, 1); set_pixel(0, cy, 1);
        const gn = pk > 0.02 ? clamp(0.95 / pk, 1, 8) : 1;
        let py = cy; for (let i = 0; i < 64; i++) { const y = clamp(Math.round(cy - w[i] * gn * (wh >> 1) * 0.95), wy0, wy0 + wh - 1), a = Math.min(y, py), c2 = Math.max(y, py); for (let q = a; q <= c2; q++) { set_pixel(i * 2, q, 1); set_pixel(i * 2 + 1, q, 1); } py = y; }
        if (pk > 0.95) txt(100, wy0 + 1, 'CLIP', 1); else if (gn >= 1.5) txt(110, wy0 + 1, 'x' + Math.floor(gn), 1);
    }
    if (scopeMode !== 1) {
        const by0 = scopeMode === 0 ? 40 : 12, bh = 62 - by0 + 1;
        for (let i = 0; i < 8; i++) { const h = Math.round(clamp(b[i], 0, 1) * bh); if (h > 0) fill_rect(2 + i * 16, 63 - h, 12, h, 1); else fill_rect(2 + i * 16, 62, 12, 1, 1); }
    }
}
/* ---- display options: start animation on/off, idle screen (animation, then black), inverted display ---- */
let uiMin = true, uiTheme = 9, bigOn = false, slideOn = false;               /* minimal style / big value display / page slide */
let knobLedsOn = true, knobLvlOn = true;                                      /* the lights under the knobs (which knobs do something here) */
let slowKnobs = false, slowAcc = {};                         /* gentle mode: knobs and jogwheel turn at half speed, messages stay twice as long */
let btnBright = false, splashOn = true, idleOn = false, dispInv = false, idleBlank = false, lastInputT = Date.now();
function noteInput() { lastInputT = Date.now(); }
function drawInverted() {
    const o = { fr: globalThis.fill_rect, dr: globalThis.draw_rect, sp: globalThis.set_pixel, pr: globalThis.print, cs: globalThis.clear_screen };
    let inW = false;                                           /* a host function that calls another drawing function itself must not be inverted twice */
    const W = (f, fix) => function () { if (inW) return f.apply(null, arguments); inW = true; try { return f.apply(null, fix(Array.prototype.slice.call(arguments))); } finally { inW = false; } };
    const fl = c => c ? 0 : 1;
    globalThis.clear_screen = () => { inW = true; try { o.fr(0, 0, 128, 64, 1); } finally { inW = false; } };
    globalThis.fill_rect = W(o.fr, a => { a[4] = fl(a[4]); return a; });
    globalThis.draw_rect = W(o.dr, a => { a[4] = fl(a[4]); return a; });
    globalThis.set_pixel = W(o.sp, a => { a[2] = fl(a[2]); return a; });
    globalThis.print = W(o.pr, a => { a[3] = a[3] === undefined ? 0 : fl(a[3]); return a; });
    try { drawCoreNH(); } finally { globalThis.fill_rect = o.fr; globalThis.draw_rect = o.dr; globalThis.set_pixel = o.sp; globalThis.print = o.pr; globalThis.clear_screen = o.cs; }
}
function draw() { if (dispInv) drawInverted(); else drawCoreNH(); }
/* No white boxes with black letters: every lit-up block (selected row, touched tile, current tab) is redrawn as a black box with a white frame and white letters. Everything drawn into such a block is flipped with it. */
let NH = null;                                                 /* set while a frame is drawn: black text / pictures inside a white block are noted here */
function drawCoreNH() {
    const o = { fr: globalThis.fill_rect, dr: globalThis.draw_rect, pr: globalThis.print, cs: globalThis.clear_screen };
    if (typeof o.fr !== 'function' || typeof o.pr !== 'function' || typeof o.dr !== 'function') { drawCore(); return; }
    const cand = [];                                           /* white blocks of a size that can hold text */
    const inC = (x, y) => { for (let i = 0; i < cand.length; i++) { const r = cand[i]; if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return r; } return null; };
    NH = { note: (k, x, y, a) => { if (!cand.length) return; const r = inC(x, y); if (r && !r.dead) { r.ops.push([k, x, y, a]); r.blk = true; } } };
    globalThis.clear_screen = function () { cand.length = 0; return o.cs(); };
    globalThis.fill_rect = function (x, y, w, h, c) {
        if (c === 1 && w >= 12 && h >= 7 && h <= 26 && !(h >= 17 && w >= 56 && w % 32 !== 0) && !(w === 40 && h === 26)) cand.push({ x: x, y: y, w: w, h: h, ops: [], dead: false, blk: false });
        else if (cand.length) {
            if (c === 0 && w * h >= 24) for (let i = 0; i < cand.length; i++) { const r = cand[i]; if (r.dead) continue; const ix = x < r.x + r.w && x + w > r.x && y < r.y + r.h && y + h > r.y, inside = x >= r.x && y >= r.y && x + w <= r.x + r.w && y + h <= r.y + r.h; if (ix && !inside) r.dead = true; }
            const r = inC(x, y); if (r && !r.dead && c === 0) { r.ops.push(['f', x, y, [w, h]]); r.blk = true; }
        }
        return o.fr(x, y, w, h, c);
    };
    globalThis.draw_rect = function (x, y, w, h, c) {
        if (cand.length && c === 0) { const r = inC(x, y); if (r && !r.dead) { r.ops.push(['d', x, y, [w, h]]); r.blk = true; } }
        return o.dr(x, y, w, h, c);
    };
    globalThis.print = function (x, y, t, c) {
        if (cand.length && c === 0) { const r = inC(x, y); if (r && !r.dead) { r.ops.push(['p', x, y, t]); r.blk = true; } }
        return o.pr(x, y, t, c);
    };
    const restore = () => { globalThis.fill_rect = o.fr; globalThis.draw_rect = o.dr; globalThis.print = o.pr; globalThis.clear_screen = o.cs; };
    try {
        drawCore();
        restore(); const nh = NH; NH = null;
        for (const r of cand) {
            if (r.dead || !r.blk) continue;
            o.fr(r.x, r.y, r.w, r.h, 0); o.dr(r.x, r.y, r.w, r.h, 1); if (r.h >= 12) o.dr(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 1);
            for (const q of r.ops) {
                if (q[0] === 'f') o.fr(q[1], q[2], q[3][0], q[3][1], 1);
                else if (q[0] === 'd') o.dr(q[1], q[2], q[3][0], q[3][1], 1);
                else if (q[0] === 'p') o.pr(q[1], q[2], q[3], 1);
                else if (q[0] === 'h') txtH(q[1], q[2], q[3], 1);
                else if (q[0] === 's') txtS(q[1], q[2], q[3], 1);
                else if (q[0] === 'b') bmp(q[1], q[2], q[3], 1);
            }
        }
    } finally { restore(); NH = null; }
}

function drawCore() {
    viewItemsFix();
    if (splashT0 > 0) { if (splashActive()) { drawSplashAt(Date.now() - splashT0); return; } splashEnd(); }
    if (idleBlank) { clear_screen(); needsRedraw = false; return; }
    if (errShow > 0) { errShow--; drawErr(); return; }
    clear_screen(); hdrDrawn = false;
    tabStrip = isTab(view) && !projUI && exitDlg < 0 && !menuOpen && askKind < 0 && !(view === 28 && broOpen) && !(brwTicks > 0 && brw) && !fb;
    if (fb && !menuOpen && askKind < 0 && exitDlg < 0) { drawFB(); return; }
    if (brwTicks > 0 && brw && !menuOpen && askKind < 0) { drawBrowse(); return; }
    if (projUI) drawProj(); else if (monOpen) drawMon(); else if (exitDlg >= 0) drawExit(); else if (askKind >= 0) drawAsk(); else if (menuOpen) drawMenu(); else if (heldStep >= 0 && !recDown && stepWinView()) drawStepEdit(); else if (view === 0) drawSeq(); else if (view === 40) drawHist(); else if (view === 42) drawScope(); else if (view === 51) drawLoad(); else if (view === 7) drawClips(); else if (view === 31) drawSong(); else if (view === 45) drawDevices(); else if (isHub(view)) drawHub(); else if (view === 20) drawMix(); else if (view === 28 && broOpen) drawBro(); else if (view === 28) { if (smpPage === 1) drawSlice(); else drawSample(); } else drawParams();
    if (toast && bigOn && toast.ck && !menuOpen && askKind < 0 && exitDlg < 0 && !monOpen) drawBigValue(toast);
    if (toast) {
        /* small message box in the top right corner (name + value on one line, thin bar under it); the page stays visible */
        const vt = String(toast.text).toUpperCase(), lb = String(toast.label).toUpperCase();
        const nmL = toast.nm && vt && !menuOpen;                 /* turning a knob: the name stands top left, the box only holds the value */
        let str = nmL ? vt : (vt ? lb + ' ' + vt : lb); const maxc = nmL ? 12 : 17; if (str.length > maxc) str = vt && vt.length < maxc - 3 ? lb.substring(0, maxc - vt.length - 1) + ' ' + vt : str.substring(0, maxc);
        const bw = str.length * 6 + 5, bx = 128 - bw;
        fill_rect(bx, 0, bw, 11, 0); draw_rect(bx, 0, bw, 11, 1);
        txt(bx + 3, 2, str, 1);
        if (nmL) { const nt = hfFit(lb, bx - 5); fill_rect(0, 0, bx, 11, 0); draw_rect(0, 0, bx + 1, 11, 1); txtH(3, 2, nt, 1); }
        const w = Math.round((bw - 2) * clamp(toast.f, 0, 1)); if (w > 0) fill_rect(bx + 1, 9, w, 1, 1);
        if (toast.ck && CURVE_KEYS[toast.ck] && !menuOpen && askKind < 0) drawCurveBox(toast.ck);
    }
    if (helpOpen) drawHelp();
    if (uiMin && playing && hdrDrawn && !toast) { const stp = ((gs - 1) % 16 + 16) % 16; fill_rect(stp * 8, 10, 8, 2, 1); }      /* the playhead runs along the header line */
    needsRedraw = false; if (menuOpen && menuAnim) needsRedraw = true;
    if (view !== slideView) { slideView = view; if (slideOn && !menuOpen && askKind < 0 && exitDlg < 0) slideT0 = Date.now(); }
    if (slideT0 > 0) {                                       /* page slide: the new page is revealed from the left in about 140 ms */
        const p = (Date.now() - slideT0) / 140;
        if (p >= 1) slideT0 = 0;
        else { const rv = Math.max(1, Math.round(128 * p)); fill_rect(rv, 0, 128 - rv, 64, 0); fill_rect(rv - 1, 0, 1, 64, 1); needsRedraw = true; }
    }
    if (saveFlash > 0) fill_rect(124, 1, 3, 3, 1);                  /* autosave dot */
}
let slideView = -1, slideT0 = 0;

/* ---- LEDs ---- */
/* The shared setLED() keeps a cache that survives module restarts while the host wipes the LEDs
 * on entry - so the repaint after a restart was silently dropped. Write straight to the host. */
function sendLed(n, c) { if (typeof move_midi_internal_send === 'function') move_midi_internal_send([0x09, 0x90, n, c]); else setLED(n, c, true); }
function sendBtn(cc, c) { if (typeof move_midi_internal_send === 'function') move_midi_internal_send([0x0B, 0xB0, cc, c]); else setButtonLED(cc, c, true); }
let ledWant = {}, ledSent = {}, btnWant = {}, btnSent = {};
function led(note, color) { ledWant[note] = color; }
function btn(cc, color) { btnWant[cc] = color; }
/* at most 8 LED messages per tick: changed pads/steps/buttons first, then 2 forced refreshes (the host may drop or wipe LEDs) */
function flushLeds() {
    let n = 0;
    const budget = 10;
    for (let i = 0; i < 32 && n < budget; i++) { const k = PAD_FIRST + i; if (ledWant[k] !== undefined && ledSent[k] !== ledWant[k]) { sendLed(k, ledWant[k]); ledSent[k] = ledWant[k]; n++; } }
    for (let i = 0; i < 16 && n < budget; i++) { const k = STEP_FIRST + i; if (ledWant[k] !== undefined && ledSent[k] !== ledWant[k]) { sendLed(k, ledWant[k]); ledSent[k] = ledWant[k]; n++; } }
    for (const k in btnWant) { if (n >= budget) break; if (btnSent[k] !== btnWant[k]) { sendBtn(+k, btnWant[k]); btnSent[k] = btnWant[k]; n++; } }
    for (let j = 0; j < 2; j++) {
        const r = ledRot++ % 48, note = r < 32 ? PAD_FIRST + r : STEP_FIRST + r - 32;
        if (ledWant[note] !== undefined) { sendLed(note, ledWant[note]); ledSent[note] = ledWant[note]; }
    }
}
function knobUse() {                                       /* which of the 8 knobs do something on the current page: their little lights under the knobs are on */
    const u = [0, 0, 0, 0, 0, 0, 0, 0];
    if (menuOpen || askKind >= 0 || exitDlg >= 0 || monOpen || idleBlank || fb) return u;
    if (heldStep >= 0 && stepWinView()) return u.map(() => 1);
    if (loopHeld && view === 0) { u[0] = 1; u[1] = 1; u[2] = 1; return u; }
    if (view === 0 || view === 7 || view === 20) return u.map(() => 1);
    const items = VIEW_ITEMS[itemsView()];
    if (items) for (let i = 0; i < items.length && i < 8; i++) u[i] = 1;
    return u;
}
const KNOB_RAMP = [123, 119, 118, 120];                   /* dim grey ... white: the lights under the knobs get brighter with the value */
function knobLevels() {                                    /* value 0..1 of each knob on this page (null = no value to show) */
    if (menuOpen || askKind >= 0 || exitDlg >= 0 || monOpen || idleBlank || fb || heldStep >= 0 || loopHeld) return null;
    if (view === 0) return seqSnd().map(k => clamp(fracVal(k, pads[ln()][PI[k]]), 0, 1));
    const items = VIEW_ITEMS[itemsView()];
    if (!items) return null;
    const out = [];
    for (let i = 0; i < items.length && i < 8; i++) { const k = items[i][0]; out.push(clamp(fracVal(k, getVal(scopeOf(k), k)), 0, 1)); }
    return out;
}
function updateLeds() {
    seqUseUpdate();
    { const ku = knobLedsOn ? knobUse() : [0, 0, 0, 0, 0, 0, 0, 0], lv = (knobLedsOn && knobLvlOn) ? knobLevels() : null;
      for (let i = 0; i < 8; i++) btn(CC.KNOB1 + i, ku[i] ? (lv && lv[i] !== undefined ? KNOB_RAMP[Math.min(3, Math.floor(lv[i] * 4))] : 120) : 0); }
    const cur = playing ? ((gs - 1 + 1024 * LL()) % LL()) : -1;
    for (let i = 0; i < 32; i++) {
        let c;
        if (view === 7) {
            const t = bank * 4 + (3 - (i >> 3)), cl = i & 7, has = trackHasSteps(cl, t);
            const tc = trkCol(t);
            if (!has && clipsCur[t] !== cl && clipsQ[t] !== cl) c = 123;
            else if (clipsCur[t] === cl) c = has ? (clipTrig(t, cl) ? 120 : tc) : 123;
            else if (clipsQ[t] === cl) c = 120;
            else c = has ? dimCol(tc) : 123;
        }
        else if (view === 20) c = i < NTRK ? (mutes[i] ? 0 : trkCol(i)) : 0;        /* mixer: the first 16 pads are the track mutes */
        else if (askKind >= 0) c = (i & 7) < 4 ? trkCol(sel) : 1;
        else c = mutes[sel] ? dimCol(trkCol(sel)) : padLed(i);
        led(PAD_FIRST + i, c);
    }
    for (let i = 0; i < 16; i++) {
        const s = stepPage * 16 + i; let c;
        if (muteHeld) c = mutes[i] ? 0 : trkCol(i);
        else if (view === 7) {
            if (loopHeld) { let pl = false, hs = false; if (i < NPAT) { for (let t = 0; t < NTRK; t++) { if (clipsCur[t] === i) pl = true; if (trackHasSteps(i, t)) hs = true; } } c = i >= NPAT ? 0 : (pl ? 120 : (hs ? 118 : 123)); }
            else c = i === sel ? 120 : (mutes[i] ? 0 : trkCol(i));
        }
        else if (view === 31) c = i < 8 ? (sceneHas(i) ? trkCol(i) : dimCol(trkCol(i))) : (i < 14 ? 118 : (i === 14 ? 123 : (songOn ? 126 : 120)));
        else if (askKind >= 0 || exitDlg >= 0 || (view === 28 && broOpen)) c = 0;      /* the steps stay visible in every page, they can be pressed there too */
        else if (s >= LL()) c = 0;
        else if (heldStep >= 0 && heldStep < LL() && s > heldStep && s < heldStep + (steps[ed[sel]][ln()][heldStep][9] || 0)) c = 126;      /* hold a step: the steps it lasts over light up white */
        else if (s === cur) c = 120;
        else if (steps[ed[sel]][ln()][s][0]) c = trkCol(sel);
        else c = 119;
        led(STEP_FIRST + i, c);
    }
    for (let b = 0; b < 4; b++) { const t = bank * 4 + b; btn(43 - b, t === sel ? 120 : trkCol(t)); }
    btn(CC.MENU, view === 7 ? 126 : 16);
    btn(CC.CAPTURE, 16);
    btn(CC.LOOP, view === 31 ? (songLoop ? 126 : 28) : (loopHeld ? 126 : 0));      /* Loop button: lit while the song loops / while held */
    for (let i = 0; i < 16; i++) btn(16 + i, ((i === 3 || i === 10 || i === 11) && shiftHeld) ? (topModes[sel] === (i === 3 ? 3 : i - 9) ? 126 : 28) : (((shiftHeld && SHIFT_OK[i]) || symLit(i)) ? 126 : 0));
    btn(CC.PLAY, playing ? 126 : 16);
    btn(CC.REC, recArmed ? 127 : 16);
    if (btnBright) {                                          /* brighter right-hand buttons: Mute, Copy (Double), Delete, Undo, Back, Sample, Loop glow even when idle */
        for (const cc of [CC.MUTE, CC.COPY, CC.DELETE, CC.UNDO, CC.BACK, CC.SAMPLE, CC.LOOP]) if (!btnWant[cc] || btnWant[cc] < 60) btnWant[cc] = 70;
        for (const cc of [CC.MENU, CC.CAPTURE, CC.PLAY, CC.REC]) if (btnWant[cc] < 60) btnWant[cc] = 70;
    }
}

/* ---- lifecycle ---- */
/* ---- splash animation: "TAKTILE" drops in letter by letter, a sweep runs over the step row, pads and steps light up in a wave ---- */
const SPL_MS = 3000;
let splashT0 = 0;
const SPL_FONT = {
    T: ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
    A: ['01110', '10001', '10001', '11111', '10001', '10001', '10001'],
    K: ['10001', '10010', '10100', '11000', '10100', '10010', '10001'],
    I: ['11111', '00100', '00100', '00100', '00100', '00100', '11111'],
    L: ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
    E: ['11111', '10000', '10000', '11110', '10000', '10000', '11111']
};
function splashActive() { return splashT0 > 0 && Date.now() - splashT0 < SPL_MS; }
function splashEnd() { splashT0 = 0; ledSent = {}; btnSent = {}; ledAge = 0; needsRedraw = true; }
function splBounce(x) {
    x = clamp(x, 0, 1);
    if (x < 1 / 2.75) return 7.5625 * x * x;
    if (x < 2 / 2.75) { x -= 1.5 / 2.75; return 7.5625 * x * x + 0.75; }
    if (x < 2.5 / 2.75) { x -= 2.25 / 2.75; return 7.5625 * x * x + 0.9375; }
    x -= 2.625 / 2.75; return 7.5625 * x * x + 0.984375;
}
function drawSplashAt(ms) {
    clear_screen();
    const word = 'TAKTILE', S = 3, LW = 5 * S, GAP = 3, x0 = Math.floor((128 - (7 * LW + 6 * GAP)) / 2), yF = 12;
    const sweepX = Math.floor((ms - 1300) / 900 * 150) - 10;
    for (let i = 0; i < 7; i++) {
        const k = (ms - 150 - i * 130) / 420;
        if (k <= 0) continue;
        const y = Math.round(-26 + (yF + 26) * splBounce(k));
        const x = x0 + i * (LW + GAP), g = SPL_FONT[word[i]];
        for (let r = 0; r < 7; r++) for (let c = 0; c < 5; c++) if (g[r][c] === '1') fill_rect(x + c * S, y + r * S, S, S, 1);
    }
    if (ms > 1300) {
        if (sweepX >= 0 && sweepX < 128) fill_rect(sweepX, 8, 1, 34, 1);
        for (let i = 0; i < 16; i++) {
            const dx = Math.round(4 + i * 7.6), on = dx < sweepX + 2 || ms > 2300;
            if (on) fill_rect(dx, 47, 5, 4, 1); else draw_rect(dx, 47, 5, 4, 1);
        }
    }
    if (ms > 1500) {
        for (let i = 0; i < 32; i++) {
            const h = 1 + Math.floor((Math.sin(ms / 110 + i * 1.3) * 0.5 + 0.5) * 7 * clamp((ms - 1500) / 400, 0, 1));
            fill_rect(2 + i * 4, 63 - h, 3, h, 1);
        }
    }
    if (ms > 1900) { const s = 'GROOVEBOX'; txt(Math.floor((128 - s.length * 6) / 2), 39, s, 1); }
}
function splashLeds(ms) {
    const pos = (ms - 200) / 1800 * 10 - 1;
    for (let i = 0; i < 32; i++) {
        const col = i & 7, row = i >> 3, d = Math.abs(col + row * 0.4 - pos);
        led(PAD_FIRST + i, d < 1.6 && ms < 2400 ? TRK_COL[(col * 2 + row) % 16] : 0);
    }
    for (let i = 0; i < 16; i++) led(STEP_FIRST + i, ms > 1300 && ms < 2700 && i <= (ms - 1300) / 60 ? 120 : 0);
}

globalThis.init = function () {
    crumb('init');
    DEF = { lens: lens.slice(), mutes: mutes.slice(), kind: kind.slice(), slot: slot.slice(), dsel: dsel.slice(), tv: tv.slice(), llen: llen.slice(), ed: ed.slice(), topModes: topModes.slice(), master: master.slice(), bpm: bpm, swing: swing };
    loadState();
    master[MK.indexOf('m_chp')] = 0;                          /* the comp detector high-pass was removed from the menu: always off */
    if (!freshState) backupState();
    if (freshState) { chosen.fill(0); }       /* a new project: track 1 is a Synth, every other track asks Synth or Drum */
    ensureCs(); for (let t = 0; t < NTRK; t++) applyClipSound(t, ed[t]);
    if (freshState) firstDrum(); crumb('state');
    queue = []; ledWant = {}; ledSent = {}; btnWant = {}; btnSent = {}; ledAge = 0; ledRot = 0; menuOpen = false; padMenu = false;
    sel = 0; view = 0; stepPage = 0; playing = false; gs = 0; heldStep = -1; heldPad = -1;
    qEverything(); initialPush = 100;
    crumb('queued'); needsRedraw = true; splashT0 = (globalThis.__NOSPLASH || !splashOn) ? 0 : Date.now(); noteInput(); draw(); crumb('drawn');
};

function syncPadsOff() {
    const want = (menuOpen || askKind >= 0 || shiftHeld || view === 7 || kind[sel] || topModes[sel] === 2) ? 1 : (topModes[sel] === 1 ? 3 : (topModes[sel] === 3 ? 4 : (chordOn() ? 5 : 0)));
    if (want !== padsOffSent) { padsOffSent = want; dspSet('padsoff', String(want)); }
}
function hostSuspend() {                                   /* keep Taktile running in the background (the sequencer keeps playing while you browse Move) */
    if (typeof host_suspend_overtake === 'function') { if (dirty) saveState(); suspended = true; exitDlg = -1; host_suspend_overtake(); return; }
    exitDlg = -1; showToast('BACKGROUND', 'NEEDS NEWER SCHWUNG', 0);
}
function exitModule() {
    if (playing) dspSet('play', '0');
    if (dirty) saveState();
    if (typeof host_exit_module === 'function') host_exit_module();
    else if (typeof host_return_to_menu === 'function') host_return_to_menu();
    else showToast('EXIT', 'Shift+Vol+Jog click', 0);
}
function tapTempo() {
    const now = Date.now();
    if (tapT.length && now - tapT[tapT.length - 1] > 2000) tapT = [];
    tapT.push(now); if (tapT.length > 5) tapT.shift();
    if (tapT.length >= 2) {
        const avg = (tapT[tapT.length - 1] - tapT[0]) / (tapT.length - 1);
        bpm = clamp(Math.round(60000 / avg), 40, 240); enqueue('bpm', 'bpm', String(bpm)); markEdited();
        showToast('TAP TEMPO', bpm + ' BPM', (bpm - 40) / 200);
    } else showToast('TAP TEMPO', 'tap again', 0);
}

try { favBuild(); } catch (e) { /* later */ }
globalThis.tick = function () {
    if (!bootTick) { bootTick = true; crumb('tick1'); }
    ticksSinceEdit++;
    if ((ticksSinceEdit & 7) === 0) { miscCheck(); chordReconcile(); }
    if (view === 50 && !menuOpen && pads[ln()][PI.grn] >= 0.01 && (ticksSinceEdit % 5) === 0) { grainPoll(); needsRedraw = true; }
    if (backTicks > 0) backTicks--;
    if (hot.t > 0 && --hot.t === 0) needsRedraw = true;
    if (clrPend.ticks > 0) clrPend.ticks--;
    ledAge++;
    if (fbPend > 0 && --fbPend === 0 && fb && !playing && !recArmed) { const tp = fb.st[fb.st.length - 1], pi = fbItems(tp.node)[tp.i]; if (pi && !pi.kid) { fbApply(); if (!tp.node.kits) fbAudition(); } }
    if (fbT > 0 && --fbT === 0 && fbOff) { dspSet(fbOff[0], fbOff[1]); fbOff = null; }
    if (deleteHeld && ++delAge > 300) { deleteHeld = false; needsRedraw = true; }      /* safety: Delete never stays on without any input */
    flushQueue(3);
    if (!queue.length) {
        if (initialPush > 0 && --initialPush === 0) qEverything();
        else if (healTicks > 0 && --healTicks === 0) { qPad(ln()); qSeq(ed[sel], ln()); }
        else if (heldStep < 0 && ticksSinceEdit > 60 && ++resyncT >= 25) {
            resyncT = 0;
            if (resyncSub >= lanesOf(resyncTrk).length) { resyncSub = 0; resyncTrk = (resyncTrk + 1) % NTRK; }
            const l2 = lanesOf(resyncTrk)[resyncSub++];
            if (laneHas(ed[resyncTrk], l2)) qSeq(ed[resyncTrk], l2);
            qPad(l2);
        }
    }
    if (saveFlash > 0 && --saveFlash === 0) needsRedraw = true;
    if (dirty && ticksSinceEdit > (playing ? 900 : 120)) { const el = Date.now() - bakT; if (el > 600000 && (!playing || el > 1800000)) { backupState(); bakT = Date.now(); } saveState(); }      /* while playing the file write waits (a big write could make the audio crackle) */

    const posNow = posFail < 20 && (playing || recDown || heldStep >= 0 || (ledAge & 3) === 0), r = posNow ? dspGet('pos') : null;
    if (typeof r === 'string' && r.indexOf(',') > 0) {
        const a = r.split(',');
        const np = a[0] === '1', ng = parseInt(a[1]) || 0;
        posFr = parseInt(a[3]) || 0;
        { const ci = parseInt(a[4]) || 0; if (ci !== ciBeats) { ciBeats = ci; needsRedraw = true; if (ci > 0) showToast('COUNT-IN', String(ci), ci / 4); } }
        if (np !== playing || ng !== gs) { playing = np; gs = ng; needsRedraw = true; }
        growTick(); loadTick(); autoPlay(); songTick(); rsTick(); meterTick(); scopeTick(); fillTick(); kitPrevTick(); if (hot.t > 0 && --hot.t === 0) needsRedraw = true;
        if (playing && followHold > 0) followHold--;
        if (playing && followHold === 0 && heldStep < 0 && LL() > 16) {   /* screen + step buttons follow the playing bar */
            const pg = Math.floor((((gs - 1 + 1024 * LL()) % LL())) / 16);
            if (pg !== stepPage && pg < pageCount()) { stepPage = pg; needsRedraw = true; }
        }
    } else if (posNow) posFail++;
    if (suspended) return;                                    /* in the background: the DSP plays on, the UI stays quiet */

    if (ledAge % 20 === 0) {
        const cp = dspGet('cpu');
        if (typeof cp === 'string' && cp.indexOf(',') > 0) { const a = cp.split(','); const na = parseInt(a[0]) || 0; if (na !== cpuAvg) { cpuAvg = na; needsRedraw = true; } if (cpuWarnT > 0) cpuWarnT--; else if (na >= 75) { cpuWarnT = 300; showToast('CPU HIGH ' + na + '%', 'LESS FX / PADS', 1); } cpuPeak = parseInt(a[1]) || 0; }
    }
    if (selfTestT === 0 && ledAge > 120) { selfTestT = 1; selfTest(); }
    tabNormalize(); if (isTab(view)) lastTab[sel] = view;
    if (askKind < 0 && !chosen[sel] && !menuOpen && heldStep < 0 && !(view === 28 && broOpen) && exitDlg < 0) { askKind = sel; askSel = kind[sel] ? 1 : 0; needsRedraw = true; }
    syncPadsOff();
    if (++clipPoll >= (view === 7 ? 4 : 20)) {
        clipPoll = 0;
        const cr = dspGet('clips');
        if (typeof cr === 'string') {
            const a = cr.split(',').map(Number);
            if (a.length === 32) {
                for (let t = 0; t < NTRK; t++) {
                    if (clipsCur[t] !== a[t] || clipsQ[t] !== a[16 + t]) needsRedraw = true;
                    clipsCur[t] = a[t]; clipsQ[t] = a[16 + t];
                }
            }
        }
    }
    if (touchK >= 0 && ++touchT > 3000) touchK = -1;          /* safety: a lost release never pins the display */
    if (toast && (touchK >= 0 ? false : --toast.ticks <= 0)) { toast = null; needsRedraw = true; }
    if (brwTicks > 0 && --brwTicks === 0) needsRedraw = true;
    if (idleOn && !idleBlank && !splashActive() && Date.now() - lastInputT > 300000) { idleBlank = true; splashT0 = Date.now(); needsRedraw = true; }
    if (idleBlank && !splashActive()) { if (splashT0 > 0) { splashT0 = 0; ledSent = {}; } for (let i = 0; i < 32; i++) led(PAD_FIRST + i, 0); for (let i = 0; i < 16; i++) led(STEP_FIRST + i, 0); for (let i = 0; i < 8; i++) btn(CC.KNOB1 + i, 0); flushLeds(); draw(); return; }
    if (splashActive()) { splashLeds(Date.now() - splashT0); flushLeds(); draw(); return; }
    if (splashT0 > 0) splashEnd();
    updateLeds(); flushLeds();
    if (needsRedraw) draw();
};

globalThis.onMidiMessageInternal = function (data) {
    if (!data) return;
    if (slowKnobs && (data[0] & 0xF0) === 0xB0 && (data[2] | 0) !== 0 && (data[1] === CC.JOG || (data[1] >= CC.KNOB1 && data[1] <= CC.KNOB8))) {
        const id = data[1], dl0 = decodeDelta(data[2] | 0);
        if (dl0) {
            if (slowAcc[id] && Math.sign(slowAcc[id]) !== Math.sign(dl0)) slowAcc[id] = 0;
            slowAcc[id] = (slowAcc[id] || 0) + dl0 * 0.5;
            const m = Math.trunc(slowAcc[id]); if (!m) return;
            slowAcc[id] -= m; data = [data[0], data[1], m > 0 ? Math.min(m, 63) : 128 + Math.max(m, -63)];
        }
    }
    if (monOpen) {
        const st0 = data[0] | 0;
        if (st0 === 0xF8 || st0 === 0xFE || st0 === 0xFA || st0 === 0xFB || st0 === 0xFC) return;
        if ((st0 & 0xF0) === 0xB0 && (data[1] | 0) === CC.BACK && (data[2] | 0) > 0) { monOpen = false; monLog = []; needsRedraw = true; return; }
        monPush(data); return;
    }
    if (((data[0] & 0xF0) === 0x90 && (data[2] | 0) > 0) || ((data[0] & 0xF0) === 0xB0 && (data[2] | 0) > 0)) {
        noteInput();
        if (idleBlank) { idleBlank = false; splashT0 = 0; ledSent = {}; btnSent = {}; ledAge = 0; needsRedraw = true; return; }
    }
    if (splashT0 > 0 && (data[2] | 0) > 0 && ((data[0] & 0xF0) === 0x90 || ((data[0] & 0xF0) === 0xB0 && data[1] !== 14 && !(data[1] >= 71 && data[1] <= 78)))) { splashEnd(); return; }
    viewItemsFix();
    const status = data[0] | 0, d1 = data[1] | 0, d2 = data[2] | 0, type = status & 0xF0;
    if ((type === 0x90 || type === 0x80) && d1 < 10) { if (type === 0x90 && d2 > 0 && d1 < 8) { touchK = d1; touchT = 0; knobTouch(d1); } else if (d1 < 8 && touchK === d1) { touchK = -1; if (toast) toast.ticks = Math.max(toast.ticks, 260); } return; }
    if (helpOpen && !(type === 0xB0 && d1 === CC.SHIFT)) { if (d2 > 0 && !(type === 0xB0 && d1 >= CC.KNOB1 && d1 <= CC.KNOB8)) { helpOpen = false; needsRedraw = true; } return; }
    if (status === 0xF8 || status === 0xF0 || status === 0xF7 || type === 0xA0 || type === 0xD0) return;

    if (projUI) { if (projInput(type, d1, d2)) return; }
    if (exitDlg >= 0) {                                       /* leave Taktile: keep it in the background or close it */
        if (type === 0xB0 && d1 === CC.JOG) { const dl = decodeDelta(d2); if (dl) { exitDlg = clamp(exitDlg + (dl > 0 ? 1 : -1), 0, 1); needsRedraw = true; } return; }
        if (type === 0xB0 && d1 === CC.JOGCLICK && d2 > 0) { const s = exitDlg; exitDlg = -1; needsRedraw = true; if (s === 0) hostSuspend(); else if (s === 1) exitModule(); return; }
        if (type === 0xB0 && d1 === CC.BACK && d2 > 0) { exitDlg = -1; needsRedraw = true; return; }
        if (type === 0xB0 && (d1 === CC.SHIFT || d1 === CC.PLAY)) { /* fall through */ } else return;
    }
    if (fb && !menuOpen && exitDlg < 0 && askKind < 0) {      /* folder browser: jog = up / down, click = open / keep, Back = up / cancel, Menu = cancel */
        if (type === 0xB0 && d1 === CC.JOG) { const dl = decodeDelta(d2); if (dl) { fbAcc += dl; if (Math.abs(fbAcc) >= 1) { fbMove(fbAcc > 0 ? 1 : -1); fbAcc = 0; } } return; }      /* one click per entry */
        if (type === 0xB0 && d1 === CC.KNOB1) { const dl = decodeDelta(d2); if (dl) fbMove(dl, 5); return; }      /* knob 1 = fast scroll, 10 entries per click */
        if (type === 0xB0 && d1 === CC.JOGCLICK) { if (d2 > 0) { if (shiftHeld) fbFav(); else fbClick(); } return; }
        if (type === 0xB0 && d1 === CC.BACK) { if (d2 > 0) fbBack(); return; }
        if (type === 0xB0 && d1 === CC.MENU) { if (d2 > 0) { while (fb) fbBack(); } return; }
    }
    if (askKind >= 0) {                                       /* first question for every new track: Synth or Drum */
        if (type === 0x90 && d2 > 0 && d1 >= PAD_FIRST && d1 <= PAD_LAST) { askDone(((d1 - PAD_FIRST) & 7) < 4 ? 0 : 1); return; }
        if (type === 0xB0 && d1 === CC.JOG) { const dl = decodeDelta(d2); if (dl) { askSel = dl > 0 ? 1 : 0; needsRedraw = true; } return; }
        if (type === 0xB0 && d1 === CC.JOGCLICK && d2 > 0) { askDone(askSel); return; }
        if (type === 0xB0 && d1 === CC.BACK && d2 > 0) { askDone(-1); return; }
        if (type === 0xB0 && (d1 === CC.SHIFT || d1 === CC.PLAY || (d1 >= CC.KNOB1 && d1 <= CC.KNOB8))) { /* fall through */ } else return;
    }
    if (type === 0x90 || type === 0x80) {
        delAge = 0;
        const on = type === 0x90 && d2 > 0;
        if (d1 >= PAD_FIRST && d1 <= PAD_LAST) onPad(d1 - PAD_FIRST, on, d2);
        else if (d1 >= STEP_FIRST && d1 <= STEP_LAST && !menuOpen) onStep(d1 - STEP_FIRST, on);
        return;
    }
    if (type !== 0xB0) return;
    if (d1 !== CC.DELETE) delAge = 0;
    if (d1 === CC.DELETE && d2 === 0 && deleteHeld) { deleteHeld = false; needsRedraw = true; }        /* a released Delete always ends delete mode, also while a menu or dialog is open */
    if (d2 > 0 && deleteHeld && (d1 === CC.BACK || d1 === CC.PLAY || d1 === CC.REC || d1 === CC.MENU || d1 === CC.UNDO)) { deleteHeld = false; needsRedraw = true; }      /* these buttons never belong to a delete: a stuck Delete is cleared by them */
    if (menuOpen && d1 !== CC.SHIFT) {
        if (d1 === CC.JOG) { const dl = decodeDelta(d2); if (dl) { if (shiftHeld && gName() === 'FIND') findStep(dl); else menuMove(dl); } }
        else if (d1 === CC.KNOB1) { const dl = decodeDelta(d2); if (dl) { menuKA += dl; if (Math.abs(menuKA) >= 2) { const sg = menuKA > 0 ? 1 : -1; menuKA = 0; if (shiftHeld && gName() === 'FIND') findStep(sg); else menuMove(sg); } } }      /* knob 1 scrolls the menus and the pickers (themes, language) like the jogwheel */
        else if (d1 === CC.JOGCLICK) { if (d2 > 0) menuSelect(); }
        else if (d1 === CC.BACK) { if (d2 > 0) menuBack(); }
        else if (d1 === CC.MENU) { if (d2 > 0) { themeRestore(); menuOpen = false; menuGrp = -1; needsRedraw = true; } }
        return;
    }
    switch (d1) {
    case CC.SHIFT:  shiftHeld = d2 > 0; syncPadsOff(); return;
    case CC.COPY:   copyHeld = d2 > 0; if (!copyHeld) { stepClip = null; songCopySrc = -1; } return;
    case CC.DELETE: if (view === 31 && !menuOpen && (d2 > 0 || songDelOn)) { if (d2 > 0) { deleteHeld = true; songDelOn = true; songDelUsed = false; delAge = 0; } else { songDelOn = false; deleteHeld = false; if (!songDelUsed) songDel(); } needsRedraw = true; return; }      /* song: Delete held + step pad = delete that step; a plain tap deletes the selected step */
        if (d2 > 0 && view === 45 && !menuOpen && !fb) { devClear(); return; } deleteHeld = d2 > 0 && !deleteHeld; delAge = 0; needsRedraw = true; return;      /* pressing again while it seems stuck (lost release) switches it off */
    case CC.LOOP:   loopHeld = d2 > 0; if (d2 > 0) { snapEu = false; showToast('LOOP HELD', 'TURN JOG = LENGTH', LL() / MAXSTEP); } return;
    case CC.UNDO:   if (d2 > 0) { if (shiftHeld) doRedo(); else doUndo(); } return;
    case CC.SAMPLE: if (d2 > 0) { if (copyHeld) opVary(); else randomize(shiftHeld); } return;
    case CC.MENU:
        if (d2 > 0 && shiftHeld) { helpOpen = true; needsRedraw = true; return; }
        if (d2 > 0) {                                        /* |||: session / clip view stays until pressed again */
            padMenu = false; menuOpen = false;
            if (view === 7) view = prevView;
            else { prevView = (SYM_VIEW_SET[view] || view === 7) ? 0 : view; view = 7; }
            symOpen = -1; acc.fill(0); needsRedraw = true;
        }
        return;
    case 43: case 42: case 41: case 40: {                    /* track button: hold + master knob = track volume, hold + Mute = mute; tap = select, tap again = pad mode */
        const t = bank * 4 + (43 - d1);
        if (d2 > 0) {
            trkHeld = t; trkUsed = false; suppressMasterVol(1);
            if (muteHeld) { trkUsed = true; muteUsed = true; if (shiftHeld) soloTrack(t); else toggleMute(t); }          /* Mute held + track button = mute / unmute */
            else if (copyHeld && t !== sel && !deleteHeld) { trkUsed = true; if (shiftHeld) copyTrackFull(sel, t); else copySound(sel, t); }                           /* Copy held + track button = copy the selected sound (with all its effects) there */
        }
        else if (trkHeld === t) {
            trkHeld = -1; suppressMasterVol(0);
            if (!trkUsed) {
                if (deleteHeld) { if (clrPend.t === t && clrPend.ticks > 0) { clrPend = { t: -1, ticks: 0 }; clearTrack(t); } else { clrPend = { t: t, ticks: 70 }; showToast('CLEAR TRACK ' + p2(t + 1) + '?', 'AGAIN = YES', 0); } }
                else if (shiftHeld) { selectTrack(t); setKind(t, kind[t] ? 0 : 1); }
                else if (view === 7 && !menuOpen && !loopHeld) { selectTrack(t); view = 0; navStack = []; padMenu = false; symOpen = -1; acc.fill(0); needsRedraw = true; showToast('TRACK ' + p2(t + 1), trkName(t), t / (NTRK - 1)); }      /* clip view: the track button opens that track */
                else if (t === sel) showToast('TRACK ' + p2(t + 1), trkName(t), t / (NTRK - 1));      /* the pad mode changes only with Shift + step 8 */
                else { selectTrack(t); showToast('TRACK ' + p2(t + 1), trkName(t), t / (NTRK - 1)); }
            }
        }
        return; }
    case CC.MASTERK: {
        if (trkHeld >= 0) { trackVol(trkHeld, decodeDelta(d2)); return; }      /* track button held: the volume knob sets that track's volume */
        if (drumHeldK.length && kind[sel]) { const dl0 = decodeDelta(d2); if (dl0) { const k = drumHeldK[drumHeldK.length - 1], l = laneOf(sel, k), v = clamp(pads[l][PI.vol] + Math.sign(dl0) * Math.min(Math.abs(dl0), 3) * 0.02, 0, 1); setLaneP(l, 'vol', v); showToast('VOL SOUND ' + (k + 1), String(Math.round(v * 100)), v); needsRedraw = true; } return; }      /* sound pad held: the volume knob sets that sound's volume */ const dl = decodeDelta(d2); if (dl) { const r = editParam('master', 'm_vol', Math.sign(dl) * Math.min(Math.abs(dl), 3), 7); if (r) { showToast('MASTER VOL', fmtVal('m_vol', r.v), r.v); markEdited(); } } return; }
    case CC.MUTE:
        if (d2 > 0) { muteHeld = true; muteUsed = false; soloAtPress = shiftHeld;
            if (trkHeld >= 0) { trkUsed = true; muteUsed = true; if (shiftHeld) soloTrack(trkHeld); else toggleMute(trkHeld); } }
        else { muteHeld = false; if (!muteUsed) { if (soloAtPress) soloTrack(sel); else toggleMute(sel); } }
        needsRedraw = true; return;
    case CC.PLAY:
        if (d2 > 0 && view === 28 && broOpen && !menuOpen) { broPreview(); return; }
        if (d2 > 0) {
            if (shiftHeld) { playing = false; gs = 0; dspSet('play', '0'); dspSet('panic', '1'); showToast('TRANSPORT', 'STOP + RESET + PANIC', 0); }
            else if (!playing && ciBeats > 0) { dspSet('play', '0'); ciBeats = 0; showToast('COUNT-IN', 'CANCELLED', 0); }
            else if (!playing && recArmed && ciOn) { dspSet('play', '2'); showToast('COUNT-IN', '1 BAR', 1); }      /* Rec armed + Play: one bar of clicks, then the recording starts */
            else { if (view === 31 && !playing && !songOn) { songOn = true; songLoop = true; songSent = -1; songLaunched = -1; songOff = 0; songNext = -1; } playing = !playing; gs = 0; dspSet('play', playing ? '1' : '0'); showToast('TRANSPORT', playing ? 'PLAY' : 'STOP', playing ? 1 : 0); }
            needsRedraw = true;
        }
        return;
    case CC.BACK:
        if (d2 > 0 && view === 45 && !fb && !menuOpen) { view = devPrev; acc.fill(0); needsRedraw = true; return; }
        if (d2 > 0 && broOpen) { broOpen = false; dspSet('brstop', '1'); dirty = true; needsRedraw = true; return; }
        if (d2 > 0 && view === 31 && songFromClips) { view = 7; songFromClips = false; acc.fill(0); needsRedraw = true; return; }
        if (d2 > 0 && (isHub(view) || navStack.length)) { hubBack(); return; }
        if (d2 > 0 && anySymLit()) { closeSym(); return; }
        if (d2 > 0 && padMenu) { padMenu = false; view = prevPM; needsRedraw = true; return; }
        if (d2 > 0) {
            if (shiftHeld) exitModule();
            else { exitDlg = 0; needsRedraw = true; }
        }
        return;
    case CC.REC:
        if (d2 > 0 && shiftHeld) { const v = cycleMaster('m_met', [0, 0.6]); showToast('METRONOME', v > 0 ? 'ON' : 'OFF', v); recSwallow = true; return; }       /* Shift + Rec = metronome */
        if (d2 === 0 && recSwallow) { recSwallow = false; return; }
        if (d2 > 0) { recDown = true; recStepUsed = false; }
        else {
            recDown = false;
            if (recStepUsed) showToast('REC', 'STEP REC END', 0);
            else { if (recArmed) recStopLen(); recArmed = !recArmed; growTrk.fill(false); if (recArmed === false) {} showToast('REC', recArmed ? 'ARMED' : 'OFF', recArmed ? 1 : 0); }
            needsRedraw = true;
        }
        return;
    case CC.LEFT:  if (d2 > 0 && recDown && !playing) { recHead = (recHead + LL() - 1) % LL(); recStepUsed = true; showToast('REC HEAD', String(recHead + 1), recHead / MAXSTEP); return; }
    case CC.LEFT_:  if (d2 > 0) { if (shiftHeld) { if (pageCount() > 1) { stepPage = clamp(stepPage - 1, 0, pageCount() - 1); followHold = 180; showToast('STEP PAGE', (stepPage + 1) + '/' + pageCount(), stepPage / Math.max(1, pageCount() - 1)); needsRedraw = true; } } else if (!menuOpen && !fb && askKind < 0 && heldStep < 0 && !projUI) { const nt = clamp(sel + (-1), 0, NTRK - 1); if (nt !== sel) { selectTrack(nt); showToast('TRACK ' + p2(nt + 1), kind[nt] ? 'DRUM' : 'SYNTH', nt / (NTRK - 1)); } } } return;
    case CC.RIGHT: if (d2 > 0 && recDown && !playing) { recHead = (recHead + 1) % LL(); recStepUsed = true; showToast('REC HEAD', String(recHead + 1), recHead / MAXSTEP); return; }
    case CC.RIGHT_: if (d2 > 0) { if (shiftHeld) { if (pageCount() > 1) { stepPage = clamp(stepPage + 1, 0, pageCount() - 1); followHold = 180; showToast('STEP PAGE', (stepPage + 1) + '/' + pageCount(), stepPage / Math.max(1, pageCount() - 1)); needsRedraw = true; } } else if (!menuOpen && !fb && askKind < 0 && heldStep < 0 && !projUI) { const nt = clamp(sel + (1), 0, NTRK - 1); if (nt !== sel) { selectTrack(nt); showToast('TRACK ' + p2(nt + 1), kind[nt] ? 'DRUM' : 'SYNTH', nt / (NTRK - 1)); } } } return;
    case CC.CAPTURE: if (d2 > 0) { if (shiftHeld) fillBar(); else doCapture(); } return;
    case CC.UP:    if (d2 > 0) { if (view === 7 || shiftHeld || recDown) stepTrack(-1); else octMove(1); } return;      /* Up / Down = pad octave; Shift + Up / Down (or the clip view) = the track above / below */
    case CC.DOWN:  if (d2 > 0) { if (view === 7 || shiftHeld || recDown) stepTrack(1); else octMove(-1); } return;
    case CC.JOG: {
        const dl = decodeDelta(d2);
        if (loopHeld && !menuOpen && !fb && view !== 31 && !(view === 28 && broOpen)) { if (dl) setLen(LL() + (dl > 0 ? 1 : -1) * (shiftHeld ? 16 : 1), shiftHeld); return; }   /* Loop + jogwheel = loop length (Shift = whole track), on every page, before anything else */
        if (trkHeld >= 0) { trackVol(trkHeld, dl); return; }                  /* track button held + jogwheel works as well */
        if (isHub(view)) { if (dl) hubMove(dl); return; }
        if (view === 45 && !menuOpen && !fb) { if (dl) devMove(dl); return; }
        if (view === 40) { if (dl) { histSel = clamp(histSel + (dl > 0 ? 1 : -1), 0, Math.max(0, undoStack.length - 1)); needsRedraw = true; } return; }
        if (isTab(view) && !menuOpen && !(view === 28 && broOpen) && (view === 28 ? (smpPage === 1 ? shiftHeld : true) : (!shiftHeld || view !== 1))) { if (dl) tabMove(dl); return; }
        if (view === 28 && broOpen && !menuOpen) { if (dl) { broSel = clamp(broSel + (dl > 0 ? 1 : -1), 0, broList.length - 1); needsRedraw = true; } return; }
        if (view === 28 && smpPage === 1 && !padMenu && !menuOpen) { if (dl) { slSel = clamp(slSel + (dl > 0 ? 1 : -1), 0, slN - 1); needsRedraw = true; } return; }
        if (view === 28 && smpPage >= 2 && !padMenu && !menuOpen) return;
        if (view === 28 && !padMenu && !menuOpen) { if (dl) { selectSample(dl > 0 ? 1 : -1); needsRedraw = true; } return; }
        if (view === 20) { if (dl) { mixPage ^= 1; needsRedraw = true; } return; }
        if (padMenu) {
            if (dl) { if (isTab(view)) tabMove(dl); else { tabEnter(1); } }
            return; }
        if (dl && view === 7 && !menuOpen) { stepTrack(dl > 0 ? 1 : -1); }   /* clip view: jog = scroll through the tracks (right = down) */
        else if (dl && !menuOpen && view === 0) {                          /* SEQ: turning walks through the track pages (SOUND, FILTER, MOD ...), a click opens the preset folders */
            if (shiftHeld) fbOpen();
            else { navStack = [0]; hubReturn = 0; symOpen = -1; tabEnter(1); showToast(TAB_NAME[1], 'TURN = NEXT PAGE', 0); }
        }
        else if (dl && !menuOpen && shiftHeld && askKind < 0 && !fb && (view === 1 || ((isTab(view) || padView(view)) && view !== 28 && view !== 36))) fbOpen();      /* Shift + jog turn on any sound page = the preset browser */
        else if (dl && !menuOpen) jogFirst(dl);                              /* every other page: the jogwheel turns the first parameter (like knob 1) */
        return; }
    case CC.JOGCLICK:
        if (d2 > 0 && view === 45 && !menuOpen && !fb && !shiftHeld) { devClick(); return; }
        if (d2 > 0 && view === 42 && !menuOpen && !shiftHeld) { scopeMode = (scopeMode + 1) % 6; needsRedraw = true; return; }
        if (d2 > 0 && view === 40 && !menuOpen && !shiftHeld) { if (undoStack.length) histApply(); return; }
        if (d2 > 0 && view === 20 && !menuOpen && !padMenu && !shiftHeld) { mixMode = (mixMode + 1) % 3; needsRedraw = true; showToast('MIX', ['VOLUME', 'DELAY SEND', 'REVERB SEND'][mixMode], mixMode / 2); return; }
        if (d2 > 0 && (view === 0 || view === 1) && !menuOpen && !padMenu && !shiftHeld && askKind < 0 && !fb) { fbOpen(); acc.fill(0); needsRedraw = true; return; }   /* jog click on SEQ/SOUND = preset browser (HOME: Shift + step 3) */
        if (d2 > 0 && view === 9 && !menuOpen && !padMenu && !shiftHeld) { synPage ^= 1; acc.fill(0); showToast(String(synPage + 1), '', 0); toast.ticks = 40; needsRedraw = true; return; }
        if (d2 > 0 && (view === 38 || view === 27 || view === 2 || view === 43 || view === 46 || view === 50) && !menuOpen && !padMenu && !shiftHeld) {      /* second / third page of FX (TIME, TYPE) and of FILTER (EQ) */
            let pg;
            if (view === 2) { view = 43; pg = 2; }
            else if (view === 43) { view = 2; pg = 1; }
            else if (view === 38 && !fxPage) { view = 50; pg = 2; }
            else if (view === 50) { view = 38; fxPage = 0; pg = 1; }
            else if (view === 46) { view = 38; fxPage = 1; pg = 2; }
            else if (view === 38) { view = 46; fxPage = 0; pg = 1; }
            else { fxPage ^= 1; pg = fxPage + 1; }
            acc.fill(0); showToast(String(pg), '', 0); toast.ticks = 40; needsRedraw = true; return;
        }
        if (d2 > 0 && isTab(view) && view !== 36 && view !== 28 && !menuOpen && !shiftHeld) return;      /* tabs without a second page: the jog click does nothing */
        if (d2 > 0 && view === 36 && !menuOpen && !padMenu && !shiftHeld) { euSetPage(euPage + 1); showToast((euPage + 1) + '/' + EU_NPG, '', 0); toast.ticks = 40; return; }
        if (d2 > 0 && view === 28 && broOpen && !menuOpen) { broClick(); return; }
        if (d2 > 0 && view === 28 && !menuOpen && !padMenu) { if (shiftHeld) { if (smpPage === 1) sliceApply(); else if (smpPage === 0) broOpenNow(); else sampleRescan(); } else { smpPage = (smpPage + 1) % 4; acc.fill(0); showToast(['SAMPLE', 'SLICE', 'ENVELOPE', 'TOOLS'][smpPage], ['START / END', 'MARKERS', 'GAIN / ATK / REL', 'FIT / HALF / MOVE'][smpPage], smpPage / 3); needsRedraw = true; } return; }
        if (d2 > 0 && shiftHeld && view === 10) { tapTempo(); return; }      /* tap tempo lives on the TEMPO page now */
        if (d2 > 0) {
            if (isHub(view)) hubEnter();
            else openHub(16);
            acc.fill(0); needsRedraw = true;
        }
        return;
    }
    if (d1 >= CC.KNOB1 && d1 <= CC.KNOB8) {
        const i = d1 - CC.KNOB1, rawD = decodeDelta(d2), delta = Math.sign(rawD) * Math.min(Math.abs(rawD), 3);
        if (!delta) return;
        if (heldStep >= 0 && stepWinView()) { stepKnob(i, delta); return; }
        if (loopHeld && view === 0 && i < 2) { euclidKnob(i, delta); return; }
        if (loopHeld && view === 0 && i === 2) { setLen(LL() + delta * (shiftHeld ? 16 : 1), false); return; }
        if (view === 0) { seqKnob(i, delta); return; }
        if (view === 7) { sessionKnob(i, delta); return; }
        if (view === 20) { mixKnob(i, delta); return; }
        if (view === 45) devSync();
        const items = VIEW_ITEMS[itemsView()];
        if (!items || i >= items.length) return;
        if (!shiftHeld && view === 1 && items[i][0] === 'type') { acc[i] += delta; if (Math.abs(acc[i]) < 3) return; acc[i] = 0; browseSound(delta > 0 ? 1 : -1); return; }       /* sound knob = list that opens and scrolls */
        if (!shiftHeld && view === 28 && !smpPage && items[i][0] === 'smp') { acc[i] += delta; if (Math.abs(acc[i]) < 2) return; sampleList(acc[i] > 0 ? 1 : -1); acc[i] = 0; return; }
        const ts0 = toastSeq;
        lastKnob = items[i][0]; hot = { v: view, i: i, t: 75 };
        if (editParam(scopeOf(items[i][0]), items[i][0], delta, i)) { markEdited(); if (toastSeq !== ts0) return; if (!bigOn && !CURVE_KEYS[items[i][0]]) { needsRedraw = true; return; } const v2 = getVal(scopeOf(items[i][0]), items[i][0]); curveKey = items[i][0]; showToast(fullLabel(view, i, items).toUpperCase(), fmtVal(items[i][0], v2), fracVal(items[i][0], v2)); if (toast) toast.nm = 1; }
    }
};

globalThis.onResume = function () {
    suspended = false; ledWant = {}; ledSent = {}; btnWant = {}; btnSent = {}; ledAge = 0; initialPush = 5; padsOffSent = -1; needsRedraw = true;
};
globalThis.onUnload = function () {
    suppressMasterVol(0);
    if (playing) dspSet('play', '0');
    if (dirty) saveState();
    backupState();
};


/* ---- safety net: an exception must not close the module. Show it and keep a log. ---- */
let lastErr = '', errCount = 0, errShow = 0;
function drawErr() {
    clear_screen();
    print(2, 2, 'TAKTILE ERROR ' + errCount, 1);
    const m = lastErr; for (let i = 0; i < 5; i++) print(2, 14 + i * 10, m.substring(i * 21, i * 21 + 21), 1);
}
function logErr(where, e) {
    errCount++;
    const msg = where + ': ' + (e && e.message ? e.message : String(e));
    lastErr = msg;
    try {
        if (typeof host_write_file === 'function')
            host_write_file('/data/UserData/schwung/taktile-error.log', 'count ' + errCount + '\n' + msg + '\n' + (e && e.stack ? e.stack : '') + '\n');
    } catch (e2) { /* ignore */ }
    errShow = 600; needsRedraw = true;
    try { drawErr(); } catch (e3) { /* ignore */ }
}
function guarded(where, f) { return function () { try { return f.apply(this, arguments); } catch (e) { logErr(where, e); } }; }
globalThis.init = guarded('init', globalThis.init);
globalThis.tick = guarded('tick', globalThis.tick);
globalThis.onMidiMessageInternal = guarded('midi', globalThis.onMidiMessageInternal);
const _unload = globalThis.onUnload;
globalThis.onUnload = function () { try { _unload(); } catch (e) { /* ignore */ } };
