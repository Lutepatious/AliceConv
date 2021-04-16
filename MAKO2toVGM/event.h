struct EVENT {
	size_t time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // �C�x���g��������N�t�����\�[�g���邽�߂̂��� �e���|=10, ����=0, ����������=20, ����=30���x��
	unsigned __int8 Event; // �C�x���g��{��
	union { // �C�x���g�̃p�����[�^
		unsigned __int8 B[10];
		unsigned __int16 W[5];
	} Param;
};

class EVENTS {
	struct EVENT* event;
	struct EVENT* dest;
	size_t counter;
	size_t time_loop_start;
	size_t time_end;

	struct EVENT* enlarge(size_t len_cur);

public:
	size_t events;
	EVENTS(size_t elems, size_t end);
	void convert(struct mako2_mml_decoded& MMLs);
	void sort(void);
};
