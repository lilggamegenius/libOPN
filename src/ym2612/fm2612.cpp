/*
**
** File: fm2612.c -- software implementation of Yamaha YM2612 FM sound generator
** Split from fm.c to keep 2612 fixes from infecting other OPN chips
**
** Copyright Jarek Burczynski (bujar at mame dot net)
** Copyright Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 1.5.1 (Genesis Plus GX ym2612.c rev. 368)
**
*/

/*
** History:
**
** 2006~2012  Eke-Eke (Genesis Plus GX):
** Huge thanks to Nemesis, lot of those fixes came from his tests on Sega Genesis hardware
** More informations at http://gendev.spritesmind.net/forum/viewtopic.php?t=386
**
**  TODO:
**
**  - core documentation
**  - BUSY flag support
**
**  CHANGELOG:
**
**  - fixed LFO implementation:
**      .added support for CH3 special mode: fixes various sound effects (birds in Warlock, bug sound in Aladdin...)
**      .inverted LFO AM waveform: fixes Spider-Man & Venom : Separation Anxiety (intro), California Games (surfing event)
**      .improved LFO timing accuracy: now updated AFTER sample output, like EG/PG updates, and without any precision loss anymore.
**  - improved internal timers emulation
**  - adjusted lowest EG rates increment values
**  - fixed Attack Rate not being updated in some specific cases (Batman & Robin intro)
**  - fixed EG behavior when Attack Rate is maximal
**  - fixed EG behavior when SL=0 (Mega Turrican tracks 03,09...) or/and Key ON occurs at minimal attenuation
**  - implemented EG output immediate changes on register writes
**  - fixed YM2612 initial values (after the reset): fixes missing intro in B.O.B
**  - implemented Detune overflow (Ariel, Comix Zone, Shaq Fu, Spiderman & many other games using GEMS sound engine)
**  - implemented accurate CSM mode emulation
**  - implemented accurate SSG-EG emulation (Asterix, Beavis&Butthead, Bubba'n Stix & many other games)
**  - implemented accurate address/data ports behavior
**
** 06-23-2007 Zsolt Vasvari:
**  - changed the timing not to require the use of floating point calculations
**
** 03-08-2003 Jarek Burczynski:
**  - fixed YM2608 initial values (after the reset)
**  - fixed flag and irqmask handling (YM2608)
**  - fixed BUFRDY flag handling (YM2608)
**
** 14-06-2003 Jarek Burczynski:
**  - implemented all of the YM2608 status register flags
**  - implemented support for external memory read/write via YM2608
**  - implemented support for deltat memory limit register in YM2608 emulation
**
** 22-05-2003 Jarek Burczynski:
**  - fixed LFO PM calculations (copy&paste bugfix)
**
** 08-05-2003 Jarek Burczynski:
**  - fixed SSG support
**
** 22-04-2003 Jarek Burczynski:
**  - implemented 100% correct LFO generator (verified on real YM2610 and YM2608)
**
** 15-04-2003 Jarek Burczynski:
**  - added support for YM2608's register 0x110 - status mask
**
** 01-12-2002 Jarek Burczynski:
**  - fixed register addressing in YM2608, YM2610, YM2610B chips. (verified on real YM2608)
**    The addressing patch used for early Neo-Geo games can be removed now.
**
** 26-11-2002 Jarek Burczynski, Nicola Salmoria:
**  - recreated YM2608 ADPCM ROM using data from real YM2608's output which leads to:
**  - added emulation of YM2608 drums.
**  - output of YM2608 is two times lower now - same as YM2610 (verified on real YM2608)
**
** 16-08-2002 Jarek Burczynski:
**  - binary exact Envelope Generator (verified on real YM2203);
**    identical to YM2151
**  - corrected 'off by one' error in feedback calculations (when feedback is off)
**  - corrected connection (algorithm) calculation (verified on real YM2203 and YM2610)
**
** 18-12-2001 Jarek Burczynski:
**  - added SSG-EG support (verified on real YM2203)
**
** 12-08-2001 Jarek Burczynski:
**  - corrected sin_tab and tl_tab data (verified on real chip)
**  - corrected feedback calculations (verified on real chip)
**  - corrected phase generator calculations (verified on real chip)
**  - corrected envelope generator calculations (verified on real chip)
**  - corrected FM volume level (YM2610 and YM2610B).
**  - changed YMxxxUpdateOne() functions (YM2203, YM2608, YM2610, YM2610B, YM2612) :
**    this was needed to calculate YM2610 FM channels output correctly.
**    (Each FM channel is calculated as in other chips, but the output of the channel
**    gets shifted right by one *before* sending to accumulator. That was impossible to do
**    with previous implementation).
**
** 23-07-2001 Jarek Burczynski, Nicola Salmoria:
**  - corrected YM2610 ADPCM type A algorithm and tables (verified on real chip)
**
** 11-06-2001 Jarek Burczynski:
**  - corrected end of sample bug in ADPCMA_calc_cha().
**    Real YM2610 checks for equality between current and end addresses (only 20 LSB bits).
**
** 08-12-98 hiro-shi:
** rename ADPCMA -> ADPCMB, ADPCMB -> ADPCMA
** move ROM limit check.(CALC_CH? -> 2610Write1/2)
** test program (ADPCMB_TEST)
** move ADPCM A/B end check.
** ADPCMB repeat flag(no check)
** change ADPCM volume rate (8->16) (32->48).
**
** 09-12-98 hiro-shi:
** change ADPCM volume. (8->16, 48->64)
** replace ym2610 ch0/3 (YM-2610B)
** change ADPCM_SHIFT (10->8) missing bank change 0x4000-0xffff.
** add ADPCM_SHIFT_MASK
** change ADPCMA_DECODE_MIN/MAX.
*/

/************************************************************************/
/*    comment of hiro-shi(Hiromitsu Shioya)                             */
/*    YM2610(B) = OPN-B                                                 */
/*    YM2610  : PSG:3ch FM:4ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/*    YM2610B : PSG:3ch FM:6ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/************************************************************************/

#include "fm2612.hpp"
#include "gcem.hpp"
#include "mamedef.hpp"

#include <algorithm>
#include <array>

/* globals */
constexpr auto FREQ_SH = 16; /* 16.16 fixed point (frequency calculations) */
constexpr auto EG_SH = 16;   /* 16.16 fixed point (envelope generator timing) */
constexpr auto LFO_SH = 24;  /*  8.24 fixed point (LFO calculations)       */

constexpr auto FREQ_MASK = (1 << FREQ_SH) - 1;

/* envelope generator */
constexpr auto ENV_BITS = 10;
constexpr auto ENV_LEN = 1 << ENV_BITS;
constexpr auto ENV_STEP = 128.0 / ENV_LEN;

constexpr auto MAX_ATT_INDEX = ENV_LEN - 1; /* 1023 */
constexpr auto MIN_ATT_INDEX = 0;           /* 0 */

enum EG : uint8_t {
	Off,
	Release,
	Sustain,
	Decay,
	Attack
};

/* operator unit */
constexpr auto SIN_BITS = 10;
constexpr auto SIN_LEN = 1 << SIN_BITS;
constexpr auto SIN_MASK = SIN_LEN - 1;

constexpr auto TL_RES_LEN = 256; /* 8 bits addressing (real chip) */

/*  TL_TAB_LEN is calculated as:
*   13 - sinus amplitude bits     (Y axis)
*   2  - sinus sign bit           (Y axis)
*   TL_RES_LEN - sinus resolution (X axis)
*/
constexpr auto TL_TAB_LEN = 13 * 2 * TL_RES_LEN;
consteval auto generate_tl_tab() {
	/* build Linear Power Table */
	std::array<signed int, TL_TAB_LEN> tl_tab{};
	for(signed int x = 0; x < TL_RES_LEN; x++) {
		double m = (1 << 16) / gcem::pow(2, (x + 1) * (ENV_STEP / 4.0) / 8.0);

		/* we never reach (1<<16) here due to the (x+1) */
		/* result fits within 16 bits at maximum */

		auto n = static_cast<int>(gcem::floor(m)); /* 16 bits here */
		n >>= 4;                      /* 12 bits here */
		if(n & 1) {                   /* round to nearest */
			n = (n >> 1) + 1;
		} else {
			n = n >> 1;
		}
		/* 11 bits here (rounded) */
		n <<= 2; /* 13 bits here (as in real chip) */

		/* 14 bits (with sign bit) */
		tl_tab[x * 2 + 0] = n;
		tl_tab[x * 2 + 1] = -tl_tab[x * 2 + 0];

		/* one entry in the 'Power' table use the following format, xxxxxyyyyyyyys with:            */
		/*        s = sign bit                                                                      */
		/* yyyyyyyy = 8-bits decimal part (0-TL_RES_LEN)                                            */
		/* xxxxx    = 5-bits integer 'shift' value (0-31) but, since Power table output is 13 bits, */
		/*            any value above 13 (included) would be discarded.                             */
		for(signed int i = 1; i < 13; i++) {
			tl_tab[x * 2 + 0 + i * 2 * TL_RES_LEN] = tl_tab[x * 2 + 0] >> i;
			tl_tab[x * 2 + 1 + i * 2 * TL_RES_LEN] = -tl_tab[x * 2 + 0 + i * 2 * TL_RES_LEN];
		}
	}
	return tl_tab;
}
constexpr auto tl_tab = generate_tl_tab();

constexpr auto ENV_QUIET = TL_TAB_LEN >> 3;

