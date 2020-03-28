#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#pragma pack (1)
struct FILE_DIR {
	unsigned __int8 FileName[6];
	unsigned __int8 FileExt[3];
	unsigned __int8 Attrib;
	unsigned __int8 Track;
	unsigned __int8 Unk[5];
} *dirs;
#pragma pack ()

#define CYLS 82
#define HDRS 2
#define SECS 22
#define LEN 256LL


#define STEP (LEN*SECS/2)

#define DIR_TRACK 2
#define FAT1_SECTOR ((DIR_TRACK+2)*SECS/2-3)
#define FAT2_SECTOR ((DIR_TRACK+2)*SECS/2-2)
#define FAT3_SECTOR ((DIR_TRACK+2)*SECS/2-1)


int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file\n", *argv);
		exit(-1);
	}

	while (--argc) {
		unsigned __int8 *buffer;
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);
		wprintf_s(L"File size %I64d.\n", fs.st_size);

		buffer = calloc(fs.st_size + 6 * LEN, 1);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		size_t rcount = fread_s(buffer + 6 * LEN, fs.st_size, 1, fs.st_size, pFi);
		if (rcount != fs.st_size) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		dirs = buffer + DIR_TRACK * STEP;
		printf_s("Directory Track %3u, Track Length %5zu\n", DIR_TRACK, STEP);

		for (size_t i = 0; i < (STEP / 0x10);i++) {
			if ((dirs + i)->FileName[0] == 0x00 || (dirs + i)->FileName[0] == 0xFF) {
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

			printf_s("File %10s Locate %3d\n", path, (dirs + i)->Track);

			unsigned __int8 Track = (dirs + i)->Track, NextTrack;
			unsigned __int8(*pFAT1)[256] = buffer + (FAT1_SECTOR * LEN);
			unsigned __int8(*pFAT2)[256] = buffer + (FAT2_SECTOR * LEN);
			unsigned __int8(*pFAT3)[256] = buffer + (FAT3_SECTOR * LEN);
			errno_t ecode = fopen_s(&pFo, path, "wb");
			if (ecode) {
				printf_s("File open error %s.\n", path);
				exit(ecode);
			}

			do {
				NextTrack = (*pFAT1)[Track];
				if (NextTrack >= 0xC0) {
					size_t remain = NextTrack - 0xC0;
					size_t rcount = fwrite(buffer + Track * STEP, LEN, remain, pFo);
					if (rcount != remain) {
						printf_s("File write error %s %zd.\n", path, rcount);
						fclose(pFo);
						exit(-2);
					}
					printf_s("Remain  %3d %3d %3d\n", (*pFAT1)[Track] - 0xC0, (*pFAT2)[Track] - 0xC0, (*pFAT3)[Track] - 0xC0);
				}
				else {
					size_t rcount = fwrite(buffer + Track * STEP, STEP, 1, pFo);
					if (rcount != 1) {
						printf_s("File write error %s %zd.\n", path, rcount);
						fclose(pFo);
						exit(-2);
					}
					printf_s("Next Track %3d %3d %3d\n", (*pFAT1)[Track], (*pFAT2)[Track], (*pFAT3)[Track]);
				}
				Track = NextTrack;
			} while (Track < 0xC0);

			fclose(pFo);
		}


		free(buffer);
	}
}
