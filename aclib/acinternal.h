unsigned __int8 convert_8dot_from_4plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row);
unsigned __int8 convert_8dot_from_3plane_to_8bitpackedpixel(unsigned __int64* dst, const unsigned __int8* src, size_t col, size_t row);

// �R���p�C���I�v�V�����ō\���̂Ɍ��Ԃ��ł��Ȃ��悤�Apragma pack�ŋl�߂邱�Ƃ��w��
#pragma pack (1)
// PC88�n��GL�t�H�[�}�b�g�̃p���b�g���
struct GL_Palette3 {
	unsigned __int16 B : 3;
	unsigned __int16 R : 3;
	unsigned __int16 u0 : 2;
	unsigned __int16 G : 3;
	unsigned __int16 u1 : 3;
	unsigned __int16 u2 : 2;
};

// X68000�t�H�[�}�b�g�̃p���b�g���
struct X68_Palette5 {
	unsigned __int16 I : 1;
	unsigned __int16 B : 5;
	unsigned __int16 R : 5;
	unsigned __int16 G : 5;
};
#pragma pack ()

#define COLOR8 (8)
#define COLOR16 (16)
#define COLOR256 (256)
