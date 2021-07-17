#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cwchar>
#include <limits>

#include "gc_cpp.h"
#include "EOMML.h"
#include "../aclib/tools.h"

eomml_decoded_CH::eomml_decoded_CH()
{
	this->MML = (unsigned __int8*)GC_malloc(32 * 1024);
}

eomml_decoded::eomml_decoded()
{
	this->CH = new class eomml_decoded_CH[6];
}

void eomml_decoded_CH::print(void)
{
	for (size_t i = 0; i < this->len; i++) {
		wprintf_s(L"%02X ", *(this->MML + i));
	}
	wprintf_s(L"\n");
}

void eomml_decoded::print(void)
{
	for (size_t i = 0; i < this->CHs; i++) {
		(this->CH + i)->print();
	}
}

bool eomml_decoded_CH::is_mute(void)
{
	return this->mute;
}

void eomml_decoded::unroll_loop(void)
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

static unsigned getDefaultLen(unsigned __int8** pmsrc, unsigned Len)
{
	unsigned NLen;
	char* tpos;

	if (isdigit(**pmsrc)) {
		unsigned TLen = strtoul((const char*)(*pmsrc), &tpos, 10);

		*pmsrc = (unsigned __int8*)tpos;
		if (TLen == 0 || TLen > 65) {
			wprintf_s(L"L %u: out of range.\n", TLen);
		}
		size_t M = 0;
		while (TLen % 2 == 0) {
			TLen >>= 1;
			M++;
		}
		if (TLen != 1) {
			wprintf_s(L"L %u: out of range.\n", TLen);
		}

		NLen = 192 >> M;
	}
	else {
		NLen = Len;
	}

	return NLen;
}

static unsigned getNoteLen(unsigned __int8** pmsrc, unsigned Len)
{
	unsigned NLen, NLen_d;
	NLen = NLen_d = getDefaultLen(pmsrc, Len);
	while (**pmsrc == '.') {
		(*pmsrc)++;
		NLen_d >>= 1;
		NLen += NLen_d;
	}

	return NLen;
}

void eomml_decoded_CH::decode(unsigned __int8* input, unsigned __int16 offs)
{

	unsigned Octave = 4; // 1 - 9
	unsigned GS = 8; // 1 - 8
	unsigned Len = 48; // 0-192
	unsigned Vol = 8; // 0-15
	unsigned XVol = 80; // 0-127
	unsigned Tempo = 120;
	unsigned Tone = 0;
	unsigned Key_order[] = { 9, 11, 0, 2, 4, 5, 7 };
	unsigned __int8* msrc = input;

	// eomml �o��
	// < 1�I�N�^�[�u����
	// > 1�I�N�^�[�u�グ
	// O �f�t�H���g4 O4��A��A4�Ɠ�����440Hz
	// X68000�ł̎�舵��
	// ���_�s�s�ł͉�No.80����81����OPM98.DAT��ǂݍ��ށB(80-160��OPM98.DAT�̉��ɂȂ�)
	while (*msrc) {
		bool tie = false;
		unsigned Octave_t;
		unsigned RLen, NLen;
		unsigned Key;

		switch (tolower(*msrc++)) {
		case 'm':
			if (tolower(*msrc) == 'b') {
				msrc++;
			}
			break;
		case 't':
			if (isdigit(*msrc)) {
				unsigned __int8* tpos;
				Tempo = strtoul((const char*)msrc, (char**)&tpos, 10);
				msrc = tpos;
			}
			if (Tempo < 32 || Tempo > 200) {
				wprintf_s(L"T %u: out of range.\n", XVol);
			}
			// wprintf_s(L"Tempo %u\n", Tempo);
			break;
		case '@':
			if (tolower(*msrc) == 'v') {
				if (isdigit(*++msrc)) {
					unsigned __int8* tpos;
					XVol = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				if (XVol > 127) {
					wprintf_s(L"@V %u: out of range.\n", XVol);
				}
				// wprintf_s(L"XVol %u\n", XVol);
			}
			else {
				if (isdigit(*msrc)) {
					unsigned __int8* tpos;
					Tone = strtoul((const char*)msrc, (char**)&tpos, 10);
					msrc = tpos;
				}
				//	wprintf_s(L"Tone %u\n", Tone);
			}
			break;
		case 'q':
			if (isdigit(*msrc)) {
				unsigned __int8* tpos;
				GS = strtoul((const char*)msrc, (char**)&tpos, 10);
				msrc = tpos;
			}
			if (GS == 0 || GS > 8) {
				wprintf_s(L"Q %u: out of range.\n", GS);
			}
			// wprintf_s(L"GS %u\n", GS);
			break;
		case 'v':
			if (isdigit(*msrc)) {
				unsigned __int8* tpos;
				Vol = strtoul((const char*)msrc, (char**)&tpos, 10);
				msrc = tpos;
			}
			if (Vol > 15) {
				wprintf_s(L"V %u: out of range.\n", Vol);
			}
			// wprintf_s(L"Vol %u\n", Vol);
			break;
		case 'o':
			if (isdigit(*msrc)) {
				unsigned __int8* tpos;
				Octave = strtoul((const char*)msrc, (char**)&tpos, 10);
				msrc = tpos;
			}
			if (Octave > 8) {
				wprintf_s(L"O %u: out of range.\n", Octave);
			}
			// wprintf_s(L"Octave %u\n", Octave);
			break;
		case '>':
			Octave++;
			if (Octave > 8) {
				wprintf_s(L">:result %u out of range.\n", Octave);
			}
			// wprintf_s(L"Octave %u\n", Octave);
			break;
		case '<':
			Octave--;
			if (Octave > 8) {
				wprintf_s(L"<:result %u out of range.\n", Octave);
			}
			// wprintf_s(L"Octave %u\n", Octave);
			break;
		case 'l':
			Len = getDefaultLen(&msrc, Len);
			// wprintf_s(L"Len %u\n", Len);
			break;
		case 'r':
			RLen = getNoteLen(&msrc, Len);
			// wprintf_s(L"Rest %u\n", RLen);
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
			Key = Key_order[tolower(*(msrc - 1)) - 'a'];
			Octave_t = Octave;
			if (*msrc == '#' || *msrc == '+') {
				if (Key == 11) {
					Key = 0;
					Octave_t++;
				}
				else {
					Key++;
				}
				msrc++;
			}
			else if (*msrc == '-') {
				if (Key == 0) {
					Key = 11;
					Octave_t--;
				}
				else {
					Key--;
				}
				msrc++;
			}
			Key += Octave_t * 12;
			NLen = getNoteLen(&msrc, Len);
			if (*msrc == '&') {
				msrc++;
				tie = true;
			}
			// wprintf_s(L"Note %u %u\n", Key, NLen);

			break;
		default:;
			msrc++;
		}
	}
}
