#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"

#include "MAKO2toVGM.h"
#include "MAKO2MML.h"
#include "event.h"

EVENTS::EVENTS(size_t elems, size_t elems_delta)
{
	this->events = elems;
	this->delta = elems_delta;

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
	this->events += this->delta;

	this->event = (struct EVENT*)GC_realloc(this->event, sizeof(struct EVENT) * events);
	if (this->event == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	this->dest = this->event + len_cur;
	return this->event;
}


void EVENTS::convert(struct mako2_mml_decoded& MMLs)
{
	size_t time_loop_start = 0;
	unsigned loop_enable = 0;

	for (size_t j = 0; j < MMLs.CHs; j++) {
		size_t i = MMLs.CHs - 1 - j;

		size_t time = 0;
		size_t len = 0;
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
				dest->Count = counter++;
				dest->Event = *src++;
				dest->time = time;
				time += *((unsigned __int16*)src);
				src += 2;
				dest->Type = 0;
				dest->CH = i;
				dest++;
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
			case 0xE9: // Tie
				dest->Count = counter++;
				dest->Event = *src++;
				dest->time = time;
				dest->Type = 31;
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
