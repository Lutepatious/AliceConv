#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

#include "gc_cpp.h"

#include "MAKO1toVGM.h"
#include "MAKO1MML.h"
#include "event1.h"
#include "toVGM1.h"

#pragma pack (1)
struct mako1_header {
	unsigned __int32 sig; // must be 0x0000A000
	struct {
		unsigned __int16 Address;
		unsigned __int16 Length;
	} CH[6];
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	enum Machine M_arch = Machine::PC9801;
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'v') {
				M_arch = Machine::PC88VA;
			}
			else if (*(*argv + 1) == L't') {
				M_arch = Machine::FMTOWNS;
			}
			else if (*(*argv + 1) == L'8') {
				M_arch = Machine::PC8801;
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

		struct mako1_header* pM1HDR = (struct mako1_header*)inbuf;

		unsigned mako1form = 0;

		if (pM1HDR->sig == 0xA000U
			&& pM1HDR->CH[5].Address + pM1HDR->CH[5].Length < fs.st_size
			&& pM1HDR->CH[0].Address + pM1HDR->CH[0].Length == pM1HDR->CH[1].Address
			&& pM1HDR->CH[1].Address + pM1HDR->CH[1].Length == pM1HDR->CH[2].Address
			&& pM1HDR->CH[2].Address + pM1HDR->CH[2].Length == pM1HDR->CH[3].Address
			&& pM1HDR->CH[3].Address + pM1HDR->CH[3].Length == pM1HDR->CH[4].Address
			&& pM1HDR->CH[4].Address + pM1HDR->CH[4].Length == pM1HDR->CH[5].Address
			&& inbuf[pM1HDR->CH[1].Address - 1] == 0xFF
			&& inbuf[pM1HDR->CH[2].Address - 1] == 0xFF
			&& inbuf[pM1HDR->CH[3].Address - 1] == 0xFF
			&& inbuf[pM1HDR->CH[4].Address - 1] == 0xFF
			&& inbuf[pM1HDR->CH[5].Address - 1] == 0xFF
			&& inbuf[pM1HDR->CH[5].Address + pM1HDR->CH[5].Length - 1] == 0xFF) {
			mako1form = 1;
		}
		if (!mako1form) {
			continue;
		}
		wprintf_s(L"%s is MAKO1 format. %u bytes\n", *argv, pM1HDR->CH[5].Address + pM1HDR->CH[5].Length);

		struct mako1_mml_decoded MMLs;
		for (size_t i = 0; i < 6; i++) {
			(MMLs.CH + i)->decode(inbuf, pM1HDR->CH[i].Address);
		}
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
		class EVENTS events(MMLs.end_time * 2, MMLs.end_time);
		events.convert(MMLs);
		events.sort();
		if (debug) {
			events.print_all();
		}

		class VGMdata1 vgmdata(MMLs.end_time, M_arch);
		vgmdata.make_init();
		vgmdata.convert(events);
		vgmdata.out(*argv);
	}
}
