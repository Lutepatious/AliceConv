#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#pragma pack (1)
struct FILE_DIR {
	unsigned __int8 FileName[6];
	unsigned __int8 FileExt[3];
	unsigned __int8 Attrib;
	unsigned __int8 Track;
	unsigned __int8 Unk[5];
} *dirs;
#pragma pack ()

#define CYLS_88 82 // real 41
#define HDRS_88 2
#define SECS_88 11 // real 22
#define LEN_88 256LL
#define DIR_TRACK_88 2

#define CYLS_98 77
#define HDRS_98 2
#define SECS_98 26
#define LEN_98 256LL
#define DIR_TRACK_98 70

#define CYLS_LV 80 // real 40
#define HDRS_LV 2
#define SECS_LV 10 // real 20
#define LEN_LV 256LL
#define DIR_TRACK_LV 2

struct N88_params {
	unsigned __int8 Cylinders;
	unsigned __int8 Heads;
	unsigned __int8 Sectors;
	unsigned __int8 Directory_Track;
	unsigned __int16 FAT_Sector[3];
	size_t Sector_Length;
	size_t Track_Length;
	size_t Load_Offset;
} N88[3] = { {CYLS_88, HDRS_88, SECS_88, DIR_TRACK_88, { (DIR_TRACK_88 + 2) * SECS_88 - 3, (DIR_TRACK_88 + 2) * SECS_88 - 2, (DIR_TRACK_88 + 2) * SECS_88 - 1}
			, LEN_88, LEN_88 * SECS_88, 6 * LEN_88}
			,{CYLS_98, HDRS_98, SECS_98, DIR_TRACK_98, { (DIR_TRACK_98 + 1) * SECS_98 - 3, (DIR_TRACK_98 + 1) * SECS_98 - 2, (DIR_TRACK_98 + 1) * SECS_98 - 1}
			, LEN_98, LEN_98 * SECS_98, SECS_98* LEN_98 / 2}
			,{CYLS_LV, HDRS_LV, SECS_LV, DIR_TRACK_LV, { (DIR_TRACK_LV + 2) * SECS_LV - 3, (DIR_TRACK_LV + 2) * SECS_LV - 2, (DIR_TRACK_LV + 2) * SECS_LV - 1}
			, LEN_LV, LEN_LV * SECS_LV, 0} };

enum f_type { F88 = 0 , F98, FLV };


int wmain(int argc, wchar_t** argv)
{
	FILE* pFi, * pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file\n", *argv);
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
		wprintf_s(L"File size %I64d.\n", fs.st_size);

		enum f_type Format;
		if (fs.st_size == 1021696LL) {
			Format = F98;
		} else if (fs.st_size == 449024LL) {
			Format = F88;
		}
		else if (fs.st_size == 409600LL) {
			Format = FLV;
		}
		else {
			wprintf_s(L"File format unknown %s.\n", *argv);
			fclose(pFi);
			continue;
		}

		unsigned __int8* buffer = GC_malloc((size_t) N88[Format].Cylinders * N88[Format].Heads * N88[Format].Track_Length);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		size_t rcount = fread_s(buffer + N88[Format].Load_Offset, fs.st_size, 1, fs.st_size, pFi);
		if (rcount != fs.st_size) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		dirs = buffer + N88[Format].Directory_Track * N88[Format].Track_Length;
		printf_s("Directory Track %3u, Track Length %5zu\n", N88[Format].Directory_Track, N88[Format].Track_Length);

		for (size_t i = 0; i < (N88[Format].Track_Length / 0x10); i++) {
			if ((dirs + i)->FileName[0] == 0x00) {
				continue;
			}
			if ((dirs + i)->FileName[0] == 0xFF) {
				break;
			}

			unsigned char fname[_MAX_FNAME] = "\0", ext[_MAX_EXT] = "\0", path[_MAX_PATH];

			memcpy_s(fname, _MAX_FNAME, (dirs + i)->FileName, 6);
			memcpy_s(ext, _MAX_EXT, (dirs + i)->FileExt, 3);
			for (size_t k = 0; k < _MAX_FNAME; k++) {
				if (fname[_MAX_FNAME - 1 - k] == ' ') {
					fname[_MAX_FNAME - 1 - k] = '\0';
				}
			}
			for (size_t k = 0; k < _MAX_EXT; k++) {
				if (ext[_MAX_EXT - 1 - k] == ' ') {
					ext[_MAX_EXT - 1 - k] = '\0';
				}
			}
			_makepath_s(path, _MAX_PATH, NULL, NULL, fname, ext);
			printf_s("File %10s Track %3u\n", path, (dirs + i)->Track);

			unsigned __int8 Track = (dirs + i)->Track, NextTrack;
			unsigned __int8(*pFAT1)[256] = buffer + (N88[Format].FAT_Sector[0] * N88[Format].Sector_Length);
			unsigned __int8(*pFAT2)[256] = buffer + (N88[Format].FAT_Sector[1] * N88[Format].Sector_Length);
			unsigned __int8(*pFAT3)[256] = buffer + (N88[Format].FAT_Sector[2] * N88[Format].Sector_Length);
			errno_t ecode = fopen_s(&pFo, path, "wb");
			if (ecode) {
				printf_s("File open error %s.\n", path);
				exit(ecode);
			}

			do {
				if ((*pFAT1)[Track] != (*pFAT2)[Track] || (*pFAT1)[Track] != (*pFAT3)[Track]) {
					printf_s("FAT mismatch %3d %3d %3d\n", (*pFAT1)[Track], (*pFAT2)[Track], (*pFAT3)[Track]);
				}
				NextTrack = (*pFAT1)[Track];
				if (NextTrack >= 0xC0) {
					size_t remain = NextTrack - 0xC0;
					size_t rcount = fwrite(buffer + Track * N88[Format].Track_Length, N88[Format].Sector_Length, remain, pFo);
					if (rcount != remain) {
						printf_s("File write error %s %zd.\n", path, rcount);
						fclose(pFo);
						exit(-2);
					}
				}
				else {
					size_t rcount = fwrite(buffer + Track * N88[Format].Track_Length, N88[Format].Track_Length, 1, pFo);
					if (rcount != 1) {
						printf_s("File write error %s %zd.\n", path, rcount);
						fclose(pFo);
						exit(-2);
					}
				}
				Track = NextTrack;
			} while (Track < 0xC0);

			fclose(pFo);
		}
	}
}
