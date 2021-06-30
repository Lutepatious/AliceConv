#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"

#include "MAKO2toVGM.h"
#include "event.h"
#include "toVGM.h"
#include "tools.h"

// MAKO.OCM (1,2共通)に埋め込まれているF-number (MAKO1は全オクターブ分入っている)
static const unsigned __int16 FNumber[] = { 0x0269, 0x028E, 0x02B4, 0x02DE, 0x0309, 0x0338, 0x0369, 0x039C, 0x03D3, 0x040E, 0x044B, 0x048D };
static const unsigned __int16 TP[] = {
	0x0EED,0x0E17,0x0D4C,0x0C8D,0x0BD9,0x0B2F,0x0A8E,0x09F6,0x0967,0x08E0,0x0860,0x07E8,
	0x0776,0x070B,0x06A6,0x0646,0x05EC,0x0597,0x0547,0x04FB,0x04B3,0x0470,0x0430,0x03F4,
	0x03BB,0x0385,0x0353,0x0323,0x02F6,0x02CB,0x02A3,0x027D,0x0259,0x0238,0x0218,0x01FA,
	0x01DD,0x01C2,0x01A9,0x0191,0x017B,0x0165,0x0151,0x013E,0x012C,0x011C,0x010C,0x00FD,
	0x00EE,0x00E1,0x00D4,0x00C8,0x00BD,0x00B2,0x00A8,0x009F,0x0096,0x008E,0x0086,0x007E,
	0x0077,0x0070,0x006A,0x0064,0x005E,0x0059,0x0054,0x004F,0x004B,0x0047,0x0043,0x003F,
	0x003B,0x0038,0x0035,0x0032,0x002F,0x002C,0x002A,0x0027,0x0025,0x0023,0x0021,0x001F,
	0x001D,0x001C,0x001A,0x0019,0x0017,0x0016,0x0015,0x0013,0x0012,0x0011,0x0010,0x000F,0x000E };

static const struct MAKO2_FM_PARAMETER_BYTE SSG_emulation = {
	0x03, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
	{{0x02, 0x0F, 0x1F, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00},
	 {0x06, 0x28, 0x1F, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00},
	 {0x04, 0x28, 0x1F, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00},
	 {0x02, 0x00, 0x1F, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00}} };

VGMdata::VGMdata(size_t elems, enum class CHIP chip, unsigned ver, union MAKO2_Tone* t, size_t ntones, bool detune_mode)
{
	this->Tunes = t;
	this->tones = ntones;
	this->version = ver;
	this->dt_mode = detune_mode;
	this->bytes = elems * 10;
	this->length = this->bytes;
	this->vgm_out = (unsigned __int8*)GC_malloc(sizeof(unsigned __int8) * this->length);
	if (this->vgm_out == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}
	this->vgm_pos = this->vgm_out;
	this->chip = chip;

	if (this->chip == CHIP::YM2203) {
		this->CHs_limit = 6;
		h_vgm.lngHzYM2203 = this->master_clock = 3993600;
		//		h_vgm.bytAYType = 0x10;
		//		h_vgm.bytAYFlagYM2203 = 0x1;
		Ex_Vols.Type = 0x86;
		Ex_Vols.Flags = 0;
		Ex_Vols.Data = 0x804C;
		Ex_Vols_count = 1;
		wprintf_s(L"YM2203 mode.\n");
	}
	else if (this->chip == CHIP::YM2608) {
		this->CHs_limit = 9;
		h_vgm.lngHzYM2608 = this->master_clock = 7987200;
		//		h_vgm.bytAYType = 0x10;
		//		h_vgm.bytAYFlagYM2608 = 0x1;
		Ex_Vols.Type = 0x87;
		Ex_Vols.Flags = 0;
		Ex_Vols.Data = 0x8040;
		Ex_Vols_count = 1;
		wprintf_s(L"YM2608 mode.\n");
	}
	else if (this->chip == CHIP::YM2151) {
		this->CHs_limit = 8;
		h_vgm.lngHzYM2151 = this->master_clock = 4000000;
		wprintf_s(L"YM2151 mode.\n");
	}
	else {
		wprintf_s(L"Please select chip by -n, -a, -x options.\n");
		exit(-5);
	}
	pCHparam = new struct CH_params[CHs_limit];
}

void VGMdata::print_all_tones(void)
{
	for (size_t i = 0; i < this->tones; i++) {
		unsigned __int8* param = (unsigned __int8*)(this->Tunes + i);
		wprintf_s(L"Voice %2zu: %02X %02X %02X %02X %02X %02X %02X\n", i, param[0], param[1], param[2], param[3], param[4], param[5], param[6]);
		for (size_t j = 0; j < 4; j++) {
			wprintf_s(L" OP %1zu: %02X %02X %02X %02X %02X %02X %02X %02X %02X\n"
				, j, param[7 + j * 9], param[7 + j * 9 + 1], param[7 + j * 9 + 2], param[7 + j * 9 + 3], param[7 + j * 9 + 4], param[7 + j * 9 + 5], param[7 + j * 9 + 6], param[7 + j * 9 + 7], param[7 + j * 9 + 8]);
		}
	}
}

