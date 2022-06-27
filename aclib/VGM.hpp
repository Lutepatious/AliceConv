#include <vector>
#include <iostream>
#include <fstream>

#pragma pack(push)
#pragma pack(1)
#include "stdtype.h"
#include "VGMFile.h"

constexpr size_t VGM_CLOCK = 44100;

class VGMHEADER : public VGM_HEADER {
public:
	VGMHEADER(void)
	{
		fccVGM = FCC_VGM;
		lngVersion = 0x171;
		lngEOFOffset = lngHzPSG = lngHzYM2413 = lngGD3Offset = lngTotalSamples = lngLoopOffset = lngLoopSamples = lngRate =
			lngHzYM2612 = lngHzYM2151 = lngDataOffset = lngHzSPCM = lngSPCMIntf = lngHzRF5C68 = lngHzYM2203 = lngHzYM2608 = lngHzYM2610 =
			lngHzYM3812 = lngHzYM3526 = lngHzY8950 = lngHzYMF262 = lngHzYMF278B = lngHzYMF271 = lngHzYMZ280B = lngHzRF5C164 =
			lngHzPWM = lngHzAY8910 = lngHzGBDMG = lngHzNESAPU = lngHzMultiPCM = lngHzUPD7759 = lngHzOKIM6258 = lngHzOKIM6295 =
			lngHzK051649 = lngHzK054539 = lngHzHuC6280 = lngHzC140 = lngHzK053260 = lngHzPokey = lngHzQSound = lngHzSCSP =
			lngExtraOffset = lngHzWSwan = lngHzVSU = lngHzSAA1099 = lngHzES5503 = lngHzES5506 = lngHzX1_010 = lngHzC352 = lngHzGA20 = 0;
		shtPSG_Feedback = 0;
		bytPSG_SRWidth = bytPSG_Flags = bytAYType = bytAYFlag = bytAYFlagYM2203 = bytAYFlagYM2608 = bytVolumeModifier = bytReserved2 =
			bytLoopBase = bytLoopModifier = bytOKI6258Flags = bytK054539Flags = bytC140Type = bytReservedFlags = bytES5503Chns = bytES5506Chns =
			bytC352ClkDiv = bytESReserved = 0;
	}
};

class OPNSSGVOL {
	VGM_HDR_EXTRA EXHDR;
	UINT8 Chips;
	VGMX_CHIP_DATA16 Ex_Vol;
public:
	OPNSSGVOL()
	{
		EXHDR.DataSize = sizeof(VGM_HDR_EXTRA);
		EXHDR.Chp2ClkOffset = 0;
		EXHDR.ChpVolOffset = sizeof(unsigned __int32);
		Chips = 1;
		Ex_Vol.Type = 0x86;
		Ex_Vol.Flags = 0;
		Ex_Vol.Data = 0;
	}

	void SetSSGVol(unsigned __int8 percent)
	{
		Ex_Vol.Data = 0x8000 | (unsigned __int16)(256.0 * percent / 100.0 + 0.5);
	}
};

class OPNASSGVOL {
	VGM_HDR_EXTRA EXHDR;
	UINT8 Chips;
	VGMX_CHIP_DATA16 Ex_Vol;
public:
	OPNASSGVOL()
	{
		EXHDR.DataSize = sizeof(VGM_HDR_EXTRA);
		EXHDR.Chp2ClkOffset = 0;
		EXHDR.ChpVolOffset = sizeof(unsigned __int32);
		Chips = 1;
		Ex_Vol.Type = 0x87;
		Ex_Vol.Flags = 0;
		Ex_Vol.Data = 0;
	}

	void SetSSGVol(unsigned __int8 percent)
	{
		Ex_Vol.Data = 0x8000 | (unsigned __int16)(256.0 * percent / 100.0 + 0.5);
	}
};

struct VGM {
	size_t time_prev_VGM_abs = 0;
	size_t time_loop_VGM_abs = 0;
	size_t vgm_loop_pos = 0;
	std::vector<unsigned __int8> vgm_body;
	VGMHEADER vgm_header;
	unsigned __int8 command = 0;

	void make_data(unsigned __int8 address, unsigned __int8 data)
	{
		vgm_body.push_back(command);
		vgm_body.push_back(address);
		vgm_body.push_back(data);
	}

