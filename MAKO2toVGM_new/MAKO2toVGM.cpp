#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <limits>

#define MAKO2
constexpr size_t TIME_MUL = 3ULL;

#include "PCMtoWAVE.hpp"
#include "MAKO2MML.hpp"
#include "Events.hpp"
#include "VGM.hpp"

constexpr size_t MASTERCLOCK_NEC_OPN = 3993600;
constexpr size_t MASTERCLOCK_NEC_OPNA = MASTERCLOCK_NEC_OPN * 2;
constexpr size_t MASTERCLOCK_SHARP_OPM = 4000000;

class VGMdata_YM2151 : public VGM_YM2151_MAKO2 {
public:
	VGMdata_YM2151(void)
	{
		this->vgm_header.lngHzYM2151 = MASTERCLOCK_SHARP_OPM;
		this->preset = NULL;
	}

	void make_init(void)
	{
		this->make_data(0x01, 0x00);
		this->make_data(0x14, 0x00);
		this->make_data(0x10, 0x64);
		this->make_data(0x11, 0x00);
		this->Tone_set_SSG_emulation(3);
		this->Tone_set_SSG_emulation(4);
		this->Tone_set_SSG_emulation(5);
	}

	void convert(class EVENTS& in)
	{
		for (auto& eve : in.events) {
			if (eve.Time == SIZE_MAX) {
				break;
			}
			Calc_VGM_Time_090(eve.Time);

			if (in.loop_enable && (eve.Time == in.time_loop_start)) {
				//				std::wcout << eve.Time << L":" << in.time_loop_start << std::endl;
				this->time_loop_VGM_abs = this->time_prev_VGM_abs;
				this->vgm_loop_pos = vgm_body.size();

				//				std::wcout << this->time_loop_VGM_abs << L":" << this->vgm_loop_pos << std::endl;

				in.loop_enable = false;
			}

			if (eve.CH >= 8) {
				continue;
			}

			switch (eve.Event) {
			case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
				this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
				this->Tempo = eve.Param;

				// この後のNAの計算とタイマ割り込みの設定は実際には不要
				this->Timer_set_FM();
				break;
			case 0xEB:
				if (eve.Param == 0) {
					this->L[eve.CH] = false;
					this->R[eve.CH] = false;
				}
				else if (eve.Param == 1) {
					this->L[eve.CH] = true;
					this->R[eve.CH] = false;
				}
				else if (eve.Param == 2) {
					this->L[eve.CH] = false;
					this->R[eve.CH] = true;

				}
				else {
					this->L[eve.CH] = true;
					this->R[eve.CH] = true;
				}
				break;
			case 0xF5: // Tone select
				this->Tone_select_FM(eve.CH, eve.Param);
				break;
			case 0x80: // Note Off
				this->Note_off_FM(eve.CH);
				break;
			case 0xF9: // Volume change @V{0-127}
				this->Volume_FM(eve.CH, eve.Volume);
				break;
			case 0xD0:
				this->Key_set_FM(eve.CH, eve.Param);
				break;
			case 0x90: // Note on
				this->Note_on_FM(eve.CH);
				break;
			case 0xFC: // Detune
				break;
			}
		}

		Calc_VGM_Time_090(in.time_end);
		this->finish();
	}
};

class VGMdata_YM2203 : public VGM_YM2203_MAKO2 {
public:
	VGMdata_YM2203(void)
	{
		this->preset = NULL;
		this->vgm_header.lngHzYM2203 = MASTERCLOCK_NEC_OPN;
		this->ex_vgm.SetSSGVol(120);
		for (size_t i = 0; i < 12; i++) {
			double Freq = 440.0 * pow(2.0, (-9.0 + i) / 12.0);
			double fFNumber = 72.0 * Freq * pow(2.0, 21.0 - 4) / this->vgm_header.lngHzYM2203;
			this->FNumber[i] = fFNumber + 0.5;
		}

		for (size_t i = 0; i < 97; i++) {
			double Freq = 440.0 * pow(2.0, (-57.0 + i) / 12.0);
			double fTP = this->vgm_header.lngHzYM2203 / (64.0 * Freq);
			this->TPeriod[i] = fTP + 0.5;
		}
	}

	void make_init(void) {
		this->make_data(1, 0);
		this->make_data(0, 0);
		this->make_data(3, 0);
		this->make_data(2, 0);
		this->make_data(5, 0);
		this->make_data(4, 0);
		this->Volume(0, 0);
		this->Volume(1, 0);
		this->Volume(2, 0);
		this->Mixer(0277);
		this->make_data(0x27, 0x30);
		this->make_data(0x90, 0);
		this->make_data(0x91, 0);
		this->make_data(0x92, 0);
		this->make_data(0x24, 0x70);
		this->make_data(0x25, 0);
	}