void VGMdata::check_all_tones_blank(void)
{
	for (size_t i = 0; i < this->tones; i++) {
		unsigned __int8* param = (unsigned __int8*)(this->Tunes + i);
		unsigned __int8 test_result = param[2] | param[3] | param[4] | param[5] | param[6];
		if (test_result) {
			wprintf_s(L"Data exist in tone %zu Header\n", i);
		}
		test_result = 0;
		for (size_t j = 0; j < 4; j++) {
			test_result |= param[7 + j * 9 + 6] | param[7 + j * 9 + 7] | param[7 + j * 9 + 8];
		}
		if (test_result) {
			wprintf_s(L"Data exist in tone %zu Ops\n", i);
		}
	}
}


void VGMdata::enlarge(void)
{
	size_t vgm_length_current = this->vgm_pos - this->vgm_out;
	size_t loop_current = this->vgm_loop_pos - this->vgm_out;
	this->length += this->bytes;
	this->vgm_out = (unsigned __int8*)GC_realloc(this->vgm_out, sizeof(unsigned __int8) * this->length);
	if (this->vgm_out == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}
	this->vgm_pos = this->vgm_out + vgm_length_current;
	this->vgm_loop_pos = this->vgm_out + loop_current;
}

void VGMdata::make_data(unsigned __int8 command, unsigned __int8 address, unsigned __int8 data)
{
	*this->vgm_pos++ = command;
	*this->vgm_pos++ = address;
	*this->vgm_pos++ = data;
}

void VGMdata::make_init(void)
{
	const static unsigned char Init_YM2203[] = {
		0x55, 0x00, 'W', 0x55, 0x00, 'A', 0x55, 0x00, 'O', 0x55, 0x27, 0x30, 0x55, 0x07, 0xBF,
		0x55, 0x90, 0x00, 0x55, 0x91, 0x00, 0x55, 0x92, 0x00, 0x55, 0x24, 0x70, 0x55, 0x25, 0x00 };

	const static unsigned char Init_YM2608[] = {
		0x56, 0x00, 'W', 0x56, 0x00, 'A', 0x56, 0x00, 'O', 0x56, 0x27, 0x30, 0x56, 0x07, 0xBF,
		0x56, 0x90, 0x00, 0x56, 0x91, 0x00, 0x56, 0x92, 0x00, 0x56, 0x24, 0x70, 0x56, 0x25, 0x00, 0x56, 0x29, 0x83 };

	const static unsigned char Init_YM2151[] = {
		0x54, 0x30, 0x00, 0x54, 0x31, 0x00, 0x54, 0x32, 0x00, 0x54, 0x33, 0x00,
		0x54, 0x34, 0x00, 0x54, 0x35, 0x00,	0x54, 0x36, 0x00, 0x54, 0x37, 0x00,
		0x54, 0x38, 0x00, 0x54, 0x39, 0x00, 0x54, 0x3A, 0x00, 0x54, 0x3B, 0x00,
		0x54, 0x3C, 0x00, 0x54, 0x3D, 0x00, 0x54, 0x3E, 0x00, 0x54, 0x3F, 0x00,
		0x54, 0x01, 0x00, 0x54, 0x18, 0x00,	0x54, 0x19, 0x00, 0x54, 0x1B, 0x00,
		0x54, 0x08, 0x00, 0x54, 0x08, 0x01, 0x54, 0x08, 0x02, 0x54, 0x08, 0x03,
		0x54, 0x08, 0x04, 0x54, 0x08, 0x05, 0x54, 0x08, 0x06, 0x54, 0x08, 0x07,
		0x54, 0x60, 0x7F, 0x54, 0x61, 0x7F,	0x54, 0x62, 0x7F, 0x54, 0x63, 0x7F,
		0x54, 0x64, 0x7F, 0x54, 0x65, 0x7F, 0x54, 0x66, 0x7F, 0x54, 0x67, 0x7F,
		0x54, 0x68, 0x7F, 0x54, 0x69, 0x7F, 0x54, 0x6A, 0x7F, 0x54, 0x6B, 0x7F,
		0x54, 0x6C, 0x7F, 0x54, 0x6D, 0x7F,	0x54, 0x6E, 0x7F, 0x54, 0x6F, 0x7F,
		0x54, 0x70, 0x7F, 0x54, 0x71, 0x7F, 0x54, 0x72, 0x7F, 0x54, 0x73, 0x7F,
		0x54, 0x74, 0x7F, 0x54, 0x75, 0x7F, 0x54, 0x76, 0x7F, 0x54, 0x77, 0x7F,
		0x54, 0x78, 0x7F, 0x54, 0x79, 0x7F,	0x54, 0x7A, 0x7F, 0x54, 0x7B, 0x7F,
		0x54, 0x7C, 0x7F, 0x54, 0x7D, 0x7F, 0x54, 0x7E, 0x7F, 0x54, 0x7F, 0x7F,
		0x54, 0x08, 0x00, 0x54, 0x08, 0x01, 0x54, 0x08, 0x02, 0x54, 0x08, 0x03,
		0x54, 0x08, 0x04, 0x54, 0x08, 0x05, 0x54, 0x08, 0x06, 0x54, 0x08, 0x07,
		0x54, 0x14, 0x00, 0x54, 0x10, 0x64, 0x54, 0x11, 0x00,
		0x54, 0x08, 0x00, 0x54, 0x08, 0x01, 0x54, 0x08, 0x02, 0x54, 0x08, 0x03,
		0x54, 0x08, 0x04, 0x54, 0x08, 0x05, 0x54, 0x08, 0x06, 0x54, 0x08, 0x07,
		0x54, 0x60, 0x7F, 0x54, 0x61, 0x7F, 0x54, 0x62, 0x7F, 0x54, 0x63, 0x7F,
		0x54, 0x64, 0x7F, 0x54, 0x65, 0x7F, 0x54, 0x66, 0x7F, 0x54, 0x67, 0x7F,
		0x54, 0x68, 0x7F, 0x54, 0x69, 0x7F, 0x54, 0x6A, 0x7F, 0x54, 0x6B, 0x7F,
		0x54, 0x6C, 0x7F, 0x54, 0x6D, 0x7F,	0x54, 0x6E, 0x7F, 0x54, 0x6F, 0x7F,
		0x54, 0x70, 0x7F, 0x54, 0x71, 0x7F, 0x54, 0x72, 0x7F, 0x54, 0x73, 0x7F,
		0x54, 0x74, 0x7F, 0x54, 0x75, 0x7F, 0x54, 0x76, 0x7F, 0x54, 0x77, 0x7F,
		0x54, 0x78, 0x7F, 0x54, 0x79, 0x7F,	0x54, 0x7A, 0x7F, 0x54, 0x7B, 0x7F,
		0x54, 0x7C, 0x7F, 0x54, 0x7D, 0x7F, 0x54, 0x7E, 0x7F, 0x54, 0x7F, 0x7F,
		0x54, 0x08, 0x00, 0x54, 0x08, 0x01, 0x54, 0x08, 0x02, 0x54, 0x08, 0x03,
		0x54, 0x08, 0x04, 0x54, 0x08, 0x05, 0x54, 0x08, 0x06, 0x54, 0x08, 0x07,
		0x54, 0x14, 0x00 };

	const unsigned char* Init;
	size_t Init_len;
	if (this->chip == CHIP::YM2203) {
		Init = Init_YM2203;
		Init_len = sizeof(Init_YM2203);
	}
	else if (this->chip == CHIP::YM2608) {
		Init = Init_YM2608;
		Init_len = sizeof(Init_YM2608);
	}
	else if (this->chip == CHIP::YM2151) {
		Init = Init_YM2151;
		Init_len = sizeof(Init_YM2151);
	}
	else {
		return;
	}

	memcpy_s(this->vgm_pos, Init_len, Init, Init_len);
	this->vgm_pos += Init_len;

	if (this->chip == CHIP::YM2151) {
		this->CH_cur = 3;
		this->pCHparam_cur = this->pCHparam + this->CH_cur;
		this->SSG_emulation_YM2151();
		this->CH_cur = 4;
		this->pCHparam_cur = this->pCHparam + this->CH_cur;
		this->SSG_emulation_YM2151();
		this->CH_cur = 5;
		this->pCHparam_cur = this->pCHparam + this->CH_cur;
		this->SSG_emulation_YM2151();
	}
}

