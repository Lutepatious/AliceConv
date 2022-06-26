#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>

// イベント順序
// 80 F4 F5 E7 E8 F9 FC E5 EB E9 90

struct EVENT {
	size_t Time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの 消音=0, テンポ=1, 音源初期化=2, タイ=8, 発音=9程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ
	struct LFO_YM2608 h2608;
	__int16 LFO_Detune; // イベントのパラメータ、LFO用のこのプログラムの内部用

	bool operator < (const struct EVENT& out) {
		unsigned CH_weight[16] = { 0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15 };
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
		else if (CH_weight[this->CH] > CH_weight[out.CH]) {
			return true;
		}
		else if (CH_weight[this->CH] < CH_weight[out.CH]) {
			return false;
		}
		else if (this->Count < out.Count) {
			return true;
		}
		else {
			return false;
		}
	}
};

class EVENTS {
	size_t time_loop_start = 0;
	size_t time_end = SIZE_MAX;
	unsigned __int8 Volume_current = 0;
	unsigned __int8 Volume_prev = 0;
	__int16 Detune_current = 0;
	__int16 Detune_prev = 0;

	bool sLFOv_ready = false;
	bool sLFOd_ready = false;
	bool hLFO_ready = false;
	bool sLFOv_direction = false;
	bool sLFOd_direction = false;

	struct {
		struct LFO_soft_volume_SSG Param;
		unsigned Mode;
		int Volume;
		unsigned __int16 Delta;
		unsigned __int16 Wait;
	} sLFOv_SSG = { { 0, 0, 0, 0, 0 }, 0, 0, 0, 0 };

	struct {
		struct LFO_soft_volume_FM Param;
		unsigned __int16 Wait;
		int Volume;
	} sLFOv_FM = { { 0, 0, 0, 0}, 0, 0 };

	struct {
		struct LFO_soft_detune Param;
		unsigned __int16 Wait;
		int Detune;
	} sLFOd = { { 0, 0, 0, 0 }, 0, 0 };

	void sLFOv_setup_FM(void)
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

	void sLFOv_setup_SSG(void)
	{
		if (this->sLFOv_ready) {
			this->sLFOv_SSG.Mode = 1;
			this->sLFOv_SSG.Volume = this->sLFOv_SSG.Delta = this->sLFOv_SSG.Param.Volume;
		}
	}

	void sLFOd_setup(void)
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
	__int8 sLFOv_exec_FM(void)
	{
		if (this->sLFOv_FM.Wait) {
			this->sLFOv_FM.Wait--;
		}
		else {
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
		}
		return this->Volume_current + this->sLFOv_FM.Volume;
	}

