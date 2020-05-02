#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../aclib/pngio.h"
#include "../aclib/accore.h"

#define PLANE 4LL
#define BPP 8
#define ROWS 480
#define X68_LEN 512

/*
�A���X�\�t�g��CG�͕����̃t�H�[�}�b�g���m�F����Ă���B�Ȃ����̂͏���ɂ��Ă���

	1.GL�t�H�[�}�b�g	�A���X�\�t�g�����PC-8801�ł����m�F�ł��Ȃ���8/512�F��3�v���[���p�t�H�[�}�b�g (640*200)����{
	2.GL3�t�H�[�}�b�g	���PC-9801�ɗp����ꂽ�����̃t�H�[�}�b�g�B���_�s�s��DPS SG���Ō� (640*400)����{ GL3���Ă͓̂�����MS-DOS�p��CG���[�_��GL3.COM�������N88-BASIC(86)�ł�GL2�������B
	3.GL3�̔h���i	X68000�̑��F�\����MSX2�p�ɂ�����ƕύX�������̂��������̂����f�R�[�h���@���킩��Ȃ��B
	4.GM3�t�H�[�}�b�g FM TOWNS��ALICE�̊�CD�Ɏ��^���ꂽIntruder�ׂ̈����̃t�H�[�}�b�g�BGL3��200���C���Ή��ɂ��ăp���b�g�����Ȃ������́B(640*201)����{ Ryu1����̈ڐA�L�b�g��GM3.COM�����O�̗R��
	5.VSP�t�H�[�}�b�g X68000�œ��_�s�s����̗p���ꂽ�t�H�[�}�b�g(PC-9801����RanceIII)�B (640*400)����{
	6.VSP200l�t�H�[�}�b�g �v��VSP��200���C���p��GL�t�H�[�}�b�g����̈ڐA�ϊ��p (640*200)����{
	7.VSP256�F�t�H�[�}�b�g �w�b�_����VSP�������g�͕ʕ��B�p�b�N�g�s�N�Z�������ꂽ256�F�̂��߂̃t�H�[�}�b�g (640*400)����{����(640*480)�����݂���B
	8.PMS�t�H�[�}�b�g �S�{������{�̗p���ꂽ�t�H�[�}�b�g��256�F��65536�F
	9.QNT�t�H�[�}�b�g 24�r�b�g�J���[�p�t�H�[�}�b�g

*/


#pragma pack (1)
// PC88�n��GL�t�H�[�}�b�g�̃p���b�g���
struct GL_Palette {
	unsigned __int16 C0 : 3;
	unsigned __int16 C1 : 3;
	unsigned __int16 u0 : 2;
	unsigned __int16 C2 : 3;
	unsigned __int16 u1 : 3;
	unsigned __int16 u2 : 2;
};

// X68000�t�H�[�}�b�g�̃p���b�g���
struct X68_Palette {
	unsigned __int16 I : 1;
	unsigned __int16 B : 5;
	unsigned __int16 R : 5;
	unsigned __int16 G : 5;
};
// VSP 256�F�t�H�[�}�b�g�̃w�b�_
struct VSP256_header {
	unsigned __int16 Column_in;
	unsigned __int16 Row_in;
	unsigned __int16 Column_out;
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[24];
	unsigned __int8 Palette[256][3];
} hVSP256;

// X68�̓��_�s�s�̈ꕔ�Ŏg���Ă���256�F�t�H�[�}�b�g�̃w�b�_ �A���r�b�O�G���f�B�A���Ȃ̂Ńo�C�g�X���b�v��Y�ꂸ��
struct X68T_header {
	unsigned __int32 Sig;
	unsigned __int32 U0;
	unsigned __int16 U1;
	unsigned __int16 U2;
	unsigned __int16 Pal[0xC0]; // Palette No. 0x40 to 0xFF
	unsigned __int16 U3;
	unsigned __int16 Start;
	unsigned __int16 Cols;
	unsigned __int16 Rows;
} hX68T;

// X68�̃����X3�I�v�V�����Z�b�g ���ԂȂ������ՑO��Ŏg���Ă���256�F�t�H�[�}�b�g�̃w�b�_ �A���r�b�O�G���f�B�A���Ȃ̂Ńo�C�g�X���b�v��Y�ꂸ��
struct X68V_header {
	unsigned __int16 U0;
	unsigned __int16 U1;
	unsigned __int16 Cols;
	unsigned __int16 Rows;
	unsigned __int16 Pal[0x100]; // Palette No. 0x00 to 0xFE �Ō��1�͖�������悤��
} hX68V;