void VGMdata::make_wait(size_t wait)
{
	while (wait) {
		const size_t wait0 = 0xFFFF;
		const size_t wait1 = 882;
		const size_t wait2 = 735;
		const size_t wait3 = 16;

		if (wait >= wait0) {
			*this->vgm_pos++ = 0x61;
			*((unsigned __int16*)this->vgm_pos) = wait0;
			this->vgm_pos += 2;
			wait -= wait0;
		}
		else if (wait == wait1 * 2 || wait == wait1 + wait2 || (wait <= wait1 + wait3 && wait >= wait1)) {
			*this->vgm_pos++ = 0x63;
			wait -= wait1;
		}
		else if (wait == wait2 * 2 || (wait <= wait2 + wait3 && wait >= wait2)) {
			*this->vgm_pos++ = 0x62;
			wait -= wait2;
		}
		else if (wait <= wait3 * 2 && wait >= wait3) {
			*this->vgm_pos++ = 0x7F;
			wait -= wait3;
		}
		else if (wait <= 15) {
			*this->vgm_pos++ = 0x70 | (wait - 1);
			wait = 0;
		}
		else {
			*this->vgm_pos++ = 0x61;
			*((unsigned __int16*)this->vgm_pos) = wait;
			this->vgm_pos += 2;
			wait = 0;
		}
	}
}

