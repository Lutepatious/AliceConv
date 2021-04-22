#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"

#include "MAKO2toVGM.h"
#include "event.h"
#include "toVGM.h"

// MAKO.OCM (1,2共通)に埋め込まれているF-number (MAKO1は全オクターブ分入っている)
static const unsigned __int16 FNumber[12] = { 0x0269, 0x028E, 0x02B4, 0x02DE, 0x0309, 0x0338, 0x0369, 0x039C, 0x03D3, 0x040E, 0x044B, 0x048D };
static const unsigned __int16 TP[] = {
	0x0EED,0x0E17,0x0D4C,0x0C8D,0x0BD9,0x0B2F,0x0A8E,0x09F6,0x0967,0x08E0,0x0860,0x07E8,
	0x0776,0x070B,0x06A6,0x0646,0x05EC,0x0597,0x0547,0x04FB,0x04B3,0x0470,0x0430,0x03F4,
	0x03BB,0x0385,0x0353,0x0323,0x02F6,0x02CB,0x02A3,0x027D,0x0259,0x0238,0x0218,0x01FA,
	0x01DD,0x01C2,0x01A9,0x0191,0x017B,0x0165,0x0151,0x013E,0x012C,0x011C,0x010C,0x00FD,
	0x00EE,0x00E1,0x00D4,0x00C8,0x00BD,0x00B2,0x00A8,0x009F,0x0096,0x008E,0x0086,0x007E,
	0x0077,0x0070,0x006A,0x0064,0x005E,0x0059,0x0054,0x004F,0x004B,0x0047,0x0043,0x003F,
	0x003B,0x0038,0x0035,0x0032,0x002F,0x002C,0x002A,0x0027,0x0025,0x0023,0x0021,0x001F,
	0x001D,0x001C,0x001A,0x0019,0x0017,0x0016,0x0015,0x0013,0x0012,0x0011,0x0010,0x000F };

