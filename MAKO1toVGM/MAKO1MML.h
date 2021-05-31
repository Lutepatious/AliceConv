class mako1_mml_decoded_CH {
	bool mute = true;
public:
	unsigned __int8* MML;
	size_t len = 0;
	size_t time_total = 0;
	size_t len_unrolled = 0;
	mako1_mml_decoded_CH();
	void decode(unsigned __int8* input, unsigned __int16 offs);
	void print(void);
	bool is_mute(void);
};

struct mako1_mml_decoded {
	class mako1_mml_decoded_CH* CH;
	const size_t CHs = 6;
	size_t end_time = 0;
	size_t loop_start_time = 0;

	mako1_mml_decoded();
	void print(void);
	void unroll_loop(void);
};

