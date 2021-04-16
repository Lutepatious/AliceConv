class mako2_mml_decoded_CH {
	bool Mute_on;

public:
	unsigned __int8* MML;
	size_t len;
	size_t len_unrolled;
	size_t time_total;
	size_t Loop_start_pos;
	size_t Loop_start_time;
	size_t Loop_delta_time;
	mako2_mml_decoded_CH();
	void mute_on(void);
	bool is_mute(void);
	void decode(unsigned __int8* input);
};

struct mako2_mml_decoded {
	class mako2_mml_decoded_CH* CH;
	size_t CHs;
	size_t end_time;
	size_t latest_CH;
	size_t loop_start_time;

	mako2_mml_decoded(size_t ch);
	void unroll_loop(void);
};

