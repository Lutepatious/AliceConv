class mako2_mml_decoded_CH {
	bool Mute_on = false;

public:
	unsigned __int8* MML;
	size_t len = 0;
	size_t len_unrolled = 0;
	size_t time_total = 0;
	size_t Loop_start_pos = 0;
	size_t Loop_start_time = 0;
	size_t Loop_delta_time = 0;
	mako2_mml_decoded_CH();
	void mute_on(void);
	bool is_mute(void);
	void decode(unsigned __int8* input, unsigned __int32 offs);
};

struct mako2_mml_decoded {
	class mako2_mml_decoded_CH* CH;
	size_t CHs;
	size_t end_time = 0;
	size_t latest_CH = 0;
	size_t loop_start_time = 0;

	mako2_mml_decoded(size_t ch);
	void unroll_loop(void);
};

