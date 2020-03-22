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
	unsigned __int8 Addr;
	unsigned __int8 Unk[5];
} *dirs;
#pragma pack ()

#define CYLS 82
#define HDRS 2
#define SECS 22
#define LEN 256


#define STEP (LEN*SECS/2)

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file\n", *argv);
		exit(-1);
	}

	while (--argc) {
		unsigned __int8 *buffer;
		unsigned __int16 *Addr;
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
			wprintf_s(L"File read error %s %d.\n", *argv, rcount);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		dirs = buffer + STEP * 2;

		for (size_t i = 0; i < STEP / 0x10;i++) {
			if ((dirs + i)->FileName[0] == 0x00 || (dirs + i)->FileName[0] == 0xFF) {
				break;
			}
			printf_s("File %c%c%c%c%c%c.%c%c%c Locate %3d * 17\n"
				, (dirs + i)->FileName[0], (dirs + i)->FileName[1], (dirs + i)->FileName[2], (dirs + i)->FileName[3], (dirs + i)->FileName[4], (dirs + i)->FileName[5]
				, (dirs + i)->FileExt[0], (dirs + i)->FileExt[1], (dirs + i)->FileExt[2], (dirs + i)->Addr);
		}


	free(buffer);
	}
}