	void convert(class EVENTS& in)
	{
		for (auto& eve : in.events) {
			if (eve.Time == SIZE_MAX) {
				break;
			}
			Calc_VGM_Time_090(eve.Time);

			if (in.loop_enable && (eve.Time == in.time_loop_start)) {
				//				std::wcout << eve.Time << L":" << in.time_loop_start << std::endl;
				this->time_loop_VGM_abs = this->time_prev_VGM_abs;
				this->vgm_loop_pos = vgm_body.size();

				//				std::wcout << this->time_loop_VGM_abs << L":" << this->vgm_loop_pos << std::endl;

				in.loop_enable = false;
			}

			if (eve.CH >= 6) {
				continue;
			}

			switch (eve.Event) {
			case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
				this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
				this->Tempo = eve.Param;

				// この後のNAの計算とタイマ割り込みの設定は実際には不要
				this->Timer_set_FM();
				break;
			case 0xFC: // Detune
				if (eve.CH < 3) {
					this->Detune_FM[eve.CH] = (__int16)((__int8)eve.Param);
				}
				else {
					this->Detune[eve.CH - 3] = (__int16)((__int8)eve.Param);
				}
				break;

			case 0xEB: // Panpot NOT FOR YM2203
				break;

			case 0xF5: // Tone select
				if (eve.CH < 3) {
					this->Tone_select_FM(eve.CH, eve.Param);
				}
				break;
			case 0x80: // Note Off
				if (eve.CH < 3) {
					this->Note_off_FM(eve.CH);
				}
				else {
					this->Note_off(eve.CH - 3);
				}
				break;
			case 0xF9: // Volume change @V{0-127}
				if (eve.CH < 3) {
					this->Volume_FM(eve.CH, eve.Volume);
				}
				else {
					this->Volume(eve.CH - 3, eve.Volume);
				}
				break;
			case 0x90: // Note on
				if (eve.CH < 3) {
					this->Note_on_FM(eve.CH);
				}
				else {
					this->Note_on(eve.CH - 3);
				}
				break;
			case 0xD0: // Key set (Part of Note on)
				if (eve.CH < 3) {
					this->Key_set_FM(eve.CH, eve.Param);
				}
				else {
					this->Key_set(eve.CH - 3, eve.Param);
				}
				break;
			case 0xD1: // sLFOd
				if (eve.CH < 3) {
					this->Key_set_LFO_FM(eve.CH, eve.LFO_Detune);
				}
				else {
					this->Key_set_LFO(eve.CH - 3, eve.LFO_Detune);
				}
				break;
			}
		}

		Calc_VGM_Time_090(in.time_end);
		this->finish();
	}

};

class VGMdata_YM2608 : public VGM_YM2608_MAKO2 {
public:
	VGMdata_YM2608(void)
	{
		this->preset = NULL;
		this->vgm_header.lngHzYM2608 = MASTERCLOCK_NEC_OPNA;
		this->ex_vgm.SetSSGVol(90);
		for (size_t i = 0; i < 12; i++) {
			double Freq = 440.0 * pow(2.0, (-9.0 + i) / 12.0);
			double fFNumber = 2.0 * 72.0 * Freq * pow(2.0, 21.0 - 4) / this->vgm_header.lngHzYM2608;
			this->FNumber[i] = fFNumber + 0.5;
		}

		for (size_t i = 0; i < 97; i++) {
			double Freq = 440.0 * pow(2.0, (-57.0 + i) / 12.0);
			double fTP = this->vgm_header.lngHzYM2608 / (2.0 * 64.0 * Freq);
			this->TPeriod[i] = fTP + 0.5;
		}
	}

	void make_init(void) {
		this->make_data(1, 0);
		this->make_data(0, 0);
		this->make_data(3, 0);
		this->make_data(2, 0);
		this->make_data(5, 0);
		this->make_data(4, 0);
		this->Volume(0, 0);
		this->Volume(1, 0);
		this->Volume(2, 0);
		this->Mixer(0277);
		this->make_data(0x27, 0x30);
		this->make_data(0x90, 0);
		this->make_data(0x91, 0);
		this->make_data(0x92, 0);
		this->make_data(0x24, 0x70);
		this->make_data(0x25, 0);
		this->make_data(0x29, 0x83);
		this->Panpot_set(0, 1, 1);
		this->Panpot_set(1, 1, 1);
		this->Panpot_set(2, 1, 1);
		this->Panpot_set(3, 1, 1);
		this->Panpot_set(4, 1, 1);
		this->Panpot_set(5, 1, 1);
	}

