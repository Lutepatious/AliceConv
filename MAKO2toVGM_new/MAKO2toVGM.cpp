#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <limits>

#include "PCMtoWAVE.hpp"
#include "MAKO2MML.hpp"
#include "events.hpp"
#include "VGM.hpp"

constexpr size_t MASTERCLOCK_NEC_OPN = 3993600;
constexpr size_t MASTERCLOCK_NEC_OPNA = MASTERCLOCK_NEC_OPN * 2;
constexpr size_t MASTERCLOCK_SHARP_OPM = 4000000;

class VGMdata_YM2151 : public VGM_YM2151_MAKO2 {
public:
	VGMdata_YM2151(void)
	{
		this->vgm_header.lngHzYM2151 = MASTERCLOCK_SHARP_OPM;
		this->preset = NULL;
	}

	void make_init(void) {
		this->make_data(0x01, 0x00);
		this->make_data(0x14, 0x00);
		this->make_data(0x10, 0x64);
		this->make_data(0x11, 0x00);
		this->Tone_set_SSG_emulation(3);
		this->Tone_set_SSG_emulation(4);
		this->Tone_set_SSG_emulation(5);
	}

	void convert(class EVENTS& in)
	{
		size_t Time_Prev = 0;
		size_t Time_Prev_VGM = 0;
		unsigned __int8 Volume[8] = { 0 };

		for (auto& eve : in.events) {
			if (eve.Time == SIZE_MAX) {
				break;
			}
			if (eve.Time - Time_Prev) {
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
				// �{���A�X��NA�̐������Z�ɔ����v�Z�덷����������ΐ��m�ɂȂ邪�A20���炵��2-4�b���x�Ȃ̂Ŗ������鎖�Ƃ����B
				// ��x�͂��������R�[�h���������̂Ń��|�W�g���̗�����ǂ��Ό�����B
				// MAKO2�͒�����9/10�Ƃ��Ē����������AMAKO1�ł�6/5�Ƃ���(���_�s�s PC-9801�ł�MAKO1��MAKO2�̔�r���犄��o��)
				// VA��BIOS�����t����̂Œ������Ȃ��B

				constexpr size_t gcd_VGMT = std::gcd(60 * VGM_CLOCK * 2 * 3, 48 * 10);
				size_t N_VGMT = eve.Time * 60 * VGM_CLOCK * 2 * 3 / gcd_VGMT;
				size_t D_VGMT = 48 * this->Tempo * 10 / gcd_VGMT;

				size_t c_VGMT = (N_VGMT / D_VGMT + 1) >> 1;
				size_t d_VGMT = c_VGMT - Time_Prev_VGM;

				Time_Prev_VGM += d_VGMT;
				this->time_prev_VGM_abs += d_VGMT;
				Time_Prev = eve.Time;

				this->make_wait(d_VGMT);
			}

			if (in.loop_enable && eve.Time == in.loop_start) {
				this->time_loop_VGM_abs = time_prev_VGM_abs;
				this->vgm_loop_pos = vgm_body.size();
				in.loop_enable = false;
			}

			switch (eve.Event) {
			case 0xF4: // Tempo ����!! �������ς��Ɨݐώ��Ԃ��ς��!! �K���Čv�Z����!!
				Time_Prev_VGM = ((Time_Prev_VGM * this->Tempo * 2) / eve.Param + 1) >> 1;
				this->Tempo = eve.Param;

				// ���̌��NA�̌v�Z�ƃ^�C�}���荞�݂̐ݒ�͎��ۂɂ͕s�v
				this->Timer_set_FM();
				break;
			case 0xEB:
				if (eve.Param == 0) {
					this->L[eve.CH] = false;
					this->R[eve.CH] = false;
				}
				else if (eve.Param == 1) {
					this->L[eve.CH] = true;
					this->R[eve.CH] = false;
				}
				else if (eve.Param == 2) {
					this->L[eve.CH] = false;
					this->R[eve.CH] = true;

				}
				else {
					this->L[eve.CH] = true;
					this->R[eve.CH] = true;
				}
				break;
			case 0xF5: // Tone select
				this->Tone_select_FM(eve.CH, eve.Param);
				break;
			case 0x80: // Note Off
				this->Note_off_FM(eve.CH);
				break;
			case 0xF9: // Volume change @V{0-127}
				Volume[eve.CH] = eve.Param;
				this->Volume_FM(eve.CH, Volume[eve.CH]);
				break;
			case 0xE1: // Velocity
				Volume[eve.CH] += eve.Param;
				Volume[eve.CH] &= 0x7F;
				this->Volume_FM(eve.CH, Volume[eve.CH]);
				break;
			case 0xD0:
				this->Key_set_FM(eve.CH, eve.Param);
				break;
			case 0x90: // Note on
				this->Note_on_FM(eve.CH);
				break;
			case 0xFC: // Detune
				break;
			}
		}
		this->finish();
	}
};

