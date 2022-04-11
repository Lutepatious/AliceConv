#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "VGM.hpp"
#include "tools.h"

constexpr size_t INIT_LEN = 1;

#pragma pack(push)
#pragma pack(1)
constexpr size_t CHs = 6;
struct PC88_MML_HEADER {
	unsigned __int16 Load_Address_Start;
	unsigned __int16 Load_Address_End; // Fllesize =  Load_Address_End - Load_Address_Start + 1 + 3
	unsigned __int16 CH_Address[CHs];
};

struct MML_Events {
	size_t Time;
	unsigned __int8 Type;
	unsigned __int8 Param;
	unsigned __int16 Param16;
};

class MML_decoded_CH {
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
	size_t len = 0;
	size_t time_total = 0;

	size_t loop_start = 0;
	bool mute = false;
	std::string MML;
	std::vector<struct MML_Events> E;
	std::vector<size_t> block_len;

	void decode(std::vector<size_t>& block_len_master)
	{
		unsigned Octave = 5; // 1 - 9
		unsigned GS = 8; // 1 - 8
		unsigned Len = 48; // 0-192
		unsigned Vol = 8; // 0-15
		unsigned XVol = 80; // 0-127
		unsigned Tempo = 120;
		unsigned Tone = 0;
		unsigned Key_order[] = { 9, 11, 0, 2, 4, 5, 7 };
		unsigned __int8* msrc = (unsigned __int8*)this->MML.c_str();
		unsigned Octave_t;
		unsigned VoltoXVol[16] = { 85, 87, 90, 93, 95, 98, 101, 103, 106, 109, 111, 114, 117, 119, 122, 125 };
		unsigned Panpot = 3;
		size_t block = 0;
		size_t block_len_total = 0;

		// eomml 覚書
		// < 1オクターブ下げ
		// > 1オクターブ上げ
		// O デフォルト4 O4のAがA4と同じで440Hz
		// L ドットが付けられる!
		while (*msrc) {
			unsigned RLen, NLen;
			unsigned Key;
			unsigned time_on, time_off;
			struct MML_Events e;

			switch (tolower(*msrc++)) {
			case 'm': // EOMMLの2文字命令はmbのみ? 念のためmfも組み込む
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					unsigned EPeriod = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
					if (EPeriod < 1 || EPeriod > 65535) {
						wprintf_s(L"T %u: out of range.\n", EPeriod);
					}
					e.Time = this->time_total;
					e.Type = 0xFA;
					e.Param16 = EPeriod;
					this->E.push_back(e);
				}
				else if (tolower(*msrc) == 'b' || tolower(*msrc) == 'f') {
					msrc++;
				}
				break;
			case 's': // EOMMLの2文字命令はmbのみ? 念のためmfも組み込む
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					unsigned EType = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
					if (EType > 15) {
						wprintf_s(L"T %u: out of range.\n", EType);
					}
					e.Time = this->time_total;
					e.Type = 0xFB;
					e.Param = EType;
					this->E.push_back(e);
				}
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

				e.Time = this->time_total;
				e.Type = 0xF4;
				e.Param = Tempo;
				this->E.push_back(e);
				break;
			case '@':
				if (tolower(*msrc) == 'v') {
					if (isdigit(*++msrc)) {
						unsigned __int8* tpos;
						XVol = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					if (XVol > 127) {
						wprintf_s(L"@V %u: out of range.\n", XVol);
					}

					e.Time = this->time_total;
					e.Type = 0xF9;
					e.Param = XVol;
					this->E.push_back(e);
				}
				else {
					if (isdigit(*msrc)) {
						unsigned __int8* tpos;
						Tone = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}

					e.Time = this->time_total;
					e.Type = 0xF5;
					e.Param = Tone;
					this->E.push_back(e);
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

						e.Time = this->time_total;
						e.Type = 0xF9;
						e.Param = Vol;
						this->E.push_back(e);
					}
				}
				else {
					XVol = Vol * 8 / 3 + 85;

					e.Time = this->time_total;
					e.Type = 0xF9;
					e.Param = XVol;
					this->E.push_back(e);
				}
				break;
			case 'o': // Octave
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					Octave = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				if (Octave > 8) {
					wprintf_s(L"O %u: out of range.\n", Octave);
				}
				break;
			case '>': // Octave up
				Octave++;
				if (Octave > 8) {
					wprintf_s(L">:result %u out of range.\n", Octave);
				}
				break;
			case '<': // Octave down
				Octave--;
				if (Octave > 8) {
					wprintf_s(L"<:result %u out of range.\n", Octave);
				}
				break;
			case 'l': // default Length
				Len = getDefaultLen(&msrc, Len);
				break;
			case 'r': // Rest
				RLen = getDefaultLen(&msrc, Len);

				e.Time = this->time_total;
				e.Type = 0x80;
				this->E.push_back(e);
				this->time_total += RLen;
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
				if (Octave_t) {
					Octave_t--;
				}
				Key += Octave_t * 12;
				NLen = getDefaultLen(&msrc, Len);
				// wprintf_s(L"Note %u %u\n", Key, NLen);
				if (*msrc == '&') {
					msrc++;
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

				e.Time = this->time_total;
				e.Type = 0x90;
				e.Param = Key; // 0 - 95 (MIDI Note No.12 - 107)
				this->E.push_back(e);

				if (time_off) {
					e.Time = this->time_total + time_on;
					e.Type = 0x80;
					this->E.push_back(e);
				}

				this->time_total += NLen;
				this->mute = false;

				break;
			case '!':
			case ' ':
				break;
			case '|': // block separator, align block here.
				size_t block_len;
				if (block_len_master[block] == 480 && this->block_len[block] == 384) {
					std::wcout << L"Fix for Intruder applied." << std::endl;
					this->time_total = block_len_total + 384;
					block++;
				}
				else if (block_len_master[block] == 480 && this->block_len[block] == 360) {
					std::wcout << L"Fix for Intruder applied." << std::endl;
					this->time_total = block_len_total + 384;
					block++;
				}
				else if (block_len_master[block] == 576 && this->block_len[block] == 384) {
					std::wcout << L"Fix for Intruder applied." << std::endl;
					this->time_total = block_len_total + 384;
					block++;
				}
				else {
					this->time_total = block_len_total + block_len_master[block++];
				}
				block_len_total = this->time_total;
				break;
			case '*':
				this->time_total = block_len_total + block_len_master[block++] + INIT_LEN;
				block_len_total = this->time_total;
				this->loop_start = this->time_total;
				if (block_len_master[block + 1] == 192 && this->block_len[block + 1] == 96 && Len == 12) {
					std::wcout << L"Fix for Rance applied." << std::endl;
					Len = 24;
				}
				break;
			default:
				wprintf_s(L"Something wrong. %c%c[%c]%c%c\n", *(msrc - 3), *(msrc - 2), *(msrc - 1), *msrc, *(msrc + 1));
			}
		}
		this->len = this->E.size();
	}

	void check_block_len()
	{
		this->block_len.clear();
		unsigned Len = 48; // 0-192
		unsigned __int8* msrc = (unsigned __int8*)this->MML.c_str();
		size_t block_time = 0;
		// eomml 覚書
		// < 1オクターブ下げ
		// > 1オクターブ上げ
		// O デフォルト4 O4のAがA4と同じで440Hz
		// L ドットが付けられる!
		while (*msrc) {
			switch (tolower(*msrc++)) {
			case 'm': // EOMMLの2文字命令はmbのみ? 念のためmfも組み込む
				if (tolower(*msrc) == 'b' || tolower(*msrc) == 'f') {
					msrc++;
				}
				break;
			case 'l': // default Length
				Len = getDefaultLen(&msrc, Len);
				break;
			case 'r': // Rest
				block_time += getDefaultLen(&msrc, Len);
				break;
			case 'a': // Note key A
			case 'b': // Note key B
			case 'c': // Note key C
			case 'd': // Note key D
			case 'e': // Note key E
			case 'f': // Note key F
			case 'g': // Note key G
				if (*msrc == '#' || *msrc == '+') { // Sharp?
					msrc++;
				}
				else if (*msrc == '-') { // flat?
					msrc++;
				}
				block_time += getDefaultLen(&msrc, Len);
				break;
			case '|': // Block Separator
			case '*':
				this->block_len.push_back(block_time);
				block_time = 0;
				break;
			default:;
			}
		}
	}
};