	void make_wait(size_t wait)
	{
		while (wait) {
			const size_t wait0 = 0xFFFF;
			const size_t wait1 = 882;
			const size_t wait2 = 735;
			const size_t wait3 = 16;

			if (wait >= wait0) {
				union {
					unsigned __int16 W;
					unsigned __int8 B[2];
				} u;
				this->vgm_body.push_back(0x61);
				u.W = wait0;
				this->vgm_body.push_back(u.B[0]);
				this->vgm_body.push_back(u.B[1]);
				wait -= wait0;
			}
			else if (wait == wait1 * 2 || wait == wait1 + wait2 || (wait <= wait1 + wait3 && wait >= wait1)) {
				this->vgm_body.push_back(0x63);
				wait -= wait1;
			}
			else if (wait == wait2 * 2 || (wait <= wait2 + wait3 && wait >= wait2)) {
				this->vgm_body.push_back(0x62);
				wait -= wait2;
			}
			else if (wait <= wait3 * 2 && wait >= wait3) {
				this->vgm_body.push_back(0x7F);
				wait -= wait3;
			}
			else if (wait < wait3) {
				this->vgm_body.push_back(0x70 | (wait - 1));
				wait = 0;
			}
			else {
				union {
					unsigned __int16 W;
					unsigned __int8 B[2];
				} u;
				this->vgm_body.push_back(0x61);
				u.W = wait;
				this->vgm_body.push_back(u.B[0]);
				this->vgm_body.push_back(u.B[1]);
				wait = 0;
			}
		}
	}

	wchar_t path[_MAX_PATH] = { 0 };
	void filename_replace_ext(wchar_t* outfilename, const wchar_t* newext)
	{
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];
		_wsplitpath_s(outfilename, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, newext);
	}
};

union Tone_Period { // Tone Period. 0 means note-off
	unsigned __int16 A;
	struct {
		unsigned __int8 L;
		unsigned __int8 H;
	} B;
};

struct VGM_YM2149 : public VGM {
	unsigned __int8 SSG_out = 0277;
	unsigned __int16 TPeriod[97] = { 0 };

	VGM_YM2149()
	{
		this->command = 0xA0;
		this->vgm_header.bytAYType = 0x10; // AY2149 for double clock mode.
		this->vgm_header.bytAYFlag = 0x11; // 0x10 means double clock.
	}

	virtual void Tone_set(const unsigned __int8& CH, const union Tone_Period& TP)
	{
		this->make_data(CH * 2, TP.B.L);
		this->make_data(CH * 2 + 1, TP.B.H);
	}
	virtual void Key_set(const unsigned __int8& CH, unsigned __int8 Key)
	{
		union Tone_Period TP;
		TP.A = TPeriod[Key];

		this->make_data(CH * 2, TP.B.L);
		this->make_data(CH * 2 + 1, TP.B.H);
	}
	virtual void Volume(const unsigned __int8& CH, const unsigned __int8& Vol)
	{
		this->make_data(CH + 8, Vol);
	}
	virtual void Mixer(unsigned __int8 M)
	{
		this->make_data(7, M);
	}
	virtual void Note_on(unsigned __int8 CH)
	{
		this->SSG_out &= ~(1 << CH);
		this->Mixer(this->SSG_out);
	}
	virtual void Note_off(unsigned __int8 CH)
	{
		this->SSG_out |= (1 << CH);
		this->Mixer(this->SSG_out);
	}

	virtual void Envelope_Generator_set(unsigned __int16 EP)
	{
		union {
			unsigned __int16 W;
			unsigned __int8 B[2];
		} U;

		U.W = EP;

		this->make_data(0x0C, U.B[1]);
		this->make_data(0x0B, U.B[0]);
	}

	virtual void Envelope_Generator_Type_set(unsigned __int8 CH, unsigned __int8 EG_Type) {
		this->make_data(0x0D, EG_Type);
		this->make_data(0x08 + CH, 0x10);
	}

