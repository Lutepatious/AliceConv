class eomml_decoded_CH {
	bool mute = true;
public:
	unsigned __int8* MML;
	size_t len = 0;
	size_t time_total = 0;
	size_t len_unrolled = 0;
	eomml_decoded_CH();
	void decode(unsigned __int8* input, unsigned __int16 offs);
	void print(void);
	bool is_mute(void);
};

struct eomml_decoded {
	class eomml_decoded_CH* CH;
	const size_t CHs = 8;
	size_t end_time = 0;
	size_t loop_start_time = 0;

	eomml_decoded();
	void print(void);
	void unroll_loop(void);
};