void VGMdata::convert_YM2203(struct EVENT& eve)
{
	this->CH_cur = eve.CH;
	this->pCHparam_cur = this->pCHparam + this->CH_cur;

	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param[0] + 1) >> 1;
		this->Tempo = eve.Param[0];

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2203();
		break;
	case 0xFC: // Detune
		this->pCHparam_cur->Detune = (__int16)((__int8)eve.Param[0]);
		break;
	case 0xF5: // Tone select
		if (this->CH_cur < 3) {
			this->pCHparam_cur->Tone = eve.Param[0];
			this->Tone_select_YM2203_FM(this->CH_cur);
		}
		break;
	case 0x80: // Note Off
		if (this->CH_cur < 3) {
			this->Note_off_YM2203_FM(this->CH_cur);
		}
		else {
			this->Note_off_YM2203_SSG(this->CH_cur - 3);
		}
		break;
	case 0xE1: // Velocity
		this->pCHparam_cur->Volume += eve.Param[0];
		this->pCHparam_cur->Volume &= 0x7F;

		if (this->CH_cur < 3) {
			this->Volume_YM2203_FM(this->CH_cur);
		}
		else {
			this->Volume_YM2203_SSG(this->CH_cur - 3);
		}
		break;
	case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
		this->pCHparam_cur->Volume = eve.Param[0];

		if (this->CH_cur < 3) {
			this->Volume_YM2203_FM(this->CH_cur);
		}
		else {
			this->Volume_YM2203_SSG(this->CH_cur - 3);
		}
		break;
	case 0x90: // Note on
		if (this->CH_cur < 3) {
			this->Note_on_YM2203_FM(this->CH_cur);
		}
		else {
			this->Note_on_YM2203_SSG(this->CH_cur - 3);
		}
		break;
	case 0x97: // Key_set
		this->pCHparam_cur->Key = eve.Param[0];
		if (this->CH_cur < 3) {
			this->Key_set_YM2203_FM(this->CH_cur);
		}
		else {
			this->Key_set_YM2203_SSG(this->CH_cur - 3);
		}
		break;
	case 0x98: // sLFOd
		if (this->CH_cur < 3) {
			this->sLFOd_YM2203_FM(this->CH_cur, eve.ParamW);
		}
		else {
			this->sLFOd_YM2203_SSG(this->CH_cur - 3, eve.ParamW);
		}
		break;
	}
}

void VGMdata::Key_set_YM2203_FM(unsigned __int8 CH)
{
	union {
		struct {
			unsigned __int16 FNumber : 11;
			unsigned __int16 Block : 3;
			unsigned __int16 dummy : 2;
		} S;
		unsigned __int8 B[2];
	} U;

	U.S.FNumber = FNumber[this->pCHparam_cur->Key % 12] + this->pCHparam_cur->Detune;
	U.S.Block = this->pCHparam_cur->Key / 12;
	this->make_data_YM2203(0xA4 + CH, U.B[1]);
	this->make_data_YM2203(0xA0 + CH, U.B[0]);
}

void VGMdata::Note_on_YM2203_FM(unsigned __int8 CH)
{
	union {
		struct {
			unsigned __int8 CH : 2;
			unsigned __int8 : 2;
			unsigned __int8 Op_mask : 4;
		} S;
		unsigned __int8 B;
	} U;

	U.S = { CH, this->pCHparam_cur->T.S.OPR_MASK };
	this->make_data_YM2203(0x28, U.B);
}

void VGMdata::Note_off_YM2203_FM(unsigned __int8 CH)
{
	union {
		struct {
			unsigned __int8 CH : 2;
			unsigned __int8 : 2;
			unsigned __int8 Op_mask : 4;
		} S;
		unsigned __int8 B;
	} U;

	U.S = { CH, 0 };
	this->make_data_YM2203(0x28, U.B);
}

void VGMdata::Key_set_YM2203_SSG(unsigned __int8 CH)
{
	union {
		unsigned __int16 W;
		unsigned __int8 B[2];
	} U;

	U.W = TP[this->pCHparam_cur->Key] + (this->dt_mode ? (-this->pCHparam_cur->Detune) : (-this->pCHparam_cur->Detune >> 2));

	this->make_data_YM2203(0x01 + CH * 2, U.B[1]);
	this->make_data_YM2203(0x00 + CH * 2, U.B[0]);
}

void VGMdata::Note_on_YM2203_SSG(unsigned __int8 CH)
{
	this->SSG_out &= ~(1 << CH);
	this->make_data_YM2203(0x07, this->SSG_out);
}

void VGMdata::Note_off_YM2203_SSG(unsigned __int8 CH)
{
	this->SSG_out |= (1 << CH);
	this->make_data_YM2203(0x07, this->SSG_out);
}

void VGMdata::sLFOd_YM2203_FM(unsigned __int8 CH, __int16 Detune)
{
	union {
		struct {
			unsigned __int16 FNumber : 11;
			unsigned __int16 Block : 3;
			unsigned __int16 dummy : 2;
		} S;
		unsigned __int8 B[2];
	} U;

	U.S.FNumber = FNumber[this->pCHparam_cur->Key % 12] + Detune;
	U.S.Block = this->pCHparam_cur->Key / 12;
	this->make_data_YM2203(0xA4 + CH, U.B[1]);
	this->make_data_YM2203(0xA0 + CH, U.B[0]);
}

void VGMdata::sLFOd_YM2203_SSG(unsigned __int8 CH, __int16 Detune)
{
	union {
		unsigned __int16 W;
		unsigned __int8 B[2];
	} U;

	U.W = TP[this->pCHparam_cur->Key] + (-Detune >> 2);

	this->make_data_YM2203(0x01 + CH * 2, U.B[1]);
	this->make_data_YM2203(0x00 + CH * 2, U.B[0]);
}

