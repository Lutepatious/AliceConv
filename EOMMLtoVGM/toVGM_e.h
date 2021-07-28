#include "stdtype.h"
#include "VGMFile.h"

struct CH_params {
	unsigned __int8 Volume;
	unsigned __int8 Tone;
	unsigned __int8 Key;
	union AC_Tone T;
	bool NoteOn;
};

class VGMdata_e {
	const static unsigned VGM_CLOCK = 44100;
	const static unsigned __int8 vgm_command_YM2151 = 0x54;
	const static unsigned __int8 vgm_command_YM2203 = 0x55;
	const struct AC_FM_PARAMETER_BYTE* preset;
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

	unsigned __int16 FNumber[12];
	unsigned __int16 TPeriod[97];

	struct EVENT* loop_start = NULL;
	struct CH_params* pCHparam = NULL;
	struct CH_params* pCHparam_cur = NULL;
	enum class Machine arch = Machine::NONE;
	unsigned __int8 CH_cur = 16;
	VGM_HEADER h_vgm = { FCC_VGM, 0, 0x171 };
	VGM_HDR_EXTRA eh_vgm = { sizeof(VGM_HDR_EXTRA), 0, sizeof(unsigned __int32) };
	VGMX_CHIP_DATA16 Ex_Vols = { 0,0,0 };
	unsigned __int16 make_VGM_Ex_Vol(double db);
	unsigned __int16 make_VGM_Ex_Vol(unsigned __int8 percent);
	void enlarge(void);
	void make_wait(size_t wait);
	void Timer_set_YM2151(void);
	void Tone_select_YM2151(void);
	void make_data(unsigned __int8 command, unsigned __int8 address, unsigned __int8 data);
	void finish(void);

	void make_data_YM2203(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2203, address, data); };
	void make_data_YM2151(unsigned __int8 address, unsigned __int8 data) { this->make_data(vgm_command_YM2151, address, data); };
	void convert_YM2203(struct EVENT& eve);
	void Tone_select_YM2203_FM(unsigned __int8 CH);
	void Key_set_YM2203_FM(unsigned __int8 CH);
	void Key_set_YM2203_SSG(unsigned __int8 CH);
	void Note_on_YM2203_FM(unsigned __int8 CH);
	void Note_on_YM2203_SSG(unsigned __int8 CH);
	void Note_off_YM2203_FM(unsigned __int8 CH);
	void Note_off_YM2203_SSG(unsigned __int8 CH);
	void Volume_YM2203_FM(unsigned __int8 CH);

	void Timer_set_YM2203(void);
	void Key_set_YM2151(void);
	void Note_on_YM2151(void);
	void Note_off_YM2151(void);
	void Volume_YM2151(void);

public:
	VGMdata_e(size_t elems, enum class Machine M_arch);
	void make_init(void);
	void convert(class EVENTS& in);
	void SetSSGVol(unsigned __int8 vol);
	void out(wchar_t* p);
};
