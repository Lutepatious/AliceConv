#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

#include "gc_cpp.h"

#pragma pack (1)
struct mako1_header {
	unsigned __int32 sig; // must be 0x0000A000
	struct {
		unsigned __int16 Address;
		unsigned __int16 Length;
	} CH[6];
};
#pragma pack ()

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		FILE* pFi;
		errno_t ecode = _wfopen_s(&pFi, *argv, L"rb");
		if (ecode || !pFi) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);

		unsigned __int8* inbuf = (unsigned __int8*)GC_malloc(fs.st_size);
		if (inbuf == NULL) {
			wprintf_s(L"Memory allocation error. \n");
			fclose(pFi);
			exit(-2);
		}

		size_t rcount = fread_s(inbuf, fs.st_size, fs.st_size, 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %zd.\n", rcount);
			fclose(pFi);
			exit(-2);
		}

		fclose(pFi);

		struct mako1_header* pM1HDR = (struct mako1_header*)inbuf;

		unsigned mako1form = 0;
		if (pM1HDR->sig == 0xA000UL) {
			mako1form = 1;
		}
	}
}