void VGMdata::Timer_set_YM2203(void)
{
	size_t NA = 1024 - (((this->master_clock * 2) / (192LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2203(0x24, (NA >> 2) & 0xFF);
	this->make_data_YM2203(0x25, NA & 0x03);
}

void VGMdata::Volume_YM2203_FM(unsigned __int8 CH)
{
	for (size_t op = 0; op < 4; op++) {
		if (this->pCHparam_cur->T.S.Connect == 7 || this->pCHparam_cur->T.S.Connect > 4 && op || this->pCHparam_cur->T.S.Connect > 3 && op >= 2 || op == 3) {
			this->make_data_YM2203(0x40 + 4 * op + CH, (~this->pCHparam_cur->Volume) & 0x7F);
		}
	}
}

void VGMdata::Volume_YM2203_SSG(unsigned __int8 CH)
{
	unsigned __int8 Vol = this->pCHparam_cur->Volume >> 3;
	if (Vol > 15) {
		Vol = 15;
	}
	this->make_data_YM2203(0x08 + CH, Vol);
}

void VGMdata::Tone_select_YM2203_FM(unsigned __int8 CH)
{
	static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
	this->pCHparam_cur->T = *(this->Tunes + this->pCHparam_cur->Tone);

	this->make_data_YM2203(0xB0 + CH, this->pCHparam_cur->T.B.FB_CON);

	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			if (j == 1 && this->version >= 3 && (this->pCHparam_cur->T.S.Connect == 7 || this->pCHparam_cur->T.S.Connect > 4 && op || this->pCHparam_cur->T.S.Connect > 3 && op == 1 || op == 3)) {
			}
			else {
				this->make_data_YM2203(0x30 + 0x10 * j + Op_index[op] + CH, *((unsigned __int8*)&this->pCHparam_cur->T.B.Op[op].DT_MULTI + j));
			}
		}
	}
}

void VGMdata::convert_YM2608(struct EVENT& eve)
{
	this->CH_cur = eve.CH;
	this->pCHparam_cur = this->pCHparam + this->CH_cur;
	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param[0] + 1) >> 1;
		this->Tempo = eve.Param[0];

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2608();
		break;
	case 0xFC: // Detune
		this->pCHparam_cur->Detune = (__int16)((__int8)eve.Param[0]);
		break;
	case 0xEB: // Panpot
		if (this->CH_cur % 6 < 3) {
			this->Panpot_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6, eve.Param[0]);
		}
		break;
	case 0xF5: // Tone select
		if (this->CH_cur % 6 < 3) {
			this->pCHparam_cur->Tone = eve.Param[0];
			this->Tone_select_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6);
		}
		break;
	case 0x80: // Note Off
		if (this->CH_cur % 6 < 3) {
			this->Note_off_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6);
		}
		else {
			this->Note_off_YM2608_SSG(this->CH_cur - 3);
		}
		break;
	case 0xE1: // Velocity
		this->pCHparam_cur->Volume += eve.Param[0];
		this->pCHparam_cur->Volume &= 0x7F;

		if (this->CH_cur % 6 < 3) {
			this->Volume_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6);
		}
		else {
			this->Volume_YM2608_SSG(this->CH_cur - 3);
		}
	case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
		this->pCHparam_cur->Volume = eve.Param[0];

		if (this->CH_cur % 6 < 3) {
			this->Volume_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6);
		}
		else {
			this->Volume_YM2608_SSG(this->CH_cur - 3);
		}
		break;
	case 0x90: // Note on
		if (this->CH_cur % 6 < 3) {
			this->Note_on_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6);
		}
		else {
			this->Note_on_YM2608_SSG(this->CH_cur - 3);
		}
		break;
	case 0x97: // Key set
		this->pCHparam_cur->Key = eve.Param[0];
		if (this->CH_cur % 6 < 3) {
			this->Key_set_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6);
		}
		else {
			this->Key_set_YM2608_SSG(this->CH_cur - 3);
		}
		break;
	case 0x98: // sLFOd
		if (this->CH_cur % 6 < 3) {
			this->sLFOd_YM2608_FM(this->CH_cur / 6, this->CH_cur % 6, eve.ParamW);
		}
		else {
			this->sLFOd_YM2608_SSG(this->CH_cur - 3, eve.ParamW);
		}
		break;
	}
}

void VGMdata::Panpot_YM2608_FM(bool port, unsigned __int8 CH, unsigned __int8 Pan)
{
	if (Pan & 0x80 || Pan == 0 || Pan == 64) {
		this->pCHparam_cur->LRAP.S.L = 1;
		this->pCHparam_cur->LRAP.S.R = 1;
	}
	else if (Pan < 64) {
		this->pCHparam_cur->LRAP.S.L = 1;
		this->pCHparam_cur->LRAP.S.R = 0;
	}
	else if (Pan > 64) {
		this->pCHparam_cur->LRAP.S.L = 0;
		this->pCHparam_cur->LRAP.S.R = 1;
	}

	this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0xB4 + CH, this->pCHparam_cur->LRAP.B);
}

