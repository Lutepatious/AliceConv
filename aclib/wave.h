#pragma pack (1)
struct WAVE_header {
	unsigned __int32 ChunkID; // must be 'RIFF' 0x52494646
	unsigned __int32 ChunkSize;
	unsigned __int32 Format;  // must be 'WAVE' 0x57415645
};

struct WAVE_chunk1 {
	unsigned __int32 Subchunk1ID; // must be 'fmt ' 0x666d7420
	unsigned __int32 Subchunk1Size;
	unsigned __int16 AudioFormat;
	unsigned __int16 NumChannels;
	unsigned __int32 SampleRate;
	unsigned __int32 ByteRate;
	unsigned __int16 BlockAlign;
	unsigned __int16 BitsPerSample;
};

struct WAVE_chunk2 {
	unsigned __int32 Subchunk2ID; // must be 'data' 0x64617461
	unsigned __int32 Subchunk2Size;
};
#pragma pack ()
