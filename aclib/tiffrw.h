#include "tiffio.h"

struct TIFF_Palette {
	uint16 R[256];
	uint16 G[256];
	uint16 B[256];
};

struct fTIFF {
	unsigned __int8 *image;
	uint32 Rows;
	uint32 Cols;
	uint16 depth;
	uint16 Format;
	struct TIFF_Palette Pal;
};

extern struct fTIFF* tiff_read(const wchar_t* infile);
