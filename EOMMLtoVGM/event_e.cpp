#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"

#include "EOMML.h"
#include "event_e.h"

EVENTS::EVENTS(size_t elems, size_t end)
{
	this->events = elems;
	this->time_end = end;

	this->event = (struct EVENT*)GC_malloc(sizeof(struct EVENT) * elems);
	if (this->event == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}
	this->dest = this->event;
	counter = 0;
}

void EVENTS::enlarge(void)
{
	size_t length_current = this->dest - this->event;
	this->events += this->time_end;

	this->event = (struct EVENT*)GC_realloc(this->event, sizeof(struct EVENT) * events);
	if (this->event == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	this->dest = this->event + length_current;
}

static int eventsort_noweight(void* x, const void* n1, const void* n2)
{
	if (((struct EVENT*)n1)->time > ((struct EVENT*)n2)->time) {
		return 1;
	}
	else if (((struct EVENT*)n1)->time < ((struct EVENT*)n2)->time) {
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

static int eventsort(void* x, const void* n1, const void* n2)
{
	unsigned CH_weight[16] = { 0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15 };
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
			if (CH_weight[((struct EVENT*)n1)->CH] > CH_weight[((struct EVENT*)n2)->CH]) {
				return -1;
			}
			else if (CH_weight[((struct EVENT*)n1)->CH] < CH_weight[((struct EVENT*)n2)->CH]) {
				return 1;
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
}

void EVENTS::convert(struct eomml_decoded& MMLs)
{
	this->loop_enable = false;

	for (size_t j = 0; j < MMLs.CHs; j++) {
		size_t i = MMLs.CHs - 1 - j;

		size_t time = 0;
		size_t len = 0;
		while (len < (MMLs.CH + i)->len_unrolled) {
			size_t length_current = this->dest - this->event;
			if (length_current + 200 >= this->events) {
				this->enlarge();
				wprintf_s(L"Memory reallocated in making sequential.\n");
			}

			unsigned __int8* src = (MMLs.CH + i)->MML + len;
			if (MMLs.loop_start_time != SIZE_MAX) {
				this->loop_enable = true;
			}
			while (src >= (MMLs.CH + i)->MML + (MMLs.CH + i)->len) {
				src -= (MMLs.CH + i)->len;
			}

			unsigned __int8* src_orig = src;
			size_t len_On, len_Off;
			switch (*src) {
			case 0x80: // Note off
				dest->Count = counter++;
				dest->Event = *src++;
				dest->time = time;
				dest->Type = 0;
				dest->CH = i;
				dest++;
				len_Off = *src++;
				time += len_Off;
				break;
			case 0xF5: // Tone select
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param = *src++;
				dest->time = time;
				dest->Type = 2;
				dest->CH = i;
				dest++;
				break;
			case 0xF9: // Volume change (0-127)
			case 0xF8: // Volume change (0-8)
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param = *src++;
				dest->time = time;
				dest->Type = 3;
				dest->CH = i;
				dest++;
				break;
			case 0xF4: // Tempo
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param = *src++;
				dest->time = time;
				dest->Type = 1;
				dest->CH = i;
				dest++;
				break;
			case 0x90: // Note on
				dest->Count = counter++;
				dest->Event = *src++;
				dest->time = time;
				dest->Type = 9;
				dest->CH = i;
				dest++;

				dest->Count = counter++;
				dest->Event = 0x97;
				dest->Param = *src++;
				dest->time = time;
				dest->Type = 2;
				dest->CH = i;
				dest++;
				len_On = *src++;
				len_Off = *src++;

				time += len_On;

				dest->Count = counter++;
				dest->Event = 0x80;
				dest->time = time;
				dest->Type = 0;
				dest->CH = i;
				dest++;

				time += len_Off;

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


	// イベント列の長さを測る。
	while ((this->event + this->length)->time < this->time_end) {
		if (this->loop_start == SIZE_MAX) {
			this->loop_start = this->length;
		}
		this->length++;
	}

#if 1
	// 重複イベントを削除し、ソートしなおす
	size_t length_work = this->length;
	for (size_t i = 0; i < length_work - 1; i++) {
		if ((this->event + i)->time == (this->event + i + 1)->time && (this->event + i)->Event == (this->event + i + 1)->Event) {
			if ((this->event + i)->Event == 0xF4 && (this->event + i)->Param == (this->event + i + 1)->Param) {
				(this->event + i)->time = SIZE_MAX;
				if (i < this->loop_start) {
					this->loop_start--;
				}
				this->length--;
			}
		}
	}
	qsort_s(this->event, this->dest - this->event, sizeof(struct EVENT), eventsort, NULL);
#endif

	wprintf_s(L"Event Length %8zu Loop from %zu\n", this->length, this->loop_start);

}

void EVENTS::print_all(void)
{
	for (size_t i = 0; i < this->length; i++) {
		wprintf_s(L"%8zu: %2d: %02X\n", (event + i)->time, (event + i)->CH, (event + i)->Event);
	}
}