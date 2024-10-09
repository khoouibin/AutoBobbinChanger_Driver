#ifndef _USBCOMM_ABC_H_
#define _USBCOMM_ABC_H_
#include <libusb-1.0/libusb.h>
#include "Poco/Event.h"
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ONS_General.h>
#include <vector>

#define USB_VID_Bootloader_RTC 0xf000
#define USB_PID_Bootloader_RTC 0x5109
#define USB_VID_ONS_RTC 0x04d8
#define USB_PID_ONS_RTC 0x0204
#define USB_VID_ABC_RTC 0x1d6b
#define USB_PID_ABC_RTC 0x5109

#define BL_USB_CMD_ID_OFFSET 0x40
#define TRANSACTION_SIZE 8 // row,Transaction
#define MESSAGE_MAX 64	   // column
#define ADDR_MASK 0x00ffffff
#define ADDR_GenSeg_Test1 0x0557C0
#define ADDR_Config_FICD 0xF8000E
#define BIT_RSTPRI 0x04
#define BL_USB_DATASIZE 48
#define MSG_DATA_SIZE 60
#define MSG_ENTITY_MAX_PACK_SIZE 30
#define MSG_ENTITY_TABLE_REPLY_SIZE 22

typedef unsigned char *ptr_usb_msg_u8;
typedef unsigned char usb_msg_u8;

// enum libusb_error {
//     LIBUSB_SUCCESS             = 0,
//     LIBUSB_ERROR_IO            = -1,
//     LIBUSB_ERROR_INVALID_PARAM = -2,
//     LIBUSB_ERROR_ACCESS        = -3,
//     LIBUSB_ERROR_NO_DEVICE     = -4,
//     LIBUSB_ERROR_NOT_FOUND     = -5,
//     LIBUSB_ERROR_BUSY          = -6,
//     LIBUSB_ERROR_TIMEOUT       = -7,
//     LIBUSB_ERROR_OVERFLOW      = -8,
//     LIBUSB_ERROR_PIPE          = -9,
//     LIBUSB_ERROR_INTERRUPTED   = -10,
//     LIBUSB_ERROR_NO_MEM        = -11,
//     LIBUSB_ERROR_NOT_SUPPORTED = -12,
//     LIBUSB_ERROR_OTHER         = -99,
// };

enum UsbDevice
{
	Device_NULL = -1,
	Device_ABC_RTC = 1,
};

enum Protocol_Command
{
	Cmd_Echo = 0x00,
	Cmd_Reset = 0x01,
	Cmd_Profile = 0x02,
	Cmd_Log = 0x03,
	Cmd_EntityTable = 0x04,
	Cmd_EntityPack = 0x05,
	Cmd_Z_PulseGen = 0x06,
	Cmd_X_PulseGen = 0x07,
	Cmd_ControlModeSwitch = 0x10,
	Cmd_HomeParts = 0x11,
	Cmd_MAX,
};

enum Protocol_PositiveResponse
{
	RespPositive_Echo = 0x40,
	RespPositive_Reset = 0x41,
	RespPositive_Profile = 0x42,
	RespPositive_Log = 0x43,
	RespPositive_EntityTable = 0x44,
	RespPositive_EntityPack = 0x45,
	RespPositive_Z_PulseGen = 0x46,
	RespPositive_X_PulseGen = 0x47,
	RespPositive_ControlModeSwitch = 0x50,
	RespPositive_HomeParts = 0x51,
};

enum Protocol_NegativeResponse
{
	RespNeg = 0x7f,
};

enum Protocol_Dummy
{
	Dummy_00 = 0x00,
	Dummy_ff = 0xff,
};

enum Echo_SubFunc
{
	SubFunc_55 = 0x55,
	SubFunc_AA = 0xAA,
};

enum Reset_SubFunc
{
	SubFunc_reset_mcu = 1,
	SubFunc_reset_usb = 2,
	SubFunc_reset_uart = 3,
	SubFunc_reset_max,
};

enum Profile_SubFunc
{
	SubFunc_profile_get = 1,
	SubFunc_profile_set = 2,
	SubFunc_profile_max,
};

enum Log_SubFunc
{
	SubFunc_log_level_get = 1,
	SubFunc_log_level_set = 2,
	SubFunc_log_msg_reply = 3,
	SubFunc_log_max,
};

enum LogLev
{
    Critial_Lev = 1,
    Error_Lev = 2,
    Warning_Lev = 4,
    Info_Lev = 8,
    Debug_Lev = 16,
    LogLev_MAX
};

