#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "VGM.hpp"
#include "tools.h"

#pragma pack(1)
struct MSX_PSG {
	unsigned __int8 sig;
	unsigned __int16 Load_Address_Start;
	unsigned __int16 Load_Address_End; // Fllesize =  Load_Address_End - Load_Address_Start + 1 + 7
	unsigned __int16 unk;
	unsigned __int16 offs[3]; // +7 for absolute.
	unsigned __int8 Volume[3];
	unsigned __int8 WaitMagnitude;
	unsigned __int8 body[];
};

union Tone_Period { // Tone Period. 0 means note-off
	unsigned __int16 A;
	struct {
		unsigned __int8 L;
		unsigned __int8 H;
	} B;
};

struct PSG_NOTE {
	unsigned __int8 length;
	union Tone_Period TP;
};

struct CH_NOTES {
	struct PSG_NOTE* Notes;
	unsigned __int8 Volume;
	size_t whole_length;

	CH_NOTES() {
		this->whole_length = 0;
		this->Notes = NULL;
		this->Volume = 0;
	};

	void Init(struct PSG_NOTE* n, unsigned __int8 v) {
		this->whole_length = 0;
		this->Notes = n;
		this->Volume = v;

		struct PSG_NOTE* pos = this->Notes;
		while (pos->length != 0xff) {
			this->whole_length += pos->length;
			pos++;
		}
	};

};

constexpr size_t CHs = 3;

struct NOTE {
	size_t time_abs;
	union Tone_Period TP;
	unsigned __int8 CH;
};

bool operator < (const struct NOTE& a, const struct NOTE& b) {
	if (a.time_abs < b.time_abs) {
		return true;
	}
	else if (a.time_abs > b.time_abs) {
		return false;
	}
	else if (a.CH < b.CH) {
		return true;
	}
	else {
		return false;
	}
}

class PSG {
	size_t total_time_LCM = 0;
	struct CH_NOTES cn[CHs];

public:
	std::vector<struct NOTE> Events;
	void Init(std::vector<__int8>& buf) {
		struct MSX_PSG* in = (MSX_PSG*)&buf.at(0);

		for (size_t i = 0; i < CHs; i++) {
			cn[i].Init((struct PSG_NOTE*)&buf[in->offs[i] + 7LL], in->Volume[i]);
		}

		if (cn[0].whole_length != cn[1].whole_length || cn[0].whole_length != cn[2].whole_length) {
			size_t total_LCM = LCM(cn[0].whole_length, cn[1].whole_length);
			total_time_LCM = LCM(total_LCM, cn[2].whole_length);
		}
		else {
			total_time_LCM = cn[0].whole_length;
		}
		std::wcout << L"Whole loop length " << total_time_LCM << std::endl;
	};

	void MakeEvents(void) {
		Events.empty();
		for (size_t ch = 0; ch < CHs; ch++) {
			size_t total_length = 0;
			for (size_t j = 0; j < this->total_time_LCM / cn[ch].whole_length; j++) {
				while (cn[ch].Notes->length != 0xFF) {
					struct NOTE n;
					n.time_abs = total_length;
					n.TP = cn[ch].Notes->TP;
					n.CH = ch;
					total_length += cn[ch].Notes->length;
					cn[ch].Notes++;
					Events.push_back(n);
				}
				struct NOTE end;
				end.time_abs = total_length;
				end.TP.A = 0xFFFF;
				end.CH = ch;
				Events.push_back(end);
			}
		}

		std::sort(Events.begin(), Events.end());

		//		for (auto& i : Events) {
		//			std::cout << std::dec << i.time_abs << ":" << (size_t)i.CH << " " << std::hex << i.TP.A << std::endl;
		//		}
	};
};

constexpr size_t MSX_VSYNC_NTSC = 60;  // Hz
constexpr size_t VGM_CLOCK = 44100; // Hz
constexpr size_t WAIT_BASE = VGM_CLOCK / MSX_VSYNC_NTSC; // must be 735

class PSGVGM {
	size_t vgm_loop_pos = 0;
	union {
		struct {
			unsigned __int8 Tone_A : 1;
			unsigned __int8 Tone_B : 1;
			unsigned __int8 Tone_C : 1;
			unsigned __int8 Noise_A : 1;
			unsigned __int8 Noise_B : 1;
			unsigned __int8 Noise_C : 1;
			unsigned __int8 IO_A : 1;
			unsigned __int8 IO_B : 1;
		} B;
		unsigned __int8 A;
	} R7;
	std::vector<unsigned __int8> vgm_body;
	VGM_HEADER vgm_header;
public:
	PSGVGM(void)
	{
		this->R7.A = 0277;
		this->vgm_header.lngHzAY8910 = 3579545; // doubled 1789772.5Hz
		this->vgm_header.bytAYType = 0x10; // AY2149 for double clock mode.
		this->vgm_header.bytAYFlag = 0x11; // 0x10 means double clock.
	}