	virtual void finish(void)
	{
		this->vgm_body.push_back(0x66);

		this->vgm_header.lngTotalSamples = this->time_prev_VGM_abs;
		this->vgm_header.lngLoopSamples = this->time_prev_VGM_abs;
		this->vgm_header.lngDataOffset = sizeof(VGMHEADER) - ((UINT8*)&this->vgm_header.lngDataOffset - (UINT8*)&this->vgm_header.fccVGM);
		this->vgm_header.lngEOFOffset = sizeof(VGMHEADER) + this->vgm_body.size() - ((UINT8*)&this->vgm_header.lngEOFOffset - (UINT8*)&this->vgm_header.fccVGM);
		this->vgm_header.lngLoopOffset = sizeof(VGMHEADER) + this->vgm_loop_pos - ((UINT8*)&this->vgm_header.lngLoopOffset - (UINT8*)&this->vgm_header.fccVGM);
	}

	virtual size_t out(wchar_t* p)
	{
		this->filename_replace_ext(p, L".vgm");
		std::ofstream outfile(path, std::ios::binary);
		if (!outfile) {
			std::wcerr << L"File " << p << L" open error." << std::endl;

			return 0;
		}

		outfile.write((const char*)&this->vgm_header, sizeof(VGMHEADER));
		outfile.write((const char*)&this->vgm_body.at(0), this->vgm_body.size());

		outfile.close();
		return sizeof(VGMHEADER) + this->vgm_body.size();
	}
};

#include "VGM_FM.hpp"

struct OPN {
	union AC_Tone Tone[3];
	struct AC_FM_PARAMETER_BYTE* preset;
};

struct VGM_YM2203 : public VGM_YM2149, public OPN {
	OPNSSGVOL ex_vgm;
	size_t Tempo = 120;
	unsigned __int16 FNumber[12] = { 0 };

	VGM_YM2203()
	{
		this->command = 0x55;
		this->vgm_header.bytAYType = 0x10;
		this->vgm_header.bytAYFlagYM2203 = 0x1;
		this->vgm_header.bytAYFlag = 0;
	}

	virtual void Timer_set_FM(void)
	{
		size_t NA = 1024 - ((((size_t)this->vgm_header.lngHzYM2203 * 2) / (192LL * this->Tempo) + 1) >> 1);
		this->make_data(0x24, (NA >> 2) & 0xFF);
		this->make_data(0x25, NA & 0x03);
	}

	virtual void Note_on_FM(unsigned __int8 CH)
	{
		union {
			struct {
				unsigned __int8 CH : 2;
				unsigned __int8 : 2;
				unsigned __int8 Op_mask : 4;
			} S;
			unsigned __int8 B;
		} U;

		U.S = { CH, this->Tone[CH].S.OPR_MASK };
		this->make_data(0x28, U.B);
	}