enum EntityTable_SubFunc
{
	SubFunc_table_get_off = 0,
	SubFunc_table_get_instant = 1,
	SubFunc_table_get_period = 2,
	SubFunc_table_get_changed = 3,
	SubFunc_entitytable_max,
};

enum EntityPack_SubFunc
{
	SubFunc_pack_get = 1,
	SubFunc_pack_set = 2,
	SubFunc_entitypack_max,
};

enum Z_PulseGen_SubFunc
{
	SubFunc_z_pulse_gen_off = 0,
	SubFunc_z_pulse_gen_rpm = 1,
	SubFunc_z_pulse_gen_pwm = 2,
	SubFunc_z_pulse_gen_max,
};

enum X_PulseGen_SubFunc
{
	SubFunc_x_pulsemode_run_stop = 0,
	SubFunc_x_pulsemode_trapezoid = 1,
	SubFunc_x_pulsemode_max,
};

enum ControlModeSwitch_SubFunc
{
	SubFunc_controlmode_uninit = 0,
	SubFunc_controlmode_ready = 1,
	SubFunc_controlmode_home = 2,
	SubFunc_controlmode_diagnosis = 3,
	SubFunc_controlmode_max,
};

enum HomeParts_SubFunc
{
	SubFunc_home_WinderStepper = 0,
	SubFunc_home_LECPA_30 = 1,
	SubFunc_home_LECPA_100 = 2,
	SubFunc_home_WinderStepper_polling_reply = 10,
	SubFunc_home_LECPA_30_polling_reply = 11,
	SubFunc_home_LECPA_100_polling_reply = 12,
	SubFunc_home_max,
};

enum HomeParts_SubCmd
{
	SubCmd_Abort = 0,
	SubCmd_Start = 1,
	SubCmd_max,
};

enum Reponse_Code
{
	POSITIVE_CODE = 0x00,
	NRC_SIZE_ZERO = 0x81,
	NRC_SIZE_nZERO = 0x82,
	NRC_SIZE_EXCEED = 0x83,
	NRC_SUBFUNC_OUTRANGE = 0x84,
	NRC_DATA_OUTRANGE = 0x85,
	NRC_CMD_NOT_FOUND = 0x86,
};

typedef struct
{
	libusb_device_handle *Dev_Handle;		// USB device handle
	libusb_context *Ctx;					// USB libusb session
	pthread_mutex_t Rx_Buffer_Mutex;		// Mutex to prevent read/Write problem.
	pthread_mutex_t Tx_Buffer_Mutex;		// Mutex to prevent read/Write problem.
	pthread_t USBComm_Rx_Thread;			// USB->host receive thread
	pthread_t USBComm_Tx_Thread;			// host->USB transmit thread
	pthread_t USBComm_Task_Thread;			// check every 1ms.
	pthread_t USBComm_AutoReConnect_Thread; // check every 0.5sec to connect.
	unsigned char device_EPn_IN;
	unsigned char device_EPn_OUT;
	int LIBUSB_Init_Success;
	int LIBUSB_RxTx_Pthread_Success;
	int USB_Device_Connect_Success;
	UsbDevice USB_TargetDevice;
} BL_USBComm_Driver_Elment_t;

enum BL_USBThreads_Control
{
	Threads_reset = 0,
	Threads_set = 1,  // wake.
	Threads_wait = 2, // sleep.
};

typedef struct
{
	Poco::Event g_USBComm_Rx_idle;
	Poco::Event g_USBComm_Tx_idle;
	Poco::Event g_USBComm_Task_idle;
} BL_USBComm_Driver_Idle_t;

typedef struct
{
	bool Init;
	bool flag_ISR;
	bool Mutex; // check TxMutex evey 1ms.
	bool Stuck;
	char Ptr_buff;
	char Ptr_comp;
	unsigned char Buff[TRANSACTION_SIZE][MESSAGE_MAX]; //[row][column]
	unsigned char MsgSize[TRANSACTION_SIZE];
} USB_Transaction_State_t;

enum Aux_Bootloader_Cmd
{
	AuxBL_Null = 0xff,
	AuxBL_NOP = 0x00,
	AuxBL_Read_SegmentChecksum = 0x01,
	AuxBL_Clear_GenSeg_Flash = 0x02,
	AuxBL_Read_PIC_FlashMemory = 0x05,
	AuxBL_Write_PIC_FlashMemory = 0x06,
	AuxBL_Read_ONS_VersionMsg = 0x07,
	AuxBL_Read_PIC_Config = 0x0a,
	AuxBL_Write_PIC_Config = 0x0b,
	AuxBL_Reset_PIC = 0x10,
	AuxBL_Reset_PIC_WriteConfig = 0x11,
	AuxBL_Negative_Response = 0x7f,
};


