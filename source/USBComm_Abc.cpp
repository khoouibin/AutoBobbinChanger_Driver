//#include "return_code.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <libusb-1.0/libusb.h>
#include <iomanip>
#include <arpa/inet.h>
#include "Timers.h"
#include "logger_wrapper.h"
#include "USBComm_Abc.h"
#include "Msg_Prot.h"
#include "interface_driver_to_ui.h"

USB_Transaction_State_t TxTransState;
USB_Transaction_State_t RxTransState;

usb_msg_echo_fbk_t g_msg_echo_fbk;
usb_msg_reset_fbk_t g_msg_reset_fbk;
usb_msg_profile_fbk_t g_msg_profile_fbk;
usb_msg_log_setting_fbk_t g_msg_log_setting_fbk;
usb_msg_entitytable_fbk_t g_msg_entitytable_fbk;
usb_msg_entitypack_fbk_t g_msg_entitypack_fbk;
usb_msg_z_pulse_gen_fbk_t g_msg_z_pulse_gen_fbk;
usb_msg_x_pulse_gen_fbk_t g_msg_x_pulse_gen_fbk;
usb_msg_control_mode_switch_fbk_t g_msg_control_mode_switch_fbk;
usb_msg_home_parts_fbk_t g_msg_home_parts_fbk;

char str_log[256];
char libusb_error_string[64];
static BL_USBComm_Driver_Elment_t USBComm_Driver_Elment = {.LIBUSB_Init_Success = -1, .LIBUSB_RxTx_Pthread_Success = -1, .USB_Device_Connect_Success = -1, .USB_TargetDevice = Device_NULL};
BL_USBComm_Driver_Idle_t USBComm_Driver_Idle;
bool g_b_USBComm_Rx_run = true;
bool g_b_USBComm_Tx_run = true;
bool g_b_USBComm_Task_run = true;
bool g_b_AutoReConnect_run = true;

// char BL_USB_WriteBuffer(void);

int USBComm_Driver_Init_Threads(void)
{
	int res_rx_pth = pthread_create(&USBComm_Driver_Elment.USBComm_Rx_Thread, NULL, USBComm_Rx_Thread_Main_Abc, NULL);
	int res_tx_pth = pthread_create(&USBComm_Driver_Elment.USBComm_Tx_Thread, NULL, USBComm_Tx_Thread_Main_Abc, NULL);
	int res_rxser_pth = pthread_create(&USBComm_Driver_Elment.USBComm_Task_Thread, NULL, USBComm_Task_Service_Abc, NULL);
	int res_autoconn_pth = pthread_create(&USBComm_Driver_Elment.USBComm_AutoReConnect_Thread, NULL, USBComm_AutoReConnect_Abc, NULL);

	if (res_rx_pth != 0)
	{
		sprintf(str_log, "%s[%d] create USBComm RxThread, errno:%d,%s", __func__, __LINE__, res_rx_pth, strerror(res_rx_pth));
		string tmp_string(str_log);
		goDriverLogger.Log("error", tmp_string);
	}
	if (res_tx_pth != 0)
	{
		sprintf(str_log, "%s[%d] create USBComm TxThread, errno:%d,%s", __func__, __LINE__, res_tx_pth, strerror(res_tx_pth));
		string tmp_string(str_log);
		goDriverLogger.Log("error", tmp_string);
	}
	if (res_rxser_pth != 0)
	{
		sprintf(str_log, "%s[%d] create USBComm TxThread, errno:%d,%s", __func__, __LINE__, res_rxser_pth, strerror(res_rxser_pth));
		string tmp_string(str_log);
		goDriverLogger.Log("error", tmp_string);
	}
	if (res_autoconn_pth != 0)
	{
		sprintf(str_log, "%s[%d] create USBComm TxThread, errno:%d,%s", __func__, __LINE__, res_autoconn_pth, strerror(res_autoconn_pth));
		string tmp_string(str_log);
		goDriverLogger.Log("error", tmp_string);
	}
	USBComm_Driver_Threads_reset();
	if (res_rx_pth == 0 && res_tx_pth == 0 && res_rxser_pth == 0)
	{
		USBComm_Driver_Elment.LIBUSB_RxTx_Pthread_Success = 0;
		return 0;
	}
	else
		return -1;
}

int USBComm_Rx_Terminate(void)
{
	g_b_USBComm_Rx_run = false;
	USBComm_Driver_Idle.g_USBComm_Rx_idle.set();

	if (pthread_join(USBComm_Driver_Elment.USBComm_Rx_Thread, NULL))
		return 0;
	else
		return -1;
}

int USBComm_Tx_Terminate(void)
{
	g_b_USBComm_Tx_run = false;
	USBComm_Driver_Idle.g_USBComm_Tx_idle.set();

	if (pthread_join(USBComm_Driver_Elment.USBComm_Tx_Thread, NULL))
		return 0;
	else
		return -1;
}

int USBComm_Task_Terminate(void)
{
	g_b_USBComm_Task_run = false;
	USBComm_Driver_Idle.g_USBComm_Task_idle.set();

	if (pthread_join(USBComm_Driver_Elment.USBComm_Task_Thread, NULL))
		return 0;
	else
		return -1;
}

int USBComm_AutoReConnect_Terminate(void)
{
	g_b_AutoReConnect_run = false;
	if (pthread_join(USBComm_Driver_Elment.USBComm_AutoReConnect_Thread, NULL))
		return 0;
	else
		return -1;
}

void USBComm_Driver_Threads_reset()
{
	USBComm_Driver_Idle.g_USBComm_Rx_idle.reset();
	USBComm_Driver_Idle.g_USBComm_Tx_idle.reset();
	USBComm_Driver_Idle.g_USBComm_Task_idle.reset();
}
void USBComm_Driver_Threads_set()
{
	USBComm_Driver_Idle.g_USBComm_Rx_idle.set();
	USBComm_Driver_Idle.g_USBComm_Tx_idle.set();
	USBComm_Driver_Idle.g_USBComm_Task_idle.set();
}

int USBComm_Driver_Init_LIBUSB(void)
{
	int init_res = libusb_init(&USBComm_Driver_Elment.Ctx);
	if (init_res != 0)
	{
		strcpy(libusb_error_string, libusb_strerror((libusb_error)init_res));
		sprintf(str_log, "%s[%d],errno:%d,%s", __func__, __LINE__, init_res, libusb_error_string);
		string tmp_string(str_log);
		goDriverLogger.Log("ERROR", tmp_string);
		USBComm_Driver_Elment.LIBUSB_Init_Success = -1;
	}
	else
		USBComm_Driver_Elment.LIBUSB_Init_Success = 0;

	return init_res;
}

