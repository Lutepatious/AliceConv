#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cstdlib>

enum class CHIP { NONE = 0, YM2203, YM2608, YM2151 };

#pragma pack (1)
struct mako2_header {
	unsigned __int32 chiptune_addr : 24;
	unsigned __int32 ver : 8;
	unsigned __int32 CH_addr[]; // 12 or 16 Channels
};


struct WAVEOUT {
	std::vector<unsigned __int8> pcm8;

	struct WAVE_header {
		unsigned __int32 ChunkID; // must be 'RIFF' 0x52494646
		unsigned __int32 ChunkSize;
		unsigned __int32 Format;  // must be 'WAVE' 0x57415645
		unsigned __int32 Subchunk1ID; // must be 'fmt ' 0x666d7420
		unsigned __int32 Subchunk1Size;
		unsigned __int16 AudioFormat;
		unsigned __int16 NumChannels;
		unsigned __int32 SampleRate;
		unsigned __int32 ByteRate;
		unsigned __int16 BlockAlign;
		unsigned __int16 BitsPerSample;
		unsigned __int32 Subchunk2ID; // must be 'data' 0x64617461
		unsigned __int32 Subchunk2Size;
	} header;

	WAVEOUT()
	{
		this->header.Subchunk2ID = _byteswap_ulong(0x64617461); // "data"
		this->header.Subchunk1ID = _byteswap_ulong(0x666d7420); // "fmt "
		this->header.Subchunk1Size = 16;
		this->header.AudioFormat = 1;
		this->header.NumChannels = 1;
		this->header.BitsPerSample = 8;
		this->header.ByteRate = this->header.NumChannels * this->header.SampleRate * this->header.BitsPerSample / 8;
		this->header.BlockAlign = this->header.NumChannels * this->header.BitsPerSample / 8;
		this->header.ChunkID = _byteswap_ulong(0x52494646); // "RIFF"
		this->header.Format = _byteswap_ulong(0x57415645); // "WAVE"

		this->header.SampleRate = 0;
		this->header.Subchunk2Size = this->pcm8.size();
		this->header.ChunkSize = 36 + this->header.Subchunk2Size;
	}

	void update(void)
	{
		this->header.Subchunk2Size = this->pcm8.size();
		this->header.ChunkSize = 36 + this->header.Subchunk2Size;
	}

	wchar_t path[_MAX_PATH] = { 0 };
	void filename_replace_ext(wchar_t* outfilename, const wchar_t* newext)
	{
		wchar_t fname[_MAX_FNAME];
		wchar_t dir[_MAX_DIR];
		wchar_t drive[_MAX_DRIVE];
		_wsplitpath_s(outfilename, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_wmakepath_s(path, _MAX_PATH, drive, dir, fname, newext);
	}

	size_t out(wchar_t* p)
	{
		this->filename_replace_ext(p, L".WAV");
		std::ofstream outfile(path, std::ios::binary);
		if (!outfile) {
			std::wcerr << L"File " << p << L" open error." << std::endl;

			return 0;
		}

		outfile.write((const char*)&this->header, sizeof(WAVE_header));
		outfile.write((const char*)&this->pcm8.at(0), this->pcm8.size());
	
		outfile.close();
		return sizeof(WAVE_header) + this->pcm8.size();
	}
};

struct PCM4 {
	unsigned __int8 H : 4;
	unsigned __int8 L : 4;
};

union PCMs {
	struct TSND {
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
		unsigned __int8 body[];
	} tsnd;

	struct MP {
		unsigned __int32 ID; // must be 'MPd ' 0x4D506400
		unsigned __int16 sSampleRate; // *100
		unsigned __int16 Len; // Whole File Length
		unsigned __int8 unk[8];
		struct PCM4 body[];
	} mp;

	struct PM {
		unsigned __int16 ID; // must be 'PM' 0x504D
		unsigned __int8 Ch;
		unsigned __int16 StartAddr;
		unsigned __int8 BitsPerSample;
		unsigned __int16 sSampleRate; // *100
		unsigned __int16 Size;
		unsigned __int16 unk;
		struct PCM4 body[];
	} pm;

