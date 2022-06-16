#pragma pack(push)
#pragma pack(1)
struct mako2_header {
	unsigned __int32 chiptune_addr : 24;
	unsigned __int32 ver : 8;
	unsigned __int32 CH_addr[]; // 12 or 16 Channels
};
#pragma pack(pop)

struct MML_Events {
	size_t Time;
	unsigned __int8 Type;
	unsigned __int8 Key;
	unsigned __int16 Len;
	unsigned __int8 Param[10];
};

class MML_decoded_CH {
	bool mute = false;

public:
	std::vector<struct MML_Events> E;
	size_t time_total = 0;
	size_t Loop_start_pos = 0;
	size_t Loop_start_time = 0;
	size_t Loop_delta_time = 0;

	void mute_on(void)
	{
		this->mute = true;
	}

	bool is_mute(void)
	{
		return this->mute;
	}

	void decode(std::vector<__int8>& input, unsigned __int32 offs)
	{
		if (!offs) {
			this->mute_on();
			return;
		}

		size_t Blocks = 0;
		unsigned __int16* pBlock_offset = (unsigned __int16*)(&input.at(0) + offs);
		unsigned __int16* pBlock_offset_work = pBlock_offset;
		while ((*pBlock_offset_work & 0xFF00) != 0xFF00) {
			Blocks++;
			pBlock_offset_work++;
		}
		size_t Loop_Block = *pBlock_offset_work & 0xFF;

		unsigned __int16 Octave = 4, Octave_current = 4;
		unsigned __int16 time_default = 64;
		__int16 gate_step = 7;
		bool flag_gate = false;
		bool tie = false;

		this->Loop_start_pos = 0;
		this->Loop_start_time = 0;

		for (size_t j = 0; j < Blocks; j++) {
			if (j == Loop_Block) {
				this->Loop_start_pos = this->E.size();
				this->Loop_start_time = this->time_total;
			}
			unsigned __int8* src = (unsigned __int8*)(&input.at(0) + *(pBlock_offset + j));

			while (*src != 0xFF) {
				struct MML_Events eve;

				unsigned __int8 key = 0, note;
				unsigned __int16 time, time_on, time_off;
				bool makenote = false;
				bool len_full = false;

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

				case 0xEC:
					src++;
					this->mute = true;
					note = 0;
					time = 0;
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
					src++;
					Octave_current = Octave = *src++;
					break;
				case 0xF1:
					src++;
					Octave_current = *src++;
					break;
				case 0xF7:
					src++;
					Octave_current = (Octave_current - 1) & 0x7;
					break;
				case 0xF8:
					src++;
					Octave_current = (Octave_current + 1) & 0x7;
					break;

				case 0xF2:
					src++;
					flag_gate = false;
					gate_step = (short)(*src++) + 1;
					break;
				case 0xEA:
					src++;
					flag_gate = true;
					gate_step = *src++;
					break;

				case 0xF3:
					if (*++src & 0x80) { // 128 - 32767
						time_default = _byteswap_ushort(*((unsigned __int16*)src)) & 0x7FFF;
						src += 2;
					}
					else { // 0 - 127
						time_default = *src++;
						if (time_default == 11 || time_default == 21 || time_default == 43) {
							std::wcout << L"Triplets? " << time_default << std::endl;
						}
					}
					break;

				case 0xE9: // Tie
					tie = true;
					eve.Type = *src++;
					E.push_back(eve);
					break;

				case 0xE0: // Counter
				case 0xE1: // Velocity
				case 0xE5: // set LFO flags 456
				case 0xEB: // Panpot
				case 0xF4: // Tempo
				case 0xF5: // Tone select
				case 0xF9: // Volume change
				case 0xFC: // Detune
					eve.Type = *src++;
					eve.Param[0] = *src++;
					E.push_back(eve);
					break;

				case 0xE4: // hLFO
					eve.Type = *src++;
					memcpy_s(eve.Param, 3, src, 3);
					src += 3;
					E.push_back(eve);
					break;

				case 0xE7:
				case 0xE6:
					eve.Type = *src++;
					memcpy_s(eve.Param, 8, src, 8);
					src += 8;
					E.push_back(eve);
					break;

				case 0xE8:
					eve.Type = *src++;
					memcpy_s(eve.Param, 10, src, 10);
					src += 10;
					E.push_back(eve);
					break;

				case 0xFA: // none
				case 0xFD: // none
					wprintf_s(L"%02X\n", *src);
					src += 2;
					break;
				case 0xF6: // none
					wprintf_s(L"%02X\n", *src);
					src += 3;
					break;
				case 0xFE: // none
					wprintf_s(L"%02X\n", *src);
					src += 4;
					break;

				default:
					wprintf_s(L"%02X\n", *src);
					src++;
				}

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
						eve.Type = 0x80;
						eve.Len = time;
						E.push_back(eve);
					}
					else {
						eve.Type = 0x90;
						eve.Key = Octave_current * 12 + note - 1; // 0 - 95 (MIDI Note No.12 - 107)
						eve.Len = time_on;
						E.push_back(eve);
						eve.Type = 0x80;
						eve.Len = time_off;
						E.push_back(eve);
					}
					this->time_total += time;
					if (Octave_current != Octave) {
						Octave_current = Octave;
					}
				}
			}
			this->Loop_delta_time = this->time_total - this->Loop_start_time;
		}
	}
};