int USBComm_Driver_GetDeviceList(int *suitable_device, int en_print)
{
	libusb_device **usbDevices;
	libusb_device_descriptor device_desc;
	libusb_config_descriptor *config_desc;
	libusb_interface_descriptor *interface_desc;
	libusb_device_handle *usb_hdl;

	int res = -1;
	unsigned char str_iProduct[200], str_iManufacturer[200];
	int numDevices;
	int targetDevice = -1;
	int targetDevice_Select = -1;
	int r;
	bool err_trig = false;

	numDevices = libusb_get_device_list(NULL, &usbDevices);
	if (numDevices < 1)
	{
		libusb_free_device_list(usbDevices, 0);
		return -1;
	}
	for (int i = 0; i < numDevices; i++)
	{
		r = libusb_get_device_descriptor(usbDevices[i], &device_desc);
		if (r < 0)
		{
			if (en_print == 1)
				printf("usb_get_device_descriptor_res:%d\n", r);
			continue;
		}

		r = libusb_open(usbDevices[i], &usb_hdl);
		if (r < 0)
		{
			err_trig = true;
			if (en_print == 1)
				printf("libusb_open error code:%d\n", r);
			continue;
		}
		memset(str_iProduct, 0, sizeof(str_iProduct));
		memset(str_iManufacturer, 0, sizeof(str_iManufacturer));
		libusb_get_string_descriptor_ascii(usb_hdl, device_desc.iProduct, str_iProduct, 200); // this api real get usb_product info from usblib_firmware.
		libusb_get_string_descriptor_ascii(usb_hdl, device_desc.iManufacturer, str_iManufacturer, 200);
		if (en_print == 1)
		{
			cout << setw(4) << setfill(' ') << std::dec << i + 1 << setw(4) << "|"
				 << std::hex << std::setw(4) << setfill('0') << device_desc.idVendor << " |"
				 << std::hex << std::setw(4) << setfill('0') << device_desc.idProduct << " |"
				 << std::setw(32) << setfill(' ') << str_iManufacturer << "|" << std::setw(32) << setfill(' ') << str_iProduct << endl;
		}

		if (targetDevice == -1)
			targetDevice = USBComm_Driver_SelectTargetDevice(device_desc.idVendor, device_desc.idProduct);
		libusb_close(usb_hdl);
	}

	if (en_print == 1 && err_trig == true)
	{
		printf("\n\nlibusb_error list:\n");
		char libusb_errorlist[]="LIBUSB_SUCCESS = 0 , LIBUSB_ERROR_IO = -1 , LIBUSB_ERROR_INVALID_PARAM = -2 , LIBUSB_ERROR_ACCESS = -3 , LIBUSB_ERROR_NO_DEVICE = -4 , LIBUSB_ERROR_NOT_FOUND = -5 , LIBUSB_ERROR_BUSY = -6 , LIBUSB_ERROR_TIMEOUT = -7 ,LIBUSB_ERROR_OVERFLOW = -8 , LIBUSB_ERROR_PIPE = -9 , LIBUSB_ERROR_INTERRUPTED = -10 , LIBUSB_ERROR_NO_MEM = -11 ,LIBUSB_ERROR_NOT_SUPPORTED = -12 , LIBUSB_ERROR_OTHER = -99";
		printf("%s\n",libusb_errorlist);

	}
	libusb_free_device_list(usbDevices, 1);
	*suitable_device = targetDevice;
	return numDevices;
}

int USBComm_Driver_SelectTargetDevice(unsigned short idVendor, unsigned short idProduct)
{
	int res = -1;

	if (idVendor == USB_VID_ABC_RTC && idProduct == USB_PID_ABC_RTC)
	{
		USBComm_Driver_Elment.USB_TargetDevice = Device_ABC_RTC;
	}
	res = (int)USBComm_Driver_Elment.USB_TargetDevice;
	return res;
}

int USBComm_Driver_getTargetDevice(void)
{
	return (int)USBComm_Driver_Elment.USB_TargetDevice;
}

int USBComm_Driver_openTargetDevice(UsbDevice dev)
{
	int res = -1;
	if (dev == Device_ABC_RTC)
	{
		USBComm_Driver_Elment.Dev_Handle = libusb_open_device_with_vid_pid(NULL, USB_VID_ABC_RTC, USB_PID_ABC_RTC);
	}
	else
	{
		USBComm_Driver_Elment.USB_Device_Connect_Success = -1;
		return -1;
	}

	if (USBComm_Driver_Elment.Dev_Handle == NULL)
	{
		libusb_close(USBComm_Driver_Elment.Dev_Handle);
		sprintf(str_log, "%s[%d] libusb OPEN_DEVICE Fault", __func__, __LINE__);
		string tmp_string(str_log);
		goDriverLogger.Log("error", tmp_string);

		USBComm_Driver_Elment.USB_TargetDevice = Device_NULL;
	}
	else
	{
		res = libusb_kernel_driver_active(USBComm_Driver_Elment.Dev_Handle, 0);
		if (res == 1)
			res = libusb_detach_kernel_driver(USBComm_Driver_Elment.Dev_Handle, 0);
		// usually, after usb device in 'CONFIGURED_STATE'(ready to connect with libusb),
		// first time connect with device the "libusb_kernel_driver_active = 1",
		// after that, the driver restart, "libusb_kernel_driver_active = 0"
		// the check processing eventually needed.(khoo)

		res = libusb_claim_interface(USBComm_Driver_Elment.Dev_Handle, 0);

		if (res == 0)
		{
			USBComm_Driver_Elment.USB_Device_Connect_Success = 0;
			USBComm_Driver_Elment.device_EPn_IN = 0x81;
			USBComm_Driver_Elment.device_EPn_OUT = 0x01;
			if (dev == Device_ABC_RTC)
				USBComm_Driver_Elment.USB_TargetDevice = Device_ABC_RTC;
			BL_USBComm_TransStateInit();
		}
		else
			USBComm_Driver_Elment.USB_Device_Connect_Success = -1;
	}
	return res;
}

int USBComm_Get_RTC_Device_Status(void)
{
	if (USBComm_Driver_Elment.USB_Device_Connect_Success == 0)
	{
		return (int)USBComm_Driver_Elment.USB_TargetDevice;
	}
	else
		return -1;
}

int USBComm_Driver_closeDeviceHandle(void)
{
	if (USBComm_Driver_Elment.USB_Device_Connect_Success == 0)
	{
		libusb_close(USBComm_Driver_Elment.Dev_Handle);
		USBComm_Driver_Elment.USB_Device_Connect_Success = -1;
		return 0;
	}
	return -1;
}

void *USBComm_AutoReConnect_Abc(void *p)
{
	int res;
	int trywait_TIMEOUT = 500;
	int dev_num, target_device;
	int res_open_dev;
	(void)p;
	Poco::Event wait_ms;
	pthread_detach(pthread_self());
	while (g_b_AutoReConnect_run == true)
	{
		wait_ms.tryWait(trywait_TIMEOUT);

		if (g_b_AutoReConnect_run == false)
			break;

		if (USBComm_Driver_Elment.USB_Device_Connect_Success == -1)
		{
			dev_num = USBComm_Driver_GetDeviceList(&target_device, 0);
			if (target_device > 0)
			{
				res_open_dev = USBComm_Driver_openTargetDevice((UsbDevice)target_device);
				sprintf(str_log, "%s[%d] target_dev:%d res:%d", __func__, __LINE__, target_device, res_open_dev);
				string tmp_string(str_log);
				goDriverLogger.Log("info", tmp_string);
			}
		}
		else
			continue;
	}
	printf("USBComm_AutoReConnect_Terminated\n");
	pthread_exit(NULL);
}

void *USBComm_Rx_Thread_Main_Abc(void *p)
{
	int res;
	char dump_data[255];
	unsigned char rx_msg[64];
	int rx_res = 0;
	int rx_actual_size;
	int res_closeHdl;
	(void)p; // avoid compiler unused-warning

	pthread_detach(pthread_self());
	while (g_b_USBComm_Rx_run == true)
	{
		USBComm_Driver_Idle.g_USBComm_Rx_idle.wait();
		if (g_b_USBComm_Rx_run == false)
			break;

		while (1)
		{
			rx_res = libusb_bulk_transfer(USBComm_Driver_Elment.Dev_Handle, USBComm_Driver_Elment.device_EPn_IN, rx_msg, 64, &rx_actual_size, 0);
			if (rx_res == 0)
			{
				pthread_mutex_lock(&USBComm_Driver_Elment.Rx_Buffer_Mutex);
				USB_RxBulkBuffer_Get_From_Bus(rx_msg, rx_actual_size);
				pthread_mutex_unlock(&USBComm_Driver_Elment.Rx_Buffer_Mutex);
			}
			else
			{
				strcpy(libusb_error_string, libusb_strerror((libusb_error)rx_res));
				sprintf(str_log, "%s[%d],errno:%d,%s", __func__, __LINE__, rx_res, libusb_error_string);
				string tmp_string(str_log);
				goDriverLogger.Log("ERROR", tmp_string);
				break;
			}
			usleep(500);
		}

		res_closeHdl = USBComm_Driver_closeDeviceHandle();
		USBComm_Driver_Elment.USB_TargetDevice = Device_NULL;

		sprintf(str_log, "%s[%d] BL_USB DevHandle close:%d, TargetDev:%d", __func__, __LINE__, res_closeHdl, USBComm_Driver_Elment.USB_TargetDevice);
		string tmp_string(str_log);
		goDriverLogger.Log("info", tmp_string);
	}
	printf("BL_USBComm_Rx_Thread_Terminated\n");
	pthread_exit(NULL);
}

