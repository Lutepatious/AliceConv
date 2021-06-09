#include "stdtype.h"
#include "VGMFile.h"

#pragma pack(1)
struct MAKO2_PARAMETER {
	unsigned __int8 Connect : 3; // CON Connection
	unsigned __int8 FB : 3; // FL Self-FeedBack
	unsigned __int8 RL : 2; // YM2151 Only
	unsigned __int8 OPR_MASK : 4;
	unsigned __int8 : 4;
	unsigned __int8 Unk[5];
	struct {
		unsigned __int8 MULTI : 4; // MUL Multiply
		unsigned __int8 DT : 3; // DT1 DeTune
		unsigned __int8 : 1; // Not used
		unsigned __int8 TL : 7; // TL Total Level
		unsigned __int8 : 1; // Not used
		unsigned __int8 AR : 5; // AR Attack Rate
		unsigned __int8 : 1; // Not used
		unsigned __int8 KS : 2; // KS Key Scale
		unsigned __int8 DR : 5; // D1R Decay Rate
		unsigned __int8 : 2; // Not used
		unsigned __int8 AMON : 1; // AMS-EN AMS On
		unsigned __int8 SR : 5; // D2R Sustain Rate
		unsigned __int8 : 1; // Not used
		unsigned __int8 DT2 : 2; // DT2
		unsigned __int8 RR : 4; // RR Release Rate
		unsigned __int8 SL : 4; // D1L Sustain Level
		unsigned __int8 Unk0[3]; // Unknown
	} Op[4];
};

#pragma pack()

struct CH_params {
	__int16 Detune;
	unsigned __int8 Volume;
	unsigned __int8 Tone;
	unsigned __int8 Key;
	unsigned __int8 AMS;
	unsigned __int8 PMS;
	unsigned __int8 Panpot = 3;
	bool NoteOn;
};

class VGMdata {
	const static unsigned VGM_CLOCK = 44100;
	const static unsigned __int8 vgm_command_YM2203 = 0x55;
	unsigned __int8* vgm_out;
	unsigned __int8* vgm_pos;
	unsigned __int8* vgm_loop_pos = NULL;
	size_t vgm_header_len = sizeof(VGM_HEADER);
	size_t master_clock;
	size_t bytes;
	size_t length;
	size_t tones;
	size_t Tempo = 120;
	size_t Time_Prev = 0;
	size_t Time_Prev_VGM = 0;
	size_t Time_Prev_VGM_abs = 0;
	size_t Time_Loop_VGM_abs = 0;
	unsigned CHs_limit;
	unsigned version;
	unsigned __int8 SSG_out = 0xBF;
	unsigned __int8 Ex_Vols_count = 0;
	size_t vgm_dlen = 0;
	size_t vgm_extra_len = 0;
	size_t padsize = 11;

	struct EVENT* loop_start = NULL;
	struct CH_params* pCHparam = NULL;
	struct CH_params* pCHparam_cur = NULL;
	unsigned __int8 CH_cur = 16;
	struct mako2_tone* T;
	VGM_HEADER h_vgm = { FCC_VGM, 0, 0x171 };
	VGM_HDR_EXTRA eh_vgm = { sizeof(VGM_HDR_EXTRA), 0, sizeof(unsigned __int32) };
	VGMX_CHIP_DATA16 Ex_Vols = { 0,0,0 };
	void enlarge(void);
	void make_wait(size_t wait);
	void make_data(unsigned __int8 command, unsigned __int8 address, unsigned __int8 data);
	void make_data_YM2203(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2203, address, data); };
	void convert_YM2203(struct EVENT& eve);
	void Tone_select_YM2203_FM(unsigned __int8 CH);
	void Key_set_YM2203_FM(unsigned __int8 CH);
	void Key_set_YM2203_SSG(unsigned __int8 CH);
	void Note_on_YM2203_FM(unsigned __int8 CH);
	void Note_on_YM2203_SSG(unsigned __int8 CH);
	void sLFOd_YM2203_FM(unsigned __int8 CH, __int16 Detune);
	void sLFOd_YM2203_SSG(unsigned __int8 CH, __int16 Detune);
	void Volume_YM2203_FM(unsigned __int8 CH);
	void Timer_set_YM2203(void);
	void finish(void);

public:
	VGMdata(size_t elems, enum class CHIP chip, unsigned ver, struct mako2_tone* t, size_t ntones);
	void print_all_tones(void);
	void make_init(void);
	void convert(class EVENTS& in);
	void out(wchar_t* p);
};