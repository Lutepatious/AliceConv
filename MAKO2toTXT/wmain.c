#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <limits.h>
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
			unsigned __int8 RL : 2; // YM2151 Only
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



#define MML_BUFSIZ (32 * 1024)

// MAKOではTPQNは48固定らしい
#define TPQN (48)
#define VGM_CLOCK (44100)


enum CHIP { NONE = 0, YM2203, YM2608, YM2151 };


struct mako2_mml_decoded {
	struct mako2_mml_CH_decoded {
		unsigned __int8 MML[MML_BUFSIZ];
		size_t len;
		size_t len_unrolled;
		size_t Loop_address;
		size_t Loop_len;
		size_t time_total;
		size_t Loop_time;
		size_t Loop_delta;
		unsigned __int8 Mute_on;
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
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの テンポ=10, 消音=0, 音源初期化=20, 発音=30程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ
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
		if (((struct EVENT*)n1)->Type > ((struct EVENT*)n2)->Type) {
			return 1;
		}
		else if (((struct EVENT*)n1)->Type < ((struct EVENT*)n2)->Type) {
			return -1;
		}
		else {
			if (((struct EVENT*)n1)->Count > ((struct EVENT*)n2)->Count) {
				return 1;
			}
			else if (((struct EVENT*)n1)->Count < ((struct EVENT*)n2)->Count) {
				return -1;
			}
			else {
				return 0;
			}
		}

	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	enum CHIP chip_force = NONE;
	while (--argc) {
		enum CHIP chip = NONE;
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'a') {
				chip_force = YM2608;
			}
			else if (*(*argv + 1) == L'n') {
				chip_force = YM2203;
			}
			else if (*(*argv + 1) == L'x') {
				chip_force = YM2151;
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
			chip = YM2608;
			mako2form = pM2HDR->ver;
		}
		if (pM2HDR->ver == 3 && pM2HDR->chiptune_addr == 0x44UL) {
			chip = YM2608;
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

		if (chip_force != NONE) {
			chip = chip_force;
		}
		else if (chip == NONE) {
			if (CHs_real == 6) {
				chip = YM2203;
			}
			else if (CHs_real == 9) {
				chip = YM2608;
			}
			else if (CHs_real == 8) {
				chip = YM2151;
			}
		}

		wprintf_s(L"%s: %zu bytes Format %u %2u/%2u CHs. %2llu Tones.\n", *argv, fs.st_size, mako2form, CHs_real, CHs, VOICEs);

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

			//			wprintf_s(L"CH %2zu: %2zu Blocks return to %2zu ", i, Blocks, Loop_Block);

			unsigned __int16 Octave = 4, Octave_current = 4;
			unsigned __int16 time_default = 48, note;
			__int16 time, time_on, time_off, gate_step = 7;
			unsigned __int8 flag_gate = 0;
			unsigned __int8* dest = (MMLs_decoded.CHs + i)->MML;
			(MMLs_decoded.CHs + i)->Loop_address = 0;
			(MMLs_decoded.CHs + i)->Loop_time = 0;

			for (size_t j = 0; j < Blocks; j++) {
				if (j == Loop_Block) {
					(MMLs_decoded.CHs + i)->Loop_address = dest - (MMLs_decoded.CHs + i)->MML;
					(MMLs_decoded.CHs + i)->Loop_time = (MMLs_decoded.CHs + i)->time_total;
				}

				pBlock_offset = (unsigned __int16*)&inbuf[pM2HDR->CH_addr[i]];
				unsigned __int8* src = &inbuf[*(pBlock_offset + j)];

				// wprintf_s(L"%2zu: ", j);
				while (*src != 0xFF) {
					unsigned makenote = 0;
					// wprintf_s(L"%02X ", *src);
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
						makenote++;
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
						makenote++;
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
						makenote++;
						break;

					case 0xE0: // Counter
					case 0xE1: // Velocity
					case 0xF4: // Tempo
					case 0xF5: // Tone select
					case 0xF9: // Volume change
					case 0xFC: // Detune
						*dest++ = *src++;
						*dest++ = *src++;
						break;

					case 0xEB: // Panpot
						chip = YM2608;
						*dest++ = *src++;
						*dest++ = *src++;
						break;

					case 0xE4: // LFO
						chip = YM2608;
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
						break;

					case 0xFE: // none
						wprintf_s(L"%02X\n", *src);
						src += 4;
						break;
					case 0xE5:
					case 0xFA: // none
					case 0xFD: // none
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
					case 0xE9: // Tie
						*dest++ = *src++;
						break;
					case 0xEC:
						(MMLs_decoded.CHs + i)->Mute_on = 1;
						src++;
						break;
					case 0xEE:
						Octave_current = Octave = (Octave + 1) & 0x7;
						src++;
						break;
					case 0xEF:
						Octave_current = Octave = (Octave - 1) & 0x7;
						src++;
						break;
					case 0xF0:
						Octave_current = Octave = *++src;
						src++;
						break;
					case 0xF1:
						Octave_current = *++src;
						src++;
						break;
					case 0xEA:
						flag_gate = 1;
						gate_step = *++src;
						src++;
						break;
					case 0xF2:
						flag_gate = 0;
						gate_step = (short)(*++src) + 1;
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
					case 0xF6: // none
					case 0xFB:
						wprintf_s(L"%02X\n", *src);
						src += 3;
						break;
					case 0xF7:
						Octave_current = (Octave_current - 1) & 0x7;
						src++;
						break;
					case 0xF8:
						Octave_current = (Octave_current + 1) & 0x7;
						src++;
						break;
					default:
						src++;
						break;
					}
					// 音程表現はとりあえず オクターブ 8bit 音名 8bitとする、VGMへの変換を考慮してMIDI化が前提のSystem3 for Win32とは変える。
					if (makenote) {
						if (time == 1) {
							time_on = 1;
						}
						else if (!flag_gate) {
							if (gate_step == 8) {
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
						else {
							time_on = time - gate_step;
							if (time_on < 0) {
								time_on = 1;
							}
						}
						time_off = time - time_on;
						if (!note) {
							time_off += time_on;
							time_on = 0;
						}
						if (time_on) {
							*dest++ = Octave_current;
							*dest++ = note - 1;
							*((unsigned __int16*)dest)++ = time_on;
							(MMLs_decoded.CHs + i)->time_total += time_on;
						}
						*dest++ = 0x80;
						*dest++ = 0;
						*((unsigned __int16*)dest)++ = time_off;
						(MMLs_decoded.CHs + i)->time_total += time_off;
						if (Octave_current != Octave) {
							Octave_current = Octave;
						}
					}
				}
				src++;

				//	wprintf_s(L"\n");
			}

			(MMLs_decoded.CHs + i)->len = dest - (MMLs_decoded.CHs + i)->MML;
			(MMLs_decoded.CHs + i)->Loop_delta = (MMLs_decoded.CHs + i)->time_total - (MMLs_decoded.CHs + i)->Loop_time;
			(MMLs_decoded.CHs + i)->Loop_len = (MMLs_decoded.CHs + i)->len - (MMLs_decoded.CHs + i)->Loop_address;

#if 0
			wprintf_s(L"MML Length %zu Time %zu Loop Start time %zu, Loop delta time %zu\n", (MMLs_decoded.CHs + i)->len, (MMLs_decoded.CHs + i)->time_total, (MMLs_decoded.CHs + i)->Loop_time, (MMLs_decoded.CHs + i)->Loop_delta);
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
		size_t loop_start_time = MMLs_decoded.CHs->Loop_time;
		size_t latest_CH = 0;
		unsigned no_loop = 1;

		// 各ループ時間の最小公倍数をとる
		for (size_t i = 0; i < CHs_real; i++) {
			// ループ展開後の長さの初期化
			(MMLs_decoded.CHs + i)->len_unrolled = (MMLs_decoded.CHs + i)->len;
			// 全チャンネルが非ループかチェック
			no_loop &= (MMLs_decoded.CHs + i)->Mute_on;
			// そもそもループしないチャネルはスキップ
			if ((MMLs_decoded.CHs + i)->Mute_on || (MMLs_decoded.CHs + i)->Loop_delta == 0) {
				continue;
			}
			delta_time_LCM = LCM(delta_time_LCM, (MMLs_decoded.CHs + i)->Loop_delta);
			// ループなしの最長時間割り出し
			if (max_time < (MMLs_decoded.CHs + i)->time_total) {
				max_time = (MMLs_decoded.CHs + i)->time_total;
			}
			// ループ開始が最後のチャネル割り出し
			if (loop_start_time < (MMLs_decoded.CHs + i)->Loop_time) {
				loop_start_time = (MMLs_decoded.CHs + i)->Loop_time;
				latest_CH = i;
			}
		}

		// 物によってはループするごとに微妙にずれていって元に戻るものもあり、極端なループ時間になる。(多分バグ)
		// あえてそれを回避せずに完全ループを生成するのでバッファはとても大きく取ろう。
		// 全チャンネルがループしないのならループ処理自体が不要
		if (no_loop) {
			wprintf_s(L"Loop: NONE %zu\n", max_time);
			end_time = max_time;
			loop_start_time = SIZE_MAX;
		}
		else {
			wprintf_s(L"Loop: Yes %zu\n", delta_time_LCM);
			end_time = loop_start_time + delta_time_LCM;
			for (size_t i = 0; i < CHs_real; i++) {
				// そもそもループしないチャネルはスキップ
				if ((MMLs_decoded.CHs + i)->Mute_on || (MMLs_decoded.CHs + i)->Loop_delta == 0) {
					continue;
				}
				unsigned __int8* src = (MMLs_decoded.CHs + i)->MML + (MMLs_decoded.CHs + i)->Loop_address;
				unsigned __int8* dest = (MMLs_decoded.CHs + i)->MML + (MMLs_decoded.CHs + i)->len;
				size_t len = (MMLs_decoded.CHs + i)->Loop_len;
				size_t time_extra = end_time - (MMLs_decoded.CHs + i)->Loop_time;
				size_t times = time_extra / (MMLs_decoded.CHs + i)->Loop_delta + !!(time_extra % (MMLs_decoded.CHs + i)->Loop_delta);
				(MMLs_decoded.CHs + i)->len_unrolled = (MMLs_decoded.CHs + i)->len + len * times;

				wprintf_s(L"%2zu: %9zu -> %9zu\n", i, (MMLs_decoded.CHs + i)->len, (MMLs_decoded.CHs + i)->len_unrolled);
			}
		}

		wprintf_s(L"Make Sequential events\n");

		// 得られた展開データからイベント列を作る。
		struct EVENT* pEVENTs = GC_malloc(sizeof(struct EVENT) * end_time * 3);
		struct EVENT* dest = pEVENTs;
		size_t counter = 0;
		size_t time_loop_start = 0;
		unsigned loop_enable = 0;
		for (size_t i = 0; i < CHs_real; i++) {
			size_t time = 0;
			size_t len = 0;
			while (len < (MMLs_decoded.CHs + i)->len_unrolled) {
				unsigned __int8* src = (MMLs_decoded.CHs + i)->MML + len;
				if ((latest_CH == i) && (src == ((MMLs_decoded.CHs + i)->MML + (MMLs_decoded.CHs + i)->Loop_address)) && (loop_start_time != SIZE_MAX)) {
					time_loop_start = time;
					loop_enable = 1;
				}
				while (src >= (MMLs_decoded.CHs + i)->MML + (MMLs_decoded.CHs + i)->len) {
					src -= (MMLs_decoded.CHs + i)->Loop_len;
				}

				unsigned __int8* src_orig = src;
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
					len += 4;
					break;
				case 0x80:
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					time += *((unsigned __int16*)src)++;
					dest->Type = 0;
					dest->CH = i;
					dest++;
					len += 4;
					break;
				case 0xE0: // Counter
				case 0xE1: // Velocity
				case 0xFC: // Detune
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					dest->Type = 20;
					dest->CH = i;
					dest++;
					len += 2;
					break;
				case 0xF5: // Tone select
				case 0xF9: // Volume change
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					dest->Type = 25;
					dest->CH = i;
					dest++;
					len += 2;
					break;
				case 0xEB: // Panpot
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					dest->Type = 27;
					dest->CH = i;
					dest++;
					len += 2;
					break;
				case 0xF4: // Tempo
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = *src++;
					dest->time = time;
					dest->Type = 10;
					dest->CH = i;
					dest++;
					len += 2;
					break;
				case 0xE9: // Tie
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param = 0;
					dest->time = time;
					dest->Type = 31;
					dest->CH = i;
					dest++;
					len++;
					break;
				case 0xE4: // LFO
					src += 4;
					break;
				default:
					wprintf_s(L"%zu: %2zu: How to reach ? %02X\n", src - (MMLs_decoded.CHs + i)->MML, i, *src);
					break;
				}
			}
		}

		// 出来上がった列の末尾に最大時間のマークをつける
		dest->time = SIZE_MAX;
		dest++;
		// イベント列をソートする
		qsort_s(pEVENTs, dest - pEVENTs, sizeof(struct EVENT), eventsort, NULL);

		// イベント列の長さを測る、時間になってもNote offが続くなら追加する
		size_t length_real = 0;
		while ((pEVENTs + length_real)->time < end_time) {
			length_real++;
		}
		while ((pEVENTs + length_real)->Event == 0x80) {
			length_real++;
		}
		wprintf_s(L"Event Length %8zu\n", length_real);

#if 1
		for (size_t i = 0; i < length_real; i++) {
			if ((pEVENTs + i)->Event == 0xE1) {
				wprintf_s(L"%8zu: %2d: %02X %02X\n", (pEVENTs + i)->time, (pEVENTs + i)->CH, (pEVENTs + i)->Event, (pEVENTs + i)->Param);
			}
		}
#endif

		wprintf_s(L"Make VGM\n");
		VGM_HEADER h_vgm = { FCC_VGM, 0, 0x171 };
		VGM_HDR_EXTRA eh_vgm = { sizeof(VGM_HDR_EXTRA), 0, sizeof(unsigned __int32) };
		VGMX_CHIP_DATA16 Ex_Vols;
		unsigned __int8 Ex_Vols_count = 0;
		unsigned CHs_limit;

		size_t vgm_header_len = sizeof(VGM_HEADER);
		size_t master_clock;

		// イベント列をvgm化する
		if (chip == YM2203) {
			CHs_limit = 6;
			h_vgm.lngHzYM2203 = master_clock = 3993600;
			h_vgm.bytAYFlagYM2203 = 0x1;
			Ex_Vols.Type = 0x86;
			Ex_Vols.Flags = 0;
			Ex_Vols.Data = 0x8099; // 0x8000 | 0x100 * 30/100 
//			Ex_Vols_count = 1;
			wprintf_s(L"YM2203 mode.\n");
		}
		else if (chip == YM2608) {
			CHs_limit = 9;
			h_vgm.lngHzYM2608 = master_clock = 7987200;
			h_vgm.bytAYFlagYM2608 = 0x1;
			Ex_Vols.Type = 0x87;
			Ex_Vols.Flags = 0;
			Ex_Vols.Data = 0x8080; // 0x8000 | 0x100 * 25/100 
//			Ex_Vols_count = 1;
			wprintf_s(L"YM2608 mode.\n");
		}
		else if (chip == YM2151) {
			CHs_limit = 8;
			chip = YM2151;
			h_vgm.lngHzYM2151 = master_clock = 4000000;
			wprintf_s(L"YM2151 mode.\n");
		}
		else {
			wprintf_s(L"Please select chip by -n, -a, -x options.\n");
			break;
		}

		unsigned __int8* vgm_out = GC_malloc(end_time * 10);
		unsigned __int8* vgm_pos = vgm_out;
		unsigned __int8* loop_pos = vgm_pos;
		size_t Tempo = 120;
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;
		size_t Time_Prev_VGM_abs = 0;
		size_t Time_Loop_VGM_abs = 0;
		unsigned __int8 vgm_command_chip[] = { 0x00, 0x55, 0x56, 0x54 };
		unsigned __int8 vgm_command_chip2[] = { 0x00, 0x55, 0x57, 0x54 };
		unsigned __int8 SSG_out = 0xBF;
		int Detune[9] = { 0 };
		unsigned Algorithm[9] = { 0 };
		unsigned Disable_note_off[9] = { 0 };

		unsigned __int8 Panpot_YM2151[8] = { 0x3, 0x3,  0x3,  0x3,  0x3,  0x3,  0x3,  0x3 };

		// 初期化
		if ((chip == YM2203)) {
			const static unsigned char Init_YM2203[] = {
				0x55, 0x00, 'W', 0x55, 0x00, 'A', 0x55, 0x00, 'O', 0x55, 0x27, 0x30, 0x55, 0x07, 0xBF,
				0x55, 0x90, 0x00, 0x55, 0x91, 0x00, 0x55, 0x92, 0x00, 0x55, 0x24, 0x70, 0x55, 0x25, 0x00 };

			memcpy_s(vgm_pos, sizeof(Init_YM2203), Init_YM2203, sizeof(Init_YM2203));
			vgm_pos += sizeof(Init_YM2203);
		}
		else if ((chip == YM2608)) {
			const static unsigned char Init_YM2608[] = {
				0x56, 0x00, 'W', 0x56, 0x00, 'A', 0x56, 0x00, 'O', 0x56, 0x27, 0x30, 0x56, 0x07, 0xBF,
				0x56, 0x90, 0x00, 0x56, 0x91, 0x00, 0x56, 0x92, 0x00, 0x56, 0x24, 0x70, 0x56, 0x25, 0x00, 0x56, 0x29, 0x83 };

			memcpy_s(vgm_pos, sizeof(Init_YM2608), Init_YM2608, sizeof(Init_YM2608));
			vgm_pos += sizeof(Init_YM2608);
		}
		else if (chip == YM2151) {
			const static unsigned char Init_YM2151[] = {
				0x54, 0x30, 0x00, 0x54, 0x31, 0x00, 0x54, 0x32, 0x00, 0x54, 0x33, 0x00, 0x54, 0x34, 0x00, 0x54, 0x35, 0x00,
				0x54, 0x36, 0x00, 0x54, 0x37, 0x00, 0x54, 0x38, 0x00, 0x54, 0x39, 0x00, 0x54, 0x3A, 0x00, 0x54, 0x3B, 0x00,
				0x54, 0x3C, 0x00, 0x54, 0x3D, 0x00, 0x54, 0x3E, 0x00, 0x54, 0x3F, 0x00, 0x54, 0x01, 0x00, 0x54, 0x18, 0x00,
				0x54, 0x19, 0x00, 0x54, 0x1B, 0x00, 0x54, 0x08, 0x00, 0x54, 0x08, 0x01, 0x54, 0x08, 0x02, 0x54, 0x08, 0x03,
				0x54, 0x08, 0x04, 0x54, 0x08, 0x05, 0x54, 0x08, 0x06, 0x54, 0x08, 0x07, 0x54, 0x60, 0x7F, 0x54, 0x61, 0x7F,
				0x54, 0x62, 0x7F, 0x54, 0x63, 0x7F, 0x54, 0x64, 0x7F, 0x54, 0x65, 0x7F, 0x54, 0x66, 0x7F, 0x54, 0x67, 0x7F,
				0x54, 0x68, 0x7F, 0x54, 0x69, 0x7F, 0x54, 0x6A, 0x7F, 0x54, 0x6B, 0x7F, 0x54, 0x6C, 0x7F, 0x54, 0x6D, 0x7F,
				0x54, 0x6E, 0x7F, 0x54, 0x6F, 0x7F, 0x54, 0x70, 0x7F, 0x54, 0x71, 0x7F, 0x54, 0x72, 0x7F, 0x54, 0x73, 0x7F,
				0x54, 0x74, 0x7F, 0x54, 0x75, 0x7F, 0x54, 0x76, 0x7F, 0x54, 0x77, 0x7F, 0x54, 0x78, 0x7F, 0x54, 0x79, 0x7F,
				0x54, 0x7A, 0x7F, 0x54, 0x7B, 0x7F, 0x54, 0x7C, 0x7F, 0x54, 0x7D, 0x7F, 0x54, 0x7E, 0x7F, 0x54, 0x7F, 0x7F,
				0x54, 0x43, 0x02, 0x54, 0x44, 0x02, 0x54, 0x45, 0x02, 0x54, 0x63, 0x0F, 0x54, 0x64, 0x0F, 0x54, 0x65, 0x0F,
				0x54, 0x83, 0x1F, 0x54, 0x84, 0x1F, 0x54, 0x85, 0x1F, 0x54, 0xA3, 0x00, 0x54, 0xA4, 0x00, 0x54, 0xA5, 0x00,
				0x54, 0xC3, 0x00, 0x54, 0xC4, 0x00, 0x54, 0xC5, 0x00, 0x54, 0xE3, 0x0F, 0x54, 0xE4, 0x0F, 0x54, 0xE5, 0x0F,
				0x54, 0x4B, 0x04, 0x54, 0x4C, 0x04, 0x54, 0x4D, 0x04, 0x54, 0x6B, 0x28, 0x54, 0x6C, 0x28, 0x54, 0x6D, 0x28,
				0x54, 0x8B, 0x1F, 0x54, 0x8C, 0x1F, 0x54, 0x8D, 0x1F, 0x54, 0xAB, 0x00, 0x54, 0xAC, 0x00, 0x54, 0xAD, 0x00,
				0x54, 0xCB, 0x00, 0x54, 0xCC, 0x00, 0x54, 0xCD, 0x00, 0x54, 0xEB, 0x0F, 0x54, 0xEC, 0x0F, 0x54, 0xED, 0x0F,
				0x54, 0x53, 0x06, 0x54, 0x54, 0x06, 0x54, 0x55, 0x06, 0x54, 0x73, 0x28, 0x54, 0x74, 0x28, 0x54, 0x75, 0x28,
				0x54, 0x93, 0x1F, 0x54, 0x94, 0x1F, 0x54, 0x95, 0x1F, 0x54, 0xB3, 0x00, 0x54, 0xB4, 0x00, 0x54, 0xB5, 0x00,
				0x54, 0xD3, 0x00, 0x54, 0xD4, 0x00, 0x54, 0xD5, 0x00, 0x54, 0xF3, 0x0F, 0x54, 0xF4, 0x0F, 0x54, 0xF5, 0x0F,
				0x54, 0x5B, 0x02, 0x54, 0x5C, 0x02, 0x54, 0x5D, 0x02, 0x54, 0x7B, 0x00, 0x54, 0x7C, 0x00, 0x54, 0x7D, 0x00,
				0x54, 0x9B, 0x1F, 0x54, 0x9C, 0x1F, 0x54, 0x9D, 0x1F, 0x54, 0xBB, 0x00, 0x54, 0xBC, 0x00, 0x54, 0xBD, 0x00,
				0x54, 0xDB, 0x00, 0x54, 0xDC, 0x00, 0x54, 0xDD, 0x00, 0x54, 0xFB, 0x0F, 0x54, 0xFC, 0x0F, 0x54, 0xFD, 0x0F,
				0x54, 0x23, 0xC3, 0x54, 0x24, 0xC3, 0x54, 0x25, 0xC3 };

			memcpy_s(vgm_pos, sizeof(Init_YM2151), Init_YM2151, sizeof(Init_YM2151));
			vgm_pos += sizeof(Init_YM2151);
		}

		for (struct EVENT* src = pEVENTs; (src - pEVENTs) <= length_real; src++) {
			if (src->time == SIZE_MAX || length_real == 0) {
				break;
			}
			if (src->CH >= CHs_limit) {
				continue;
			}
			if (src->time - Time_Prev) {
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
				// 実際のタイミングにするために9/10倍している。

				size_t c_VGMT = (src->time * 60 * VGM_CLOCK * 2 * 9 / (48 * Tempo * 10) + 1) >> 1;
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

			if ((src - pEVENTs) == length_real) {
				break;
			}

			if (loop_enable && src->time == time_loop_start && src->Type) {
				Time_Loop_VGM_abs = Time_Prev_VGM_abs;
				loop_pos = vgm_pos;
				loop_enable = 0;
			}

			switch (src->Event) {
			case 0xF4: // Tempo 注意!! ここが変わると累積時間も変わる!! 必ず再計算せよ!!
//				wprintf_s(L"%8zu: OLD tempo %zd total %zd\n", src->time, Tempo, Time_Prev_VGM);
				Time_Prev_VGM = ((Time_Prev_VGM * Tempo * 2) / src->Param + 1) >> 1;
				Tempo = src->Param;
				size_t NA;
				if (chip == YM2203) {
					NA = 1024 - (((master_clock * 2) / (192LL * src->Param) + 1) >> 1);
				}
				else if (chip == YM2608) {
					NA = 1024 - (((master_clock * 2) / (384LL * src->Param) + 1) >> 1);
				}
				else if (chip == YM2151) {
					NA = 1024 - (((3 * master_clock * 2) / (512LL * src->Param) + 1) >> 1);
				}
				// wprintf_s(L"%8zu: Tempo %zd NA %zd\n", src->time, Tempo, NA);
				if ((chip == YM2203) || (chip == YM2608)) {
					*vgm_pos++ = vgm_command_chip[chip];
					*vgm_pos++ = 0x24;
					*vgm_pos++ = (NA >> 2) & 0xFF;
					*vgm_pos++ = vgm_command_chip[chip];
					*vgm_pos++ = 0x25;
					*vgm_pos++ = NA & 0x03;
				}
				else if (chip == YM2151) {
					*vgm_pos++ = vgm_command_chip[chip];
					*vgm_pos++ = 0x10;
					*vgm_pos++ = (NA >> 2) & 0xFF;
					*vgm_pos++ = vgm_command_chip[chip];
					*vgm_pos++ = 0x11;
					*vgm_pos++ = NA & 0x03;
				}
				//				wprintf_s(L"%8zu: NEW tempo %zd total %zd\n", src->time, Tempo, Time_Prev_VGM);
				break;
			case 0xFC: // Detune
				Detune[src->CH] = (__int8)src->Param;
				break;
			case 0xEB: // Panpot
				if (chip == YM2608) {
					unsigned __int8 Panpot = 0xC0;
					if (src->Param & 0x80 || src->Param == 0) {
					}
					else if (src->Param < 64) {
						Panpot = 0x80;
					}
					else if (src->Param > 64) {
						Panpot = 0x40;
					}
					if (src->CH < 3) {
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xB4 + src->CH;
						*vgm_pos++ = Panpot;
					}
					else if (src->CH > 5) {
						*vgm_pos++ = vgm_command_chip2[chip];
						*vgm_pos++ = 0xB4 + src->CH - 6;
						*vgm_pos++ = Panpot;
					}
				}
				else if (chip == YM2151) {
					Panpot_YM2151[src->CH] = 0x3;
					if (src->Param & 0x80 || src->Param == 0) {
					}
					else if (src->Param < 64) {
						Panpot_YM2151[src->CH] = 0x2;
					}
					else if (src->Param > 64) {
						Panpot_YM2151[src->CH] = 0x1;
					}
				}
				break;
			case 0xF5: // Tone select
				if ((chip == YM2203) || (chip == YM2608)) {
					static unsigned __int8 Op_index[4] = { 0, 8, 4, 0xC };
					if (src->CH < 3) {
						Algorithm[src->CH] = (T + src->Param)->H.S.Connect;
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0xB0 + src->CH;
						*vgm_pos++ = (T + src->Param)->H.B;

						for (size_t op = 0; op < 4; op++) {
							for (size_t j = 0; j < 6; j++) {
								if (j == 1 && chip != YM2203 &&
									(Algorithm[src->CH] == 7 || Algorithm[src->CH] > 4 && op
										|| Algorithm[src->CH] > 3 && op >= 2 || op == 3)) {
								}
								else {
									*vgm_pos++ = vgm_command_chip[chip];
									*vgm_pos++ = 0x30 + 0x10 * j + Op_index[op] + src->CH;
									*vgm_pos++ = (T + src->Param)->Op[op].B[j];
								}
							}
						}

					}
					else if (src->CH > 5) {
						Algorithm[src->CH] = (T + src->Param)->H.S.Connect;
						*vgm_pos++ = vgm_command_chip2[chip];
						*vgm_pos++ = 0xB0 + src->CH - 6;
						*vgm_pos++ = (T + src->Param)->H.B;

						for (size_t op = 0; op < 4; op++) {
							for (size_t j = 0; j < 6; j++) {
								if (j == 1 &&
									(Algorithm[src->CH] == 7 || Algorithm[src->CH] > 4 && op
									|| Algorithm[src->CH] > 3 && op >= 2 || op == 3)) {
								}
								else {
									*vgm_pos++ = vgm_command_chip2[chip];
									*vgm_pos++ = 0x30 + 0x10 * j + Op_index[op] + src->CH - 6;
									*vgm_pos++ = (T + src->Param)->Op[op].B[j];
								}
							}
						}
					}
				}
				else if (chip == YM2151) {
					static unsigned __int8 Op_index[4] = { 0, 0x10, 8, 0x18 };
					if (src->CH < 8) {
						Algorithm[src->CH] = (T + src->Param)->H.S.Connect;
						(T + src->Param)->H.S.RL = Panpot_YM2151[src->CH];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x08;
						*vgm_pos++ = src->CH;

						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x20 + src->CH;
						*vgm_pos++ = (T + src->Param)->H.B;

						for (size_t op = 0; op < 4; op++) {
							for (size_t j = 0; j < 6; j++) {
								*vgm_pos++ = vgm_command_chip[chip];
								*vgm_pos++ = 0x40 + 0x20 * j + Op_index[op] + src->CH;
								*vgm_pos++ = (T + src->Param)->Op[op].B[j];
							}
						}
					}
				}
				break;
			case 0x80: // Note Off
				if (Disable_note_off[src->CH]) {
					Disable_note_off[src->CH] = 0;
				}
				else {
					if ((chip == YM2203) || (chip == YM2608)) {
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
						else if (src->CH > 5) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x28;
							*vgm_pos++ = (src->CH - 6) | 0x04;
						}
					}
					else if (chip == YM2151) {
						if (src->CH < 8) {
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x08;
							*vgm_pos++ = src->CH;
						}
					}
				}
				break;
			case 0xF9: // Volume change FMはアルゴリズムに合わせてスロット音量を変える仕様
				if ((chip == YM2203) || (chip == YM2608)) {
					if (src->CH < 3) {
						switch (Algorithm[src->CH]) {
						case 7:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x40 + src->CH;
							*vgm_pos++ = ~src->Param;
						case 5:
						case 6:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x44 + src->CH;
							*vgm_pos++ = ~src->Param;
						case 4:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x48 + src->CH;
							*vgm_pos++ = ~src->Param;
						case 0:
						case 1:
						case 2:
						case 3:
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
					else if (src->CH > 5) {
						switch (Algorithm[src->CH]) {
						case 7:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x40 + src->CH - 6;
							*vgm_pos++ = ~src->Param;
						case 5:
						case 6:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x44 + src->CH - 6;
							*vgm_pos++ = ~src->Param;
						case 4:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x48 + src->CH - 6;
							*vgm_pos++ = ~src->Param;
						case 0:
						case 1:
						case 2:
						case 3:
							*vgm_pos++ = vgm_command_chip2[chip];
							*vgm_pos++ = 0x4C + src->CH - 6;
							*vgm_pos++ = ~src->Param;
							break;
						default:
							wprintf_s(L"? How to reach ?\n");
						}
					}
				}
				else if (chip == YM2151) {
					if (src->CH < 8) {
						switch (Algorithm[src->CH]) {
						case 7:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x60 + src->CH;
							*vgm_pos++ = ~src->Param & 0x7F;
						case 5:
						case 6:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x68 + src->CH;
							*vgm_pos++ = ~src->Param & 0x7F;
						case 4:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x70 + src->CH;
							*vgm_pos++ = ~src->Param & 0x7F;
						case 0:
						case 1:
						case 2:
						case 3:
							*vgm_pos++ = vgm_command_chip[chip];
							*vgm_pos++ = 0x78 + src->CH;
							*vgm_pos++ = ~src->Param & 0x7F;
							break;
						default:
							wprintf_s(L"? How to reach ?\n");
						}
					}
				}
				break;
			case 0x00: // Note On
			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				if ((chip == YM2203) || (chip == YM2608)) {
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

						U.W = TP[src->Event][src->Param] + (-Detune[src->CH] >> 2);

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
					else if (src->CH > 5) {
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
						*vgm_pos++ = 0xA4 + src->CH - 6;
						*vgm_pos++ = U.B[1];
						*vgm_pos++ = vgm_command_chip2[chip];
						*vgm_pos++ = 0xA0 + src->CH - 6;
						*vgm_pos++ = U.B[0];
						*vgm_pos++ = vgm_command_chip[chip];
						*vgm_pos++ = 0x28;
						*vgm_pos++ = (src->CH - 6) | 0xF4;
					}
				}
				else if (chip == YM2151) {
					if (src->CH < 8) {
						unsigned key = src->Event * 12 + src->Param;
						if (key < 3) {
							wprintf_s(L"%zu: %2u: Very low key%2u\n", src->time, src->CH, key);
							key += 12;
						}
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
			case 0xE9: // Tie
				Disable_note_off[src->CH] = 1;
				break;
			case 0xE0: // Counter
			case 0xE1:
				break;
			}

		}

		*vgm_pos++ = 0x66;
		size_t vgm_dlen = vgm_pos - vgm_out;

		size_t vgm_extra_len = 0;
		size_t padsize = 11;
		if (Ex_Vols_count) {
			vgm_extra_len = sizeof(VGM_HDR_EXTRA) + 1 + sizeof(VGMX_CHIP_DATA16) + padsize;
		}

		size_t vgm_data_abs = vgm_header_len + vgm_extra_len;
		h_vgm.lngTotalSamples = Time_Prev_VGM_abs;
		h_vgm.lngDataOffset = vgm_data_abs - ((UINT8*)&h_vgm.lngDataOffset - (UINT8*)&h_vgm.fccVGM);
		h_vgm.lngExtraOffset = vgm_header_len - ((UINT8*)&h_vgm.lngExtraOffset - (UINT8*)&h_vgm.fccVGM);
		h_vgm.lngEOFOffset = vgm_data_abs + vgm_dlen - ((UINT8*)&h_vgm.lngEOFOffset - (UINT8*)&h_vgm.fccVGM);

		if (loop_start_time != SIZE_MAX) {
			h_vgm.lngLoopSamples = Time_Prev_VGM_abs - Time_Loop_VGM_abs;
			h_vgm.lngLoopOffset = vgm_data_abs + (loop_pos - vgm_out) - ((UINT8*)&h_vgm.lngLoopOffset - (UINT8*)&h_vgm.fccVGM);
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
		if (vgm_extra_len) {
			fwrite(&eh_vgm, 1, sizeof(VGM_HDR_EXTRA), pFo);
			if (Ex_Vols_count) {
				fwrite(&Ex_Vols_count, 1, 1, pFo);
				fwrite(&Ex_Vols, sizeof(VGMX_CHIP_DATA16), Ex_Vols_count, pFo);
			}
			UINT8 PADDING[15] = { 0 };
			fwrite(PADDING, 1, padsize, pFo);
		}
		fwrite(vgm_out, 1, vgm_dlen, pFo);
		wprintf_s(L"VGM body length %8zu\n", vgm_dlen);

		fclose(pFo);
	}
}