void *USBComm_Tx_Thread_Main_Abc(void *p)
{
	int res;
	(void)p;
	Poco::Event wait_ms;
	pthread_detach(pthread_self());
	while (g_b_USBComm_Tx_run == true)
	{
		USBComm_Driver_Idle.g_USBComm_Tx_idle.wait();
		if (g_b_USBComm_Tx_run == false)
			break;

		while (USBComm_Driver_getTargetDevice() != Device_NULL)
		{
			pthread_mutex_lock(&USBComm_Driver_Elment.Tx_Buffer_Mutex);
			res = USB_TxBulkBuffer_To_Bus();
			pthread_mutex_unlock(&USBComm_Driver_Elment.Tx_Buffer_Mutex);
			wait_ms.tryWait(1);

			if (res != 1 && res != 0)
			{
				strcpy(libusb_error_string, libusb_strerror((libusb_error)res));
				sprintf(str_log, "%s[%d],errno:%d,%s", __func__, __LINE__, res, libusb_error_string);
				string tmp_string(str_log);
				goDriverLogger.Log("ERROR", tmp_string);
			}
		}
	}
	printf("BL_USBComm_Tx_Thread_Terminated\n");
	pthread_exit(NULL);
}

bool USB_Msg_Parser(USB_TaskResp_msg_t* task_msg);

void *USBComm_Task_Service_Abc(void *p)
{
	int res;
	char dump_data[255];
	(void)p; // avoid compiler unused-warning
	Poco::Event wait_ms;

	char msg_res;
	char USB_Parser_res;
	unsigned char msg_length;
    bool new_msg;	
    USB_TaskResp_msg_t Task_msg;
    USB_TaskResp_msg_t* pTask_msg=&Task_msg;
    usb_msg_u8 msg[MESSAGE_MAX];

	usb_msg_echo_fbk_t* p_echofbk= &g_msg_echo_fbk;
	usb_msg_reset_fbk_t* p_resetfbk= &g_msg_reset_fbk;
	usb_msg_profile_fbk_t* p_profilefbk = &g_msg_profile_fbk;
	usb_msg_log_setting_fbk_t* p_logsettingfbk = &g_msg_log_setting_fbk;
	usb_msg_log_reply_t* pLogsetting_msg = (usb_msg_log_reply_t*)&Task_msg;
	usb_msg_entitytable_fbk_t *p_entitytable_fbk = &g_msg_entitytable_fbk;
	usb_msg_entitypack_fbk_t *p_entitypack_fbk = &g_msg_entitypack_fbk;
	usb_msg_entity_pack_t* p_entitypack_msg=(usb_msg_entity_pack_t*)&Task_msg;
	usb_msg_z_pulse_gen_fbk_t *p_z_pulse_gen_fbk = &g_msg_z_pulse_gen_fbk;
	usb_msg_x_pulse_gen_fbk_t *p_x_pulse_gen_fbk = &g_msg_x_pulse_gen_fbk;
	usb_msg_control_mode_switch_fbk_t *p_mode_switch_fbk = &g_msg_control_mode_switch_fbk;
	usb_msg_home_parts_fbk_t *p_home_parts_fbk = &g_msg_home_parts_fbk;
	usb_msg_home_parts_reply_t* pHome_parts_reply=(usb_msg_home_parts_reply_t*)&Task_msg;

	pthread_detach(pthread_self());
	while (g_b_USBComm_Task_run == true)
	{
		USBComm_Driver_Idle.g_USBComm_Task_idle.wait();

		if (g_b_USBComm_Task_run == false)
			break;

		while (USBComm_Driver_getTargetDevice() != Device_NULL)
		{
			pthread_mutex_lock(&USBComm_Driver_Elment.Rx_Buffer_Mutex);

			switch (USBComm_Driver_Elment.USB_TargetDevice)
			{
			case Device_NULL:
				printf("USB Rx_Service-Device Null\n");
				res = -1;
				break;

			case Device_ABC_RTC:
				new_msg = USB_Msg_Parser(&Task_msg);
				if (new_msg ==true)
				{
					if (Task_msg.cmd_id_rep==RespPositive_Echo)
					{
						p_echofbk->echo_fbk.cmd_id_rep =Task_msg.cmd_id_rep;
						p_echofbk->echo_fbk.sub_func =Task_msg.sub_func;
						memcpy(p_echofbk->echo_fbk.data, pTask_msg->data, 60);
						p_echofbk->echo_fbk_wake.set();
					}
					else if (Task_msg.cmd_id_rep==RespPositive_Reset)
					{
						p_resetfbk->reset_fbk.cmd_id =Task_msg.cmd_id_rep;
						p_resetfbk->reset_fbk.sub_func =Task_msg.sub_func;
						p_resetfbk->reset_fbk_wake.set();
					}
					else if (Task_msg.cmd_id_rep==RespPositive_Profile)
					{
						p_profilefbk->profile_fbk.cmd_id =Task_msg.cmd_id_rep;
						p_profilefbk->profile_fbk.sub_func =Task_msg.sub_func;
						p_profilefbk->profile_fbk.profile_number =Task_msg.argv_0;
						memcpy(p_profilefbk->profile_fbk.data, pTask_msg->data, MSG_DATA_SIZE);
						p_profilefbk->profile_fbk_wake.set();
					}
					else if (Task_msg.cmd_id_rep==RespPositive_Log)
					{
						if (pLogsetting_msg->sub_func == SubFunc_log_msg_reply)
						{
							sprintf(str_log, "%s[%d]cnt:%d, msg:%s", __func__, __LINE__,
									(int)pLogsetting_msg->log_counter, pLogsetting_msg->data);
							string tmp_string(str_log);
							goDriverLogger.Log("debug", tmp_string);
						}
						else
						{
							p_logsettingfbk->log_setting_fbk.cmd_id_rep = pLogsetting_msg->cmd_id_rep;
							p_logsettingfbk->log_setting_fbk.sub_func = pLogsetting_msg->sub_func;
							p_logsettingfbk->log_setting_fbk.data[0] = pLogsetting_msg->data[0];
							p_logsettingfbk->log_setting_fbk_wake.set();
						}
					}
					else if (Task_msg.cmd_id_rep == RespPositive_EntityTable)
					{
						p_entitytable_fbk->entitytable_fbk.cmd_id_rep = Task_msg.cmd_id_rep;
						p_entitytable_fbk->entitytable_fbk.sub_func = Task_msg.sub_func;
						p_entitytable_fbk->entitytable_fbk.reply_period = Task_msg.argv_0;
						p_entitytable_fbk->entitytable_fbk.table_size = Task_msg.argv_1;
						memcpy(p_entitytable_fbk->entitytable_fbk.data, pTask_msg->data, sizeof(p_entitytable_fbk->entitytable_fbk.data));
						p_entitytable_fbk->entitytable_fbk_wake.set();

						if (p_entitytable_fbk->entitytable_fbk.table_size > 0)
						{
							string str_entitytab_value = " ";
							std::stringstream ss;
							for (int i = 0; i < p_entitytable_fbk->entitytable_fbk.table_size; i++)
							{
								ss.str("");
								ss << std::hex << std::setw(2) << setfill('0') << (int)p_entitytable_fbk->entitytable_fbk.data[i];
								str_entitytab_value += ss.str() + " ";
							}
							sprintf(str_log, "%s[%d] RespId:0x%02x,", __func__, __LINE__, Task_msg.cmd_id_rep);
							string tmp_string(str_log);
							tmp_string += str_entitytab_value;
							goDriverLogger.Log("debug", tmp_string);
						}
					}
					else if (Task_msg.cmd_id_rep == RespPositive_EntityPack)
					{
						if (Task_msg.sub_func == SubFunc_pack_get)
						{
							p_entitypack_fbk->entitypack_fbk.cmd_id_rep = Task_msg.cmd_id_rep;
							p_entitypack_fbk->entitypack_fbk.sub_func = Task_msg.sub_func;
							p_entitypack_fbk->entitypack_fbk.pack_size = Task_msg.argv_0;
							memcpy(p_entitypack_fbk->entitypack_fbk.entity_pack, p_entitypack_msg->entity_pack, sizeof(p_entitypack_msg->entity_pack));
							p_entitypack_fbk->entitypack_fbk_wake.set();

							if (p_entitypack_fbk->entitypack_fbk.pack_size > 0)
							{
								string str_entitypack_value = " ";
								for (int i = 0; i < p_entitypack_fbk->entitypack_fbk.pack_size; i++)
								{
									str_entitypack_value += "entity[" +
															std::to_string(p_entitypack_fbk->entitypack_fbk.entity_pack[i].entity_name) +
															"]=" + std::to_string(p_entitypack_fbk->entitypack_fbk.entity_pack[i].entity_value) + ",";
								}
								sprintf(str_log, "%s[%d] RespId:0x%02x", __func__, __LINE__, Task_msg.cmd_id_rep);
								string tmp_string(str_log);
								tmp_string += str_entitypack_value;
								goDriverLogger.Log("debug", tmp_string);
							}
						}
						else if (Task_msg.sub_func == SubFunc_pack_set)
						{
							p_entitypack_fbk->entitypack_fbk.cmd_id_rep = Task_msg.cmd_id_rep;
							p_entitypack_fbk->entitypack_fbk.sub_func = Task_msg.sub_func;
							p_entitypack_fbk->entitypack_fbk.pack_size = Task_msg.argv_0;
							p_entitypack_fbk->entitypack_fbk_wake.set();
						}
					}
					else if (Task_msg.cmd_id_rep == RespPositive_Z_PulseGen)
					{
						p_z_pulse_gen_fbk->z_pulse_gen_fbk.cmd_id_rep = Task_msg.cmd_id_rep;
						p_z_pulse_gen_fbk->z_pulse_gen_fbk.sub_func = Task_msg.sub_func;
						p_z_pulse_gen_fbk->z_pulse_gen_fbk_wake.set();
					}
					else if (Task_msg.cmd_id_rep == RespPositive_X_PulseGen)
					{
						p_x_pulse_gen_fbk->x_pulse_gen_fbk.cmd_id_rep = Task_msg.cmd_id_rep;
						p_x_pulse_gen_fbk->x_pulse_gen_fbk.sub_func = Task_msg.sub_func;
						p_x_pulse_gen_fbk->x_pulse_gen_fbk_wake.set();
					}
					else if (Task_msg.cmd_id_rep == RespPositive_ControlModeSwitch)
					{
						p_mode_switch_fbk->control_mode_switch_fbk.cmd_id_rep = Task_msg.cmd_id_rep;
						p_mode_switch_fbk->control_mode_switch_fbk.sub_func = Task_msg.sub_func;
						p_mode_switch_fbk->control_mode_switch_fbk.control_status = Task_msg.argv_0;
						p_mode_switch_fbk->control_mode_switch_fbk.switch_status = Task_msg.argv_1;
						p_mode_switch_fbk->control_mode_switch_fbk_wake.set();
					}
					else if (Task_msg.cmd_id_rep == RespPositive_HomeParts)
					{
						if (pHome_parts_reply->sub_func ==SubFunc_home_LECPA_30_polling_reply)
						{
							sprintf(str_log, "%s[%d] subf:0x%02x, routine:0x%02x, state:%d", __func__, __LINE__,
									pHome_parts_reply->sub_func,
									pHome_parts_reply->home_routine,
									pHome_parts_reply->home_state);
							string tmp_string(str_log);
							goDriverLogger.Log("debug", tmp_string);
						}
						else if (pHome_parts_reply->sub_func == SubFunc_home_LECPA_30)
						{
							p_home_parts_fbk->home_parts_fbk.cmd_id_rep = pHome_parts_reply->cmd_id_rep;
							p_home_parts_fbk->home_parts_fbk.sub_func = pHome_parts_reply->sub_func;
							p_home_parts_fbk->home_parts_fbk.home_routine = pHome_parts_reply->home_routine;
							p_home_parts_fbk->home_parts_fbk.home_state = pHome_parts_reply->home_state;
							p_home_parts_fbk->home_parts_fbk_wake.set();
						}
					}
				}
				res = 0;
				break;

			default:
				sprintf(str_log, "%s[%d]Device Define UNKNOWN", __func__, __LINE__);
				string tmp_string(str_log);
				goDriverLogger.Log("error", tmp_string);
				break;
			}

			pthread_mutex_unlock(&USBComm_Driver_Elment.Rx_Buffer_Mutex);
			wait_ms.tryWait(1);
		}
	}
	printf("BL_USBComm_Task_Service_Terminated\n");
	return NULL;
}