// �C���[�W���ۊǗp�\����
struct image_info2 {
	unsigned __int8* image;
	unsigned __int8(*Palette)[256][3];
	size_t start_x;
	size_t start_y;
	size_t len_x;
	size_t len_y;
	unsigned colors;
} iInfo;

enum fmt_cg { NONE, GL, GL3, GM3, VSP, VSP200l, VSP256, PMS, PMS16, QNT, X68R, X68T, X68V };

int wmain(int argc, wchar_t** argv)
{
	FILE* pFi;
	struct image_info* pI = NULL;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		enum fmt_cg g_fmt = NONE;

		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		unsigned __int8 hbuf[0x40];
		size_t rcount = fread_s(hbuf, sizeof(hbuf), sizeof(hbuf), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		fseek(pFi, 0, SEEK_SET);

		unsigned __int8 t = 0;

		// �ŏ���48�o�C�g��0�Ŗ��ߐs������Ă��邩?
		for (int i = 0; i < 0x30; i++) {
			t |= hbuf[i];
		}

		// �ŏ���48�o�C�g��0�Ŗ��ߐs������Ă���! ������GL�n�t�H�[�}�b�g�Ȃ�GM3��!
		if (t == 0 && hbuf[0x31] & 0x80) {
			wprintf_s(L"Palette not found. Assume 200 Line\n");
			g_fmt = GM3;
		}
		// GL3�t�H�[�}�b�g
		else if (t < 0x10 && hbuf[0x31] & 0x80) {
			g_fmt = GL3;
		}
		// GL�t�H�[�}�b�g
		else if ((hbuf[0] & 0xC0) == 0xC0 && hbuf[3] >= 2) {
			g_fmt = GL;
		}
		// �w�b�_������̒l������ �܂�X68000�œ��_�s�s��256�F�t�H�[�}�b�g��?
		else if (*(unsigned __int64*)hbuf == 0x11000000LL) {
			g_fmt = X68T;
		}
		// X68�ł��ԂȂ������ՑO�邩? �������Œ肳��Ă���̂Œ��ׂ�l�������
		else if (*(unsigned __int64*)hbuf == 0xF000900110002000LL) {
			g_fmt = X68V;
		}
		// X68��RanceII��? �������Œ肳��Ă���
		else if ((*(unsigned __int64*)hbuf == 0x11LL) || (*(unsigned __int64*)hbuf == 0x111LL)) {
			g_fmt = X68R;
		}
		// �Ƃ肠����VSP���Ɖ��肷��
		else {
			// �w�b�_������VSP�ł͂��蓾�Ȃ��l�Ȃ�G���[���b�Z�[�W���o���Ď��s���܂��傤
			if (hbuf[1] >= 3 || hbuf[5] >= 3 || hbuf[3] >= 2 || hbuf[7] >= 2 || hbuf[8] >= 2) {
				wprintf_s(L"Wrong data exist. %s is not VSP and variants.\n", *argv);
				continue;
			}
			// �ŏ���16�o�C�g��0�Ŗ��ߐs������Ă���! �����VSP�ł͖���!
			t = 0;
			for (int i = 0; i < 0x10; i++) {
				t |= hbuf[i];
			}
			if (t == 0) {
				wprintf_s(L"Wrong data exist. %s is not VSP and variants.\n", *argv);
				continue;
			}

			// VSP16�Ɖ��肵�ăp���b�g�G���A��0�Ŗ��ߐs������Ă��邩���ׂ�B
			size_t i = 0xA;
			do {
				if (hbuf[i] != 0)
					break;
			} while (++i < 0x3A);

			// VSP256���ǂ����̔��� VSP256�t���O�������Ă��邩? �J���[�p���b�g��VSP256�̂��̂�?
			if (hbuf[8] == 1 || (i >= 0x20 && i < 0x37)) {
				g_fmt = VSP256;
			}

			// VSP256���ǂ����̔��� �摜�̋N�_�f�[�^��VSP16�͈̔͊O��?
			unsigned __int16* p = hbuf;
			if (*p > 0x50) {
				g_fmt = VSP256;
			}
			if (g_fmt != VSP256) {
				// VSP200l���ǂ����̔��� VSP16�ł��肦�Ȃ��p���b�g���?
				if (hbuf[0xA] >= 0x10 || hbuf[0xC] >= 0x10 || hbuf[0xE] >= 0x10 || hbuf[0x10] >= 0x10 ||
					hbuf[0x12] >= 0x10 || hbuf[0x14] >= 0x10 || hbuf[0x16] >= 0x10 || hbuf[0x18] >= 0x10) {
					g_fmt = VSP200l;
				}
				else {
					g_fmt = VSP;
				}
			}
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		if ((g_fmt == GL3) || (g_fmt == GM3)) {
			pI = decode_GL3(pFi, (g_fmt == GM3) ? 1 : 0);
			if (pI == NULL) {
				continue;
			}
			iInfo.image = pI->image;
			iInfo.start_x = pI->start_x;
			iInfo.start_y = pI->start_y;
			iInfo.len_x = pI->len_x;
			iInfo.len_y = pI->len_y;
			wprintf_s(L"Start %3zu/%3zu %3zu*%3zu GL3\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y);
		}
		else if (g_fmt == GL) {
			pI = decode_GL(pFi);
			if (pI == NULL) {
				continue;
			}
			iInfo.image = pI->image;
			iInfo.start_x = pI->start_x;
			iInfo.start_y = pI->start_y;
			iInfo.len_x = pI->len_x;
			iInfo.len_y = pI->len_y;
			wprintf_s(L"Start %3zu/%3zu %3zu*%3zu GL\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y);
		}
		else if (g_fmt == VSP256) {
			size_t rcount = fread_s(&hVSP256, sizeof(hVSP256), sizeof(hVSP256), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}

			size_t vsp_len = fs.st_size - sizeof(hVSP256);
			unsigned __int8* vsp_data = malloc(vsp_len);
			if (vsp_data == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				fclose(pFi);
				exit(-2);
			}

			rcount = fread_s(vsp_data, vsp_len, 1, vsp_len, pFi);
			if (rcount != vsp_len) {
				wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
				free(vsp_data);
				fclose(pFi);
				exit(-2);
			}
			fclose(pFi);

			const size_t vsp_in_x = hVSP256.Column_in;
			const size_t vsp_in_y = hVSP256.Row_in;
			const size_t vsp_out_x = hVSP256.Column_out + 1;
			const size_t vsp_out_y = hVSP256.Row_out + 1;
			const size_t vsp_len_x = hVSP256.Column_out - hVSP256.Column_in + 1;
			const size_t vsp_len_y = hVSP256.Row_out - hVSP256.Row_in + 1;
			const size_t vsp_len_decoded = vsp_len_y * vsp_len_x;

			unsigned __int8* vsp_data_decoded = malloc(vsp_len_decoded);
			if (vsp_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(vsp_data);
				exit(-2);
			}

			wprintf_s(L"%3zu/%3zu - %3zu/%3zu VSP256 %d:%d size %zu => %zu.\n", vsp_in_x, vsp_in_y, vsp_out_x, vsp_out_y, hVSP256.Unknown[0], hVSP256.Unknown[1], vsp_len, vsp_len_decoded);

			size_t count = vsp_len, cp_len;
			unsigned __int8* src = vsp_data, * dst = vsp_data_decoded, * cp_src;

			while (count-- && (dst - vsp_data_decoded) < vsp_len_decoded) {
				switch (*src) {
				case 0xF8:
				case 0xF9:
				case 0xFA:
				case 0xFB:
					src++;
					*dst++ = *src++;
					break;
				case 0xFC:
					cp_len = *(src + 1) + 3;
					for (size_t len = 0; len < cp_len; len++) {
						memcpy_s(dst, 2, src + 2, 2);
						dst += 2;
					}
					src += 4;
					count -= 3;
					break;
				case 0xFD:
					cp_len = *(src + 1) + 4;
					memset(dst, *(src + 2), cp_len);
					dst += cp_len;
					src += 3;
					count -= 2;
					break;
				case 0xFE:
					cp_len = *(src + 1) + 3;
					cp_src = dst - vsp_len_x * 2;
					memcpy_s(dst, cp_len, cp_src, cp_len);
					dst += cp_len;
					src += 2;
					count--;
					break;
				case 0xFF:
					cp_len = *(src + 1) + 3;
					cp_src = dst - vsp_len_x;
					memcpy_s(dst, cp_len, cp_src, cp_len);
					dst += cp_len;
					src += 2;
					count--;
					break;
				default:
					*dst++ = *src++;
				}
			}
			free(vsp_data);

			iInfo.image = vsp_data_decoded;
			iInfo.start_x = vsp_in_x;
			iInfo.start_y = vsp_in_y;
			iInfo.len_x = vsp_len_x;
			iInfo.len_y = vsp_len_y;
			iInfo.colors = 256;
			iInfo.Palette = hVSP256.Palette;
		}
		else if (g_fmt == VSP200l) {
			pI = decode_VSP200l(pFi);
			if (pI == NULL) {
				continue;
			}
			iInfo.image = pI->image;
			iInfo.start_x = pI->start_x;
			iInfo.start_y = pI->start_y;
			iInfo.len_x = pI->len_x;
			iInfo.len_y = pI->len_y;
			wprintf_s(L"Start %3zu/%3zu %3zu*%3zu VSP200l\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y);
		}
		else if (g_fmt == VSP) {
			pI = decode_VSP(pFi);
			if (pI == NULL) {
				continue;
			}
			iInfo.image = pI->image;
			iInfo.start_x = pI->start_x;
			iInfo.start_y = pI->start_y;
			iInfo.len_x = pI->len_x;
			iInfo.len_y = pI->len_y;
			wprintf_s(L"Start %3zu/%3zu %3zu*%3zu VSP\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y);
		}
		else if (g_fmt == X68T) {
			pI = decode_X68T(pFi);
			if (pI == NULL) {
				continue;
			}
			iInfo.image = pI->image;
			iInfo.start_x = pI->start_x;
			iInfo.start_y = pI->start_y;
			iInfo.len_x = pI->len_x;
			iInfo.len_y = pI->len_y;
			wprintf_s(L"Start %3zu/%3zu %3zu*%3zu X68T\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y);
		}
		else if (g_fmt == X68V) {
			size_t rcount = fread_s(&hX68V, sizeof(hX68V), sizeof(hX68V), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}
			size_t x68_len = fs.st_size - sizeof(hX68V);


			unsigned __int8* x68_data = malloc(x68_len);
			if (x68_data == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				fclose(pFi);
				exit(-2);
			}

			rcount = fread_s(x68_data, x68_len, 1, x68_len, pFi);
			if (rcount != x68_len) {
				wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
				free(x68_data);
				fclose(pFi);
				exit(-2);
			}
			fclose(pFi);

			size_t x68_in_x = 96;
			size_t x68_in_y = 56;
			size_t x68_len_x = _byteswap_ushort(hX68V.Cols);
			size_t x68_len_y = _byteswap_ushort(hX68V.Rows);
			size_t x68_len_decoded = x68_len_x * x68_len_y;
			unsigned __int8* x68_data_decoded = malloc(x68_len_decoded);
			if (x68_data_decoded == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				free(x68_data);
				exit(-2);
			}
			wprintf_s(L"%3zu/%3zu - %3zu/%3zu X68_256V size %zu => %zu.\n", x68_in_x, x68_in_y, x68_in_x + x68_len_x, x68_in_y + x68_len_y, x68_len, x68_len_decoded);

			size_t count = x68_len, cp_len;

			unsigned __int8* src = x68_data, * dst = x68_data_decoded, prev = ~*src, repeat = 0;
			while (count-- && (dst - x68_data_decoded) < x68_len_decoded) {
				if (repeat) {
					repeat = 0;
					cp_len = *src - 2; // range -2 to 253. Minus cancells previous data. 
					if (cp_len > 0) {
						memset(dst, prev, cp_len);
					}
					dst += cp_len;
					src++;
					prev = ~*src;
				}
				else if (*src == prev) {
					repeat = 1;
					prev = *src;
					*dst++ = *src++;
				}
				else {
					repeat = 0;
					prev = *src;
					*dst++ = *src++;
				}
			}
			//		wprintf_s(L"%06I64X: %6I64d %6I64u\n", src - x68_data + sizeof(hX68V), count, dst - x68_data_decoded);


			free(x68_data);

			iInfo.image = x68_data_decoded;
			iInfo.start_x = x68_in_x;
			iInfo.start_y = x68_in_y;
			iInfo.len_x = x68_len_x;
			iInfo.len_y = x68_len_y;
			iInfo.colors = 256;
		}
		else if (g_fmt == X68R) {
			pI = decode_X68R(pFi);
			if (pI == NULL) {
				continue;
			}
			iInfo.image = pI->image;
			iInfo.start_x = pI->start_x;
			iInfo.start_y = pI->start_y;
			iInfo.len_x = pI->len_x;
			iInfo.len_y = pI->len_y;
			wprintf_s(L"Start %3zu/%3zu %3zu*%3zu X68R\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y);
		}

		size_t canvas_x, canvas_y;

		if ((g_fmt == X68T) || (g_fmt == X68V)) {
			canvas_x = canvas_y = X68_LEN;
		}
		else {
			canvas_x = (iInfo.start_x + iInfo.len_x) > 640 ? (iInfo.start_x + iInfo.len_x) : 640;
			canvas_y = 400;
		}

		unsigned __int8 t_color = 0x10;

		if ((g_fmt == GM3) || (g_fmt == GL3) || (g_fmt == VSP256) || (g_fmt == X68T))
			t_color = 0;
		else if (g_fmt == X68V)
			t_color = 256;
		else if ((g_fmt == VSP200l) || (g_fmt == GL))
			t_color = 8;


		if ((g_fmt == VSP200l) || (g_fmt == GL))
			canvas_y = 200;
		else if (g_fmt == GM3)
			canvas_y = 201;

		canvas_y = (iInfo.start_y + iInfo.len_y) > canvas_y ? (iInfo.start_y + iInfo.len_y) : canvas_y;

		unsigned __int8* canvas;
		canvas = malloc(canvas_y * canvas_x);
		if (canvas == NULL) {
			wprintf_s(L"Memory allocation error. \n");
			free(iInfo.image);
			exit(-2);
		}

		memset(canvas, t_color, canvas_y * canvas_x);
		for (size_t iy = 0; iy < iInfo.len_y; iy++) {
			memcpy_s(&canvas[(iInfo.start_y + iy) * canvas_x + iInfo.start_x], iInfo.len_x, &iInfo.image[iy * iInfo.len_x], iInfo.len_x);
		}
		free(iInfo.image);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		png_color pal[256];
		png_byte trans[256];

		memset(pal, 0, sizeof(pal));
		memset(trans, 0xFF, sizeof(trans));
		trans[t_color] = 0;

		if (g_fmt == VSP256) {
			for (size_t ci = 0; ci < 256; ci++) {
				color_256to256(&pal[ci], (*iInfo.Palette)[ci][2], (*iInfo.Palette)[ci][0], (*iInfo.Palette)[ci][1]);
			}
		}
		else if ((g_fmt == GL) || (g_fmt == GL3) || (g_fmt == GM3) || (g_fmt == X68R) || (g_fmt == VSP200l) || (g_fmt == VSP) || (g_fmt == X68T)) {
		}
		else if ((g_fmt == X68V)) {
			union X68Pal_conv {
				struct X68_Palette Pal;
				unsigned __int16 Pin;
			} P;
			for (size_t ci = 0; ci < iInfo.colors; ci++) {
				P.Pin = _byteswap_ushort(hX68V.Pal[ci]);
				color_32to256(&pal[ci], P.Pal.B, P.Pal.R, P.Pal.G);
			}
		}
		else {
			for (size_t ci = 0; ci < iInfo.colors; ci++) {
				color_16to256(&pal[ci], (*iInfo.Palette)[ci][0], (*iInfo.Palette)[ci][1], (*iInfo.Palette)[ci][2]);
			}
			color_16to256(&pal[iInfo.colors], 0, 0, 0);
		}

		if (t_color == 0x10) {
			iInfo.colors = 17;
		}
		if (t_color == 8) {
			iInfo.colors = 9;
		}

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;

		if ((g_fmt == GL) || (g_fmt == GL3) || (g_fmt == GM3) || (g_fmt == X68R) || (g_fmt == VSP200l) || (g_fmt == VSP) || (g_fmt == X68T)) {
			imgw.Pal = pI->Pal8;
			imgw.Trans = pI->Trans;
			imgw.nPal = pI->colors;
			imgw.nTrans = pI->colors;
		}
		else {
			imgw.Pal = pal;
			imgw.Trans = trans;
			imgw.nPal = iInfo.colors;
			imgw.nTrans = iInfo.colors;
		}
		imgw.pXY = ((g_fmt == VSP200l) || (g_fmt == GM3) || (g_fmt == GL)) ? 2 : (g_fmt == X68T) ? 3 : 1;

		imgw.image = malloc(canvas_y * sizeof(png_bytep));
		if (imgw.image == NULL) {
			fprintf_s(stderr, "Memory allocation error. \n");
			free(canvas);
			exit(-2);
		}
		for (size_t j = 0; j < canvas_y; j++)
			imgw.image[j] = (png_bytep)&canvas[j * canvas_x];

		void* res = png_create(&imgw);
		if (res == NULL) {
			wprintf_s(L"File %s create/write error\n", path);
		}

		free(imgw.image);
		free(canvas);
	}
}
