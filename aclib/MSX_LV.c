#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pngio.h"
#include "accore.h"
#include "acinternal.h"

// コンパイラオプションで構造体に隙間ができないよう、pragma packで詰めることを指定
#pragma pack(1)
// MSX版Little Vampireのフォーマット
struct MSX_LV {
	unsigned __int8 M1;
	unsigned __int8 NegCols; // ~(Cols - 1)
	unsigned __int8 Rows;
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_MSX_LV(FILE* pFi)
{
	const unsigned colours = COLOR16;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct MSX_LV);
	struct MSX_LV* data = malloc(fs.st_size);
	if (data == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		fclose(pFi);
		exit(-2);
	}

	const size_t rcount = fread_s(data, fs.st_size, 1, fs.st_size, pFi);
	if (rcount != fs.st_size) {
		wprintf_s(L"File read error.\n");
		free(data);
		fclose(pFi);
		exit(-2);
	}
	fclose(pFi);

	const size_t start_x = 0;
	const size_t start_y = 0;
	const size_t len_col = data->Columns ? data->Columns : 256;
	const size_t len_x = len_col * 2;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_col;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}






	size_t decode_len = len_x * len_y;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded);
		exit(-2);
	}

	for (size_t i = 0; i < len_decoded; i++) {
		decode_buffer[i * 2] = (data_decoded[i] & 0xF0) >> 4;
		decode_buffer[i * 2 + 1] = data_decoded[i] & 0xF;
	}

	free(data_decoded);

	static struct image_info I;
	static wchar_t sType[] = L"MSX_I";
	static png_byte Trans[COLOR16 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	I.image = decode_buffer;
	I.start_x = start_x;
	I.start_y = start_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.colors = colours + 1;
	I.Pal8 = NULL;
	I.Trans = Trans;
	I.sType = sType;
	I.BGcolor = colours;

	free(data);
	return &I;
}