int BL_USBComm_Driver_TransData_bulk()
{
	int act_leng;
	static char k = 0;
	unsigned char test_msg[32] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	test_msg[0] = k;
	k++;
	libusb_bulk_transfer(USBComm_Driver_Elment.Dev_Handle, USBComm_Driver_Elment.device_EPn_OUT, test_msg, 32, &act_leng, 0);
	return 0;
}

int BL_USBComm_Driver_TransData_interrupt()
{
	int act_leng;
	unsigned char test_msg[32] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	libusb_interrupt_transfer(USBComm_Driver_Elment.Dev_Handle, USBComm_Driver_Elment.device_EPn_OUT, test_msg, 32, &act_leng, 0);
	return 0;
}

void BL_USBComm_TransStateInit(void)
{
	TxTransState.Init = 1;
	TxTransState.Mutex = 0;
	TxTransState.Stuck = 0;
	TxTransState.Ptr_buff = -1;
	TxTransState.Ptr_comp = -1;

	RxTransState.Init = 1;
	RxTransState.Mutex = 0;
	RxTransState.Stuck = 0;
	RxTransState.Ptr_buff = -1;
	RxTransState.Ptr_comp = -1;
	USBComm_Driver_Threads_set();
}

char USB_TxBulkBuffer_To_Bus(void)
{
    char tx_ptr = TxTransState.Ptr_comp;
	unsigned char tx_size;
	unsigned char tx_msg[MESSAGE_MAX];
	int transferred;
	int tx_res;

    if (TxTransState.Init != 1)
        return -1;

    if ((TxTransState.Ptr_buff ^ TxTransState.Ptr_comp) == 0)
        return 1;

	tx_size = TxTransState.MsgSize[(unsigned char)tx_ptr];
	memcpy(tx_msg, TxTransState.Buff[(unsigned char)tx_ptr], tx_size);
	tx_res = libusb_bulk_transfer(USBComm_Driver_Elment.Dev_Handle, USBComm_Driver_Elment.device_EPn_OUT, tx_msg, (int)tx_size, &transferred, 0);

	TxTransState.Mutex = 0;
	TxTransState.Ptr_comp++;
	TxTransState.Ptr_comp &= 0x07;
	return tx_res;
}

char USB_RxBulkBuffer_Get_From_Bus(unsigned char rx_data[], int rx_length)
{
	int i_RxLength = 0;
	char *ptr_RxBuff;
	ptr_RxBuff = &RxTransState.Ptr_buff;

	if (USBComm_Driver_Elment.USB_Device_Connect_Success == -1)
		return -1;

	if (RxTransState.Ptr_buff == -1)
		RxTransState.Ptr_buff = 0;

	memcpy(RxTransState.Buff[(unsigned char)*ptr_RxBuff], rx_data, MESSAGE_MAX);
	RxTransState.MsgSize[(unsigned char)*ptr_RxBuff] = (unsigned char)rx_length;

	RxTransState.Ptr_buff++;
	RxTransState.Ptr_buff &= 0x07;

	return 0;
}