	virtual void Note_off_FM(unsigned __int8 CH)
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
		this->make_data(0x28, U.B);
	}

	virtual void Volume_FM(unsigned __int8 CH, unsigned __int8 Volume)
	{
		for (size_t op = 0; op < 4; op++) {
			if (this->Tone[CH].S.Connect == 7 || this->Tone[CH].S.Connect > 4 && op || this->Tone[CH].S.Connect > 3 && op >= 2 || op == 3) {
				this->make_data(0x40 + 4 * op + CH, Volume);
			}
		}
	}

	virtual void Tone_select_FM(unsigned __int8 CH, unsigned __int8 Tone) {
		static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
		this->Tone[CH].B = *(this->preset + Tone);

		this->make_data(0xB0 + CH, this->Tone[CH].B.FB_CON);

		for (size_t op = 0; op < 4; op++) {
			for (size_t j = 0; j < 6; j++) {
				if (j == 1 && (this->Tone[CH].S.Connect == 7 || this->Tone[CH].S.Connect > 4 && op || this->Tone[CH].S.Connect > 3 && op == 1 || op == 3)) {
				}
				else {
					this->make_data(0x30 + 0x10 * j + Op_index[op] + CH, *((unsigned __int8*)&this->Tone[CH].B.Op[op].DT_MULTI + j));
				}
			}
		}
	}

	void Key_set_FM(unsigned __int8 CH, unsigned __int8 Key)
	{
		union {
			struct {
				unsigned __int16 FNumber : 11;
				unsigned __int16 Block : 3;
				unsigned __int16 : 2;
			} S;
			unsigned __int8 B[2];
		} U;

		unsigned __int8 Octave = Key / 12;
		U.S.FNumber = this->FNumber[Key % 12];
		if (Octave == 8) {
			U.S.FNumber <<= 1;
			Octave = 7;
		}
		U.S.Block = Octave;

		this->make_data(0xA4 + CH, U.B[1]);
		this->make_data(0xA0 + CH, U.B[0]);
	}

	void finish(void)
	{
		this->vgm_body.push_back(0x66);

		this->vgm_header.lngTotalSamples = this->time_prev_VGM_abs;
		this->vgm_header.lngDataOffset = sizeof(VGMHEADER) + sizeof(OPNSSGVOL) - ((UINT8*)&this->vgm_header.lngDataOffset - (UINT8*)&this->vgm_header.fccVGM);
		this->vgm_header.lngExtraOffset = sizeof(VGMHEADER) - ((UINT8*)&this->vgm_header.lngExtraOffset - (UINT8*)&this->vgm_header.fccVGM);
		this->vgm_header.lngEOFOffset = sizeof(VGMHEADER) + sizeof(OPNSSGVOL) + vgm_body.size() - ((UINT8*)&this->vgm_header.lngEOFOffset - (UINT8*)&this->vgm_header.fccVGM);

		if (this->vgm_loop_pos) {
			this->vgm_header.lngLoopSamples = this->time_prev_VGM_abs - this->time_loop_VGM_abs;
			this->vgm_header.lngLoopOffset = sizeof(VGMHEADER) + sizeof(OPNSSGVOL) + this->vgm_loop_pos - ((UINT8*)&this->vgm_header.lngLoopOffset - (UINT8*)&this->vgm_header.fccVGM);
		}
	}

	size_t out(wchar_t* p)
	{
		this->filename_replace_ext(p, L".vgm");
		std::ofstream outfile(path, std::ios::binary);
		if (!outfile) {
			std::wcerr << L"File " << p << L" open error." << std::endl;

			return 0;
		}

		outfile.write((const char*)&this->vgm_header, sizeof(VGMHEADER));
		outfile.write((const char*)&this->ex_vgm, sizeof(OPNSSGVOL));
		outfile.write((const char*)&this->vgm_body.at(0), this->vgm_body.size());
		outfile.close();
		return sizeof(VGMHEADER) + this->vgm_body.size();
	}
};

struct OPNA {
	union AC_Tone Tone2[3];
	union {
		struct {
			unsigned __int8 PMS : 3;
			unsigned __int8 : 1;
			unsigned __int8 AMS : 2;
			unsigned __int8 R : 1;
			unsigned __int8 L : 1;
		} S;
		unsigned __int8 B;
	} LR_AMS_PMS[6];
};

struct VGM_YM2608 : public VGM_YM2203, public OPNA {
	OPNSSGVOL ex_vgm;
	unsigned __int8 command2 = 0;

	VGM_YM2608()
	{
		this->command = 0x56;
		this->command2 = 0x57;
		this->vgm_header.bytAYType = 0x10;
		this->vgm_header.bytAYFlagYM2203 = 0;
		this->vgm_header.bytAYFlagYM2608 = 0x1;
		this->vgm_header.bytAYFlag = 0;
		this->LR_AMS_PMS[0].B = 0;
		this->LR_AMS_PMS[1].B = 0;
		this->LR_AMS_PMS[2].B = 0;
		this->LR_AMS_PMS[3].B = 0;
		this->LR_AMS_PMS[4].B = 0;
		this->LR_AMS_PMS[5].B = 0;
	}

	void make_data2(unsigned __int8 address, unsigned __int8 data)
	{
		vgm_body.push_back(command2);
		vgm_body.push_back(address);
		vgm_body.push_back(data);
	}

	virtual void Timer_set_FM(void)
	{
		size_t NA = 1024 - ((((size_t)this->vgm_header.lngHzYM2608 * 2) / (192LL * this->Tempo) + 1) >> 1);
		this->make_data(0x24, (NA >> 2) & 0xFF);
		this->make_data(0x25, NA & 0x03);
	}

	void Panpot_set(unsigned __int8 CH, bool L, bool R)
	{
		this->LR_AMS_PMS[CH].S.L = L;
		this->LR_AMS_PMS[CH].S.R = R;

		if (CH < 3) {
			this->make_data(0xB4 + CH, this->LR_AMS_PMS[CH].B);
		}
		else if (CH < 6) {
			this->make_data2(0xB4 + CH - 3, this->LR_AMS_PMS[CH].B);
		}
	}

