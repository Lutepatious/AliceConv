#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#pragma pack (1)
struct LINKMAP {
	unsigned __int8 ArchiveID;
	unsigned __int8 FileNo;
} *linkmap;
#pragma pack ()

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
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

		buffer = malloc(fs.st_size + 0x100);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		size_t rcount = fread_s(buffer+0x100, fs.st_size, 1, fs.st_size, pFi);
		if (rcount != fs.st_size) {
			wprintf_s(L"File read error %s %d.\n", *argv, rcount);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		Addr = buffer + 0x100;
		unsigned __int16 i = 0;
		while (*(Addr + i)) {
			if (0x100L * *(Addr + i) < fs.st_size) {
				wprintf_s(L"Entry %03u: %06X.\n", i, 0x100L * *(Addr + i));
			}
			i++;
		}

		linkmap = buffer + 0x100L * *Addr;
		i = 0;
		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];

		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);

		unsigned __int8 arc_ID = towupper(*fname) - L'A' + 1;
		while ((linkmap + i)->ArchiveID != 0x1A ) {
			if (arc_ID == (linkmap + i)->ArchiveID) {
				size_t wsize = 0x100L * ((size_t) *(Addr + (linkmap + i)->FileNo + 1) - (size_t) *(Addr + (linkmap + i)->FileNo));

				if (wsize) {
					wchar_t newdir[_MAX_DIR];
					wchar_t newfname[_MAX_FNAME];
					swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname + 1);
					_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
					struct __stat64 dpath;
					if (_wstat64(path, &dpath) && errno == ENOENT)
					{
						wprintf_s(L"Out path %s created\n", path);
						_wmkdir(path);
					}
					swprintf_s(newfname, _MAX_FNAME, L"%03d%c%03d", i + 1, towupper(*fname), (linkmap + i)->FileNo);
					_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");
					wprintf_s(L"Out size %5d, name %s\n", wsize, path);


					ecode = _wfopen_s(&pFo, path, L"wb");
					if (ecode) {
						wprintf_s(L"File open error %s.\n", *argv);
						free(buffer);
						exit(ecode);
					}

					rcount = fwrite(buffer + 0x100L * *(Addr + (linkmap + i)->FileNo), 1, wsize, pFo);
					if (rcount != wsize) {
						wprintf_s(L"File write error %s.\n", *argv);
						fclose(pFo);
						free(buffer);
						exit(-2);
					}

					fclose(pFo);
				}
			}
			i++;
		}

		free(buffer);
	}
}
