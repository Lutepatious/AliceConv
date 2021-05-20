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

int eventsort_noweight(void* x, const void* n1, const void* n2)
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

int eventsort(void* x, const void* n1, const void* n2)
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

void EVENTS::sLFOv_setup_FM(void)
{
	if (this->sLFOv_ready) {
		this->sLFOv_FM.Wait = this->sLFOv_FM.Param.Wait1;
		this->sLFOv_FM.Volume = 0;
		if (this->sLFOv_direction) {
			this->sLFOv_FM.Param.Delta1 = -this->sLFOv_FM.Param.Delta1;
			this->sLFOv_FM.Param.Limit = -this->sLFOv_FM.Param.Limit;
			this->sLFOv_direction = false;
		}
	}
}

void EVENTS::sLFOv_setup_SSG(void)
{
	if (this->sLFOv_ready) {
		this->sLFOv_SSG.Mode = 1;
		this->sLFOv_SSG.Volume = this->sLFOv_SSG.Delta = this->sLFOv_SSG.Param.Volume;
	}
}


__int8 EVENTS::sLFOv_exec_FM(void)
{
	if (this->sLFOv_FM.Wait) {
		this->sLFOv_FM.Wait--;
		return this->Volume_current + this->sLFOv_FM.Volume;
	}
	this->sLFOv_FM.Wait = this->sLFOv_FM.Param.Wait2;
	this->sLFOv_FM.Volume += this->sLFOv_FM.Param.Delta1;

	if (this->sLFOv_FM.Param.Delta1 >= 0) {
		if (this->sLFOv_FM.Volume >= this->sLFOv_FM.Param.Limit) {
			this->sLFOv_FM.Volume = this->sLFOv_FM.Param.Limit;
			this->sLFOv_FM.Param.Delta1 = -this->sLFOv_FM.Param.Delta1;
			this->sLFOv_FM.Param.Limit = -this->sLFOv_FM.Param.Limit;
			this->sLFOv_direction = true;
		}
	}
	else {
		if (this->sLFOv_FM.Volume <= this->sLFOv_FM.Param.Limit) {
			this->sLFOv_FM.Volume = this->sLFOv_FM.Param.Limit;
			this->sLFOv_FM.Param.Delta1 = -this->sLFOv_FM.Param.Delta1;
			this->sLFOv_FM.Param.Limit = -this->sLFOv_FM.Param.Limit;
			this->sLFOv_direction = false;
		}
	}
	return this->Volume_current + this->sLFOv_FM.Volume;
}

__int8 EVENTS::sLFOv_exec_SSG(void)
{
	if (this->sLFOv_SSG.Mode == 1) { // 音量増
		if (this->sLFOv_SSG.Volume >= 0x3FFF) {
			this->sLFOv_SSG.Volume = 0x4000;
			this->sLFOv_SSG.Volume += this->sLFOv_SSG.Delta;
			this->sLFOv_SSG.Wait = this->sLFOv_SSG.Param.Wait1;
			this->sLFOv_SSG.Delta = this->sLFOv_SSG.Param.Delta1;
			this->sLFOv_SSG.Mode = 2;
		}
		else {
			this->sLFOv_SSG.Volume += this->sLFOv_SSG.Delta;
		}
		if (this->sLFOv_SSG.Volume > 0x4000) {
			this->sLFOv_SSG.Volume = 0x4000;
		}
	}
	else if (this->sLFOv_SSG.Mode > 1 && this->sLFOv_SSG.Mode < 5) { // 音量減
		this->sLFOv_SSG.Volume -= this->sLFOv_SSG.Delta;
		if (this->sLFOv_SSG.Volume < 0) {
			this->sLFOv_SSG.Volume = 0;
			this->sLFOv_SSG.Mode = 0;
		}
		if (this->sLFOv_SSG.Mode == 2) {
			if (this->sLFOv_SSG.Wait) {
				this->sLFOv_SSG.Delta = this->sLFOv_SSG.Param.Delta2;
				this->sLFOv_SSG.Mode = 3;
			}
			this->sLFOv_SSG.Wait--;
		}
	}
	return (int)this->Volume_current * this->sLFOv_SSG.Volume >> 14;
}

