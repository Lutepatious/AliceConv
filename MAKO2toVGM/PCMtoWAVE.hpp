#include <iostream>
#include <cstdlib>

#pragma pack(push)
#pragma pack(1)

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
		this->header.BlockAlign = this->header.NumChannels * this->header.BitsPerSample / 8;
		this->header.ChunkID = _byteswap_ulong(0x52494646); // "RIFF"
		this->header.Format = _byteswap_ulong(0x57415645); // "WAVE"

		this->header.SampleRate = 0;
		this->header.Subchunk2Size = this->pcm8.size();
		this->header.ChunkSize = 36 + this->header.Subchunk2Size;
		this->header.ByteRate = this->header.NumChannels * this->header.SampleRate * this->header.BitsPerSample / 8;
	}

	void update(size_t freq)
	{
		this->header.SampleRate = freq;
		this->header.ByteRate = this->header.NumChannels * this->header.SampleRate * this->header.BitsPerSample / 8;
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

#pragma pack(pop)