void VGMdata::Key_set_YM2608_FM(bool port, unsigned __int8 CH)
{
	union {
		struct {
			unsigned __int16 FNumber : 11;
			unsigned __int16 Block : 3;
			unsigned __int16 : 2;
		} S;
		unsigned __int8 B[2];
	} U;

	U.S.FNumber = FNumber[this->pCHparam_cur->Key % 12] + this->pCHparam_cur->Detune;
	U.S.Block = this->pCHparam_cur->Key / 12;
	this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0xA4 + CH, U.B[1]);
	this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0xA0 + CH, U.B[0]);
}

void VGMdata::Key_set_YM2608_SSG(unsigned __int8 CH)
{
	union {
		unsigned __int16 W;
		unsigned __int8 B[2];
	} U;

	U.W = TP[this->pCHparam_cur->Key] + (this->dt_mode ? (-this->pCHparam_cur->Detune) : (-this->pCHparam_cur->Detune >> 2));

	this->make_data_YM2608port0(0x01 + CH * 2, U.B[1]);
	this->make_data_YM2608port0(0x00 + CH * 2, U.B[0]);
}

void VGMdata::Note_on_YM2608_FM(bool port, unsigned __int8 CH)
{
	union {
		struct {
			unsigned __int8 CH : 2;
			unsigned __int8 Port : 1;
			unsigned __int8 : 1;
			unsigned __int8 Op_mask : 4;
		} S;
		unsigned __int8 B;
	} U;

	U.S = { CH, port, this->pCHparam_cur->T.S.OPR_MASK };
	this->make_data_YM2608port0(0x28, U.B);
}

void VGMdata::Note_off_YM2608_FM(bool port, unsigned __int8 CH)
{
	union {
		struct {
			unsigned __int8 CH : 2;
			unsigned __int8 Port : 1;
			unsigned __int8 : 1;
			unsigned __int8 Op_mask : 4;
		} S;
		unsigned __int8 B;
	} U;

	U.S = { CH, port, 0 };
	this->make_data_YM2608port0(0x28, U.B);
}

void VGMdata::Note_on_YM2608_SSG(unsigned __int8 CH)
{
	this->SSG_out &= ~(1 << CH);
	this->make_data_YM2608port0(0x07, this->SSG_out);
}

void VGMdata::Note_off_YM2608_SSG(unsigned __int8 CH)
{
	this->SSG_out |= (1 << CH);
	this->make_data_YM2608port0(0x07, this->SSG_out);
}

void VGMdata::sLFOd_YM2608_FM(bool port, unsigned __int8 CH, __int16 Detune)
{
	union {
		struct {
			unsigned __int16 FNumber : 11;
			unsigned __int16 Block : 3;
			unsigned __int16 : 2;
		} S;
		unsigned __int8 B[2];
	} U;

	U.S.FNumber = FNumber[this->pCHparam_cur->Key % 12] + Detune;
	U.S.Block = this->pCHparam_cur->Key / 12;
	this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0xA4 + CH, U.B[1]);
	this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0xA0 + CH, U.B[0]);
}

void VGMdata::sLFOd_YM2608_SSG(unsigned __int8 CH, __int16 Detune)
{
	union {
		unsigned __int16 W;
		unsigned __int8 B[2];
	} U;

	U.W = TP[this->pCHparam_cur->Key] + (-Detune >> 2);

	this->make_data_YM2608port0(0x01 + CH * 2, U.B[1]);
	this->make_data_YM2608port0(0x00 + CH * 2, U.B[0]);
}

void VGMdata::Timer_set_YM2608(void)
{
	size_t NA = 1024 - (((this->master_clock * 2) / (384LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2608port0(0x24, (NA >> 2) & 0xFF);
	this->make_data_YM2608port0(0x25, NA & 0x03);
}

void VGMdata::Volume_YM2608_FM(bool port, unsigned __int8 CH)
{
	for (size_t op = 0; op < 4; op++) {
		if (this->pCHparam_cur->T.S.Connect == 7 || this->pCHparam_cur->T.S.Connect > 4 && op || this->pCHparam_cur->T.S.Connect > 3 && op >= 2 || op == 3) {
			this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0x40 + 4 * op + CH, (~this->pCHparam_cur->Volume) & 0x7F);
		}
	}
}

void VGMdata::Volume_YM2608_SSG(unsigned __int8 CH)
{
	unsigned __int8 Vol = this->pCHparam_cur->Volume >> 3;
	if (Vol > 15) {
		Vol = 15;
	}
	this->make_data_YM2608port0(0x08 + CH, Vol);
}

void VGMdata::Tone_select_YM2608_FM(bool port, unsigned __int8 CH)
{
	static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
	this->pCHparam_cur->T = *(this->Tunes + this->pCHparam_cur->Tone);

	this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0xB0 + CH, this->pCHparam_cur->T.B.FB_CON);

	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			if (j == 1 && this->version >= 3 && (this->pCHparam_cur->T.S.Connect == 7 || this->pCHparam_cur->T.S.Connect > 4 && op || this->pCHparam_cur->T.S.Connect > 3 && op == 1 || op == 3)) {
			}
			else {
				this->make_data(port ? this->vgm_command_YM2608port1 : this->vgm_command_YM2608port0, 0x30 + 0x10 * j + Op_index[op] + CH, *((unsigned __int8*)&this->pCHparam_cur->T.B.Op[op].DT_MULTI + j));
			}
		}
	}
}

