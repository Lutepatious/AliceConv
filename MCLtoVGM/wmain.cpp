#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "VGM.hpp"
#include "tools.h"

#pragma pack(1)
struct LEN_TIME {
	unsigned __int8 timeH : 5; // event time
	unsigned __int8 len : 3; // bytes per interrupt
	unsigned __int8 timeL; // event time
};

struct PSG_TP {
	unsigned __int8 H : 4; // TP high
	unsigned __int8 : 2; // not used
	unsigned __int8 key : 2; // must be 0
	unsigned __int8 L; // TP low
};

struct PSG_VOL {
	unsigned __int8 VOL : 4; // Volume
	unsigned __int8 key : 4; // must be 0x8
};

struct Event_Chain {
	unsigned __int8 len;
	unsigned __int16 Next : 14;
	unsigned __int16 key : 2; // must be 1
};

struct MSX_PSG {
	unsigned __int8 sig;
	unsigned __int16 Load_Address_Start;
	unsigned __int16 Load_Address_End; // Fllesize =  Load_Address_End - Load_Address_Start + 1 + 7
	unsigned __int16 unk;
	unsigned __int8 body[];
};

union Tone_Period { // Tone Period. 0 means note-off
	unsigned __int16 A;
	struct {
		unsigned __int8 L;
		unsigned __int8 H;
	} B;
};

constexpr size_t CHs = 3;

struct NOTE {
	size_t time_abs;
	size_t counter;
	union Tone_Period TP;
	unsigned __int8 volume;
	unsigned __int8 CH;


	bool operator < (const struct NOTE& out) {
		if (this->time_abs < out.time_abs) {
			return true;
		}
		else if (this->time_abs > out.time_abs) {
			return false;
		}
		else if (this->CH < out.CH) {
			return true;
		}
		else if (this->CH > out.CH) {
			return false;
		}
		else if (this->counter < out.counter) {
			return true;
		}
		else {
			return false;
		}
	}
};

constexpr size_t MSX_VSYNC_NTSC = 60;  // Hz
constexpr size_t VGM_CLOCK = 44100; // Hz
constexpr size_t WAIT_BASE = VGM_CLOCK / MSX_VSYNC_NTSC; // must be 735

class VGMdata_YM2149 {
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
	VGMHEADER vgm_header;
public:
	VGMdata_YM2149(void)
	{
		this->R7.A = 0277;
		this->vgm_header.lngHzAY8910 = 3579545; // doubled 1789772.5Hz
		this->vgm_header.bytAYType = 0x10; // AY2149 for double clock mode.
		this->vgm_header.bytAYFlag = 0x11; // 0x10 means double clock.
	}

