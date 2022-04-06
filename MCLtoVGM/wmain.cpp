#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "VGM.hpp"
#include "tools.h"

#pragma pack(push)
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
constexpr size_t WAIT_BASE = VGM_CLOCK / MSX_VSYNC_NTSC; // must be 735

class VGMdata_YM2149 : public VGM_YM2149 {
public:
	VGMdata_YM2149(void)
	{
		this->vgm_header.lngHzAY8910 = 3579545; // doubled 1789772.5Hz
	}

	void make_init(void) {
		union Tone_Period tp;
		tp.A = 0x9C9;
		this->Tone_set(0, tp);
		this->Volume(0, 0);
		tp.A = 0x9C8;
		this->Tone_set(1, tp);
		this->Volume(1, 0);
		tp.A = 0x9CA;
		this->Tone_set(2, tp);
		this->Volume(2, 0);
		this->Mixer(0277);

		this->Mixer(0270);

		this->vgm_loop_pos = this->vgm_body.size();
	}

	void convert(std::vector<struct NOTE>& in)
	{
		size_t time_prev = 0;
		for (const auto& i : in) {
			size_t wait = i.time_abs - time_prev;
			if (wait) {
				wait *= WAIT_BASE;

				time_prev = i.time_abs;
				this->time_prev_VGM_abs += wait;
				this->make_wait(wait);
			}

			if (i.TP.A == 0xFFFF) {
				break;
			}

			this->Volume(i.CH, i.volume);
			this->Tone_set(i.CH, i.TP);
		}
		this->finish();
	}
};

#pragma pack(pop)

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

		std::sort(Events.begin(), Events.end());

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