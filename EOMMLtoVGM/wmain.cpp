#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <cctype>
#include <cstring>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

#include "gc_cpp.h"

#include "EOMMLtoVGM.h"
#include "EOMML.h"
#include "event_e.h"
#include "toVGM_e.h"

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	unsigned __int8 SSG_Volume = 0;
	enum Machine M_arch = Machine::X68000;
	bool Tones_tousin = false;
	bool Old98_Octave = false;
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'8') {
				M_arch = Machine::PC8801;
			}
			else if (*(*argv + 1) == L'9') {
				M_arch = Machine::PC9801;
			}
			else if (*(*argv + 1) == L'T') {
				M_arch = Machine::X68000;
				Tones_tousin = true;
			}
			else if (*(*argv + 1) == L's') {
				int tVol = _wtoi(*argv + 2);
				if (tVol > 255) {
					SSG_Volume = 255;
				}
				else if (tVol < 0) {
					SSG_Volume = 0;
				}
				else {
					SSG_Volume = tVol;
				}
			}
			continue;
		}

		FILE* pFi;
		errno_t ecode = _wfopen_s(&pFi, *argv, L"rb");
		if (ecode || !pFi) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		std::vector<unsigned __int8> inbuf(fs.st_size);

		size_t rcount = fread_s(&inbuf[0], fs.st_size, fs.st_size, 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %zd.\n", rcount);
			fclose(pFi);
			exit(-2);
		}

		fclose(pFi);

		std::vector<unsigned __int8> buffer;
		std::vector<unsigned __int8>* in = NULL;
		if (inbuf[0] == '"') {
			M_arch = Machine::PC9801;
			Old98_Octave = true;
			std::string instr((char *) &inbuf[0]);
			std::stringstream instr_s(instr);
			std::string line;
			bool header = true;
			while (std::getline(instr_s, line)) {
//				std::cout << line << std::endl;
				line.erase(line.end() - 1); // remove CR
				const std::string eomac("\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\"");
				const std::string domac("\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[domac]\",\"[eomac]\",\"[eomac]\"");
				if (line == eomac  || line == domac) {
					header = false;
//					std::cout << "Header end." << std::endl;
					buffer.push_back(0xff);
				}
				else if (header) {
					std::string col;
					std::stringstream line_s(line);
					while (std::getline(line_s, col, ',')) {
						unsigned long v = std::stoul(col.substr(1, col.size() - 2));
//						std::cout << v << std::endl;
						buffer.push_back((unsigned __int8)v);
					}
				}
				else if (line.size() > 1) {
					buffer.insert(buffer.end(), line.begin() + 1, line.end() - 1);
					buffer.push_back(0x0d);
				}
				in = &buffer;
			}

#if 0
			for (size_t i = 0; i < buffer.size(); i++) {
				printf_s("%02X ", buffer[i]);
			}
#endif
		}
		else if (inbuf[0] != 0x00) {
			continue;
		}
		else {
			in = &inbuf;
		}

		struct eomml_decoded MMLs(&(*in)[0], in->size(), Tones_tousin, Old98_Octave);

		MMLs.decode();

		buffer.empty();
		inbuf.empty();

		if (debug) {
			MMLs.print();
		}

		MMLs.unroll_loop();

		if (!MMLs.end_time) {
			wprintf_s(L"No Data. skip.\n");
			continue;
		}

		wprintf_s(L"Make Sequential events\n");
		// 得られた展開データからイベント列を作る。
		class EVENTS events(MMLs.end_time * 2, MMLs.end_time, M_arch);
		events.convert(MMLs);
		events.sort();
		if (debug) {
			events.print_all();
		}

		class VGMdata_e vgmdata(MMLs.end_time, M_arch, Tones_tousin);
		vgmdata.make_init();
		vgmdata.convert(events);
		if (SSG_Volume) {
			vgmdata.SetSSGVol(SSG_Volume);
		}
		vgmdata.out(*argv);
	}
}