unsigned long GetCurrentTime_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned long)((1000000 * (tv.tv_sec % 1000)) + tv.tv_usec);
}

char USB_Msg_To_TxBulkBuffer(ptr_usb_msg_u8 send_msg, unsigned char msg_size) // FirstIn
{
    char *ptr_TxBuff;
    ptr_TxBuff = &TxTransState.Ptr_buff;

    if (TxTransState.Ptr_buff == -1)
    {
        TxTransState.Ptr_buff = 0;
        TxTransState.Ptr_comp = 0;
    }
	pthread_mutex_lock(&USBComm_Driver_Elment.Tx_Buffer_Mutex);
    memcpy(TxTransState.Buff[(unsigned char)TxTransState.Ptr_buff], send_msg, msg_size);
    TxTransState.MsgSize[(unsigned char)TxTransState.Ptr_buff] = msg_size;
    TxTransState.Ptr_buff++;
    TxTransState.Ptr_buff &= 0x07;
	pthread_mutex_unlock(&USBComm_Driver_Elment.Tx_Buffer_Mutex);

    if ((TxTransState.Ptr_buff ^ TxTransState.Ptr_comp) == 0)
        TxTransState.Stuck = 1;
    return 0;
}

char USB_Msg_From_RxBuffer(usb_msg_u8 *msg_cmd, unsigned char *msg_size)
{
    char res = -1;
    char *ptr_RxBuff;
    ptr_RxBuff = &RxTransState.Ptr_comp;

    if (RxTransState.Init != 1)
    {
        res = -1;
        return res;
    }
    if ((RxTransState.Ptr_buff ^ RxTransState.Ptr_comp) == 0)
    {
        res = 1;
        return res;
    }
    if (RxTransState.Ptr_buff != -1 && RxTransState.Ptr_comp == -1)
    {
        RxTransState.Ptr_comp = 0;
        res = -2;
        return res;
    }
	memcpy(msg_cmd, (ptr_usb_msg_u8)RxTransState.Buff[(unsigned char)*ptr_RxBuff], MESSAGE_MAX);
    *msg_size = RxTransState.MsgSize[(unsigned char)*ptr_RxBuff];
    RxTransState.Ptr_comp++;
    RxTransState.Ptr_comp &= 0x07;
    res = 0;
    return res;
}

int usb_message_echo(unsigned char sub_func)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_echo_t msg_echo;
	msg_echo.cmd_id = Cmd_Echo;
	msg_echo.sub_func = sub_func;
	msg_echo.data[0] = Dummy_ff;
	msg_echo.data[1] = Dummy_ff;
	memset(msg_echo.data, 0, sizeof(msg_echo.data));
	usb_msg_echo_fbk_t* p_echofbk=&g_msg_echo_fbk;;
	unsigned long t_start = GetCurrentTime_us();
	p_echofbk->echo_fbk_wake.reset();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&msg_echo, sizeof(usb_msg_echo_t));
	while (1)
	{
		res_wake = p_echofbk->echo_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] ResponseId=0x%02x, SubFunction=0x%02x, response msg=%s",
					__func__, __LINE__,
					p_echofbk->echo_fbk.cmd_id_rep,
					p_echofbk->echo_fbk.sub_func,
					p_echofbk->echo_fbk.data);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("echo message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start)/1000);
	return res;
}

int usb_message_reset(unsigned char sub_func, unsigned int delay_time)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_reset_t msg_reset;
	msg_reset.cmd_id = Cmd_Reset;
	msg_reset.sub_func = sub_func;
	msg_reset.delay_time = delay_time;
	usb_msg_reset_fbk_t *p_resetfbk = &g_msg_reset_fbk;
	unsigned long t_start = GetCurrentTime_us();
	p_resetfbk->reset_fbk_wake.reset();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&msg_reset, sizeof(usb_msg_reset_t));
	while (1)
	{
		res_wake = p_resetfbk->reset_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] ResponseId:0x%02x, SubFunction:0x%02x",
					__func__, __LINE__,
					p_resetfbk->reset_fbk.cmd_id,
					p_resetfbk->reset_fbk.sub_func);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("reset message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_get_profile_01(RTC_Profile_01_t* ret_profile_01)
{
	int res = 0;
	usb_msg_profile_t usb_profile_msg;
	res = usb_message_profile_get(01, &usb_profile_msg);
	if (res == 0)
	{
		memcpy(&ret_profile_01->p01_01, (ptr_usb_msg_u8)&usb_profile_msg.data, sizeof(ret_profile_01->p01_01));
		memcpy(&ret_profile_01->p01_02, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_01->p01_01), sizeof(ret_profile_01->p01_02));
		memcpy(&ret_profile_01->p01_03, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_01->p01_01) + sizeof(ret_profile_01->p01_02), sizeof(ret_profile_01->p01_03));
		memcpy(&ret_profile_01->p01_04, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_01->p01_01) + sizeof(ret_profile_01->p01_02) + sizeof(ret_profile_01->p01_03), sizeof(ret_profile_01->p01_04));
		memcpy(&ret_profile_01->p01_05, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_01->p01_01) + sizeof(ret_profile_01->p01_02) + sizeof(ret_profile_01->p01_03) + sizeof(ret_profile_01->p01_04), sizeof(ret_profile_01->p01_05));
		memcpy(&ret_profile_01->p01_06, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_01->p01_01) + sizeof(ret_profile_01->p01_02) + sizeof(ret_profile_01->p01_03) + sizeof(ret_profile_01->p01_04) + sizeof(ret_profile_01->p01_05), sizeof(ret_profile_01->p01_06));
	}
	return res;
}

int usb_get_profile_02(RTC_Profile_02_t* ret_profile_02)
{
	int res = 0;
	usb_msg_profile_t usb_profile_msg;
	res = usb_message_profile_get(02, &usb_profile_msg);
	if (res == 0)
	{
		memcpy(&ret_profile_02->p02_01, (ptr_usb_msg_u8)&usb_profile_msg.data, sizeof(ret_profile_02->p02_01));
		memcpy(&ret_profile_02->p02_02, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01), sizeof(ret_profile_02->p02_02));
		memcpy(&ret_profile_02->p02_03, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02), sizeof(ret_profile_02->p02_03));
		memcpy(&ret_profile_02->p02_04, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03), sizeof(ret_profile_02->p02_04));
		memcpy(&ret_profile_02->p02_05, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03) + sizeof(ret_profile_02->p02_04), sizeof(ret_profile_02->p02_05));
		memcpy(&ret_profile_02->p02_06, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03) + sizeof(ret_profile_02->p02_04) + sizeof(ret_profile_02->p02_05), sizeof(ret_profile_02->p02_06));
		memcpy(&ret_profile_02->p02_07, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03) + sizeof(ret_profile_02->p02_04) + sizeof(ret_profile_02->p02_05)+ sizeof(ret_profile_02->p02_06), sizeof(ret_profile_02->p02_07));
		memcpy(&ret_profile_02->p02_08, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03) + sizeof(ret_profile_02->p02_04) + sizeof(ret_profile_02->p02_05)+ sizeof(ret_profile_02->p02_06)+ sizeof(ret_profile_02->p02_07), sizeof(ret_profile_02->p02_08));
		memcpy(&ret_profile_02->p02_09, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03) + sizeof(ret_profile_02->p02_04) + sizeof(ret_profile_02->p02_05)+ sizeof(ret_profile_02->p02_06)+ sizeof(ret_profile_02->p02_07)+ sizeof(ret_profile_02->p02_08), sizeof(ret_profile_02->p02_09));
		memcpy(&ret_profile_02->p02_10, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03) + sizeof(ret_profile_02->p02_04) + sizeof(ret_profile_02->p02_05)+ sizeof(ret_profile_02->p02_06)+ sizeof(ret_profile_02->p02_07)+ sizeof(ret_profile_02->p02_08)+ sizeof(ret_profile_02->p02_09), sizeof(ret_profile_02->p02_10));
		memcpy(&ret_profile_02->p02_11, (ptr_usb_msg_u8)&usb_profile_msg.data + sizeof(ret_profile_02->p02_01) + sizeof(ret_profile_02->p02_02) + sizeof(ret_profile_02->p02_03) + sizeof(ret_profile_02->p02_04) + sizeof(ret_profile_02->p02_05)+ sizeof(ret_profile_02->p02_06)+ sizeof(ret_profile_02->p02_07)+ sizeof(ret_profile_02->p02_08)+ sizeof(ret_profile_02->p02_09)+ sizeof(ret_profile_02->p02_10), sizeof(ret_profile_02->p02_11));
	}
	return res;
}

