#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"

#include "MAKO2toVGM.h"
#include "MAKO2MML.h"
#include "PCMtoWAVE.h"
#include "../aclib/tools.h"

mako2_mml_decoded_CH::mako2_mml_decoded_CH()
{
	MML = (unsigned __int8*)GC_malloc(32 * 1024);
}

void mako2_mml_decoded_CH::mute_on(void)
{
	this->Mute_on = true;
}

bool mako2_mml_decoded_CH::is_mute(void)
{
	return this->Mute_on;
}

void mako2_mml_decoded_CH::decode(unsigned __int8* input, unsigned __int32 offs)
{
	if (!offs) {
		this->mute_on();
		return;
	}

	size_t Blocks = 0;
	unsigned __int16* pBlock_offset = (unsigned __int16*)(input + offs);
	unsigned __int16* pBlock_offset_work = pBlock_offset;
	while ((*pBlock_offset_work & 0xFF00) != 0xFF00) {
		Blocks++;
		pBlock_offset_work++;
	}
	size_t Loop_Block = *pBlock_offset_work & 0xFF;


	unsigned __int16 Octave = 4, Octave_current = 4;
	unsigned __int16 time_default = 0x40, note;
	__int16 time, time_on, time_off, gate_step = 7;
	bool flag_gate = false;
	bool tie = false;
	unsigned __int8* dest = this->MML;

	this->Loop_start_pos = 0;
	this->Loop_start_time = 0;

	for (size_t j = 0; j < Blocks; j++) {
		if (j == Loop_Block) {
			this->Loop_start_pos = dest - this->MML;
			this->Loop_start_time = this->time_total;
		}

		unsigned __int8* src = input + *(pBlock_offset + j);

		//				wprintf_s(L"%2zu: ", j);
		while (*src != 0xFF) {
//			wprintf_s(L"%02X ", *src);
			bool makenote = false;
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
				makenote = true;
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
				time = *((unsigned __int16*)src);
				src += 2;
				makenote = true;
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
				makenote = true;
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
			case 0xF2:
				flag_gate = false;
				gate_step = (short)(*++src) + 1;
				src++;
				break;
			case 0xEA:
				flag_gate = true;
				gate_step = *++src;
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

			case 0xFA: // none
			case 0xFD: // none
				wprintf_s(L"%02X\n", *src);
				src += 2;
				break;

			case 0xFE: // none
				wprintf_s(L"%02X\n", *src);
				src += 4;
				break;

			case 0xE0: // Counter
			case 0xE1: // Velocity
			case 0xF4: // Tempo
			case 0xF5: // Tone select
			case 0xF9: // Volume change
			case 0xFC: // Detune
				memcpy_s(dest, 2, src, 2);
				dest += 2;
				src += 2;
				break;

			case 0xEC:
				this->Mute_on = 1;
				src++;
				note = 0;
				time = 0;
				makenote = true;
				break;

			case 0xE5: // set flags 4 5 6
			case 0xEB: // Panpot
				memcpy_s(dest, 2, src, 2);
				dest += 2;
				src += 2;
				break;

			case 0xE9: // Tie
				*dest++ = *src++;
				tie = true;
				break;

			case 0xE7:
			case 0xE6:
				memcpy_s(dest, 9, src, 9);
				dest += 9;
				src += 9;
				break;

			case 0xE8:
				memcpy_s(dest, 11, src, 11);
				dest += 11;
				src += 11;
				break;

			case 0xE4: // hLFO
				memcpy_s(dest, 4, src, 4);
				dest += 4;
				src += 4;
				break;

			default:
				src++;
			}
			// 音程表現はオクターブ*12+音名の8bit(0-95)とする。MIDIノートナンバーから12引いた値となる。
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
				if (!note || (time_on == 0)) {
					*dest++ = 0x80;
					*((unsigned __int16*)dest) = time;
					dest += 2;
				}
				else {
					*dest++ = 0x90;
					*dest++ = Octave_current * 12 + note - 1; // 0 - 95 (MIDI Note No.12 - 107)
					*((unsigned __int16*)dest) = time_on;
					dest += 2;
					*((unsigned __int16*)dest) = time_off;
					dest += 2;
				}
				this->time_total += time;
				if (Octave_current != Octave) {
					Octave_current = Octave;
				}
			}
		}
		//				wprintf_s(L"\n");
	}
	this->len = dest - this->MML;
	this->Loop_delta_time = this->time_total - this->Loop_start_time;
}

mako2_mml_decoded::mako2_mml_decoded(size_t ch)
{
	CHs = ch;
	CH = new class mako2_mml_decoded_CH[CHs];
}

void mako2_mml_decoded::unroll_loop(void)
{
	// ループを展開し、全チャネルが同一長のループになるように調整する。
	bool debug = false;
	size_t delta_time_LCM = 1;
	size_t max_time = 0;
	bool no_loop = true;

	// 各ループ時間の最小公倍数をとる
	for (size_t i = 0; i < this->CHs; i++) {
		// ループ展開後の長さの初期化
		(this->CH + i)->len_unrolled = (this->CH + i)->len;
		// ループなしの最長時間割り出し
		if (max_time < (this->CH + i)->time_total) {
			max_time = (this->CH + i)->time_total;
		}
		// 全チャンネルが非ループかチェック
		no_loop &= (this->CH + i)->is_mute();
		// そもそもループしないチャネルはスキップ
		if ((this->CH + i)->is_mute() || (this->CH + i)->Loop_delta_time == 0) {
			continue;
		}
		delta_time_LCM = LCM(delta_time_LCM, (this->CH + i)->Loop_delta_time);
		// ループ開始が最後のチャネル割り出し
		if (this->loop_start_time < (this->CH + i)->Loop_start_time) {
			this->loop_start_time = (this->CH + i)->Loop_start_time;
			this->latest_CH = i;
		}
	}

	// 物によってはループするごとに微妙にずれていって元に戻るものもあり、極端なループ時間になる。(多分バグ)
	// あえてそれを回避せずに完全ループを生成するのでバッファはとても大きく取ろう。
	// 全チャンネルがループしないのならループ処理自体が不要
	if (no_loop) {
		wprintf_s(L"Loop: NONE %zu\n", max_time);
		this->end_time = max_time;
		this->loop_start_time = SIZE_MAX;
	}
	else {
		wprintf_s(L"Loop: Yes %zu Start %zu\n", delta_time_LCM, this->loop_start_time);
		this->end_time = this->loop_start_time + delta_time_LCM;
		for (size_t i = 0; i < this->CHs; i++) {
			// そもそもループしないチャネルはスキップ
			if ((this->CH + i)->is_mute() || (this->CH + i)->Loop_delta_time == 0) {
				continue;
			}
			size_t time_extra = this->end_time - (this->CH + i)->Loop_start_time;
			size_t times = time_extra / (this->CH + i)->Loop_delta_time + !!(time_extra % (this->CH + i)->Loop_delta_time);
			(this->CH + i)->len_unrolled = (this->CH + i)->len * (times + 1) - (this->CH + i)->Loop_start_pos * times;
			if (debug) {
				wprintf_s(L"%2zu: %9zu -> %9zu : loop from %zu(%zu)\n", i, (this->CH + i)->len, (this->CH + i)->len_unrolled, (this->CH + i)->Loop_start_pos, (this->CH + i)->Loop_start_time);
			}
		}
	}
}
