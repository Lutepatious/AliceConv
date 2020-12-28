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
#pragma pack (1)
// VSP 200���C���t�H�[�}�b�g
struct VSP200l {
	unsigned __int16 Column_in; // divided by 8
	unsigned __int16 Row_in;
	unsigned __int16 Column_out; // divided by 8
	unsigned __int16 Row_out;
	unsigned __int8 Unknown[2];
	struct GL_Palette3 Pal3[8];
	unsigned __int8 body[];
};
#pragma pack()

struct image_info* decode_VSP200l(FILE* pFi)
{
	const size_t planes = 3;
	const size_t depth = 8;
	const unsigned colours = COLOR8;
	struct __stat64 fs;
	_fstat64(_fileno(pFi), &fs);

	const size_t len = fs.st_size - sizeof(struct VSP200l);
	struct VSP200l* data = malloc(fs.st_size);
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

	unsigned __int8* data_decoded = malloc(len_decoded + 512);
	if (data_decoded == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data);
		exit(-2);
	}

	decode_d4_VSP(data_decoded, data->body, len_decoded, len_y, planes);

	// �f�[�^�̕��т�[x][plane][y]����[y*x][plane]�ɕς���B
	const size_t len_dot8 = len_col * len_y;
	struct plane4_dot8* buffer_dot8 = calloc(len_dot8, sizeof(struct plane4_dot8));
	if (buffer_dot8 == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded);
		exit(-2);
	}

	struct plane4_dot8* buffer_dot8_dest = buffer_dot8;
	for (size_t iy = 0; iy < len_y; iy++) {
		for (size_t ix = 0; ix < len_col; ix++) {
			for (size_t ip = 0; ip < planes; ip++) {
				buffer_dot8_dest->pix8[ip] = data_decoded[(ix * planes + ip) * len_y + iy];
			}
			buffer_dot8_dest++;
		}
	}
	free(data_decoded);


	size_t decode_len = len_x * len_y;
	unsigned __int8* decode_buffer = malloc(decode_len);
	if (decode_buffer == NULL) {
		wprintf_s(L"Memory allocation error.\n");
		free(data_decoded);
		exit(-2);
	}

	convert_plane4_dot8_to_index8(decode_buffer, buffer_dot8, len_dot8);
	free(buffer_dot8);

	static struct image_info I;
	static wchar_t sType[] = L"VSP200l";
	static png_color Pal8[COLOR8 + 1];
	static png_byte Trans[COLOR8 + 1];

	memset(Trans, 0xFF, sizeof(Trans));
	Trans[colours] = 0;

	for (size_t ci = 0; ci < colours; ci++) {
		struct fPal8 inPal3;
		inPal3.R = data->Pal3[ci].R;
		inPal3.G = data->Pal3[ci].G;
		inPal3.B = data->Pal3[ci].B;
		color_8to256(&Pal8[ci], &inPal3);
	}
	color_8to256(&Pal8[colours], NULL);

	I.image = decode_buffer;
	I.start_x = in_x;
	I.start_y = in_y;
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