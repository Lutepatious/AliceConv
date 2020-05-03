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
			//			wprintf_s(L"Palette not found. Assume 200 Line\n");
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

		if ((g_fmt == GL3) || (g_fmt == GM3)) {
			pI = decode_GL3(pFi, (g_fmt == GM3) ? 1 : 0);
		}
		else if (g_fmt == GL) {
			pI = decode_GL(pFi);
		}
		else if (g_fmt == VSP256) {
			pI = decode_VSP256(pFi);
		}
		else if (g_fmt == VSP200l) {
			pI = decode_VSP200l(pFi);
		}
		else if (g_fmt == VSP) {
			pI = decode_VSP(pFi);
		}
		else if (g_fmt == X68T) {
			pI = decode_X68T(pFi);
		}
		else if (g_fmt == X68V) {
			pI = decode_X68V(pFi);
		}
		else if (g_fmt == X68R) {
			pI = decode_X68R(pFi);
		}

		if (pI == NULL) {
			continue;
		}
		wprintf_s(L"Start %3zu/%3zu %3zu*%3zu %s\n", pI->start_x, pI->start_y, pI->len_x, pI->len_y, pI->sType);

		size_t canvas_x, canvas_y;

		if ((g_fmt == X68T) || (g_fmt == X68V)) {
			canvas_x = canvas_y = X68_LEN;
		}
		else {
			canvas_x = (pI->start_x + pI->len_x) > 640 ? (pI->start_x + pI->len_x) : 640;
			canvas_y = 400;
		}

		if ((g_fmt == VSP200l) || (g_fmt == GL))
			canvas_y = 200;
		else if (g_fmt == GM3)
			canvas_y = 201;

		canvas_y = (pI->start_y + pI->len_y) > canvas_y ? (pI->start_y + pI->len_y) : canvas_y;

		png_bytep canvas;
		canvas = malloc(canvas_y * canvas_x);
		if (canvas == NULL) {
			wprintf_s(L"Memory allocation error. \n");
			free(pI->image);
			exit(-2);
		}

		memset(canvas, pI->BGcolor, canvas_y * canvas_x);
		for (size_t iy = 0; iy < pI->len_y; iy++) {
			memcpy_s(&canvas[(pI->start_y + iy) * canvas_x + pI->start_x], pI->len_x, &pI->image[iy * pI->len_x], pI->len_x);
		}
		free(pI->image);

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".png");

		struct fPNGw imgw;
		imgw.outfile = path;
		imgw.depth = 8;
		imgw.Cols = canvas_x;
		imgw.Rows = canvas_y;
		imgw.Pal = pI->Pal8;
		imgw.Trans = pI->Trans;
		imgw.nPal = pI->colors;
		imgw.nTrans = pI->colors;
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