int usb_message_profile_get(unsigned char profile_number, usb_msg_profile_t* profile_msg)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_profile_t msg_profile;
	msg_profile.cmd_id = Cmd_Profile;
	msg_profile.sub_func = SubFunc_profile_get;
	msg_profile.profile_number = profile_number;
	msg_profile.ignore = Dummy_ff;
	usb_msg_profile_fbk_t *p_profilefbk = &g_msg_profile_fbk;
	unsigned long t_start = GetCurrentTime_us();
	p_profilefbk->profile_fbk_wake.reset();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&msg_profile, 4);
	while (1)
	{
		res_wake = p_profilefbk->profile_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x,prof_num:%d",
					__func__, __LINE__,
					p_profilefbk->profile_fbk.cmd_id,
					p_profilefbk->profile_fbk.sub_func,
					p_profilefbk->profile_fbk.profile_number);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			memcpy(profile_msg, (ptr_usb_msg_u8)&p_profilefbk->profile_fbk,sizeof(usb_msg_profile_t));
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("get profile message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_set_profile_01(RTC_Profile_01_t *set_profile_01)
{
	int res = 0;
	usb_msg_profile_t usb_profile_msg;
	memcpy(usb_profile_msg.data + 0, (ptr_usb_msg_u8)&set_profile_01->p01_01, sizeof(set_profile_01->p01_01));
	memcpy(usb_profile_msg.data + sizeof(set_profile_01->p01_01), (ptr_usb_msg_u8)&set_profile_01->p01_02, sizeof(set_profile_01->p01_02));
	memcpy(usb_profile_msg.data + sizeof(set_profile_01->p01_01) + sizeof(set_profile_01->p01_02), (ptr_usb_msg_u8)&set_profile_01->p01_03, sizeof(set_profile_01->p01_03));
	memcpy(usb_profile_msg.data + sizeof(set_profile_01->p01_01) + sizeof(set_profile_01->p01_02) + sizeof(set_profile_01->p01_03), (ptr_usb_msg_u8)&set_profile_01->p01_04, sizeof(set_profile_01->p01_04));
	memcpy(usb_profile_msg.data + sizeof(set_profile_01->p01_01) + sizeof(set_profile_01->p01_02) + sizeof(set_profile_01->p01_03) + sizeof(set_profile_01->p01_04), (ptr_usb_msg_u8)&set_profile_01->p01_05, sizeof(set_profile_01->p01_05));
	memcpy(usb_profile_msg.data + sizeof(set_profile_01->p01_01) + sizeof(set_profile_01->p01_02) + sizeof(set_profile_01->p01_03) + sizeof(set_profile_01->p01_04) + sizeof(set_profile_01->p01_05), (ptr_usb_msg_u8)&set_profile_01->p01_06, sizeof(set_profile_01->p01_06));
	res = usb_message_profile_set(01, &usb_profile_msg);
	return res;
}

int usb_set_profile_02(RTC_Profile_02_t *set_profile_02)
{
	int res = 0;
	usb_msg_profile_t usb_profile_msg;
	memcpy(usb_profile_msg.data, (ptr_usb_msg_u8)&set_profile_02->p02_01, sizeof(set_profile_02->p02_01));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01), (ptr_usb_msg_u8)&set_profile_02->p02_02, sizeof(set_profile_02->p02_02));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02), (ptr_usb_msg_u8)&set_profile_02->p02_03, sizeof(set_profile_02->p02_03));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03), (ptr_usb_msg_u8)&set_profile_02->p02_04, sizeof(set_profile_02->p02_04));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03) + sizeof(set_profile_02->p02_04), (ptr_usb_msg_u8)&set_profile_02->p02_05, sizeof(set_profile_02->p02_05));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03) + sizeof(set_profile_02->p02_04) + sizeof(set_profile_02->p02_05), (ptr_usb_msg_u8)&set_profile_02->p02_06, sizeof(set_profile_02->p02_06));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03) + sizeof(set_profile_02->p02_04) + sizeof(set_profile_02->p02_05) + sizeof(set_profile_02->p02_06), (ptr_usb_msg_u8)&set_profile_02->p02_07, sizeof(set_profile_02->p02_07));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03) + sizeof(set_profile_02->p02_04) + sizeof(set_profile_02->p02_05) + sizeof(set_profile_02->p02_06) + sizeof(set_profile_02->p02_07), (ptr_usb_msg_u8)&set_profile_02->p02_08, sizeof(set_profile_02->p02_08));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03) + sizeof(set_profile_02->p02_04) + sizeof(set_profile_02->p02_05) + sizeof(set_profile_02->p02_06) + sizeof(set_profile_02->p02_07) + sizeof(set_profile_02->p02_08), (ptr_usb_msg_u8)&set_profile_02->p02_09, sizeof(set_profile_02->p02_09));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03) + sizeof(set_profile_02->p02_04) + sizeof(set_profile_02->p02_05) + sizeof(set_profile_02->p02_06) + sizeof(set_profile_02->p02_07) + sizeof(set_profile_02->p02_08) + sizeof(set_profile_02->p02_09), (ptr_usb_msg_u8)&set_profile_02->p02_10, sizeof(set_profile_02->p02_10));
	memcpy(usb_profile_msg.data + sizeof(set_profile_02->p02_01) + sizeof(set_profile_02->p02_02) + sizeof(set_profile_02->p02_03) + sizeof(set_profile_02->p02_04) + sizeof(set_profile_02->p02_05) + sizeof(set_profile_02->p02_06) + sizeof(set_profile_02->p02_07) + sizeof(set_profile_02->p02_08) + sizeof(set_profile_02->p02_09) + sizeof(set_profile_02->p02_10), (ptr_usb_msg_u8)&set_profile_02->p02_11, sizeof(set_profile_02->p02_11));
	res = usb_message_profile_set(02, &usb_profile_msg);
	return res;
}

