#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

#include "gc_cpp.h"

#include "MAKO2toVGM.h"
#include "MAKO2MML.h"
#include "event.h"

#pragma pack (1)
struct mako2_header {
	unsigned __int32 chiptune_addr : 24;
	unsigned __int32 ver : 8;
	unsigned __int32 CH_addr[];
};

struct mako2_tone {
	union {
		struct {
			unsigned __int8 Connect : 3; // CON Connection
			unsigned __int8 FB : 3; // FL Self-FeedBack
			unsigned __int8 RL : 2; // YM2151 Only
		} S;
		unsigned __int8 B;
	} H;
	unsigned __int8 Unk[6]; // FM? or what?
	union {
		struct {
			unsigned __int8 MULTI : 4; // MUL Multiply
			unsigned __int8 DT : 3; // DT1 DeTune
			unsigned __int8 NU0 : 1; // Not used
			unsigned __int8 TL : 7; // TL Total Level
			unsigned __int8 NU1 : 1; // Not used
			unsigned __int8 AR : 5; // AR Attack Rate
			unsigned __int8 NU2 : 1; // Not used
			unsigned __int8 KS : 2; // KS Key Scale
			unsigned __int8 DR : 5; // D1R Decay Rate
			unsigned __int8 NU3 : 2; // Not used
			unsigned __int8 AMON : 1; // AMS-EN AMS On
			unsigned __int8 SR : 5; // D2R Sustain Rate
			unsigned __int8 NU4 : 1; // Not used
			unsigned __int8 DT2 : 2; // DT2
			unsigned __int8 RR : 4; // RR Release Rate
			unsigned __int8 SL : 4; // D1L Sustain Level
			unsigned __int8 Unk0[3]; // Unknown
		} S;
		unsigned __int8 B[9];
	} Op[4];
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
			continue;
		}
		FILE* pFi, * pFo;
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

		size_t VOICEs = (pM2HDR->CH_addr[0] - pM2HDR->chiptune_addr) / sizeof(struct mako2_tone);
		struct mako2_tone* T = (struct mako2_tone*)inbuf + pM2HDR->chiptune_addr;

		if (debug) {
			for (size_t i = 0; i < VOICEs; i++) {
				wprintf_s(L"Voice %2llu: FB %1d Connect %1d\n", i, (T + i)->H.S.FB, (T + i)->H.S.Connect);
				for (size_t j = 0; j < 4; j++) {
					wprintf_s(L" OP %1llu: DT %1d MULTI %2d TL %3d KS %1d AR %2d DR %2d SR %2d SL %2d RR %2d\n"
						, j, (T + i)->Op[j].S.DT, (T + i)->Op[j].S.MULTI, (T + i)->Op[j].S.TL, (T + i)->Op[j].S.KS
						, (T + i)->Op[j].S.AR, (T + i)->Op[j].S.DR, (T + i)->Op[j].S.SR, (T + i)->Op[j].S.SL, (T + i)->Op[j].S.RR);
				}
			}
		}

		unsigned CHs = (pM2HDR->chiptune_addr - 4) / 4;
		unsigned CHs_real = CHs;

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

		wprintf_s(L"%s: %zu bytes Format %u %2u/%2u CHs. %2llu Tones.\n", *argv, fs.st_size, mako2form, CHs_real, CHs, VOICEs);

		struct mako2_mml_decoded MMLs(CHs_real);
		for (size_t i = 0; i < CHs_real; i++) {
			if (!pM2HDR->CH_addr[i]) {
				(MMLs.CH + i)->mute_on();
				continue;
			}

			(MMLs.CH + i)->decode(&inbuf[pM2HDR->CH_addr[i]]);
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

		wprintf_s(L"Make VGM\n");


	}
}
