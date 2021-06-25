#include "stdtype.h"
#include "VGMFile.h"

#pragma pack(1)
struct AC_FM_PARAMETER {
	unsigned __int8 Connect : 3; // CON Connection
	unsigned __int8 FB : 3; // FL Self-FeedBack
	unsigned __int8 RL : 2; // YM2151 Only
	unsigned __int8 OPR_MASK : 4;
	unsigned __int8 : 4;
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
	} Op[4];
};

#define LEN_AC_FM_PARAMETER (sizeof(struct AC_FM_PARAMETER))

struct AC_FM_PARAMETER_BYTE {
	unsigned __int8 FB_CON;       // CON Connection, FL Self-FeedBack
	unsigned __int8 OPMASK;       // Operator Mask
	struct {
		unsigned __int8 DT_MULTI; // MUL Multiply,  DT1 DeTune
		unsigned __int8 TL;       // TL Total Level
		unsigned __int8 KS_AR;    // AR Attack Rate, KS Key Scale
		unsigned __int8 AM_DR;    // D1R Decay Rate, AMS-EN AMS On
		unsigned __int8 DT2_SR;   // D2R Sustain Rate,  DT2
		unsigned __int8 SL_RR;    // RR Release Rate, D1L Sustain Level
	} Op[4];
};

#define LEN_AC_FM_PARAMETER_BYTE (sizeof(struct AC_FM_PARAMETER_BYTE))

union AC_Tone {
	struct AC_FM_PARAMETER S;
	struct AC_FM_PARAMETER_BYTE B;
};

#pragma pack()

struct CH_params {
	unsigned __int8 Volume;
	unsigned __int8 Tone;
	unsigned __int8 Key;
	bool NoteOn;
};

class VGMdata1 {
	const static unsigned VGM_CLOCK = 44100;
	const static unsigned __int8 vgm_command_YM2203 = 0x55;
	const static unsigned __int8 vgm_command_YM2612port0 = 0x52;
	const static unsigned __int8 vgm_command_YM2612port1 = 0x53;
	const struct AC_FM_PARAMETER_BYTE* preset;
	union AC_Tone T;
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
	unsigned version;
	unsigned __int8 SSG_out = 0xBF;
	unsigned __int8 Ex_Vols_count = 0;
	size_t vgm_dlen = 0;
	size_t vgm_extra_len = 0;
	size_t padsize = 11;

#ifndef STATIC_FNUMBER
	unsigned __int16 FNumber[12];
#endif

	struct EVENT* loop_start = NULL;
	struct CH_params* pCHparam = NULL;
	struct CH_params* pCHparam_cur = NULL;
	enum class Machine arch = Machine::NONE;
	unsigned __int8 CH_cur = 16;
	VGM_HEADER h_vgm = { FCC_VGM, 0, 0x171 };
	VGM_HDR_EXTRA eh_vgm = { sizeof(VGM_HDR_EXTRA), 0, sizeof(unsigned __int32) };
	VGMX_CHIP_DATA16 Ex_Vols = { 0,0,0 };
	void enlarge(void);
	void make_wait(size_t wait);
	void make_data(unsigned __int8 command, unsigned __int8 address, unsigned __int8 data);
	void finish(void);

	void make_data_YM2203(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2203, address, data); };
	void convert_YM2203(struct EVENT& eve);
	void Tone_select_YM2203_FM(unsigned __int8 CH);
	void Key_set_YM2203_FM(unsigned __int8 CH);
	void Key_set_YM2203_SSG(unsigned __int8 CH);
	void Note_on_YM2203_FM(unsigned __int8 CH);
	void Note_on_YM2203_SSG(unsigned __int8 CH);
	void Volume_YM2203_FM(unsigned __int8 CH);
	void Timer_set_YM2203(void);

	void make_data_YM2612port0(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2612port0, address, data); };
	void make_data_YM2612port1(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2612port1, address, data); };
	void convert_YM2612(struct EVENT& eve);
	void Tone_select_YM2612_FMport0(unsigned __int8 CH);
	void Tone_select_YM2612_FMport1(unsigned __int8 CH);
	void Key_set_YM2612_FMport0(unsigned __int8 CH);
	void Key_set_YM2612_FMport1(unsigned __int8 CH);
	void Note_on_YM2612_FMport0(unsigned __int8 CH);
	void Note_on_YM2612_FMport1(unsigned __int8 CH);
	void Volume_YM2612_FMport0(unsigned __int8 CH);
	void Volume_YM2612_FMport1(unsigned __int8 CH);
	void Timer_set_YM2612(void);


public:
	VGMdata1(size_t elems, enum class Machine M_arch);
	void make_init(void);
	void convert(class EVENTS& in);
	void out(wchar_t* p);
};