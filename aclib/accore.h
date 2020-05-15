struct image_info {
	png_bytep image;
	png_colorp Pal8;
	png_bytep Trans;
	wchar_t* sType;
	size_t start_x;
	size_t start_y;
	size_t len_x;
	size_t len_y;
	size_t offset_x;
	size_t offset_y;
	unsigned __int32 colors;
	unsigned __int8 BGcolor;
};

struct image_info* decode_GL(FILE* pFi);
struct image_info* decode_GL3(FILE* pFi, int isGM3);
struct image_info* decode_X68R(FILE* pFi);
struct image_info* decode_VSP200l(FILE* pFi);
struct image_info* decode_VSP(FILE* pFi);
struct image_info* decode_X68T(FILE* pFi);
struct image_info* decode_X68V(FILE* pFi);
struct image_info* decode_VSP256(FILE* pFi);
struct image_info* decode_MSX_DRS(FILE* pFi);
struct image_info* decode_MSX_TT(FILE* pFi);
struct image_info* decode_MSX_GL(FILE* pFi);
struct image_info* decode_MSX_I(FILE* pFi);
struct image_info* decode_MSX_LV(FILE* pFi);
struct image_info* decode_MSX_LP(FILE* pFi);