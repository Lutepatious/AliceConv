#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

enum class Machine { NONE = 0, PC88VA, FMTOWNS, PC9801 };
constexpr size_t CHs = 6;

#pragma pack (1)
struct mako1_header {
	unsigned __int32 sig; // must be 0x0000A000
	struct {
		unsigned __int16 Address;
		unsigned __int16 Length;
	} CH[CHs];
};

#pragma pack ()
struct MML_Events {
	size_t Time;
	unsigned __int8 Type;
	unsigned __int8 Param;
	unsigned __int16 Param16;
};

class MML_decoded_CH {
	bool mute = true;
public:
	std::vector<struct MML_Events> E;
	size_t time_total = 0;
	void decode(unsigned __int8* in)
	{
		unsigned __int8* src = in;
		unsigned __int8 gate_step = 8, time_default = 48;

		while (*src != 0xFF) {
			struct MML_Events eve;

			unsigned __int8 key = 0, time, time_on, time_off;
			bool makenote = false;
			bool len_full = false;

			switch (*src) {
			case 0xF1:
			case 0xFB:
				src += 3;
				break;
			case 0xF6:
			case 0xFE:
				src += 4;
				break;
			case 0xF7:
			case 0xFA:
			case 0xFC:
			case 0xFD:
				src += 2;
				break;

			case 0xF2: // Gate/Step 設定 (発生時間/全時間) 引数 0-7 => 1/8 - 8/8
				gate_step = *++src + 1;
				src++;
				break;
			case 0xF3: // デフォルト音長設定
				time_default = *++src;
				src++;
				break;
			case 0xF4: // Tempo
			case 0xF5: // Tone select
			case 0xF9: // Volume change
				eve.Time = time_total;
				eve.Type = *src++;
				eve.Param = *src++;
				E.push_back(eve);
				break;
			case 0xF8: // 前の音の再発声
				src++;
				time = *src++;
				makenote = true;
				break;
			default:
				if (*src <= 0x60 || *src == 0x70) {
					key = *src++;
					time = *src++;
					makenote = true;
				}
				else if (*src >= 0x80 && *src <= 0xE0) {
					key = *src++ & ~0x80;
					time = *src++;
					len_full = true;
					makenote = true;
				}
				else {
					src++;
				}
			}
			// 音程表現はオクターブ*12+音名の8bit(0-95)とする。MIDIノートナンバーから12引いた値となる。
			if (makenote) {
				if (!time) {
					time = time_default;
				}

				if (time == 1) {
					time_on = 1;
				}
				else {
					if (len_full) {
						time_on = time;
					}
					else if (gate_step == 8) {
						time_on = time - 1;
					}
					else {
						time_on = (time >> 3) * gate_step;
						if (!time_on) {
							time_on = 1;
						}
						if (time == time_on) {
							time_on--;
						}
					}
				}
				time_off = time - time_on;
				if (key == 0x70 || time_on == 0) {
					eve.Time = time_total;
					eve.Type = 0x80;
					E.push_back(eve);
				}
				else {
					eve.Time = time_total;
					eve.Type = 0x90;
					eve.Param = key; // 0 - 96 (MIDI Note No.12 - 108)
					E.push_back(eve);
					if (time_off) {
						eve.Time = time_total + time_on;
						eve.Type = 0x80;
						E.push_back(eve);
					}
					this->mute = false;
				}
				this->time_total += time;
			}
		}
	}

	bool is_mute(void)
	{
		return this->mute;
	}
};

struct MML_decoded {
	class MML_decoded_CH CH[CHs];
	size_t end_time = 0;
	size_t loop_start_time = 0;

