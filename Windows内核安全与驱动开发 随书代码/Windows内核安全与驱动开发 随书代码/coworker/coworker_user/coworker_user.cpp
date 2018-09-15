// coworker_user.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"

#define CWK_DEV_SYM L"\\\\.\\slbkcdo_3948d33e"

// ��Ӧ�ò����������һ���ַ�����
#define  CWK_DVC_SEND_STR \
	(ULONG)CTL_CODE( \
	FILE_DEVICE_UNKNOWN, \
	0x911,METHOD_BUFFERED, \
	FILE_WRITE_DATA)

// ��������ȡһ���ַ���
#define  CWK_DVC_RECV_STR \
	(ULONG)CTL_CODE( \
	FILE_DEVICE_UNKNOWN, \
	0x912,METHOD_BUFFERED, \
	FILE_READ_DATA)

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE device = NULL;
	ULONG ret_len;
    int ret = 0;
    char *msg = {"Hello driver, this is a message from app.\r\n"};

	// ���豸.ÿ��Ҫ����������ʱ�����Դ�Ϊ���Ӵ��豸
	device=CreateFile(CWK_DEV_SYM,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_SYSTEM,0);
	if (device == INVALID_HANDLE_VALUE)
	{
		printf("coworker demo: Open device failed.\r\n");
		return -1;
	}
	else
		printf("coworker demo: Open device successfully.\r\n");

    if(!DeviceIoControl(device, CWK_DVC_SEND_STR, msg, strlen(msg) + 1, NULL, 0, &ret_len, 0))
    {
        printf("coworker demo: Send message failed.\r\n");
        ret = -2;
    }
    else
        printf("coworker demo: Send message successfully.\r\n");

    CloseHandle(device);
	return ret;
}