	void convert(class EVENTS& in)
	{
		for (auto& eve : in.events) {
			if (eve.Time == SIZE_MAX) {
				break;
			}
			Calc_VGM_Time_090(eve.Time);

			if (in.loop_enable && (eve.Time == in.time_loop_start)) {
				//				std::wcout << eve.Time << L":" << in.time_loop_start << std::endl;
				this->time_loop_VGM_abs = this->time_prev_VGM_abs;
				this->vgm_loop_pos = vgm_body.size();

				//				std::wcout << this->time_loop_VGM_abs << L":" << this->vgm_loop_pos << std::endl;

				in.loop_enable = false;
			}

			if (eve.CH >= 9) {
				continue;
			}

			switch (eve.Event) {
			case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
				this->Time_Prev_VGM = ((this->Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
				this->Tempo = eve.Param;

				// この後のNAの計算とタイマ割り込みの設定は実際には不要
				this->Timer_set_FM();
				break;
			case 0xFC: // Detune
				if (eve.CH < 3) {
					this->Detune_FM[eve.CH] = (__int16)((__int8)eve.Param);
				}
				else if (eve.CH > 5) {
					this->Detune_FM2[eve.CH - 6] = (__int16)((__int8)eve.Param);
				}
				else {
					this->Detune[eve.CH - 3] = (__int16)((__int8)eve.Param);
				}
				break;

			case 0xEB:
				if (eve.CH < 3) {
					if (eve.Param & 0x80 || eve.Param == 0 || eve.Param == 64) {
						this->Panpot_set(eve.CH, 1, 1);
					}
					else if (eve.Param < 64) {
						this->Panpot_set(eve.CH, 1, 0);
					}
					else if (eve.Param > 64) {
						this->Panpot_set(eve.CH, 0, 1);
					}
				}
				else if (eve.CH > 5) {
					if (eve.Param & 0x80 || eve.Param == 0 || eve.Param == 64) {
						this->Panpot_set(eve.CH - 3, 1, 1);
					}
					else if (eve.Param < 64) {
						this->Panpot_set(eve.CH - 3, 1, 0);
					}
					else if (eve.Param > 64) {
						this->Panpot_set(eve.CH - 3, 0, 1);
					}
				}
				break;

			case 0xF5: // Tone select
				if (eve.CH < 3) {
					this->Tone_select_FM(eve.CH, eve.Param);
				}
				else if (eve.CH > 5) {
					this->Tone_select_FM2(eve.CH - 6, eve.Param);
				}
				break;
			case 0x80: // Note Off
				if (eve.CH < 3) {
					this->Note_off_FM(eve.CH);
				}
				else if (eve.CH > 5) {
					this->Note_off_FM2(eve.CH - 6);
				}
				else {
					this->Note_off(eve.CH - 3);
				}
				break;
			case 0xF9: // Volume change @V{0-127}
				if (eve.CH < 3) {
					this->Volume_FM(eve.CH, eve.Volume);
				}
				else if (eve.CH > 5) {
					this->Volume_FM2(eve.CH - 6, eve.Volume);
				}
				else {
					this->Volume(eve.CH - 3, eve.Volume);
				}
				break;
			case 0x90: // Note on
				if (eve.CH < 3) {
					this->Note_on_FM(eve.CH);
				}
				else if (eve.CH > 5) {
					this->Note_on_FM2(eve.CH - 6);
				}
				else {
					this->Note_on(eve.CH - 3);
				}
				break;
			case 0xD0: // Key set (Part of Note on)
				if (eve.CH < 3) {
					this->Key_set_FM(eve.CH, eve.Param);
				}
				else if (eve.CH > 5) {
					this->Key_set_FM2(eve.CH - 6, eve.Param);
				}
				else {
					this->Key_set(eve.CH - 3, eve.Param);
				}
				break;
			case 0xD1: // sLFOd
				if (eve.CH < 3) {
					this->Key_set_LFO_FM(eve.CH, eve.LFO_Detune);
				}
				else if (eve.CH > 5) {
					this->Key_set_LFO_FM2(eve.CH - 6, eve.LFO_Detune);
				}
				else {
					this->Key_set_LFO(eve.CH - 3, eve.LFO_Detune);
				}
				break;
			}
		}

		Calc_VGM_Time_090(in.time_end);
		this->finish();
	}

};

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

		if (debug) {
			m.print_mml();
		}
		m.unroll_loop();

		if (!m.end_time) {
			continue;
		}

		class EVENTS events;
		events.convert(m);
		if (debug) {
			events.print_all();
		}

		size_t outsize = 0;
		if (chip == CHIP::YM2203) {
			class VGMdata_YM2203 v2203;
			v2203.init((union MAKO2_Tone*)&inbuf.at(pM2HDR->chiptune_addr), mako2form, early_detune);
			v2203.make_init();
			v2203.convert(events);
			if (SSG_Volume) {
				v2203.ex_vgm.SetSSGVol(SSG_Volume);
			}
			outsize = v2203.out(*argv);
		}
		else if (chip == CHIP::YM2608) {
			class VGMdata_YM2608 v2608;
			v2608.init((union MAKO2_Tone*)&inbuf.at(pM2HDR->chiptune_addr), mako2form, early_detune);
			v2608.make_init();
			v2608.convert(events);
			if (SSG_Volume) {
				v2608.ex_vgm.SetSSGVol(SSG_Volume);
			}
			outsize = v2608.out(*argv);
		}
		else if (chip == CHIP::YM2151) {
			class VGMdata_YM2151 v2151;
			v2151.init((union MAKO2_Tone*)&inbuf.at(pM2HDR->chiptune_addr), mako2form, early_detune);
			v2151.make_init();
			v2151.convert(events);
			outsize = v2151.out(*argv);
		}

		if (outsize == 0) {
			std::wcerr << L"File output failed." << std::endl;

			continue;
		}
		else {
			std::wcout << outsize << L" bytes written." << std::endl;
		}
	}
}