void VGMdata::convert_YM2151(struct EVENT& eve)
{
	this->CH_cur = eve.CH;
	this->pCHparam_cur = this->pCHparam + this->CH_cur;

	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param[0] + 1) >> 1;
		this->Tempo = eve.Param[0];

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2151();
		break;
	case 0xFC: // Detune
		this->pCHparam_cur->Detune = (__int16)((__int8)eve.Param[0]);
		break;
	case 0xF5: // Tone select
		this->pCHparam_cur->Tone = eve.Param[0];
		this->Note_off_YM2151();
		this->Tone_select_YM2151();
		break;
	case 0x80: // Note Off
		this->Note_off_YM2151();
		break;
	case 0xE1: // Velocity
		this->pCHparam_cur->Volume += eve.Param[0];
		this->pCHparam_cur->Volume &= 0x7F;

		this->Note_off_YM2151();
		this->Volume_YM2151();
		break;
	case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
		this->pCHparam_cur->Volume = eve.Param[0];

		this->Note_off_YM2151();
		this->Volume_YM2151();
		break;
	case 0x97:
		this->pCHparam_cur->Key = eve.Param[0];
		this->Key_set_YM2151();
		break;
	case 0x90: // Note on
		this->Note_off_YM2151();
		this->Note_on_YM2151();
		break;
	}
}

void VGMdata::Timer_set_YM2151(void)
{
	size_t NA = 1024 - (((3 * this->master_clock * 2) / (512LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2151(0x10, (NA >> 2) & 0xFF);
	this->make_data_YM2151(0x11, NA & 0x03);
}

void VGMdata::Tone_select_YM2151(void)
{
	static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
	this->pCHparam_cur->T = *(this->Tunes + this->pCHparam_cur->Tone);

	this->make_data_YM2151(0x20 + this->CH_cur, this->pCHparam_cur->T.B.FB_CON | 0xC0);
	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			this->make_data_YM2151(0x40 + 0x20 * j + Op_index[op] + this->CH_cur, *((unsigned __int8*)&this->pCHparam_cur->T.B.Op[op].DT_MULTI + j));
		}
	}
}

void VGMdata::SSG_emulation_YM2151(void)
{
	static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
	this->pCHparam_cur->T.B = SSG_emulation;

	this->make_data_YM2151(0x20 + this->CH_cur, this->pCHparam_cur->T.B.FB_CON | 0xC0);
	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			this->make_data_YM2151(0x40 + 0x20 * j + Op_index[op] + this->CH_cur, *((unsigned __int8*)&this->pCHparam_cur->T.B.Op[op].DT_MULTI + j));
		}
	}
}

void VGMdata::Key_set_YM2151(void)
{
	if (this->pCHparam_cur->Key < 3) {
		wprintf_s(L"%2u: Very low key%2u\n", this->CH_cur, this->pCHparam_cur->Key);
		this->pCHparam_cur->Key += 12;
	}
	this->pCHparam_cur->Key -= 3;
	union {
		struct {
			unsigned __int8 note : 4;
			unsigned __int8 oct : 3;
			unsigned __int8 : 1;
		} S;
		unsigned __int8 KC;
	} U;
	U.KC = 0;

	unsigned pre_note = this->pCHparam_cur->Key % 12;
	U.S.note = (pre_note << 2) / 3;
	U.S.oct = this->pCHparam_cur->Key / 12;
	this->make_data_YM2151(0x28 + this->CH_cur, U.KC);
}

void VGMdata::Note_on_YM2151(void) {
	union {
		struct {
			unsigned __int8 CH : 3;
			unsigned __int8 Op_mask : 4;
			unsigned __int8 : 1;
		} S;
		unsigned __int8 B;
	} U;

	U.S = { this->CH_cur, this->pCHparam_cur->T.S.OPR_MASK };
	this->make_data_YM2151(0x08, U.B);
	this->pCHparam_cur->NoteOn = true;
}

void VGMdata::Note_off_YM2151(void)
{
	union {
		struct {
			unsigned __int8 CH : 3;
			unsigned __int8 Op_mask : 4;
			unsigned __int8 : 1;
		} S;
		unsigned __int8 B;
	} U;

	U.S = { this->CH_cur, 0 };

	if (this->pCHparam_cur->NoteOn) {
		this->make_data_YM2151(0x08, U.B);
		this->pCHparam_cur->NoteOn = false;
	}
}

void VGMdata::Volume_YM2151(void)
{
	for (size_t op = 0; op < 4; op++) {
		if (this->pCHparam_cur->T.S.Connect == 7 || this->pCHparam_cur->T.S.Connect > 4 && op || this->pCHparam_cur->T.S.Connect > 3 && op >= 2 || op == 3) {
			this->make_data_YM2151(0x60 + 8 * op + this->CH_cur, (~this->pCHparam_cur->Volume) & 0x7F);
		}
	}
}

