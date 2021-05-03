#pragma pack(1)
struct LFO_soft_volume_SSG {
	unsigned __int16 Volume;
	unsigned __int16 Wait1;
	unsigned __int16 Delta2;
	unsigned __int16 Delta_last;
	unsigned __int16 Delta1;
};

struct LFO_soft_volume_FM {
	unsigned __int16 Wait1;
	unsigned __int16 Wait2;
	__int16 Delta1;
	__int16 Limit;
};

struct LFO_soft_detune {
	unsigned __int16 Wait1;
	unsigned __int16 Wait2;
	__int16 Delta1;
	__int16 Limit;
};
#pragma pack()

// �C�x���g����
// 80 F4 F5 E7 E8 F9 FC E5 EB E9 90

struct EVENT {
	size_t time;
	size_t Count;
	unsigned __int8 CH; //
	unsigned __int8 Type; // �C�x���g��������N�t�����\�[�g���邽�߂̂��� ����=0, �e���|=1, ����������=2, �^�C=8, ����=9���x��
	unsigned __int8 Event; // �C�x���g��{��
	unsigned __int8 Param[3]; // �C�x���g�̃p�����[�^
	__int16 ParamW; // �C�x���g�̃p�����[�^�ALFO�p�̂��̃v���O�����̓����p
};

class EVENTS {
	struct EVENT* dest;
	size_t counter = 0;
	size_t time_loop_start = 0;
	size_t time_end = SIZE_MAX;
	unsigned __int8 Volume_current = 0;
	unsigned __int8 Volume_prev = 0;
	__int16 Detune_current = 0;
	__int16 Detune_prev = 0;

	bool Disable_note_off = false;
	bool Disable_LFO = false;

	bool sLFOv_ready = false;
	bool sLFOd_ready = false;
	bool sLFOv_direction = false;
	bool sLFOd_direction = false;


	struct {
		struct LFO_soft_volume_SSG Param;
		unsigned Mode;
		int Volume;
		unsigned __int16 Delta;
		unsigned __int16 Wait;
	} sLFOv_SSG = { { 0, 0, 0, 0, 0 }, 0, 0, 0, 0 };

	struct {
		struct LFO_soft_volume_FM Param;
		unsigned __int16 Wait;
		int Volume;
	} sLFOv_FM = { { 0, 0, 0, 0}, 0, 0 };

	struct {
		struct LFO_soft_detune Param;
		unsigned __int16 Wait;
		int Detune;
	} sLFOd = { { 0, 0, 0, 0 }, 0, 0 };

	void enlarge(void);
	void sLFOv_setup_FM(void);
	void sLFOv_setup_SSG(void);
	void sLFOd_setup(void);
	void sLFOv_exec_FM(void);
	void sLFOv_exec_SSG(void);
	void sLFOd_exec(void);
	void LFO_note_off(void);
	void init(void);

public:
	struct EVENT* event;
	size_t events;
	size_t length = 0;
	size_t loop_start = SIZE_MAX;
	bool loop_enable = false;
	EVENTS(size_t elems, size_t end);
	void convert(struct mako2_mml_decoded& MMLs, bool direction = false);
	void sort(void);
	void print_all(void);
};
