#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"

#include "MAKO2toVGM.h"
#include "MAKO2MML.h"
#include "event.h"

EVENTS::EVENTS(size_t elems, size_t end)
{
	this->events = elems;
	this->time_end = end;
	this->time_loop_start = 0;

	this->event = (struct EVENT *) GC_malloc(sizeof(struct EVENT) * elems);
	if (this->event == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}
	this->dest = this->event;
	counter = 0;
}

struct EVENT* EVENTS::enlarge(size_t len_cur)
{
	this->events += this->time_end;

	this->event = (struct EVENT*)GC_realloc(this->event, sizeof(struct EVENT) * events);
	if (this->event == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	this->dest = this->event + len_cur;
	return this->event;
}

int eventsort(void* x, const void* n1, const void* n2)
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

void EVENTS::convert(struct mako2_mml_decoded& MMLs)
{
	unsigned loop_enable = 0;

	for (size_t j = 0; j < MMLs.CHs; j++) {
		size_t i = MMLs.CHs - 1 - j;

		size_t time = 0;
		size_t len = 0;
		bool Disable_note_off = false;
		while (len < (MMLs.CH + i)->len_unrolled) {
			size_t length_current = this->dest - this->event;
			if (length_current == this->events) {
				this->enlarge(length_current);
				wprintf_s(L"Memory reallocated in making sequential.\n");
			}

			unsigned __int8* src = (MMLs.CH + i)->MML + len;
			if ((MMLs.latest_CH == i) && (src == ((MMLs.CH + i)->MML + (MMLs.CH + i)->Loop_start_pos)) && (MMLs.loop_start_time != SIZE_MAX)) {
				time_loop_start = time;
				loop_enable = 1;
			}
			while (src >= (MMLs.CH + i)->MML + (MMLs.CH + i)->len) {
				src -= (MMLs.CH + i)->len - (MMLs.CH + i)->Loop_start_pos;
			}

			unsigned __int8* src_orig = src;
			switch (*src) {
			case 0x80: // Note off
				if (Disable_note_off) {
					Disable_note_off = false;
					src++;
				}
				else {
					dest->Count = counter++;
					dest->Event = *src++;
					dest->time = time;
					dest->Type = 0;
					dest->CH = i;
					dest++;
				}
				time += *((unsigned __int16*)src);
				src += 2;
				break;
			case 0xE9: // Tie
				Disable_note_off = true;
				src++;
				break;
			case 0xE0: // Counter
			case 0xE1: // Velocity
			case 0xFC: // Detune
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.B[0] = *src++;
				dest->time = time;
				dest->Type = 20;
				dest->CH = i;
				dest++;
				break;
			case 0xE5: // set flags 4 5 6
			case 0xF5: // Tone select
			case 0xF9: // Volume change
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.B[0] = *src++;
				dest->time = time;
				dest->Type = 25;
				dest->CH = i;
				dest++;
				break;
			case 0xEB: // Panpot
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.B[0] = *src++;
				dest->time = time;
				dest->Type = 27;
				dest->CH = i;
				dest++;
				break;
			case 0xF4: // Tempo
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.B[0] = *src++;
				dest->time = time;
				dest->Type = 10;
				dest->CH = i;
				dest++;
				break;
			case 0xE4: // hLFO
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.B[0] = *src++;
				dest->Param.B[1] = *src++;
				dest->Param.B[2] = *src++;
				dest->time = time;
				dest->Type = 26;
				dest->CH = i;
				dest++;
				break;
			case 0xE7: // sLFOv
			case 0xE6: // sLFOd
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.W[0] = *((unsigned __int16*)src);
				src += 2;
				dest->Param.W[1] = *((unsigned __int16*)src);
				src += 2;
				dest->Param.W[2] = *((unsigned __int16*)src);
				src += 2;
				dest->Param.W[3] = *((unsigned __int16*)src);
				src += 2;
				dest->time = time;
				dest->Type = 25;
				dest->CH = i;
				dest++;
				break;
			case 0xE8: // sLFOv
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.W[0] = *((unsigned __int16*)src);
				src += 2;
				dest->Param.W[1] = *((unsigned __int16*)src);
				src += 2;
				dest->Param.W[2] = *((unsigned __int16*)src);
				src += 2;
				dest->Param.W[3] = *((unsigned __int16*)src);
				src += 2;
				dest->Param.W[4] = *((unsigned __int16*)src);
				src += 2;
				dest->time = time;
				dest->Type = 25;
				dest->CH = i;
				dest++;
				break;
			case 0x90: // Note on
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param.B[0] = *src++;
				dest->time = time;
				time += *((unsigned __int16*)src);
				src += 2;
				dest->Type = 30;
				dest->CH = i;
				dest++;
				break;
			default:
				wprintf_s(L"%zu: %2zu: How to reach ? %02X %02X %02X %02X %02X %02X %02X %02X,%02X %02X\n", src - (MMLs.CH + i)->MML, i, *(src - 8), *(src - 7), *(src - 6), *(src - 5), *(src - 4), *(src - 3), *(src - 2), *(src - 1), *src, *(src + 1));
				break;
			}
			len += src - src_orig;
		}
	}

	// 出来上がった列の末尾に最大時間のマークをつける
	dest->time = SIZE_MAX;
	dest++;
}

void EVENTS::sort(void)
{
	// イベント列をソートする
	qsort_s(this->event, this->dest - this->event, sizeof(struct EVENT), eventsort, NULL);

	// イベント列の長さを測る。もしループ末尾がループ入り口直前と一致するならループ起終点を繰り上げる
	size_t length_real = 0;
	size_t loop_start_index = SIZE_MAX;
	while ((this->event + length_real)->time < this->time_end) {
		if ((this->event + length_real)->time >= this->time_loop_start) {
			if (loop_start_index == SIZE_MAX) {
				loop_start_index = length_real;
			}
		}
		length_real++;
	}
	if (loop_start_index != SIZE_MAX) {
		while (loop_start_index > 0
			&& (this->event + length_real)->CH == (this->event + loop_start_index)->CH
			&& (this->event + length_real)->Event == (this->event + loop_start_index)->Event
			&& (this->event + length_real)->Param.B[0] == (this->event + loop_start_index)->Param.B[0]
			&& (this->event + length_real)->time - (this->event + length_real - 1)->time == (this->event + loop_start_index)->time - (this->event + loop_start_index - 1)->time
			)
		{
			length_real--;
			loop_start_index--;
		}
	}

	wprintf_s(L"Event Length %8zu Loop from %zu\n", length_real, loop_start_index);

}