VGMdata::VGMdata(size_t elems, enum class CHIP chip, unsigned ver, struct mako2_tone* t, size_t ntones)
{
	this->T = t;
	this->tones = ntones;
	this->version = ver;
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
		h_vgm.bytAYFlagYM2203 = 0x1;
		wprintf_s(L"YM2203 mode.\n");
	}
	else if (this->chip == CHIP::YM2608) {
		this->CHs_limit = 9;
		h_vgm.lngHzYM2608 = this->master_clock = 7987200;
		h_vgm.bytAYFlagYM2608 = 0x1;
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
		wprintf_s(L"Voice %2llu: FB %1d Connect %1d\n", i, (this->T + i)->H.S.FB, (this->T + i)->H.S.Connect);
		for (size_t j = 0; j < 4; j++) {
			wprintf_s(L" OP %1llu: DT %1d MULTI %2d TL %3d KS %1d AR %2d DR %2d SR %2d SL %2d RR %2d\n"
				, j, (this->T + i)->Op[j].S.DT, (this->T + i)->Op[j].S.MULTI, (this->T + i)->Op[j].S.TL, (this->T + i)->Op[j].S.KS
				, (this->T + i)->Op[j].S.AR, (this->T + i)->Op[j].S.DR, (this->T + i)->Op[j].S.SR, (this->T + i)->Op[j].S.SL, (this->T + i)->Op[j].S.RR);
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
		0x54, 0x14, 0x00,
		0x54, 0x43, 0x02, 0x54, 0x44, 0x02, 0x54, 0x45, 0x02, 0x54, 0x63, 0x0F, 0x54, 0x64, 0x0F, 0x54, 0x65, 0x0F,
		0x54, 0x83, 0x1F, 0x54, 0x84, 0x1F, 0x54, 0x85, 0x1F, 0x54, 0xA3, 0x00, 0x54, 0xA4, 0x00, 0x54, 0xA5, 0x00,
		0x54, 0xC3, 0x00, 0x54, 0xC4, 0x00, 0x54, 0xC5, 0x00, 0x54, 0xE3, 0x0F, 0x54, 0xE4, 0x0F, 0x54, 0xE5, 0x0F,
		0x54, 0x4B, 0x04, 0x54, 0x4C, 0x04, 0x54, 0x4D, 0x04, 0x54, 0x6B, 0x28, 0x54, 0x6C, 0x28, 0x54, 0x6D, 0x28,
		0x54, 0x8B, 0x1F, 0x54, 0x8C, 0x1F, 0x54, 0x8D, 0x1F, 0x54, 0xAB, 0x00, 0x54, 0xAC, 0x00, 0x54, 0xAD, 0x00,
		0x54, 0xCB, 0x00, 0x54, 0xCC, 0x00, 0x54, 0xCD, 0x00, 0x54, 0xEB, 0x0F, 0x54, 0xEC, 0x0F, 0x54, 0xED, 0x0F,
		0x54, 0x53, 0x06, 0x54, 0x54, 0x06, 0x54, 0x55, 0x06, 0x54, 0x73, 0x28, 0x54, 0x74, 0x28, 0x54, 0x75, 0x28,
		0x54, 0x93, 0x1F, 0x54, 0x94, 0x1F, 0x54, 0x95, 0x1F, 0x54, 0xB3, 0x00, 0x54, 0xB4, 0x00, 0x54, 0xB5, 0x00,
		0x54, 0xD3, 0x00, 0x54, 0xD4, 0x00, 0x54, 0xD5, 0x00, 0x54, 0xF3, 0x0F, 0x54, 0xF4, 0x0F, 0x54, 0xF5, 0x0F,
		0x54, 0x5B, 0x02, 0x54, 0x5C, 0x02, 0x54, 0x5D, 0x02, 0x54, 0x7B, 0x00, 0x54, 0x7C, 0x00, 0x54, 0x7D, 0x00,
		0x54, 0x9B, 0x1F, 0x54, 0x9C, 0x1F, 0x54, 0x9D, 0x1F, 0x54, 0xBB, 0x00, 0x54, 0xBC, 0x00, 0x54, 0xBD, 0x00,
		0x54, 0xDB, 0x00, 0x54, 0xDC, 0x00, 0x54, 0xDD, 0x00, 0x54, 0xFB, 0x0F, 0x54, 0xFC, 0x0F, 0x54, 0xFD, 0x0F,
		0x54, 0x23, 0xC3, 0x54, 0x24, 0xC3, 0x54, 0x25, 0xC3 };

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
	this->pCHparam_cur = this->pCHparam + eve.CH;

	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param.B[0] + 1) >> 1;
		this->Tempo = eve.Param.B[0];

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2203();
		break;
	case 0xFC: // Detune
		this->pCHparam_cur->Detune = (__int16)((__int8)eve.Param.B[0]);
		break;
	case 0xF5: // Tone select
		if (eve.CH < 3) {
			this->pCHparam_cur->Tone = eve.Param.B[0];
			this->Tone_select_YM2203_FM(eve.CH);
		}
		break;
	case 0x80: // Note Off
		if (eve.CH < 3) {
			this->make_data_YM2203(0x28, eve.CH);
		}
		else if (eve.CH < 6) {
			this->SSG_out |= (1 << (eve.CH - 3));
			this->make_data_YM2203(0x07, this->SSG_out);
		}
		break;
	case 0xE1: // Velocity
		this->pCHparam_cur->Volume += eve.Param.B[0];
		this->pCHparam_cur->Volume &= 0x7F;

		if (eve.CH < 3) {
			this->Volume_YM2203_FM(eve.CH);
		}
		else {
			this->make_data_YM2203(0x08 + eve.CH - 3, this->pCHparam_cur->Volume >> 3);
		}
		break;
	case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
		this->pCHparam_cur->Volume = eve.Param.B[0];

		if (eve.CH < 3) {
			this->Volume_YM2203_FM(eve.CH);
		}
		else {
			this->make_data_YM2203(0x08 + eve.CH - 3, this->pCHparam_cur->Volume >> 3);
		}
		break;
	case 0x90: // Note on
		if (eve.CH >= 3) {
			union {
				unsigned __int16 W;
				unsigned __int8 B[2];
			} U;

			U.W = TP[eve.Param.B[0]] + (-(pCHparam + eve.CH)->Detune >> 2);

			this->make_data_YM2203(0x01 + (eve.CH - 3) * 2, U.B[1]);
			this->make_data_YM2203(0x00 + (eve.CH - 3) * 2, U.B[0]);
			this->SSG_out &= ~(1 << (eve.CH - 3));
			this->make_data_YM2203(0x07, this->SSG_out);
			break;
		}

		union {
			struct {
				unsigned __int16 FNumber : 11;
				unsigned __int16 Block : 3;
				unsigned __int16 dummy : 2;
			} S;
			unsigned __int8 B[2];
		} U;

		U.S.FNumber = FNumber[eve.Param.B[0] % 12] + (pCHparam + eve.CH)->Detune;
		U.S.Block = eve.Param.B[0] / 12;
		this->make_data_YM2203(0xA4 + eve.CH, U.B[1]);
		this->make_data_YM2203(0xA0 + eve.CH, U.B[0]);
		this->make_data_YM2203(0x28, eve.CH | 0xF0);
		break;
	case 0xE0: // Counter
		break;
	}
}