	void decode(std::vector<__int8>& in, struct mako1_header* m1h)
	{
		for (size_t i = 0; i < CHs; i++) {
			this->CH[i].decode((unsigned __int8*)&in.at(m1h->CH[i].Address));
		}
	}
	void unroll_loop(void)
	{
		// ループを展開し、全チャネルが同一長のループになるように調整する。
		bool debug = false;
		size_t max_time = 0;
		size_t delta_time_LCM = 1;
		bool no_loop = true;

		// 各ループ時間の最小公倍数をとる
		for (size_t i = 0; i < CHs; i++) {
			// ループなしの最長時間割り出し
			if (max_time < this->CH[i].time_total) {
				max_time = this->CH[i].time_total;
			}

			// 全チャンネルが非ループかチェック
			no_loop &= this->CH[i].is_mute();
			// そもそもループしないチャネルはスキップ
			if (this->CH[i].is_mute() || this->CH[i].time_total == 0) {
				continue;
			}
			delta_time_LCM = this->LCM(delta_time_LCM, this->CH[i].time_total);
		}
		// 物によってはループするごとに微妙にずれていって元に戻るものもあり、極端なループ時間になる。(多分バグ)
		// あえてそれを回避せずに完全ループを生成するのでバッファはとても大きく取ろう。
		// 全チャンネルがループしないのならループ処理自体が不要
		if (no_loop) {
			std::wcout << L"Loop: NONE " << max_time << std::endl;
			this->end_time = max_time;
			this->loop_start_time = SIZE_MAX;
		}
		else {
			std::wcout << L"Loop: Yes " << delta_time_LCM << std::endl;
			this->end_time = delta_time_LCM;
			for (size_t i = 0; i < CHs; i++) {
				// そもそもループしないチャネルはスキップ
				if (this->CH[i].is_mute() || this->CH[i].time_total == 0) {
					continue;
				}
				size_t time_extra = this->end_time;
				size_t times = time_extra / this->CH[i].time_total + !!(time_extra % this->CH[i].time_total);
				if (debug) {
					std::wcout << i << L": " << this->CH[i].time_total << L" -> " << delta_time_LCM << L"(x" << delta_time_LCM / this->CH[i].time_total << L")" << std::endl;
				}

				// ループ回数分のイベントの複写
				std::vector<struct MML_Events> t = this->CH[i].E;
				for (size_t m = 0; m < times - 1; m++) {
					for (auto& e : t) {
						e.Time += this->CH[i].time_total;
					}
					this->CH[i].E.insert(this->CH[i].E.end(), t.begin(), t.end());
				}
			}
		}
	}

	size_t LCM(size_t a, size_t b)
	{
		size_t L, S;
		if (a == b) {
			return a;
		}
		else if (a > b) {
			L = a;
			S = b;
		}
		else {
			L = b;
			S = a;
		}
		while (S != 0) {
			size_t mod = L % S;
			L = S;
			S = mod;
		}
		return a * b / L;
	}
};


struct EVENT {
	size_t Time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの 消音=0, テンポ=1, 音源初期化=2, タイ=8, 発音=9程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ
	unsigned __int16 Param16; // イベントのパラメータ 16ビット

