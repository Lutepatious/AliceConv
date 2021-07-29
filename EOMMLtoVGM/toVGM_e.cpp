#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"

#include "EOMMLtoVGM.h"
#include "EOMML.h"
#include "event_e.h"
#include "toVGM_e.h"
#include "tools.h"

#define MASTERCLOCK_NEC_OPN (3993600)
#define MASTERCLOCK_SHARP_OPM (4000000)

void VGMdata_e::enlarge(void)
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

void VGMdata_e::make_data(unsigned __int8 command, unsigned __int8 address, unsigned __int8 data)
{
	*this->vgm_pos++ = command;
	*this->vgm_pos++ = address;
	*this->vgm_pos++ = data;
}

void VGMdata_e::make_init(void)
{
	const static unsigned char Init_YM2203[] = {
		0x55, 0x00, 'W', 0x55, 0x00, 'A', 0x55, 0x00, 'O', 0x55, 0x27, 0x30, 0x55, 0x07, 0xBF,
		0x55, 0x90, 0x00, 0x55, 0x91, 0x00, 0x55, 0x92, 0x00, 0x55, 0x24, 0x70, 0x55, 0x25, 0x00 };

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

	if (this->arch == Machine::X68000) {
		Init = Init_YM2151;
		Init_len = sizeof(Init_YM2151);
	}
	else {
		Init = Init_YM2203;
		Init_len = sizeof(Init_YM2203);
	}

	memcpy_s(this->vgm_pos, Init_len, Init, Init_len);
	this->vgm_pos += Init_len;
}

void VGMdata_e::make_wait(size_t wait)
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
		else if (wait < wait3) {
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

void VGMdata_e::finish(void)
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

void VGMdata_e::out(wchar_t* p)
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

void VGMdata_e::convert_YM2151(struct EVENT& eve)
{
	this->CH_cur = eve.CH;
	this->pCHparam_cur = this->pCHparam + this->CH_cur;

	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
		this->Tempo = eve.Param;

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2151();
		break;
	case 0xF5: // Tone select
		this->pCHparam_cur->Tone = eve.Param;
		this->Note_off_YM2151();
		this->Tone_select_YM2151();
		break;
	case 0x80: // Note Off
		this->Note_off_YM2151();
		break;
	case 0xF8: // Volume change V{0-15}
		this->pCHparam_cur->Volume = ((unsigned)eve.Param * 34 + 1) >> 2;

		this->Volume_YM2151();
		break;
	case 0xF9: // Volume change V{0-127}
		this->pCHparam_cur->Volume = eve.Param;

		this->Volume_YM2151();
		break;
	case 0x97:
		this->pCHparam_cur->Key = eve.Param;
		this->Key_set_YM2151();
		break;
	case 0x90: // Note on
		this->Note_on_YM2151();
		break;
	}
}

