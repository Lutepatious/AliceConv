#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <cctype>
#include <cstring>
#include <limits>
#include <sys/types.h>

#include "gc_cpp.h"

#include "EOMMLtoVGM.h"
#include "EOMML.h"


// イベント順序
// 80 F4 F5 F9 90

struct EVENT {
	size_t time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの 消音=0, テンポ=1, 音源初期化=2, タイ=8, 発音=9程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ

	bool operator < (const struct EVENT& out) {
		unsigned CH_weight[16] = { 0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15 };
		if (this->time < out.time) {
			return true;
		}
		else if (this->time > out.time) {
			return false;
		}
		else if (this->Type < out.Type) {
			return true;
		}
		else if (this->Type > out.Type) {
			return false;
		}
		else if (CH_weight[this->CH] > CH_weight[out.CH]) {
			return true;
		}
		else if (CH_weight[this->CH] < CH_weight[out.CH]) {
			return false;
		}
		else if (this->Count < out.Count) {
			return true;
		}
		else {
			return false;
		}
	}
};

class EVENTS {
	size_t counter;
	size_t time_end = SIZE_MAX;
	enum class Machine Arch;

public:
	std::vector<struct EVENT> events;
	size_t length = 0;
	size_t loop_start = SIZE_MAX;
	bool loop_enable = false;
	EVENTS(size_t elems, size_t end, enum class Machine M_arch)
	{
		this->time_end = end;
		this->Arch = M_arch;

		counter = 0;
	}

	void convert(struct eomml_decoded& MMLs)
	{
		this->loop_enable = false;

		for (size_t j = 0; j < MMLs.CHs; j++) {
			size_t i = MMLs.CHs - 1 - j;
			if (Arch == Machine::X68000) {
				i = j;
			}

			size_t time = 0;
			size_t len = 0;
			while (len < (MMLs.CH + i)->len_unrolled) {
				unsigned __int8* src = (MMLs.CH + i)->MML + len;
				if (MMLs.loop_start_time != SIZE_MAX) {
					this->loop_enable = true;
				}
				while (src >= (MMLs.CH + i)->MML + (MMLs.CH + i)->len) {
					src -= (MMLs.CH + i)->len;
				}

				unsigned __int8* src_orig = src;
				size_t len_On, len_Off;

				struct EVENT e;

				switch (*src) {
				case 0x80: // Note off
					e.Count = counter++;
					e.Event = *src++;
					e.time = time;
					e.Type = 0;
					e.CH = i;
					this->events.push_back(e);
					len_Off = *src++;
					time += len_Off;
					break;
				case 0xF5: // Tone select
				case 0xEB: // Panpot
					e.Count = counter++;
					e.Event = *src++;
					e.Param = *src++;
					e.time = time;
					e.Type = 2;
					e.CH = i;
					this->events.push_back(e);
					break;
				case 0xF9: // Volume change (0-127)
					e.Count = counter++;
					e.Event = *src++;
					e.Param = *src++;
					e.time = time;
					e.Type = 3;
					e.CH = i;
					this->events.push_back(e);
					break;
				case 0xF4: // Tempo
					e.Count = counter++;
					e.Event = *src++;
					e.Param = *src++;
					e.time = time;
					e.Type = 1;
					e.CH = i;
					this->events.push_back(e);
					break;
				case 0x90: // Note on
					e.Count = counter++;
					e.Event = *src++;
					e.time = time;
					e.Type = 9;
					e.CH = i;
					this->events.push_back(e);

					e.Count = counter++;
					e.Event = 0x97;
					e.Param = *src++;
					e.time = time;
					e.Type = 2;
					e.CH = i;
					this->events.push_back(e);
					len_On = *src++;
					len_Off = *src++;

					time += len_On;

					e.Count = counter++;
					e.Event = 0x80;
					e.time = time;
					e.Type = 0;
					e.CH = i;
					this->events.push_back(e);

					time += len_Off;

					break;
				default:
					wprintf_s(L"%zu: %2zu: How to reach ? %02X %02X %02X %02X %02X %02X %02X %02X,%02X %02X\n", src - (MMLs.CH + i)->MML, i, *(src - 8), *(src - 7), *(src - 6), *(src - 5), *(src - 4), *(src - 3), *(src - 2), *(src - 1), *src, *(src + 1));
					break;
				}
				len += src - src_orig;
			}
		}

		// 出来上がった列の末尾に最大時間のマークをつける
		struct EVENT end;
		end.time = SIZE_MAX;
		this->events.push_back(end);

		// イベント列をソートする
		std::sort(this->events.begin(), this->events.end());

		// イベント列の長さを測る。
		while (this->events[this->length].time < this->time_end) {
			if (this->loop_start == SIZE_MAX) {
				this->loop_start = this->length;
			}
			this->length++;
		}

		// 重複イベントを削除し、ソートしなおす
		size_t length_work = this->length;
		for (size_t i = 0; i < length_work - 1; i++) {
			if (this->events[i].time == this->events[i + 1].time && this->events[i].Event == this->events[i + 1].Event) {
				if (this->events[i].Event == 0xF4 && this->events[i].Param == this->events[i + 1].Param) {
					this->events[i].time = SIZE_MAX;
					if (i < this->loop_start) {
						this->loop_start--;
					}
					this->length--;
				}
			}
		}
		std::sort(this->events.begin(), this->events.end());
		this->events.resize(this->length);

		wprintf_s(L"Event Length %8zu Loop from %zu\n", this->length, this->loop_start);
	}
	void print_all(void)
	{
		for (auto& i : this->events) {
			wprintf_s(L"%8zu: %2d: %02X\n", i.time, i.CH, i.Event);
		}
	}
};