void VGMdata::Timer_set_YM2203(void)
{
	size_t NA = 1024 - (((this->master_clock * 2) / (192LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2203(0x24, (NA >> 2) & 0xFF);
	this->make_data_YM2203(0x25, NA & 0x03);
}

void VGMdata::Volume_YM2203_FM(unsigned __int8 CH)
{
	unsigned Algorithm = (T + this->pCHparam_cur->Tone)->H.S.Connect;

	for (size_t op = 0; op < 4; op++) {
		if (Algorithm == 7 || Algorithm > 4 && op || Algorithm > 3 && op >= 2 || op == 3) {
			this->make_data_YM2203(0x40 + 4 * op + CH, (~this->pCHparam_cur->Volume) & 0x7F);
		}
	}
}

void VGMdata::Tone_select_YM2203_FM(unsigned __int8 CH)
{
	static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
	struct mako2_tone* tone_cur = T + this->pCHparam_cur->Tone;
	unsigned Algorithm = tone_cur->H.S.Connect;

	this->make_data_YM2203(0xB0 + CH, tone_cur->H.B);

	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			if (j == 1 && this->version >= 3 && (Algorithm == 7 || Algorithm > 4 && op || Algorithm > 3 && op >= 2 || op == 3)) {
			}
			else {
				this->make_data_YM2203(0x30 + 0x10 * j + Op_index[op] + CH, tone_cur->Op[op].B[j]);
			}
		}
	}
}

void VGMdata::convert_YM2608(struct EVENT& eve)
{
	this->pCHparam_cur = this->pCHparam + eve.CH;
	unsigned __int8 out_Port;
	unsigned __int8 out_Ch;
	union LR_AMS_PMS_YM2608 LRAP;
	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param.B[0] + 1) >> 1;
		this->Tempo = eve.Param.B[0];

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2608();
		break;
	case 0xFC: // Detune
		this->pCHparam_cur->Detune = (__int16)((__int8)eve.Param.B[0]);
		break;
	case 0xEB: // Panpot
		this->pCHparam_cur->Panpot = 3;
		if (eve.CH < 3) {
			out_Ch = eve.CH;
			out_Port = vgm_command_YM2608port0;
		}
		else if (eve.CH > 5) {
			out_Ch = eve.CH - 6;
			out_Port = vgm_command_YM2608port1;
		}
		else {
			break;
		}

		if (eve.Param.B[0] & 0x80 || eve.Param.B[0] == 0) {
			this->pCHparam_cur->Panpot = 3;
		}
		else if (eve.Param.B[0] < 64) {
			this->pCHparam_cur->Panpot = 2;
		}
		else if (eve.Param.B[0] > 64) {
			this->pCHparam_cur->Panpot = 1;
		}

		LRAP.B = 0;
		LRAP.S.LR = this->pCHparam_cur->Panpot;
		this->make_data(out_Port, 0xB4 + out_Ch, LRAP.B);
		break;
	case 0xF5: // Tone select
		if (eve.CH < 3) {
			this->pCHparam_cur->Tone = eve.Param.B[0];
			this->Tone_select_YM2608_FMport0(eve.CH);
		}
		else if (eve.CH > 5) {
			this->pCHparam_cur->Tone = eve.Param.B[0];
			this->Tone_select_YM2608_FMport1(eve.CH - 6);
		}
		break;
	case 0x80: // Note Off
		if (eve.CH < 3) {
			this->make_data_YM2608port0(0x28, eve.CH);
		}
		else if (eve.CH < 6) {
			this->SSG_out |= (1 << (eve.CH - 3));
			this->make_data_YM2608port0(0x07, this->SSG_out);
		}
		else if (eve.CH > 5) {
			this->make_data_YM2608port0(0x28, (eve.CH - 6) | 0x04);
		}
		break;
	case 0xE1: // Velocity
		this->pCHparam_cur->Volume += eve.Param.B[0];
		this->pCHparam_cur->Volume &= 0x7F;

		if (eve.CH < 3) {
			this->Volume_YM2608_FMport0(eve.CH);
		}
		else if (eve.CH > 5) {
			this->Volume_YM2608_FMport1(eve.CH - 6);
		}
		else {
			this->make_data_YM2608port0(0x08 + eve.CH - 3, (pCHparam + eve.CH)->Volume >> 3);
		}
	case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
		this->pCHparam_cur->Volume = eve.Param.B[0];

		if (eve.CH < 3) {
			this->Volume_YM2608_FMport0(eve.CH);
		}
		else if (eve.CH > 5) {
			this->Volume_YM2608_FMport1(eve.CH - 6);
		}
		else {
			this->make_data_YM2608port0(0x08 + eve.CH - 3, (pCHparam + eve.CH)->Volume >> 3);
		}
		break;
	case 0x90: // Note on
		if (eve.CH < 3) {
			out_Ch = eve.CH;
			out_Port = 0x56;
		}
		else if (eve.CH > 5) {
			out_Ch = eve.CH - 6;
			out_Port = 0x57;
		}
		else {
			union {
				unsigned __int16 W;
				unsigned __int8 B[2];
			} U;

			U.W = TP[eve.Param.B[0]] + (-this->pCHparam_cur->Detune >> 2);

			this->make_data_YM2608port0(0x01 + (eve.CH - 3) * 2, U.B[1]);
			this->make_data_YM2608port0(0x00 + (eve.CH - 3) * 2, U.B[0]);
			this->SSG_out &= ~(1 << (eve.CH - 3));
			this->make_data_YM2608port0(0x07, this->SSG_out);
			break;
		}

		union {
			struct {
				unsigned __int16 FNumber : 11;
				unsigned __int16 Block : 3;
				unsigned __int16 dummy : 2;
			} S;
			unsigned __int8 B[2];
		} U;

		U.S.FNumber = FNumber[eve.Param.B[0] % 12] + this->pCHparam_cur->Detune;
		U.S.Block = eve.Param.B[0] / 12;
		this->make_data(out_Port, 0xA4 + out_Ch, U.B[1]);
		this->make_data(out_Port, 0xA0 + out_Ch, U.B[0]);
		this->make_data_YM2608port0(0x28, out_Ch | 0xF0 | ((eve.CH > 5) ? 0x4 : 0));
		break;
	case 0xE0: // Counter
		break;
	}
}