	void make_data(unsigned __int8 address, unsigned __int8 data)
	{
		vgm_body.push_back(0xA0);
		vgm_body.push_back(address);
		vgm_body.push_back(data);
	}
	void Tone_Set(const unsigned __int8& CH, const union Tone_Period& TP)
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
		this->R7.A = M;
		this->make_data(7, this->R7.A);
	}

	void make_init(void) {
		union Tone_Period tp;
		tp.A = 0x9C9;
		this->Tone_Set(0, tp);
		this->Volume(0, 0);
		tp.A = 0x9C8;
		this->Tone_Set(1, tp);
		this->Volume(1, 0);
		tp.A = 0x9CA;
		this->Tone_Set(2, tp);
		this->Volume(2, 0);
		this->Mixer(0277);

		this->Mixer(0270);

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

	void convert(std::vector<struct NOTE>& in)
	{
		size_t time_prev = 0;
		size_t time_prev_VGM_abs = 0;
		for (const auto& i : in) {
			size_t wait = i.time_abs - time_prev;
			if (wait) {
				wait *= WAIT_BASE;

				time_prev = i.time_abs;
				time_prev_VGM_abs += wait;
				this->make_wait(wait);
			}

			if (i.TP.A == 0xFFFF) {
				break;
			}

			this->Volume(i.CH, i.volume);
			this->Tone_Set(i.CH, i.TP);
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

		std::vector<struct NOTE> Events;
		struct Event_Chain* Ch[CHs];
		Ch[0] = (struct Event_Chain*)in->body;
		Ch[1] = (struct Event_Chain*)((__int8*)(Ch[0] + 1) + Ch[0]->len);
		Ch[2] = (struct Event_Chain*)((__int8*)(Ch[1] + 1) + Ch[1]->len);

		bool in_events = true;
		size_t time_total = 0;
		size_t Counter = 0;
		while (in_events) {
			size_t time_total_Ch[3];
			for (size_t i = 0; i < CHs; i++) {
				time_total_Ch[i] = time_total;

				size_t Chlen = Ch[i]->len - 2;
				struct LEN_TIME* lw = (struct LEN_TIME*)((unsigned __int8*)(Ch[i] + 1) + 1);

				while (Chlen) {
					size_t time = ((size_t)lw->timeH << 8) + (size_t)lw->timeL;
					size_t len_tmp = lw->len;
					struct NOTE n;
					n.counter = Counter++;
					n.CH = i;
					n.TP.A = 0;
					n.volume = 0;
					n.time_abs = time_total_Ch[i];
					time_total_Ch[i] += time;

					__int8* event = (__int8*)(lw + 1);
					while (len_tmp) {
						struct PSG_VOL* pVOL;
						struct PSG_TP* pTP;
						switch (*event & 0xF0) {
						case 0x80:
							pVOL = (struct PSG_VOL*)event;
							n.volume = pVOL->VOL;

							event++;
							len_tmp--;
							break;
						case 0x00:
							pTP = (struct PSG_TP*)event;
							n.TP.B.H = pTP->H;
							n.TP.B.L = pTP->L;

							event += 2;
							len_tmp -= 2;
							break;
						default:
							std::wcout << L"not yet implement." << std::endl;
						}
					}
					Events.push_back(n);

					Chlen -= sizeof(struct LEN_TIME);
					Chlen -= lw->len;
					lw = (struct LEN_TIME*)((unsigned __int8*)(lw + 1) + lw->len);
				}

				struct NOTE n;
				n.counter = Counter++;
				n.CH = i;
				n.TP.A = 0;
				n.volume = 0;
				n.time_abs = time_total_Ch[i];
				Events.push_back(n);

			}
			if (!Ch[0]->key && !Ch[1]->key && !Ch[2]->key) {
				in_events = false;
			}

			for (size_t i = 0; i < CHs; i++) {
				if (time_total_Ch[i] > time_total) {
					time_total = time_total_Ch[i];
				}
				Ch[i] = (struct Event_Chain*)&inbuf.at(Ch[i]->Next);
			}
		}


#if 0
		for (size_t i = 0; i < CHs; i++) {
			size_t time_total = 0;
			bool in_events = true;
			while (in_events) {
//				std::wcout << std::dec << L"Ch." << i + 1 << L". " << Ch[i]->len << L"bytes. Next " << std::hex << Ch[i]->Next << std::endl;

				size_t Chlen = Ch[i]->len - 2;
				struct LEN_TIME* lw = (struct LEN_TIME*)((unsigned __int8*)(Ch[i] + 1) + 1);
				while (Chlen) {
					size_t time = ((size_t)lw->timeH << 8) + (size_t)lw->timeL;
					size_t len_tmp = lw->len;
					struct NOTE n;
					n.CH = i;
					n.TP.A = 0;
					n.volume = 0;
					n.time_abs = time_total;
					time_total += time;

					__int8* event = (__int8*)(lw + 1);
					while (len_tmp) {
						struct PSG_VOL* pVOL;
						struct PSG_TP* pTP;
						switch (*event & 0xF0) {
						case 0x80:
							pVOL = (struct PSG_VOL*)event;
							n.volume = pVOL->VOL;

							event++;
							len_tmp--;
							break;
						case 0x00:
							pTP = (struct PSG_TP*)event;
							n.TP.B.H = pTP->H;
							n.TP.B.L = pTP->L;

							event += 2;
							len_tmp -= 2;
							break;
						default:
							std::wcout << L"not yet implement." << std::endl;
						}
					}
					Events.push_back(n);

					Chlen -= sizeof(struct LEN_TIME);
					Chlen -= lw->len;
					lw = (struct LEN_TIME*)((unsigned __int8*)(lw + 1) + lw->len);
				}

				if (!Ch[i]->key) {
					in_events = false;
				}

				Ch[i] = (struct Event_Chain*)&inbuf.at(Ch[i]->Next);
			}
		}
#endif

		std::sort(Events.begin(), Events.end());

#if 0
		for (size_t i = 0; i < Events.size(); i++) {
			std::wcout << std::dec << Events.at(i).time_abs << L". " << Events.at(i).CH << L":" << std::hex << Events.at(i).TP.A << L" " << Events.at(i).volume << std::endl;
		}

#endif
		class VGMdata_YM2149 v;
		v.make_init();
		v.convert(Events);

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