// protocol:
//                               byte0  byte1 byte2 byte3     byte4 byte5.....
//   Host->Client(cmd):          cmd_id AddrL AddrM AddrH     size  data[n]
//   Client->Host(feedback):     offset AddrL AddrM AddrH     size  data[n]
// 1.AuxBL_NOP                   0x00   0xrr  0xss  0xtt      0
//   feedback                    0x40   0xrr  0xss  0xtt+0x40 0
// 2.AuxBL_Read_SegmentChecksum  0x01   0xff  0xff  0xff      1     0/1    0:general segment, 1:auxiliary segment.
//   feedback                    0x41   0x00  0x00  0x00      2     chksumL  chksumH
// 3.AuxBL_Clear_GenSeg_Flash    0x02   0xff  0xff  0xff      0
//   feedback                    0x42   0x00  0x00  0x00      0
// 4.AuxBL_Read_PIC_FlashMemory  0x05   0xrr  0xss  0xtt      16~32
//   feedback                    0x45   0xrr  0xss  0xtt      16~32 data[0]...data[size-1]
// 5.AuxBL_Write_PIC_FlashMemory 0x06   0xuu  0xvv  0xww      16~32 data[0]...data[size-1]
//   feedback                    0x46   0x00  0x00  0x00      0
// 6.AuxBL_Read_ONS_VersionMsg   0x07   0xii  0xjj  0xkk      1          0/1    0:version number(18bytes), 1:version note(48bytes), addr:memory_offset.
//   feedback                    0x47   0xii  0xjj  0xkk      18~48      data[n]
// 7.AuxBL_Read_PIC_Config       0x0a   0xii  0xjj  0xkk      1
//   feedback                    0x4a   0xii  0xjj  0xkk      1          data[0]
// 8.AuxBL_Write_PIC_Config      0x0b   0xii  0xjj  0xkk      1          data[0]
//   feedback                    0x4b   0x00  0x00  0x00      0
// 9.AuxBL_Reset_PIC             0x10   0xff  0xff  0xff      0
//   feedback                    0x50   0x00  0x00  0x00      0


typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char argv_0;
	unsigned char argv_1;
	unsigned char data[MSG_DATA_SIZE];
} USB_Task_msg_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned char argv_0;
	unsigned char argv_1;
	unsigned char data[MSG_DATA_SIZE];
} USB_TaskResp_msg_t;

typedef struct
{
	unsigned char resp_id;
	unsigned char cmd_id;
	unsigned char neg_code;
	unsigned char neg_argv;
	unsigned char data[MSG_DATA_SIZE];
} USB_NegResponse_msg_t;


typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char data[2];
} usb_msg_echo_t;

typedef struct
{
	USB_TaskResp_msg_t echo_fbk;
	Poco::Event echo_fbk_wake;
} usb_msg_echo_fbk_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned short  delay_time;
} usb_msg_reset_t;

typedef struct
{
	usb_msg_reset_t reset_fbk;
	Poco::Event reset_fbk_wake;
} usb_msg_reset_fbk_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char profile_number;
	unsigned char ignore;
	unsigned char data[MSG_DATA_SIZE];
} usb_msg_profile_t;

typedef struct
{
	usb_msg_profile_t profile_fbk;
	Poco::Event profile_fbk_wake;
} usb_msg_profile_fbk_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char set_level;
	unsigned char ignore;
	unsigned char data[MSG_DATA_SIZE];
} usb_msg_log_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned short log_counter;
	unsigned char data[MSG_DATA_SIZE];
} usb_msg_log_reply_t;

typedef struct
{
	usb_msg_log_reply_t log_setting_fbk;
	Poco::Event log_setting_fbk_wake;
} usb_msg_log_setting_fbk_t;

// template:
// SubFunc_table_get_instant: 0x04, 0x01, 0x00, 22;
// reply:                     0x44, 0x01, 0x00, 22, ...(data = 22 bytes)
// SubFunc_table_get_period:  0x04, 0x02, msec, 22;
// reply:                     0x44, 0x02, msec, 22, ...(data = 22 bytes)
// SubFunc_table_get_changed: 0x04, 0x03, 0x00, 22;
// reply:                     0x44, 0x03, 0x00, 22, ...(data = 22 bytes)
// SubFunc_table_get_off:     0x04, 0x04, 0x00, 0;
// reply:                     0x44, 0x04, 0x00, 0,

