struct EVENT {
	size_t time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの テンポ=10, 消音=0, 音源初期化=20, 発音=30程度で
	unsigned __int8 Event; // イベント種本体
	union { // イベントのパラメータ
		unsigned __int8 B[10];
		unsigned __int16 W[5];
	} Param;
};

class EVENTS {
	struct EVENT* event;
	struct EVENT* dest;
	size_t counter;

public:
	size_t events;
	size_t delta;
	EVENTS(size_t elems, size_t elems_delta);
	struct EVENT* enlarge(size_t len_cur);
	void convert(struct mako2_mml_decoded& MMLs);
};
