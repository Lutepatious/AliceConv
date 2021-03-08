#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"

#pragma pack (1)
struct mako2_header {
	unsigned __int32 chiptune_addr : 24;
	unsigned __int32 ver : 8;
	unsigned __int32 CH_addr[];
};

struct mako2_tone {
	unsigned __int8 Connect : 3; // Connection
	unsigned __int8 FB : 3; // Self-FeedBack
	unsigned __int8 NU0 : 2; // Not used
	unsigned __int8 Unk0; // Unknown
	unsigned __int8 Unk1[5]; // Unknown
	struct {
		unsigned __int8 MULTI : 4; // Multiple
		unsigned __int8 DT : 3; // DeTune
		unsigned __int8 NU0 : 1; // Not used
		unsigned __int8 TL : 7; // Total Level
		unsigned __int8 NU1 : 1; // Not used
		unsigned __int8 AR : 5; // Attack Rate
		unsigned __int8 NU2 : 1; // Not used
		unsigned __int8 KS : 2; // Key Scale
		unsigned __int8 DR : 5; // Decay Rate
		unsigned __int8 NU3 : 3; // Not used
		unsigned __int8 SR : 5; // Sustain Rate
		unsigned __int8 NU4 : 3; // Not used
		unsigned __int8 RR : 4; // Release Rate
		unsigned __int8 SL : 4; // Sustain Level
		unsigned __int8 Unk0[3]; // Unknown
	} Op[4];
};

#pragma pack ()

// MAKO.OCM (1,2共通)に埋め込まれているF-number (MAKO1は全オクターブ分入っている)
static const unsigned __int16 FNumber[12] = { 0x0269, 0x028E, 0x02B4, 0x02DE, 0x0309, 0x0338, 0x0369, 0x039C, 0x03D3, 0x040E, 0x044B, 0x048D };


// 音程表現はとりあえず オクターブ 8bit 音名 8bitとする、VGMへの変換を考慮してMIDI化が前提のSystem3 for Win32とは変える。
#define MAKENOTE() \
	if (*src == 0xE9) {time_on = time; src++;} \
	else { \
		if (gate_time != 256) { \
			if (time > gate_time) {time_on = time - gate_time;} \
			else {time_on = 1;} \
		} else { \
			if (time == 1) {time_on = 1;} \
			else if (gate_step == 8) {time_on = time - 1;} \
			else if (time < 8) {time_on = 1;} \
			else {time_on = (time >> 3) * gate_step;} \
		} \
	} \
	time_off = time - time_on; \
	if (!note) {time_off += time_on; time_on = 0;} \
	if (time_on) {*dest++ = Octave_current; *dest++ = note - 1; *((unsigned __int16*)dest)++ = time_on; (MMLs_decoded.CHs + i)->time_total += time_on;} \
	*dest++ = 0x80; *dest++ = 0; *((unsigned __int16*)dest)++ = time_off; (MMLs_decoded.CHs + i)->time_total += time_off;\
	if (Octave_current != Octave) {Octave_current = Octave;}

#define MML_BUFSIZ (4 * 1024 * 1024)

struct mako2_mml_decoded {
	struct mako2_mml_CH_decoded {
		unsigned __int8 MML[MML_BUFSIZ];
		size_t len;
		size_t Loop_address;
		size_t Loop_len;
		size_t time_total;
		size_t Loop_time;
		size_t Loop_delta;
	} *CHs;
};

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


int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		FILE* pFi, * pFo;
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode || !pFi) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);
		wprintf_s(L"File %s size %I64d.\n", *argv, fs.st_size);

		unsigned __int8* inbuf = GC_malloc(fs.st_size);
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

		struct mako2_header* pM2HDR = inbuf;

		unsigned mako2form = 0;
		if (pM2HDR->ver == 1 && pM2HDR->chiptune_addr == 0x34UL) {
			mako2form = pM2HDR->ver;
		}
		if (pM2HDR->ver == 2 && pM2HDR->chiptune_addr == 0x44UL) {
			mako2form = pM2HDR->ver;
		}
		if (pM2HDR->ver == 3 && pM2HDR->chiptune_addr == 0x44UL) {
			mako2form = pM2HDR->ver;
		}

		if (!mako2form) {
			wprintf_s(L"Format ID  isn't match. %d %ld\n", pM2HDR->ver, pM2HDR->chiptune_addr);
			fclose(pFi);
			continue;
		}

		size_t ct_len = pM2HDR->CH_addr[0] - pM2HDR->chiptune_addr;
		size_t VOICEs = ct_len / sizeof(struct mako2_tone);
		struct mako2_tone* T = inbuf + pM2HDR->chiptune_addr;
#if 0
		for (size_t i = 0; i < VOICEs; i++) {
			wprintf_s(L"Voice %2llu: FB %1d Connect %1d\n", i, (T + i)->FB, (T + i)->Connect);
			for (size_t j = 0; j < 4; j++) {
				wprintf_s(L" OP %1llu: DT %1d MULTI %2d TL %3d KS %1d AR %2d DR %2d SR %2d SL %2d RR %2d\n"
					, j, (T + i)->Op[j].DT, (T + i)->Op[j].MULTI, (T + i)->Op[j].TL, (T + i)->Op[j].KS
					, (T + i)->Op[j].AR, (T + i)->Op[j].DR, (T + i)->Op[j].SR, (T + i)->Op[j].SL, (T + i)->Op[j].RR);
			}
		}
