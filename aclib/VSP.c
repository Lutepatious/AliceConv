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
// VSP 16色フォーマット
struct VSP {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct {
		unsigned __int8 B;
		unsigned __int8 R;
		unsigned __int8 G;
	} Pal4[16];
	unsigned __int8 body[];
};
#pragma pack ()

struct image_info* decode_VSP(FILE* pFi)
{
	const size_t planes = 4;
	const size_t depth = 8;
	const unsigned colours = COLOR16;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct VSP);
	struct VSP* data = GC_malloc(fs.st_size);
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

	const size_t in_col = data->Column_in;
	const size_t in_x = in_col * 8;
	const size_t in_y = data->Row_in;
	const size_t out_col = data->Column_out;
	const size_t out_x = out_col * 8;
	const size_t out_y = data->Row_out;
	const size_t len_col = data->Column_out - data->Column_in;
	const size_t len_x = len_col * 8;
	const size_t len_y = data->Row_out - data->Row_in;
	const size_t len_decoded = len_y * len_col * planes;

	unsigned __int8* data_decoded = GC_malloc(len_decoded + 512);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		exit(-2);
	}

	decode_d4_VSP(data_decoded, data->body, len_decoded, len_y, planes);

	struct plane4_dot8* buffer_dot8 = convert_CPY_to_YCP(data_decoded, len_y, len_col, planes);
	unsigned __int8* decode_buffer = convert_plane4_dot8_to_index8(buffer_dot8, len_y * len_col);

	static const wchar_t sType[] = L"VSP";
	png_colorp Pal8 = GC_malloc(sizeof(png_color) * (colours + 1));
	png_bytep Trans = GC_malloc(sizeof(png_byte) * (colours + 1));
	memset(Trans, 0xFF, sizeof(png_byte) * (colours + 1));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		struct fPal8 inPal4;
		inPal4.R = data->Pal4[ci].R;
		inPal4.G = data->Pal4[ci].G;
		inPal4.B = data->Pal4[ci].B;
		color_16to256(&Pal8[ci], &inPal4);
	}
	color_16to256(&Pal8[colours], NULL);

	struct image_info* pI = GC_malloc(sizeof(struct image_info));
	pI->image = decode_buffer;
	pI->start_x = in_x;
	pI->start_y = in_y;
	pI->len_x = len_x;
	pI->len_y = len_y;
	pI->offset_x = 0;
	pI->offset_y = 0;
	pI->colors = colours + 1;
	pI->Pal8 = Pal8;
	pI->Trans = Trans;
	pI->sType = sType;
	pI->BGcolor = colours;

	return pI;
}