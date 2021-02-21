#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "../aclib/wave.h"

#pragma pack (1)
struct TSND_header {
	unsigned char Name[8];
	unsigned __int32 ID;
	unsigned __int32 Size;
	unsigned __int32 LoopStart;
	unsigned __int32 LoopEnd;
	unsigned __int16 tSampleRate;
	unsigned __int16 unk1;
	unsigned __int8 note;
	unsigned __int8 unk2;
	unsigned __int16 unk3;
} tsndH;
#pragma pack ()

struct WAVE_header waveH;
struct WAVE_chunk1 waveC1;
struct WAVE_chunk2 waveC2;

int wmain(int argc, wchar_t **argv)
{
	FILE *pFi, *pFo;

	if (argc < 2) {
		wprintf_s(L"Usage: %s file ...\n", *argv);
		exit(-1);
	}

	while (--argc) {
		char change = 0;
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

		if ((unsigned __int32)sig == 0x01000034UL) {
			wprintf_s(L"MAKO2 (OPN) format skip.\n");
			fclose(pFi);
			continue;
		}
		else if ((unsigned __int32)sig == 0x02000044UL) {
			wprintf_s(L"MAKO2 (OPNA) format skip.\n");
			fclose(pFi);
			continue;
		}

		rcount = fread_s(&tsndH, sizeof(tsndH), sizeof(tsndH), 1, pFi);
		if (rcount != 1) {
			wprintf_s(L"File read error %s.\n", *argv);
			fclose(pFi);
			exit(-2);
		}

		unsigned __int8* buffer = GC_malloc(tsndH.Size);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			fclose(pFi);
			exit(-2);
		}

		rcount = fread_s(buffer, tsndH.Size, 1, tsndH.Size, pFi);
		if (rcount != tsndH.Size) {
			wprintf_s(L"File read error %s %zd.\n", *argv, rcount);
			fclose(pFi);
			exit(-2);
		}
		fclose(pFi);

		// WAVE 8bitは符号なし8bit表現で、TOWNS SNDは符号1bit+7bitで-0と+0に1の差がある表現な上に中心値は-0。
		// そこでトリッキーな変換を行う
		// 負数をそのまま使うことで0x80(-0)-0xFF(-127)を0x80(128)-0xFF(255)とする。
		// 正数は反転し0x00(+0)-0x7F(127)を0x7F(-1)-0x00(-128)にする。
		// 自分でコード書いて忘れていたので解析する羽目になったからコメント入れた(2020.05.22)
		for (size_t i = 0; i < rcount; i++) {
			if (!(buffer[i] & 0x80)) {
				buffer[i] = ~(buffer[i] | 0x80);
			}
		}

		waveC2.Subchunk2ID = _byteswap_ulong(0x64617461);
		waveC2.Subchunk2Size = rcount;

		waveC1.Subchunk1ID = _byteswap_ulong(0x666d7420);
		waveC1.Subchunk1Size = 16;
		waveC1.AudioFormat = 1;
		waveC1.NumChannels = 1;
		waveC1.SampleRate = (2000L * tsndH.tSampleRate / 98 + 1)  >> 1; // 0.098で割るのではなく、2000/98を掛けて+1して2で割る
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
		wprintf_s(L"Data size %lu, SampleRatio %lu.\n", waveC2.Subchunk2Size, waveC1.SampleRate);
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

	return 0;
}