	bool operator < (const struct EVENT& out) {
		unsigned CH_weight[16] = { 0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15 };
		if (this->Time < out.Time) {
			return true;
		}
		else if (this->Time > out.Time) {
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
public:
	std::vector<struct EVENT> events;
	size_t loop_start = SIZE_MAX;
	bool loop_enable = false;

	void convert(struct MML_decoded& MMLs) {
		size_t time_end = MMLs.end_time;
		size_t counter = 0;
		this->loop_enable = false;

		for (size_t j = 0; j < CHs; j++) {
			size_t i = CHs - 1 - j;

			size_t time = 0;
			size_t len = 0;
			for (auto& e : MMLs.CH[i].E) {
				if (MMLs.loop_start_time != SIZE_MAX) {
					this->loop_enable = true;
				}

				struct EVENT eve;
				eve.Count = counter++;
				eve.Event = e.Type;
				eve.Time = e.Time;
				eve.CH = i;

				switch (e.Type) {
				case 0x80: // Note off
					eve.Type = 0;
					this->events.push_back(eve);
					break;
				case 0xF5: // Tone select
				case 0xEB: // Panpot
					eve.Param = e.Param;
					eve.Type = 2;
					this->events.push_back(eve);
					break;
				case 0xF9: // Volume change (0-127)
					eve.Param = e.Param;
					eve.Type = 3;
					this->events.push_back(eve);
					break;
				case 0xF4: // Tempo
					eve.Param = e.Param;
					eve.Type = 1;
					this->events.push_back(eve);
					break;
				case 0x90: // Note on
					eve.Type = 8;
					this->events.push_back(eve);
					eve.Event = 0x98;
					eve.Param = e.Param;
					eve.Type = 2;
					this->events.push_back(eve);
					break;
				}
			}
		}

		// 出来上がった列の末尾に最大時間のマークをつける
		struct EVENT end;
		end.Type = 9;
		end.Event = 0xFF;
		end.Time = SIZE_MAX;
		this->events.push_back(end);

		// イベント列をソートする
		std::sort(this->events.begin(), this->events.end());

		size_t length = 0;
		// イベント列の長さを測る。
		while (this->events[length].Time < MMLs.end_time) {
			if (this->loop_start == SIZE_MAX) {
				this->loop_start = length;
			}
			length++;
		}

		// 重複イベントを削除し、ソートしなおす
		size_t length_work = length;
		for (size_t i = 0; i < length_work - 1; i++) {
			if (this->events[i].Time == this->events[i + 1].Time && this->events[i].Event == this->events[i + 1].Event) {
				if (this->events[i].Event == 0xF4 && this->events[i].Param == this->events[i + 1].Param) {
					this->events[i].Time = SIZE_MAX;
					if (i < this->loop_start) {
						this->loop_start--;
					}
					length--;
				}
			}
		}
		std::sort(this->events.begin(), this->events.end());
		this->events.resize(length);
	}
	void print_all(void)
	{
		for (auto& e : this->events) {
			std::wcout << e.Time << L": " << e.CH << L": " << e.Event << std::endl;
		}
	}
};

#include "VGM.hpp"
constexpr size_t MASTERCLOCK_NEC_OPN = 3993600;

class VGMdata_YM2203 : public VGM_YM2203 {
public:
	VGMdata_YM2203(void)
	{
		this->vgm_header.lngHzYM2203 = MASTERCLOCK_NEC_OPN;
		this->ex_vgm.SetSSGVol(120);
		this->preset = (struct AC_FM_PARAMETER_BYTE*)preset_98;

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

};
class VGMdata_YM2203_PC9801 : public VGMdata_YM2203 {
public:
	VGMdata_YM2203_PC9801(void)
	{
		this->ex_vgm.SetSSGVol(120);
		this->preset = (struct AC_FM_PARAMETER_BYTE*)preset_98;
	}
	void convert(class EVENTS& in)
	{
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;

		for (auto& eve : in.events) {
			if (eve.Time == SIZE_MAX) {
				break;
			}
			if (eve.Time - Time_Prev) {
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

				size_t c_VGMT = (6ULL * eve.Time * 60 * VGM_CLOCK * 2 / (5ULL * 48 * this->Tempo) + 1) >> 1;
				size_t d_VGMT = c_VGMT - Time_Prev_VGM;

				Time_Prev_VGM += d_VGMT;
				this->time_prev_VGM_abs += d_VGMT;
				Time_Prev = eve.Time;

				this->make_wait(d_VGMT);
			}

			if (in.loop_enable && eve.Time == in.loop_start) {
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
			case 0x98: // Key_set
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
class VGMdata_YM2203_PC88VA : public VGMdata_YM2203 {
public:
	VGMdata_YM2203_PC88VA(void)
	{
		this->ex_vgm.SetSSGVol(200);
		this->preset = (struct AC_FM_PARAMETER_BYTE*)preset_88;
	}
	void convert(class EVENTS& in)
	{
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;

		for (auto& eve : in.events) {
			if (eve.Time == SIZE_MAX) {
				break;
			}
			if (eve.Time - Time_Prev) {
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

				size_t c_VGMT = (eve.Time * 60 * VGM_CLOCK * 2 / (48 * this->Tempo) + 1) >> 1;
				size_t d_VGMT = c_VGMT - Time_Prev_VGM;

				Time_Prev_VGM += d_VGMT;
				this->time_prev_VGM_abs += d_VGMT;
				Time_Prev = eve.Time;

				this->make_wait(d_VGMT);
			}

			if (in.loop_enable && eve.Time == in.loop_start) {
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
			case 0x98: // Key_set
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

constexpr size_t MASTERCLOCK_FMTOWNS_OPN2 = 7200000;

class VGMdata_YM2612 : public VGM_YM2612 {
public:
	VGMdata_YM2612(void)
	{
		this->vgm_header.lngHzYM2612 = MASTERCLOCK_FMTOWNS_OPN2;
		this->preset = (struct AC_FM_PARAMETER_BYTE*)preset_98;

		for (size_t i = 0; i < 12; i++) {
			double Freq = 440.0 * pow(2.0, (-9.0 + i) / 12.0);
			double fFNumber = 144.0 * Freq * pow(2.0, 21.0 - 4) / this->vgm_header.lngHzYM2612;
			this->FNumber[i] = fFNumber + 0.5;
		}
		for (size_t i = 0; i < 97; i++) {
			this->TPeriod[i] = 0;
		}
	}
	void make_init(void) {
		const static std::vector<unsigned char> Init{
			0x52, 0x27, 0x30, 0x52, 0x90, 0x00, 0x52, 0x91, 0x00, 0x52, 0x92, 0x00, 0x52, 0x24, 0x70, 0x52, 0x25, 0x00,
			0x52, 0xB4, 0x80, 0x52, 0xB5, 0xC0, 0x52, 0xB6, 0x40, 0x53, 0xB4, 0x80, 0x53, 0xB5, 0xC0, 0x53, 0xB6, 0x40 };
		vgm_body.insert(vgm_body.begin(), Init.begin(), Init.end());
	}
	void convert(class EVENTS& in)
	{
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;

		for (auto& eve : in.events) {
			if (eve.Time == SIZE_MAX) {
				break;
			}
			if (eve.Time - Time_Prev) {
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

				size_t c_VGMT = (eve.Time * 60 * VGM_CLOCK * 2 / (48 * this->Tempo) + 1) >> 1;
				size_t d_VGMT = c_VGMT - Time_Prev_VGM;

				Time_Prev_VGM += d_VGMT;
				this->time_prev_VGM_abs += d_VGMT;
				Time_Prev = eve.Time;

				this->make_wait(d_VGMT);
			}

			if (in.loop_enable && eve.Time == in.loop_start) {
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
				else {
					this->Tone_select_FM2(eve.CH - 3, eve.Param & 0x7F);
				}
				break;
			case 0x80: // Note Off
				if (eve.CH < 3) {
					this->Note_off_FM(eve.CH);
				}
				else {
					this->Note_off_FM2(eve.CH - 3);
				}
				break;
			case 0xF9: // Volume change @V{0-127}
				if (eve.CH < 3) {
					this->Volume_FM(eve.CH, ~eve.Param & 0x7F);
				}
				else {
					this->Volume_FM2(eve.CH - 3, ~eve.Param & 0x7F);
				}
				break;
			case 0x90: // Note on
				if (eve.CH < 3) {
					this->Note_on_FM(eve.CH);
				}
				else {
					this->Note_on_FM2(eve.CH - 3);
				}
				break;
			case 0x98: // Key_set
				if (eve.CH < 3) {
					this->Key_set_FM(eve.CH, eve.Param);
				}
				else {
					this->Key_set_FM2(eve.CH - 3, eve.Param);
				}
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
		std::wcerr << L"Usage: " << *argv << L" [-v | -t] [-s<num>] file..." << std::endl;
		exit(-1);
	}

	unsigned __int8 SSG_Volume = 0;
	enum Machine M_arch = Machine::PC9801;
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'v') {
				M_arch = Machine::PC88VA;
			}
			else if (*(*argv + 1) == L't') {
				M_arch = Machine::FMTOWNS;
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

		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		struct mako1_header* pM1HDR = (struct mako1_header*)&inbuf.at(0);

		if (pM1HDR->sig == 0xA000U
			&& (size_t)pM1HDR->CH[5].Address + pM1HDR->CH[5].Length < inbuf.size()
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
		}
		else {
			continue;
		}
		std::wcout << *argv << L" is MAKO1 format. " << pM1HDR->CH[5].Address + pM1HDR->CH[5].Length << L"bytes." << std::endl;

		struct MML_decoded MMLs;
		MMLs.decode(inbuf, pM1HDR);
		MMLs.unroll_loop();

		if (!MMLs.end_time) {
			std::wcout << L"No Data. skip." << std::endl;
			continue;
		}

		wprintf_s(L"Make Sequential events\n");
		// 得られた展開データからイベント列を作る。
		class EVENTS events;
		events.convert(MMLs);
		if (debug) {
			events.print_all();
		}

#if 0
		class VGMdata1 vgmdata(MMLs.end_time, M_arch);
		vgmdata.make_init();
		vgmdata.convert(events);
		if (SSG_Volume) {
			vgmdata.SetSSGVol(SSG_Volume);
		}
		vgmdata.out(*argv);
#endif
	}
}