// SubFunc_pack_get:          0x05, 0x01, 0x00, entity_pack_size;
// reply:                     0x45, 0x01, 0x00, entity_pack_size, ...(data = n entity_pack)
// SubFunc_pack_set:          0x05, 0x02, 0x00, entity_pack_size, ...(data = n entity_pack);
// reply:                     0x45, 0x02, 0x00, 0;

typedef struct
{
	unsigned char entity_name;
	unsigned char entity_value;
} ioentity_pack_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char reply_period;
	unsigned char table_size;
	unsigned char data[MSG_ENTITY_TABLE_REPLY_SIZE];
} usb_msg_entitytable_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned char reply_period;
	unsigned char table_size;
	unsigned char data[MSG_ENTITY_TABLE_REPLY_SIZE];
} usb_msg_entitytable_reply_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char pack_size;
	unsigned char argv;
	ioentity_pack_t entity_pack[MSG_ENTITY_MAX_PACK_SIZE];
} usb_msg_entity_pack_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned char pack_size;
	unsigned char argv;
	ioentity_pack_t entity_pack[MSG_ENTITY_MAX_PACK_SIZE];
} usb_msg_entity_pack_reply_t;

typedef struct
{
	usb_msg_entitytable_reply_t entitytable_fbk;
	Poco::Event entitytable_fbk_wake;
} usb_msg_entitytable_fbk_t;

typedef struct
{
	usb_msg_entity_pack_reply_t entitypack_fbk;
	Poco::Event entitypack_fbk_wake;
} usb_msg_entitypack_fbk_t;

typedef struct
{
	UINT_16 period_hiword;
	UINT_16 period_loword;
	UINT_16 dutyon_hiword;
	UINT_16 dutyon_loword;
} z_pulse_width_modulation_t;

enum Zrpm
{
	z_100rpm = 0,
	z_200rpm = 1,
	z_300rpm = 2,
	z_400rpm = 3,
	z_500rpm = 4,
	z_600rpm = 5,
	z_700rpm = 6,
	z_800rpm = 7,
	z_900rpm = 8,
	z_1000rpm = 9,
	z_1100rpm = 10,
	z_1200rpm = 11,
	z_1300rpm = 12,
	z_1400rpm = 13,
	z_1500rpm = 14,
	z_1600rpm = 15,
	z_1700rpm = 16,
	z_1800rpm = 17,
	z_1900rpm = 18,
	z_2000rpm = 19,
	z_2100rpm = 20,
	z_2200rpm = 21,
	z_2300rpm = 22,
	z_2400rpm = 23,
	z_2500rpm = 24,
	z_2600rpm = 25,
	z_2700rpm = 26,
	z_2800rpm = 27,
	z_2900rpm = 28,
	z_3000rpm = 29,
	z_3100rpm = 30,
	z_3200rpm = 31,
	z_3300rpm = 32,
	z_3400rpm = 33,
	z_3500rpm = 34,
	z_3600rpm = 35,
	z_rpm_max,
};

typedef struct
{
	enum Zrpm z_rpm_enum;
} z_pulse_rpm_enum_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char argv_0;
	unsigned char argv_1;
	z_pulse_width_modulation_t z_pwm_value;
	z_pulse_rpm_enum_t z_rpm_value;
} usb_msg_z_pulse_gen_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned char argv_0;
	unsigned char argv_1;
} usb_msg_z_pulse_gen_reply_t;

typedef struct
{
	usb_msg_z_pulse_gen_reply_t z_pulse_gen_fbk;
	Poco::Event z_pulse_gen_fbk_wake;
} usb_msg_z_pulse_gen_fbk_t;

typedef union
{
	UINT_32 u32;
	UINT_16 u16[2];
    UCHAR_8 u8[4];
}
OCx_pulse_count_type_t;

typedef struct
{
    OCx_pulse_count_type_t period;
    OCx_pulse_count_type_t dutyon;
} OCx_sequence_t;

typedef struct
{
    OCx_sequence_t cx[2];
    OCx_sequence_t cx_last;
} OCx_src_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned short steps;
	OCx_src_t x_sequence;
} usb_msg_x_pulse_gen_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned char argv_0;
	unsigned char argv_1;
} usb_msg_x_pulse_gen_reply_t;

typedef struct
{
	usb_msg_x_pulse_gen_reply_t x_pulse_gen_fbk;
	Poco::Event x_pulse_gen_fbk_wake;
} usb_msg_x_pulse_gen_fbk_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char argv_0;
	unsigned char argv_1;
} usb_msg_control_mode_switch_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned char control_status;
	unsigned char switch_status;
} usb_msg_control_mode_switch_reply_t;

