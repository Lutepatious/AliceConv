#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#pragma pack (1)
struct WAVE_header {
	unsigned __int32 ChunkID; // must be 'RIFF' 0x52494646
	unsigned __int32 ChunkSize;
	unsigned __int32 Format;  // must be 'WAVE' 0x57415645
} waveH;

struct WAVE_chunk1 {
	unsigned __int32 Subchunk1ID; // must be 'fmt ' 0x666d7420
	unsigned __int32 Subchunk1Size;
	unsigned __int16 AudioFormat;
	unsigned __int16 NumChannels;
	unsigned __int32 SampleRate;
	unsigned __int32 ByteRate;
	unsigned __int16 BlockAlign;
	unsigned __int16 BitsPerSample;
} waveC1;

struct WAVE_chunk2 {
	unsigned __int32 Subchunk2ID; // must be 'data' 0x64617461
	unsigned __int32 Subchunk2Size;
} waveC2;

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

unsigned __int8 *inbuf;
unsigned __int8 *buffer;

#pragma pack ()
int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		unsigned __int32 SampleRate;
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
			wprintf_s(L"File open error %s.\n", *argv);
			exit(ecode);
		}

		struct __stat64 fs;
		_fstat64(_fileno(pFi), &fs);
		wprintf_s(L"File size %I64d.\n", fs.st_size);

		size_t rcount = fread_s(&pmH, sizeof(pmH), sizeof(pmH), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}
		if (pmH.ID == _byteswap_ushort(0x504D)) {
			wprintf_s(L"File size %ld. %d Ch. %ld Hz. %d bits/sample.\n",
				pmH.Size, pmH.Ch, pmH.sSampleRate * 100, pmH.BitsPerSample);

			if (pmH.Ch != 1 || pmH.BitsPerSample != 4) {
				wprintf_s(L"Skip File %s.\n", *argv);
				fclose(pFi);
				continue;
			}
			size_t rsize = pmH.Size;
			SampleRate = 100L * pmH.sSampleRate;
			inbuf = malloc(rsize);
			if (inbuf == NULL) {
				wprintf_s(L"Memory allocation error.\n");
				fclose(pFi);
				exit(-2);
			}

			rcount = fread_s(inbuf, rsize, 1, rsize, pFi);
			if (rcount != rsize) {
				wprintf_s(L"File read error %s %d.\n", *argv, rcount);
			}
			fclose(pFi);

		} else {
			fseek(pFi, 0, SEEK_SET);
			rcount = fread_s(&mpH, sizeof(mpH), sizeof(mpH), 1, pFi);
			if (rcount != 1) {
				wprintf_s(L"File read error %s.\n", *argv);
				fclose(pFi);
				exit(-2);
			}
			if (mpH.ID == _byteswap_ulong(0x4D506400)) {
				wprintf_s(L"File size %ld. %ld Hz.\n", mpH.Len, mpH.sSampleRate * 100);
				size_t rsize = mpH.Len - 0x10;
				SampleRate = 100L * mpH.sSampleRate;
				inbuf = malloc(rsize);
				if (inbuf == NULL) {
					wprintf_s(L"Memory allocation error.\n");
					fclose(pFi);
					exit(-2);
				}

				rcount = fread_s(inbuf, rsize, 1, rsize, pFi);
				if (rcount != rsize) {
					wprintf_s(L"File read error %s %d.\n", *argv, rcount);
				}
				fclose(pFi);

			}
			else {
				size_t rsize = fs.st_size;
				wprintf_s(L"Unknown file type. %s\n", *argv);
				fseek(pFi, 0, SEEK_SET);
				inbuf = malloc(rsize);
				if (inbuf == NULL) {
					wprintf_s(L"Memory allocation error.\n");
					fclose(pFi);
					exit(-2);
				}
				SampleRate = 8000;
				rcount = fread_s(inbuf, rsize, 1, rsize, pFi);
				if (rcount != rsize) {
					wprintf_s(L"File read error %s %d.\n", *argv, rcount);
				}
				fclose(pFi);
			}
		}

		buffer = malloc(rcount*2);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			free(inbuf);
			exit(-2);
		}

		for (size_t i = 0; i < rcount; i++) {
			buffer[i*2] = inbuf[i] & 0xF0;
			buffer[i * 2 + 1] = (inbuf[i] << 4) & 0xF0;
		}

		free(inbuf);

		size_t len = 0;

		while (buffer[len] && len < rcount * 2) {
			len++;
		}

		if (len) {
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
			if (ecode) {
				wprintf_s(L"File open error %s.\n", *argv);
				free(buffer);
				exit(ecode);
			}

			rcount = fwrite(&waveH, sizeof(waveH), 1, pFo);
			if (rcount != 1) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				free(buffer);
				exit(-2);
			}
			rcount = fwrite(&waveC1, sizeof(waveC1), 1, pFo);
			if (rcount != 1) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				free(buffer);
				exit(-2);
			}
			rcount = fwrite(&waveC2, sizeof(waveC2), 1, pFo);
			if (rcount != 1) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				free(buffer);
				exit(-2);
			}
			rcount = fwrite(buffer, 1, waveC2.Subchunk2Size, pFo);
			if (rcount != waveC2.Subchunk2Size) {
				wprintf_s(L"File write error %s.\n", *argv);
				fclose(pFo);
				free(buffer);
				exit(-2);
			}
			fclose(pFo);
		}

		free(buffer);
	}
}
