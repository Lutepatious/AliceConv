#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"
#include "MAKO1MML.h"
#include "../aclib/tools.h"

mako1_mml_decoded_CH::mako1_mml_decoded_CH()
{
	this->MML = (unsigned __int8*)GC_malloc(32 * 1024);
}

mako1_mml_decoded::mako1_mml_decoded()
{
	this->CH = new class mako1_mml_decoded_CH[6];
}

void mako1_mml_decoded_CH::decode(unsigned __int8* input, unsigned __int16 offs)
{
	unsigned __int8* src = input + offs;
	unsigned __int8* dest = this->MML;
	unsigned __int8 gate_step = 8, time_default = 48;

	while (*src != 0xFF) {
		unsigned __int8 key, time, time_on, time_off;
		bool makenote = false;
		bool len_full = false;

		switch (*src) {
		case 0xF1:
		case 0xFB:
			src += 3;
			break;
		case 0xF6:
		case 0xFE:
			src += 4;
			break;
		case 0xF7:
		case 0xFA:
		case 0xFC:
		case 0xFD:
			src += 2;
			break;

		case 0xF2: // Gate/Step �ݒ� (��������/�S����) ���� 0-7 => 1/8 - 8/8
			gate_step = *++src + 1;
			src++;
			break;
		case 0xF3: // �f�t�H���g�����ݒ�
			time_default = *++src;
			src++;
			break;
		case 0xF4: // Tempo
		case 0xF5: // Tone select
		case 0xF9: // Volume change
			*dest++ = *src++;
			*dest++ = *src++;
			break;
		case 0xF8: // �O�̉��̍Ĕ���
			src++;
			time = *src++;
			makenote = true;
			break;
		default:
			if (*src <= 0x60 || *src == 0x70) {
				key = *src++;
				time = *src++;
				makenote = true;
			}
			else if (*src >= 0x80 && *src <= 0xE0) {
				key = *src++ & ~0x80;
				time = *src++;
				len_full = true;
				makenote = true;
			}
			else {
				src++;
			}
		}
		// �����\���̓I�N�^�[�u*12+������8bit(0-95)�Ƃ���BMIDI�m�[�g�i���o�[����12�������l�ƂȂ�B
		if (makenote) {
			if (!time) {
				time = time_default;
			}

			if (time == 1) {
				time_on = 1;
			}
			else {
				if (len_full) {
					time_on = time;
				}
				else if (gate_step == 8) {
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
			time_off = time - time_on;
			if (key == 0x70 || time_on == 0) {
				*dest++ = 0x80;
				*dest++ = time;
			}
			else {
				*dest++ = 0x90;
				*dest++ = key; // 0 - 96 (MIDI Note No.12 - 108)
				*dest++ = time_on;
				*dest++ = time_off;
				this->mute = false;
			}
			this->time_total += time;
		}
	}
	this->len = dest - this->MML;
}

void mako1_mml_decoded_CH::print(void)
{
	for (size_t i = 0; i < this->len; i++) {
		wprintf_s(L"%02X ", *(this->MML + i));
	}
	wprintf_s(L"\n");
}

void mako1_mml_decoded::print(void)
{
	for (size_t i = 0; i < this->CHs; i++) {
		(this->CH + i)->print();
	}
}

bool mako1_mml_decoded_CH::is_mute(void)
{
	return this->mute;
}

void mako1_mml_decoded::unroll_loop(void)
{
	// ���[�v��W�J���A�S�`���l�������꒷�̃��[�v�ɂȂ�悤�ɒ�������B
	bool debug = false;
	size_t max_time = 0;
	size_t delta_time_LCM = 1;
	bool no_loop = true;

	// �e���[�v���Ԃ̍ŏ����{�����Ƃ�
	for (size_t i = 0; i < this->CHs; i++) {
		// ���[�v�W�J��̒����̏�����
		(this->CH + i)->len_unrolled = (this->CH + i)->len;
		// ���[�v�Ȃ��̍Œ����Ԋ���o��
		if (max_time < (this->CH + i)->time_total) {
			max_time = (this->CH + i)->time_total;
		}

		// �S�`�����l�����񃋁[�v���`�F�b�N
		no_loop &= (this->CH + i)->is_mute();
		// �����������[�v���Ȃ��`���l���̓X�L�b�v
		if ((this->CH + i)->is_mute() || (this->CH + i)->time_total == 0) {
			continue;
		}
		delta_time_LCM = LCM(delta_time_LCM, (this->CH + i)->time_total);
	}
	// ���ɂ���Ă̓��[�v���邲�Ƃɔ����ɂ���Ă����Č��ɖ߂���̂�����A�ɒ[�ȃ��[�v���ԂɂȂ�B(�����o�O)
	// �����Ă������������Ɋ��S���[�v�𐶐�����̂Ńo�b�t�@�͂ƂĂ��傫����낤�B
	// �S�`�����l�������[�v���Ȃ��̂Ȃ烋�[�v�������̂��s�v
	if (no_loop) {
		wprintf_s(L"Loop: NONE %zu\n", max_time);
		this->end_time = max_time;
		this->loop_start_time = SIZE_MAX;
	}
	else {
		wprintf_s(L"Loop: Yes %zu\n", delta_time_LCM);
		this->end_time = delta_time_LCM;
		for (size_t i = 0; i < this->CHs; i++) {
			// �����������[�v���Ȃ��`���l���̓X�L�b�v
			if ((this->CH + i)->is_mute() || (this->CH + i)->time_total == 0) {
				continue;
			}
			size_t time_extra = this->end_time;
			size_t times = time_extra / (this->CH + i)->time_total + !!(time_extra % (this->CH + i)->time_total);
			(this->CH + i)->len_unrolled = (this->CH + i)->len * (times + 1);
			if (debug) {
				wprintf_s(L"%2zu: %9zu -> %9zu \n", i, (this->CH + i)->len, (this->CH + i)->len_unrolled);
			}
		}
	}
}
