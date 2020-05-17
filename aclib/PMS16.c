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
#pragma pack (1)
struct PMS16 {
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

struct image_info* decode_PMS16(FILE* pFi)
{
	const unsigned colours = COLOR65536;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct PMS16);
	struct PMS16* data = malloc(fs.st_size);
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

	const size_t in_x = data->Column_in;
	const size_t in_y = data->Row_in;
	const size_t len_x = data->Columns;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_x;

	wprintf_s(L"Start %3lu/%3lu %3lu*%3lu %04X %1u %2u depth %1u %1u body %08lX pal %08lX unkonwn %08lX %08lX %08lX %08lX\n"
		, data->Column_in, data->Row_in, data->Columns, data->Rows, data->Sig, data->Ver, data->len_hdr, data->depth, data->bits
		, data->offset_body, data->offset_Pal, data->U0, data->U1, data->U2, data->U3);


	return NULL;
}