#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "pngio.h"
#include "accore.h"
#include "acinternal.h"

// �R���p�C���I�v�V�����ō\���̂Ɍ��Ԃ��ł��Ȃ��悤�Apragma pack�ŋl�߂邱�Ƃ��w��
#pragma pack (1)
// �v���[���f�[�^���p�b�N�g�s�N�Z���֕ϊ���Ƀo�C�g�X���b�v���鏈���̉ǐ����グ�邽�߂̋��p��
static union {
	__int64 a;
	__int8 a8[8];
} u;

// PMS16�ŋߎ�����F�����k�\�����Ă��镔�����畜������ׂ̃r�b�g�t�B�[���h�\����
// pragma pack���܂Ƃ߂����̂�decode_d16�֐��{�̂��痣�ꂽ�����Œ�`���Ă���
// ���ʌv8�r�b�g��
struct COLOR_src_low {
	unsigned __int8 Bl : 2;
	unsigned __int8 Gl : 4;
	unsigned __int8 Rl : 2;
};

// ��ʌv8�r�b�g��
struct COLOR_src_high {
	unsigned __int8 Bh : 3;
	unsigned __int8 Gh : 2;
	unsigned __int8 Rh : 3;
};

// �����v16�r�b�g��
struct COLOR_dest {
	unsigned __int16 Bl : 2;
	unsigned __int16 Bh : 3;
	unsigned __int16 Gl : 4;
	unsigned __int16 Gh : 2;
	unsigned __int16 Rl : 2;
	unsigned __int16 Rh : 3;
};
#pragma pack()