class MML_decoded {
public:
	class MML_decoded_CH CH[CHs];
	size_t max_blocks = 0;
	std::vector<size_t> master_block_len;

	void correct_block_len()
	{
		for (auto& i : CH) {
			i.check_block_len();
			if (this->max_blocks < i.block_len.size()) {
				this->max_blocks = i.block_len.size();
			}
		}
		for (auto& i : CH) {
			if (this->max_blocks != i.block_len.size()) {
				i.block_len.resize(this->max_blocks);
			}
		}
		this->master_block_len.resize(this->max_blocks, 0);
		for (size_t i = 0; i < this->max_blocks; i++) {
			for (size_t j = 0; j < CHs; j++) {
				if (this->master_block_len[i] < this->CH[j].block_len[i]) {
					this->master_block_len[i] = this->CH[j].block_len[i];
				}
			}
		}
	}
};

// イベント順序
// 80 F4 F5 F9 90

struct EVENT {
	size_t Time;
	size_t count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの 消音=0, テンポ=1, 音源初期化=2, タイ=8, 発音=9程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ
	unsigned __int16 Param16; // イベントのパラメータ 16ビット

	bool operator < (const struct EVENT& out) {
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
		else if (this->CH < out.CH) {
			return true;
		}
		else if (this->CH > out.CH) {
			return false;
		}
		else if (this->count < out.count) {
			return true;
		}
		else if (this->count > out.count) {
			return false;
		}
		else {
			return false;
		}
	}
};