/* sin waveform table in 'decibel' scale */
consteval auto generate_sin_tab() {
	/* build Logarithmic Sinus table */
	std::array<signed int, SIN_LEN> sin_tab{};
	for(signed int i = 0; i < SIN_LEN; i++) {
		/* non-standard sinus */
		double m = gcem::sin(((i * 2) + 1) * M_PI / SIN_LEN); /* checked against the real chip */
		/* we never reach zero here due to ((i*2)+1) */
		double o;
		if(m > 0.0) {
			o = 8 * gcem::log(1.0 / m) / gcem::log(2.0); /* convert to 'decibels' */
		} else {
			o = 8 * gcem::log(-1.0 / m) / gcem::log(2.0);
		} /* convert to 'decibels' */

		o = o / (ENV_STEP / 4);

		auto n = static_cast<int>(2.0 * o);
		if(n & 1) { /* round to nearest */
			n = (n >> 1) + 1;
		} else {
			n = n >> 1;
		}

		/* 13-bits (8.5) value is formatted for above 'Power' table */
		sin_tab[i] = n * 2 + (m >= 0.0 ? 0 : 1);
	}
	return sin_tab;
}
constexpr auto sin_tab = generate_sin_tab();

/* sustain level table (3dB per step) */
/* bit0, bit1, bit2, bit3, bit4, bit5, bit6 */
/* 1,    2,    4,    8,    16,   32,   64   (value)*/
/* 0.75, 1.5,  3,    6,    12,   24,   48   (dB)*/

/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
/* attenuation value (10 bits) = (SL << 2) << 3 */
#define SC(db) (uint32_t)(db * (4.0 / ENV_STEP))
constexpr std::array<uint32_t, 16> sl_table = {
        SC(0), SC(1), SC(2), SC(3), SC(4), SC(5), SC(6), SC(7),
        SC(8), SC(9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)};
#undef SC

constexpr auto RATE_STEPS = 8;
constexpr std::array<uint8_t, 19 *RATE_STEPS> eg_inc = {

        /*cycle:0 1  2 3  4 5  6 7*/

        /* 0 */ 0, 1, 0, 1, 0, 1, 0, 1, /* rates 00..11 0 (increment by 0 or 1) */
        /* 1 */ 0, 1, 0, 1, 1, 1, 0, 1, /* rates 00..11 1 */
        /* 2 */ 0, 1, 1, 1, 0, 1, 1, 1, /* rates 00..11 2 */
        /* 3 */ 0, 1, 1, 1, 1, 1, 1, 1, /* rates 00..11 3 */

        /* 4 */ 1, 1, 1, 1, 1, 1, 1, 1, /* rate 12 0 (increment by 1) */
        /* 5 */ 1, 1, 1, 2, 1, 1, 1, 2, /* rate 12 1 */
        /* 6 */ 1, 2, 1, 2, 1, 2, 1, 2, /* rate 12 2 */
        /* 7 */ 1, 2, 2, 2, 1, 2, 2, 2, /* rate 12 3 */

        /* 8 */ 2, 2, 2, 2, 2, 2, 2, 2, /* rate 13 0 (increment by 2) */
        /* 9 */ 2, 2, 2, 4, 2, 2, 2, 4, /* rate 13 1 */
        /*10 */ 2, 4, 2, 4, 2, 4, 2, 4, /* rate 13 2 */
        /*11 */ 2, 4, 4, 4, 2, 4, 4, 4, /* rate 13 3 */

        /*12 */ 4, 4, 4, 4, 4, 4, 4, 4, /* rate 14 0 (increment by 4) */
        /*13 */ 4, 4, 4, 8, 4, 4, 4, 8, /* rate 14 1 */
        /*14 */ 4, 8, 4, 8, 4, 8, 4, 8, /* rate 14 2 */
        /*15 */ 4, 8, 8, 8, 4, 8, 8, 8, /* rate 14 3 */

        /*16 */ 8, 8, 8, 8, 8, 8, 8, 8,         /* rates 15 0, 15 1, 15 2, 15 3 (increment by 8) */
        /*17 */ 16, 16, 16, 16, 16, 16, 16, 16, /* rates 15 2, 15 3 for attack */
        /*18 */ 0, 0, 0, 0, 0, 0, 0, 0,         /* infinity rates for attack and decay(s) */
};

#define O(a) (a * RATE_STEPS)

/*note that there is no O(17) in this table - it's directly in the code */
/* Envelope Generator rates (32 + 64 rates + 32 RKS) */
constexpr std::array<uint8_t, 32 + 64 + 32> eg_rate_select2612 = {
        /* 32 infinite time rates (same as Rate 0) */
        O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),
        O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),
        O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),
        O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),

        /* rates 00-11 */
        /*
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	*/
        O(18), O(18), O(0), O(0),
        O(0), O(0), O(2), O(2),// Nemesis's tests

        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),
        O(0), O(1), O(2), O(3),

        /* rate 12 */
        O(4), O(5), O(6), O(7),

        /* rate 13 */
        O(8), O(9), O(10), O(11),

        /* rate 14 */
        O(12), O(13), O(14), O(15),

        /* rate 15 */
        O(16), O(16), O(16), O(16),

        /* 32 dummy rates (same as 15 3) */
        O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16),
        O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16),
        O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16),
        O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16)

};
#undef O

/*rate  0,    1,    2,   3,   4,   5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15*/
/*shift 11,   10,   9,   8,   7,   6,  5,  4,  3,  2, 1,  0,  0,  0,  0,  0 */
/*mask  2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3, 1,  0,  0,  0,  0,  0 */

#define O(a) (a * 1)
/* Envelope Generator counter shifts (32 + 64 rates + 32 RKS) */
constexpr std::array<uint8_t, 32 + 64 + 32> eg_rate_shift = {
        /* 32 infinite time rates */
        /* O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
	O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
	O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
	O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0), */

        /* fixed (should be the same as rate 0, even if it makes no difference since increment value is 0 for these rates) */
        O(11), O(11), O(11), O(11), O(11), O(11), O(11), O(11),
        O(11), O(11), O(11), O(11), O(11), O(11), O(11), O(11),
        O(11), O(11), O(11), O(11), O(11), O(11), O(11), O(11),
        O(11), O(11), O(11), O(11), O(11), O(11), O(11), O(11),

        /* rates 00-11 */
        O(11), O(11), O(11), O(11),
        O(10), O(10), O(10), O(10),
        O(9), O(9), O(9), O(9),
        O(8), O(8), O(8), O(8),
        O(7), O(7), O(7), O(7),
        O(6), O(6), O(6), O(6),
        O(5), O(5), O(5), O(5),
        O(4), O(4), O(4), O(4),
        O(3), O(3), O(3), O(3),
        O(2), O(2), O(2), O(2),
        O(1), O(1), O(1), O(1),
        O(0), O(0), O(0), O(0),

        /* rate 12 */
        O(0), O(0), O(0), O(0),

        /* rate 13 */
        O(0), O(0), O(0), O(0),

        /* rate 14 */
        O(0), O(0), O(0), O(0),

        /* rate 15 */
        O(0), O(0), O(0), O(0),

        /* 32 dummy rates (same as 15 3) */
        O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
        O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
        O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
        O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0)

};
#undef O

constexpr std::array<uint8_t, 4 * 32> dt_tab = {
        /* this is YM2151 and YM2612 phase increment data (in 10.10 fixed point format)*/
        /* FD=0 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* FD=1 */
        0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
        2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
        /* FD=2 */
        1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
        5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16,
        /* FD=3 */
        2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
        8, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22};

/* OPN key frequency number -> key code follow table */
/* fnum higher 4bit -> keycode lower 2bit */
constexpr std::array<uint8_t, 16> opn_fktable = {0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3};

/* 8 LFO speed parameters */
/* each value represents number of samples that one LFO level will last for */
constexpr std::array<uint32_t, 8> lfo_samples_per_step = {108, 77, 71, 67, 62, 44, 8, 5};

/*There are 4 different LFO AM depths available, they are:
  0 dB, 1.4 dB, 5.9 dB, 11.8 dB
  Here is how it is generated (in EG steps):

  11.8 dB = 0, 2, 4, 6, 8, 10,12,14,16...126,126,124,122,120,118,....4,2,0
   5.9 dB = 0, 1, 2, 3, 4, 5, 6, 7, 8....63, 63, 62, 61, 60, 59,.....2,1,0
   1.4 dB = 0, 0, 0, 0, 1, 1, 1, 1, 2,...15, 15, 15, 15, 14, 14,.....0,0,0

  (1.4 dB is losing precision as you can see)

  It's implemented as generator from 0..126 with step 2 then a shift
  right N times, where N is:
    8 for 0 dB
    3 for 1.4 dB
    1 for 5.9 dB
    0 for 11.8 dB
*/
constexpr std::array<uint8_t, 4> lfo_ams_depth_shift = {8, 3, 1, 0};