typedef struct
{
	usb_msg_control_mode_switch_reply_t control_mode_switch_fbk;
	Poco::Event control_mode_switch_fbk_wake;
} usb_msg_control_mode_switch_fbk_t;

typedef struct
{
	unsigned char cmd_id;
	unsigned char sub_func;
	unsigned char sub_cmd;
	unsigned char argv_1;
} usb_msg_home_parts_t;

typedef struct
{
	unsigned char cmd_id_rep;
	unsigned char sub_func;
	unsigned char home_routine;
	unsigned char home_state;
} usb_msg_home_parts_reply_t;

typedef struct
{
	usb_msg_home_parts_reply_t home_parts_fbk;
	Poco::Event home_parts_fbk_wake;
} usb_msg_home_parts_fbk_t;

// should be create a profile.h to collect below.
typedef struct
{
	UCHAR_8 p01_01;
	CHAR_8 p01_02;
	UINT_16 p01_03;
	INT_16 p01_04;
	UINT_32 p01_05;
	INT_32 p01_06;
} RTC_Profile_01_t;

typedef struct
{
    UCHAR_8 p02_01;
    CHAR_8 p02_02;
    INT_16 p02_03;
    INT_16 p02_04;
    INT_32 p02_05;
    INT_32 p02_06;
    INT_32 p02_07;
    CHAR_8 p02_08;
    INT_32 p02_09;
    CHAR_8 p02_10;
    INT_32 p02_11;
} RTC_Profile_02_t;

int USBComm_Driver_GetDeviceList(int *suitable_device, int en_print);
int USBComm_Driver_SelectTargetDevice(unsigned short idVendor, unsigned short idProduct);
int USBComm_Driver_getTargetDevice(void);
int USBComm_Driver_openTargetDevice(UsbDevice dev);
int USBComm_Driver_closeDeviceHandle(void);

void USBComm_Driver_Threads_reset();
void USBComm_Driver_Threads_set();
int USBComm_Driver_Init_Threads(void);
void *USBComm_Rx_Thread_Main_Abc(void *p);
void *USBComm_Tx_Thread_Main_Abc(void *p);
void *USBComm_Task_Service_Abc(void *p);
void *USBComm_AutoReConnect_Abc(void *p);

int USBComm_Rx_Terminate(void);
int USBComm_Tx_Terminate(void);
int USBComm_Task_Terminate(void);
int USBComm_AutoReConnect_Terminate(void);

int USBComm_Get_RTC_Device_Status(void);
int USBComm_Driver_Init_LIBUSB(void);

int BL_USBComm_Driver_TransData_bulk();
int BL_USBComm_Driver_TransData_interrupt();

void BL_USBComm_TransStateInit(void);
char USB_TxBulkBuffer_To_Bus(void);
char USB_RxBulkBuffer_Get_From_Bus(unsigned char data_in[], int data_length);
char USB_Msg_From_RxBuffer(usb_msg_u8 *msg_cmd, unsigned char *msg_size);
char USB_Msg_To_TxBulkBuffer(ptr_usb_msg_u8 send_msg, unsigned char msg_size);

unsigned long GetCurrentTime_us(void);
int usb_message_echo(unsigned char sub_func);
int usb_message_reset(unsigned char sub_func, unsigned int delay_time = 500);
int usb_message_profile_get(unsigned char profile_number, usb_msg_profile_t* profile_msg);
int usb_message_profile_set(unsigned char profile_number, usb_msg_profile_t* profile_msg);

int usb_get_profile_01(RTC_Profile_01_t* ret_profile_01);
int usb_set_profile_01(RTC_Profile_01_t* set_profile_01);

int usb_get_profile_02(RTC_Profile_02_t* ret_profile_02);
int usb_set_profile_02(RTC_Profile_02_t* set_profile_02);

int usb_message_log_level_get();
int usb_message_log_level_set(enum LogLev u8_value);

int usb_message_set_entity_table_reply_mode(int mode);
int usb_message_get_entity_pack(std::vector<ioentity_pack_t> *entities);
int usb_message_set_entity_pack(std::vector<ioentity_pack_t> *entities);

int usb_message_set_z_pulse_gen(int z_rpm);
int usb_message_set_x_pulse_gen(int pulse_mode,int spr, int steps, int max_rpm);

int usb_message_control_mode_switch(int rtc_control_mode);
int usb_message_home_LECPA_30(int home_action);

#endif