	struct PCM4 pcm4[];
};


#pragma pack ()


wchar_t* filename_replace_ext(wchar_t* outfilename, const wchar_t* newext)
{
	static wchar_t path[_MAX_PATH];
	wchar_t fname[_MAX_FNAME];
	wchar_t dir[_MAX_DIR];
	wchar_t drive[_MAX_DRIVE];
	_wsplitpath_s(outfilename, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
	_wmakepath_s(path, _MAX_PATH, drive, dir, fname, newext);

	return path;
}

int wmain(int argc, wchar_t** argv)
{
	bool debug = false;

	if (argc < 2) {
		std::wcerr << L"Usage: " << *argv << L" [-a | -n | -x] [-d] [-s<num>] file..." << std::endl;
		exit(-1);
	}

	unsigned __int8 SSG_Volume = 0;
	enum CHIP chip_force = CHIP::NONE;
	bool early_detune = false;
	while (--argc) {
		if (**++argv == L'-') {
			if (*(*argv + 1) == L'a') {
				chip_force = CHIP::YM2608;
			}
			else if (*(*argv + 1) == L'n') {
				chip_force = CHIP::YM2203;
			}
			else if (*(*argv + 1) == L'x') {
				chip_force = CHIP::YM2151;
			}
			else if (*(*argv + 1) == L'd') {
				early_detune = true;
			}
			else if (*(*argv + 1) == L's') {
				int tVol = _wtoi(*argv + 2);
				if (tVol > 255) {
					SSG_Volume = 255;
				}
				else if (tVol < 0) {
					SSG_Volume = 0;
				}
				else {
					SSG_Volume = tVol;
				}
			}
			continue;
		}
		enum CHIP chip = CHIP::NONE;

		std::ifstream infile(*argv, std::ios::binary);
		if (!infile) {
			std::wcerr << L"File " << *argv << L" open error." << std::endl;

			continue;
		}

		std::vector<__int8> inbuf{ std::istreambuf_iterator<__int8>(infile), std::istreambuf_iterator<__int8>() };

		infile.close();

		struct mako2_header* pM2HDR = (struct mako2_header*)&inbuf.at(0);
		union PCMs *pPCM = (union PCMs*)&inbuf.at(0);

		unsigned CHs = 0;
		unsigned mako2form = 0;
		struct WAVEOUT wout;

		if (pM2HDR->chiptune_addr == 0x34UL && pM2HDR->ver == 1) {
			mako2form = pM2HDR->ver;
			CHs = 12;
		}
		else if (pM2HDR->chiptune_addr == 0x44UL && pM2HDR->ver >= 1 && pM2HDR->ver <= 3) {
			mako2form = pM2HDR->ver;
			CHs = 16;
		}
		else if (pPCM->tsnd.LoopEnd == 0 && pPCM->tsnd.Size)
		{
			// WAVE 8bitは符号なし8bit表現で、TOWNS SNDは符号1bit+7bitで-0と+0に1の差がある表現な上に中心値は-0。
			// そこでトリッキーな変換を行う
			// 負数をそのまま使うことで0x80(-0)-0xFF(-127)を0x80(128)-0xFF(255)とする。
			// 正数は反転し0x00(+0)-0x7F(127)を0x7F(127)-0x00(0)にする。
			// 自分でコード書いて忘れていたので解析する羽目になったからコメント入れた(2020.05.22)
			for (size_t i = 0; i < pPCM->tsnd.Size; i++) {
				if (!(pPCM->tsnd.body[i] & 0x80)) {
					wout.pcm8.push_back(~(pPCM->tsnd.body[i] | 0x80));
				}
				else {
					wout.pcm8.push_back(pPCM->tsnd.body[i]);
				}
			}
			wout.header.SampleRate = (2000L * pPCM->tsnd.tSampleRate / 98 + 1) >> 1; // 0.098で割るのではなく、2000/98を掛けて+1して2で割る
		}
		else if (pPCM->mp.ID == _byteswap_ulong(0x4D506400))
		{
			for (size_t i = 0; i < pPCM->mp.Len - 0x10; i++) {
				wout.pcm8.push_back(pPCM->mp.body[i].L << 4);
				wout.pcm8.push_back(pPCM->mp.body[i].H << 4);
			}
			wout.header.SampleRate = 100LL * pPCM->mp.sSampleRate;
		}
		else if (pPCM->pm.ID == _byteswap_ushort(0x504D))
		{
			if (pPCM->pm.Ch != 1 || pPCM->pm.BitsPerSample != 4) {
				std::wcout << L"Skip File" << std::endl;
				continue;
			}
			for (size_t i = 0; i < pPCM->pm.Size; i++) {
				wout.pcm8.push_back(pPCM->pm.body[i].L << 4);
				wout.pcm8.push_back(pPCM->pm.body[i].H << 4);
			}
			wout.header.SampleRate = 100LL * pPCM->pm.sSampleRate;
		}
		else
		{
			for (size_t i = 0; i < inbuf.size(); i++) {
				wout.pcm8.push_back(pPCM->pcm4[i].L << 4);
				wout.pcm8.push_back(pPCM->pcm4[i].H << 4);
			}
			wout.header.SampleRate = 8000;
		}
		if (wout.pcm8.size()) {
			wout.update();
			wout.out(*argv);

			continue;
		}

		unsigned CHs_real = CHs;

	}
}
