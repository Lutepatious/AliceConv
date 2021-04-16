wchar_t* filename_replace_ext(wchar_t* outfilename, const wchar_t* newext);
void PCMtoWAV(unsigned __int8* pcmdata, wchar_t* filename, size_t filelen);
size_t LCM(size_t a, size_t b);

enum class CHIP { NONE = 0, YM2203, YM2608, YM2151 };
