#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "stdtype.h"
#include "VGMFile.h"

#include "tools.h"

#pragma pack(1)
constexpr size_t CHs = 6;

struct PC88_MML_HEADER {
	unsigned __int16 Load_Address_Start;
	unsigned __int16 Load_Address_End; // Fllesize =  Load_Address_End - Load_Address_Start + 1 + 3
	unsigned __int16 CH_Address[CHs];
};



#pragma pack()

int wmain(int argc, wchar_t** argv)
{
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

		struct PC88_MML_HEADER* h = (struct PC88_MML_HEADER*)&inbuf.at(0);
		std::wcout << h->Load_Address_End - h->Load_Address_Start + 4 << "/" << inbuf.size() << std::endl;

		for (size_t ch = 0; ch < CHs; ch++) {
			std::wcout << std::hex << h->CH_Address[ch] - h->CH_Address[0] + 0x10 << std::endl;

		}

		unsigned __int16 *a = (unsigned __int16*)&inbuf.at(16);
		int count = 0;
		while (count < 6) {
			if (*a != 0) {
				std::wcout << std::hex << *a - h->CH_Address[0] + 0x10 << std::endl;
			}

			if (*a == 0) {
				count++;
			}
			a++;
		}

	}
	return 0;
}