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
アリスソフトのCGは複数のフォーマットが確認されている。なお名称は勝手につけている

	1.GLフォーマット	アリスソフト時代はPC-8801でしか確認できないが8/512色の3プレーン用フォーマット (640*200)が基本
	2.GL3フォーマット	主にPC-9801に用いられた初期のフォーマット。闘神都市やDPS SGが最後 (640*400)が基本 GL3ってのは当時のMS-DOS用のCGローダがGL3.COMだからでN88-BASIC(86)版はGL2だった。
	3.GL3の派生品	X68000の多色表示やMSX2用にちょっと変更したものがあったのだがデコード方法がわからない。
	4.GM3フォーマット FM TOWNS版ALICEの館CDに収録されたIntruderの為だけのフォーマット。GL3を200ライン対応にしてパレット情報を省いたもの。(640*201)が基本 Ryu1さんの移植キットのGM3.COMが名前の由来
	5.VSPフォーマット X68000版闘神都市から採用されたフォーマット(PC-9801だとRanceIII)。 (640*400)が基本
	6.VSP200lフォーマット 要はVSPの200ライン用でGLフォーマットからの移植変換用 (640*200)が基本
	7.VSP256色フォーマット ヘッダこそVSPだが中身は別物。パックトピクセル化された256色のためのフォーマット (640*400)が基本だが(640*480)も存在する。
	8.PMSフォーマット 鬼畜王から本採用されたフォーマットで256色か65536色
	9.QNTフォーマット 24ビットカラー用フォーマット

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

		// 最初の48バイトが0で埋め尽くされているか?
		for (int i = 0; i < 0x30; i++) {
			t |= hbuf[i];
		}

		// 最初の48バイトが0で埋め尽くされている! しかしGL系フォーマットならGM3だ!
		if (t == 0 && hbuf[0x31] & 0x80) {
			//			wprintf_s(L"Palette not found. Assume 200 Line\n");
			g_fmt = GM3;
		}
		// GL3フォーマット
		else if (t < 0x10 && hbuf[0x31] & 0x80) {
			g_fmt = GL3;
		}
		// GLフォーマット
		else if ((hbuf[0] & 0xC0) == 0xC0 && hbuf[3] >= 2) {
			g_fmt = GL;
		}
		// ヘッダが特定の値か判定 まずX68000版闘神都市の256色フォーマットか?
		else if (*(unsigned __int64*)hbuf == 0x11000000LL) {
			g_fmt = X68T;
		}
		// X68版あぶない文化祭前夜か? 実装が固定されているので調べる値も一つだけ
		else if (*(unsigned __int64*)hbuf == 0xF000900110002000LL) {
			g_fmt = X68V;
		}
		// X68版RanceIIか? 実装が固定されている
		else if ((*(unsigned __int64*)hbuf == 0x11LL) || (*(unsigned __int64*)hbuf == 0x111LL)) {
			g_fmt = X68R;
		}
		// とりあえずVSPだと仮定する
		else {
			// ヘッダ部分がVSPではあり得ない値ならエラーメッセージを出して次行きましょう
			if (hbuf[1] >= 3 || hbuf[5] >= 3 || hbuf[3] >= 2 || hbuf[7] >= 2 || hbuf[8] >= 2) {
				wprintf_s(L"Wrong data exist. %s is not VSP and variants.\n", *argv);
				continue;
			}
			// 最初の16バイトが0で埋め尽くされている! これはVSPでは無い!
			t = 0;
			for (int i = 0; i < 0x10; i++) {
				t |= hbuf[i];
			}
			if (t == 0) {
				wprintf_s(L"Wrong data exist. %s is not VSP and variants.\n", *argv);
				continue;
			}

			// VSP16と仮定してパレットエリアが0で埋め尽くされているか調べる。
			size_t i = 0xA;
			do {
				if (hbuf[i] != 0)
					break;
			} while (++i < 0x3A);

			// VSP256かどうかの判定 VSP256フラグが立っているか? カラーパレットがVSP256のものか?
			if (hbuf[8] == 1 || (i >= 0x20 && i < 0x37)) {
				g_fmt = VSP256;
			}

			// VSP256かどうかの判定 画像の起点データがVSP16の範囲外か?
			unsigned __int16* p = hbuf;
			if (*p > 0x50) {
				g_fmt = VSP256;
			}
			if (g_fmt != VSP256) {
				// VSP200lかどうかの判定 VSP16でありえないパレット情報か?
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
