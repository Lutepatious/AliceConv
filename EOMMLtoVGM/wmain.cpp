#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <sys/types.h>

enum class Machine { NONE = 0, X68000, PC9801 };
constexpr size_t channels = 6;

struct MML_Events {
	size_t Time;
	unsigned __int8 Type;
	unsigned __int8 Param;
	unsigned __int16 Param16;
};

class MML_decoded_CH {
	bool mute = true;

	static unsigned getDefaultLen(unsigned __int8** pmsrc, unsigned Len)
	{
		unsigned NLen, NLen_d;
		char* tpos;

		if (isdigit(**pmsrc)) {
			unsigned TLen = strtoul((const char*)(*pmsrc), &tpos, 10);

			*pmsrc = (unsigned __int8*)tpos;
			if (TLen == 0 || TLen > 65) {
				wprintf_s(L"L %u: out of range.\n", TLen);
			}
			if (192 % TLen) {
				wprintf_s(L"L %u: out of range.\n", TLen);
				wprintf_s(L"Something wrong. %c%c[%c]%c%c\n", *(*pmsrc - 3), *(*pmsrc - 2), *(*pmsrc - 1), **pmsrc, *(*pmsrc + 1));

			}
			NLen_d = 192 / TLen;
		}
		else {
			NLen_d = Len;
		}
		NLen = NLen_d;
		while (**pmsrc == '.') {
			(*pmsrc)++;
			NLen_d >>= 1;
			NLen += NLen_d;
		}
		return NLen;
	}

public:
	std::vector<struct MML_Events> MML;
	size_t time_total = 0;
	bool x68tt = false;
	enum Machine M_arch = Machine::NONE;

