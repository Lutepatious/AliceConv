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

		unsigned __int8* inbuf = (unsigned __int8*)GC_malloc(fs.st_size);
		if (inbuf == NULL) {
			wprintf_s(L"Memory allocation error. \n");
			fclose(pFi);
			exit(-2);
		}

		size_t rcount = fread_s(inbuf, fs.st_size, fs.st_size, 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %zd.\n", rcount);
			fclose(pFi);
			exit(-2);
		}

		fclose(pFi);

		unsigned __int8* mpos = (unsigned __int8*)memchr(inbuf, '\xFF', fs.st_size);
		size_t len_header = mpos - inbuf;
		struct eomml_decoded MMLs(inbuf, fs.st_size);

		MMLs.decode();

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
