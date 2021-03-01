#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "../aclib/wave.h"

struct WAVE_header waveH;
struct WAVE_chunk1 waveC1;
struct WAVE_chunk2 waveC2;

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		FILE* pFi, * pFo;

		char change = 0;
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode || !pFi) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);
		wprintf_s(L"File size %I64d.\n", fs.st_size);

		size_t rcount = fread_s(&waveH, sizeof(waveH), sizeof(waveH), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		if (waveH.ChunkID != _byteswap_ulong(0x52494646) || waveH.Format != _byteswap_ulong(0x57415645)) {
			wprintf_s(L"File %s is not WAVE file.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		if (fs.st_size > waveH.ChunkSize + 8LL) {
			wprintf_s(L"The file %s too long. %ld bytes.\n", *argv, waveH.ChunkSize + 8);
		}

		if (fs.st_size < waveH.ChunkSize + 8LL) {
			wprintf_s(L"The file %s too short. %ld bytes.\n", *argv, waveH.ChunkSize + 8);
		}

		rcount = fread_s(&waveC1, sizeof(waveC1), sizeof(waveC1), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		if (waveC1.Subchunk1ID != _byteswap_ulong(0x666d7420)) {
			wprintf_s(L"File %s is not WAVE file.\n", *argv);
			fclose(pFi);
			exit(-2);
		}
		if (waveC1.Subchunk1Size != 16 || waveC1.AudioFormat != 1) {
			wprintf_s(L"File %s is not PCM WAVE file. Skip.", *argv);
			fclose(pFi);
			continue;
		}

		wprintf_s(L"%d ch. %ld Hz. %ld bytes/sec. %d bytes/(sample*ch). %d bits/sample.\n",
			waveC1.NumChannels, waveC1.SampleRate, waveC1.ByteRate, waveC1.BlockAlign, waveC1.BitsPerSample);

		if (waveC1.ByteRate != waveC1.NumChannels * waveC1.SampleRate * waveC1.BitsPerSample / 8) {
			wprintf_s(L"ByteRate not match.\n");
		}

		if (waveC1.BlockAlign != waveC1.NumChannels * waveC1.BitsPerSample / 8) {
			wprintf_s(L"BlockAlign not match. Wrong %d -> Correct %d.\n", waveC1.BlockAlign, waveC1.NumChannels * waveC1.BitsPerSample / 8);
			waveC1.BlockAlign = waveC1.NumChannels * waveC1.BitsPerSample / 8;
			change = 1;
		}

		rcount = fread_s(&waveC2, sizeof(waveC2), sizeof(waveC2), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			exit(-2);
		}

		if (waveC2.Subchunk2ID != _byteswap_ulong(0x64617461)) {
			wprintf_s(L"File %s is not WAVE file.\n", *argv);
			exit(-2);
		}
		wprintf_s(L"Data size %ld.\n", waveC2.Subchunk2Size);
		if (fs.st_size != sizeof(waveH) + sizeof(waveC1) + sizeof (waveC2) + waveC2.Subchunk2Size) {
			change = 1;
			wprintf_s(L"The file %s not match. %I64d bytes.\n", *argv, sizeof(waveH) + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size);
		}

		if (waveH.ChunkSize != 4 + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size) {
			change = 1;
			wprintf_s(L"The file %s not match. %I64d bytes.\n", *argv, sizeof(waveH) + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size);
		}

		unsigned __int8* buffer = GC_malloc(waveC2.Subchunk2Size);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			exit(-2);
		}

		rcount = fread_s(buffer, waveC2.Subchunk2Size, 1, waveC2.Subchunk2Size, pFi);
		if (rcount != waveC2.Subchunk2Size) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
		}
		fclose(pFi);

		// tail check
		if (waveC1.BitsPerSample == 8 && waveC1.NumChannels == 1) {
			while (rcount && buffer[rcount - 1] == 0) {
				rcount--;
			}
		}
		if (rcount != waveC2.Subchunk2Size) {
			wprintf_s(L"The file %s needs truncate %d to %zd.\n", *argv, waveC2.Subchunk2Size, rcount);
			change = 1;
			waveC2.Subchunk2Size = rcount;
		}

		if (change) {
			wchar_t path[_MAX_PATH];
			wchar_t ext[_MAX_EXT];
			wchar_t fname[_MAX_FNAME];
			wchar_t dir[_MAX_DIR];
			wchar_t drive[_MAX_DRIVE];

			_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);

//			if (wcscat_s(fname, _MAX_FNAME, L"_clean")) {
//				wprintf_s(L"%s: new filename generation error.\n", *argv);
//				exit(1);
//			}

			_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".WAV");
			waveH.ChunkSize = 4 + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size;

			ecode = _wfopen_s(&pFo, path, L"wb");
			if (ecode || !pFo) {
				wprintf_s(L"File open error %s.\n", *argv);
				exit(ecode);
			}

			rcount = fwrite(&waveH, sizeof(waveH), 1, pFo);
			if (rcount != 1) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				exit(-2);
			}
			rcount = fwrite(&waveC1, sizeof(waveC1), 1, pFo);
			if (rcount != 1) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				exit(-2);
			}
			rcount = fwrite(&waveC2, sizeof(waveC2), 1, pFo);
			if (rcount != 1) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				exit(-2);
			}
			rcount = fwrite(buffer, 1, waveC2.Subchunk2Size, pFo);
			if (rcount != waveC2.Subchunk2Size) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				exit(-2);
			}
			fclose(pFo);
		}
	}

	return 0;
}