struct image_info {
	png_bytep image;
	png_colorp Pal8;
	png_bytep Trans;
	size_t start_x;
	size_t start_y;
	size_t len_x;
	size_t len_y;
	unsigned __int32 colors;
};

struct image_info* decode_GL(FILE* pFi);
struct image_info* decode_GL3(FILE* pFi, int isGM3);
struct image_info* decode_X68R(FILE* pFi);
struct image_info* decode_VSP200l(FILE* pFi);
struct image_info* decode_VSP(FILE* pFi);
struct image_info* decode_X68T(FILE* pFi);
