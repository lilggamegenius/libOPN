#pragma once

#include "mamedef.hpp"

#include <cstdint>
#include <array>
#include <span>


void ym2612_stream_update(uint8_t ChipID, stream_sample_t **outputs, size_t samples);
int device_start_ym2612(uint8_t ChipID, int clock);
void device_stop_ym2612(uint8_t ChipID);
void device_reset_ym2612(uint8_t ChipID);

void ym2612_w(uint8_t ChipID, offs_t offset, uint8_t data);
void ym2612_set_mute_mask(uint8_t ChipID, uint32_t MuteMask);


void ym2612_update_request(void *param);

using FMSAMPLE = stream_sample_t;

using FM_TIMERHANDLER = void (*)(void *, int, int, int);

using FM_IRQHANDLER = void (*)(void *, int);

// declare our structs early, so we can use them in definitions
struct FM_SLOT;
struct FM_CHANNEL;
struct FM_STATE;
struct FM_3SLOT;
struct FM_OPN;
struct YM2612;

/* FM_TIMERHANDLER : Stop or Start timer         */
/* int n          = chip number                  */
/* int c          = Channel 0=TimerA,1=TimerB    */
/* int count      = timer count (0=stop)         */
/* double stepTime = step time of one count (sec.)*/

/* FM_IRQHHANDLER : IRQ level changing sense     */
/* int n       = chip number                     */
/* int irq     = IRQ level 0=OFF,1=ON            */

/* struct describing a single operator (SLOT) */
struct FM_SLOT{
	int32_t *DT;        /* detune          :dt_tab[DT] */
	uint8_t KSR;        /* key scale rate  :3-KSR */
	uint32_t ar;            /* attack rate  */
	uint32_t d1r;        /* decay rate   */
	uint32_t d2r;        /* sustain rate */
	uint32_t rr;            /* release rate */
	uint8_t ksr;        /* key scale rate  :kcode>>(3-KSR) */
	uint32_t mul;        /* multiple        :ML_TABLE[ML] */

	/* Phase Generator */
	uint32_t phase;        /* phase counter */
	int32_t Incr;        /* phase step */

	/* Envelope Generator */
	uint8_t state;        /* phase type */
	uint32_t tl;            /* total level: TL << 3 */
	int32_t volume;        /* envelope counter */
	uint32_t sl;            /* sustain level:sl_table[SL] */
	uint32_t vol_out;    /* current output from EG circuit (without AM from LFO) */

	uint8_t eg_sh_ar;    /*  (attack state) */
	uint8_t eg_sel_ar;    /*  (attack state) */
	uint8_t eg_sh_d1r;    /*  (decay state) */
	uint8_t eg_sel_d1r;    /*  (decay state) */
	uint8_t eg_sh_d2r;    /*  (sustain state) */
	uint8_t eg_sel_d2r;    /*  (sustain state) */
	uint8_t eg_sh_rr;    /*  (release state) */
	uint8_t eg_sel_rr;    /*  (release state) */

	uint8_t ssg;        /* SSG-EG waveform */
	uint8_t ssgn;        /* SSG-EG negated output */

	uint8_t key;        /* 0=last key was KEY OFF, 1=KEY ON */

	/* LFO */
	uint32_t AMmask;        /* AM enable flag */

	void KEYON(uint8_t CsmOn = 0);
	void KEYOFF(uint8_t CsmOn = 0);

	void KEYOFF_CSM();

	void set_det_mul(FM_STATE &ST, FM_CHANNEL &CH, int v);  /* set detune & multiple */
	void set_tl(int v);                                     /* set total level */
	void set_ar_ksr(FM_CHANNEL &CH, int v);                 /* set attack rate & key scale  */
	void set_dr(int v);                                     /* set decay rate */
	void set_sr(int v);                                     /* set sustain rate */
	void set_sl_rr(int v);                                  /* set release rate */
};

struct FM_CHANNEL {
	std::array<FM_SLOT, 4> SLOTs;    /* four SLOTs (operators) */

	uint8_t ALGO;        /* algorithm */
	uint8_t FB;            /* feedback shift */
	std::array<int32_t, 2> op1_out;    /* op1 output for feedback */

	int32_t *connect1;    /* SLOT1 output pointer */
	int32_t *connect3;    /* SLOT3 output pointer */
	int32_t *connect2;    /* SLOT2 output pointer */
	int32_t *connect4;    /* SLOT4 output pointer */

	int32_t *mem_connect;/* where to put the delayed sample (MEM) */
	int32_t mem_value;    /* delayed sample (MEM) value */

	int32_t pms;        /* channel PMS */
	uint8_t ams;        /* channel AMS */

	uint32_t fc;            /* fnum,blk:adjusted to sample rate */
	uint8_t kcode;        /* key code:                        */
	uint32_t block_fnum;    /* current blk/fnum value for this slot (can be different betweeen slots of one channel in 3slot mode) */
	uint8_t Muted;

	void update_ssg_eg_channel();
};