int usb_message_profile_set(unsigned char profile_number, usb_msg_profile_t* profile_msg)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	profile_msg->cmd_id = Cmd_Profile;
	profile_msg->sub_func = SubFunc_profile_set;
	profile_msg->profile_number = profile_number;
	profile_msg->ignore = Dummy_ff;
	usb_msg_profile_fbk_t *p_profilefbk = &g_msg_profile_fbk;
	unsigned long t_start = GetCurrentTime_us();
	p_profilefbk->profile_fbk_wake.reset();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)profile_msg, sizeof(usb_msg_profile_t));

	while (1)
	{
		res_wake = p_profilefbk->profile_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x,prof_num:%d",
					__func__, __LINE__,
					p_profilefbk->profile_fbk.cmd_id,
					p_profilefbk->profile_fbk.sub_func,
					p_profilefbk->profile_fbk.profile_number);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("get profile message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_message_log_level_get()
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_log_t log_msg;
	log_msg.cmd_id = Cmd_Log;
	log_msg.sub_func = SubFunc_log_level_get;
	log_msg.set_level = Dummy_ff;
	log_msg.ignore = Dummy_ff;
	usb_msg_log_setting_fbk_t *p_logsettingfbk = &g_msg_log_setting_fbk;
	unsigned long t_start = GetCurrentTime_us();
	p_logsettingfbk->log_setting_fbk_wake.reset();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&log_msg, 4);

	while (1)
	{
		res_wake = p_logsettingfbk->log_setting_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x,debug_level:0x%02x",
					__func__, __LINE__,
					p_logsettingfbk->log_setting_fbk.cmd_id_rep,
					p_logsettingfbk->log_setting_fbk.sub_func,
					p_logsettingfbk->log_setting_fbk.data[0]);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("get log-setting message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_message_log_level_set(enum LogLev u8_value)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_log_t log_msg;
	log_msg.cmd_id = Cmd_Log;
	log_msg.sub_func = SubFunc_log_level_set;
	log_msg.set_level = u8_value;
	log_msg.ignore = Dummy_ff;
	usb_msg_log_setting_fbk_t *p_logsettingfbk = &g_msg_log_setting_fbk;
	unsigned long t_start = GetCurrentTime_us();
	p_logsettingfbk->log_setting_fbk_wake.reset();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&log_msg, 4);

	while (1)
	{
		res_wake = p_logsettingfbk->log_setting_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x,debug_level:0x%02x",
					__func__, __LINE__,
					p_logsettingfbk->log_setting_fbk.cmd_id_rep,
					p_logsettingfbk->log_setting_fbk.sub_func,
					p_logsettingfbk->log_setting_fbk.data[0]);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("set log-setting message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_message_set_entity_table_reply_mode(int mode)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_entitytable_t entab_msg;
	entab_msg.cmd_id = Cmd_EntityTable;
	entab_msg.sub_func = char(mode);
	entab_msg.reply_period = (mode == SubFunc_table_get_period)?100:Dummy_00;
	entab_msg.table_size = 0;
	usb_msg_entitytable_fbk_t *p_entitytable_fbk = &g_msg_entitytable_fbk;
	p_entitytable_fbk->entitytable_fbk_wake.reset();
	unsigned long t_start = GetCurrentTime_us();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&entab_msg, 4);
	while (1)
	{
		res_wake = p_entitytable_fbk->entitytable_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x,period:%d,size:%d",
					__func__, __LINE__,
					p_entitytable_fbk->entitytable_fbk.cmd_id_rep,
					p_entitytable_fbk->entitytable_fbk.sub_func,
					p_entitytable_fbk->entitytable_fbk.reply_period,
					p_entitytable_fbk->entitytable_fbk.table_size);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("set entity-table reply mode message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_message_get_entity_pack(std::vector<ioentity_pack_t> *entities)
{
	int res = 0;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_entity_pack_t enpack_msg;
	enpack_msg.cmd_id = Cmd_EntityPack;
	enpack_msg.sub_func = SubFunc_pack_get;
	enpack_msg.pack_size = entities->size();

	for (int i = 0; i < entities->size(); i++)
	{
		enpack_msg.entity_pack[i].entity_name = (entities->begin() + i)->entity_name;
		enpack_msg.entity_pack[i].entity_value = 0xff;
	}

	usb_msg_entitypack_fbk_t *p_entitypack_fbk = &g_msg_entitypack_fbk;
	p_entitypack_fbk->entitypack_fbk_wake.reset();
	unsigned long t_start = GetCurrentTime_us();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&enpack_msg, (enpack_msg.pack_size * 2) + 4);
	while (1)
	{
		res_wake = p_entitypack_fbk->entitypack_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x,size:%d",
					__func__, __LINE__,
					p_entitypack_fbk->entitypack_fbk.cmd_id_rep,
					p_entitypack_fbk->entitypack_fbk.sub_func,
					p_entitypack_fbk->entitypack_fbk.pack_size);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("get entity-pack reply mode message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_message_set_entity_pack(std::vector<ioentity_pack_t> *entities)
{
	int res = 0;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_entity_pack_t enpack_msg;
	enpack_msg.cmd_id = Cmd_EntityPack;
	enpack_msg.sub_func = SubFunc_pack_set;
	enpack_msg.pack_size = entities->size();

	for (int i = 0; i < entities->size(); i++)
	{
		enpack_msg.entity_pack[i].entity_name = (entities->begin() + i)->entity_name;
		enpack_msg.entity_pack[i].entity_value = (entities->begin() + i)->entity_value;
	}

	usb_msg_entitypack_fbk_t *p_entitypack_fbk = &g_msg_entitypack_fbk;
	p_entitypack_fbk->entitypack_fbk_wake.reset();
	unsigned long t_start = GetCurrentTime_us();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&enpack_msg, (enpack_msg.pack_size * 2) + 4);
	while (1)
	{
		res_wake = p_entitypack_fbk->entitypack_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x",
					__func__, __LINE__,
					p_entitypack_fbk->entitypack_fbk.cmd_id_rep,
					p_entitypack_fbk->entitypack_fbk.sub_func);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("set entity-pack reply mode message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int z_rpm_pulse_value_converter(int z_rpm, int z_pulse_per_round, int *period_hiword, int *period_loword, int *dutyon_hiword, int *dutyon_loword)
{
	int iFcy = 60e6;
	int z_pulse_period, z_pulse_dutyon;

	if (z_rpm <= 0)
		return -1;
	z_pulse_period = iFcy / z_rpm / z_pulse_per_round;
	z_pulse_period *= 60;
	z_pulse_dutyon = z_pulse_period / 2;
	*period_hiword = (z_pulse_period >> 16) & 0xffff;
	*period_loword = (z_pulse_period) & 0xffff;
	*dutyon_hiword = (z_pulse_dutyon >> 16) & 0xffff;
	*dutyon_loword = (z_pulse_dutyon) & 0xffff;
	return 0;
}

int usb_message_set_z_pulse_gen(int z_rpm)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	int w, x, y, z;
	usb_msg_z_pulse_gen_t z_pulse_gen_msg;
	z_pulse_gen_msg.cmd_id = Cmd_Z_PulseGen;
	z_pulse_gen_msg.sub_func = (z_rpm == 0) ? SubFunc_z_pulse_gen_off : SubFunc_z_pulse_gen_pwm;

	if (z_rpm_pulse_value_converter(z_rpm, 500, &w, &x, &y, &z) == -1)
	{
		z_pulse_gen_msg.z_pwm_value.period_hiword = 0;
		z_pulse_gen_msg.z_pwm_value.period_loword = 0;
		z_pulse_gen_msg.z_pwm_value.dutyon_hiword = 0;
		z_pulse_gen_msg.z_pwm_value.dutyon_loword = 0;
	}
	else
	{
		z_pulse_gen_msg.z_pwm_value.period_hiword = (UINT_16)w;
		z_pulse_gen_msg.z_pwm_value.period_loword = (UINT_16)x;
		z_pulse_gen_msg.z_pwm_value.dutyon_hiword = (UINT_16)y;
		z_pulse_gen_msg.z_pwm_value.dutyon_loword = (UINT_16)z;
	}

	usb_msg_z_pulse_gen_fbk_t *p_z_pulse_gen_fbk = &g_msg_z_pulse_gen_fbk;
	p_z_pulse_gen_fbk->z_pulse_gen_fbk_wake.reset();
	unsigned long t_start = GetCurrentTime_us();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&z_pulse_gen_msg, 12);
	while (1)
	{
		res_wake = p_z_pulse_gen_fbk->z_pulse_gen_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x",
					__func__, __LINE__,
					p_z_pulse_gen_fbk->z_pulse_gen_fbk.cmd_id_rep,
					p_z_pulse_gen_fbk->z_pulse_gen_fbk.sub_func);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("set entity-table reply mode message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int x_trapezoid_pulse_gen(int max_rpm, int spr, OCx_src_t *ocx_scr)
{
    int iFcy = 60e6;
    int max_rpm_period_cnt = (int)(((long)iFcy * 60) / (long)spr / (long)max_rpm);
    float accel = (float)max_rpm / 14;
    accel = (accel < 5) ? 5 : accel;

    int cx0, cx1, den, num;
    ocx_scr->cx[0].period.u32 = (int)((float)(((long)iFcy * 60) / (long)spr) / accel);
    ocx_scr->cx[0].dutyon.u32 = (ocx_scr->cx[0].period.u32) >> 1;

    cx0 = ocx_scr->cx[0].period.u32;
    num = cx0 + cx0;
    den = 5;
    cx1 = num / den;
    cx1 = cx0 - cx1;
    cx1 = (int)((float)cx1 / 1.4485289);

    ocx_scr->cx[1].period.u32 = cx1;
    ocx_scr->cx[1].dutyon.u32 = cx1 >> 1;
    ocx_scr->cx_last.period.u32 = max_rpm_period_cnt;
    ocx_scr->cx_last.dutyon.u32 = max_rpm_period_cnt >> 1;
    return 0;
}

int usb_message_set_x_pulse_gen(int pulse_mode, int spr, int steps, int max_rpm)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	OCx_src_t ocx_src;
	usb_msg_x_pulse_gen_t x_pulse_gen_msg;
	x_pulse_gen_msg.cmd_id = Cmd_X_PulseGen;
	x_pulse_gen_msg.sub_func = (pulse_mode == 0) ? SubFunc_x_pulsemode_run_stop : SubFunc_x_pulsemode_trapezoid;
	x_pulse_gen_msg.steps = (unsigned short)steps;

	if (x_pulse_gen_msg.sub_func == SubFunc_x_pulsemode_trapezoid)
	{
		x_trapezoid_pulse_gen(max_rpm, spr, &ocx_src);
		printf("%s[%d],cx[0].period=%d, cx[1].period=%d, cx_last.period=%d\n",
			   __func__, __LINE__, ocx_src.cx[0].period.u32, ocx_src.cx[1].period.u32, ocx_src.cx_last.period.u32);
		memcpy(&x_pulse_gen_msg.x_sequence, &ocx_src, sizeof(OCx_src_t));
	}

	usb_msg_x_pulse_gen_fbk_t *p_x_pulse_gen_fbk = &g_msg_x_pulse_gen_fbk;
	p_x_pulse_gen_fbk->x_pulse_gen_fbk_wake.reset();
	unsigned long t_start = GetCurrentTime_us();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&x_pulse_gen_msg, 28);
	while (1)
	{
		res_wake = p_x_pulse_gen_fbk->x_pulse_gen_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunction:0x%02x",
					__func__, __LINE__,
					p_x_pulse_gen_fbk->x_pulse_gen_fbk.cmd_id_rep,
					p_x_pulse_gen_fbk->x_pulse_gen_fbk.sub_func);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("set x-pulse gen reply mode message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_message_control_mode_switch(int rtc_control_mode)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_control_mode_switch_t mode_switch_msg;
	mode_switch_msg.cmd_id = Cmd_ControlModeSwitch;
	mode_switch_msg.argv_0 = Dummy_00;
	mode_switch_msg.argv_1 = Dummy_00;
	mode_switch_msg.sub_func = (unsigned char)rtc_control_mode;

	usb_msg_control_mode_switch_fbk_t *p_mode_switch_fbk = &g_msg_control_mode_switch_fbk;
	p_mode_switch_fbk->control_mode_switch_fbk_wake.reset();
	unsigned long t_start = GetCurrentTime_us();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&mode_switch_msg, 4);
	while (1)
	{
		res_wake = p_mode_switch_fbk->control_mode_switch_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunc:0x%02x, control_state:%d,switch_state:%d",
					__func__, __LINE__,
					p_mode_switch_fbk->control_mode_switch_fbk.cmd_id_rep,
					p_mode_switch_fbk->control_mode_switch_fbk.sub_func,
					p_mode_switch_fbk->control_mode_switch_fbk.control_status,
					p_mode_switch_fbk->control_mode_switch_fbk.switch_status);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("set rtc_control_mode switch reply mode message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

int usb_message_home_LECPA_30(int home_action)
{
	int res;
	int res_wake;
	int nop_trywait_TIMEOUT = 50;
	usb_msg_home_parts_t home_parts_msg;
	home_parts_msg.cmd_id = Cmd_HomeParts;
	home_parts_msg.sub_func =SubFunc_home_LECPA_30;
	home_parts_msg.sub_cmd = (HomeParts_SubCmd)home_action;
	home_parts_msg.argv_1 = Dummy_00;

	usb_msg_home_parts_fbk_t *p_home_parts_fbk = &g_msg_home_parts_fbk;
	p_home_parts_fbk->home_parts_fbk_wake.reset();
	unsigned long t_start = GetCurrentTime_us();
	USB_Msg_To_TxBulkBuffer((ptr_usb_msg_u8)&home_parts_msg, 4);
	while (1)
	{
		res_wake = p_home_parts_fbk->home_parts_fbk_wake.tryWait(nop_trywait_TIMEOUT);
		if (res_wake == 1)
		{
			sprintf(str_log, "%s[%d] RespId:0x%02x, SubFunc:0x%02x, routine:%d, state:%d",
					__func__, __LINE__,
					p_home_parts_fbk->home_parts_fbk.cmd_id_rep,
					p_home_parts_fbk->home_parts_fbk.sub_func,
					p_home_parts_fbk->home_parts_fbk.home_routine,
					p_home_parts_fbk->home_parts_fbk.home_state);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
			res = 0;
		}
		else
		{
			sprintf(str_log, "%s[%d] time out", __func__, __LINE__);
			string tmp_string(str_log);
			goDriverLogger.Log("error", tmp_string);
			res = -1;
		}
		break;
	}
	printf("set home parts message, escape:%ld ms. \n", (GetCurrentTime_us() - t_start) / 1000);
	return res;
}

bool USB_Msg_Parser(USB_TaskResp_msg_t *task_msg)
{
	usb_msg_u8 msg[MESSAGE_MAX];
	USB_TaskResp_msg_t *p_taskmsg;
	USB_NegResponse_msg_t *p_negresp;
	unsigned char msg_length;
	bool b_new_msg = false;
	char nrc_res;
	char msg_res;
	p_taskmsg = (USB_TaskResp_msg_t *)msg;
	p_negresp = (USB_NegResponse_msg_t *)msg;
	msg_res = USB_Msg_From_RxBuffer(msg, &msg_length);
	if (msg_res == 0)
	{
		if (p_taskmsg->cmd_id_rep == RespNeg)
		{
			sprintf(str_log, "%s[%d] CmdId:0x%02x NegCode:0x%02x, msg:%s",
					__func__, __LINE__,
					p_negresp->cmd_id,
					p_negresp->neg_code,
					p_negresp->data);
			string tmp_string(str_log);
			goDriverLogger.Log("info", tmp_string);
		}
		else
		{
			if (p_taskmsg->cmd_id_rep == RespPositive_Echo)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, MESSAGE_MAX);
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_Reset)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_reset_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_Profile)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_profile_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_Log)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_log_reply_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_EntityTable)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_entitytable_reply_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_EntityPack)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_entity_pack_reply_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_Z_PulseGen)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_z_pulse_gen_reply_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_X_PulseGen)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_x_pulse_gen_reply_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_ControlModeSwitch)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_control_mode_switch_reply_t));
				b_new_msg = true;
			}
			else if (p_taskmsg->cmd_id_rep == RespPositive_HomeParts)
			{
				memcpy(task_msg, (usb_msg_u8 *)msg, sizeof(usb_msg_home_parts_reply_t));
				b_new_msg = true;
			}
		}
	}
	return b_new_msg;
}