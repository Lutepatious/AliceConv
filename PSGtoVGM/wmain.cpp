#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>

#include "VGM.hpp"

#pragma pack(push)
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
			size_t total_LCM = std::lcm(cn[0].whole_length, cn[1].whole_length);
			total_time_LCM = std::lcm(total_LCM, cn[2].whole_length);
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
constexpr size_t WAIT_BASE = VGM_CLOCK / MSX_VSYNC_NTSC; // must be 735

class VGMdata_YM2149 : public VGM_YM2149 {
public:
	VGMdata_YM2149(void)
	{
		this->vgm_header.lngHzAY8910 = 3579545; // doubled 1789772.5Hz
	}

	void make_init(const unsigned __int8(&V)[3]) {
		this->Mixer(0277);

		this->Tone_set(0, 0);
		this->Tone_set(1, 0);
		this->Tone_set(2, 0);
		this->Volume(0, V[0]);
		this->Volume(1, V[1]);
		this->Volume(2, V[2]);
		this->Mixer(0270);

		this->vgm_loop_pos = this->vgm_body.size();
	}
	void convert(std::vector<struct NOTE>& in, const unsigned __int8 Mag)
	{
		size_t time_prev = 0;
		for (const auto& i : in) {
			size_t wait = i.time_abs - time_prev;
			if (wait) {
				wait *= WAIT_BASE * Mag;

				time_prev = i.time_abs;
				this->time_prev_VGM_abs += wait;
				this->make_wait(wait);
			}

			if (i.TP.A == 0xFFFF) {
				break;
			}

			this->Tone_set(i.CH, i.TP.A);
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

		class PSG p;
		p.Init(inbuf);
		p.MakeEvents();


		class VGMdata_YM2149 v;
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