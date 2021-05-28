#pragma pack(1)

struct PCM4 {
	unsigned __int8 H : 4;
	unsigned __int8 L : 4;
};

struct PM_header {
	unsigned __int16 ID; // must be 'PM' 0x504D
	unsigned __int8 Ch;
	unsigned __int16 StartAddr;
	unsigned __int8 BitsPerSample;
	unsigned __int16 sSampleRate; // *100
	unsigned __int16 Size;
	unsigned __int16 unk;
	struct PCM4 body[];
};

struct MP_header {
	unsigned __int32 ID; // must be 'MPd ' 0x4D506400
	unsigned __int16 sSampleRate; // *100
	unsigned __int16 Len; // File Length
	unsigned __int8 unk[8];
	struct PCM4 body[];
};

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
	unsigned __int8 body[];
};
#pragma pack ()

void PCMtoWAV(unsigned __int8* pcmdata, wchar_t* filename, size_t filelen);