class EVENTS {
	size_t time_end = SIZE_MAX;

public:
	std::vector<struct EVENT> events;
	size_t length = 0;
	size_t loop_start = INIT_LEN;
	bool loop_enable = true;
	void convert(class MML_decoded& MMLs)
	{
		size_t counter = 0;
		for (size_t i = 0; i < CHs; i++) {
			for (auto& e : MMLs.CH[i].E) {
				struct EVENT eve;
				eve.count = counter++;
				eve.CH = i;
				eve.Time = e.Time;
				eve.Event = e.Type;

				switch (e.Type) {
				case 0x80: // Note off
					eve.Type = 0;
					events.push_back(eve);
					break;
				case 0xF5: // Tone select
					eve.Param = e.Param;
					eve.Type = 2;
					events.push_back(eve);
					break;
				case 0xF9: // Volume change (0-127)
					eve.Param = e.Param;
					eve.Type = 3;
					events.push_back(eve);
					break;
				case 0xF4: // Tempo
					eve.Param = e.Param;
					eve.Type = 1;
					events.push_back(eve);
					break;
				case 0x90: // Note on
					eve.Type = 8;
					events.push_back(eve);
					eve.Event = 0x97;
					eve.Param = e.Param;
					eve.Type = 2;
					events.push_back(eve);
					break;
				case 0xFB: // Envelope Type
					eve.Param = e.Param;
					eve.Type = 3;
					events.push_back(eve);
					break;
				case 0xFA: // Envelope Period
					eve.Param16 = e.Param16;
					eve.Type = 3;
					events.push_back(eve);
					break;
				}
			}
			struct EVENT end;

			end.count = counter++;
			end.CH = i;
			end.Time = MMLs.CH[i].time_total;
			end.Event = 0xFF;
			end.Type = 9;
			events.push_back(end);
		}

		std::sort(events.begin(), events.end());
	}

