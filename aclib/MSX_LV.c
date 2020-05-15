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

// �R���p�C���I�v�V�����ō\���̂Ɍ��Ԃ��ł��Ȃ��悤�Apragma pack�ŋl�߂邱�Ƃ��w��
#pragma pack(1)
// MSX��Little Vampire�̃t�H�[�}�b�g
struct MSX_LV {
	unsigned __int8 M1;
	unsigned __int8 NegCols; // ~(Cols - 1)
	unsigned __int8 Rows;
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_MSX_LV(FILE* pFi)
{
	const unsigned colours = COLOR8;
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
	const size_t len_col= data->NegCols == 0x01 ? 256 : (unsigned __int8)(~(data->NegCols)) + 1LL;
	const size_t len_x = len_col * 2;
	const size_t len_y = data->Rows;
	const size_t len_decoded = len_y * len_col;
	unsigned __int8* data_decoded = malloc(len_decoded);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	__int64 count = len, cp_len;
	size_t rows_real = 0;
	unsigned __int8* src = data->body, * dst = data_decoded, prev = ~*src, repeat = 0;

	while (count-- > 0 && (dst - data_decoded) < len_decoded) {
		if ((src - data) % 0x100 == 0) {
			repeat = 0;
			prev = ~*src;
		}
		if (*src == 0xFF && *(src + 1) == 0x1A) {
			break;
		}
		else if (*src == 0xF3 && *(src + 1) == 0xFF) {
			src += 2;
			count--;
		}
		else if (repeat) {
			repeat = 0;
			cp_len = *src - 2;
			if (cp_len > 0) {
				memset(dst, prev, cp_len);
			}
			src++;
			dst += cp_len;
			prev = ~*src;
		}
		else if (*src == 0x88) {
			prev = *src;
			src++;
			rows_real++;
		}
		else if (*src == prev) {
			repeat = 1;
			prev = *src;
			*dst++ = *src++;
		}
		else {
			repeat = 0;
			prev = *src;
			*dst++ = *src++;
		}
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

	struct fPal8 Pal3[8] = { { 0x0, 0x0, 0x0 }, { 0x7, 0x0, 0x0 }, { 0x0, 0x7, 0x0 }, { 0x7, 0x7, 0x0 },
						{ 0x0, 0x0, 0x7 }, { 0x7, 0x0, 0x7 }, { 0x0, 0x7, 0x7 }, { 0x7, 0x7, 0x7 } };

	static struct image_info I;
	static wchar_t sType[] = L"MSX_LV";
	static png_color Pal8[COLOR8 + 1];
	static png_byte Trans[COLOR8 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		color_8to256(&Pal8[ci], &Pal3[ci]);
	}
	color_8to256(&Pal8[colours], NULL);


	I.image = decode_buffer;
	I.start_x = start_x;
	I.start_y = start_y;
	I.len_x = len_x;
	I.len_y = len_y;
	I.colors = colours + 1;
	I.Pal8 = Pal8;
	I.Trans = Trans;
	I.sType = sType;
	I.BGcolor = colours;

	free(data);
	return &I;
}