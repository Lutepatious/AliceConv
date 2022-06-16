#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cstdlib>

#include "PCMtoWAVE.hpp"
#include "MAKO2MML.hpp"

enum class CHIP { NONE = 0, YM2203, YM2608, YM2151 };

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;

	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" [-a | -n | -x] [-d] [-s<num>] file..." << std::endl;
		exit(-1);
	}

	unsigned __int8 SSG_Volume = 0;
	enum CHIP chip_force = CHIP::NONE;
	bool early_detune = false;
	while (--argc) {
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
		enum CHIP chip = CHIP::NONE;

		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		struct mako2_header* pM2HDR = (struct mako2_header*)&inbuf.at(0);
		union PCMs* pPCM = (union PCMs*)&inbuf.at(0);

		unsigned CHs = 0;
		unsigned mako2form = 0;
		struct WAVEOUT wout;

		if (pM2HDR->chiptune_addr == 0x34UL && pM2HDR->ver == 1) {
			mako2form = pM2HDR->ver;
			CHs = 12;
		}
		else if (pM2HDR->chiptune_addr == 0x44UL && pM2HDR->ver >= 1 && pM2HDR->ver <= 3) {
			mako2form = pM2HDR->ver;
			CHs = 16;
		}
		else if (pPCM->tsnd.LoopEnd == 0 && pPCM->tsnd.Size)
		{
			// WAVE 8bitは符号なし8bit表現で、TOWNS SNDは符号1bit+7bitで-0と+0に1の差がある表現な上に中心値は-0。
			// そこでトリッキーな変換を行う
			// 負数をそのまま使うことで0x80(-0)-0xFF(-127)を0x80(128)-0xFF(255)とする。
			// 正数は反転し0x00(+0)-0x7F(127)を0x7F(127)-0x00(0)にする。
			// 自分でコード書いて忘れていたので解析する羽目になったからコメント入れた(2020.05.22)
			for (size_t i = 0; i < pPCM->tsnd.Size; i++) {
				if (!(pPCM->tsnd.body[i] & 0x80)) {
					wout.pcm8.push_back(~(pPCM->tsnd.body[i] | 0x80));
				}
				else {
					wout.pcm8.push_back(pPCM->tsnd.body[i]);
				}
			}
			wout.update((2000L * pPCM->tsnd.tSampleRate / 98 + 1) >> 1); // 0.098で割るのではなく、2000/98を掛けて+1して2で割る
		}
		else if (pPCM->mp.ID == _byteswap_ulong(0x4D506400))
		{
			for (size_t i = 0; i < pPCM->mp.Len - 0x10; i++) {
				wout.pcm8.push_back(pPCM->mp.body[i].L << 4);
				wout.pcm8.push_back(pPCM->mp.body[i].H << 4);
			}
			wout.update(100LL * pPCM->mp.sSampleRate);
		}
		else if (pPCM->pm.ID == _byteswap_ushort(0x504D))
		{
			if (pPCM->pm.Ch != 1 || pPCM->pm.BitsPerSample != 4) {
				std::wcout << L"Skip File" << std::endl;
				continue;
			}
			for (size_t i = 0; i < pPCM->pm.Size; i++) {
				wout.pcm8.push_back(pPCM->pm.body[i].L << 4);
				wout.pcm8.push_back(pPCM->pm.body[i].H << 4);
			}
			wout.update(100LL * pPCM->pm.sSampleRate);
		}
		else
		{
			for (size_t i = 0; i < inbuf.size(); i++) {
				wout.pcm8.push_back(pPCM->pcm4[i].L << 4);
				wout.pcm8.push_back(pPCM->pcm4[i].H << 4);
			}
			while (*(wout.pcm8.end() - 1) == 0) {
				wout.pcm8.pop_back();
			}

			wout.update(8000ULL);
		}
		if (wout.pcm8.size()) {
			size_t wsize = wout.out(*argv);
			std::wcout << wsize << L" bytes written." << std::endl;

			wout.pcm8.clear();
			continue;
		}
		else if (!mako2form) {
			continue;
		}

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

		wprintf_s(L"%s: %zu bytes Format %u %2u/%2u CHs.\n", *argv, inbuf.size(), mako2form, CHs_real, CHs);

		struct MML_decoded m;
		m.init(CHs_real);
		m.decode(inbuf, pM2HDR);
		if (debug) {
			m.print_delta();
		}
		m.unroll_loop();


	}
}
