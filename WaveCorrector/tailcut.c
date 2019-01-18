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

#pragma pack ()

unsigned __int8 *buffer;

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}


	while (--argc) {
		char change=0;
		errno_t ecode = _wfopen_s(&pFi, *++argv, L"rb");
		if (ecode) {
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

		if (fs.st_size > waveH.ChunkSize + 8) {
			wprintf_s(L"The file %s too long. %ld bytes.\n", *argv, waveH.ChunkSize + 8);
		}

		if (fs.st_size < waveH.ChunkSize + 8) {
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
			wprintf_s(L"The file %s not match. %ld bytes.\n", *argv, sizeof(waveH) + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size);
		}

		if (waveH.ChunkSize != 4 + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size) {
			change = 1;
			wprintf_s(L"The file %s not match. %ld bytes.\n", *argv, sizeof(waveH) + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size);
		}

		buffer = malloc(waveC2.Subchunk2Size);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			exit(-2);
		}

		rcount = fread_s(buffer, waveC2.Subchunk2Size, 1, waveC2.Subchunk2Size, pFi);
		if (rcount != waveC2.Subchunk2Size) {
			wprintf_s(L"File read error %s %d.\n", *argv, rcount);
		}
		fclose(pFi);

		// tail check
		if (waveC1.BitsPerSample == 8 && waveC1.NumChannels == 1) {
			while (rcount && buffer[rcount - 1] == 0) {
				rcount--;
			}
		}
		if (rcount != waveC2.Subchunk2Size) {
			wprintf_s(L"The file %s needs truncate %d to %d.\n", *argv, waveC2.Subchunk2Size, rcount);
			change = 1;
			waveC2.Subchunk2Size = rcount;
		}

		if (change) {
			waveH.ChunkSize = 4 + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size;

			ecode = _wfopen_s(&pFo, *argv, L"wb");
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

	return 0;
}