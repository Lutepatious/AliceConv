#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cwchar>
#include <cctype>
#include <limits>
#include <sys/stat.h>
#include <sys/types.h>

#include "gc_cpp.h"

#define CHs_MAX (8)

static unsigned getDefaultLen(unsigned __int8** pmsrc, unsigned Len)
{
	unsigned NLen;
	char* tpos;

	if (isdigit(**pmsrc)) {
		unsigned TLen = strtoul((const char*)(*pmsrc), &tpos, 10);

		*pmsrc = (unsigned __int8*)tpos;
		if (TLen == 0 || TLen > 65) {
			wprintf_s(L"L %u: out of range.\n", TLen);
		}
		size_t M = 0;
		while (TLen % 2 == 0) {
			TLen >>= 1;
			M++;
		}
		if (TLen != 1) {
			wprintf_s(L"L %u: out of range.\n", TLen);
		}

		NLen = 192 >> M;
	}
	else {
		NLen = Len;
	}

	return NLen;
}

static unsigned getNoteLen(unsigned __int8** pmsrc, unsigned Len)
{
	unsigned NLen, NLen_d;
	NLen = NLen_d = getDefaultLen(pmsrc, Len);
	while (**pmsrc == '.') {
		(*pmsrc)++;
		NLen_d >>= 1;
		NLen += NLen_d;
	}

	return NLen;
}

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

		for (size_t i = 0; i < CH; i++) {
			printf_s("%1zu: %s\n", i, mml[i]);
		}

		// eomml 覚書
		// < 1オクターブ下げ
		// > 1オクターブ上げ
		// O デフォルト4 O4のAがA4と同じで440Hz
		// X68000版の取り扱い
		// 闘神都市では音No.80から81音にOPM98.DATを読み込む。(80-160がOPM98.DATの音になる)
		unsigned counter[0x100] = { 0 };

		for (size_t i = 0; i < CH; i++) {
			unsigned Octave = 4; // 1 - 9
			unsigned GS = 8; // 1 - 8
			unsigned Len = 48; // 0-192
			unsigned Vol = 8; // 0-15
			unsigned XVol = 80; // 0-127
			unsigned Tempo = 120;
			unsigned Tone = 0;
			unsigned Key_order[] = { 9, 11, 0, 2, 4, 5, 7 };
			unsigned __int8* msrc = mml[i];
			while (*msrc) {
				bool tie = false;
				unsigned Octave_t;
				unsigned RLen, NLen;
				unsigned Key;


				counter[*msrc]++;

				switch (tolower(*msrc++)) {
				case 'm':
					if (tolower(*msrc) == 'b') {
						msrc++;
					}
					break;
				case 't':
					if (isdigit(*msrc)) {
						unsigned __int8* tpos;
						Tempo = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					if (Tempo < 32 || Tempo > 200) {
						wprintf_s(L"T %u: out of range.\n", XVol);
					}
					// wprintf_s(L"Tempo %u\n", Tempo);
					break;
				case '@':
					if (tolower(*msrc) == 'v') {
						if (isdigit(*++msrc)) {
							unsigned __int8* tpos;
							XVol = strtoul((const char*)msrc, (char**)&tpos, 10);
							msrc = tpos;
						}
						if (XVol > 127) {
							wprintf_s(L"@V %u: out of range.\n", XVol);
						}
						// wprintf_s(L"XVol %u\n", XVol);
					}
					else {
						if (isdigit(*msrc)) {
							unsigned __int8* tpos;
							Tone = strtoul((const char*)msrc, (char**)&tpos, 10);
							msrc = tpos;
						}
						//	wprintf_s(L"Tone %u\n", Tone);
					}
					break;
				case 'q':
					if (isdigit(*msrc)) {
						unsigned __int8* tpos;
						GS = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					if (GS == 0 || GS > 8) {
						wprintf_s(L"Q %u: out of range.\n", GS);
					}
					// wprintf_s(L"GS %u\n", GS);
					break;
				case 'v':
					if (isdigit(*msrc)) {
						unsigned __int8* tpos;
						Vol = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					if (Vol > 15) {
						wprintf_s(L"V %u: out of range.\n", Vol);
					}
					// wprintf_s(L"Vol %u\n", Vol);
					break;
				case 'o':
					if (isdigit(*msrc)) {
						unsigned __int8* tpos;
						Octave = strtoul((const char*)msrc, (char**)&tpos, 10);
						msrc = tpos;
					}
					if (Octave > 8) {
						wprintf_s(L"O %u: out of range.\n", Octave);
					}
					// wprintf_s(L"Octave %u\n", Octave);
					break;
				case '>':
					Octave++;
					if (Octave > 8) {
						wprintf_s(L">:result %u out of range.\n", Octave);
					}
					// wprintf_s(L"Octave %u\n", Octave);
					break;
				case '<':
					Octave--;
					if (Octave > 8) {
						wprintf_s(L"<:result %u out of range.\n", Octave);
					}
					// wprintf_s(L"Octave %u\n", Octave);
					break;
				case 'l':
					Len = getDefaultLen(&msrc, Len);
					// wprintf_s(L"Len %u\n", Len);
					break;
				case 'r':
					RLen = getNoteLen(&msrc, Len);
					// wprintf_s(L"Rest %u\n", RLen);
					break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				case 'g':
					Key = Key_order[tolower(*(msrc - 1)) - 'a'];
					Octave_t = Octave;
					if (*msrc == '#' || *msrc == '+') {
						if (Key == 11) {
							Key = 0;
							Octave_t++;
						}
						else {
							Key++;
						}
						msrc++;
					}
					else if (*msrc == '-') {
						if (Key == 0) {
							Key = 11;
							Octave_t--;
						}
						else {
							Key--;
						}
						msrc++;
					}
					Key += Octave_t * 12;
					NLen = getNoteLen(&msrc, Len);
					if (*msrc == '&') {
						msrc++;
						tie = true;
					}
					// wprintf_s(L"Note %u %u\n", Key, NLen);

					break;
				default:;
					msrc++;
				}
			}
		}
		for (size_t j = 0; j < 0x100; j++) {
			if (counter[j]) {
				printf_s("%c:%u ", j, counter[j]);
			}
		}
		printf_s("\n");

	}
}
