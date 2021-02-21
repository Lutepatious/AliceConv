#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "../aclib/wave.h"

#pragma pack (1)
struct PM_header {
	unsigned __int16 ID; // must be 'PM' 0x504D
	unsigned __int8 Ch;
	unsigned __int16 StartAddr;
	unsigned __int8 BitsPerSample;
	unsigned __int16 sSampleRate; // *100
	unsigned __int16 Size;
	unsigned __int16 unk;
} pmH;

struct MP_header {
	unsigned __int32 ID; // must be 'MPd ' 0x4D506400
	unsigned __int16 sSampleRate; // *100
	unsigned __int16 Len; // File Length
	unsigned __int8 unk[8];
} mpH;

struct PCM4 {
	unsigned __int8 H : 4;
	unsigned __int8 L : 4;
};
#pragma pack ()

int wmain(int argc, wchar_t **argv)
{

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		FILE* pFi, * pFo;
		struct PCM4* inbuf;
		unsigned __int32 SampleRate;
		size_t rsize;
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);
		wprintf_s(L"File size %I64d.\n", fs.st_size);

		unsigned __int64 sig;
		size_t rcount = fread_s(&sig, sizeof(sig), sizeof(sig), 1, pFi);
		fseek(pFi, 0, SEEK_SET);

		if ((unsigned __int32) sig == 0x01000034UL) {
			wprintf_s(L"MAKO2 (OPN) format skip.\n");
			fclose(pFi);
			continue;
		}
		else if ((unsigned __int32) sig == 0x02000044UL) {
			wprintf_s(L"MAKO2 (OPNA) format skip.\n");
			fclose(pFi);
			continue;
		}
		else if ((unsigned __int32) sig == _byteswap_ulong(0x4D506400)) {
			rcount = fread_s(&mpH, sizeof(mpH), sizeof(mpH), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}
			wprintf_s(L"File size %ld. %ld Hz.\n", mpH.Len, mpH.sSampleRate * 100);
			rsize = mpH.Len - 0x10;
			SampleRate = 100L * mpH.sSampleRate;
		}
		else if ((unsigned __int16) sig == _byteswap_ushort(0x504D)) {
			rcount = fread_s(&pmH, sizeof(pmH), sizeof(pmH), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}
			wprintf_s(L"File size %ld. %d Ch. %ld Hz. %d bits/sample.\n",
				pmH.Size, pmH.Ch, pmH.sSampleRate * 100, pmH.BitsPerSample);

			if (pmH.Ch != 1 || pmH.BitsPerSample != 4) {
				wprintf_s(L"Skip File %s.\n", *argv);
				fclose(pFi);
				continue;
			}
			rsize = pmH.Size;
			SampleRate = 100L * pmH.sSampleRate;
		}
		else {
			rsize = fs.st_size;
			wprintf_s(L"Unknown file type. %s\n", *argv);
			fseek(pFi, 0, SEEK_SET);
			SampleRate = 8000;
		}

		inbuf = GC_malloc(rsize);
		if (inbuf == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}
		rcount = fread_s(inbuf, rsize, 1, rsize, pFi);
		if (rcount != rsize) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
		}
		fclose(pFi);

		unsigned __int8* buffer = GC_malloc(rcount*2);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			exit(-2);
		}

		for (size_t i = 0; i < rcount; i++) {
			buffer[i * 2] = inbuf[i].L << 4;
			buffer[i * 2 + 1] = inbuf[i].H << 4;
		}

		size_t len = 0;

		while (buffer[len] && len < rcount * 2) {
			len++;
		}

		if (len) {
			struct WAVE_header waveH;
			struct WAVE_chunk1 waveC1;
			struct WAVE_chunk2 waveC2;

			waveC2.Subchunk2ID = _byteswap_ulong(0x64617461);
			waveC2.Subchunk2Size = len;

			waveC1.Subchunk1ID = _byteswap_ulong(0x666d7420);
			waveC1.Subchunk1Size = 16;
			waveC1.AudioFormat = 1;
			waveC1.NumChannels = 1;
			waveC1.SampleRate = SampleRate;
			waveC1.BitsPerSample = 8;
			waveC1.ByteRate = waveC1.NumChannels * waveC1.SampleRate * waveC1.BitsPerSample / 8;
			waveC1.BlockAlign = waveC1.NumChannels * waveC1.BitsPerSample / 8;

			waveH.ChunkID = _byteswap_ulong(0x52494646);
			waveH.Format = _byteswap_ulong(0x57415645);
			waveH.ChunkSize = 4 + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size;

			wchar_t path[_MAX_PATH];
			wchar_t fname[_MAX_FNAME];
			wchar_t dir[_MAX_DIR];
			wchar_t drive[_MAX_DRIVE];

			_wsplitpath_s(*argv, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
			_wmakepath_s(path, _MAX_PATH, drive, dir, fname, L".WAV");
			wprintf_s(L"Data size %ld, SampleRatio %ld.\n", waveC2.Subchunk2Size, waveC1.SampleRate);
			wprintf_s(L"Output to %s.\n", path);

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
}