	virtual void Note_on_FM2(unsigned __int8 CH)
	{
		union {
			struct {
				unsigned __int8 CH : 2;
				unsigned __int8 Second : 1;
				unsigned __int8 : 1;
				unsigned __int8 Op_mask : 4;
			} S;
			unsigned __int8 B;
		} U;

		U.S = { CH, 1, this->Tone2[CH].S.OPR_MASK };
		this->make_data(0x28, U.B);
	}

	virtual void Note_off_FM2(unsigned __int8 CH)
	{
		union {
			struct {
				unsigned __int8 CH : 2;
				unsigned __int8 Second : 1;
				unsigned __int8 : 1;
				unsigned __int8 Op_mask : 4;
			} S;
			unsigned __int8 B;
		} U;

		U.S = { CH, 1, 0 };
		this->make_data(0x28, U.B);
	}

	virtual void Volume_FM2(unsigned __int8 CH, unsigned __int8 Volume)
	{
		for (size_t op = 0; op < 4; op++) {
			if (this->Tone2[CH].S.Connect == 7 || this->Tone2[CH].S.Connect > 4 && op || this->Tone2[CH].S.Connect > 3 && op >= 2 || op == 3) {
				this->make_data2(0x40 + 4 * op + CH, Volume);
			}
		}
	}

	virtual void Tone_select_FM2(unsigned __int8 CH, unsigned __int8 Tone) {
		static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
		this->Tone2[CH].B = *(this->preset + Tone);

		this->make_data2(0xB0 + CH, this->Tone2[CH].B.FB_CON);

		for (size_t op = 0; op < 4; op++) {
			for (size_t j = 0; j < 6; j++) {
				if (j == 1 && (this->Tone2[CH].S.Connect == 7 || this->Tone2[CH].S.Connect > 4 && op || this->Tone2[CH].S.Connect > 3 && op == 1 || op == 3)) {
				}
				else {
					this->make_data2(0x30 + 0x10 * j + Op_index[op] + CH, *((unsigned __int8*)&this->Tone2[CH].B.Op[op].DT_MULTI + j));
				}
			}
		}
	}

	void Key_set_FM2(unsigned __int8 CH, unsigned __int8 Key)
	{
		union {
			struct {
				unsigned __int16 FNumber : 11;
				unsigned __int16 Block : 3;
				unsigned __int16 : 2;
			} S;
			unsigned __int8 B[2];
		} U;

		unsigned __int8 Octave = Key / 12;
		U.S.FNumber = this->FNumber[Key % 12];
		if (Octave == 8) {
			U.S.FNumber <<= 1;
			Octave = 7;
		}
		U.S.Block = Octave;

		this->make_data2(0xA4 + CH, U.B[1]);
		this->make_data2(0xA0 + CH, U.B[0]);
	}

};

struct VGM_YM2612 : public VGM_YM2608 {
	VGM_YM2612()
	{
		this->command = 0x52;
		this->command2 = 0x53;
		this->vgm_header.bytAYType = 0;
		this->vgm_header.bytAYFlagYM2203 = 0;
		this->vgm_header.bytAYFlagYM2608 = 0;
		this->vgm_header.bytAYFlag = 0;
	}

	void Timer_set_FM(void)
	{
		size_t NA = 1024 - ((((size_t)this->vgm_header.lngHzYM2612 * 2) / (192LL * this->Tempo) + 1) >> 1);
		this->make_data(0x24, (NA >> 2) & 0xFF);
		this->make_data(0x25, NA & 0x03);
	}

	void Tone_set(const unsigned __int8& CH, const union Tone_Period& TP)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
	void Key_set(const unsigned __int8& CH, unsigned __int8 Key)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
	void Volume(const unsigned __int8& CH, const unsigned __int8& Vol)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
	void Mixer(unsigned __int8 M)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
	void Note_on(unsigned __int8 CH)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
	void Note_off(unsigned __int8 CH)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
	void Envelope_Generator_set(unsigned __int16 EP)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
	void Envelope_Generator_Type_set(unsigned __int8 CH, unsigned __int8 EG_Type)
	{
		std::wcerr << L"Invalid. The chip have not this feautre." << std::endl;
	}
};

