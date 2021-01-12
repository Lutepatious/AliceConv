#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gc.h"
#include "pngio.h"
#include "accore.h"
#include "acinternal.h"

// コンパイラオプションで構造体に隙間ができないよう、pragma packで詰めることを指定
#pragma pack (1)
// PMS 8bit depth (256色インデックスカラー)の構造体 まさかVSP256色とはヘッダ違いなだけとは思いもよらなかった。
struct PMS8 {
	unsigned __int16 Sig;
	unsigned __int16 Ver;
	unsigned __int16 len_hdr;
	unsigned __int8 depth;
	unsigned __int8 bits;
	unsigned __int32 U0;
	unsigned __int32 U1;
	unsigned __int32 Column_in;
	unsigned __int32 Row_in;
	unsigned __int32 Columns;
	unsigned __int32 Rows;
	unsigned __int32 offset_body;
	unsigned __int32 offset_Pal;
	unsigned __int32 U2;
	unsigned __int32 U3;
	unsigned __int8 data[];
};
#pragma pack ()

struct image_info* decode_PMS8(FILE* pFi)
{
	const unsigned colours = COLOR256;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct PMS8);
	struct PMS8* data = GC_malloc(fs.st_size);
	if (data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	const size_t rcount = fread_s(data, fs.st_size, 1, fs.st_size, pFi);
	if (rcount != fs.st_size) {
		wprintf_s(L"File read error.\n");
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	const size_t in_x = data->Column_in;
	const size_t in_y = data->Row_in;
	const size_t len_x = data->Columns;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_x;
	unsigned __int8* data_decoded = GC_malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	decode_d8(data_decoded, data->data + data->offset_body - data->len_hdr, len_decoded, len_x);

	static const wchar_t sType[] = L"PMS8";
	png_colorp Pal8 = GC_malloc(sizeof(png_color) * colours);
	png_bytep Trans = GC_malloc(sizeof(png_byte) * colours);
	memset(Trans, 0xFF, sizeof(png_byte) * colours);

	struct fPal8* inPal8 = data->data + data->offset_Pal - data->len_hdr;
	for (size_t ci = 0; ci < colours; ci++) {
		color_256to256(&Pal8[ci], inPal8++);
	}

	struct image_info* pI = GC_malloc(sizeof(struct image_info));
	pI->image = data_decoded;
	pI->start_x = in_x;
	pI->start_y = in_y;
	pI->len_x = len_x;
	pI->len_y = len_y;
	pI->offset_x = 0;
	pI->offset_y = 0;
	pI->colors = colours;
	pI->Pal8 = Pal8;
	pI->Trans = Trans;
	pI->sType = sType;
	pI->BGcolor = 0;

	return pI;
}