void EVENTS::sLFOd_setup(void)
{
	if (this->sLFOd_ready) {
		this->sLFOd.Wait = this->sLFOd.Param.Wait1;
		this->sLFOd.Detune = 0;
		if (this->sLFOd_direction) {
			this->sLFOd.Param.Delta1 = -this->sLFOd.Param.Delta1;
			this->sLFOd.Param.Limit = -this->sLFOd.Param.Limit;
			this->sLFOd_direction = false;
		}
	}
}

__int16 EVENTS::sLFOd_exec(void)
{
	if (this->sLFOd.Wait) {
		this->sLFOd.Wait--;
		return this->Detune_current + this->sLFOd.Detune;
	}

	this->sLFOd.Wait = this->sLFOd.Param.Wait2;
	this->sLFOd.Detune += this->sLFOd.Param.Delta1;

	if (this->sLFOd.Param.Delta1 >= 0) {
		if (this->sLFOd.Detune >= this->sLFOd.Param.Limit) {
			this->sLFOd.Detune = this->sLFOd.Param.Limit;
			this->sLFOd.Param.Delta1 = -this->sLFOd.Param.Delta1;
			this->sLFOd.Param.Limit = -this->sLFOd.Param.Limit;
			this->sLFOd_direction = true;
		}
	}
	else {
		if (this->sLFOd.Detune <= this->sLFOd.Param.Limit) {
			this->sLFOd.Detune = this->sLFOd.Param.Limit;
			this->sLFOd.Param.Delta1 = -this->sLFOd.Param.Delta1;
			this->sLFOd.Param.Limit = -this->sLFOd.Param.Limit;
			this->sLFOd_direction = false;
		}
	}
	return this->Detune_current + this->sLFOd.Detune;
}

void EVENTS::init(void)
{
	this->Volume_current = 0;
	this->Detune_current = 0;
	this->Volume_prev = 0;
	this->Detune_prev = 0;

	this->Disable_note_off = false;
	this->Disable_LFO_init = false;

	this->sLFOv_ready = false;
	this->sLFOd_ready = false;
	this->sLFOv_direction = false;
	this->sLFOd_direction = false;
#if 0
	this->sLFOd_first_wait = true;
#endif
	this->sLFOv_SSG = { { 0, 0, 0, 0, 0 }, 0, 0, 0, 0 };
	this->sLFOv_FM = { { 0, 0, 0, 0}, 0, 0 };
	this->sLFOd = { { 0, 0, 0, 0 }, 0, 0 };

}

void EVENTS::LFO_note_off(void)
{
	if (sLFOv_ready) {
		if (sLFOv_SSG.Mode != 0 && sLFOv_SSG.Mode < 4) {
			this->sLFOv_SSG.Delta = this->sLFOv_SSG.Param.Delta_last;
			sLFOv_SSG.Mode = 4;
		}
	}
}

