#define CHs_MAX (8)

class eomml_decoded_CH {
	bool mute = true;
public:
	unsigned __int8* MML;
	size_t len = 0;
	size_t time_total = 0;
	size_t len_unrolled = 0;
	eomml_decoded_CH();
	void decode(unsigned __int8* input);
	void print(void);
	bool is_mute(void);
};

struct eomml_decoded {
	class eomml_decoded_CH* CH;
	size_t CHs = CHs_MAX;
	size_t end_time = 0;
	size_t loop_start_time = 0;

	eomml_decoded(size_t ch = CHs_MAX);
	void print(void);
	void unroll_loop(void);
};

