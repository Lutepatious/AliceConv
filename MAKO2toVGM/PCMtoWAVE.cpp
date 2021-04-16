#include <cstdio>
#include <cstdlib>

#include "gc_cpp.h"

#include "../aclib/wave.h"
#include "MAKO2toVGM.h"
#include "PCMtoWAVE.h"

struct PM_header *pmH;
struct MP_header *mpH;
struct TSND_header *tsndH;

void PCMtoWAV(unsigned __int8* pcmdata, wchar_t* filename, size_t filelen)
{
	size_t pcm_size;
	size_t pcm_srate;
	size_t wcount;
	struct WAVE_header waveH;
	struct WAVE_chunk1 waveC1;
	struct WAVE_chunk2 waveC2;
	unsigned __int8* buffer = NULL;
	size_t len = 0;

	if (*((unsigned __int64*)pcmdata + 2) == 0 && *((unsigned __int32*)pcmdata + 3)) {
		tsndH = (TSND_header*) pcmdata;
		buffer = tsndH->body;
		// WAVE 8bitは符号なし8bit表現で、TOWNS SNDは符号1bit+7bitで-0と+0に1の差がある表現な上に中心値は-0。
		// そこでトリッキーな変換を行う
		// 負数をそのまま使うことで0x80(-0)-0xFF(-127)を0x80(128)-0xFF(255)とする。
		// 正数は反転し0x00(+0)-0x7F(127)を0x7F(127)-0x00(0)にする。
		// 自分でコード書いて忘れていたので解析する羽目になったからコメント入れた(2020.05.22)
		for (size_t i = 0; i < tsndH->Size; i++) {
			if (!(buffer[i] & 0x80)) {
				buffer[i] = ~(buffer[i] | 0x80);
			}
		}
		pcm_srate = (2000L * tsndH->tSampleRate / 98 + 1) >> 1; // 0.098で割るのではなく、2000/98を掛けて+1して2で割る
		len = tsndH->Size;
	}
	else {
		struct PCM4* pcm_inbuf = NULL;

		if (*(unsigned __int32*)pcmdata == _byteswap_ulong(0x4D506400)) {
			mpH = (struct MP_header*) pcmdata;
			wprintf_s(L"File size %ld. %ld Hz.\n", mpH->Len, mpH->sSampleRate * 100);
			pcm_size = mpH->Len - 0x10;
			pcm_srate = 100LL * mpH->sSampleRate;
			pcm_inbuf = mpH->body;
		}
		else if (*(unsigned __int16*)pcmdata == _byteswap_ushort(0x504D)) {
			pmH = (struct PM_header*) pcmdata;
			wprintf_s(L"File size %ld. %d Ch. %ld Hz. %d bits/sample.\n",
				pmH->Size, pmH->Ch, pmH->sSampleRate * 100, pmH->BitsPerSample);

			if (pmH->Ch != 1 || pmH->BitsPerSample != 4) {
				wprintf_s(L"Skip File %s.\n", filename);
				return;
			}
			pcm_size = pmH->Size;
			pcm_srate = 100LL * pmH->sSampleRate;
			pcm_inbuf = pmH->body;
		}
		else {
			wprintf_s(L"Unknown file type. %s\n", filename);
			pcm_size = filelen;
			pcm_srate = 8000;
			pcm_inbuf = (struct PCM4*) pcmdata;
		}

		buffer = (unsigned __int8*) GC_malloc(pcm_size * 2);
		if (buffer == NULL) {
			wprintf_s(L"Memory allocation error.\n");
			exit(-2);
		}

		for (size_t i = 0; i < pcm_size; i++) {
			buffer[i * 2] = pcm_inbuf[i].L << 4;
			buffer[i * 2 + 1] = pcm_inbuf[i].H << 4;
		}

		while (buffer[len] && len < pcm_size * 2) {
			len++;
		}

		if (!len) {
			return;
		}
	}

	waveC2.Subchunk2ID = _byteswap_ulong(0x64617461); // "data"
	waveC2.Subchunk2Size = len;

	waveC1.Subchunk1ID = _byteswap_ulong(0x666d7420); // "fmt "
	waveC1.Subchunk1Size = 16;
	waveC1.AudioFormat = 1;
	waveC1.NumChannels = 1;
	waveC1.SampleRate = pcm_srate;
	waveC1.BitsPerSample = 8;
	waveC1.ByteRate = waveC1.NumChannels * waveC1.SampleRate * waveC1.BitsPerSample / 8;
	waveC1.BlockAlign = waveC1.NumChannels * waveC1.BitsPerSample / 8;

	waveH.ChunkID = _byteswap_ulong(0x52494646); // "RIFF"
	waveH.Format = _byteswap_ulong(0x57415645); // "WAVE"
	waveH.ChunkSize = 4 + sizeof(waveC1) + sizeof(waveC2) + waveC2.Subchunk2Size;

	wchar_t* path = filename_replace_ext(filename, L".WAV");
	wprintf_s(L"Data size %ld, SampleRatio %ld.\n", waveC2.Subchunk2Size, waveC1.SampleRate);
	wprintf_s(L"Output to %s.\n", path);

	FILE* pFo;
	errno_t ecode = _wfopen_s(&pFo, path, L"wb");
	if (ecode || !pFo) {
		wprintf_s(L"File open error %s.\n", filename);
		exit(ecode);
	}

	wcount = fwrite(&waveH, sizeof(waveH), 1, pFo);
	if (wcount != 1) {
		wprintf_s(L"File write error %s.\n", filename);
		fclose(pFo);
		exit(-2);
	}
	wcount = fwrite(&waveC1, sizeof(waveC1), 1, pFo);
	if (wcount != 1) {
		wprintf_s(L"File write error %s.\n", filename);
		fclose(pFo);
		exit(-2);
	}
	wcount = fwrite(&waveC2, sizeof(waveC2), 1, pFo);
	if (wcount != 1) {
		wprintf_s(L"File write error %s.\n", filename);
		fclose(pFo);
		exit(-2);
	}
	wcount = fwrite(buffer, 1, waveC2.Subchunk2Size, pFo);
	if (wcount != waveC2.Subchunk2Size) {
		wprintf_s(L"File write error %s.\n", filename);
		fclose(pFo);
		exit(-2);
	}
	fclose(pFo);
}