	void init(bool opm98, enum Machine arch)
	{
		this->x68tt = opm98;
		this->M_arch = arch;
	}
	bool is_mute(void) {
		return this->mute;
	}
	void decode(std::string& s)
	{
		unsigned Octave = (this->M_arch == Machine::PC9801) ? 5 : 4; // 1 - 9
		unsigned GS = 8; // 1 - 8
		unsigned Len = 48; // 0-192
		unsigned Vol = 8; // 0-15
		unsigned XVol = 80; // 0-127
		unsigned Tempo = 120;
		unsigned Tone = 0;
		unsigned Key_order[] = { 9, 11, 0, 2, 4, 5, 7 };
		unsigned __int8* msrc = (unsigned __int8*)s.c_str();
		unsigned Octave_t;
		unsigned VoltoXVol[16] = { 85, 87, 90, 93, 95, 98, 101, 103, 106, 109, 111, 114, 117, 119, 122, 125 };
		unsigned Panpot = 3;

		// eomml 覚書
		// < 1オクターブ下げ
		// > 1オクターブ上げ
		// O デフォルト4 O4のAがA4と同じで440Hz
		// L ドットが付けられる!
		// X68000版の取り扱い
		// 闘神都市では音No.80から81音にOPM98.DATを読み込む。(80-160がOPM98.DATの音になる)
		while (*msrc) {
			unsigned RLen, NLen;
			unsigned Key = 0;
			unsigned time_on, time_off;
			struct MML_Events e;
			e.Time = this->time_total;

			switch (tolower(*msrc++)) {
			case 'm': // EOMMLの2文字命令はmbのみ? 念のためmfも組み込む
				if (tolower(*msrc) == 'b' || tolower(*msrc) == 'f') {
					msrc++;
				}
				break;
			case 'p': // Panpot
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					Panpot = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				if (Panpot > 3) {
					wprintf_s(L"P %u: out of range.\n", Panpot);
				}
				// wprintf_s(L"Panpot %u\n", Panpot);
				e.Type = 0xEB;
				e.Param = Panpot;
				this->MML.push_back(e);
				break;
			case 't': // Tempo
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					Tempo = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				if (Tempo < 32 || Tempo > 200) {
					wprintf_s(L"T %u: out of range.\n", Tempo);
				}
				// wprintf_s(L"Tempo %u\n", Tempo);
				e.Type = 0xF4;
				e.Param = Tempo;
				this->MML.push_back(e);
				break;
			case '@':
				if (tolower(*msrc) == 'o') {
					if (isdigit(*++msrc)) {
						unsigned __int8* tpos;
						Octave = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					if (Octave > 8) {
						wprintf_s(L"O %u: out of range.\n", Octave);
					}
				}
				if (tolower(*msrc) == 'v') {
					if (isdigit(*++msrc)) {
						unsigned __int8* tpos;
						XVol = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					if (XVol > 127) {
						wprintf_s(L"@V %u: out of range.\n", XVol);
					}
					// wprintf_s(L"XVol %u\n", XVol);
					e.Type = 0xF9;
					e.Param = XVol;
					this->MML.push_back(e);
				}
				else {
					if (isdigit(*msrc)) {
						unsigned __int8* tpos;
						Tone = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					//	wprintf_s(L"Tone %u\n", Tone);
					e.Type = 0xF5;
					e.Param = Tone;
					this->MML.push_back(e);
				}
				break;
			case 'q':
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					GS = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				if (GS == 0 || GS > 8) {
					wprintf_s(L"Q %u: out of range.\n", GS);
				}
				// wprintf_s(L"GS %u\n", GS);
				break;
			case 'v': // Volume
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					Vol = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				if (Vol > 15) {
					wprintf_s(L"V %u: out of range.\n", Vol);
					if (Vol < 128) {
						wprintf_s(L"Assume @V\n");
						e.Type = 0xF9;
						e.Param = Vol;
						this->MML.push_back(e);
					}
				}
				else {
					// wprintf_s(L"Vol %u\n", Vol);
					e.Type = 0xF9;
					e.Param = Vol * 8 / 3 + 85;
					this->MML.push_back(e);
				}
				break;
			case 'o': // Octave
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					Octave = strtoul((const char*)msrc, (char**)&tpos, 10);
					if (this->M_arch == Machine::PC9801) {
						Octave--;
					}
					msrc = tpos;
				}
				if (Octave > 8) {
					wprintf_s(L"O %u: out of range.\n", Octave);
				}
				// wprintf_s(L"Octave %u\n", Octave);
				break;
			case '>': // Octave up
				if (this->x68tt) {
					Octave--;
				}
				else {
					Octave++;
				}
				if (Octave > 8) {
					wprintf_s(L">:result %u out of range.\n", Octave);
				}
				// wprintf_s(L"Octave %u\n", Octave);
				break;
			case '<': // Octave down
				if (this->x68tt) {
					Octave++;
				}
				else {
					Octave--;
				}
				if (Octave > 8) {
					wprintf_s(L"<:result %u out of range.\n", Octave);
				}
				// wprintf_s(L"Octave %u\n", Octave);
				break;
			case 'l': // de fault Length
				Len = getDefaultLen(&msrc, Len);
				// wprintf_s(L"Len %u\n", Len);
				break;
			case 'r': // Rest
				RLen = getDefaultLen(&msrc, Len);
				// wprintf_s(L"Rest %u\n", RLen);
				e.Type = 0x80;
				this->MML.push_back(e);
				this->time_total += RLen;
				break;
			case '^': // Note continue
				NLen = getDefaultLen(&msrc, Len);
				if (GS == 8) {
					time_on = NLen - 1;
				}
				else {
					time_on = NLen * GS >> 3;
					if (time_on == 0) {
						time_on = 1;
					}
				}
				time_off = NLen - time_on;

				e.Type = 0x90;
				e.Param = Key; // 0 - 95 (MIDI Note No.12 - 107)
				this->MML.push_back(e);
				e.Type = 0x80;
				e.Time += time_on;
				this->MML.push_back(e);
				this->time_total += NLen;
				this->mute = false;
				break;
			case 'a': // Note key A
			case 'b': // Note key B
			case 'c': // Note key C
			case 'd': // Note key D
			case 'e': // Note key E
			case 'f': // Note key F
			case 'g': // Note key G
				Key = Key_order[tolower(*(msrc - 1)) - 'a'];
				Octave_t = Octave;
				if (*msrc == '#' || *msrc == '+') { // Sharp?
					if (Key == 11) {
						Key = 0;
						Octave_t++;
					}
					else {
						Key++;
					}
					msrc++;
				}
				else if (*msrc == '-') { // flat?
					if (Key == 0) {
						Key = 11;
						Octave_t--;
					}
					else {
						Key--;
					}
					msrc++;
				}
				else if (*msrc == ' ') {
					msrc++;
				}
				Key += Octave_t * 12;
				NLen = getDefaultLen(&msrc, Len);
				// wprintf_s(L"Note %u %u\n", Key, NLen);
				if (*msrc == '&') {
					msrc++;
					time_on = NLen;
				}
				else if (*msrc == '^') {
					time_on = NLen;
				}
				else if (NLen == 1)
				{
					time_on = 1;
				}
				else if (GS == 8) {
					time_on = NLen - 1;
				}
				else {
					time_on = NLen * GS >> 3;
					if (time_on == 0) {
						time_on = 1;
					}
				}
				time_off = NLen - time_on;

				e.Type = 0x90;
				e.Param = Key; // 0 - 95 (MIDI Note No.12 - 107)
				this->MML.push_back(e);
				if (time_off) {
					e.Type = 0x80;
					e.Time += time_on;
					this->MML.push_back(e);
				}
				this->time_total += NLen;
				this->mute = false;

				break;
			case '|':
			case '!':
			case ' ':
				break;
			default:
				wprintf_s(L"Something wrong. %c%c[%c]%c%c\n", *(msrc - 3), *(msrc - 2), *(msrc - 1), *msrc, *(msrc + 1));
			}
		}
	}

};

struct MML_decoded {
	class MML_decoded_CH CH[channels];

	size_t end_time = 0;
	size_t loop_start_time = 0;

	void init(bool opm98, enum Machine M)
	{
		for (auto& c : this->CH) {
			c.init(opm98, M);
		}
	}

	void decode(std::string(&s)[6])
	{
		bool debug = false;
		for (size_t i = 0; i < channels; i++) {
			this->CH[i].decode(s[i]);
		}
	}

	void unroll_loop(void) {
		// ループを展開し、全チャネルが同一長のループになるように調整する。
		bool debug = false;
		size_t max_time = 0;
		size_t delta_time_LCM = 1;
		bool no_loop = true;

		// 各ループ時間の最小公倍数をとる
		for (size_t i = 0; i < channels; i++) {
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
			wprintf_s(L"Loop: NONE %zu %zu\n", max_time, delta_time_LCM);
			this->end_time = max_time;
			this->loop_start_time = SIZE_MAX;
		}
		else {
			wprintf_s(L"Loop: Yes %zu\n", delta_time_LCM);
			this->end_time = delta_time_LCM;
			for (size_t i = 0; i < channels; i++) {
				// そもそもループしないチャネルはスキップ
				if (this->CH[i].is_mute() || this->CH[i].time_total == 0) {
					continue;
				}
				size_t time_extra = this->end_time;
				size_t times = time_extra / this->CH[i].time_total + !!(time_extra % this->CH[i].time_total);
				if (debug) {
					wprintf_s(L"%2zu: %9zu -> %9zu (x %zu)\n", i, this->CH[i].time_total, delta_time_LCM, delta_time_LCM / this->CH[i].time_total);
				}

				// ループ回数分のイベントの複写
				std::vector<struct MML_Events> t = this->CH[i].MML;
				for (size_t m = 0; m < times - 1; m++) {
					for (auto& e : t) {
						e.Time += this->CH[i].time_total;
					}
					this->CH[i].MML.insert(this->CH[i].MML.end(), t.begin(), t.end());
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

// イベント順序
// 80 F4 F5 F9 90

struct EVENT {
	size_t Time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの 消音=0, テンポ=1, 音源初期化=2, タイ=8, 発音=9程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ

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
	enum class Machine Arch = Machine::NONE;

public:
	std::vector<struct EVENT> events;
	size_t loop_start = SIZE_MAX;
	bool loop_enable = false;
	void init(enum class Machine M_arch)
	{
		this->Arch = M_arch;
	}

	void convert(struct MML_decoded& MMLs)
	{
		size_t time_end = MMLs.end_time;
		size_t counter = 0;
		this->loop_enable = false;

		for (size_t j = 0; j < channels; j++) {
			size_t i = channels - 1 - j;
			if (Arch == Machine::X68000) {
				i = j;
			}

			for (auto& in : MMLs.CH[i].MML) {
				if (MMLs.loop_start_time != SIZE_MAX) {
					this->loop_enable = true;
				}

				struct EVENT e;
				e.Count = counter++;
				e.Event = in.Type;
				e.Time = in.Time;
				e.CH = i;

				switch (in.Type) {
				case 0x80: // Note off
					e.Type = 0;
					this->events.push_back(e);
					break;
				case 0xF5: // Tone select
				case 0xEB: // Panpot
					e.Param = in.Param;
					e.Type = 2;
					this->events.push_back(e);
					break;
				case 0xF9: // Volume change (0-127)
					e.Param = in.Param;
					e.Type = 3;
					this->events.push_back(e);
					break;
				case 0xF4: // Tempo
					e.Param = in.Param;
					e.Type = 1;
					this->events.push_back(e);
					break;
				case 0x90: // Note on
					e.Type = 8;
					this->events.push_back(e);
					e.Event = 0x97;
					e.Param = in.Param;
					e.Type = 2;
					this->events.push_back(e);
					break;
				default:
					wprintf_s(L"%u: %2zu: How to reach ? \n", in.Type, i);
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
		while (this->events[length].Time < time_end) {
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
		for (auto& i : this->events) {
			wprintf_s(L"%8zu: %2d: %02X\n", i.Time, i.CH, i.Event);
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

				// wprintf_s(L"%8zu: %10zd %6zd %10zd\n", src->time, c_VGMT, d_VGMT, Time_Prev_VGM);
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

				// wprintf_s(L"%8zu: %10zd %6zd %10zd\n", src->time, c_VGMT, d_VGMT, Time_Prev_VGM);
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
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'9') {
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

		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}
		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		std::wcout << inbuf.size() << std::endl;
		std::vector <unsigned __int8> MML_Table[channels];
		std::vector <std::string> MML_Body[channels];
		std::string master_tune;

		if (inbuf[0] == '"') {
			M_arch = Machine::PC9801;
			std::string instr(&inbuf[0]);
			std::stringstream instr_s(instr);
			std::string line;
			bool header = true;
			bool master = false;
			size_t linecounter = 0;
			while (std::getline(instr_s, line)) {
				//				std::cout << line << std::endl;
				line.erase(line.end() - 1); // remove CR
				const std::string eomac("\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[eomac]\"");
				const std::string domac("\"[eomac]\",\"[eomac]\",\"[eomac]\",\"[domac]\",\"[eomac]\",\"[eomac]\"");
				if (line == eomac || line == domac) {
					header = false;
					master = true;
				}
				else if (header) {
					std::string col;
					std::stringstream line_s(line);
					size_t CH = 0;
					while (std::getline(line_s, col, ',')) {
						unsigned long v = std::stoul(col.substr(1, col.size() - 2));
						//						std::cout << v << std::endl;
						MML_Table[CH++].push_back((unsigned __int8)v);
					}
					col.empty();
				}
				else if (master) {
					master_tune = line.substr(1, line.size() - 2);
					master_tune.push_back('|');
					master = false;
				}
				else if (line.size() > 1) {
					std::string eomml_sig("[eomml");
					std::string::size_type s_end = line.find(eomml_sig);
					if (s_end != std::string::npos) {
						linecounter++;
					}
					else {
						std::string s = line.substr(1, line.size() - 2);
						s.push_back('|');
						MML_Body[linecounter].emplace_back(s);
					}
				}
			}

		}
		else if (inbuf[0] == 0x00) {
			std::vector<__int8>::iterator Table_end = std::find(inbuf.begin(), inbuf.end(), '\xff');
			size_t CH_counter = 0;
			for (std::vector <__int8>::iterator Table_pos = inbuf.begin(); Table_pos < Table_end; Table_pos++) {
				MML_Table[CH_counter % channels].push_back(*Table_pos);
				CH_counter++;
			}

			std::string eomml_sig("[eomml");
			std::string cblacket_sig("]");

			std::string instr(&inbuf.at(Table_end - inbuf.begin() + 1));
			std::string::size_type m_end = instr.find(std::string("\r"));
			master_tune = instr.substr(0, m_end);
			master_tune.push_back('|');
			std::string  n1 = instr.substr(m_end + 1, instr.size());

			for (size_t CH = 0; CH < channels; CH++) {
				std::string::size_type s_end = n1.find(eomml_sig);
				std::string s = n1.substr(0, s_end);
				std::stringstream s0(s);
				std::string ss;
				while (std::getline(s0, ss, '\r')) {
					ss.push_back('|');
					MML_Body[CH].emplace_back(ss);
				}

				if (CH < channels - 1) {
					std::string n0 = n1.substr(s_end + 1, n1.size());
					std::string::size_type s_begin = n0.find(cblacket_sig);
					n1 = n0.substr(s_begin + 2, n0.size());
				}
			}
		}
		else {
			continue;
		}

		if (debug) {
			for (size_t CH = 0; CH < channels; CH++) {
				for (auto& h : MML_Table[CH]) {
					std::wcout << h << " ";
				}
				std::wcout << std::endl;
				for (auto& b : MML_Body[CH]) {
					std::cout << b.c_str() << std::endl;
				}
			}
		}

		std::string MML_FullBody[channels];
		for (size_t CH = 0; CH < channels; CH++) {
			MML_FullBody[CH] += master_tune;
			for (size_t j = 0; j < MML_Table[CH].size(); j++) {
				MML_FullBody[CH] += MML_Body[CH][MML_Table[CH][j]];
			}
		}

		if (debug) {
			for (size_t CH = 0; CH < channels; CH++) {
				std::cout << MML_FullBody[CH].c_str() << std::endl;
			}
		}

		struct MML_decoded MMLs;

		MMLs.init(Tones_tousin, M_arch);
		MMLs.decode(MML_FullBody);
		MMLs.unroll_loop();

		if (!MMLs.end_time) {
			std::wcerr << L"No Data. skip." << std::endl;
			continue;
		}

		// 得られた展開データからイベント列を作る。
		class EVENTS events;
		events.init(M_arch);
		events.convert(MMLs);
		if (debug) {
			events.print_all();
		}
		if (M_arch == Machine::PC9801) {
			class VGMdata_YM2203 v2203;
			v2203.make_init();
			v2203.convert(events);
			if (SSG_Volume) {
				v2203.ex_vgm.SetSSGVol(SSG_Volume);
			}
			v2203.out(*argv);
		}
		else {
			class VGMdata_YM2151 v2151;
			v2151.make_init();
			if (Tones_tousin) {
				v2151.set_opm98();
			}
			v2151.convert(events);
			v2151.out(*argv);
		}
	}
}