#include "VGM.hpp"
constexpr size_t MASTERCLOCK_NEC_OPN = 3993600;
constexpr size_t MASTERCLOCK_SHARP_OPM = 4000000;

class VGMdata_YM2203 : public VGM_YM2203 {
public:
	VGMdata_YM2203(void)
	{
		this->vgm_header.lngHzYM2203 = MASTERCLOCK_NEC_OPN;
		this->ex_vgm.SetSSGVol(120);
		this->preset = (struct AC_FM_PARAMETER_BYTE*)preset_88;

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
		const static std::vector<unsigned char> Init{
			0x55, 0x00, 'W', 0x55, 0x00, 'A', 0x55, 0x00, 'O', 0x55, 0x27, 0x30, 0x55, 0x07, 0xBF,
			0x55, 0x90, 0x00, 0x55, 0x91, 0x00, 0x55, 0x92, 0x00, 0x55, 0x24, 0x70, 0x55, 0x25, 0x00 };
		vgm_body.insert(vgm_body.begin(), Init.begin(), Init.end());
	}

	void convert(class EVENTS& in)
	{
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;

		for (auto& eve : in.events) {
			if (eve.time == SIZE_MAX) {
				break;
			}
			if (eve.time - Time_Prev) {
				// Tqn = 60 / Tempo
				// TPQN = 48
				// Ttick = Tqn / 48
				// c_VGMT = Ttick * src_time * VGM_CLOCK 
				//        = 60 / Tempo / 48 * ticks * VGM_CLOCK
				//        = 60 * VGM_CLOCK * ticks / (48 * tempo)
				//        = 60 * VGM_CLOCK * ticks / (48 * master_clock / (192 * (1024 - NA)) (OPN) 
				//        = 60 * VGM_CLOCK * ticks / (48 * master_clock / (384 * (1024 - NA)) (OPNA) 
				//        = 60 * VGM_CLOCK * ticks / (48 * master_clock * 3 / (512 * (1024 - NA)) (OPM) 
				//
				// 本来、更にNAの整数演算に伴う計算誤差を加味すれば正確になるが、20分鳴らして2-4秒程度なので無視する事とした。
				// 一度はそうしたコードも書いたのでレポジトリの履歴を追えば見つかる。
				// MAKO2は長さを9/10として調整したが、MAKO1では6/5とする(闘神都市 PC-9801版のMAKO1とMAKO2の比較から割り出し)
				// VAはBIOSが演奏するので調整しない。

				size_t c_VGMT = (eve.time * 60 * VGM_CLOCK * 2 / (48 * this->Tempo) + 1) >> 1;
				size_t d_VGMT = c_VGMT - Time_Prev_VGM;

				// wprintf_s(L"%8zu: %10zd %6zd %10zd\n", src->time, c_VGMT, d_VGMT, Time_Prev_VGM);
				Time_Prev_VGM += d_VGMT;
				this->time_prev_VGM_abs += d_VGMT;
				Time_Prev = eve.time;

				this->make_wait(d_VGMT);
			}

			if (in.loop_enable && eve.time == in.loop_start) {
				this->time_loop_VGM_abs = time_prev_VGM_abs;
				this->vgm_loop_pos = vgm_body.size();
				in.loop_enable = false;
			}

			switch (eve.Event) {
			case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
				Time_Prev_VGM = ((Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
				this->Tempo = eve.Param;

				// この後のNAの計算とタイマ割り込みの設定は実際には不要
				this->Timer_set_FM();
				break;
			case 0xF5: // Tone select
				if (eve.CH < 3) {
					this->Tone_select_FM(eve.CH, eve.Param & 0x7F);
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
					this->Volume_FM(eve.CH, ~eve.Param & 0x7F);
				}
				else {
					this->Volume(eve.CH - 3, (((unsigned)eve.Param - 84) * 16 - 1) / 43);
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
			case 0x97: // Key_set
				if (eve.CH < 3) {
					this->Key_set_FM(eve.CH, eve.Param);
				}
				else {
					this->Key_set(eve.CH - 3, eve.Param);
				}
				break;
			}
		}
		this->finish();
	}
};

class VGMdata_YM2151 : public VGM_YM2151 {
public:
	VGMdata_YM2151(void)
	{
		this->vgm_header.lngHzYM2151 = MASTERCLOCK_SHARP_OPM;
		this->preset = (struct AC_FM_PARAMETER_BYTE_OPM*)preset_x68;
	}

	void make_init(void) {
		const static std::vector<unsigned char> Init{
			0x54, 0x30, 0x14, 0x54, 0x31, 0x14, 0x54, 0x32, 0x14, 0x54, 0x33, 0x14,
			0x54, 0x34, 0x14, 0x54, 0x35, 0x14,	0x54, 0x36, 0x14, 0x54, 0x37, 0x14,
			0x54, 0x38, 0x00, 0x54, 0x39, 0x00, 0x54, 0x3A, 0x00, 0x54, 0x3B, 0x00,
			0x54, 0x3C, 0x00, 0x54, 0x3D, 0x00, 0x54, 0x3E, 0x00, 0x54, 0x3F, 0x00,
			0x54, 0x01, 0x00, 0x54, 0x18, 0x00,	0x54, 0x19, 0x00, 0x54, 0x1B, 0x00,
			0x54, 0x08, 0x00, 0x54, 0x08, 0x01, 0x54, 0x08, 0x02, 0x54, 0x08, 0x03,
			0x54, 0x08, 0x04, 0x54, 0x08, 0x05, 0x54, 0x08, 0x06, 0x54, 0x08, 0x07,
			0x54, 0x60, 0x7F, 0x54, 0x61, 0x7F,	0x54, 0x62, 0x7F, 0x54, 0x63, 0x7F,
			0x54, 0x64, 0x7F, 0x54, 0x65, 0x7F, 0x54, 0x66, 0x7F, 0x54, 0x67, 0x7F,
			0x54, 0x68, 0x7F, 0x54, 0x69, 0x7F, 0x54, 0x6A, 0x7F, 0x54, 0x6B, 0x7F,
			0x54, 0x6C, 0x7F, 0x54, 0x6D, 0x7F,	0x54, 0x6E, 0x7F, 0x54, 0x6F, 0x7F,
			0x54, 0x70, 0x7F, 0x54, 0x71, 0x7F, 0x54, 0x72, 0x7F, 0x54, 0x73, 0x7F,
			0x54, 0x74, 0x7F, 0x54, 0x75, 0x7F, 0x54, 0x76, 0x7F, 0x54, 0x77, 0x7F,
			0x54, 0x78, 0x7F, 0x54, 0x79, 0x7F,	0x54, 0x7A, 0x7F, 0x54, 0x7B, 0x7F,
			0x54, 0x7C, 0x7F, 0x54, 0x7D, 0x7F, 0x54, 0x7E, 0x7F, 0x54, 0x7F, 0x7F,
			0x54, 0x14, 0x00, 0x54, 0x10, 0x64, 0x54, 0x11, 0x00 };
		vgm_body.insert(vgm_body.begin(), Init.begin(), Init.end());
	}
	
	void set_opm98(void)
	{
		memcpy_s((void*)&this->preset[0x4F], sizeof(struct AC_FM_PARAMETER_BYTE_OPM) * (200 - 0x4F), preset_x68_opm98, sizeof(struct AC_FM_PARAMETER_BYTE_OPM) * 82);
	}

	void convert(class EVENTS& in)
	{
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;

		for (auto& eve : in.events) {
			if (eve.time == SIZE_MAX) {
				break;
			}
			if (eve.time - Time_Prev) {
				// Tqn = 60 / Tempo
				// TPQN = 48
				// Ttick = Tqn / 48
				// c_VGMT = Ttick * src_time * VGM_CLOCK 
				//        = 60 / Tempo / 48 * ticks * VGM_CLOCK
				//        = 60 * VGM_CLOCK * ticks / (48 * tempo)
				//        = 60 * VGM_CLOCK * ticks / (48 * master_clock / (192 * (1024 - NA)) (OPN) 
				//        = 60 * VGM_CLOCK * ticks / (48 * master_clock / (384 * (1024 - NA)) (OPNA) 
				//        = 60 * VGM_CLOCK * ticks / (48 * master_clock * 3 / (512 * (1024 - NA)) (OPM) 
				//
				// 本来、更にNAの整数演算に伴う計算誤差を加味すれば正確になるが、20分鳴らして2-4秒程度なので無視する事とした。
				// 一度はそうしたコードも書いたのでレポジトリの履歴を追えば見つかる。
				// MAKO2は長さを9/10として調整したが、MAKO1では6/5とする(闘神都市 PC-9801版のMAKO1とMAKO2の比較から割り出し)
				// VAはBIOSが演奏するので調整しない。

				size_t c_VGMT = (eve.time * 60 * VGM_CLOCK * 2 / (48 * this->Tempo) + 1) >> 1;
				size_t d_VGMT = c_VGMT - Time_Prev_VGM;

				// wprintf_s(L"%8zu: %10zd %6zd %10zd\n", src->time, c_VGMT, d_VGMT, Time_Prev_VGM);
				Time_Prev_VGM += d_VGMT;
				this->time_prev_VGM_abs += d_VGMT;
				Time_Prev = eve.time;

				this->make_wait(d_VGMT);
			}

			if (in.loop_enable && eve.time == in.loop_start) {
				this->time_loop_VGM_abs = time_prev_VGM_abs;
				this->vgm_loop_pos = vgm_body.size();
				in.loop_enable = false;
			}

			switch (eve.Event) {
			case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
				Time_Prev_VGM = ((Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
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
				this->Volume_FM(eve.CH, eve.Param);
				break;
			case 0x97:
				this->Key_set_FM(eve.CH, eve.Param);
				break;
			case 0x90: // Note on
				this->Note_on_FM(eve.CH);
				break;
			}
		}
		this->finish();
	}
};

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
			if (*(*argv + 1) == L'9') {
				Old98_Octave = true;
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

		size_t fsize = std::filesystem::file_size(*argv);
		std::vector<__int8> inbuf(fsize);
		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}
		infile.read(&inbuf.at(0), fsize);
		infile.close();

		std::wcout << inbuf.size() << std::endl;
		std::vector<__int8> buffer;
		std::vector<__int8>* in = NULL;
		if (inbuf[0] == '"') {
			M_arch = Machine::PC9801;
			Old98_Octave = true;
			std::string instr(&inbuf[0]);
			std::stringstream instr_s(instr);
			std::string line;
			bool header = true;
			while (std::getline(instr_s, line)) {
				//				std::cout << line << std::endl;
				line.erase(line.end() - 1); // remove CR
				const std::string eomac("\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\"");
				const std::string domac("\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[domac]\",\"[eomac]\",\"[eomac]\"");
				if (line == eomac || line == domac) {
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
					col.empty();
				}
				else if (line.size() > 1) {
					buffer.insert(buffer.end(), line.begin() + 1, line.end() - 1);
					buffer.push_back(0x0d);
				}
				in = &buffer;
			}
			line.empty();

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

		std::wcout << in->size() << std::endl;
		struct eomml_decoded MMLs((unsigned __int8*)&in->at(0), in->size(), Tones_tousin, Old98_Octave);

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
		if (debug) {
			events.print_all();
		}

		if (M_arch == Machine::X68000) {
			class VGMdata_YM2151 v2151;
			v2151.make_init();
			if (Tones_tousin) {
				v2151.set_opm98();
			}
			v2151.convert(events);
			v2151.out(*argv);
		}
		else {
			class VGMdata_YM2203 v2203;
			v2203.make_init();
			v2203.convert(events);
			if (SSG_Volume) {
				v2203.ex_vgm.SetSSGVol(SSG_Volume);
			}
			v2203.out(*argv);
		}
	}
}