	void make_data(unsigned __int8 command, unsigned __int8 address, unsigned __int8 data)
	{
	}
	void make_data_AY8910(unsigned __int8 address, unsigned __int8 data)
	{
		vgm_body.push_back(0xA0);
		vgm_body.push_back(address);
		vgm_body.push_back(data);
	}
	void Tone_Set_AY8910(const unsigned __int8& CH, const union Tone_Period& TP)
	{
		this->make_data_AY8910(CH * 2, TP.B.L);
		this->make_data_AY8910(CH * 2 + 1, TP.B.H);
	}
	void Volume_AY8910(const unsigned __int8& CH, const unsigned __int8& Vol)
	{
		this->make_data_AY8910(CH + 8, Vol);
	}
	void Mixer_AY8910(unsigned __int8 M)
	{
		this->R7.A = M;
		this->make_data_AY8910(7, this->R7.A);
	}

	void make_init(const unsigned __int8(&V)[3]) {
		union Tone_Period tp;
		tp.A = 0x9C9;
		this->Tone_Set_AY8910(0, tp);
		this->Volume_AY8910(0, 0);
		tp.A = 0x9C8;
		this->Tone_Set_AY8910(1, tp);
		this->Volume_AY8910(1, 0);
		tp.A = 0x9CA;
		this->Tone_Set_AY8910(2, tp);
		this->Volume_AY8910(2, 0);
		this->Mixer_AY8910(0277);

		tp.A = 0;
		this->Tone_Set_AY8910(0, tp);
		this->Tone_Set_AY8910(1, tp);
		this->Tone_Set_AY8910(2, tp);
		this->Volume_AY8910(0, V[0]);
		this->Volume_AY8910(1, V[1]);
		this->Volume_AY8910(2, V[2]);
		this->Mixer_AY8910(0270);

		this->vgm_loop_pos = this->vgm_body.size();
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

	void convert(std::vector<struct NOTE>& in, const unsigned __int8 Mag)
	{
		size_t time_prev = 0;
		size_t time_prev_VGM_abs = 0;
		for (const auto& i : in) {
			size_t wait = i.time_abs - time_prev;
			if (wait) {
				wait *= WAIT_BASE * Mag;

				time_prev = i.time_abs;
				time_prev_VGM_abs += wait;
				this->make_wait(wait);
			}

			if (i.TP.A == 0xFFFF) {
				break;
			}

			this->Tone_Set_AY8910(i.CH, i.TP);
		}
		this->vgm_body.push_back(0x66);

		this->vgm_header.lngTotalSamples = time_prev_VGM_abs;
		this->vgm_header.lngLoopSamples = time_prev_VGM_abs;
		this->vgm_header.lngDataOffset = sizeof(VGM_HEADER) - ((UINT8*)&this->vgm_header.lngDataOffset - (UINT8*)&this->vgm_header.fccVGM);
		this->vgm_header.lngEOFOffset = sizeof(VGM_HEADER) + this->vgm_body.size() - ((UINT8*)&this->vgm_header.lngEOFOffset - (UINT8*)&this->vgm_header.fccVGM);
		this->vgm_header.lngLoopOffset = sizeof(VGM_HEADER) + this->vgm_loop_pos - ((UINT8*)&this->vgm_header.lngLoopOffset - (UINT8*)&this->vgm_header.fccVGM);

	}

	size_t out(wchar_t* p)
	{
		wchar_t* outpath = filename_replace_ext(p, L".vgm");
		std::ofstream outfile(outpath, std::ios::binary);
		if (!outfile) {
			std::wcerr << L"File " << p << L" open error." << std::endl;

			return 0;
		}

		outfile.write((const char*)&this->vgm_header, sizeof(VGM_HEADER));
		outfile.write((const char*)&this->vgm_body.at(0), this->vgm_body.size());

		outfile.close();
		return sizeof(VGM_HEADER) + this->vgm_body.size();
	}
};


#pragma pack()


int wmain(int argc, wchar_t** argv)
{
	bool debug = false;
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	while (--argc) {
		std::ifstream infile(*++argv, std::ios::binary);

		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		struct MSX_PSG* in = (MSX_PSG*)&inbuf.at(0);

		if (in->sig != 0xFE) {
			std::wcerr << L"Wrong format" << *argv << L". ignore." << std::endl;

			continue;
		}
		std::wcout << L"File " << *argv << L" size " << in->Load_Address_End - in->Load_Address_Start + 1 + 7 << L" bytes." << std::endl;

		class PSG p;
		p.Init(inbuf);
		p.MakeEvents();


		class PSGVGM v;
		v.make_init(in->Volume);
		v.convert(p.Events, in->WaitMagnitude);

		inbuf.empty();

		size_t outsize = v.out(*argv);
		if (outsize == 0) {
			std::wcerr << L"File output failed." << std::endl;

			continue;
		}
		else {
			std::wcout << outsize << L" bytes written." << std::endl;
		}
	}

	return 0;
}