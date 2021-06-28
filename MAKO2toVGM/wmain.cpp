#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

#include "gc_cpp.h"

#include "MAKO2toVGM.h"
#include "PCMtoWAVE.h"
#include "MAKO2MML.h"
#include "event.h"
#include "toVGM.h"
#include "tools.h"

#pragma pack (1)
struct mako2_header {
	unsigned __int32 chiptune_addr : 24;
	unsigned __int32 ver : 8;
	unsigned __int32 CH_addr[];
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	enum CHIP chip_force = CHIP::NONE;
	bool early_detune = false;
	while (--argc) {
		enum CHIP chip = CHIP::NONE;
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'a') {
				chip_force = CHIP::YM2608;
			}
			else if (*(*argv + 1) == L'n') {
				chip_force = CHIP::YM2203;
			}
			else if (*(*argv + 1) == L'x') {
				chip_force = CHIP::YM2151;
			}
			else if (*(*argv + 1) == L'd') {
				early_detune = true;
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

		struct mako2_header* pM2HDR = (struct mako2_header*)inbuf;

		unsigned mako2form = 0;
		if (pM2HDR->ver == 1 && pM2HDR->chiptune_addr == 0x34UL) {
			mako2form = pM2HDR->ver;
		}
		if (pM2HDR->ver == 2 && pM2HDR->chiptune_addr == 0x44UL) {
			chip = CHIP::YM2608;
			mako2form = pM2HDR->ver;
		}
		if (pM2HDR->ver == 3 && pM2HDR->chiptune_addr == 0x44UL) {
			chip = CHIP::YM2608;
			mako2form = pM2HDR->ver;
		}

		// MAKO2フォーマットでないならPCMであると仮定する
		if (!mako2form) {
			PCMtoWAV(inbuf, *argv, fs.st_size);
			continue;
		}

		unsigned CHs = (pM2HDR->chiptune_addr - 4) / 4;
		unsigned CHs_real = CHs;

		// 例:Ch.8が空でないときにCHs_realは9を維持していなければならない。よって[CHs_real - 1]を[--CHs_real]と書き換えてはならない。
		while (!pM2HDR->CH_addr[CHs_real - 1] && --CHs_real);
		if (!CHs_real) {
			wprintf_s(L"No Data. skip.\n");
			continue;
		}

		if (chip_force != CHIP::NONE) {
			chip = chip_force;
		}
		else if (chip == CHIP::NONE) {
			if (CHs_real == 6) {
				chip = CHIP::YM2203;
			}
			else if (CHs_real == 9) {
				chip = CHIP::YM2608;
			}
			else if (CHs_real == 8) {
				chip = CHIP::YM2151;
			}
		}

		wprintf_s(L"%s: %zu bytes Format %u %2u/%2u CHs.\n", *argv, fs.st_size, mako2form, CHs_real, CHs);

		struct mako2_mml_decoded MMLs(CHs_real);
		for (size_t i = 0; i < CHs_real; i++) {
			(MMLs.CH + i)->decode(inbuf, pM2HDR->CH_addr[i]);
		}

		MMLs.unroll_loop();

		if (!MMLs.end_time) {
			wprintf_s(L"No Data. skip.\n");
			continue;
		}

		wprintf_s(L"Make Sequential events\n");
		// 得られた展開データからイベント列を作る。

		class EVENTS events(MMLs.end_time * 2, MMLs.end_time);
		events.convert(MMLs, chip == CHIP::YM2151);
		events.sort();
		if (debug) {
			events.print_all();
		}

		wprintf_s(L"Make VGM\n");
		class VGMdata vgmdata(MMLs.end_time, chip, mako2form, (union MAKO2_Tone*)(inbuf + pM2HDR->chiptune_addr), (pM2HDR->CH_addr[0] - pM2HDR->chiptune_addr) / sizeof(struct mako2_tone), early_detune);
		vgmdata.check_all_tones_blank();
		vgmdata.make_init();
		vgmdata.convert(events);
		vgmdata.out(*argv);
	}
}