struct OPM {
	union AC_Tone_OPM Tone[8];
	bool L[8];
	bool R[8];
	struct AC_FM_PARAMETER_BYTE_OPM* preset;
};

struct VGM_YM2151 : public VGM, public OPM {
	size_t Tempo = 120;

	VGM_YM2151()
	{
		this->command = 0x54;
		for (size_t i = 0; i < 8; i++) {
			this->L[i] = true;
			this->R[i] = true;
		}
	}

	void Timer_set_FM(void)
	{
		size_t NA = 1024 - (((3 * (size_t)this->vgm_header.lngHzYM2151 * 2) / (512LL * this->Tempo) + 1) >> 1);
		this->make_data(0x10, (NA >> 2) & 0xFF);
		this->make_data(0x11, NA & 0x03);
	}
	virtual void Note_on_FM(unsigned __int8 CH) {
		union {
			struct {
				unsigned __int8 CH : 3;
				unsigned __int8 Op_mask : 4;
				unsigned __int8 : 1;
			} S;
			unsigned __int8 B;
		} U;

		U.S = { CH, this->Tone[CH].S.OPR_MASK };
		this->make_data(0x08, U.B);
	}
	void Note_off_FM(unsigned __int8 CH)
	{
		union {
			struct {
				unsigned __int8 CH : 3;
				unsigned __int8 Op_mask : 4;
				unsigned __int8 : 1;
			} S;
			unsigned __int8 B;
		} U;

		U.S = { CH, 0 };
		this->make_data(0x08, U.B);
	}
	virtual void Volume_FM(unsigned __int8 CH, unsigned __int8 Volume)
	{
		for (size_t op = 0; op < 4; op++) {
			if (this->Tone[CH].S.Connect == 7 || this->Tone[CH].S.Connect > 4 && op || this->Tone[CH].S.Connect > 3 && op >= 2 || op == 3) {
				this->make_data(0x60 + 8 * op + CH, ~Volume & 0x7F);
			}
		}
	}
	virtual void Tone_select_FM(unsigned __int8 CH, unsigned __int8 Tone)
	{
		static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
		this->Tone[CH].B = this->preset[Tone - 1];
		this->Tone[CH].S.L = this->L[CH];
		this->Tone[CH].S.R = this->R[CH];

		this->make_data(0x20 + CH, this->Tone[CH].B.FB_CON);
		this->make_data(0x38 + CH, this->Tone[CH].B.PMS_AMS);
		this->make_data(0x18, this->Tone[CH].B.LFRQ);
		this->make_data(0x19, this->Tone[CH].B.PMD);
		this->make_data(0x19, this->Tone[CH].B.AMD);
		this->make_data(0x1B, this->Tone[CH].B.CT_WAVE);
		for (size_t j = 0; j < 6; j++) {
			for (size_t op = 0; op < 4; op++) {
				this->make_data(0x40 + 0x20 * j + Op_index[op] + CH, *((unsigned __int8*)&this->Tone[CH].B.Op[op].DT_MULTI + j));
			}
		}
	}
	void Key_set_FM(unsigned __int8 CH, unsigned __int8 Key)
	{
		if (Key < 3) {
			wprintf_s(L"%2u: Very low key %2u\n", CH, Key);
			Key += 12;
		}
		Key -= 3;
		union {
			struct {
				unsigned __int8 note : 4;
				unsigned __int8 oct : 3;
				unsigned __int8 : 1;
			} S;
			unsigned __int8 KC;
		} U;
		U.KC = 0;

		unsigned pre_note = Key % 12;
		U.S.note = (pre_note << 2) / 3;
		U.S.oct = Key / 12;
		this->make_data(0x28 + CH, U.KC);

		union {
			struct {
				unsigned __int8 : 2;
				unsigned __int8 Frac : 6;
			} S;
			unsigned __int8 KF;
		} V;

		V.S.Frac = 5;
		this->make_data(0x30 + CH, V.KF);
	}