	__int8 sLFOv_exec_SSG(void)
	{
		// sLFOv_SSG.Volumeの最大値はは0x4000つまり2^14=16384
		// これをVolume_currentに乗じて2^14で割っている。

		if (this->sLFOv_SSG.Mode == 1) { // 音量増
			this->sLFOv_SSG.Volume += this->sLFOv_SSG.Delta;
			if (this->sLFOv_SSG.Volume >= 0x4000) {
				this->sLFOv_SSG.Volume = 0x4000;
				this->sLFOv_SSG.Wait = this->sLFOv_SSG.Param.Wait1;
				this->sLFOv_SSG.Delta = this->sLFOv_SSG.Param.Delta1;
				this->sLFOv_SSG.Mode = 2;
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

	__int16 sLFOd_exec(void)
	{
		if (this->sLFOd.Wait) {
			this->sLFOd.Wait--;
		}
		else {
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
		}
		return this->sLFOd.Detune;
	}

	void LFO_note_off(void)
	{
		if (sLFOv_ready) {
			if (sLFOv_SSG.Mode != 0 && sLFOv_SSG.Mode < 4) {
				this->sLFOv_SSG.Delta = this->sLFOv_SSG.Param.Delta_last;
				sLFOv_SSG.Mode = 4;
			}
		}
	}

	void init(void)
	{
		this->Volume_current = 0;
		this->Detune_current = 0;
		this->Volume_prev = 0;
		this->Detune_prev = 0;

		this->sLFOv_ready = false;
		this->sLFOd_ready = false;
		this->sLFOv_direction = false;
		this->sLFOd_direction = false;

		this->sLFOv_SSG = { { 0, 0, 0, 0, 0 }, 0, 0, 0, 0 };
		this->sLFOv_FM = { { 0, 0, 0, 0 }, 0, 0 };
		this->sLFOd = { { 0, 0, 0, 0 }, 0, 0 };
	}

public:
	std::vector<struct EVENT> events;
	size_t loop_start = SIZE_MAX;
	bool loop_enable = false;

	void convert(struct MML_decoded& MMLs)
	{
		size_t counter = 0;
		bool Disable_note_off = false;
		bool Disable_LFO_init = false;


		this->loop_enable = false;

		for (size_t j = 0; j < MMLs.CH.size(); j++) {
			this->init();
			size_t i = MMLs.CH.size() - 1 - j;

			size_t time = 0;
			for (auto& e : MMLs.CH[i].E) {
				if ((MMLs.latest_CH == i) && (&e == &MMLs.CH[i].E.at(MMLs.CH[i].Loop_start_pos)) && (MMLs.loop_start_time != SIZE_MAX)) {
					this->time_loop_start = time;
					this->loop_enable = true;
				}

				struct EVENT eve;
				eve.Count = counter++;
				eve.Event = e.Type;
				eve.Time = time;
				eve.CH = i;

				size_t len_On, len_Off;

				switch (e.Type) {
				case 0x80: // Note off
					eve.Type = 0;
					this->events.push_back(eve);
					len_Off = e.Len_off;
					time += len_Off;
					this->LFO_note_off();
					break;

				case 0xE9: // Tie
					Disable_note_off = true;
					break;

				case 0xE0: // Counter
					break;

				case 0xE5: // set flags 4 5 6
					this->sLFOd_ready = !!(e.Param & 0x1);
					this->sLFOv_ready = !!(e.Param & 0x2);
					this->hLFO_ready = !!(e.Param & 0x4);
					break;

				case 0xF5: // Tone select
				case 0xEB: // Panpot
					eve.Type = 2;
					eve.Param = e.Param;
					this->events.push_back(eve);
					break;

				case 0xFC: // Detune
					eve.Type = 2;
					eve.Param = e.Param;
					this->events.push_back(eve);
					this->Detune_current = eve.Param;
					break;

				case 0xE1: // Velocity
					if (this->sLFOv_ready) {
						this->Volume_current += eve.Param;
					}
					else {
						eve.Type = 2;
						eve.Param = e.Param;
						this->events.push_back(eve);
					}
					break;

				case 0xF9: // Volume change
					if (this->sLFOv_ready) {
						this->Volume_current += eve.Param;
					}
					else {
						eve.Type = 2;
						eve.Param = e.Param;
						this->events.push_back(eve);
						this->Volume_current = eve.Param;
					}
					break;

				case 0xF4: // Tempo
					eve.Type = 1;
					eve.Param = e.Param;
					this->events.push_back(eve);
					break;

				case 0xE4: // hLFO
					eve.Type = 2;
					eve.h2608 = e.uLFO.h2608;
					this->events.push_back(eve);
					break;

				case 0xE7: // sLFOd
					this->sLFOd.Param = e.uLFO.sd;
					this->sLFOd_ready = true;
					this->sLFOd_direction = false;
					this->sLFOd_setup();
					//				wprintf_s(L"sLFO_detune %1zX: %08zX: w%04X %04X d%04X l%04X: %04X %04X\n", i, time, this->sLFOd.Param.Wait1, this->sLFOd.Param.Wait2, this->sLFOd.Param.Delta1, this->sLFOd.Param.Limit, this->sLFOd.Wait, this->sLFOd.Detune);
					break;

				case 0xE6: // sLFOv
					this->sLFOv_FM.Param = e.uLFO.svFM;
					this->sLFOv_ready = true;
					this->sLFOv_FM.Wait = this->sLFOv_FM.Param.Wait1;
					this->sLFOv_FM.Volume = 0;
					this->sLFOv_direction = false;
					this->sLFOv_setup_FM();
					//				wprintf_s(L"sLFOv_FM    %1zX: %08zX: w%04X %04X d%04X l%04X\n", i, time, this->sLFOv_FM.Param.Wait1, this->sLFOv_FM.Param.Wait2, this->sLFOv_FM.Param.Delta1, this->sLFOv_FM.Param.Limit);
					break;

				case 0xE8: // sLFOv
					this->sLFOv_SSG.Param = e.uLFO.svSSG;
					this->sLFOv_ready = true;
					//				wprintf_s(L"sLFOv_SSG   %1zX: %08zX: v%04X w%04X d%04X %04X %04X\n", i, time, this->sLFOv_SSG.Param.Volume, this->sLFOv_SSG.Param.Wait1, this->sLFOv_SSG.Param.Delta1, this->sLFOv_SSG.Param.Delta2, this->sLFOv_SSG.Param.Delta_last);
					break;

				case 0x90: // Note on
					len_On = e.Len_on;
					len_Off = e.Len_off;
					eve.Type = 9;
					this->events.push_back(eve);

					eve.Count = counter++;
					eve.Event = 0xD0;
					eve.Param = e.Key;
					eve.Type = 2;
					this->events.push_back(eve);

					this->Detune_prev = this->Detune_current;


					if (Disable_LFO_init) {
						Disable_LFO_init = false;
					}
					else {
						this->sLFOd_setup();
						if (i < 3 || i > 5) {
							this->sLFOv_setup_FM();
						}
						else {
							this->sLFOv_setup_SSG();
						}
					}

					for (size_t k = 0; k < len_On + len_Off; k += TIME_MUL) {
						if (this->sLFOd_ready) {
							__int16 Detune = this->Detune_current + this->sLFOd_exec();
							if (this->Detune_prev != Detune) {
								this->Detune_prev = Detune;
								eve.Count = counter++;
								eve.Event = 0xD1;
								eve.LFO_Detune = Detune;
								eve.Time = time + k;
								eve.Type = 2;
								this->events.push_back(eve);
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
								eve.Count = counter++;
								eve.Event = 0xF9;
								eve.Param = Volume;
								eve.Time = time + k;
								eve.Type = 2;
								this->events.push_back(eve);
							}
						}
					}

					time += len_On;

					if (Disable_note_off) {
						Disable_note_off = false;
						Disable_LFO_init = true;
					}
					else {
						this->LFO_note_off();
						eve.Count = counter++;
						eve.Event = 0x80;
						eve.Time = time;
						eve.Type = 0;
						this->events.push_back(eve);
					}
					time += len_Off;

					break;

				default:
					std::wcout << L"How to reach? :" << e.Type << L"," << e.Param << std::endl;
					break;
				}

			}

		}
		// 出来上がった列の末尾に最大時間のマークをつける
		struct EVENT end;
		end.Count = counter;
		end.Type = 9;
		end.Event = 0xFF;
		end.Time = SIZE_MAX;
		this->events.push_back(end);

		// イベント列をソートする
		std::sort(this->events.begin(), this->events.end());
		size_t length = 0;
		// イベント列の長さを測る。
		while (this->events[length].Time < MMLs.end_time) {
			if (this->loop_start == SIZE_MAX) {
				this->loop_start = length;
			}
			length++;
		}

		// 重複イベントを削除し、ソートしなおす
		size_t length_work = length;
		for (size_t i = 0; i < length_work - 1; i++) {
			if (this->events[i].Time == this->events[i + 1].Time && this->events[i].Event == this->events[i + 1].Event) {
				if (this->events[i].Event == 0xF4 && this->events[i].Param == this->events[i + 1].Param) {
					this->events[i].Time = SIZE_MAX;
					if (i < this->loop_start) {
						this->loop_start--;
					}
					length--;
				}
			}
		}
		std::sort(this->events.begin(), this->events.end());
		this->events.resize(length);
	}

	void print_all(void)
	{
		for (auto& i : events) {
			std::wcout << std::dec << i.Time << ":" << std::hex << i.CH << ":" << i.Event << std::endl;
		}
		std::wcout << std::dec << std::endl;
	}
};