void VGMdata_e::Timer_set_YM2151(void)
{
	size_t NA = 1024 - (((3 * this->master_clock * 2) / (512LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2151(0x10, (NA >> 2) & 0xFF);
	this->make_data_YM2151(0x11, NA & 0x03);
}

void VGMdata_e::Tone_select_YM2151(void)
{
	static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
	this->pCHparam_cur->T_x68.B = *(this->preset_x68 + this->pCHparam_cur->Tone);

	this->make_data_YM2151(0x20 + this->CH_cur, this->pCHparam_cur->T_x68.B.FB_CON);
	this->make_data_YM2151(0x38 + this->CH_cur, this->pCHparam_cur->T_x68.B.PMS_AMS);
	this->make_data_YM2151(0x18, this->pCHparam_cur->T_x68.B.LFRQ);
	this->make_data_YM2151(0x19, this->pCHparam_cur->T_x68.B.PMD);
	this->make_data_YM2151(0x19, this->pCHparam_cur->T_x68.B.AMD);
	this->make_data_YM2151(0x1B, this->pCHparam_cur->T_x68.B.CT_WAVE);
	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			this->make_data_YM2151(0x40 + 0x20 * j + Op_index[op] + this->CH_cur, *((unsigned __int8*)&this->pCHparam_cur->T_x68.B.Op[op].DT_MULTI + j));
		}
	}
}

void VGMdata_e::Key_set_YM2151(void)
{
	if (this->pCHparam_cur->Key < 2) {
		wprintf_s(L"%2u: Very low key%2u\n", this->CH_cur, this->pCHparam_cur->Key);
		this->pCHparam_cur->Key += 12;
	}
	this->pCHparam_cur->Key -= 2;
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

	union {
		struct {
			unsigned __int8 : 2;
			unsigned __int8 Frac : 6;
		} S;
		unsigned __int8 KF;
	} V;

	V.S.Frac = 5;
	this->make_data_YM2151(0x29 + this->CH_cur, V.KF);
}

void VGMdata_e::Note_on_YM2151(void) {
	union {
		struct {
			unsigned __int8 CH : 3;
			unsigned __int8 Op_mask : 4;
			unsigned __int8 : 1;
		} S;
		unsigned __int8 B;
	} U;

	U.S = { this->CH_cur, this->pCHparam_cur->T_x68.S.OPR_MASK };
	this->make_data_YM2151(0x08, U.B);
	this->pCHparam_cur->NoteOn = true;
}

void VGMdata_e::Note_off_YM2151(void)
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

void VGMdata_e::Volume_YM2151(void)
{
	for (size_t op = 0; op < 4; op++) {
		if (this->pCHparam_cur->T_x68.S.Connect == 7 || this->pCHparam_cur->T_x68.S.Connect > 4 && op || this->pCHparam_cur->T_x68.S.Connect > 3 && op >= 2 || op == 3) {
			this->make_data_YM2151(0x60 + 8 * op + this->CH_cur, (~this->pCHparam_cur->Volume) & 0x7F);
		}
	}
}

void VGMdata_e::convert_YM2203(struct EVENT& eve)
{
	this->CH_cur = eve.CH;
	this->pCHparam_cur = this->pCHparam + this->CH_cur;

	switch (eve.Event) {
	case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
		this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
		this->Tempo = eve.Param;

		// この後のNAの計算とタイマ割り込みの設定は実際には不要
		this->Timer_set_YM2203();
		break;
	case 0xF5: // Tone select
		if (this->CH_cur < 3) {
			this->pCHparam_cur->Tone = eve.Param & 0x7F;
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
	case 0xF8: // Volume change V{0-15}

		if (this->CH_cur < 3) {
			this->pCHparam_cur->Volume = ~(((unsigned)eve.Param * 34 + 1) >> 2) & 0x7F;
			this->Volume_YM2203_FM(this->CH_cur);
		}
		else {
			this->pCHparam_cur->Volume = eve.Param;
			this->make_data_YM2203(0x08 + this->CH_cur - 3, this->pCHparam_cur->Volume);
		}
		break;
	case 0xF9: // Volume change @V{0-127}

		if (this->CH_cur < 3) {
			this->pCHparam_cur->Volume = ~eve.Param & 0x7F;
			this->Volume_YM2203_FM(this->CH_cur);
		}
		else {
			this->pCHparam_cur->Volume = ((unsigned)eve.Param * 121 + 512) >> 10;
			this->make_data_YM2203(0x08 + this->CH_cur - 3, this->pCHparam_cur->Volume);
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
		this->pCHparam_cur->Key = eve.Param;
		if (this->CH_cur < 3) {
			this->Key_set_YM2203_FM(this->CH_cur);
		}
		else {
			this->Key_set_YM2203_SSG(this->CH_cur - 3);
		}
		break;
	}
}

void VGMdata_e::Key_set_YM2203_FM(unsigned __int8 CH)
{
	union {
		struct {
			unsigned __int16 FNumber : 11;
			unsigned __int16 Block : 3;
			unsigned __int16 : 2;
		} S;
		unsigned __int8 B[2];
	} U;

	unsigned __int8 Octave = this->pCHparam_cur->Key / 12;
	U.S.FNumber = this->FNumber[this->pCHparam_cur->Key % 12];
	if (Octave == 8) {
		U.S.FNumber <<= 1;
		Octave = 7;
	}
	U.S.Block = Octave;

	this->make_data_YM2203(0xA4 + CH, U.B[1]);
	this->make_data_YM2203(0xA0 + CH, U.B[0]);
}

void VGMdata_e::Note_on_YM2203_FM(unsigned __int8 CH)
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

void VGMdata_e::Note_off_YM2203_FM(unsigned __int8 CH)
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

void VGMdata_e::Key_set_YM2203_SSG(unsigned __int8 CH)
{
	union {
		unsigned __int16 W;
		unsigned __int8 B[2];
	} U;

	U.W = this->TPeriod[this->pCHparam_cur->Key];

	this->make_data_YM2203(0x01 + CH * 2, U.B[1]);
	this->make_data_YM2203(0x00 + CH * 2, U.B[0]);
}

void VGMdata_e::Note_on_YM2203_SSG(unsigned __int8 CH)
{
	this->SSG_out &= ~(1 << CH);
	this->make_data_YM2203(0x07, this->SSG_out);
}

void VGMdata_e::Note_off_YM2203_SSG(unsigned __int8 CH)
{
	this->SSG_out |= (1 << CH);
	this->make_data_YM2203(0x07, this->SSG_out);
}

void VGMdata_e::Timer_set_YM2203(void)
{
	size_t NA = 1024 - (((this->master_clock * 2) / (192LL * this->Tempo) + 1) >> 1);
	this->make_data_YM2203(0x24, (NA >> 2) & 0xFF);
	this->make_data_YM2203(0x25, NA & 0x03);
}

void VGMdata_e::Volume_YM2203_FM(unsigned __int8 CH)
{
	for (size_t op = 0; op < 4; op++) {
		if (this->pCHparam_cur->T.S.Connect == 7 || this->pCHparam_cur->T.S.Connect > 4 && op || this->pCHparam_cur->T.S.Connect > 3 && op >= 2 || op == 3) {
			this->make_data_YM2203(0x40 + 4 * op + CH, this->pCHparam_cur->Volume);
		}
	}
}

void VGMdata_e::Tone_select_YM2203_FM(unsigned __int8 CH)
{
	static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
	this->pCHparam_cur->T.B = *(this->preset + this->pCHparam_cur->Tone);

	this->make_data_YM2203(0xB0 + CH, this->pCHparam_cur->T.B.FB_CON);

	for (size_t op = 0; op < 4; op++) {
		for (size_t j = 0; j < 6; j++) {
			if (j == 1 && (this->pCHparam_cur->T.S.Connect == 7 || this->pCHparam_cur->T.S.Connect > 4 && op || this->pCHparam_cur->T.S.Connect > 3 && op == 1 || op == 3)) {
			}
			else {
				this->make_data_YM2203(0x30 + 0x10 * j + Op_index[op] + CH, *((unsigned __int8*)&this->pCHparam_cur->T.B.Op[op].DT_MULTI + j));
			}
		}
	}
}