	void finish(void)
	{
		this->vgm_body.push_back(0x66);

		this->vgm_header.lngTotalSamples = this->time_prev_VGM_abs;
		this->vgm_header.lngDataOffset = sizeof(VGMHEADER) - ((UINT8*)&this->vgm_header.lngDataOffset - (UINT8*)&this->vgm_header.fccVGM);
		this->vgm_header.lngEOFOffset = sizeof(VGMHEADER) + vgm_body.size() - ((UINT8*)&this->vgm_header.lngEOFOffset - (UINT8*)&this->vgm_header.fccVGM);

		if (this->vgm_loop_pos) {
			this->vgm_header.lngLoopSamples = this->time_prev_VGM_abs - this->time_loop_VGM_abs;
			this->vgm_header.lngLoopOffset = sizeof(VGMHEADER) + this->vgm_loop_pos - ((UINT8*)&this->vgm_header.lngLoopOffset - (UINT8*)&this->vgm_header.fccVGM);
		}
	}

	size_t out(wchar_t* p)
	{
		this->filename_replace_ext(p, L".vgm");
		std::ofstream outfile(path, std::ios::binary);
		if (!outfile) {
			std::wcerr << L"File " << p << L" open error." << std::endl;

			return 0;
		}

		outfile.write((const char*)&this->vgm_header, sizeof(VGMHEADER));
		outfile.write((const char*)&this->vgm_body.at(0), this->vgm_body.size());
		outfile.close();
		return sizeof(VGMHEADER) + this->vgm_body.size();
	}
};

struct VGM_YM2151_MAKO2 : public VGM_YM2151 {
	union MAKO2_Tone *M2Tones;
	union MAKO2_Tone M2_Tone[8];
	unsigned version;

	void init(union MAKO2_Tone *pM2Tone, unsigned ver)
	{
		this->M2Tones = pM2Tone;
		this->version = ver;
	}

	void Note_on_FM(unsigned __int8 CH) {
		union {
			struct {
				unsigned __int8 CH : 3;
				unsigned __int8 Op_mask : 4;
				unsigned __int8 : 1;
			} S;
			unsigned __int8 B;
		} U;

		U.S = { CH, this->M2_Tone[CH].S.OPR_MASK };
		this->make_data(0x08, U.B);
	}

	void Volume_FM(unsigned __int8 CH, unsigned __int8 Volume)
	{
		for (size_t op = 0; op < 4; op++) {
			if (this->M2_Tone[CH].S.Connect == 7 || this->M2_Tone[CH].S.Connect > 4 && op || this->M2_Tone[CH].S.Connect > 3 && op >= 2 || op == 3) {
				this->make_data(0x60 + 8 * op + CH, ~Volume & 0x7F);
			}
		}
	}

	void Tone_select_FM(unsigned __int8 CH, unsigned __int8 Tone)
	{
		static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
		this->M2_Tone[CH] = this->M2Tones[Tone];
		this->M2_Tone[CH].S.L = this->L[CH];
		this->M2_Tone[CH].S.R = this->R[CH];

		this->make_data(0x20 + CH, this->M2_Tone[CH].B.FB_CON);
		for (size_t j = 0; j < 6; j++) {
			for (size_t op = 0; op < 4; op++) {
				this->make_data(0x40 + 0x20 * j + Op_index[op] + CH, *((unsigned __int8*)&this->M2_Tone[CH].B.Op[op].DT_MULTI + j));
			}
		}
	}

	void Tone_set_SSG_emulation(unsigned __int8 CH)
	{
		static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
		this->M2_Tone[CH].B = SSG_emulation;
		this->M2_Tone[CH].S.L = this->L[CH];
		this->M2_Tone[CH].S.R = this->R[CH];

		this->make_data(0x20 + CH, this->M2_Tone[CH].B.FB_CON);
		for (size_t j = 0; j < 6; j++) {
			for (size_t op = 0; op < 4; op++) {
				this->make_data(0x40 + 0x20 * j + Op_index[op] + CH, *((unsigned __int8*)&this->M2_Tone[CH].B.Op[op].DT_MULTI + j));
			}
		}
	}

};

struct VGM_YM2203_MAKO2 : public VGM_YM2203 {
	union MAKO2_Tone M2_Tone[3];

};

struct VGM_YM2608_MAKO2 : public VGM_YM2608 {
	union MAKO2_Tone M2_Tone[3];
	union MAKO2_Tone M2_Tone2[3];

};
#pragma pack(pop)