struct FM_STATE {
	//running_device *device;
	void *param;                /* this chip parameter  */
	double freqbase;            /* frequency base       */
	int timer_prescaler;    /* timer prescaler      */
	uint8_t irq;                /* interrupt level      */
	uint8_t irqmask;            /* irq mask             */
#if FM_BUSY_FLAG_SUPPORT
	TIME_TYPE	busy_expiry_time;	/* expiry time of the busy status */
#endif
	uint32_t clock;                /* master clock  (Hz)   */
	uint32_t rate;                /* sampling rate (Hz)   */
	uint8_t address;            /* address register     */
	uint8_t status = 0;                /* status flag          */
	uint32_t mode = 0;                /* mode  CSM / 3SLOT    */
	uint8_t fn_h;                /* freq latch           */
	uint8_t prescaler_sel;        /* prescaler selector   */
	int32_t TA;                    /* timer a              */
	int32_t TAC = 0;                /* timer a counter      */
	uint8_t TB;                    /* timer b              */
	int32_t TBC = 0;                /* timer b counter      */
	/* local time tables */
	int32_t dt_tab[8][32];        /* DeTune table         */
	/* Extention Timer and IRQ handler */
};

/***********************************************************/
/* OPN unit                                                */
/***********************************************************/

/* OPN 3slot struct */
struct FM_3SLOT{
	std::array<uint32_t, 3> fc;            /* fnum3,blk3: calculated */
	uint8_t fn_h;            /* freq3 latch */
	std::array<uint8_t, 3> kcode;        /* key code */
	std::array<uint32_t, 3> block_fnum;    /* current fnum value for this slot (can be different betweeen slots of one channel in 3slot mode) */
	uint8_t key_csm = 0;        /* CSM mode Key-ON flag */
};

/* OPN/A/B common state */
struct FM_OPN{
	//uint8_t type;            /* chip type */
	FM_STATE STATE;                /* general state */
	FM_3SLOT SL3;            /* 3 slot mode state */
	std::span<FM_CHANNEL> P_CH;            /* pointer of CH */
	std::array<unsigned int, 6*2> pan;    /* fm channels output masks (0xffffffff = enable) */

	uint32_t eg_cnt = 0;            /* global envelope generator counter */
	uint32_t eg_timer = 0;        /* global envelope generator counter works at frequency = chipclock/144/3 */
	uint32_t eg_timer_add;    /* step of eg_timer */
	uint32_t eg_timer_overflow;/* envelope generator timer overlfows every 3 samples (on real chip) */


	/* there are 2048 FNUMs that can be generated using FNUM/BLK registers
       but LFO works with one more bit of a precision so we really need 4096 elements */
	std::array<uint32_t, 4096> fn_table; /* fnumber->increment counter */
	uint32_t fn_max;    /* maximal phase increment (used for phase overflow) */

	/* LFO */
	uint8_t lfo_cnt = 0;            /* current LFO phase (out of 128) */
	uint32_t lfo_timer = 0;          /* current LFO phase runs at LFO frequency */
	uint32_t lfo_timer_add;      /* step of lfo_timer */
	uint32_t lfo_timer_overflow; /* LFO timer overflows every N samples (depends on LFO frequency) */
	uint32_t LFO_AM = 126;             /* current LFO AM step */
	uint32_t LFO_PM = 0;             /* current LFO PM step */

	int32_t m2, c1, c2;        /* Phase Modulation input for operators 2,3,4 */
	int32_t mem;            /* one sample delay memory */
	std::array<int32_t, 6> out_fm;        /* outputs of working channels */

	void setup_connection(FM_CHANNEL &CH, int ch);

	void WriteMode(int reg, int value);
	void WriteReg(int reg, int value);
	void SetPres(int pres, int timer_prescaler);
	void set_timers(FM_STATE &ST, int v);

	void init_timetables(const double &freqbase);
	void advance_eg_channel(std::span<FM_SLOT, 4> SLOTS);
	void advance_lfo(); /* advance LFO to next sample */
	void update_phase_lfo_slot(FM_SLOT &SLOT, int32_t pms, uint32_t block_fnum);
	void update_phase_lfo_channel(FM_CHANNEL &CH);
	void refresh_fc_eg_slot(FM_SLOT &SLOT, int fc, int kc);
	void refresh_fc_eg_chan(FM_CHANNEL &CH);
};

/* here's the virtual YM2612 */
struct YM2612{
	std::array<uint8_t, 512> REGS;            /* registers            */
	FM_OPN OPN;                /* OPN state            */
	std::array<FM_CHANNEL,6> CH;                /* channel state        */
	uint8_t addr_A1 = 0;            /* address line A1      */

	/* dac output (YM2612) */
	int dacEnable = 0;
	int32_t dacOut = 0;
	bool MuteDAC = false;

	YM2612(void *param, int baseclock, int rate);

	~YM2612();

	void reset();
	void reset_channels(int num);

	void update(FMSAMPLE **buffer, size_t length);

	int write(uint8_t address, uint8_t v);

	void set_mutemask(uint32_t MuteMask);

	void chan_calc(FM_CHANNEL &channel);
};