// �f�[�^�̕��т�[col][plane][y]����[y][col][plane]�ɕς���B
struct plane4_dot8* convert_CPY_to_YCP(const unsigned __int8* src, size_t len_y, size_t len_col, size_t planes)
{
	struct plane4_dot8* buffer_dot8 = GC_malloc(len_y * len_col * sizeof(struct plane4_dot8));
	if (buffer_dot8 == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	struct plane4_dot8* buffer_dot8_dest = buffer_dot8;
	for (size_t iy = 0; iy < len_y; iy++) {
		for (size_t ix = 0; ix < len_col; ix++) {
			for (size_t ip = 0; ip < planes; ip++) {
				buffer_dot8_dest->pix8[ip] = src[(ix * planes + ip) * len_y + iy];
			}
			buffer_dot8_dest++;
		}
	}

	return buffer_dot8;
}

// �f�[�^�̕��т�[y][plane][col]����[y][col][plane]�ɕς���B
struct plane4_dot8* convert_YPC_to_YCP(const unsigned __int8* src, size_t len_y, size_t len_col, size_t planes)
{
	struct plane4_dot8* buffer_dot8 = GC_malloc(len_y * len_col * sizeof(struct plane4_dot8));
	if (buffer_dot8 == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	struct plane4_dot8* buffer_dot8_dest = buffer_dot8;
	for (size_t iy = 0; iy < len_y; iy++) {
		for (size_t ix = 0; ix < len_col; ix++) {
			for (size_t ip = 0; ip < planes; ip++) {
				buffer_dot8_dest->pix8[ip] = src[(iy * planes + ip) * len_col + ix];
			}
			buffer_dot8_dest++;
		}
	}

	return buffer_dot8;
}

// �f�R�[�h���ꂽ�v���[���f�[�^��8�r�b�g�p�b�N�g�s�N�Z���ɕϊ�(4�v���[����)
unsigned __int8* convert_plane4_dot8_to_index8(const struct plane4_dot8* src, size_t len)
{
	unsigned __int8* buffer = GC_malloc(len * sizeof(unsigned __int64));
	if (buffer == NULL) {
		fwprintf_s(stderr, L"Memory allocation error.\n");
		exit(-2);
	}
	unsigned __int64* dst = buffer;
	for (size_t p = 0; p < len; p++) {
		for (size_t x = 0; x < 8; x++) {
			unsigned __int8 index = 1 << x;
			u.a8[x] = ((src->pix8[0] & index) ? 1 : 0) | ((src->pix8[1] & index) ? 2 : 0) | ((src->pix8[2] & index) ? 4 : 0) | ((src->pix8[3] & index) ? 8 : 0);
		}
		*dst = _byteswap_uint64(u.a);
		src++;
		dst++;
	}
	return buffer;
}

// �f�R�[�h���ꂽ4�r�b�g�p�b�N�g�s�N�Z����8�r�b�g�p�b�N�g�s�N�Z���ɕϊ�(���g���G���f�B�A���p)
unsigned __int8* convert_index4_to_index8_LE(const unsigned __int8* src, size_t len)
{
	unsigned __int8* buffer = GC_malloc(len * 2);
	if (buffer == NULL) {
		fwprintf_s(stderr, L"Memory allocation error.\n");
		exit(-2);
	}
	unsigned __int8* dst = buffer;
	for (size_t i = 0; i < len; i++) {
		union {
			unsigned __int8 a8;
			struct PackedPixel4 p4;
		} u;
		u.a8 = *src++;
		*dst++ = u.p4.H;
		*dst++ = u.p4.L;
	}

	return buffer;
}

// �f�R�[�h���ꂽ4�r�b�g�p�b�N�g�s�N�Z����8�r�b�g�p�b�N�g�s�N�Z���ɕϊ�(�r�b�O�G���f�B�A���p)
unsigned __int8* convert_index4_to_index8_BE(const unsigned __int8* src, size_t len)
{
	unsigned __int8* buffer = GC_malloc(len * 2);
	if (buffer == NULL) {
		fwprintf_s(stderr, L"Memory allocation error.\n");
		exit(-2);
	}
	unsigned __int8* dst = buffer;
	for (size_t i = 0; i < len; i++) {
		union {
			unsigned __int8 a8;
			struct PackedPixel4 p4;
		} u;
		u.a8 = *src++;
		*dst++ = u.p4.L;
		*dst++ = u.p4.H;
	}

	return buffer;
}

// VSP�����VSP 200 line�p�̃f�R�[�_
// VSP�`���͂�����Ɠ���ň�ʓI�ȉ��X�L�����ł͂Ȃ���1�o�C�g(8�h�b�g)���Ƃ̏c�X�L����
void decode_d4_VSP(unsigned __int8* destination, unsigned __int8* source, size_t length, size_t Row_per_Col, size_t planes)
{
	size_t cp_len, cur_plane;
	unsigned __int8* src = source, * dst = destination, * cp_src, negate = 0;
	while ((dst - destination) < length) {
		switch (*src) {
		case 0x00: // 1��O�̓��v���[���̃f�[�^��1�o�C�g�ڂŎw�肵�����������R�s�[
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - Row_per_Col * planes;
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		case 0x01: // 2�o�C�g�ڂ̒l��1�o�C�g�ڂŎw�肵�������J��Ԃ�
			cp_len = *(src + 1) + 1LL;
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			break;
		case 0x02: // 2-3�o�C�g�ڂ̒l��1�o�C�g�ڂŎw�肵�������J��Ԃ�
			cp_len = *(src + 1) + 1LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 4;
			break;
		case 0x03: // ��0�v���[��(��)�̃f�[�^��1�o�C�g�ڂŎw�肵�����������R�s�[(negate��1�Ȃ�r�b�g���]����)
			cur_plane = ((dst - destination) / Row_per_Col) % planes;
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - Row_per_Col * cur_plane;
			if (negate) {
				for (size_t len = 0; len < cp_len; len++) {
					*dst++ = ~*cp_src++;
				}
			}
			else {
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
			}
			src += 2;
			negate = 0;
			break;
		case 0x04: // ��1�v���[��(��)�̃f�[�^��1�o�C�g�ڂŎw�肵�����������R�s�[(negate��1�Ȃ�r�b�g���]����)
			cur_plane = ((dst - destination) / Row_per_Col) % planes;
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - Row_per_Col * (cur_plane - 1);
			if (negate) {
				for (size_t len = 0; len < cp_len; len++) {
					*dst++ = ~*cp_src++;
				}
			}
			else {
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
			}
			src += 2;
			negate = 0;
			break;
		case 0x05: // ��2�v���[��(��)�̃f�[�^��1�o�C�g�ڂŎw�肵�����������R�s�[(negate��1�Ȃ�r�b�g���]����)�AVSP200l�ł͂����ɐ��䂪���Ă͂Ȃ�Ȃ��B
			cur_plane = ((dst - destination) / Row_per_Col) % planes;
			cp_len = *(src + 1) + 1LL;
			cp_src = dst - Row_per_Col * (cur_plane - 2);
			if (negate) {
				for (size_t len = 0; len < cp_len; len++) {
					*dst++ = ~*cp_src++;
				}
			}
			else {
				memcpy_s(dst, cp_len, cp_src, cp_len);
				dst += cp_len;
			}
			src += 2;
			negate = 0;
			break;
		case 0x06: // �@�\3,4,5�ŃR�s�[����f�[�^�𔽓]���邩�w��
			src++;
			negate = 1;
			break;
		case 0x07: // �������Ȃ��A�G�X�P�[�v�p
			src++;
			*dst++ = *src++;
			break;
		default:
			*dst++ = *src++;
		}
	}

}

// VSP256, PMS8, PMS16�̃A���t�@ch �ɋ��ʂ�8bit depth�f�R�[�_
void decode_d8(unsigned __int8* destination, unsigned __int8* source, size_t length, size_t Col_per_Row)
{
	size_t cp_len;
	unsigned __int8* src = source, * dst = destination, * cp_src;
	while ((dst - destination) < length) {
		switch (*src) {
		case 0xF8: // �������Ȃ��A�G�X�P�[�v�p
		case 0xF9:
		case 0xFA:
		case 0xFB:
			src++;
			*dst++ = *src++;
			break;
		case 0xFC: // 2,3�o�C�g�ڂ�2�s�N�Z���̒l��1�o�C�g�ڂŎw�肵���񐔌J��Ԃ��B
			cp_len = *(src + 1) + 3LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 4;
			break;
		case 0xFD: // 2�o�C�g�ڂ�1�s�N�Z���̒l��1�o�C�g�ڂŎw�肵���񐔌J��Ԃ��B
			cp_len = *(src + 1) + 4LL;
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			break;
		case 0xFE: // 2�s�O�̒l��1�o�C�g�ڂŎw�肵�������ŃR�s�[
			cp_len = *(src + 1) + 3LL;
			cp_src = dst - Col_per_Row * 2;
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		case 0xFF: // 1�s�O�̒l��1�o�C�g�ڂŎw�肵�������ŃR�s�[
			cp_len = *(src + 1) + 3LL;
			cp_src = dst - Col_per_Row;
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		default:
			*dst++ = *src++;
		}
	}
}

// PMS16��16bit depth�f�R�[�_ ���͂�8bit���ŏo�͂�16bit���ł��邱�Ƃɒ���
void decode_d16(unsigned __int16* destination, unsigned __int8* source, size_t length, size_t Col_per_Row)
{
	size_t cp_len;
	unsigned __int8* src = source;
	unsigned __int16* dst = destination, * cp_src;
	while ((dst - destination) < length) {
		switch (*src) {
		case 0xF8: // �������Ȃ��A�G�X�P�[�v�p
			src++;
			*dst++ = *(unsigned __int16*)src;
			src += 2;
			break;
		case 0xF9: // �A������ߎ������F�̈��k�\���A1�o�C�g�ڂɒ����A2�o�C�g�ڂ��e�F��ʃr�b�g(B3G2R3)�A3�o�C�g�ڈȍ~���e�F���ʃr�b�g(B2G4R2)�B
			cp_len = *(src + 1) + 1LL;
			// ��3�̋��p�̂͏����n�ɂ���Ă̓R���p�C�����ɃG���[�ɂȂ�̂�switch���̊O�ɏo���������ǂ����������ǐ����グ�邽�߂ɂ����ɕ��u
			static union {
				struct COLOR_src_high h;
				unsigned __int8 a8;
			} uh;
			static union {
				struct COLOR_src_low l;
				unsigned __int8 a8;
			} ul;
			static union {
				struct COLOR_dest d;
				unsigned __int16 a16;
			} ud;

			src += 2;
			uh.a8 = *src++;
			ud.d.Rh = uh.h.Rh;
			ud.d.Gh = uh.h.Gh;
			ud.d.Bh = uh.h.Bh;
			for (size_t len = 0; len < cp_len; len++) {
				ul.a8 = *src++;
				ud.d.Rl = ul.l.Rl;
				ud.d.Gl = ul.l.Gl;
				ud.d.Bl = ul.l.Bl;
				*dst++ = ud.a16;
			}
			break;
		case 0xFA: // 1�s�O��1�s�N�Z����̒l���R�s�[
			*dst = *(dst - Col_per_Row + 1);
			dst++;
			src++;
			break;
		case 0xFB: // 1�s�O��1�s�N�Z���O�̒l���R�s�[
			*dst = *(dst - Col_per_Row - 1);
			dst++;
			src++;
			break;
		case 0xFC: // 2-5�o�C�g�ڂ�2�s�N�Z���̒l��1�o�C�g�ڂŎw�肵���񐔌J��Ԃ��B
			cp_len = *(src + 1) + 2LL;
			for (size_t len = 0; len < cp_len; len++) {
				wmemcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 6;
			break;
		case 0xFD: // 2,3�o�C�g�ڂ�1�s�N�Z���̒l��1�o�C�g�ڂŎw�肵���񐔌J��Ԃ��B
			cp_len = *(src + 1) + 3LL;
			wmemset(dst, *(unsigned __int16*)(src + 2), cp_len);
			dst += cp_len;
			src += 4;
			break;
		case 0xFE: // 2�s�O�̒l��1�o�C�g�ڂŎw�肵�������ŃR�s�[
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - Col_per_Row * 2;
			wmemcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		case 0xFF: // 1�s�O�̒l��1�o�C�g�ڂŎw�肵�������ŃR�s�[
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - Col_per_Row;
			wmemcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		default:
			*dst++ = *(unsigned __int16*)src;
			src += 2;
		}
	}
}