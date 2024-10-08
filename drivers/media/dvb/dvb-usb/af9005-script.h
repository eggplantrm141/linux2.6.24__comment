/*
File automatically generated by createinit.py using data
extracted from AF05BDA.sys (windows driver):

dd if=AF05BDA.sys of=initsequence bs=1 skip=88316 count=1110
python createinit.py > af9005-script.h

*/

typedef struct {
	u16 reg;
	u8 pos;
	u8 len;
	u8 val;
} RegDesc;

RegDesc script[] = {
	{0xa180, 0x0, 0x8, 0xa},
	{0xa181, 0x0, 0x8, 0xd7},
	{0xa182, 0x0, 0x8, 0xa3},
	{0xa0a0, 0x0, 0x8, 0x0},
	{0xa0a1, 0x0, 0x5, 0x0},
	{0xa0a1, 0x5, 0x1, 0x1},
	{0xa0c0, 0x0, 0x4, 0x1},
	{0xa20e, 0x4, 0x4, 0xa},
	{0xa20f, 0x0, 0x8, 0x40},
	{0xa210, 0x0, 0x8, 0x8},
	{0xa32a, 0x0, 0x4, 0xa},
	{0xa32c, 0x0, 0x8, 0x20},
	{0xa32b, 0x0, 0x8, 0x15},
	{0xa1a0, 0x1, 0x1, 0x1},
	{0xa000, 0x0, 0x1, 0x1},
	{0xa000, 0x1, 0x1, 0x0},
	{0xa001, 0x1, 0x1, 0x1},
	{0xa001, 0x0, 0x1, 0x0},
	{0xa001, 0x5, 0x1, 0x0},
	{0xa00e, 0x0, 0x5, 0x10},
	{0xa00f, 0x0, 0x3, 0x4},
	{0xa00f, 0x3, 0x3, 0x5},
	{0xa010, 0x0, 0x3, 0x4},
	{0xa010, 0x3, 0x3, 0x5},
	{0xa016, 0x4, 0x4, 0x3},
	{0xa01f, 0x0, 0x6, 0xa},
	{0xa020, 0x0, 0x6, 0xa},
	{0xa2bc, 0x0, 0x1, 0x1},
	{0xa2bc, 0x5, 0x1, 0x1},
	{0xa015, 0x0, 0x8, 0x50},
	{0xa016, 0x0, 0x1, 0x0},
	{0xa02a, 0x0, 0x8, 0x50},
	{0xa029, 0x0, 0x8, 0x4b},
	{0xa614, 0x0, 0x8, 0x46},
	{0xa002, 0x0, 0x5, 0x19},
	{0xa003, 0x0, 0x5, 0x1a},
	{0xa004, 0x0, 0x5, 0x19},
	{0xa005, 0x0, 0x5, 0x1a},
	{0xa008, 0x0, 0x8, 0x69},
	{0xa009, 0x0, 0x2, 0x2},
	{0xae1b, 0x0, 0x8, 0x69},
	{0xae1c, 0x0, 0x8, 0x2},
	{0xae1d, 0x0, 0x8, 0x2a},
	{0xa022, 0x0, 0x8, 0xaa},
	{0xa006, 0x0, 0x8, 0xc8},
	{0xa007, 0x0, 0x2, 0x0},
	{0xa00c, 0x0, 0x8, 0xba},
	{0xa00d, 0x0, 0x2, 0x2},
	{0xa608, 0x0, 0x8, 0xba},
	{0xa60e, 0x0, 0x2, 0x2},
	{0xa609, 0x0, 0x8, 0x80},
	{0xa60e, 0x2, 0x2, 0x3},
	{0xa00a, 0x0, 0x8, 0xb6},
	{0xa00b, 0x0, 0x2, 0x0},
	{0xa011, 0x0, 0x8, 0xb9},
	{0xa012, 0x0, 0x2, 0x0},
	{0xa013, 0x0, 0x8, 0xbd},
	{0xa014, 0x0, 0x2, 0x2},
	{0xa366, 0x0, 0x1, 0x1},
	{0xa2bc, 0x3, 0x1, 0x0},
	{0xa2bd, 0x0, 0x8, 0xa},
	{0xa2be, 0x0, 0x8, 0x14},
	{0xa2bf, 0x0, 0x8, 0x8},
	{0xa60a, 0x0, 0x8, 0xbd},
	{0xa60e, 0x4, 0x2, 0x2},
	{0xa60b, 0x0, 0x8, 0x86},
	{0xa60e, 0x6, 0x2, 0x3},
	{0xa001, 0x2, 0x2, 0x1},
	{0xa1c7, 0x0, 0x8, 0xf5},
	{0xa03d, 0x0, 0x8, 0xb1},
	{0xa616, 0x0, 0x8, 0xff},
	{0xa617, 0x0, 0x8, 0xad},
	{0xa618, 0x0, 0x8, 0xad},
	{0xa61e, 0x3, 0x1, 0x1},
	{0xae1a, 0x0, 0x8, 0x0},
	{0xae19, 0x0, 0x8, 0xc8},
	{0xae18, 0x0, 0x8, 0x61},
	{0xa140, 0x0, 0x8, 0x0},
	{0xa141, 0x0, 0x8, 0xc8},
	{0xa142, 0x0, 0x7, 0x61},
	{0xa023, 0x0, 0x8, 0xff},
	{0xa021, 0x0, 0x8, 0xad},
	{0xa026, 0x0, 0x1, 0x0},
	{0xa024, 0x0, 0x8, 0xff},
	{0xa025, 0x0, 0x8, 0xff},
	{0xa1c8, 0x0, 0x8, 0xf},
	{0xa2bc, 0x1, 0x1, 0x0},
	{0xa60c, 0x0, 0x4, 0x5},
	{0xa60c, 0x4, 0x4, 0x6},
	{0xa60d, 0x0, 0x8, 0xa},
	{0xa371, 0x0, 0x1, 0x1},
	{0xa366, 0x1, 0x3, 0x7},
	{0xa338, 0x0, 0x8, 0x10},
	{0xa339, 0x0, 0x6, 0x7},
	{0xa33a, 0x0, 0x6, 0x1f},
	{0xa33b, 0x0, 0x8, 0xf6},
	{0xa33c, 0x3, 0x5, 0x4},
	{0xa33d, 0x4, 0x4, 0x0},
	{0xa33d, 0x1, 0x1, 0x1},
	{0xa33d, 0x2, 0x1, 0x1},
	{0xa33d, 0x3, 0x1, 0x1},
	{0xa16d, 0x0, 0x4, 0xf},
	{0xa161, 0x0, 0x5, 0x5},
	{0xa162, 0x0, 0x4, 0x5},
	{0xa165, 0x0, 0x8, 0xff},
	{0xa166, 0x0, 0x8, 0x9c},
	{0xa2c3, 0x0, 0x4, 0x5},
	{0xa61a, 0x0, 0x6, 0xf},
	{0xb200, 0x0, 0x8, 0xa1},
	{0xb201, 0x0, 0x8, 0x7},
	{0xa093, 0x0, 0x1, 0x0},
	{0xa093, 0x1, 0x5, 0xf},
	{0xa094, 0x0, 0x8, 0xff},
	{0xa095, 0x0, 0x8, 0xf},
	{0xa080, 0x2, 0x5, 0x3},
	{0xa081, 0x0, 0x4, 0x0},
	{0xa081, 0x4, 0x4, 0x9},
	{0xa082, 0x0, 0x5, 0x1f},
	{0xa08d, 0x0, 0x8, 0x1},
	{0xa083, 0x0, 0x8, 0x32},
	{0xa084, 0x0, 0x1, 0x0},
	{0xa08e, 0x0, 0x8, 0x3},
	{0xa085, 0x0, 0x8, 0x32},
	{0xa086, 0x0, 0x3, 0x0},
	{0xa087, 0x0, 0x8, 0x6e},
	{0xa088, 0x0, 0x5, 0x15},
	{0xa089, 0x0, 0x8, 0x0},
	{0xa08a, 0x0, 0x5, 0x19},
	{0xa08b, 0x0, 0x8, 0x92},
	{0xa08c, 0x0, 0x5, 0x1c},
	{0xa120, 0x0, 0x8, 0x0},
	{0xa121, 0x0, 0x5, 0x10},
	{0xa122, 0x0, 0x8, 0x0},
	{0xa123, 0x0, 0x7, 0x40},
	{0xa123, 0x7, 0x1, 0x0},
	{0xa124, 0x0, 0x8, 0x13},
	{0xa125, 0x0, 0x7, 0x10},
	{0xa1c0, 0x0, 0x8, 0x0},
	{0xa1c1, 0x0, 0x5, 0x4},
	{0xa1c2, 0x0, 0x8, 0x0},
	{0xa1c3, 0x0, 0x5, 0x10},
	{0xa1c3, 0x5, 0x3, 0x0},
	{0xa1c4, 0x0, 0x6, 0x0},
	{0xa1c5, 0x0, 0x7, 0x10},
	{0xa100, 0x0, 0x8, 0x0},
	{0xa101, 0x0, 0x5, 0x10},
	{0xa102, 0x0, 0x8, 0x0},
	{0xa103, 0x0, 0x7, 0x40},
	{0xa103, 0x7, 0x1, 0x0},
	{0xa104, 0x0, 0x8, 0x18},
	{0xa105, 0x0, 0x7, 0xa},
	{0xa106, 0x0, 0x8, 0x20},
	{0xa107, 0x0, 0x8, 0x40},
	{0xa108, 0x0, 0x4, 0x0},
	{0xa38c, 0x0, 0x8, 0xfc},
	{0xa38d, 0x0, 0x8, 0x0},
	{0xa38e, 0x0, 0x8, 0x7e},
	{0xa38f, 0x0, 0x8, 0x0},
	{0xa390, 0x0, 0x8, 0x2f},
	{0xa60f, 0x5, 0x1, 0x1},
	{0xa170, 0x0, 0x8, 0xdc},
	{0xa171, 0x0, 0x2, 0x0},
	{0xa2ae, 0x0, 0x1, 0x1},
	{0xa2ae, 0x1, 0x1, 0x1},
	{0xa392, 0x0, 0x1, 0x1},
	{0xa391, 0x2, 0x1, 0x0},
	{0xabc1, 0x0, 0x8, 0xff},
	{0xabc2, 0x0, 0x8, 0x0},
	{0xabc8, 0x0, 0x8, 0x8},
	{0xabca, 0x0, 0x8, 0x10},
	{0xabcb, 0x0, 0x1, 0x0},
	{0xabc3, 0x5, 0x3, 0x7},
	{0xabc0, 0x6, 0x1, 0x0},
	{0xabc0, 0x4, 0x2, 0x0},
	{0xa344, 0x4, 0x4, 0x1},
	{0xabc0, 0x7, 0x1, 0x1},
	{0xabc0, 0x2, 0x1, 0x1},
	{0xa345, 0x0, 0x8, 0x66},
	{0xa346, 0x0, 0x8, 0x66},
	{0xa347, 0x0, 0x4, 0x0},
	{0xa343, 0x0, 0x4, 0xa},
	{0xa347, 0x4, 0x4, 0x2},
	{0xa348, 0x0, 0x4, 0xc},
	{0xa348, 0x4, 0x4, 0x7},
	{0xa349, 0x0, 0x6, 0x2},
};
