#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"
#include "EOMML.h"
#include "../aclib/tools.h"

eomml_decoded_CH::eomml_decoded_CH()
{
	this->MML = (unsigned __int8*)GC_malloc(32 * 1024);
}

eomml_decoded::eomml_decoded(unsigned __int8* header, size_t fsize)
{
	this->mpos = (unsigned __int8*)memchr(header, '\xFF', fsize);
	this->pEOF = header + fsize;
	this->CH = new class eomml_decoded_CH[CHs];
	this->mml_block = (struct MML_BLOCK*) header;
	this->mml_blocks = (mpos - header) / CHs;
	this->block = (unsigned __int8**)GC_malloc(sizeof(unsigned __int8*) * this->mml_blocks);
	this->dest = this->mmls = (unsigned __int8*)GC_malloc(fsize - (mpos - header));
}

void eomml_decoded_CH::print(void)
{
	for (size_t i = 0; i < this->len; i++) {
		wprintf_s(L"%02X ", *(this->MML + i));
	}
	wprintf_s(L"\n");
}

void eomml_decoded::print(void)
{
	for (size_t i = 0; i < CHs; i++) {
		(this->CH + i)->print();
	}
}

bool eomml_decoded_CH::is_mute(void)
{
	return this->mute;
}

void eomml_decoded::unroll_loop(void)
{
	// ループを展開し、全チャネルが同一長のループになるように調整する。
	bool debug = false;
	size_t max_time = 0;
	size_t delta_time_LCM = 1;
	bool no_loop = true;

	// 各ループ時間の最小公倍数をとる
	for (size_t i = 0; i < CHs; i++) {
		// ループ展開後の長さの初期化
		(this->CH + i)->len_unrolled = (this->CH + i)->len;
		// ループなしの最長時間割り出し
		if (max_time < (this->CH + i)->time_total) {
			max_time = (this->CH + i)->time_total;
		}

		// 全チャンネルが非ループかチェック
		no_loop &= (this->CH + i)->is_mute();
		// そもそもループしないチャネルはスキップ
		if ((this->CH + i)->is_mute() || (this->CH + i)->time_total == 0) {
			continue;
		}
		delta_time_LCM = LCM(delta_time_LCM, (this->CH + i)->time_total);
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
		for (size_t i = 0; i < CHs; i++) {
			// そもそもループしないチャネルはスキップ
			if ((this->CH + i)->is_mute() || (this->CH + i)->time_total == 0) {
				continue;
			}
			size_t time_extra = this->end_time;
			size_t times = time_extra / (this->CH + i)->time_total + !!(time_extra % (this->CH + i)->time_total);
			(this->CH + i)->len_unrolled = (this->CH + i)->len * (times + 1);
			if (debug) {
				wprintf_s(L"%2zu: %9zu -> %9zu (x %zu)\n", i, (this->CH + i)->time_total, delta_time_LCM, delta_time_LCM / (this->CH + i)->time_total);
			}
		}
	}
}

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

void eomml_decoded_CH::decode(unsigned __int8* input)
{

	unsigned Octave = 4; // 1 - 9
	unsigned GS = 8; // 1 - 8
	unsigned Len = 48; // 0-192
	unsigned Vol = 8; // 0-15
	unsigned XVol = 80; // 0-127
	unsigned Tempo = 120;
	unsigned Tone = 0;
	unsigned Key_order[] = { 9, 11, 0, 2, 4, 5, 7 };
	unsigned __int8* msrc = input;
	unsigned __int8* dest = this->MML;
	unsigned Octave_t;
	unsigned VoltoXVol[16] = {85, 87, 90, 93, 95, 98, 101, 103, 106, 109, 111, 114, 117, 119, 122, 125};

	// eomml 覚書
	// < 1オクターブ下げ
	// > 1オクターブ上げ
	// O デフォルト4 O4のAがA4と同じで440Hz
	// L ドットが付けられる!
	// X68000版の取り扱い
	// 闘神都市では音No.80から81音にOPM98.DATを読み込む。(80-160がOPM98.DATの音になる)
	while (*msrc) {
		unsigned RLen, NLen;
		unsigned Key;
		unsigned time_on, time_off;

		switch (tolower(*msrc++)) {
		case 'm': // EOMMLの2文字命令はmbのみ? 念のためmfも組み込む
			if (tolower(*msrc) == 'b' || tolower(*msrc) == 'f') {
				msrc++;
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
			// wprintf_s(L"Tempo %u\n", Tempo);
			*dest++ = 0xF4;
			*dest++ = Tempo;
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
				*dest++ = 0xF9;
				*dest++ = XVol;
			}
			else {
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					Tone = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				//	wprintf_s(L"Tone %u\n", Tone);
				*dest++ = 0xF5;
				*dest++ = Tone;
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
			}
			else {
				// wprintf_s(L"Vol %u\n", Vol);
				*dest++ = 0xF8;
				*dest++ = Vol;
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
			// wprintf_s(L"Octave %u\n", Octave);
			break;
		case '>': // Octave up
			Octave++;
			if (Octave > 8) {
				wprintf_s(L">:result %u out of range.\n", Octave);
			}
			// wprintf_s(L"Octave %u\n", Octave);
			break;
		case '<': // Octave down
			Octave--;
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
			*dest++ = 0x80;
			*dest++ = RLen;
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

			*dest++ = 0x90;
			*dest++ = Key; // 0 - 95 (MIDI Note No.12 - 107)
			*dest++ = time_on;
			*dest++ = time_off;
			this->time_total += NLen;
			this->mute = false;

			break;
		default:
			wprintf_s(L"Something wrong. %c%c[%c]%c%c\n", *(msrc - 3), *(msrc - 2), *(msrc - 1), *msrc, *(msrc + 1));
		}
	}
	this->len = dest - this->MML;
}

void eomml_decoded_CH::decode_block(unsigned __int8* input)
{
}

void eomml_decoded::decode(void)
{
	size_t CH = 0;
	this->mml[CH++] = this->dest;
	bool in_bracket = false;

	for (unsigned __int8* src = mpos + 1; *src != '\x1A' && src < this->pEOF; src++) {
		switch (*src) {
		case ' ':
			break;
		case '[':
			in_bracket = true;
			break;
		case ']':
			in_bracket = false;
			*this->dest++ = '\0';
			this->mml[CH++] = this->dest;
			break;
		default:
			if (!in_bracket) {
				*this->dest++ = *src;
			}
		}
	}

	CH--;

	for (size_t i = 0; i < CHs; i++) {
		size_t j = 0;
		unsigned __int8* src = this->block[j++] = this->mml[i];
		while (NULL != (src = (unsigned __int8*)strchr((const char*)src, '\x0d'))) {
			*src = '\0';
			this->block[j++] = ++src;
		}
		size_t newmml_len = 4096;
		unsigned __int8* newmml = (unsigned __int8*)GC_malloc(newmml_len);
		strcpy_s((char*)newmml, newmml_len, (const char*)this->block[0]);
		for (size_t k = 0; k < this->mml_blocks; k++) {
			size_t index = (this->mml_block + k)->ch[i] + 1;
			//				wprintf_s(L"%zu\n", index);

			if (newmml_len < strlen((const char*)newmml) + strlen((const char*)this->block[index])) {
				newmml_len += 4096;
				newmml = (unsigned __int8*)GC_realloc(newmml, newmml_len);
			}
			strcat_s((char*)newmml, newmml_len, (const char*)this->block[index]);
		}
		//			puts((const char*)newmml);
		(this->CH + i)->decode(newmml);

	}
}