struct MML_decoded {
	std::vector<class MML_decoded_CH> CH;
	size_t end_time = 0;
	size_t latest_CH = 0;
	size_t loop_start_time = 0;

	void init(size_t ch)
	{
		this->CH.resize(ch);
	}

	void decode(std::vector<__int8>& in, struct mako2_header* m2h)
	{
		for (size_t i = 0; i < this->CH.size(); i++) {
			this->CH[i].decode(in, m2h->CH_addr[i]);
		}
	}

	void print_delta(void)
	{
		for (size_t i = 0; i < this->CH.size(); i++) {
			std::cout << this->CH[i].Loop_delta_time << std::endl;
		}
	}

	void unroll_loop(void)
	{
		// ループを展開し、全チャネルが同一長のループになるように調整する。
		bool debug = false;
		size_t delta_time_LCM = 1;
		size_t max_time = 0;
		bool no_loop = true;

		// 各ループ時間の最小公倍数をとる
		for (size_t i = 0; i < this->CH.size(); i++) {
			// ループなしの最長時間割り出し
			if (max_time < this->CH[i].time_total) {
				max_time = this->CH[i].time_total;
			}
			// 全チャンネルが非ループかチェック
			no_loop &= this->CH[i].is_mute();
			// そもそもループしないチャネルはスキップ
			if (this->CH[i].is_mute() || this->CH[i].Loop_delta_time == 0) {
				continue;
			}
			delta_time_LCM = std::lcm(delta_time_LCM, this->CH[i].Loop_delta_time);
			// ループ開始が最後のチャネル割り出し
			if (this->loop_start_time < this->CH[i].Loop_start_time) {
				this->loop_start_time = this->CH[i].Loop_start_time;
				this->latest_CH = i;
			}
		}

		// 物によってはループするごとに微妙にずれていって元に戻るものもあり、極端なループ時間になる。(多分バグ)
		// あえてそれを回避せずに完全ループを生成するのでバッファはとても大きく取ろう。
		// 全チャンネルがループしないのならループ処理自体が不要
		if (no_loop) {
			std::wcout << L"Loop: NONE " << max_time << std::endl;
			this->end_time = max_time;
			this->loop_start_time = SIZE_MAX;
		}
		else {
			wprintf_s(L"Loop: Yes %zu Start %zu\n", delta_time_LCM, this->loop_start_time);
			this->end_time = this->loop_start_time + delta_time_LCM;
			for (size_t i = 0; i < this->CH.size(); i++) {
				// そもそもループしないチャネルはスキップ
				if (this->CH[i].is_mute() || this->CH[i].Loop_delta_time == 0) {
					continue;
				}
				size_t time_extra = this->end_time - this->CH[i].Loop_start_time;
				size_t times = time_extra / this->CH[i].Loop_delta_time + !!(time_extra % this->CH[i].Loop_delta_time);
				if (debug) {
					std::wcout << i << L": " << this->CH[i].time_total << L" -> " << delta_time_LCM << L"(x" << delta_time_LCM / this->CH[i].time_total << L")" << L": loop from " << this->CH[i].Loop_start_pos << L"/" << this->CH[i].Loop_start_time << std::endl;
				}

				// ループ回数分のイベント複写
				std::vector<struct MML_Events> t{ this->CH[i].E.begin() + this->CH[i].Loop_start_pos, this->CH[i].E.end() };
				if (debug) {
					std::wcout << i << L": " << t.size() << L"/" << this->CH[i].E.size() << std::endl;
				}

				for (size_t j = 0; j < times - 1; j++) {
					this->CH[i].E.insert(this->CH[i].E.end(), t.begin(), t.end());
				}
				if (debug) {
					std::wcout << i << L": " << t.size() << L"/" << this->CH[i].E.size() << std::endl;
				}
			}
		}
	}

};
