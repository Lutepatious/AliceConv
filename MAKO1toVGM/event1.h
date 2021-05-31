#include <limits>
// �C�x���g����
// 80 F4 F5 F9 90

struct EVENT {
	size_t time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // �C�x���g��������N�t�����\�[�g���邽�߂̂��� ����=0, �e���|=1, ����������=2, �^�C=8, ����=9���x��
	unsigned __int8 Event; // �C�x���g��{��
	unsigned __int8 Param; // �C�x���g�̃p�����[�^
};

class EVENTS {
	struct EVENT* dest;
	size_t counter = 0;
	size_t time_end = SIZE_MAX;
	void enlarge(void);

public:
	struct EVENT* event;
	size_t events;
	size_t length = 0;
	size_t loop_start = SIZE_MAX;
	bool loop_enable = false;
	EVENTS(size_t elems, size_t end);
	void convert(struct mako1_mml_decoded& MMLs);
	void sort(void);
	void print_all(void);
};
