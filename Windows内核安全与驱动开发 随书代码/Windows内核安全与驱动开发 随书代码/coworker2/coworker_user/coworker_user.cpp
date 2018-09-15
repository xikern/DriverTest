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
    char tst_msg[1024] = { 0 };

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

    // ���￪ʼ����ʵ�Ƕ�������һϵ�в��ԡ�����3���ַ�����
    // 1.����Ϊ0.Ӧ�ÿ����������롣
    // 2.����Ϊ511�ֽڣ�Ӧ�ÿ����������롣
    // 3.����Ϊ512�ֽڣ�Ӧ�÷���ʧ�ܡ�
    // 4.����Ϊ1024�ֽڵ��ַ���������������������Ϊ128��Ӧ�÷���ʧ�ܡ�
    // 5.��һ�ζ�ȡ��Ӧ�ö���msg�����ݡ�
    // 5.��һ�ζ�ȡ��Ӧ�ö�������Ϊ511�ֽڵ��ַ�����
    // 6.�ڶ��ζ�ȡ��Ӧ�ö�������Ϊ0���ַ�����
    do {
        memset(tst_msg, '\0', 1);
        if(!DeviceIoControl(device, CWK_DVC_SEND_STR, tst_msg, 1, NULL, 0, &ret_len, 0))
        {
            ret = -3;
            break;
        }
        else
        {
            printf("TEST1 PASS.\r\n");
        }

        memset(tst_msg, '\0', 512);
        memset(tst_msg, 'a', 511);
        if(!DeviceIoControl(device, CWK_DVC_SEND_STR, tst_msg, 512, NULL, 0, &ret_len, 0))
        {
            ret = -5;
            break;
        }
        else
        {
            printf("TEST2 PASS.\r\n");
        }

        memset(tst_msg, '\0', 513);
        memset(tst_msg, 'a', 512);
        if(DeviceIoControl(device, CWK_DVC_SEND_STR, tst_msg, 513, NULL, 0, &ret_len, 0))
        {
            // ����������Ѿ���������Ӧ����ʧ�ܡ�����ɹ�����
            // ��Ϊ�Ǵ���
            ret = -5;
            break;
        }
        else
        {
            printf("TEST3 PASS.\r\n");
        }

        memset(tst_msg, '\0', 1024);
        memset(tst_msg, 'a', 1023);
        if(DeviceIoControl(device, CWK_DVC_SEND_STR, tst_msg, 128, NULL, 0, &ret_len, 0))
        {
            // �����������Ȼ�������������ַ�����������Ӧ����ʧ
            // �ܡ�����ɹ�������Ϊ�Ǵ���
            ret = -5;
            break;
        }
        else
        {
            printf("TEST4 PASS.\r\n");
        }
        free(tst_msg);

        // ���ڿ�ʼ�����������һ��������Ӧ����msg.
        if(DeviceIoControl(device, CWK_DVC_RECV_STR, NULL, 0, tst_msg, 1024, &ret_len, 0) == 0 || ret_len != strlen(msg) + 1)
        {
            ret = -6;
            break;
        }
        else 
        {
            printf("TEST5 PASS.\r\n");
        }

        // �ڶ���������Ӧ���ǳ���Ϊ0�Ŀ��ַ�����
        if(DeviceIoControl(device, CWK_DVC_RECV_STR, NULL, 0, tst_msg, 1024, &ret_len, 0) == 0 || ret_len != 1)
        {
            ret = -6;
            break;
        }
        else 
        {
            printf("TEST6 PASS.\r\n");
        }

        // ������������Ӧ���ǳ���Ϊ511��ȫa�ַ���
        if(DeviceIoControl(device, CWK_DVC_RECV_STR, NULL, 0, tst_msg, 1024, &ret_len, 0) != 0 || ret_len != 511 + 1)
        {
            ret = -6;
            break;
        }
        else 
        {
            printf("TEST7 PASS.\r\n");
        }
    } while(0);
    CloseHandle(device);
	return ret;
}