/*There are 8 different LFO PM depths available, they are:
  0, 3.4, 6.7, 10, 14, 20, 40, 80 (cents)

  Modulation level at each depth depends on F-NUMBER bits: 4,5,6,7,8,9,10
  (bits 8,9,10 = FNUM MSB from OCT/FNUM register)

  Here we store only first quarter (positive one) of full waveform.
  Full table (lfo_pm_table) containing all 128 waveforms is build
  at run (init) time.

  One value in table below represents 4 (four) basic LFO steps
  (1 PM step = 4 AM steps).

  For example:
   at LFO SPEED=0 (which is 108 samples per basic LFO step)
   one value from "lfo_pm_output" table lasts for 432 consecutive
   samples (4*108=432) and one full LFO waveform cycle lasts for 13824
   samples (32*432=13824; 32 because we store only a quarter of whole
            waveform in the table below)
*/
constexpr uint8_t lfo_pm_output[7 * 8][8] = {
        /* 7 bits meaningful (of F-NUMBER), 8 LFO output levels per one depth (out of 32), 8 LFO depths */
        /* FNUM BIT 4: 000 0001xxxx */
        /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 1 */ {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 2 */ {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 3 */ {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 4 */ {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 5 */ {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 6 */ {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 7 */ {0, 0, 0, 0, 1, 1, 1, 1},

        /* FNUM BIT 5: 000 0010xxxx */
        /* DEPTH 0 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 1 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 2 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 3 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 4 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 5 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 6 */
        {0, 0, 0, 0, 1, 1, 1, 1},
        /* DEPTH 7 */
        {0, 0, 1, 1, 2, 2, 2, 3},

        /* FNUM BIT 6: 000 0100xxxx */
        /* DEPTH 0 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 1 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 2 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 3 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 4 */
        {0, 0, 0, 0, 0, 0, 0, 1},
        /* DEPTH 5 */
        {0, 0, 0, 0, 1, 1, 1, 1},
        /* DEPTH 6 */
        {0, 0, 1, 1, 2, 2, 2, 3},
        /* DEPTH 7 */
        {0, 0, 2, 3, 4, 4, 5, 6},

        /* FNUM BIT 7: 000 1000xxxx */
        /* DEPTH 0 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 1 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 2 */
        {0, 0, 0, 0, 0, 0, 1, 1},
        /* DEPTH 3 */
        {0, 0, 0, 0, 1, 1, 1, 1},
        /* DEPTH 4 */
        {0, 0, 0, 1, 1, 1, 1, 2},
        /* DEPTH 5 */
        {0, 0, 1, 1, 2, 2, 2, 3},
        /* DEPTH 6 */
        {0, 0, 2, 3, 4, 4, 5, 6},
        /* DEPTH 7 */
        {0, 0, 4, 6, 8, 8, 0xa, 0xc},

        /* FNUM BIT 8: 001 0000xxxx */
        /* DEPTH 0 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 1 */
        {0, 0, 0, 0, 1, 1, 1, 1},
        /* DEPTH 2 */
        {0, 0, 0, 1, 1, 1, 2, 2},
        /* DEPTH 3 */
        {0, 0, 1, 1, 2, 2, 3, 3},
        /* DEPTH 4 */
        {0, 0, 1, 2, 2, 2, 3, 4},
        /* DEPTH 5 */
        {0, 0, 2, 3, 4, 4, 5, 6},
        /* DEPTH 6 */
        {0, 0, 4, 6, 8, 8, 0xa, 0xc},
        /* DEPTH 7 */
        {0, 0, 8, 0xc, 0x10, 0x10, 0x14, 0x18},

        /* FNUM BIT 9: 010 0000xxxx */
        /* DEPTH 0 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 1 */
        {0, 0, 0, 0, 2, 2, 2, 2},
        /* DEPTH 2 */
        {0, 0, 0, 2, 2, 2, 4, 4},
        /* DEPTH 3 */
        {0, 0, 2, 2, 4, 4, 6, 6},
        /* DEPTH 4 */
        {0, 0, 2, 4, 4, 4, 6, 8},
        /* DEPTH 5 */
        {0, 0, 4, 6, 8, 8, 0xa, 0xc},
        /* DEPTH 6 */
        {0, 0, 8, 0xc, 0x10, 0x10, 0x14, 0x18},
        /* DEPTH 7 */
        {0, 0, 0x10, 0x18, 0x20, 0x20, 0x28, 0x30},

        /* FNUM BIT10: 100 0000xxxx */
        /* DEPTH 0 */
        {0, 0, 0, 0, 0, 0, 0, 0},
        /* DEPTH 1 */
        {0, 0, 0, 0, 4, 4, 4, 4},
        /* DEPTH 2 */
        {0, 0, 0, 4, 4, 4, 8, 8},
        /* DEPTH 3 */
        {0, 0, 4, 4, 8, 8, 0xc, 0xc},
        /* DEPTH 4 */
        {0, 0, 4, 8, 8, 8, 0xc, 0x10},
        /* DEPTH 5 */
        {0, 0, 8, 0xc, 0x10, 0x10, 0x14, 0x18},
        /* DEPTH 6 */
        {0, 0, 0x10, 0x18, 0x20, 0x20, 0x28, 0x30},
        /* DEPTH 7 */
        {0, 0, 0x20, 0x30, 0x40, 0x40, 0x50, 0x60},

};

/* all 128 LFO PM waveforms */
consteval std::array<int32_t, 128 * 8 * 32> generate_lfo_pm_table() {
	/* build LFO PM modulation table */
	std::array<int32_t, 128 * 8 * 32> lfo_pm_table{};
	for(signed int i = 0; i < 8; i++) {             /* 8 PM depths */
		for(uint8_t fnum = 0; fnum < 128; fnum++) { /* 7 bits meaningful of F-NUMBER */
			for(uint8_t step = 0; step < 8; step++) {
				uint8_t value = 0;
				for(uint32_t bit_tmp = 0; bit_tmp < 7; bit_tmp++) { /* 7 bits */
					if(fnum & (1 << bit_tmp)) {                     /* only if bit "bit_tmp" is set */
						const uint32_t offset_depth = i;
						const uint32_t offset_fnum_bit = bit_tmp * 8;
						value += lfo_pm_output[offset_fnum_bit + offset_depth][step];
					}
				}
				/* 32 steps for LFO PM (sinus) */
				lfo_pm_table[(fnum * 32 * 8) + (i * 32) + step + 0] = value;
				lfo_pm_table[(fnum * 32 * 8) + (i * 32) + (step ^ 7) + 8] = value;
				lfo_pm_table[(fnum * 32 * 8) + (i * 32) + step + 16] = -value;
				lfo_pm_table[(fnum * 32 * 8) + (i * 32) + (step ^ 7) + 24] = -value;
			}
		}
	}
	return lfo_pm_table;
}
constexpr std::array<int32_t, 128 * 8 * 32> lfo_pm_table = generate_lfo_pm_table();
/* 128 combinations of 7 bits meaningful (of F-NUMBER), 8 LFO depths, 32 LFO output levels per one depth */

/* register number to channel number , slot offset */
constexpr int OPN_CHAN(int N) {
	return N & 3;
}

constexpr int OPN_SLOT(int N) {
	return (N >> 2) & 3;
}

/* slot number */
enum Slot {
	SLOT1,
	SLOT2,
	SLOT3,
	SLOT4
};

/***************************************************************************

  2612intf.c

  The YM2612 emulator supports up to 2 chips.
  Each chip has the following connections:
  - Status Read / Control Write A
  - Port Read / Data Write A
  - Control Write B
  - Data Write B

***************************************************************************/

struct ym2612_state {
	YM2612 *chip;
};

static std::array<ym2612_state, MAX_CHIPS> YM2612Data;

/* update request from fm.c */
void ym2612_update_request(void *param) {
	auto *info = reinterpret_cast<ym2612_state *>(param);
	info->chip->update(DUMMYBUF, 0);
}

/***********************************************************/
/*    YM2612                                               */
/***********************************************************/

void ym2612_stream_update(uint8_t ChipID, stream_sample_t **outputs, size_t samples) {
	ym2612_state *info = &YM2612Data[ChipID];
	info->chip->update(outputs, samples);
}

int device_start_ym2612(uint8_t ChipID, int clock) {
	if(ChipID >= MAX_CHIPS) {
		return 0;
	}

	ym2612_state *info = &YM2612Data[ChipID];
	auto rate = clock / 144;

	/**** initialize YM2612 ****/
	info->chip = new YM2612(info, clock, rate);
	return rate;
}

void device_stop_ym2612(uint8_t ChipID) {
	ym2612_state *info = &YM2612Data[ChipID];
	delete info->chip;
}

void device_reset_ym2612(uint8_t ChipID) {
	ym2612_state *info = &YM2612Data[ChipID];
	info->chip->reset();
}

void ym2612_w(uint8_t ChipID, offs_t offset, uint8_t data) {
	ym2612_state *info = &YM2612Data[ChipID];
	info->chip->write(offset & 3, data);
}

void ym2612_set_mute_mask(uint8_t ChipID, uint32_t MuteMask) {
	ym2612_state *info = &YM2612Data[ChipID];
	info->chip->set_mutemask(MuteMask);
}

void FM_SLOT::KEYON(uint8_t CsmOn) {
	if(!key && !CsmOn) {
		/* restart Phase Generator */
		phase = 0;

		/* reset SSG-EG inversion flag */
		ssgn = 0;

		if((ar + ksr) < 94 /*32+62*/) {
			state = (volume <= MIN_ATT_INDEX) ? ((sl == MIN_ATT_INDEX) ? EG::Sustain : EG::Decay) : EG::Attack;
		} else {
			/* force attenuation level to 0 */
			volume = MIN_ATT_INDEX;

			/* directly switch to Decay (or Sustain) */
			state = (sl == MIN_ATT_INDEX) ? EG::Sustain : EG::Decay;
		}

		/* recalculate EG output */
		if((ssg & 0x08) && (ssgn ^ (ssg & 0x04))) {
			vol_out = ((uint32_t) (0x200 - volume) & MAX_ATT_INDEX) + tl;
		} else {
			vol_out = (uint32_t) volume + tl;
		}
	}

	key = 1;
}

void FM_SLOT::KEYOFF(uint8_t CsmOn) {
	if(key && !CsmOn) {
		if(state > EG::Release) {
			state = EG::Release; /* phase . Release */

			/* SSG-EG specific update */
			if(ssg & 0x08) {
				/* convert EG attenuation level */
				if(ssgn ^ (ssg & 0x04)) {
					volume = (0x200 - volume);
				}

				/* force EG attenuation level */
				if(volume >= 0x200) {
					volume = MAX_ATT_INDEX;
					state = EG::Off;
				}

				/* recalculate EG output */
				vol_out = (uint32_t) volume + tl;
			}
		}
	}

	key = 0;
}

void FM_SLOT::KEYOFF_CSM() {
	if(!key) {
		if(state > EG::Release) {
			state = EG::Release; /* phase -> Release */

			/* SSG-EG specific update */
			if(ssg & 0x08) {
				/* convert EG attenuation level */
				if(ssgn ^ (ssg & 0x04)) {
					volume = (0x200 - volume);
				}

				/* force EG attenuation level */
				if(volume >= 0x200) {
					volume = MAX_ATT_INDEX;
					state = EG::Off;
				}

				/* recalculate EG output */
				vol_out = (uint32_t) volume + tl;
			}
		}
	}
}

/* OPN Mode Register Write */
void FM_OPN::set_timers(FM_STATE &ST, int v) {
	/* b7 = CSM MODE */
	/* b6 = 3 slot mode */
	/* b5 = reset b */
	/* b4 = reset a */
	/* b3 = timer enable b */
	/* b2 = timer enable a */
	/* b1 = load b */
	/* b0 = load a */

	if((STATE.mode ^ v) & 0xC0) {
		/* phase increment need to be recalculated */
		P_CH[2].SLOTs[SLOT1].Incr = -1;

		/* CSM mode disabled and CSM key ON active*/
		if(((v & 0xC0) != 0x80) && SL3.key_csm) {
			/* CSM Mode Key OFF (verified by Nemesis on real hardware) */
			P_CH[2].SLOTs[SLOT1].KEYOFF_CSM();
			P_CH[2].SLOTs[SLOT2].KEYOFF_CSM();
			P_CH[2].SLOTs[SLOT3].KEYOFF_CSM();
			P_CH[2].SLOTs[SLOT4].KEYOFF_CSM();
			SL3.key_csm = 0;
		}
	}

	/* reset Timer b flag */
	//if( v & 0x20 )
	//	FM_STATUS_RESET(ST,0x02);
	/* reset Timer a flag */
	//if( v & 0x10 )
	//	FM_STATUS_RESET(ST,0x01);
	/* load b */
	if(v & 0x02) {
		if(ST.TBC == 0) {
			ST.TBC = (256 - ST.TB) << 4;
			/* External timer handler */
			//if (ST.timer_handler) (ST.timer_handler)(n,1,ST.TBC * ST.timer_prescaler,ST.clock);
		}
	} else { /* stop timer b */
		if(ST.TBC != 0) {
			ST.TBC = 0;
			//if (ST.timer_handler) (ST.timer_handler)(n,1,0,ST.clock);
		}
	}
	/* load a */
	if(v & 0x01) {
		if(ST.TAC == 0) {
			ST.TAC = (1024 - ST.TA);
			/* External timer handler */
			//if (ST.timer_handler) (ST.timer_handler)(n,0,ST.TAC * ST.timer_prescaler,ST.clock);
		}
	} else { /* stop timer a */
		if(ST.TAC != 0) {
			ST.TAC = 0;
			//if (ST.timer_handler) (ST.timer_handler)(n,0,0,ST.clock);
		}
	}
	ST.mode = v;
}

/* set algorithm connection */
void FM_OPN::setup_connection(FM_CHANNEL &CH, int ch) {
	int32_t *carrier = &out_fm[ch];

	int32_t *&om1 = CH.connect1;
	int32_t *&om2 = CH.connect3;
	int32_t *&oc1 = CH.connect2;

	int32_t *&memc = CH.mem_connect;

	switch(CH.ALGO) {
		case 0:
			/* M1---C1---MEM---M2---C2---OUT */
			om1 = &c1;
			oc1 = &mem;
			om2 = &c2;
			memc = &m2;
			break;
		case 1:
			/* M1------+-MEM---M2---C2---OUT */
			/*      C1-+                     */
			om1 = &mem;
			oc1 = &mem;
			om2 = &c2;
			memc = &m2;
			break;
		case 2:
			/* M1-----------------+-C2---OUT */
			/*      C1---MEM---M2-+          */
			om1 = &c2;
			oc1 = &mem;
			om2 = &c2;
			memc = &m2;
			break;
		case 3:
			/* M1---C1---MEM------+-C2---OUT */
			/*                 M2-+          */
			om1 = &c1;
			oc1 = &mem;
			om2 = &c2;
			memc = &c2;
			break;
		case 4:
			/* M1---C1-+-OUT */
			/* M2---C2-+     */
			/* MEM: not used */
			om1 = &c1;
			oc1 = carrier;
			om2 = &c2;
			memc = &mem; /* store it anywhere where it will not be used */
			break;
		case 5:
			/*    +----C1----+     */
			/* M1-+-MEM---M2-+-OUT */
			/*    +----C2----+     */
			om1 = nullptr; /* special mark */
			oc1 = carrier;
			om2 = carrier;
			memc = &m2;
			break;
		case 6:
			/* M1---C1-+     */
			/*      M2-+-OUT */
			/*      C2-+     */
			/* MEM: not used */
			om1 = &c1;
			oc1 = carrier;
			om2 = carrier;
			memc = &mem; /* store it anywhere where it will not be used */
			break;
		case 7:
			/* M1-+     */
			/* C1-+-OUT */
			/* M2-+     */
			/* C2-+     */
			/* MEM: not used*/
			om1 = carrier;
			oc1 = carrier;
			om2 = carrier;
			memc = &mem; /* store it anywhere where it will not be used */
			break;
	}

	CH.connect4 = carrier;
}

/* set detune & multiple */
void FM_SLOT::set_det_mul(FM_STATE &ST, FM_CHANNEL &CH, int v) {
	mul = static_cast<bool>(v & 0x0f) ? (v & 0x0f) * 2 : 1;
	DT = ST.dt_tab[(v >> 4) & 7];
	CH.SLOTs[SLOT1].Incr = -1;
}

/* set total level */
void FM_SLOT::set_tl(int v) {
	tl = (v & 0x7f) << (ENV_BITS - 7); /* 7bit TL */

	/* recalculate EG output */
	if(static_cast<bool>(ssg & 0x08) && static_cast<bool>(ssgn ^ (ssg & 0x04)) && (state > EG::Release)) {
		vol_out = ((uint32_t) (0x200 - volume) & MAX_ATT_INDEX) + tl;
	} else {
		vol_out = (uint32_t) volume + tl;
	}
}

/* set attack rate & key scale  */
void FM_SLOT::set_ar_ksr(FM_CHANNEL &CH, int v) {
	uint8_t old_KSR = KSR;

	ar = static_cast<bool>(v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;

	KSR = 3 - (v >> 6);
	if(KSR != old_KSR) {
		CH.SLOTs[SLOT1].Incr = -1;
	}

	/* Even if it seems unnecessary, in some odd case, KSR and KC are both modified   */
	/* and could result in kc remaining unchanged.                              */
	/* In such case, AR values would not be recalculated despite ar has changed */
	/* This fixes the introduction music of Batman & Robin    (Eke-Eke)               */
	if((ar + ksr) < 94 /*32+62*/) {
		eg_sh_ar = eg_rate_shift[ar + ksr];
		eg_sel_ar = eg_rate_select2612[ar + ksr];
	} else {
		eg_sh_ar = 0;
		eg_sel_ar = 18 * RATE_STEPS; /* verified by Nemesis on real hardware */
	}
}

/* set decay rate */
void FM_SLOT::set_dr(int v) {
	d1r = static_cast<bool>(v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;

	eg_sh_d1r = eg_rate_shift[d1r + ksr];
	eg_sel_d1r = eg_rate_select2612[d1r + ksr];
}

/* set sustain rate */
void FM_SLOT::set_sr(int v) {
	d2r = static_cast<bool>(v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;

	eg_sh_d2r = eg_rate_shift[d2r + ksr];
	eg_sel_d2r = eg_rate_select2612[d2r + ksr];
}

/* set release rate */
void FM_SLOT::set_sl_rr(int v) {
	sl = sl_table[v >> 4];

	/* check EG state changes */
	if((state == EG::Decay) && (volume >= (int32_t) (sl))) {
		state = EG::Sustain;
	}

	rr = 34 + ((v & 0x0f) << 2);

	eg_sh_rr = eg_rate_shift[rr + ksr];
	eg_sel_rr = eg_rate_select2612[rr + ksr];
}

/* advance LFO to next sample */
void FM_OPN::advance_lfo() {
	if(lfo_timer_overflow) { /* LFO enabled ? */
		/* increment LFO timer */
		lfo_timer += lfo_timer_add;

		/* when LFO is enabled, one level will last for 108, 77, 71, 67, 62, 44, 8 or 5 samples */
		while(lfo_timer >= lfo_timer_overflow) {
			lfo_timer -= lfo_timer_overflow;

			/* There are 128 LFO steps */
			lfo_cnt = (lfo_cnt + 1) & 127;

			// Valley Bell: Replaced old code (non-inverted triangle) with
			// the one from Genesis Plus GX 1.71.
			/* triangle (inverted) */
			/* AM: from 126 to 0 step -2, 0 to 126 step +2 */
			if(lfo_cnt < 64) {
				LFO_AM = (lfo_cnt ^ 63) << 1;
			} else {
				LFO_AM = (lfo_cnt & 63) << 1;
			}

			/* PM works with 4 times slower clock */
			LFO_PM = lfo_cnt >> 2;
		}
	}
}

void FM_OPN::advance_eg_channel(std::span<FM_SLOT, 4> SLOTS) {
	for(auto &SLOT: SLOTS) { /* four operators per channel */// Todo: Check if this must be iterated in reverse or if it can stay as normal iteration
		switch(SLOT.state) {
			case EG::Attack: /* attack phase */
				if(!(eg_cnt & ((1 << SLOT.eg_sh_ar) - 1))) {
					/* update attenuation level */
					SLOT.volume += (~SLOT.volume * (eg_inc[SLOT.eg_sel_ar + ((eg_cnt >> SLOT.eg_sh_ar) & 7)]))
					               >> 4;

					/* check phase transition*/
					if(SLOT.volume <= MIN_ATT_INDEX) {
						SLOT.volume = MIN_ATT_INDEX;
						SLOT.state = (SLOT.sl == MIN_ATT_INDEX) ? EG::Sustain : EG::Decay; /* special case where SL=0 */
					}

					/* recalculate EG output */
					if((SLOT.ssg & 0x08) && (SLOT.ssgn ^ (SLOT.ssg & 0x04))) { /* SSG-EG Output Inversion */
						SLOT.vol_out = ((uint32_t) (0x200 - SLOT.volume) & MAX_ATT_INDEX) + SLOT.tl;
					} else {
						SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
					}
				}
				break;

			case EG::Decay: /* decay phase */
				if(!(eg_cnt & ((1 << SLOT.eg_sh_d1r) - 1))) {
					/* SSG EG type */
					if(SLOT.ssg & 0x08) {
						/* update attenuation level */
						if(SLOT.volume < 0x200) {
							SLOT.volume += 4 * eg_inc[SLOT.eg_sel_d1r + ((eg_cnt >> SLOT.eg_sh_d1r) & 7)];

							/* recalculate EG output */
							if(SLOT.ssgn ^ (SLOT.ssg & 0x04)) { /* SSG-EG Output Inversion */
								SLOT.vol_out = ((uint32_t) (0x200 - SLOT.volume) & MAX_ATT_INDEX) + SLOT.tl;
							} else {
								SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
							}
						}

					} else {
						/* update attenuation level */
						SLOT.volume += eg_inc[SLOT.eg_sel_d1r + ((eg_cnt >> SLOT.eg_sh_d1r) & 7)];

						/* recalculate EG output */
						SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
					}

					/* check phase transition*/
					if(SLOT.volume >= (int32_t) (SLOT.sl)) {
						SLOT.state = EG::Sustain;
					}
				}
				break;

			case EG::Sustain: /* sustain phase */
				if(!(eg_cnt & ((1 << SLOT.eg_sh_d2r) - 1))) {
					/* SSG EG type */
					if(SLOT.ssg & 0x08) {
						/* update attenuation level */
						if(SLOT.volume < 0x200) {
							SLOT.volume += 4 * eg_inc[SLOT.eg_sel_d2r + ((eg_cnt >> SLOT.eg_sh_d2r) & 7)];

							/* recalculate EG output */
							if(SLOT.ssgn ^ (SLOT.ssg & 0x04)) { /* SSG-EG Output Inversion */
								SLOT.vol_out = ((uint32_t) (0x200 - SLOT.volume) & MAX_ATT_INDEX) + SLOT.tl;
							} else {
								SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
							}
						}
					} else {
						/* update attenuation level */
						SLOT.volume += eg_inc[SLOT.eg_sel_d2r + ((eg_cnt >> SLOT.eg_sh_d2r) & 7)];

						/* check phase transition*/
						if(SLOT.volume >= MAX_ATT_INDEX) {
							SLOT.volume = MAX_ATT_INDEX;
						}
						/* do not change SLOT.state (verified on real chip) */

						/* recalculate EG output */
						SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
					}
				}
				break;

			case EG::Release: /* release phase */
				if(!static_cast<bool>(eg_cnt & ((1 << SLOT.eg_sh_rr) - 1))) {
					/* SSG EG type */
					if(SLOT.ssg & 0x08) {
						/* update attenuation level */
						if(SLOT.volume < 0x200) {
							SLOT.volume += 4 * eg_inc[SLOT.eg_sel_rr + ((eg_cnt >> SLOT.eg_sh_rr) & 7)];
						}
						/* check phase transition */
						if(SLOT.volume >= 0x200) {
							SLOT.volume = MAX_ATT_INDEX;
							SLOT.state = EG::Off;
						}
					} else {
						/* update attenuation level */
						SLOT.volume += eg_inc[SLOT.eg_sel_rr + ((eg_cnt >> SLOT.eg_sh_rr) & 7)];

						/* check phase transition*/
						if(SLOT.volume >= MAX_ATT_INDEX) {
							SLOT.volume = MAX_ATT_INDEX;
							SLOT.state = EG::Off;
						}
					}

					/* recalculate EG output */
					SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
				}
				break;
		}
	}
}

/* SSG-EG update process */
/* The behavior is based upon Nemesis tests on real hardware */
/* This is actually executed before each sample */
void FM_CHANNEL::update_ssg_eg_channel() {                   // NOLINT(readability-function-cognitive-complexity)
	for(auto &SLOT: SLOTs) { /* four operators per channel */// Todo: check if iteration order matters
		/* detect SSG-EG transition */
		/* this is not required during release phase as the attenuation has been forced to MAX and output invert flag is not used */
		/* if an Attack Phase is programmed, inversion can occur on each sample */
		if(static_cast<bool>(SLOT.ssg & 0x08) && (SLOT.volume >= 0x200) && (SLOT.state > EG::Release)) {
			if(static_cast<bool>(SLOT.ssg & 0x01)) /* bit 0 = hold SSG-EG */
			{
				/* set inversion flag */
				if(static_cast<bool>(SLOT.ssg & 0x02)) {
					SLOT.ssgn = 4;
				}

				/* force attenuation level during decay phases */
				if((SLOT.state != EG::Attack) && !static_cast<bool>(SLOT.ssgn ^ (SLOT.ssg & 0x04))) {
					SLOT.volume = MAX_ATT_INDEX;
				}
			} else { /* loop SSG-EG */
				/* toggle output inversion flag or reset Phase Generator */
				if(static_cast<bool>(SLOT.ssg & 0x02)) {
					SLOT.ssgn ^= 4;
				} else {
					SLOT.phase = 0;
				}

				/* same as Key ON */
				if(SLOT.state != EG::Attack) {
					if((SLOT.ar + SLOT.ksr) < (32 + 62)) {
						SLOT.state = (SLOT.volume <= MIN_ATT_INDEX)
						                     ? ((SLOT.sl == MIN_ATT_INDEX) ? EG::Sustain : EG::Decay)
						                     : EG::Attack;
					} else {
						/* Attack Rate is maximal: directly switch to Decay or Sustain */
						SLOT.volume = MIN_ATT_INDEX;
						SLOT.state = (SLOT.sl == MIN_ATT_INDEX) ? EG::Sustain : EG::Decay;
					}
				}
			}

			/* recalculate EG output */
			if(static_cast<bool>(SLOT.ssgn ^ (SLOT.ssg & 0x04))) {
				SLOT.vol_out = ((uint32_t) (0x200 - SLOT.volume) & MAX_ATT_INDEX) + SLOT.tl;
			} else {
				SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
			}
		}
	}
}

void FM_OPN::update_phase_lfo_slot(FM_SLOT &SLOT, int32_t pms, uint32_t block_fnum) {
	uint32_t fnum_lfo = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
	int32_t lfo_fn_table_index_offset = lfo_pm_table[fnum_lfo + pms + LFO_PM];

	block_fnum = block_fnum * 2 + lfo_fn_table_index_offset;

	if(static_cast<bool>(lfo_fn_table_index_offset)) { /* LFO phase modulation active */
		uint8_t blk = (block_fnum & 0x7000) >> 12;
		uint32_t fn = block_fnum & 0xfff;

		/* recalculate keyscale code */
		/*int kc = (blk<<2) | opn_fktable[fn >> 7];*/
		/* This really stupid bug caused a read outside of the
		   array [size 0x10] and returned invalid values.
		   This caused an annoying vibrato for some notes.
		   (Note: seems to be a copy-and-paste from OPNWriteReg -> case 0xA0)
		    Why are MAME cores always SOO buggy ?! */
		/* Oh, and before I forget: it's correct in fm.c */
		uint32_t kc = (blk << 2) | opn_fktable[fn >> 8];
		/* Thanks to Blargg - his patch that helped me to find this bug */

		/* recalculate (frequency) phase increment counter */
		int32_t fc = static_cast<int32_t>(fn_table[fn] >> (7 - blk)) + SLOT.DT[kc];

		/* (frequency) phase overflow (credits to Nemesis) */
		if(fc < 0) { fc += static_cast<int32_t>(fn_max); }

		/* update phase */
		SLOT.phase += (fc * SLOT.mul) >> 1;
	} else { /* LFO phase modulation  = zero */
		SLOT.phase += SLOT.Incr;
	}
}

void FM_OPN::update_phase_lfo_channel(FM_CHANNEL &CH) {
	uint32_t block_fnum = CH.block_fnum;

	uint32_t fnum_lfo = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
	int32_t lfo_fn_table_index_offset = lfo_pm_table[fnum_lfo + CH.pms + LFO_PM];

	block_fnum = block_fnum * 2 + lfo_fn_table_index_offset;

	if(static_cast<bool>(lfo_fn_table_index_offset)) { /* LFO phase modulation active */
		uint8_t blk = (block_fnum & 0x7000) >> 12;
		uint32_t fn = block_fnum & 0xfff;

		/* recalculate keyscale code */
		/*int kc = (blk<<2) | opn_fktable[fn >> 7];*/
		/* the same stupid bug as above */
		uint32_t kc = (blk << 2) | opn_fktable[fn >> 8];

		/* recalculate (frequency) phase increment counter */
		auto fc = static_cast<int32_t>(fn_table[fn] >> (7 - blk));

		/* (frequency) phase overflow (credits to Nemesis) */
		int32_t finc = fc + CH.SLOTs[SLOT1].DT[kc];
		if(finc < 0) finc += static_cast<int32_t>(fn_max);
		CH.SLOTs[SLOT1].phase += (finc * CH.SLOTs[SLOT1].mul) >> 1;

		finc = fc + CH.SLOTs[SLOT2].DT[kc];
		if(finc < 0) finc += static_cast<int32_t>(fn_max);
		CH.SLOTs[SLOT2].phase += (finc * CH.SLOTs[SLOT2].mul) >> 1;

		finc = fc + CH.SLOTs[SLOT3].DT[kc];
		if(finc < 0) finc += static_cast<int32_t>(fn_max);
		CH.SLOTs[SLOT3].phase += (finc * CH.SLOTs[SLOT3].mul) >> 1;

		finc = fc + CH.SLOTs[SLOT4].DT[kc];
		if(finc < 0) finc += static_cast<int32_t>(fn_max);
		CH.SLOTs[SLOT4].phase += (finc * CH.SLOTs[SLOT4].mul) >> 1;
	} else { /* LFO phase modulation  = zero */
		CH.SLOTs[SLOT1].phase += CH.SLOTs[SLOT1].Incr;
		CH.SLOTs[SLOT2].phase += CH.SLOTs[SLOT2].Incr;
		CH.SLOTs[SLOT3].phase += CH.SLOTs[SLOT3].Incr;
		CH.SLOTs[SLOT4].phase += CH.SLOTs[SLOT4].Incr;
	}
}

/* update phase increment and envelope generator */
void FM_OPN::refresh_fc_eg_slot(FM_SLOT &SLOT, int fc, int kc) {
	uint32_t ksr = kc >> SLOT.KSR;

	fc += SLOT.DT[kc];

	/* detects frequency overflow (credits to Nemesis) */
	if(fc < 0) fc += fn_max;

	/* (frequency) phase increment counter */
	SLOT.Incr = (fc * SLOT.mul) >> 1;

	if(SLOT.ksr != ksr) {
		SLOT.ksr = ksr;

		/* calculate envelope generator rates */
		if((SLOT.ar + SLOT.ksr) < 32 + 62) {
			SLOT.eg_sh_ar = eg_rate_shift[SLOT.ar + SLOT.ksr];
			SLOT.eg_sel_ar = eg_rate_select2612[SLOT.ar + SLOT.ksr];
		} else {
			SLOT.eg_sh_ar = 0;
			SLOT.eg_sel_ar = 18 * RATE_STEPS; /* verified by Nemesis on real hardware (Attack phase is blocked) */
		}

		SLOT.eg_sh_d1r = eg_rate_shift[SLOT.d1r + SLOT.ksr];
		SLOT.eg_sh_d2r = eg_rate_shift[SLOT.d2r + SLOT.ksr];
		SLOT.eg_sh_rr = eg_rate_shift[SLOT.rr + SLOT.ksr];

		SLOT.eg_sel_d1r = eg_rate_select2612[SLOT.d1r + SLOT.ksr];
		SLOT.eg_sel_d2r = eg_rate_select2612[SLOT.d2r + SLOT.ksr];
		SLOT.eg_sel_rr = eg_rate_select2612[SLOT.rr + SLOT.ksr];
	}
}

/* update phase increment counters */
void FM_OPN::refresh_fc_eg_chan(FM_CHANNEL &CH) {
	if(CH.SLOTs[SLOT1].Incr == -1) {
		int fc = CH.fc;
		int kc = CH.kcode;
		refresh_fc_eg_slot(CH.SLOTs[SLOT1], fc, kc);
		refresh_fc_eg_slot(CH.SLOTs[SLOT2], fc, kc);
		refresh_fc_eg_slot(CH.SLOTs[SLOT3], fc, kc);
		refresh_fc_eg_slot(CH.SLOTs[SLOT4], fc, kc);
	}
}

constexpr signed int op_calc(uint32_t phase, unsigned int env, signed int pm) {
	uint32_t p;

	p = (env << 3) + sin_tab[(((signed int) ((phase & ~FREQ_MASK) + (pm << 15))) >> FREQ_SH) & SIN_MASK];

	if(p >= TL_TAB_LEN) {
		return 0;
	}
	return tl_tab[p];
}

constexpr signed int op_calc1(uint32_t phase, unsigned int env, signed int pm) {
	uint32_t p;

	p = (env << 3) + sin_tab[(((signed int) ((phase & ~FREQ_MASK) + pm)) >> FREQ_SH) & SIN_MASK];

	if(p >= TL_TAB_LEN) {
		return 0;
	}
	return tl_tab[p];
}

constexpr auto volume_calc(FM_SLOT &SLOT, const uint32_t &AM) {
	return SLOT.vol_out + (AM & SLOT.AMmask);
}

void YM2612::chan_calc(FM_CHANNEL &channel) {
	if(channel.Muted) {
		return;
	}

	OPN.m2 = OPN.c1 = OPN.c2 = OPN.mem = 0;

	*channel.mem_connect = channel.mem_value; /* restore delayed sample (MEM) value to m2 or c2 */

	uint32_t AM = OPN.LFO_AM >> channel.ams;
	unsigned int eg_out = volume_calc(channel.SLOTs[SLOT1], AM);
	int32_t out = channel.op1_out[0] + channel.op1_out[1];
	channel.op1_out[0] = channel.op1_out[1];

	if(!channel.connect1) {
		/* algorithm 5  */
		OPN.mem = OPN.c1 = OPN.c2 = channel.op1_out[0];
	} else {
		/* other algorithms */
		*channel.connect1 += channel.op1_out[0];
	}

	channel.op1_out[1] = 0;
	if(eg_out < ENV_QUIET) /* SLOT 1 */
	{
		if(!channel.FB) {
			out = 0;
		}

		channel.op1_out[1] = op_calc1(channel.SLOTs[SLOT1].phase, eg_out, (out << channel.FB));
	}

	eg_out = volume_calc(channel.SLOTs[SLOT3], AM);
	if(eg_out < ENV_QUIET) { /* SLOT 3 */
		*channel.connect3 += op_calc(channel.SLOTs[SLOT3].phase, eg_out, OPN.m2);
	}

	eg_out = volume_calc(channel.SLOTs[SLOT2], AM);
	if(eg_out < ENV_QUIET) { /* SLOT 2 */
		*channel.connect2 += op_calc(channel.SLOTs[SLOT2].phase, eg_out, OPN.c1);
	}

	eg_out = volume_calc(channel.SLOTs[SLOT4], AM);
	if(eg_out < ENV_QUIET) { /* SLOT 4 */
		*channel.connect4 += op_calc(channel.SLOTs[SLOT4].phase, eg_out, OPN.c2);
	}

	/* store current MEM */
	channel.mem_value = OPN.mem;

	/* update phase counters AFTER output calculations */
	if(channel.pms) {
		/* add support for 3 slot mode */
		if((OPN.STATE.mode & 0xC0) && (&channel == &this->CH[2])) {
			OPN.update_phase_lfo_slot(channel.SLOTs[SLOT1], channel.pms, OPN.SL3.block_fnum[1]);
			OPN.update_phase_lfo_slot(channel.SLOTs[SLOT2], channel.pms, OPN.SL3.block_fnum[2]);
			OPN.update_phase_lfo_slot(channel.SLOTs[SLOT3], channel.pms, OPN.SL3.block_fnum[0]);
			OPN.update_phase_lfo_slot(channel.SLOTs[SLOT4], channel.pms, channel.block_fnum);
		} else {
			OPN.update_phase_lfo_channel(channel);
		}
	} else /* no LFO phase modulation */
	{
		channel.SLOTs[SLOT1].phase += channel.SLOTs[SLOT1].Incr;
		channel.SLOTs[SLOT2].phase += channel.SLOTs[SLOT2].Incr;
		channel.SLOTs[SLOT3].phase += channel.SLOTs[SLOT3].Incr;
		channel.SLOTs[SLOT4].phase += channel.SLOTs[SLOT4].Incr;
	}
}

static void FMCloseTable() {
#ifdef SAVE_SAMPLE
	fclose(sample[0]);
#endif
}

/* write a OPN mode register 0x20-0x2f */
void FM_OPN::WriteMode(int reg, int value) {
	switch(reg) {
		case 0x21: /* Test */
			break;
		case 0x22:        /* LFO FREQ (YM2608/YM2610/YM2610B/YM2612) */
			if(value & 8) /* LFO enabled ? */
			{
				lfo_timer_overflow = lfo_samples_per_step[value & 7] << LFO_SH;
			} else {
				// Valley Bell: Ported from Genesis Plus GX 1.71
				// hold LFO waveform in reset state
				lfo_timer_overflow = 0;
				lfo_timer = 0;
				lfo_cnt = 0;

				LFO_PM = 0;
				LFO_AM = 126;
				//lfo_timer_overflow = 0;
			}
			break;
		case 0x24: /* timer A High 8*/
			STATE.TA = (STATE.TA & 0x03) | (((int) value) << 2);
			break;
		case 0x25: /* timer A Low 2*/
			STATE.TA = (STATE.TA & 0x3fc) | (value & 3);
			break;
		case 0x26: /* timer B */
			STATE.TB = value;
			break;
		case 0x27: /* mode, timer control */
			set_timers(STATE, value);
			break;
		case 0x28: { /* key on / off */
			uint8_t c = value & 0x03;
			if(c == 3) break;
			//if( (v&0x04) && (type & TYPE_6CH) ) c+=3;
			if(value & 0x04) c += 3;
			FM_CHANNEL &CH = P_CH[c];
			// CH = &CH[c];
			if(value & 0x10) CH.SLOTs[SLOT1].KEYON(SL3.key_csm);//FM_KEYON(OPN, CH, SLOT1);
			else
				CH.SLOTs[SLOT1].KEYOFF(SL3.key_csm);//FM_KEYOFF(OPN, CH, SLOT1);
			if(value & 0x20) CH.SLOTs[SLOT2].KEYON(SL3.key_csm);
			else
				CH.SLOTs[SLOT2].KEYOFF(SL3.key_csm);
			if(value & 0x40) CH.SLOTs[SLOT3].KEYON(SL3.key_csm);
			else
				CH.SLOTs[SLOT3].KEYOFF(SL3.key_csm);
			if(value & 0x80) CH.SLOTs[SLOT4].KEYON(SL3.key_csm);
			else
				CH.SLOTs[SLOT3].KEYOFF(SL3.key_csm);
			break;
		}
		default: break;
	}
}

/* write a OPN register (0x30-0xff) */
void FM_OPN::WriteReg(int reg, int value) {
	uint8_t c = OPN_CHAN(reg);

	if(c == 3) return; /* 0xX3,0xX7,0xXB,0xXF */

	if(reg >= 0x100) c += 3;

	FM_CHANNEL &CH = P_CH[c];

	FM_SLOT &SLOT = CH.SLOTs[OPN_SLOT(reg)];

	switch(reg & 0xf0) {
		case 0x30: /* DET , MUL */
			SLOT.set_det_mul(STATE, CH, value);
			break;

		case 0x40: /* TL */
			SLOT.set_tl(value);
			break;

		case 0x50: /* KS, AR */
			SLOT.set_ar_ksr(CH, value);
			break;

		case 0x60: /* bit7 = AM ENABLE, DR */
			SLOT.set_dr(value);

			SLOT.AMmask = static_cast<bool>(value & 0x80) ? ~0 : 0;
			break;

		case 0x70: /*     SR */
			SLOT.set_sr(value);
			break;

		case 0x80: /* SL, RR */
			SLOT.set_sl_rr(value);
			break;

		case 0x90: /* SSG-EG */
			SLOT.ssg = value & 0x0f;

			/* recalculate EG output */
			if(SLOT.state > EG::Release) {
				if(static_cast<bool>(SLOT.ssg & 0x08) && static_cast<bool>(SLOT.ssgn ^ (SLOT.ssg & 0x04))) {
					SLOT.vol_out = ((uint32_t) (0x200 - SLOT.volume) & MAX_ATT_INDEX) + SLOT.tl;
				} else {
					SLOT.vol_out = (uint32_t) SLOT.volume + SLOT.tl;
				}
			}

			/* SSG-EG envelope shapes :

			E AtAlH
			1 0 0 0  \\\\

			1 0 0 1  \___

			1 0 1 0  \/\/
					  ___
			1 0 1 1  \

			1 1 0 0  ////
					  ___
			1 1 0 1  /

			1 1 1 0  /\/\

			1 1 1 1  /___


			E = SSG-EG enable


			The shapes are generated using Attack, Decay and Sustain phases.

			Each single character in the diagrams above represents this whole
			sequence:

			- when KEY-ON = 1, normal Attack phase is generated (*without* any
			  difference when compared to normal mode),

			- later, when envelope level reaches minimum level (max volume),
			  the EG switches to Decay phase (which works with bigger steps
			  when compared to normal mode - see below),

			- later when envelope level passes the SL level,
			  the EG swithes to Sustain phase (which works with bigger steps
			  when compared to normal mode - see below),

			- finally when envelope level reaches maximum level (min volume),
			  the EG switches to Attack phase again (depends on actual waveform).

			Important is that when switch to Attack phase occurs, the phase counter
			of that operator will be zeroed-out (as in normal KEY-ON) but not always.
			(I havent found the rule for that - perhaps only when the output level is low)

			The difference (when compared to normal Envelope Generator mode) is
			that the resolution in Decay and Sustain phases is 4 times lower;
			this results in only 256 steps instead of normal 1024.
			In other words:
			when SSG-EG is disabled, the step inside of the EG is one,
			when SSG-EG is enabled, the step is four (in Decay and Sustain phases).

			Times between the level changes are the same in both modes.


			Important:
			Decay 1 Level (so called SL) is compared to actual SSG-EG output, so
			it is the same in both SSG and no-SSG modes, with this exception:

			when the SSG-EG is enabled and is generating raising levels
			(when the EG output is inverted) the SL will be found at wrong level !!!
			For example, when SL=02:
				0 -6 = -6dB in non-inverted EG output
				96-6 = -90dB in inverted EG output
			Which means that EG compares its level to SL as usual, and that the
			output is simply inverted afterall.


			The Yamaha's manuals say that AR should be set to 0x1f (max speed).
			That is not necessary, but then EG will be generating Attack phase.

			*/

			break;

		case 0xa0:
			switch(OPN_SLOT(reg)) {
				case 0: /* 0xa0-0xa2 : FNUM1 */
				{
					uint32_t fn = ((static_cast<uint32_t>((STATE.fn_h) & 7)) << 8) + value;
					uint8_t blk = STATE.fn_h >> 3;
					/* keyscale code */
					CH.kcode = (blk << 2) | opn_fktable[fn >> 7];
					/* phase increment counter */
					CH.fc = fn_table[fn * 2] >> (7 - blk);

					/* store fnum in clear form for LFO PM calculations */
					CH.block_fnum = (blk << 11) | fn;

					CH.SLOTs[SLOT1].Incr = -1;
				} break;
				case 1: /* 0xa4-0xa6 : FNUM2,BLK */
					STATE.fn_h = value & 0x3f;
					break;
				case 2: /* 0xa8-0xaa : 3CH FNUM1 */
					if(reg < 0x100) {
						uint32_t fn = (((uint32_t) (SL3.fn_h & 7)) << 8) + value;
						uint8_t blk = SL3.fn_h >> 3;
						/* keyscale code */
						SL3.kcode[c] = (blk << 2) | opn_fktable[fn >> 7];
						/* phase increment counter */
						SL3.fc[c] = fn_table[fn * 2] >> (7 - blk);
						SL3.block_fnum[c] = (blk << 11) | fn;
						(P_CH)[2].SLOTs[SLOT1].Incr = -1;
					}
					break;
				case 3: /* 0xac-0xae : 3CH FNUM2,BLK */
					if(reg < 0x100) {
						SL3.fn_h = value & 0x3f;
					}
					break;
			}
			break;

		case 0xb0:
			switch(OPN_SLOT(reg)) {
				case 0: /* 0xb0-0xb2 : FB,ALGO */
				{
					int feedback = (value >> 3) & 7;
					CH.ALGO = value & 7;
					CH.FB = feedback ? feedback + 6 : 0;
					setup_connection(CH, c);
				} break;
				case 1:                        /* 0xb4-0xb6 : L , R , AMS , PMS (YM2612/YM2610B/YM2610/YM2608) */
					CH.pms = (value & 7) * 32; /* CH.pms = PM depth * 32 (index in lfo_pm_table) */

					/* b4-5 AMS */
					CH.ams = lfo_ams_depth_shift[(value >> 4) & 0x03];

					/* PAN :  b7 = L, b6 = R */
					pan[c * 2] = (value & 0x80) ? ~0 : 0;
					pan[c * 2 + 1] = (value & 0x40) ? ~0 : 0;
					break;
			}
			break;
	}
}

/* initialize time tables */
void FM_OPN::init_timetables(const double &freqbase) {
	/* DeTune table */
	for(int d = 0; d <= 3; d++) {
		for(int i = 0; i <= 31; i++) {
			double rate = static_cast<double>(dt_tab[d * 32 + i])
			              * freqbase
			              * (1 << (FREQ_SH - 10)); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
			STATE.dt_tab[d][i] = static_cast<int32_t>(rate);
			STATE.dt_tab[d + 4][i] = -STATE.dt_tab[d][i];
		}
	}

	/* there are 2048 FNUMs that can be generated using FNUM/BLK registers
    but LFO works with one more bit of a precision so we really need 4096 elements */
	/* calculate fnumber -> increment counter table */
	for(int i = 0; i < 4096; i++) {
		/* freq table for octave 7 */
		/* OPN phase increment counter = 20bit */
		/* the correct formula is : F-Number = (144 * fnote * 2^20 / M) / 2^(B-1) */
		/* where sample clock is  M/144 */
		/* this means the increment value for one clock sample is FNUM * 2^(B-1) = FNUM * 64 for octave 7 */
		/* we also need to handle the ratio between the chip frequency and the emulated frequency (can be 1.0)  */
		fn_table[i] = static_cast<uint32_t>(static_cast<double>(i) * 32 * freqbase * (1 << (FREQ_SH - 10)));
		/* -10 because chip works with 10.10 fixed point, while we use 16.16 */
	}

	/* maximal frequency is required for Phase overflow calculation, register size is 17 bits (Nemesis) */
	fn_max = static_cast<uint32_t>(static_cast<double>(0x20000) * freqbase * (1 << (FREQ_SH - 10)));
}

/* prescaler set (and make time tables) */
void FM_OPN::SetPres(int pres, int timer_prescaler) {
	/* frequency base */
	STATE.freqbase = static_cast<bool>(STATE.rate) ? (static_cast<double>(STATE.clock) / STATE.rate) / pres : 0;

	/* EG is updated every 3 samples */
	eg_timer_add = static_cast<uint32_t>((1 << EG_SH) * STATE.freqbase);
	eg_timer_overflow = (3) * (1 << EG_SH);

	/* LFO timer increment (every sample) */
	lfo_timer_add = static_cast<uint32_t>((1 << LFO_SH) * STATE.freqbase);

	/* Timer base time */
	STATE.timer_prescaler = timer_prescaler;

	/* SSG part  prescaler set */
	//if( SSGpres ) (*ST.SSG.set_clock)( ST.param, ST.clock * 2 / SSGpres );

	/* make time tables */
	init_timetables(STATE.freqbase);
}

void YM2612::reset_channels(int num) {
	for(int c = 0; c < num; c++) {
		//memset(&CH[c], 0x00, sizeof(FM_CH));
		CH[c].mem_value = 0;
		CH[c].op1_out[0] = 0;
		CH[c].op1_out[1] = 0;
		CH[c].fc = 0;
		for(auto &s: CH[c].SLOTs) {
			//memset(&CH[c].SLOT[s], 0x00, sizeof(FM_SLOT));
			s.Incr = -1;
			s.key = 0;
			s.phase = 0;
			s.ssg = 0;
			s.ssgn = 0;
			s.state = EG::Off;
			s.volume = MAX_ATT_INDEX;
			s.vol_out = MAX_ATT_INDEX;
		}
	}
}

//#endif /* BUILD_OPN */

/*******************************************************************************/
/*      YM2612 local section                                                   */
/*******************************************************************************/

/* Generate samples for one of the YM2612s */
void YM2612::update(FMSAMPLE **buffer, size_t length) {
	/* set bufer */
	FMSAMPLE *bufL = buffer[0];
	FMSAMPLE *bufR = buffer[1];

	std::span<FM_CHANNEL, 6> cch = CH;

	int32_t dacOut;
	if(MuteDAC) {
		dacOut = 0;
	} else {
		dacOut = this->dacOut;
	}

	/* refresh PG and EG */
	FM_OPN &opn = this->OPN;
	opn.refresh_fc_eg_chan(cch[0]);
	opn.refresh_fc_eg_chan(cch[1]);
	if(opn.STATE.mode & 0xc0) {
		/* 3SLOT MODE */
		if(cch[2].SLOTs[SLOT1].Incr == -1) {
			opn.refresh_fc_eg_slot(cch[2].SLOTs[SLOT1], opn.SL3.fc[1], opn.SL3.kcode[1]);
			opn.refresh_fc_eg_slot(cch[2].SLOTs[SLOT2], opn.SL3.fc[2], opn.SL3.kcode[2]);
			opn.refresh_fc_eg_slot(cch[2].SLOTs[SLOT3], opn.SL3.fc[0], opn.SL3.kcode[0]);
			opn.refresh_fc_eg_slot(cch[2].SLOTs[SLOT4], cch[2].fc, cch[2].kcode);
		}
	} else {
		opn.refresh_fc_eg_chan(cch[2]);
	}
	opn.refresh_fc_eg_chan(cch[3]);
	opn.refresh_fc_eg_chan(cch[4]);
	opn.refresh_fc_eg_chan(cch[5]);
	if(length == 0) {
		for(auto &channel: cch) {
			channel.update_ssg_eg_channel();
		}
		/*
		cch[0].update_ssg_eg_channel();
		cch[1].update_ssg_eg_channel();
		cch[2].update_ssg_eg_channel();
		cch[3].update_ssg_eg_channel();
		cch[4].update_ssg_eg_channel();
		cch[5].update_ssg_eg_channel();
		 */
	}

	/* buffering */
	auto &out_fm = opn.out_fm;
	for(decltype(length) i = 0; i < length; i++) {
		/* clear outputs */
		out_fm.fill(0);
		/*
		out_fm[0] = 0;
		out_fm[1] = 0;
		out_fm[2] = 0;
		out_fm[3] = 0;
		out_fm[4] = 0;
		out_fm[5] = 0;
		 */

		/* update SSG-EG output */
		for(auto &channel: cch) {
			channel.update_ssg_eg_channel();
		}
		/*
		cch[0].update_ssg_eg_channel();
		cch[1].update_ssg_eg_channel();
		cch[2].update_ssg_eg_channel();
		cch[3].update_ssg_eg_channel();
		cch[4].update_ssg_eg_channel();
		cch[5].update_ssg_eg_channel();
		 */

		/* calculate FM */
		chan_calc(cch[0]);
		chan_calc(cch[1]);
		chan_calc(cch[2]);
		chan_calc(cch[3]);
		chan_calc(cch[4]);
		if(dacEnable != 0) {
			*cch[5].connect4 += dacOut;
		} else {
			chan_calc(cch[5]);
		}

		/* advance LFO */
		opn.advance_lfo();

		/* advance envelope generator */
		opn.eg_timer += opn.eg_timer_add;
		while(opn.eg_timer >= opn.eg_timer_overflow) {
			opn.eg_timer -= opn.eg_timer_overflow;
			opn.eg_cnt++;

			for(auto &channel: cch) {
				opn.advance_eg_channel(channel.SLOTs);
			}

			/*
			opn.advance_eg_channel(cch[0].SLOTs);
			opn.advance_eg_channel(cch[1].SLOTs);
			opn.advance_eg_channel(cch[2].SLOTs);
			opn.advance_eg_channel(cch[3].SLOTs);
			opn.advance_eg_channel(cch[4].SLOTs);
			opn.advance_eg_channel(cch[5].SLOTs);
			 */
		}

		for(auto &item: out_fm) {
			item = std::clamp(item, -8192, 8192);
		}

		/*
		if(out_fm[0] > 8192) {
			out_fm[0] = 8192;
		} else if(out_fm[0] < -8192)
			out_fm[0] = -8192;
		if(out_fm[1] > 8192) {
			out_fm[1] = 8192;
		} else if(out_fm[1] < -8192)
			out_fm[1] = -8192;
		if(out_fm[2] > 8192) {
			out_fm[2] = 8192;
		} else if(out_fm[2] < -8192)
			out_fm[2] = -8192;
		if(out_fm[3] > 8192) {
			out_fm[3] = 8192;
		} else if(out_fm[3] < -8192)
			out_fm[3] = -8192;
		if(out_fm[4] > 8192) {
			out_fm[4] = 8192;
		} else if(out_fm[4] < -8192)
			out_fm[4] = -8192;
		if(out_fm[5] > 8192) {
			out_fm[5] = 8192;
		} else if(out_fm[5] < -8192)
			out_fm[5] = -8192;
		 */

		FMSAMPLE lt = 0;
		FMSAMPLE rt = 0;
		/* 6-channels mixing  */
		for(auto fm = 0, pan = 0; fm < 6; fm++) {
			lt += FMSAMPLE((out_fm[fm] >> 0) & opn.pan[pan++]);
			rt += FMSAMPLE((out_fm[fm] >> 0) & opn.pan[pan++]);
		}
		/*
		lt = ((out_fm[0] >> 0) & opn.pan[0]);
		rt = ((out_fm[0] >> 0) & opn.pan[1]);
		lt += ((out_fm[1] >> 0) & opn.pan[2]);
		rt += ((out_fm[1] >> 0) & opn.pan[3]);
		lt += ((out_fm[2] >> 0) & opn.pan[4]);
		rt += ((out_fm[2] >> 0) & opn.pan[5]);
		lt += ((out_fm[3] >> 0) & opn.pan[6]);
		rt += ((out_fm[3] >> 0) & opn.pan[7]);
		lt += ((out_fm[4] >> 0) & opn.pan[8]);
		rt += ((out_fm[4] >> 0) & opn.pan[9]);
		lt += ((out_fm[5] >> 0) & opn.pan[10]);
		rt += ((out_fm[5] >> 0) & opn.pan[11]);
		 */

#ifdef SAVE_SAMPLE
		SAVE_ALL_CHANNELS
#endif

		/* buffering */
		bufL[i] = lt;
		bufR[i] = rt;

		/* CSM mode: if CSM Key ON has occured, CSM Key OFF need to be sent       */
		/* only if Timer A does not overflow again (i.e CSM Key ON not set again) */
		opn.SL3.key_csm <<= 1;

		/* CSM Mode Key ON still disabled */
		if(opn.SL3.key_csm & 2) {
			/* CSM Mode Key OFF (verified by Nemesis on real hardware) */
			for(auto &slot: cch[2].SLOTs) {
				slot.KEYOFF_CSM();
			}
			/*
			cch[2].SLOTs[SLOT1].KEYOFF_CSM();
			cch[2].SLOTs[SLOT2].KEYOFF_CSM();
			cch[2].SLOTs[SLOT3].KEYOFF_CSM();
			cch[2].SLOTs[SLOT4].KEYOFF_CSM();
			 */
			opn.SL3.key_csm = 0;
		}
	}
}

/* initialize YM2612 emulator(s) */
YM2612::YM2612(void *param, int baseclock, int rate) : REGS(), CH() {
	OPN.STATE.param = param;
	OPN.P_CH = CH;
	OPN.STATE.clock = baseclock;
	OPN.STATE.rate = rate;
}

/* shut down emulator */
YM2612::~YM2612() {
	FMCloseTable();
}

/* reset one of chip */
void YM2612::reset() {
	FM_OPN &opn = this->OPN;

	opn.SetPres(6 * 24, 6 * 24);

	opn.eg_timer = 0;
	opn.eg_cnt = 0;

	opn.lfo_timer = 0;
	opn.lfo_cnt = 0;
	opn.LFO_AM = 126;
	opn.LFO_PM = 0;

	opn.STATE.TAC = 0;
	opn.STATE.TBC = 0;

	opn.SL3.key_csm = 0;

	opn.STATE.status = 0;
	opn.STATE.mode = 0;

	//memset(REGS, 0x00, sizeof(REGS));
	REGS.fill(0x00);

	opn.WriteMode(0x22, 0x00);

	opn.WriteMode(0x27, 0x30);
	opn.WriteMode(0x26, 0x00);
	opn.WriteMode(0x25, 0x00);
	opn.WriteMode(0x24, 0x00);

	reset_channels(6);

	for(int i = 0xb6; i >= 0xb4; i--) {
		opn.WriteReg(i, 0xc0);
		opn.WriteReg(i | 0x100, 0xc0);
	}
	for(int i = 0xb2; i >= 0x30; i--) {
		opn.WriteReg(i, 0);
		opn.WriteReg(i | 0x100, 0);
	}

	/* DAC mode clear */
	dacEnable = 0;
	dacOut = 0;
}

/* YM2612 write */
/* n = number  */
/* a = address */
/* v = value   */
int YM2612::write(uint8_t address, uint8_t v) {
	//v &= 0xff; /* adjust to 8 bit bus */
	int addr;
	switch(address & 3) {
		case 0: /* address port 0 */
			OPN.STATE.address = v;
			addr_A1 = 0;
			break;

		case 1: /* data port 0    */
			if(addr_A1 != 0) {
				break;
			} /* verified on real YM2608 */

			addr = OPN.STATE.address;
			REGS[addr] = v;
			switch(addr & 0xf0) {
				case 0x20: /* 0x20-0x2f Mode */
					switch(addr) {
						case 0x2a: /* DAC data (YM2612) */
							ym2612_update_request(OPN.STATE.param);
							dacOut = ((int) v - 0x80) << 6; /* level unknown */
							break;
						case 0x2b: /* DAC Sel  (YM2612) */
							/* b7 = dac enable */
							dacEnable = v & 0x80;
							break;
						default: /* OPN section */
							ym2612_update_request(OPN.STATE.param);
							/* write register */
							OPN.WriteMode(addr, v);
					}
					break;
				default: /* 0x30-0xff OPN section */
					ym2612_update_request(OPN.STATE.param);
					/* write register */
					OPN.WriteReg(addr, v);
			}
			break;

		case 2: /* address port 1 */
			OPN.STATE.address = v;
			addr_A1 = 1;
			break;

		case 3: /* data port 1    */
			if(addr_A1 != 1) {
				break;
			} /* verified on real YM2608 */

			addr = OPN.STATE.address;
			REGS[addr | 0x100] = v;
			ym2612_update_request(OPN.STATE.param);
			OPN.WriteReg(addr | 0x100, v);
			break;
	}
	return OPN.STATE.irq;
}

void YM2612::set_mutemask(uint32_t MuteMask) {
	for(uint8_t CurChn = 0; CurChn < 6; CurChn++) {
		CH[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	}
	MuteDAC = static_cast<bool>((MuteMask >> 6) & 0x01);
}