void VGMdata::Timer_set_YM2608(void)
{
	size_t NA = 1024 - (((this->master_clock * 2) / (384LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2608port0(0x24, (NA >> 2) & 0xFF);
	this->make_data_YM2608port0(0x25, NA & 0x03);
}

void VGMdata::Volume_YM2608_FMport0(unsigned __int8 CH)
{
	unsigned Algorithm = (T + this->pCHparam_cur->Tone)->H.S.Connect;

	for (size_t op = 0; op < 4; op++) {
		if (Algorithm == 7 || Algorithm > 4 && op || Algorithm > 3 && op >= 2 || op == 3) {
			this->make_data_YM2608port0(0x40 + 4 * op + CH, (~this->pCHparam_cur->Volume) & 0x7F);
		}
	}
}

void VGMdata::Volume_YM2608_FMport1(unsigned __int8 CH)
{
	unsigned Algorithm = (T + this->pCHparam_cur->Tone)->H.S.Connect;

	for (size_t op = 0; op < 4; op++) {
		if (Algorithm == 7 || Algorithm > 4 && op || Algorithm > 3 && op >= 2 || op == 3) {
			this->make_data_YM2608port1(0x40 + 4 * op + CH, (~this->pCHparam_cur->Volume) & 0x7F);
		}
	}
}

void VGMdata::Tone_select_YM2608_FMport0(unsigned __int8 CH)
{
	static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
	struct mako2_tone* tone_cur = T + this->pCHparam_cur->Tone;
	unsigned Algorithm = tone_cur->H.S.Connect;

	this->make_data_YM2608port0(0xB0 + CH, tone_cur->H.B);

	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			if (j == 1 && this->version >= 3 && (Algorithm == 7 || Algorithm > 4 && op || Algorithm > 3 && op >= 2 || op == 3)) {
			}
			else {
				this->make_data_YM2608port0(0x30 + 0x10 * j + Op_index[op] + CH, tone_cur->Op[op].B[j]);
			}
		}
	}
}

void VGMdata::Tone_select_YM2608_FMport1(unsigned __int8 CH)
{
	static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
	struct mako2_tone* tone_cur = T + this->pCHparam_cur->Tone;
	unsigned Algorithm = tone_cur->H.S.Connect;

	this->make_data_YM2608port1(0xB0 + CH, tone_cur->H.B);

	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			if (j == 1 && this->version >= 3 && (Algorithm == 7 || Algorithm > 4 && op || Algorithm > 3 && op >= 2 || op == 3)) {
			}
			else {
				this->make_data_YM2608port1(0x30 + 0x10 * j + Op_index[op] + CH, tone_cur->Op[op].B[j]);
			}
		}
	}
}

