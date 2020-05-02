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

// VSP 200���C���t�H�[�}�b�g
static struct VSP200l {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct GL_Palette3 Pal3[8];
	unsigned __int8 body[];
};

// VSP 16�F�t�H�[�}�b�g
static struct VSP {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4[16];
	unsigned __int8 body[];
};

// X68�̓��_�s�s�̈ꕔ�Ŏg���Ă���256�F�t�H�[�}�b�g �A���v�f�̓r�b�O�G���f�B�A���Ȃ̂Ńo�C�g�X���b�v��Y�ꂸ��
static struct X68T {
	unsigned __int32 Sig;
	unsigned __int32 U0;
	unsigned __int16 U1;
	unsigned __int16 U2;
	unsigned __int16 Pal5BE[0xC0]; // Palette No. 0x40 to 0xFF
	unsigned __int16 U3;
	unsigned __int16 StartBE;
	unsigned __int16 ColsBE;
	unsigned __int16 RowsBE;
	unsigned __int8 body[];
};

// X68�̃����X3�I�v�V�����Z�b�g ���ԂȂ������ՑO��Ŏg���Ă���256�F�t�H�[�}�b�g �A���v�f�̓r�b�O�G���f�B�A���Ȃ̂Ńo�C�g�X���b�v��Y�ꂸ��
static struct X68V {
	unsigned __int16 U0;
	unsigned __int16 U1;
	unsigned __int16 ColsBE;
	unsigned __int16 RowsBE;
	unsigned __int16 Pal5BE[0x100]; // Palette No. 0x00 to 0xFE �Ō��1�͖���?
	unsigned __int8 body[];
};

// VSP 256�F�t�H�[�}�b�g
static struct VSP256 {
	unsigned __int16 Column_in;
	unsigned __int16 Row_in;
	unsigned __int16 Column_out;
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[24];
	struct {
		unsigned __int8 R;
		unsigned __int8 G;
		unsigned __int8 B;
	} Pal8[256];
	unsigned __int8 body[];
};

// �v���[���f�[�^���p�b�N�g�s�N�Z���֕ϊ���Ƀo�C�g�X���b�v���鏈���̉ǐ����グ�邽�߂̋��p��
static union {
	__int64 a;
	__int8 a8[8];
} u;
#pragma pack()

// �f�R�[�h���ꂽ�v���[���f�[�^���p�b�N�g�s�N�Z���ɕϊ�(4�v���[����)
unsigned __int8 convert_8dot_from_4plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	unsigned __int8 max_ccode = 0;
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0) | ((*(src + col * 3) & (1 << x)) ? 8 : 0);
				if (max_ccode < u.a8[x]) {
					max_ccode = u.a8[x];
				}
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 3;
	}
	return max_ccode + 1;
}

// �f�R�[�h���ꂽ�v���[���f�[�^���p�b�N�g�s�N�Z���ɕϊ�(3�v���[����)
unsigned __int8 convert_8dot_from_3plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row)
{
	unsigned __int8 max_ccode = 0;
	for (size_t y = 0; y < row; y++) {
		for (size_t x2 = 0; x2 < col; x2++) {
			for (size_t x = 0; x < 8; x++) {
				u.a8[x] = ((*src & (1 << x)) ? 1 : 0) | ((*(src + col) & (1 << x)) ? 2 : 0) | ((*(src + col * 2) & (1 << x)) ? 4 : 0);
				if (max_ccode < u.a8[x]) {
					max_ccode = u.a8[x];
				}
			}
			(*dst) = _byteswap_uint64(u.a);
			src++;
			dst++;
		}
		src += col * 2;
	}
	return max_ccode + 1;
}

