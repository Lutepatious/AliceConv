#include <vector>
#include <iostream>
#include <fstream>

#pragma pack(push)
#pragma pack(1)
#include "stdtype.h"
#include "VGMFile.h"

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

	VGM_YM2149()
	{
		this->command = 0xA0;
		this->vgm_header.bytAYType = 0x10; // AY2149 for double clock mode.
		this->vgm_header.bytAYFlag = 0x11; // 0x10 means double clock.
	}

	void Tone_set(const unsigned __int8& CH, const union Tone_Period& TP)
	{
		this->make_data(CH * 2, TP.B.L);
		this->make_data(CH * 2 + 1, TP.B.H);
	}
	void Volume(const unsigned __int8& CH, const unsigned __int8& Vol)
	{
		this->make_data(CH + 8, Vol);
	}
	void Mixer(unsigned __int8 M)
	{
		this->make_data(7, M);
	}
	void Note_on(unsigned __int8 CH)
	{
		this->SSG_out &= ~(1 << CH);
		this->Mixer(this->SSG_out);
	}
	void Note_off(unsigned __int8 CH)
	{
		this->SSG_out |= (1 << CH);
		this->Mixer(this->SSG_out);
	}

	void Envelope_Generator_set(unsigned __int16 EP)
	{
		union {
			unsigned __int16 W;
			unsigned __int8 B[2];
		} U;

		U.W = EP;

		this->make_data(0x0C, U.B[1]);
		this->make_data(0x0B, U.B[0]);
	}

	void Envelope_Generator_Type_set(unsigned __int8 CH, unsigned __int8 EG_Type) {
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

	void Timer_set_FM(void)
	{
		size_t NA = 1024 - ((((size_t) this->vgm_header.lngHzYM2203 * 2) / (192LL * this->Tempo) + 1) >> 1);
		this->make_data(0x24, (NA >> 2) & 0xFF);
		this->make_data(0x25, NA & 0x03);
	}

	void Note_on_FM(unsigned __int8 CH)
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

	void Note_off_FM(unsigned __int8 CH)
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

	void Volume_FM(unsigned __int8 CH, unsigned __int8 Volume)
	{
		for (size_t op = 0; op < 4; op++) {
			if (this->Tone[CH].S.Connect == 7 || this->Tone[CH].S.Connect > 4 && op || this->Tone[CH].S.Connect > 3 && op >= 2 || op == 3) {
				this->make_data(0x40 + 4 * op + CH, Volume);
			}
		}
	}

	void Tone_select_FM(unsigned __int8 CH, unsigned __int8 Tone) {
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

#pragma pack(pop)