void VGMdata::finish(void)
{
	*this->vgm_pos++ = 0x66;
	this->vgm_dlen = this->vgm_pos - this->vgm_out;

	if (this->Ex_Vols_count) {
		this->vgm_extra_len = sizeof(VGM_HDR_EXTRA) + 1 + sizeof(VGMX_CHIP_DATA16) + this->padsize;
	}

	size_t vgm_data_abs = this->vgm_header_len + this->vgm_extra_len;
	this->h_vgm.lngTotalSamples = this->Time_Prev_VGM_abs;
	this->h_vgm.lngDataOffset = vgm_data_abs - ((UINT8*)&this->h_vgm.lngDataOffset - (UINT8*)&this->h_vgm.fccVGM);
	this->h_vgm.lngExtraOffset = this->vgm_header_len - ((UINT8*)&this->h_vgm.lngExtraOffset - (UINT8*)&this->h_vgm.fccVGM);
	this->h_vgm.lngEOFOffset = vgm_data_abs + this->vgm_dlen - ((UINT8*)&this->h_vgm.lngEOFOffset - (UINT8*)&this->h_vgm.fccVGM);

	if (loop_start != NULL) {
		this->h_vgm.lngLoopSamples = this->Time_Prev_VGM_abs - this->Time_Loop_VGM_abs;
		this->h_vgm.lngLoopOffset = vgm_data_abs + (this->vgm_loop_pos - this->vgm_out) - ((UINT8*)&this->h_vgm.lngLoopOffset - (UINT8*)&this->h_vgm.fccVGM);
	}
}

void VGMdata::convert(class EVENTS& in)
{
	for (struct EVENT* src = in.event; (src - in.event) <= in.length; src++) {
		if (this->vgm_pos - this->vgm_out + 200 >= this->length) {
			this->enlarge();
			wprintf_s(L"Memory reallocated in making VGM.\n");
		}
		if (src->time == SIZE_MAX || this->length == 0) {
			break;
		}
		if (src->CH >= this->CHs_limit) {
			continue;
		}
		if (src->time - this->Time_Prev) {
			// Tqn = 60 / Tempo
			// TPQN = 48
			// Ttick = Tqn / 48
			// c_VGMT = Ttick * src_time * VGM_CLOCK 
			//        = 60 / Tempo / 48 * ticks * VGM_CLOCK
			//        = 60 * VGM_CLOCK * ticks / (48 * tempo)
			//        = 60 * VGM_CLOCK * ticks / (48 * master_clock / (192 * (1024 - NA)) (OPN) 
			//        = 60 * VGM_CLOCK * ticks / (48 * master_clock / (384 * (1024 - NA)) (OPNA) 
			//        = 60 * VGM_CLOCK * ticks / (48 * master_clock * 3 / (512 * (1024 - NA)) (OPM) 
			//
			// 実際のタイミングにするために9/10倍している。
			// 本来、更にNAの整数演算に伴う計算誤差を加味すれば正確になるが、20分鳴らして2-4秒程度なので無視する事とした。
			// 一度はそうしたコードも書いたのでレポジトリの履歴を追えば見つかる。

			size_t c_VGMT = (src->time * 60 * VGM_CLOCK * 2 * 9 / (48 * this->Tempo * 10) + 1) >> 1;
			size_t d_VGMT = c_VGMT - this->Time_Prev_VGM;

			//				wprintf_s(L"%8zu: %zd %zd %zd\n", src->time, c_VGMT, d_VGMT, Time_Prev_VGM);
			this->Time_Prev_VGM += d_VGMT;
			this->Time_Prev_VGM_abs += d_VGMT;
			this->Time_Prev = src->time;

			this->make_wait(d_VGMT);
		}

		if ((src - in.event) == in.length) {
			break;
		}

		if (in.loop_enable && (src - in.event) == in.loop_start) {
			this->Time_Loop_VGM_abs = Time_Prev_VGM_abs;
			this->vgm_loop_pos = this->vgm_pos;
			this->loop_start = src;
			in.loop_enable = false;
		}

		if (chip == CHIP::YM2203) {
			this->convert_YM2203(*src);
		}
		else if (chip == CHIP::YM2608) {
			this->convert_YM2608(*src);
		}
		else if (chip == CHIP::YM2151) {
			this->convert_YM2151(*src);
		}
	}
	this->finish();
}

void VGMdata::out(wchar_t* p)
{
	FILE* pFo;
	wchar_t* path = filename_replace_ext(p, L".vgm");

	errno_t ecode = _wfopen_s(&pFo, path, L"wb");
	if (ecode || !pFo) {
		fwprintf_s(stderr, L"%s cannot open\n", path);
		exit(ecode);
	}

	fwrite(&h_vgm, 1, this->vgm_header_len, pFo);
	if (this->vgm_extra_len) {
		fwrite(&eh_vgm, 1, sizeof(VGM_HDR_EXTRA), pFo);
		if (this->Ex_Vols_count) {
			fwrite(&this->Ex_Vols_count, 1, 1, pFo);
			fwrite(&this->Ex_Vols, sizeof(VGMX_CHIP_DATA16), this->Ex_Vols_count, pFo);
		}
		UINT8 PADDING[15] = { 0 };
		fwrite(PADDING, 1, this->padsize, pFo);
	}
	fwrite(this->vgm_out, 1, this->vgm_dlen, pFo);
	wprintf_s(L"VGM body length %8zu\n", this->vgm_dlen);

	fclose(pFo);

}