#endif

		unsigned CHs = (pM2HDR->chiptune_addr - 4) / 4;
		unsigned CHs_real = 0;

		for (size_t i = 0; i < CHs; i++) {
			if (pM2HDR->CH_addr[i]) {
				CHs_real++;
			}
		}
		wprintf_s(L"Format %u %2u/%2u CHs. %2llu Tones.\n", mako2form, CHs_real, CHs, VOICEs);

		// 各MML部のデコード T.T.氏によるsystem32 for Win32のソースをポインタ制御に書き換えたもの。
		struct mako2_mml_decoded MMLs_decoded;
		MMLs_decoded.CHs = GC_malloc(sizeof(struct mako2_mml_CH_decoded) * CHs_real);
		if (MMLs_decoded.CHs == NULL) {
			wprintf_s(L"Memory allocation error. \n");
			exit(-2);
		}

		for (size_t i = 0; i < CHs_real; i++) {
			size_t Blocks = 0;
			unsigned __int16* pBlock_offset = (unsigned __int16*)&inbuf[pM2HDR->CH_addr[i]];
			unsigned __int16 Block_offset = *pBlock_offset;
			while ((Block_offset & 0xFF00) != 0xFF00) {
				Blocks++;
				Block_offset = *++pBlock_offset;
			}
			size_t Loop_Block = Block_offset & 0xFF;

			wprintf_s(L"CH %2zu: %2zu Blocks retuon to %2zu ", i, Blocks, Loop_Block);

			unsigned __int16 Octave = 4, Octave_current = 4;
			unsigned __int16 time_default = 48;
			unsigned __int16 gate_step = 7, gate_time = 256;
			unsigned __int16 time, time_on, time_off, note;
			unsigned __int8* dest = (MMLs_decoded.CHs + i)->MML;
			(MMLs_decoded.CHs + i)->Loop_address = (MMLs_decoded.CHs + i)->MML;
			(MMLs_decoded.CHs + i)->Loop_time = 0;

			for (size_t j = 0; j < Blocks; j++) {
				if (j == Loop_Block) {
					(MMLs_decoded.CHs + i)->Loop_address = dest - (MMLs_decoded.CHs + i)->MML;
					(MMLs_decoded.CHs + i)->Loop_time = (MMLs_decoded.CHs + i)->time_total;
				}

				pBlock_offset = (unsigned __int16*)&inbuf[pM2HDR->CH_addr[i]];
				unsigned __int8* src = &inbuf[*(pBlock_offset + j)];

				//				wprintf_s(L"%2zu: ", j);
				while (*src != 0xFF) {
					//					wprintf_s(L"%02X ", *src);
					switch (*src) {
					case 0x00:
					case 0x01:
					case 0x02:
					case 0x03:
					case 0x04:
					case 0x05:
					case 0x06:
					case 0x07:
					case 0x08:
					case 0x09:
					case 0x0A:
					case 0x0B:
					case 0x0C:
						note = *src++;
						time = *src++;
						MAKENOTE();
						break;

					case 0x0D:
					case 0x0E:
					case 0x0F:
					case 0x10:
					case 0x11:
					case 0x12:
					case 0x13:
					case 0x14:
					case 0x15:
					case 0x16:
					case 0x17:
					case 0x18:
					case 0x19:
						note = *src++ - 0x0d;
						time = *((unsigned __int16*)src)++;
						MAKENOTE();
						break;

					case 0x80:
					case 0x81:
					case 0x82:
					case 0x83:
					case 0x84:
					case 0x85:
					case 0x86:
					case 0x87:
					case 0x88:
					case 0x89:
					case 0x8A:
					case 0x8B:
					case 0x8C:
						note = *src++ & 0x7F;
						time = time_default;
						MAKENOTE();
						break;

					case 0xE0: // Counter
					case 0xE1:
					case 0xEB:
					case 0xF4: // Tempo
					case 0xF5: // Tone select
					case 0xF9: // Volume change
					case 0xFC: // Detune
						*dest++ = *src++;
						*dest++ = *src++;
						break;

					case 0xE4:
					case 0xFE:
						src += 4;
						break;
					case 0xE5:
					case 0xF1:
					case 0xFA:
					case 0xFD:
						src += 2;
						break;
					case 0xE6:
					case 0xE7:
						src += 9;
						break;
					case 0xE8:
						src += 11;
						break;
					case 0xE9:
						src++;
						break;
					case 0xEA:
						gate_time = *++src;
						src++;
					case 0xEC:
						*dest++ = *src++;
						break;

					case 0xEE:
						Octave_current = ++Octave;
						src++;
						break;
					case 0xEF:
						Octave_current = --Octave;
						src++;
						break;
					case 0xF0:
						Octave_current = Octave = *++src;
						src++;
						break;
					case 0xF2:
						gate_step = *++src + 1;
						gate_time = 256;
						src++;
						break;
					case 0xF3:
						if (*(src + 1) & 0x80) { // 128 - 32767
							time_default = _byteswap_ushort(*((unsigned __int16*)(src + 1))) & 0x7FFF;
							src += 3;
						}
						else { // 0 - 127
							time_default = *(src + 1);
							src += 2;
						}
						break;
					case 0xF6:
					case 0xFB:
						src += 3;
						break;
					case 0xF7:
						Octave_current--;
						src++;
						break;
					case 0xF8:
						Octave_current++;
						src++;
						break;
					default:
						src++;
						break;
					}
				}
				src++;
				//				wprintf_s(L"%zu %% 48 = %zu\n", (MMLs_decoded.CHs + i)->time_total, (MMLs_decoded.CHs + i)->time_total % 48);

								//				wprintf_s(L"\n");
			}

			(MMLs_decoded.CHs + i)->len = dest - (MMLs_decoded.CHs + i)->MML;
			(MMLs_decoded.CHs + i)->Loop_delta = (MMLs_decoded.CHs + i)->time_total - (MMLs_decoded.CHs + i)->Loop_time;
			(MMLs_decoded.CHs + i)->Loop_len = (MMLs_decoded.CHs + i)->len - (MMLs_decoded.CHs + i)->Loop_address;

			wprintf_s(L"MML Length %zu Time %zu Loop Start time %zu, Loop delta time %zu\n", (MMLs_decoded.CHs + i)->len, (MMLs_decoded.CHs + i)->time_total, (MMLs_decoded.CHs + i)->Loop_time, (MMLs_decoded.CHs + i)->Loop_delta);
#if 0
			for (size_t j = 0; j < (MMLs_decoded.CHs + i)->len; j++) {
				wprintf_s(L"%02X ", (MMLs_decoded.CHs + i)->MML[j]);
			}
			wprintf_s(L"\n");
#endif
		}

		// ループを展開し、全チャネルが同一長のループになるように調整する。
		size_t delta_time_LCM = MMLs_decoded.CHs->Loop_delta;
		size_t max_len = MMLs_decoded.CHs->len;
		size_t end_time = 0;
		size_t loop_start_time = max_len;

		// 各ループ時間の最大公約数をとる
		for (size_t i = 1; i < CHs_real; i++) {
			delta_time_LCM = LCM(delta_time_LCM, (MMLs_decoded.CHs + i)->Loop_delta);
			end_time = (end_time < (MMLs_decoded.CHs + i)->Loop_time + delta_time_LCM) ? (MMLs_decoded.CHs + i)->Loop_time + delta_time_LCM : end_time;
			max_len = (max_len < (MMLs_decoded.CHs + i)->len) ? (MMLs_decoded.CHs + i)->len : max_len;
		}
		end_time = (end_time < MMLs_decoded.CHs->Loop_time + delta_time_LCM) ? MMLs_decoded.CHs->Loop_time + delta_time_LCM : end_time;

		// 最大公約数が極端に大ならそもそもループはないものとする
		// 物によってはループするごとに微妙にずれていって元に戻るものもある(多分バグ)
		if (delta_time_LCM > 10000000) {
			wprintf_s(L"loop over 10000000? I believe something wrong or No loop %zu\n", delta_time_LCM);
			end_time = loop_start_time = max_len;
		}
		else {
			loop_start_time = end_time - delta_time_LCM;
			wprintf_s(L"Unroll loops %zut - %zut.\n", loop_start_time, end_time);
			for (size_t i = 0; i < CHs_real; i++) {
				unsigned __int8* src = (MMLs_decoded.CHs + i)->MML + (MMLs_decoded.CHs + i)->Loop_address;
				unsigned __int8* dest = (MMLs_decoded.CHs + i)->MML + (MMLs_decoded.CHs + i)->len;
				size_t len = (MMLs_decoded.CHs + i)->Loop_len;
				size_t time_extra = end_time - (MMLs_decoded.CHs + i)->Loop_time;
				size_t times = time_extra / (MMLs_decoded.CHs + i)->Loop_delta + !!(time_extra % (MMLs_decoded.CHs + i)->Loop_delta);

				times--;
//				wprintf_s(L"loop start %zut, Unroll len=%zut loop %zu times.\n", (MMLs_decoded.CHs + i)->Loop_time, (MMLs_decoded.CHs + i)->Loop_delta, times);
				for (size_t j = 0; j < times; j++) {
					memcpy_s(dest + len * j, len, src, len);
				}

				(MMLs_decoded.CHs + i)->len += len * times;
//				wprintf_s(L"MML Length %zu Loop address %zu\n", (MMLs_decoded.CHs + i)->len, (MMLs_decoded.CHs + i)->Loop_address);
#if 0
				for (size_t j = 0; j < (MMLs_decoded.CHs + i)->len; j++) {
					wprintf_s(L"%02X ", (MMLs_decoded.CHs + i)->MML[j]);
				}
				wprintf_s(L"\n");
#endif
			}
		}
	}
}