void EVENTS::convert(struct mako2_mml_decoded& MMLs, bool direction)
{
	this->loop_enable = false;

	for (size_t j = 0; j < MMLs.CHs; j++) {
		this->init();
		size_t i = MMLs.CHs - 1 - j;
		if (direction) {
			i = j;
		}

		size_t time = 0;
		size_t len = 0;
		while (len < (MMLs.CH + i)->len_unrolled) {
			size_t length_current = this->dest - this->event;
			if (length_current + 400 >= this->events) {
				this->enlarge();
				wprintf_s(L"Memory reallocated in making sequential.\n");
			}

			unsigned __int8* src = (MMLs.CH + i)->MML + len;
			if ((MMLs.latest_CH == i) && (src == ((MMLs.CH + i)->MML + (MMLs.CH + i)->Loop_start_pos)) && (MMLs.loop_start_time != SIZE_MAX)) {
				this->time_loop_start = time;
				this->loop_enable = true;
			}
			while (src >= (MMLs.CH + i)->MML + (MMLs.CH + i)->len) {
				src -= (MMLs.CH + i)->len - (MMLs.CH + i)->Loop_start_pos;
			}

			unsigned __int8* src_orig = src;
			size_t len_On, len_Off;
			switch (*src) {
			case 0x80: // Note off
				len_Off = *(unsigned __int16*)(src + 1);
				this->LFO_note_off();
				dest->Count = counter++;
				dest->Event = *src++;
				dest->time = time;
				dest->Type = 0;
				dest->CH = i;
				dest++;
				time += len_Off;
				src += 2;
				break;
			case 0xE9: // Tie
//				wprintf_s(L"Tie      %1zX: %08zX\n", i, time);
				this->Disable_note_off = true;
				src++;
				break;
			case 0xE0: // Counter
				src += 2;
				break;
			case 0xE5: // set flags 4 5 6
				src++;
				this->sLFOd_ready = !!(*src & 0x1);
				this->sLFOv_ready = !!(*src & 0x2);
				src++;
				break;
			case 0xF5: // Tone select
			case 0xEB: // Panpot
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param[0] = *src++;
				dest->time = time;
				dest->Type = 2;
				dest->CH = i;
				dest++;
				break;
			case 0xFC: // Detune
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param[0] = *src++;
				dest->time = time;
				dest->Type = 2;
				dest->CH = i;
				dest++;
				this->Detune_current = (__int8) dest->Param[0];
				break;
			case 0xE1: // Velocity
				if (this->sLFOv_ready) {
					src++;
					this->Volume_current += *src++;
				}
				else {
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param[0] = *src++;
					dest->time = time;
					dest->Type = 2;
					dest->CH = i;
					dest++;
				}
				break;
			case 0xF9: // Volume change
				if (this->sLFOv_ready) {
					src++;
					this->Volume_current = *src++;
				}
				else {
					dest->Count = counter++;
					dest->Event = *src++;
					dest->Param[0] = this->Volume_current = *src++;
					dest->time = time;
					dest->Type = 2;
					dest->CH = i;
					dest++;
				}
				break;
			case 0xF4: // Tempo
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param[0] = *src++;
				dest->time = time;
				dest->Type = 1;
				dest->CH = i;
				dest++;
				break;
			case 0xE4: // hLFO
				dest->Count = counter++;
				dest->Event = *src++;
				dest->Param[0] = *src++;
				dest->Param[1] = *src++;
				dest->Param[2] = *src++;
				dest->time = time;
				dest->Type = 2;
				dest->CH = i;
				dest++;
				break;
			case 0xE7: // sLFOd
				src++;
				this->sLFOd.Param = *((struct LFO_soft_detune*)src);
				src += sizeof(struct LFO_soft_detune);
				this->sLFOd_ready = true;
				this->sLFOd_direction = false;
				this->sLFOd_setup();
				wprintf_s(L"sLFO_detune %1zX: %08zX: w%04X %04X d%04X l%04X: %04X %04X\n", i, time, this->sLFOd.Param.Wait1, this->sLFOd.Param.Wait2, this->sLFOd.Param.Delta1, this->sLFOd.Param.Limit, this->sLFOd.Wait, this->sLFOd.Detune);
				break;
			case 0xE6: // sLFOv
				src++;
				this->sLFOv_FM.Param = *((struct LFO_soft_volume_FM*)src);
				src += sizeof(struct LFO_soft_volume_FM);
				this->sLFOv_ready = true;
				this->sLFOv_FM.Wait = this->sLFOv_FM.Param.Wait1;
				this->sLFOv_FM.Volume = 0;
				this->sLFOv_direction = false;
				this->sLFOv_setup_FM();
				wprintf_s(L"sLFOv_FM    %1zX: %08zX: w%04X %04X d%04X l%04X\n", i, time, this->sLFOv_FM.Param.Wait1, this->sLFOv_FM.Param.Wait2, this->sLFOv_FM.Param.Delta1, this->sLFOv_FM.Param.Limit);
				break;
			case 0xE8: // sLFOv
				src++;
				this->sLFOv_SSG.Param = *((struct LFO_soft_volume_SSG*)src);
				src += sizeof(struct LFO_soft_volume_SSG);
				this->sLFOv_ready = true;
				wprintf_s(L"sLFOv_SSG   %1zX: %08zX: v%04X w%04X d%04X %04X %04X\n", i, time, this->sLFOv_SSG.Param.Volume, this->sLFOv_SSG.Param.Wait1, this->sLFOv_SSG.Param.Delta1, this->sLFOv_SSG.Param.Delta2, this->sLFOv_SSG.Param.Delta_last);
				break;
			case 0x90: // Note on
				len_On = *(unsigned __int16*)(src + 2);
				len_Off = *(unsigned __int16*)(src + 4);
				dest->Count = counter++;
				dest->Event = *src++;
				dest->time = time;
				dest->Type = 9;
				dest->CH = i;
				dest++;

				dest->Count = counter++;
				dest->Event = 0x97;
				dest->Param[0] = *src++;
				dest->time = time;
				dest->Type = 2;
				dest->CH = i;
				dest++;
				src += 4;

				this->Detune_prev = this->Detune_current;

				if (this->Disable_LFO_init) {
					this->Disable_LFO_init = false;
				}
				else {
					this->sLFOd_setup();

#if 0
					if (this->sLFOd_first_wait) {
						this->sLFOd_first_wait = false;
						this->sLFOd.Wait++;
					}
#endif
					if (i < 3 || i > 5) {
						this->sLFOv_setup_FM();
					}
					else {
						this->sLFOv_setup_SSG();
					}
				}

				for (size_t k = 0; k < len_On + len_Off; k++) {
					if (this->sLFOd_ready) {
						__int16 Detune = this->sLFOd_exec();
						if (this->Detune_prev != Detune) {
							this->Detune_prev = Detune;
							dest->Count = counter++;
							dest->Event = 0x98;
							dest->ParamW = Detune;
							dest->time = time + k;
							dest->Type = 2;
							dest->CH = i;
							dest++;
						}
					}

					if (this->sLFOv_ready) {
						__int8 Volume;
						if (i < 3 || i > 5) {
							Volume = this->sLFOv_exec_FM();
						}
						else {
							Volume = this->sLFOv_exec_SSG();
							//							wprintf_s(L"Volume %01zd: %8zu+%8zu: %04X\n", i, time, k, this->sLFOv_SSG.Volume);
						}
						if (this->Volume_prev != Volume) {
							this->Volume_prev = Volume;
							dest->Count = counter++;
							dest->Event = 0xF9;
							dest->Param[0] = Volume;
							dest->time = time + k;
							dest->Type = 2;
							dest->CH = i;
							dest++;
						}
					}
				}

				time += len_On;

				if (this->Disable_note_off) {
					this->Disable_note_off = false;
					this->Disable_LFO_init = true;
				}
				else if (i > 2 && i < 6 && (sLFOv_ready || sLFOd_ready)) {

				}
				else {
					this->LFO_note_off();
					dest->Count = counter++;
					dest->Event = 0x80;
					dest->time = time;
					dest->Type = 0;
					dest->CH = i;
					dest++;
				}
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
		if ((this->event + this->length)->time >= this->time_loop_start) {
			if (this->loop_start == SIZE_MAX) {
				this->loop_start = this->length;
			}
		}
		this->length++;
	}

#if 1
	// 重複イベントを削除し、ソートしなおす
	size_t length_work = this->length;
	for (size_t i = 0; i < length_work - 1; i++) {
		if ((this->event + i)->time == (this->event + i + 1)->time && (this->event + i)->Event == (this->event + i + 1)->Event) {
			if ((this->event + i)->Event == 0xF4 && (this->event + i)->Param[0] == (this->event + i + 1)->Param[0]) {
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

	// もしループ末尾がループ入り口直前と一致するならループ起終点を繰り上げる
	if (this->loop_start != SIZE_MAX) {
		while (this->loop_start > 0
			&& (this->event + this->length - 1)->CH == (this->event + this->loop_start - 1)->CH
			&& (this->event + this->length - 1)->Event == (this->event + this->loop_start - 1)->Event
			&& (this->event + this->length - 1)->Param[0] == (this->event + this->loop_start - 1)->Param[0]
			&& (this->event + this->length)->time - (this->event + this->length - 1)->time == (this->event + this->loop_start)->time - (this->event + this->loop_start - 1)->time
			)
		{
			this->length--;
			this->loop_start--;
		}
	}

	wprintf_s(L"Event Length %8zu Loop from %zu\n", this->length, this->loop_start);

}

void EVENTS::print_all(void)
{
	for (size_t i = 0; i < this->length; i++) {
		wprintf_s(L"%8zu: %2d: %02X\n", (event + i)->time, (event + i)->CH, (event + i)->Event);
	}
}