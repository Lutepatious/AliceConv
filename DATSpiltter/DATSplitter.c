#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"

#pragma pack (1)
struct LINKMAP {
	unsigned __int8 ArchiveID;
	unsigned __int8 FileNo;
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		FILE* pFi, * pFo;

		unsigned __int16* Addr;
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);
		wprintf_s(L"File size %I64d.\n", fs.st_size);

		unsigned __int8* buffer = GC_malloc(fs.st_size + 0x100);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		size_t rcount = fread_s(buffer + 0x100, fs.st_size, 1, fs.st_size, pFi);
		if (rcount != fs.st_size) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		Addr = buffer + 0x100LL;
		unsigned __int16 i = 0;
		size_t lmlen;

		while (*(Addr + i) || i == 0) {
			if (0x100LL * *(Addr + i) <= fs.st_size) {
				size_t len;
				if (*(Addr + i + 1)) {
					len = 0x100L * ((size_t) * (Addr + i + 1) - (size_t) * (Addr + i));
				} else {
					len = fs.st_size - 0x100L * ((size_t) * (Addr + i) - 1);
				}

				wprintf_s(L"Entry %03u: %06X, %10zd.\n", i, 0x100L * *(Addr + i), len);
				if (i == 0) {
					lmlen = len;
				}
			}
			i++;
		}

		struct LINKMAP* linkmap = buffer + 0x100LL * *Addr;
		i = 0;
		int have_linkmap = 1;
		while ((linkmap + i)->ArchiveID != 0x1A) {
			if ((linkmap + i)->ArchiveID > 0x1A && (linkmap + i)->ArchiveID != 0x63) {
				have_linkmap = 0;
				break;
			}
			i++;
			if (i >= lmlen / sizeof(struct LINKMAP)) {
				have_linkmap = 0;
				break;
			}
		}

		wchar_t path[_MAX_PATH];
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];
		wchar_t newdir[_MAX_DIR];
		wchar_t newfname[_MAX_FNAME];
		_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		if (have_linkmap) {
			wprintf_s(L"With link map.\n");

			unsigned __int8 arc_ID = towupper(*fname) - L'A' + 1;

			linkmap = buffer + 0x100LL * *Addr;
			i = 0;
			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname + 1);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT)
			{
				_wmkdir(path);
				wprintf_s(L"Out path %s created.\n", path);
			}
			while ((linkmap + i)->ArchiveID != 0x1A) {
				if (arc_ID == (linkmap + i)->ArchiveID) {
					size_t wsize = 0x100L * ((size_t) * (Addr + (linkmap + i)->FileNo + 1) - (size_t) * (Addr + (linkmap + i)->FileNo));

					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%04d%c%03d", i + 1, towupper(*fname), (linkmap + i)->FileNo);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");
						wprintf_s(L"Out size %6zd, name %s.\n", wsize, path);

						ecode = _wfopen_s(&pFo, path, L"wb");
						if (ecode || !pFo) {
							wprintf_s(L"File open error %s.\n", *argv);
							exit(ecode);
						}

						rcount = fwrite(buffer + 0x100LL * *(Addr + (linkmap + i)->FileNo), 1, wsize, pFo);
						if (rcount != wsize) {
							wprintf_s(L"File write error %s.\n", *argv);
							fclose(pFo);
							exit(-2);
						}
						fclose(pFo);
					}
				}
				i++;
			}

		}
		else {
			wprintf_s(L"No link map.\n");
			i = 0;
			swprintf_s(newdir, _MAX_DIR, L"%s%s", dir, fname);
			_wmakepath_s(path, _MAX_PATH, drive, newdir, NULL, NULL);
			struct __stat64 dpath;
			if (_wstat64(path, &dpath) && errno == ENOENT) {
				_wmkdir(path);
				wprintf_s(L"Out path %s created.\n", path);
			}
			while (*(Addr + i) || i == 0) {
				size_t F_Addr = 0x100LL * *(Addr + i);
				if (F_Addr < fs.st_size) {
					__int16 wblock = *(Addr + i + 1) - *(Addr + i);
					if (wblock < 0) {
						wblock = fs.st_size / 0x100 + 1 - *(Addr + i);
					}
					size_t wsize = 0x100L * (size_t)wblock;
					if (wsize) {
						swprintf_s(newfname, _MAX_FNAME, L"%s%03d", fname, i + 1);
						_wmakepath_s(path, _MAX_PATH, drive, newdir, newfname, L".DAT");
						wprintf_s(L"Entry %03u: %06zX %10zu bytes, name %s.\n", i, F_Addr, wsize, path);

						ecode = _wfopen_s(&pFo, path, L"wb");
						if (ecode) {
							wprintf_s(L"File open error %s.\n", *argv);
							exit(ecode);
						}

						rcount = fwrite(buffer + F_Addr, 1, wsize, pFo);
						if (rcount != wsize) {
							wprintf_s(L"File write error %s.\n", *argv);
							fclose(pFo);
							exit(-2);
						}

						fclose(pFo);
					}
				}
				i++;
			}

		}
	}
}
