#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <cctype>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

#include "gc_cpp.h"

#include "EOMML.h"

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		++argv;

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

		unsigned __int8* mpos = (unsigned __int8*)memchr(inbuf, '\xFF', fs.st_size);

		unsigned __int8* mmls = (unsigned __int8*)GC_malloc(fs.st_size - (mpos - inbuf));
		unsigned __int8* dest = mmls;
		unsigned __int8* mml[CHs_MAX + 1];
		size_t CH = 0;
		mml[CH++] = mmls;
		bool in_bracket = false;

		for (unsigned __int8* src = mpos + 1; *src != '\x1A' && src < inbuf + fs.st_size; src++) {
			switch (*src) {
			case ' ':
				break;
			case '\x0d':
				break;
			case '[':
				in_bracket = true;
				break;
			case ']':
				in_bracket = false;
				*dest++ = '\0';
				mml[CH++] = dest;
				break;
			default:
				if (!in_bracket) {
					*dest++ = *src;
				}
			}
		}

		CH--;

		wprintf_s(L"File %s. %zu CHs.\n", *argv, CH);

		struct eomml_decoded MMLs(CH);
		for (size_t i = 0; i < CH; i++) {
			(MMLs.CH + i)->decode(mml[i]);
		}

		if (true) {
			MMLs.print();
		}

		MMLs.unroll_loop();

		if (!MMLs.end_time) {
			wprintf_s(L"No Data. skip.\n");
			continue;
		}
	}
}