void VGMdata::convert_YM2151(struct EVENT& eve)
{
	this->pCHparam_cur = this->pCHparam + eve.CH;

	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param.B[0] + 1) >> 1;
		this->Tempo = eve.Param.B[0];

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2151();
		break;
	case 0xFC: // Detune
		this->pCHparam_cur->Detune = (__int16)((__int8)eve.Param.B[0]);
		break;
	case 0xEB: // Panpot
		this->pCHparam_cur->Panpot = 3;
		if (eve.Param.B[0] & 0x80 || eve.Param.B[0] == 0) {
			this->pCHparam_cur->Panpot = 3;
		}
		else if (eve.Param.B[0] < 64) {
			this->pCHparam_cur->Panpot = 2;
		}
		else if (eve.Param.B[0] > 64) {
			this->pCHparam_cur->Panpot = 1;
		}
		break;
	case 0xF5: // Tone select
		this->pCHparam_cur->Tone = eve.Param.B[0];
		this->Note_off_YM2151(eve.CH);
		this->Tone_select_YM2151(eve.CH);
		break;
	case 0x80: // Note Off
		this->Note_off_YM2151(eve.CH);
		break;
	case 0xE1: // Velocity
		this->pCHparam_cur->Volume += eve.Param.B[0];
		this->pCHparam_cur->Volume &= 0x7F;

		this->Note_off_YM2151(eve.CH);
		this->Volume_YM2151(eve.CH);
		break;
	case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
		this->pCHparam_cur->Volume = eve.Param.B[0];

		this->Note_off_YM2151(eve.CH);
		this->Volume_YM2151(eve.CH);
		break;
	case 0x90: // Note on
		this->Note_off_YM2151(eve.CH);
		this->Note_on_YM2151(eve.CH, eve.Param.B[0]);
		break;
	case 0xE0: // Counter
		break;
	}
}

void VGMdata::Timer_set_YM2151(void)
{
	size_t NA = 1024 - (((3 * this->master_clock * 2) / (512LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2151(0x10, (NA >> 2) & 0xFF);
	this->make_data_YM2151(0x11, NA & 0x03);
}

void VGMdata::Tone_select_YM2151(unsigned __int8 CH)
{
	static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
	struct mako2_tone* tone_cur = T + this->pCHparam_cur->Tone;
	unsigned Algorithm = tone_cur->H.S.Connect;
	tone_cur->H.S.RL = this->pCHparam_cur->Panpot;

	this->make_data_YM2151(0x20 + CH, tone_cur->H.B);
	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			this->make_data_YM2151(0x40 + 0x20 * j + Op_index[op] + CH, tone_cur->Op[op].B[j]);
		}
	}
}

void VGMdata::Note_off_YM2151(unsigned __int8 CH)
{
	if (this->pCHparam_cur->NoteOn) {
		this->make_data_YM2151(0x08, CH);
		this->pCHparam_cur->NoteOn = false;
	}
}

void VGMdata::Note_on_YM2151(unsigned __int8 CH, unsigned __int8 key)
{
	if (key < 3) {
		wprintf_s(L"%2u: Very low key%2u\n", CH, key);
		key += 12;
	}
	key -= 3;
	union {
		struct {
			unsigned __int8 note : 4;
			unsigned __int8 oct : 3;
			unsigned __int8 dummy : 1;
		} S;
		unsigned __int8 KC;
	} U;
	U.KC = 0;

	unsigned pre_note = key % 12;
	U.S.note = (pre_note << 2) / 3;
	U.S.oct = key / 12;
	this->make_data_YM2151(0x28 + CH, U.KC);
	this->make_data_YM2151(0x08, CH | 0x78);
	this->pCHparam_cur->NoteOn = true;
}

void VGMdata::Volume_YM2151(unsigned __int8 CH)
{
	unsigned Algorithm = (T + this->pCHparam_cur->Tone)->H.S.Connect;

	for (size_t op = 0; op < 4; op++) {
		if (Algorithm == 7 || Algorithm > 4 && op || Algorithm > 3 && op >= 2 || op == 3) {
			this->make_data_YM2151(0x60 + 8 * op + CH, (~this->pCHparam_cur->Volume) & 0x7F);
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