	void print_all(void)
	{
		for (auto& e : this->events) {
			wprintf_s(L"%8zu: %2d: %02X\n", e.Time, e.CH, e.Event);
		}
	}
};

constexpr size_t MASTERCLOCK_NEC_OPN = 3993600;

class VGMdata_YM2203 : public VGM_YM2203 {
public:
	VGMdata_YM2203(void)
	{
		this->vgm_header.lngHzYM2203 = MASTERCLOCK_NEC_OPN;
		this->ex_vgm.SetSSGVol(200);
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
		const static std::vector<unsigned char> Init = {
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
#if 0
			case 0xFA:
				if (eve.CH >= 3) {
					this->Envelope_Generator_set(eve.Param16);
				}
				break;
			case 0xFB:
				if (eve.CH >= 3) {
					this->Envelope_Generator_Type_set(eve.CH - 3, eve.Param);
				}
				break;
#endif
			}

		}
		this->finish();
	}

};

#pragma pack(pop)

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" file ..." << std::endl;
		exit(-1);
	}

	while (--argc) {
		std::ifstream infile(*++argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		struct PC88_MML_HEADER* h = (struct PC88_MML_HEADER*)&inbuf.at(0);
		//		std::wcout << h->Load_Address_End - h->Load_Address_Start + 4 << "/" << inbuf.size() << std::endl;

		std::vector<unsigned __int16> MMLIndex(CHs);
		for (size_t ch = 0; ch < CHs; ch++) {
			MMLIndex.at(ch) = h->CH_Address[ch] - h->CH_Address[0] + 0x10;
		}

#if 0
		std::wcout << std::hex;
		for (auto& i : MMLIndex) {
			std::wcout << i << " ";
		}
		std::wcout << std::endl;
#endif

		std::vector<std::vector<unsigned __int16>> MMLAddr(CHs);
		for (size_t ch = 0; ch < CHs; ch++) {
			unsigned __int16* a = (unsigned __int16*)&inbuf.at(MMLIndex.at(ch));
			while (*a != 0) {
				if (*a != 0xFFFF) {
					MMLAddr[ch].push_back(*a - h->CH_Address[0] + 0x10);
				}
				else {
					MMLAddr[ch].push_back(0xFFFF);
				}
				a++;
			}
		}

		class MML_decoded M;

		for (size_t i = 0; i < CHs; i++) {
			for (auto& j : MMLAddr[i]) {
				if (j == 0xFFFF) {
					M.CH[i].MML += '*';
				}
				else {
					for (unsigned __int16 k = j; inbuf.at(k) != (char)0xFF; k++) {
						M.CH[i].MML += inbuf.at(k);
					}
					M.CH[i].MML += '|';
				}
			}
			//			std::cout << M.CH[i].MML << std::endl;
		}
		M.correct_block_len();
#if 0
		for (size_t i = 0; i < CHs; i++) {
			for (auto& k : M.CH[i].block_len) {
				std::cout << k << " ";
			}
			std::cout << std::endl;
		}
		std::cout << "-----" << std::endl;
		for (auto& k : M.master_block_len) {
			std::cout << k << " ";
		}
		std::cout << std::endl;
#endif
		for (size_t i = 0; i < CHs; i++) {
			M.CH[i].decode(M.master_block_len);
		}
		class EVENTS E;
		E.convert(M);
		//		E.print_all();

		class VGMdata_YM2203 V;

		V.make_init();
		V.convert(E);

		size_t outsize = V.out(*argv);
		if (outsize == 0) {
			std::wcerr << L"File output failed." << std::endl;

			continue;
		}
		else {
			std::wcout << outsize << L" bytes written." << std::endl;
		}
	}
	return 0;
}