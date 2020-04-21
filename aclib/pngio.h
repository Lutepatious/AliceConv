#include "png.h"
#include "zlib.h"

struct fPNG {
	FILE* pFi;
	png_structp ppng;
	png_infop pinfo;
	png_bytepp image;
	png_colorp Pal;
	png_bytep Trans;
	png_uint_32 Rows;
	png_uint_32 Cols;
	int nPal;
	int nTrans;
	int depth;
};

struct fPNGw {
	wchar_t* outfile;
	png_bytepp image;
	png_colorp Pal;
	png_bytep Trans;
	png_uint_32 Rows;
	png_uint_32 Cols;
	int nPal;
	int nTrans;
	int depth;
	int pXY;
};

extern struct fPNG* png_open(wchar_t* infile);
extern void png_close(struct fPNG* pimg);
extern void* png_create(struct fPNGw* pngw);