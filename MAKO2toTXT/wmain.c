#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"

#pragma pack (1)
struct mako2_header {
	unsigned __int32 chiptune_addr : 24;
	unsigned __int32 ver : 8;
	unsigned __int32 CH_addr[];
};

struct mako2_tone {
	union {
		struct {
			unsigned __int8 Connect : 3; // Connection
			unsigned __int8 FB : 3; // Self-FeedBack
			unsigned __int8 NU0 : 2; // Not used
		} S;
		unsigned __int8 B;
	} H;
	unsigned __int8 Unk[6]; // FM? or what?
	union {
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
		} S;
		unsigned __int8 B[9];
	} Op[4];
};

#pragma pack ()

// MAKO.OCM (1,2共通)に埋め込まれているF-number (MAKO1は全オクターブ分入っている)
static const unsigned __int16 FNumber[12] = { 0x0269, 0x028E, 0x02B4, 0x02DE, 0x0309, 0x0338, 0x0369, 0x039C, 0x03D3, 0x040E, 0x044B, 0x048D };
static const unsigned __int16 TP[8][12] = {
	{ 0x0EED,0x0E17,0x0D4C,0x0C8D,0x0BD9,0x0B2F,0x0A8E,0x09F6,0x0967,0x08E0,0x0860,0x07E8 },
	{ 0x0776,0x070B,0x06A6,0x0646,0x05EC,0x0597,0x0547,0x04FB,0x04B3,0x0470,0x0430,0x03F4 },
	{ 0x03BB,0x0385,0x0353,0x0323,0x02F6,0x02CB,0x02A3,0x027D,0x0259,0x0238,0x0218,0x01FA },
	{ 0x01DD,0x01C2,0x01A9,0x0191,0x017B,0x0165,0x0151,0x013E,0x012C,0x011C,0x010C,0x00FD },
	{ 0x00EE,0x00E1,0x00D4,0x00C8,0x00BD,0x00B2,0x00A8,0x009F,0x0096,0x008E,0x0086,0x007E },
	{ 0x0077,0x0070,0x006A,0x0064,0x005E,0x0059,0x0054,0x004F,0x004B,0x0047,0x0043,0x003F },
	{ 0x003B,0x0038,0x0035,0x0032,0x002F,0x002C,0x002A,0x0027,0x0025,0x0023,0x0021,0x001F },
	{ 0x001D,0x001C,0x001A,0x0019,0x0017,0x0016,0x0015,0x0013,0x0012,0x0011,0x0010,0x000F } };


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

#define MML_BUFSIZ (10 * 1024 * 1024)

// MAKOではTPQNは48固定らしい
#define TPQN (48)
#define VGM_CLOCK (44100)

// 108辺りか?
#define TEMPO_BASE 112
#define VGM_WAITBASE2X ((TEMPO_BASE*VGM_CLOCK)/TPQN)

enum CHIP { NONE = 0, YM2203, YM2608, YM2151 };
VGM_HEADER h_vgm = { FCC_VGM, 0, 0x171 };


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

struct EVENT {
	size_t time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの テンポ=0, 消音=1, 音源初期化=2, 発音=3程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ
	unsigned __int8 loop_start; // ループ起点か?
};