enum class CHIP { NONE = 0, YM2203, YM2608, YM2151 };


int wmain(int argc, wchar_t** argv)
{
	bool debug = false;

	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" [-a | -n | -x] [-d] [-s<num>] file..." << std::endl;
		exit(-1);
	}

	unsigned __int8 SSG_Volume = 0;
	enum CHIP chip_force = CHIP::NONE;
	bool early_detune = false;
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'a') {
				chip_force = CHIP::YM2608;
			}
			else if (*(*argv + 1) == L'n') {
				chip_force = CHIP::YM2203;
			}
			else if (*(*argv + 1) == L'x') {
				chip_force = CHIP::YM2151;
			}
			else if (*(*argv + 1) == L'd') {
				early_detune = true;
			}
			else if (*(*argv + 1) == L's') {
				int tVol = _wtoi(*argv + 2);
				if (tVol > 255) {
					SSG_Volume = 255;
				}
				else if (tVol < 0) {
					SSG_Volume = 0;
				}
				else {
					SSG_Volume = tVol;
				}
			}
			continue;
		}
		enum CHIP chip = CHIP::NONE;

		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		struct mako2_header* pM2HDR = (struct mako2_header*)&inbuf.at(0);
		union PCMs* pPCM = (union PCMs*)&inbuf.at(0);

		unsigned CHs = 0;
		unsigned mako2form = 0;
		struct WAVEOUT wout;

		if (pM2HDR->chiptune_addr == 0x34UL && pM2HDR->ver == 1) {
			mako2form = pM2HDR->ver;
			CHs = 12;
		}
		else if (pM2HDR->chiptune_addr == 0x44UL && pM2HDR->ver >= 1 && pM2HDR->ver <= 3) {
			mako2form = pM2HDR->ver;
			CHs = 16;
		}
		else if (pPCM->tsnd.LoopEnd == 0 && pPCM->tsnd.Size)
		{
			// WAVE 8bit�͕����Ȃ�8bit�\���ŁATOWNS SND�͕���1bit+7bit��-0��+0��1�̍�������\���ȏ�ɒ��S�l��-0�B
			// �����Ńg���b�L�[�ȕϊ����s��
			// ���������̂܂܎g�����Ƃ�0x80(-0)-0xFF(-127)��0x80(128)-0xFF(255)�Ƃ���B
			// �����͔��]��0x00(+0)-0x7F(127)��0x7F(127)-0x00(0)�ɂ���B
			// �����ŃR�[�h�����ĖY��Ă����̂ŉ�͂���H�ڂɂȂ�������R�����g���ꂽ(2020.05.22)
			for (size_t i = 0; i < pPCM->tsnd.Size; i++) {
				if (!(pPCM->tsnd.body[i] & 0x80)) {
					wout.pcm8.push_back(~(pPCM->tsnd.body[i] | 0x80));
				}
				else {
					wout.pcm8.push_back(pPCM->tsnd.body[i]);
				}
			}
			wout.update((2000L * pPCM->tsnd.tSampleRate / 98 + 1) >> 1); // 0.098�Ŋ���̂ł͂Ȃ��A2000/98���|����+1����2�Ŋ���
		}
		else if (pPCM->mp.ID == _byteswap_ulong(0x4D506400))
		{
			for (size_t i = 0; i < pPCM->mp.Len - 0x10; i++) {
				wout.pcm8.push_back(pPCM->mp.body[i].L << 4);
				wout.pcm8.push_back(pPCM->mp.body[i].H << 4);
			}
			wout.update(100LL * pPCM->mp.sSampleRate);
		}
		else if (pPCM->pm.ID == _byteswap_ushort(0x504D))
		{
			if (pPCM->pm.Ch != 1 || pPCM->pm.BitsPerSample != 4) {
				std::wcout << L"Skip File" << std::endl;
				continue;
			}
			for (size_t i = 0; i < pPCM->pm.Size; i++) {
				wout.pcm8.push_back(pPCM->pm.body[i].L << 4);
				wout.pcm8.push_back(pPCM->pm.body[i].H << 4);
			}
			wout.update(100LL * pPCM->pm.sSampleRate);
		}
		else
		{
			for (size_t i = 0; i < inbuf.size(); i++) {
				wout.pcm8.push_back(pPCM->pcm4[i].L << 4);
				wout.pcm8.push_back(pPCM->pcm4[i].H << 4);
			}
			while (*(wout.pcm8.end() - 1) == 0) {
				wout.pcm8.pop_back();
			}

			wout.update(8000ULL);
		}
		if (wout.pcm8.size()) {
			size_t wsize = wout.out(*argv);
			std::wcout << wsize << L" bytes written." << std::endl;

			wout.pcm8.clear();
			continue;
		}
		else if (!mako2form) {
			continue;
		}

		unsigned CHs_real = CHs;

		// ��:Ch.8����łȂ��Ƃ���CHs_real��9���ێ����Ă��Ȃ���΂Ȃ�Ȃ��B�����[CHs_real - 1]��[--CHs_real]�Ə��������Ă͂Ȃ�Ȃ��B
		while (!pM2HDR->CH_addr[CHs_real - 1] && --CHs_real);
		if (!CHs_real) {
			wprintf_s(L"No Data. skip.\n");
			continue;
		}

		if (chip_force != CHIP::NONE) {
			chip = chip_force;
		}
		else if (chip == CHIP::NONE) {
			if (CHs_real == 6) {
				chip = CHIP::YM2203;
			}
			else if (CHs_real == 9) {
				chip = CHIP::YM2608;
			}
			else if (CHs_real == 8) {
				chip = CHIP::YM2151;
			}
		}

		wprintf_s(L"%s: %zu bytes Format %u %2u/%2u CHs.\n", *argv, inbuf.size(), mako2form, CHs_real, CHs);

		struct MML_decoded m;
		m.init(CHs_real);
		m.decode(inbuf, pM2HDR);
		if (debug) {
			m.print_delta();
		}
		m.unroll_loop();

		class EVENTS events;
		events.convert(m);
		if (debug) {
			events.print_all();
		}

		size_t outsize = 0;
		if (chip == CHIP::YM2203) {
		}
		else if (chip == CHIP::YM2608) {
		}
		else if (chip == CHIP::YM2151) {
			class VGMdata_YM2151 v2151;
			v2151.init((union MAKO2_Tone*) &inbuf.at(pM2HDR->chiptune_addr), mako2form);
			v2151.make_init();
			v2151.convert(events);
			outsize = v2151.out(*argv);
		}

		if (outsize == 0) {
			std::wcerr << L"File output failed." << std::endl;

			continue;
		}
		else {
			std::wcout << outsize << L" bytes written." << std::endl;
		}
	}
}
