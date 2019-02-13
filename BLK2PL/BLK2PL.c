#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>


#define PLANE 4
#define ROW 400
#define COLUMN (640 / 8)

unsigned __int8 Palette[16][3];
unsigned __int8 buffer_blk[PLANE][ROW][COLUMN];
unsigned __int8 buffer_pl[ROW][PLANE][COLUMN];

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		size_t rcount = fread_s(Palette, sizeof(Palette), sizeof(Palette), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}
		rcount = fread_s(buffer_blk, sizeof(buffer_blk), sizeof(buffer_blk), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		for (size_t y = 0; y < ROW; y++) {
				for (size_t i = 0; i < PLANE; i++) {
					for (size_t x = 0; x < COLUMN; x++) {
					buffer_pl[y][i][x] = buffer_blk[i][y][x];
				}
			}
		}

		ecode = _wfopen_s(&pFo, L"test.pl", L"wb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		fwrite(Palette, sizeof(Palette), 1, pFo);
		fwrite(buffer_pl, sizeof(buffer_pl), 1, pFo);

		fclose(pFo);


	}
}
