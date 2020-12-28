// コンパイラオプションで構造体に隙間ができないよう、pragma packで詰めることを指定
#pragma pack (1)
// PC88系のGLフォーマットのパレット情報
struct GL_Palette3 {
	unsigned __int16 B : 3;
	unsigned __int16 R : 3;
	unsigned __int16 u0 : 2;
	unsigned __int16 G : 3;
	unsigned __int16 u1 : 3;
	unsigned __int16 u2 : 2;
};

// X68000フォーマットのパレット情報
struct X68_Palette5 {
	unsigned __int16 I : 1;
	unsigned __int16 B : 5;
	unsigned __int16 R : 5;
	unsigned __int16 G : 5;
};

// MSXフォーマットのパレット情報
struct MSX_Palette {
	unsigned __int16 B : 4;
	unsigned __int16 R : 4;
	unsigned __int16 G : 4;
	unsigned __int16 u0 : 4;
};


struct plane4_dot8 {
	unsigned __int8 pix8[4];
};

// 4bitパックトピクセルにおける配置
struct PackedPixel4 {
	unsigned __int8	L : 4;
	unsigned __int8 H : 4;
};
#pragma pack ()

#define COLOR8 (8)
#define COLOR16 (16)
#define COLOR256 (256)
#define COLOR65536 (65536)

extern void convert_8dot_plane4_to_index8(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row);
extern void convert_8dot_plane3_to_index8(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row);
extern void convert_plane4_dot8_to_index8(unsigned __int64* dst, const struct plane4_dot8* src, size_t len);
extern unsigned __int8* convert_index4_to_index8_LE(const unsigned __int8* src, size_t len);
extern unsigned __int8* convert_index4_to_index8_BE(const unsigned __int8* src, size_t len);
