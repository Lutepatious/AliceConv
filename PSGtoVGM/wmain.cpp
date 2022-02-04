#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>

#include "tools.h"

#pragma pack(1)
struct MSX_PSG {
	unsigned __int8 sig;
	unsigned __int16 Load_Address_Start;
	unsigned __int16 Load_Address_End; // Fllesize =  Load_Address_End - Load_Address_Start + 1 + 7
	unsigned __int16 unk;
	unsigned __int16 offs[3]; // +7 for absolute.
	unsigned __int8 Volume[3];
	unsigned __int8 unk1;
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
	std::vector<struct NOTE> Events;

public:
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
					do {
						total_length += cn[ch].Notes->length;
						cn[ch].Notes++;
					} while (n.TP.A == cn[ch].Notes->TP.A);
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

		for (auto& i : Events) {
			std::cout << std::dec << i.time_abs << ":" << (size_t)i.CH << " " << std::hex << i.TP.A << std::endl;
		}
	};
};

#pragma pack()

constexpr double MSX_PSG_CLOCK_MASTER = 1789772.5; // 1.7897725 MHz
constexpr double MSX_PSG_CLOCK_BASE = MSX_PSG_CLOCK_MASTER / 16.0; // 111860.78125 Hz

constexpr size_t MSX_VSYNC_NTSC = 60;  // Hz
constexpr size_t VGM_CLOCK = 44100; // Hz

constexpr size_t WAIT_BASE = VGM_CLOCK * 4 / MSX_VSYNC_NTSC;

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	while (--argc) {
		struct __stat64 fs;
		if (_wstat64(*++argv, &fs) == -1) {
			std::wcerr << L"File " << *argv << L" is not exist." << std::endl;

			continue;
		}

		std::ifstream infile(*argv, std::ios::binary);

		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf(fs.st_size);
		infile.read(&inbuf.at(0), inbuf.size());

		infile.close();

		struct MSX_PSG* in = (MSX_PSG*)&inbuf.at(0);

		if (in->sig != 0xFE) {
			std::wcerr << L"Wrong format" << *argv << L". ignore." << std::endl;

			continue;
		}
		std::wcout << L"File " << *argv << L" size " << in->Load_Address_End - in->Load_Address_Start + 1 + 7 << L"/" << fs.st_size << L" bytes." << std::endl;

		class PSG p;
		p.Init(inbuf);
		p.MakeEvents();
	}
}