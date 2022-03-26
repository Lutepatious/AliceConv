constexpr size_t channels = 6;

struct MML_BLOCK {
	unsigned __int8 ch[channels];
};

class eomml_decoded_CH {
	bool mute = true;
public:
	unsigned __int8* MML;
	size_t len = 0;
	size_t time_total = 0;
	size_t len_unrolled = 0;
	bool x68tt = false;
	bool Oct98 = false;

	eomml_decoded_CH();
	void decode(unsigned __int8* input);
	void print(void);
	bool is_mute(void);
};

struct eomml_decoded {
	class eomml_decoded_CH* CH;
	struct MML_BLOCK* mml_block;
	unsigned __int8** block;
	unsigned __int8* mmls;
	unsigned __int8* dest;
	unsigned __int8* mpos;
	unsigned __int8* pEOF;
	unsigned __int8* mml[channels + 1] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

	const size_t CHs = channels;
	size_t mml_blocks;
	size_t end_time = 0;
	size_t loop_start_time = 0;

	eomml_decoded(unsigned __int8* header, size_t fsize, bool opm98, bool oct98);
	void decode(void);
	void print(void);
	void unroll_loop(void);
};
