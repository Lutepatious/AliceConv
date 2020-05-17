#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

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

struct COLOR_src_low {
	unsigned __int8 Bl : 2;
	unsigned __int8 Gl : 4;
	unsigned __int8 Rl : 2;
};

struct COLOR_src_high {
	unsigned __int8 Bh : 3;
	unsigned __int8 Gh : 2;
	unsigned __int8 Rh : 3;
};

struct COLOR_dest {
	unsigned __int16 Bl : 2;
	unsigned __int16 Bh : 3;
	unsigned __int16 Gl : 4;
	unsigned __int16 Gh : 2;
	unsigned __int16 Rl : 2;
	unsigned __int16 Rh : 3;
};
#pragma pack()

// �f�R�[�h���ꂽ�v���[���f�[�^���p�b�N�g�s�N�Z���ɕϊ�(4�v���[����)
void convert_8dot_from_4plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0) | ((*(src + col * 3) & (1 << x)) ? 8 : 0);
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 3;
	}
}

// �f�R�[�h���ꂽ�v���[���f�[�^���p�b�N�g�s�N�Z���ɕϊ�(3�v���[����)
void convert_8dot_from_3plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0);
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 2;
	}
}

// VSP256 PMS8 PMS16�̃A���t�@ch�ɋ��ʂ�8bit depth�f�R�[�_
void decode_d8(unsigned __int8 *destination, unsigned __int8 *source, size_t length, size_t Col_per_Row)
{
	size_t cp_len;
	unsigned __int8* src = source, * dst = destination, * cp_src;
	while ((dst - destination) < length) {
		switch (*src) {
		case 0xF8:
		case 0xF9:
		case 0xFA:
		case 0xFB:
			src++;
			*dst++ = *src++;
			break;
		case 0xFC:
			cp_len = *(src + 1) + 3LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2, src + 2, 2);
				dst += 2;
			}
			src += 4;
			break;
		case 0xFD:
			cp_len = *(src + 1) + 4LL;
			memset(dst, *(src + 2), cp_len);
			dst += cp_len;
			src += 3;
			break;
		case 0xFE:
			cp_len = *(src + 1) + 3LL;
			cp_src = dst - Col_per_Row * 2;
			memcpy_s(dst, cp_len, cp_src, cp_len);
			dst += cp_len;
			src += 2;
			break;
		case 0xFF:
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

// PMS16��16bit depth�f�R�[�_
void decode_d16(unsigned __int16* destination, unsigned __int8* source, size_t length, size_t Col_per_Row)
{
	size_t cp_len;
	unsigned __int8* src = source;
	unsigned __int16* dst = destination, * cp_src;
	while ((dst - destination) < length) {
		switch (*src) {
		case 0xF8:
			src++;
			*dst++ = *(unsigned __int16*)src;
			src += 2;
			break;
		case 0xF9:
			cp_len = *(src + 1) + 1LL;
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
			for (size_t len = 0; len < cp_len; len++) {
				ul.a8 = *src++;
				ud.d.Rh = uh.h.Rh;
				ud.d.Rl = ul.l.Rl;
				ud.d.Gh = uh.h.Gh;
				ud.d.Gl = ul.l.Gl;
				ud.d.Bh = uh.h.Bh;
				ud.d.Bl = ul.l.Bl;
				*dst++ = ud.a16;
			}
			break;
		case 0xFA:
			*dst = *(dst - Col_per_Row + 1);
			dst++;
			src++;
			break;
		case 0xFB:
			*dst = *(dst - Col_per_Row - 1);
			dst++;
			src++;
			break;
		case 0xFC:
			cp_len = *(src + 1) + 2LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, 2 * sizeof(unsigned __int16), src + 2, 2 * sizeof(unsigned __int16));
				dst += 2;
			}
			src += 6;
			break;
		case 0xFD:
			cp_len = *(src + 1) + 3LL;
			for (size_t len = 0; len < cp_len; len++) {
				memcpy_s(dst, sizeof(unsigned __int16), src + 2, sizeof(unsigned __int16));
				dst++;
			}
			src += 4;
			break;
		case 0xFE:
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - Col_per_Row * 2;
			memcpy_s(dst, cp_len * sizeof(unsigned __int16), cp_src, cp_len * sizeof(unsigned __int16));
			dst += cp_len;
			src += 2;
			break;
		case 0xFF:
			cp_len = *(src + 1) + 2LL;
			cp_src = dst - Col_per_Row;
			memcpy_s(dst, cp_len * sizeof(unsigned __int16), cp_src, cp_len * sizeof(unsigned __int16));
			dst += cp_len;
			src += 2;
			break;
		default:
			*dst++ = *(unsigned __int16*)src;
			src += 2;
		}
	}
}
