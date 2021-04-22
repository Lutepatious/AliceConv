#include "stdtype.h"
#include "VGMFile.h"

#pragma pack(1)
struct mako2_tone {
	union {
		struct {
			unsigned __int8 Connect : 3; // CON Connection
			unsigned __int8 FB : 3; // FL Self-FeedBack
			unsigned __int8 RL : 2; // YM2151 Only
		} S;
		unsigned __int8 B;
	} H;
	unsigned __int8 Unk[6]; // FM? or what?
	union {
		struct {
			unsigned __int8 MULTI : 4; // MUL Multiply
			unsigned __int8 DT : 3; // DT1 DeTune
			unsigned __int8 NU0 : 1; // Not used
			unsigned __int8 TL : 7; // TL Total Level
			unsigned __int8 NU1 : 1; // Not used
			unsigned __int8 AR : 5; // AR Attack Rate
			unsigned __int8 NU2 : 1; // Not used
			unsigned __int8 KS : 2; // KS Key Scale
			unsigned __int8 DR : 5; // D1R Decay Rate
			unsigned __int8 NU3 : 2; // Not used
			unsigned __int8 AMON : 1; // AMS-EN AMS On
			unsigned __int8 SR : 5; // D2R Sustain Rate
			unsigned __int8 NU4 : 1; // Not used
			unsigned __int8 DT2 : 2; // DT2
			unsigned __int8 RR : 4; // RR Release Rate
			unsigned __int8 SL : 4; // D1L Sustain Level
			unsigned __int8 Unk0[3]; // Unknown
		} S;
		unsigned __int8 B[9];
	} Op[4];
};

union LR_AMS_PMS_YM2608 {
	struct {
		unsigned __int8 PMS : 3;
		unsigned __int8 NC : 1;
		unsigned __int8 AMS : 2;
		unsigned __int8 LR : 2;
	} S;
	unsigned __int8 B;
};
#pragma pack()

struct CH_params {
	__int16 Detune;
	unsigned __int8 Volume;
	unsigned __int8 Tone;
	unsigned __int8 AMS;
	unsigned __int8 PMS;
	unsigned __int8 Panpot = 3;
	bool NoteOn;
};

class VGMdata {
	const static unsigned VGM_CLOCK = 44100;
	const static unsigned __int8 vgm_command_YM2151 = 0x54;
	const static unsigned __int8 vgm_command_YM2203 = 0x55;
	const static unsigned __int8 vgm_command_YM2608port0 = 0x56;
	const static unsigned __int8 vgm_command_YM2608port1 = 0x57;
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
	struct mako2_tone* T;
	enum class CHIP chip = CHIP::NONE;
	VGM_HEADER h_vgm = { FCC_VGM, 0, 0x171 };
	VGM_HDR_EXTRA eh_vgm = { sizeof(VGM_HDR_EXTRA), 0, sizeof(unsigned __int32) };
	VGMX_CHIP_DATA16 Ex_Vols = { 0,0,0 };
	void enlarge(void);
	void make_wait(size_t wait);
	void make_data(unsigned __int8 command, unsigned __int8 address, unsigned __int8 data);
	void make_data_YM2203(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2203, address, data); };
	void make_data_YM2151(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2151, address, data); };
	void make_data_YM2608port0(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2608port0, address, data); };
	void make_data_YM2608port1(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2608port1, address, data); };
	void convert_YM2203(struct EVENT& eve);
	void convert_YM2608(struct EVENT& eve);
	void convert_YM2151(struct EVENT& eve);
	void Tone_select_YM2151(unsigned __int8 CH);
	void Tone_select_YM2203_FM(unsigned __int8 CH);
	void Tone_select_YM2608_FMport0(unsigned __int8 CH);
	void Tone_select_YM2608_FMport1(unsigned __int8 CH);
	void Note_off_YM2151(unsigned __int8 CH);
	void Note_on_YM2151(unsigned __int8 CH, unsigned __int8 key);
	void Note_on_YM2203_FM(unsigned __int8 CH, unsigned __int8 key);
	void Note_on_YM2203_SSG(unsigned __int8 CH, unsigned __int8 key);
	void Volume_YM2151(unsigned __int8 CH);
	void Volume_YM2203_FM(unsigned __int8 CH);
	void Volume_YM2608_FMport0(unsigned __int8 CH);
	void Volume_YM2608_FMport1(unsigned __int8 CH);
	void Timer_set_YM2203(void);
	void Timer_set_YM2608(void);
	void Timer_set_YM2151(void);
	void finish(void);

public:
	VGMdata(size_t elems, enum class CHIP chip, unsigned ver, struct mako2_tone* t, size_t ntones);
	void print_all_tones(void);
	void make_init(void);
	void convert(class EVENTS& in);
	void out(wchar_t* p);
};