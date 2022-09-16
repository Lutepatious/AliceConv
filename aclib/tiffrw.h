#include "tiffio.h"

struct TIFF_Palette {
	unsigned __int16 R[256];
	unsigned __int16 G[256];
	unsigned __int16 B[256];
};

struct fTIFF {
	unsigned __int8 *image;
	unsigned __int32 Rows;
	unsigned __int32 Cols;
	unsigned __int16 depth;
	unsigned __int16 Format;
	struct TIFF_Palette Pal;
};

extern struct fTIFF* tiff_read(const wchar_t* infile);