int eventsort(const* x, const void* n1, const void* n2)
{
	if (((struct EVENT*)n1)->time > ((struct EVENT*)n2)->time) {
		return 1;
	}
	else if (((struct EVENT*)n1)->time < ((struct EVENT*)n2)->time) {
		return -1;
	}
	else {
#if 0
		if (((struct EVENT*)n1)->Type > ((struct EVENT*)n2)->Type) {
			return 1;
		}
		else if (((struct EVENT*)n1)->Type < ((struct EVENT*)n2)->Type) {
			return -1;
		}
		else {
#endif
			if (((struct EVENT*)n1)->Count > ((struct EVENT*)n2)->Count) {
				return 1;
			}
			else if (((struct EVENT*)n1)->Count < ((struct EVENT*)n2)->Count) {
				return -1;
			}
			else {
				return 0;
			}
#if 0
		}

#endif
	}
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
			wprintf_s(L"Voice %2llu: FB %1d Connect %1d\n", i, (T + i)->H.S.FB, (T + i)->H.S.Connect);
			for (size_t j = 0; j < 4; j++) {
				wprintf_s(L" OP %1llu: DT %1d MULTI %2d TL %3d KS %1d AR %2d DR %2d SR %2d SL %2d RR %2d\n"
					, j, (T + i)->Op[j].S.DT, (T + i)->Op[j].S.MULTI, (T + i)->Op[j].S.TL, (T + i)->Op[j].S.KS
					, (T + i)->Op[j].S.AR, (T + i)->Op[j].S.DR, (T + i)->Op[j].S.SR, (T + i)->Op[j].S.SL, (T + i)->Op[j].S.RR);
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

//			wprintf_s(L"CH %2zu: %2zu Blocks retuon to %2zu ", i, Blocks, Loop_Block);

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
						wprintf_s(L"%02X\n", *src);
						src += 4;
						break;
					case 0xE5:
					case 0xF1:
					case 0xFA:
					case 0xFD:
						wprintf_s(L"%02X\n", *src);
						src += 2;
						break;
					case 0xE6:
					case 0xE7:
						wprintf_s(L"%02X\n", *src);
						src += 9;
						break;
					case 0xE8:
						wprintf_s(L"%02X\n", *src);
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
						wprintf_s(L"%02X\n", *src);
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

//			wprintf_s(L"MML Length %zu Time %zu Loop Start time %zu, Loop delta time %zu\n", (MMLs_decoded.CHs + i)->len, (MMLs_decoded.CHs + i)->time_total, (MMLs_decoded.CHs + i)->Loop_time, (MMLs_decoded.CHs + i)->Loop_delta);
#if 0
			for (size_t j = 0; j < (MMLs_decoded.CHs + i)->len; j++) {
				wprintf_s(L"%02X ", (MMLs_decoded.CHs + i)->MML[j]);
			}
			wprintf_s(L"\n");
#endif
		}

		// ループを展開し、全チャネルが同一長のループになるように調整する。
		size_t delta_time_LCM = MMLs_decoded.CHs->Loop_delta;
		size_t max_time = MMLs_decoded.CHs->time_total;
		size_t end_time = 0;
		size_t loop_start_time = max_time;
		size_t latest_CH;

		// 各ループ時間の最大公約数をとる
		for (size_t i = 1; i < CHs_real; i++) {
			delta_time_LCM = LCM(delta_time_LCM, (MMLs_decoded.CHs + i)->Loop_delta);
			if (max_time < (MMLs_decoded.CHs + i)->time_total) {
				max_time = (MMLs_decoded.CHs + i)->time_total;
			}
			if (end_time < (MMLs_decoded.CHs + i)->Loop_time + delta_time_LCM) {
				end_time = (MMLs_decoded.CHs + i)->Loop_time + delta_time_LCM;
				latest_CH = i;
			}
		}
		if (end_time < MMLs_decoded.CHs->Loop_time + delta_time_LCM) {
			end_time = MMLs_decoded.CHs->Loop_time + delta_time_LCM;
			latest_CH = 0;
		}

		// 最大公約数が極端に大ならそもそもループはないものとする
		// 物によってはループするごとに微妙にずれていって元に戻るものもある(多分バグ)
		if (delta_time_LCM > 10000000) {
			wprintf_s(L"loop over 10000000? I believe something wrong or No loop %zu\n", delta_time_LCM);
			end_time = max_time;
			loop_start_time = -1LL;
		}
		else {
			loop_start_time = end_time - delta_time_LCM;
//			wprintf_s(L"Unroll loops %zut - %zut.\n", loop_start_time, end_time);
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

		// 得られた展開データからイベント列を作る。
		struct EVENT* pEVENTs = GC_malloc(sizeof(struct EVENT) * 12 * 1024 * 1024);
		struct EVENT* dest = pEVENTs;
		size_t counter = 0;
		for (size_t j = 0; j < CHs_real; j++) {
			size_t i = CHs_real - j - 1;
			unsigned __int8* src = (MMLs_decoded.CHs + i)->MML;
			size_t time = 0;

			while (src - (MMLs_decoded.CHs + i)->MML < (MMLs_decoded.CHs + i)->len) {
				if ((latest_CH == i) && (src == ((MMLs_decoded.CHs + i)->MML + (MMLs_decoded.CHs + i)->Loop_address)) && (loop_start_time != -1LL)) {
					dest->loop_start = 1;
				}
				switch (*src) {
				case 0x00: //Note On
				case 0x01:
				case 0x02:
				case 0x03:
				case 0x04:
				case 0x05:
				case 0x06:
				case 0x07:
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					time += *((unsigned __int16*)src)++;
					dest->Type = 30;
					dest->CH = i;
					dest++;
					break;
				case 0x80:
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					time += *((unsigned __int16*)src)++;
					dest->Type = 10;
					dest->CH = i;
					dest++;
					break;
				case 0xEC:
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = 0;
					dest->time = time;
					dest->Type = 20;
					dest->CH = i;
					dest++;
					break;
				case 0xE0: // Counter
				case 0xE1:
				case 0xEB:
				case 0xF5: // Tone select
				case 0xF9: // Volume change
				case 0xFC: // Detune
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					dest->Type = 20;
					dest->CH = i;
					dest++;
					break;
				case 0xF4: // Tempo
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					dest->Type = 0;
					dest->CH = i;
					dest++;
					break;
				default:
					wprintf_s(L"? How to reach ? %02X", *src);
					break;
				}
			}
		}
		dest->time = -1;
		dest++;
		qsort_s(pEVENTs, dest - pEVENTs, sizeof(struct EVENT), eventsort, NULL);

		size_t length_real = 0;
		while ((pEVENTs + length_real)->time <= end_time) {
			length_real++;
		}
//		wprintf_s(L"Whole length %zu/%zu\n", length_real, dest - pEVENTs - 1);

#if 1
		for (size_t i = 0; i < length_real; i++) {
			if ((pEVENTs + i)->Event == 0xE0 || (pEVENTs + i)->Event == 0xE1 || (pEVENTs + i)->Event == 0xEB || (pEVENTs + i)->Event == 0xEC) {
				wprintf_s(L"%8zu: %2d: %02X %02X %d\n", (pEVENTs + i)->time, (pEVENTs + i)->CH, (pEVENTs + i)->Event, (pEVENTs + i)->Param, (pEVENTs + i)->loop_start);
			}
		}
#endif

		// イベント列をvgm化する
		unsigned __int8* vgm_out = GC_malloc(100 * 1024 * 1024);
		unsigned __int8* vgm_pos = vgm_out;
		int* Detune = GC_malloc(sizeof(int) * CHs_real);
		size_t Tempo = TEMPO_BASE;
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;
		size_t Time_Prev_VGM_abs = 0;
		size_t Time_Loop_VGM_abs = 0;
		enum CHIP chip = NONE;
		unsigned __int8 vgm_command_chip[] = { 0x00, 0x55, 0x56, 0x54 };
		unsigned __int8 vgm_command_chip2[] = { 0x00, 0x55, 0x57, 0x54 };
		unsigned __int8 SSG_out = 0xFF;
		unsigned* Algorithm = GC_malloc(sizeof(unsigned) * CHs_real);

		size_t vgm_header_len = 0xc0;

		if (CHs_real == 6) {
			chip = YM2203;
			h_vgm.lngHzYM2203 = 3993600;
		}
		else if (CHs_real == 9) {
			chip = YM2608;
			h_vgm.lngHzYM2608 = 7987200;
		}
		else if (CHs_real == 8) {
			chip = YM2151;
			h_vgm.lngHzYM2151 = 4000000;
		}

		for (struct EVENT* src = pEVENTs; (src - pEVENTs) < length_real; src++) {
			if (src->time - Time_Prev) {
				size_t c_VGMT = ((VGM_WAITBASE2X * src->time / Tempo) + 1) >> 1;
				size_t d_VGMT = c_VGMT - Time_Prev_VGM;

				//				wprintf_s(L"%8zu: %zd %zd %zd\n", src->time, c_VGMT, d_VGMT, Time_Prev_VGM);
				Time_Prev_VGM += d_VGMT;
				Time_Prev_VGM_abs += d_VGMT;
				Time_Prev = src->time;

				while (d_VGMT) {
					const size_t wait0 = 0xFFFF;
					const size_t wait1 = 882;
					const size_t wait2 = 735;

					const size_t wait3 = 16;
					if (d_VGMT >= wait0) {
						*vgm_pos++ = 0x61;
						*((unsigned __int16*)vgm_pos)++ = wait0;
						d_VGMT -= wait0;
					}
					else if (d_VGMT == wait1 * 2 || d_VGMT == wait1 + wait2 || (d_VGMT <= wait1 + wait3 && d_VGMT >= wait1)) {
						*vgm_pos++ = 0x63;
						d_VGMT -= wait1;
					}
					else if (d_VGMT == wait2 * 2 || (d_VGMT <= wait2 + wait3 && d_VGMT >= wait2)) {
						*vgm_pos++ = 0x62;
						d_VGMT -= wait2;
					}
					else if (d_VGMT <= wait3 * 2 && d_VGMT >= wait3) {
						*vgm_pos++ = 0x7F;
						d_VGMT -= wait3;
					}
					else if (d_VGMT <= 15) {
						*vgm_pos++ = 0x70 | (d_VGMT - 1);
						d_VGMT = 0;
					}
					else {
						*vgm_pos++ = 0x61;
						*((unsigned __int16*)vgm_pos)++ = d_VGMT;
						d_VGMT = 0;
					}
				}
			}

			if (src->loop_start) {
				Time_Loop_VGM_abs = Time_Prev_VGM_abs;
				h_vgm.lngLoopOffset = vgm_header_len + (vgm_pos - vgm_out) - ((UINT8*)&h_vgm.lngLoopOffset - (UINT8*)&h_vgm.fccVGM);
			}

			switch (src->Event) {
			case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
//				wprintf_s(L"%8zu: OLD tempo %zd total %zd\n", src->time, Tempo, Time_Prev_VGM);
				Time_Prev_VGM = ((Time_Prev_VGM * Tempo << 1) / src->Param + 1) >> 1;
				Tempo = src->Param;
//				wprintf_s(L"%8zu: NEW tempo %zd total %zd\n", src->time, Tempo, Time_Prev_VGM);
				break;
			case 0xF5: // Tone select
				if (chip == YM2203) {
					if (src->CH < 3) {
						Algorithm[src->CH] = (T + src->Param)->H.S.Connect;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xB0 + src->CH;
						*vgm_pos++ = (T + src->Param)->H.B;
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x30 + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[0].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x38 + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[1].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x34 + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[2].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x3C + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[3].B[plen];
						}
					}
				}
				else if (chip == YM2608) {
					if (src->CH < 3) {
						Algorithm[src->CH] = (T + src->Param)->H.S.Connect;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xB0 + src->CH;
						*vgm_pos++ = (T + src->Param)->H.B;
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x30 + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[0].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x38 + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[1].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x34 + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[2].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x3C + src->CH + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[3].B[plen];
						}
					}
					else if (src->CH < 6) {
						Algorithm[src->CH] = (T + src->Param)->H.S.Connect;
						*vgm_pos++ = vgm_command_chip2[chip];
						*vgm_pos++ = 0xB0 + src->CH - 3;
						*vgm_pos++ = (T + src->Param)->H.B;
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x30 + src->CH - 3 + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[0].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x38 + src->CH - 3 + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[1].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x34 + src->CH - 3 + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[2].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x3C + src->CH - 3 + plen * 0x10;
							*vgm_pos++ = (T + src->Param)->Op[3].B[plen];
						}
					}

				}
				else if (chip == YM2151) {
					if (src->CH < 8) {
						Algorithm[src->CH] = (T + src->Param)->H.S.Connect;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x20 + src->CH;
						*vgm_pos++ = (T + src->Param)->H.B | 0xC0;
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x40 + src->CH + plen * 0x20;
							*vgm_pos++ = (T + src->Param)->Op[0].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x50 + src->CH + plen * 0x20;
							*vgm_pos++ = (T + src->Param)->Op[1].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH + plen * 0x20;
							*vgm_pos++ = (T + src->Param)->Op[2].B[plen];
						}
						for (unsigned plen = 0; plen < 6; plen++) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x58 + src->CH + plen * 0x20;
							*vgm_pos++ = (T + src->Param)->Op[3].B[plen];
						}
					}
				}
				break;
			case 0xFC: // Detune
				Detune[src->CH] = (__int8)src->Param;
				break;
			case 0x80: // Note Off
				if (chip == YM2203) {
					if (src->CH < 3) {
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28;
						*vgm_pos++ = src->CH;
					}
					else if (src->CH < 6) {
						SSG_out |= (1 << (src->CH - 3));
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x07;
						*vgm_pos++ = SSG_out;
					}
				}
				else if (chip == YM2608) {
					if (src->CH < 3) {
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28;
						*vgm_pos++ = src->CH;
					}
					else if (src->CH < 6) {
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28;
						*vgm_pos++ = (src->CH - 3) | 0x04;
					}
					else if (src->CH < 9) {
						SSG_out |= (1 << (src->CH - 6));
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x07;
						*vgm_pos++ = SSG_out;
					}
				}
				else if (chip == YM2151) {
					if (src->CH < 8) {
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x08;
						*vgm_pos++ = src->CH;
					}
				}
				break;
			case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
				if (chip == YM2203) {
					if (src->CH < 3) {
						switch (Algorithm[src->CH]) {
						case 0:
						case 1:
						case 2:
						case 3:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 4:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 5:
						case 6:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x44 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 7:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x40 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x44 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						default:
							wprintf_s(L"? How to reach ?\n");
						}
					}
					else if (src->CH < 6) {
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x08 + src->CH - 3;
						*vgm_pos++ = src->Param >> 3;
					}
				}
				else if (chip == YM2608) {
					if (src->CH < 3) {
						switch (Algorithm[src->CH]) {
						case 0:
						case 1:
						case 2:
						case 3:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 4:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 5:
						case 6:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x44 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 7:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x40 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x44 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x4C + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						default:
							wprintf_s(L"? How to reach ?\n");
						}
					}
					else if (src->CH < 6) {
						switch (Algorithm[src->CH - 3]) {
						case 0:
						case 1:
						case 2:
						case 3:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x4C + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							break;
						case 4:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x48 + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = 0x57;
							*vgm_pos++ = 0x4C + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							break;
						case 5:
						case 6:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x44 + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x48 + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x4C + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							break;
						case 7:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x40 + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x44 + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x48 + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x4C + src->CH - 3;
							*vgm_pos++ = ~src->Param;
							break;
						default:
							wprintf_s(L"? How to reach ?\n");
						}
					}
					else if (src->CH < 9) {
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x08 + src->CH - 6;
						*vgm_pos++ = src->Param >> 3;
					}
				}
				else if (chip == YM2151) {
					if (src->CH < 8) {
						switch (Algorithm[src->CH]) {
						case 0:
						case 1:
						case 2:
						case 3:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x78 + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 4:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x70 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x78 + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 5:
						case 6:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x68 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x70 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x78 + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						case 7:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x60 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x68 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x70 + src->CH;
							*vgm_pos++ = ~src->Param;
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x78 + src->CH;
							*vgm_pos++ = ~src->Param;
							break;
						default:
							wprintf_s(L"? How to reach ?\n");
						}
					}
				}
				break;
			case 0x00: //Note On
			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				if (chip == YM2203) {
					if (src->CH < 3) {
						union {
							struct {
								unsigned __int16 FNumber : 11;
								unsigned __int16 Block : 3;
								unsigned __int16 dummy : 2;
							} S;
							unsigned __int8 B[2];
						} U;

						U.S.FNumber = FNumber[src->Param] + Detune[src->CH];
						U.S.Block = src->Event;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xA4 + src->CH;
						*vgm_pos++ = U.B[1];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xA0 + src->CH;
						*vgm_pos++ = U.B[0];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28;
						*vgm_pos++ = src->CH | 0xF0;
					}
					else if (src->CH < 6) {
						union {
							unsigned __int16 W;
							unsigned __int8 B[2];
						} U;

						U.W = TP[src->Event][src->Param];

						if (Detune == -4) {
							U.W += 1;
						}
						else if (Detune == -8) {
							U.W += 2;
						}
						else if (Detune == -12) {
							U.W += 3;
						}

						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x01 + (src->CH - 3) * 2;
						*vgm_pos++ = U.B[1];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x00 + (src->CH - 3) * 2;
						*vgm_pos++ = U.B[0];
						SSG_out &= ~(1 << (src->CH - 3));
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x07;
						*vgm_pos++ = SSG_out;
					}
				}
				else if (chip == YM2608) {
					if (src->CH < 3) {
						union {
							struct {
								unsigned __int16 FNumber : 11;
								unsigned __int16 Block : 3;
								unsigned __int16 dummy : 2;
							} S;
							unsigned __int8 B[2];
						} U;

						U.S.FNumber = FNumber[src->Param] + Detune[src->CH];
						U.S.Block = src->Event;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xA4 + src->CH;
						*vgm_pos++ = U.B[1];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xA0 + src->CH;
						*vgm_pos++ = U.B[0];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28;
						*vgm_pos++ = src->CH | 0xF0;
					}
					else if (src->CH < 6) {
						union {
							struct {
								unsigned __int16 FNumber : 11;
								unsigned __int16 Block : 3;
								unsigned __int16 dummy : 2;
							} S;
							unsigned __int8 B[2];
						} U;

						U.S.FNumber = FNumber[src->Param] + Detune[src->CH];
						U.S.Block = src->Event;
						*vgm_pos++ = vgm_command_chip2[chip];
						*vgm_pos++ = 0xA4 + src->CH - 3;
						*vgm_pos++ = U.B[1];
						*vgm_pos++ = vgm_command_chip2[chip];
						*vgm_pos++ = 0xA0 + src->CH - 3;
						*vgm_pos++ = U.B[0];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28;
						*vgm_pos++ = (src->CH - 3) | 0xF4;
					}
					else if (src->CH < 9) {
						union {
							unsigned __int16 W;
							unsigned __int8 B[2];
						} U;

						U.W = TP[src->Event][src->Param];

						if (Detune == -4) {
							U.W += 1;
						}
						else if (Detune == -8) {
							U.W += 2;
						}
						else if (Detune == -12) {
							U.W += 3;
						}

						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x01 + (src->CH - 6) * 2;
						*vgm_pos++ = U.B[1];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x00 + (src->CH - 6) * 2;
						*vgm_pos++ = U.B[0];
						SSG_out &= ~(1 << (src->CH - 6));
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x07;
						*vgm_pos++ = SSG_out;
					}
				}
				else if (chip == YM2151) {
					if (src->CH < 8) {
						unsigned key = src->Event * 12 + src->Param;
						key -= 3;
						unsigned oct = key / 12;
						unsigned pre_note = key % 12;
						unsigned note = (pre_note << 2) / 3;
//						wprintf_s(L"%8zu: %u-%2u to %zu-%2u\n", src->time, src->Event, src->Param, oct, note);
						union {
							struct {
								unsigned __int8 note : 4;
								unsigned __int8 oct : 3;
								unsigned __int8 dummy : 1;
							} S;
							unsigned __int8 KC;
						} U;

						U.S.note = note;
						U.S.oct = oct;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28 + src->CH;
						*vgm_pos++ = U.KC;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x08;
						*vgm_pos++ = src->CH | 0x78;
					}
				}
				break;
			case 0xE0: // Counter
			case 0xE1:
			case 0xEB:
			case 0xEC:
				break;
			}

		}
		*vgm_pos++ = 0x66;
		size_t vgm_dlen = vgm_pos - vgm_out;

		h_vgm.lngTotalSamples = Time_Prev_VGM_abs;
		h_vgm.lngDataOffset = vgm_header_len - ((UINT8*)&h_vgm.lngDataOffset - (UINT8*)&h_vgm.fccVGM);
		h_vgm.lngEOFOffset = vgm_header_len + vgm_dlen - ((UINT8*)&h_vgm.lngEOFOffset - (UINT8*)&h_vgm.fccVGM);

		if (Time_Loop_VGM_abs) {
			h_vgm.lngLoopSamples = Time_Prev_VGM_abs - Time_Loop_VGM_abs;
		}

		wchar_t fn[_MAX_FNAME], fpath[_MAX_PATH];

		_wsplitpath_s(*argv, NULL, 0, NULL, 0, fn, _MAX_FNAME, NULL, 0);
		_wmakepath_s(fpath, _MAX_PATH, NULL, NULL, fn, L".vgm");

		ecode = _wfopen_s(&pFo, fpath, L"wb");
		if (ecode || !pFo) {
			fwprintf_s(stderr, L"%s cannot open\n", fpath);
			exit(ecode);
		}

		fwrite(&h_vgm, 1, vgm_header_len, pFo);
		fwrite(vgm_out, 1, vgm_dlen, pFo);

		fclose(pFo);
	}
}