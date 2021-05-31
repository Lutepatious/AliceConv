#include <limits>
// イベント順序
// 80 F4 F5 F9 90

struct EVENT {
	size_t time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // イベント種をランク付けしソートするためのもの 消音=0, テンポ=1, 音源初期化=2, タイ=8, 発音=9程度で
	unsigned __int8 Event; // イベント種本体
	unsigned __int8 Param; // イベントのパラメータ
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
