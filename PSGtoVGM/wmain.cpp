#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>

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

struct PSG_NOTE {
	unsigned __int8 length;
	union Tone_Period { // Tone Period. 0 means note-off
		unsigned __int16 A;
		struct {
			unsigned __int8 L;
			unsigned __int8 H;
		} B;
	} TP;
};

class CH_NOTES {
	struct PSG_NOTE* Notes;
	unsigned __int8 Volume;

public:
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


#pragma pack()

constexpr double MSX_PSG_CLOCK_MASTER = 1789772.5; // 1.7897725 MHz
constexpr double MSX_PSG_CLOCK_BASE = MSX_PSG_CLOCK_MASTER / 16.0; // 111860.78125 Hz

constexpr double MSX_VSYNC_NTSC = 60.0 / 1.001;  // Hz
constexpr size_t VGM_CLOCK = 44100ULL; // Hz

constexpr size_t WAIT_BASE = 44100 * 4 * 1001 / 60000; // VGM_CLOCK * 4 / MSX_VSYNC_NTSC 

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

		struct MSX_PSG* in = (MSX_PSG *) &inbuf.at(0);

		if (in->sig != 0xFE) {
			std::wcerr << L"Wrong format" << *argv << L". ignore." << std::endl;

			continue;
		}
		std::wcout << L"File " << *argv << L" size " << in->Load_Address_End - in->Load_Address_Start + 1 + 7 << L"/" << fs.st_size << L" bytes." << std::endl;

		constexpr size_t CHs = 3;

		class CH_NOTES cn[CHs];
		for (size_t i = 0; i < CHs; i++) {
			cn[i].Init((struct PSG_NOTE*)&inbuf[in->offs[i] + 7LL], in->Volume[i]);
		}

		if (cn[0].whole_length != cn[1].whole_length || cn[0].whole_length != cn[2].whole_length) {
			std::wcout << L"Channel length not match." << std::endl;
		}
	}
}