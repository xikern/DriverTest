///
/// @file   coworker_sys.c
/// @author tanwen
/// @date   2012-5-28
///

#include <ntifs.h>
#include <wdmsec.h>

PDEVICE_OBJECT g_cdo = NULL;

const GUID  CWK_GUID_CLASS_MYCDO =
{0x17a0d1e0L, 0x3249, 0x12e1, {0x92,0x16, 0x45, 0x1a, 0x21, 0x30, 0x29, 0x06}};

#define CWK_CDO_SYB_NAME    L"\\??\\slbkcdo_3948d33e"

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

// ����һ���������������ַ���
#define CWK_STR_LEN_MAX 512
typedef struct {
    LIST_ENTRY list_entry;
    char buf[CWK_STR_LEN_MAX];
} CWK_STR_NODE;

// ��������һ������������֤��������İ�ȫ��
KSPIN_LOCK g_cwk_lock;
// һ���¼�����ʶ�Ƿ����ַ�������ȡ
KEVENT  g_cwk_event;
// �����и�����ͷ
LIST_ENTRY g_cwk_str_list;

#define MEM_TAG 'cwkr'

// �����ڴ沢��ʼ��һ������ڵ�
CWK_STR_NODE *cwkMallocStrNode()
{
    CWK_STR_NODE *ret = ExAllocatePoolWithTag(
        NonPagedPool, sizeof(CWK_STR_NODE), MEM_TAG);
    if(ret == NULL)
        return NULL;
    return ret;
}

void cwkUnload(PDRIVER_OBJECT driver)
{
	UNICODE_STRING cdo_syb = RTL_CONSTANT_STRING(CWK_CDO_SYB_NAME);
    CWK_STR_NODE *str_node;
    ASSERT(g_cdo != NULL);
   	IoDeleteSymbolicLink(&cdo_syb);
    IoDeleteDevice(g_cdo);

    // ����ı��̬�ȣ��ͷŷ�����������ں��ڴ档
    while(TRUE)
    {
        str_node = (CWK_STR_NODE *)ExInterlockedRemoveHeadList(
            &g_cwk_str_list, &g_cwk_lock);
        // str_node = RemoveHeadList(&g_cwk_str_list);
        if(str_node != NULL)
            ExFreePool(str_node);
        else
            break;
    };
}

NTSTATUS cwkDispatch(		
			      IN PDEVICE_OBJECT dev,
			      IN PIRP irp)
{
    PIO_STACK_LOCATION  irpsp = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG ret_len = 0;
    while(dev == g_cdo) 
    {
        // �����������Ƿ���g_cdo�ģ��Ǿͷǳ�����ˡ�
        // ��Ϊ�������ֻ���ɹ���һ���豸�����Կ���ֱ��
        // ����ʧ�ܡ�
	    if(irpsp->MajorFunction == IRP_MJ_CREATE || irpsp->MajorFunction == IRP_MJ_CLOSE)
	    {
            // ���ɺ͹ر��������һ�ɼ򵥵ط��سɹ��Ϳ���
            // �ˡ��������ۺ�ʱ�򿪺͹رն����Գɹ���
            break;
	    }
    	
        if(irpsp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
	    {
		    // ����DeviceIoControl��
            PVOID buffer = irp->AssociatedIrp.SystemBuffer;  
            ULONG inlen = irpsp->Parameters.DeviceIoControl.InputBufferLength;
            ULONG outlen = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
		    ULONG len;
            CWK_STR_NODE *str_node;
		    switch(irpsp->Parameters.DeviceIoControl.IoControlCode)
		    {
            case CWK_DVC_SEND_STR:

                ASSERT(buffer != NULL);
                ASSERT(outlen == 0);

                // ��ȫ�ı��̬��֮һ:������뻺��ĳ��ȶ��ڳ��ȳ���Ԥ�ڵģ���
                // �Ϸ��ش���
                if(inlen > CWK_STR_LEN_MAX)
                {
                    status = STATUS_INVALID_PARAMETER;
                    break;                    
                }

                // ��ȫ�ı��̬��֮��������ַ����ĳ��ȣ���Ҫʹ��strlen!���ʹ
                // ��strlen,һ�������߹�������û�н��������ַ������ᵼ���ں���
                // �����ʷǷ��ڴ�ռ��������
                DbgPrint("strnlen = %d\r\n", strnlen((char *)buffer, inlen));
                if(strnlen((char *)buffer, inlen) == inlen)
                {
                    // �ַ���ռ���˻����������м�û�н����������̷��ش���
                    status = STATUS_INVALID_PARAMETER;
                    break;                    
                }

                // ���ڿ�����Ϊ���뻺���ǰ�ȫ���Ҳ�������ġ�����ڵ㡣
                str_node = cwkMallocStrNode();
                if(str_node == NULL)
                {
                    // ������䲻���ռ��ˣ�������Դ����Ĵ���
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }

                // ǰ���Ѿ�����˻������е��ַ�����ȷ���Ⱥ��ʶ��Һ��н�����
                // ������������ʲô�����������ַ����԰�ȫ�Զ��Բ����ǳ���Ҫ��
                strncpy(str_node->buf,(char *)buffer, CWK_STR_LEN_MAX); 
                // ���뵽����ĩβ����������֤��ȫ�ԡ�
                ExInterlockedInsertTailList(&g_cwk_str_list, (PLIST_ENTRY)str_node, &g_cwk_lock);
                // InsertTailList(&g_cwk_str_list, (PLIST_ENTRY)str_node);
                // ��ӡ
                // DbgPrint((char *)buffer);
                // ��ô���ھͿ�����Ϊ��������Ѿ��ɹ�����Ϊ�ո��Ѿ�������һ
                // ������ô���������¼��������������Ѿ���Ԫ���ˡ�
                KeSetEvent(&g_cwk_event, 0, TRUE);
                break;
            case CWK_DVC_RECV_STR:
                ASSERT(buffer != NULL);
                ASSERT(inlen == 0);
                // Ӧ��Ҫ������ַ������Դˣ���ȫ��Ҫ�����������Ҫ�㹻����
                if(outlen < CWK_STR_LEN_MAX)
                {
                    status = STATUS_INVALID_PARAMETER;
                    break;                    
                }
                while(1) 
                {
                    // ���뵽����ĩβ����������֤��ȫ�ԡ�
                    str_node = (CWK_STR_NODE *)ExInterlockedRemoveHeadList(&g_cwk_str_list, &g_cwk_lock);
                    // str_node = RemoveHeadList(&g_cwk_str_list);
                    if(str_node != NULL)
                    {
                        // ��������£�ȡ�����ַ������ǾͿ�������������С�Ȼ��
                        // ��������ͷ����˳ɹ���
                        strncpy((char *)buffer, str_node->buf, CWK_STR_LEN_MAX);
                        ret_len = strnlen(str_node->buf, CWK_STR_LEN_MAX) + 1;
                        ExFreePool(str_node);
                        break;
                    }
                    else
                    {
                        // ���ںϷ���Ҫ���ڻ�������Ϊ�յ�����£��ȴ��¼�����
                        // ������Ҳ����˵�������������û���ַ�������ͣ�����ȴ�
                        // ������Ӧ�ó���Ҳ�ᱻ����ס��DeviceIoControl�ǲ��᷵��
                        // �ġ�����һ���оͻ᷵�ء�����������������֪ͨ��Ӧ�á�
                        KeWaitForSingleObject(&g_cwk_event,Executive,KernelMode,0,0);
                    }
                }
                break;
            default:
                // ������������ǲ����ܵ�����δ֪������һ�ɷ��طǷ���������
                status = STATUS_INVALID_PARAMETER;
                break;
            }
        }
        break;
    }
    // ���ؽ��
	irp->IoStatus.Information = ret_len;
	irp->IoStatus.Status = status;
	IoCompleteRequest(irp,IO_NO_INCREMENT);
	return status;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING reg_path)
{
	NTSTATUS status;
	ULONG i;
    UCHAR mem[256] = { 0 };

	// ����һ�������豸��Ȼ�����ɷ������ӡ�
	UNICODE_STRING sddl = RTL_CONSTANT_STRING(L"D:P(A;;GA;;;WD)");
	UNICODE_STRING cdo_name = RTL_CONSTANT_STRING(L"\\Device\\cwk_3948d33e");
	UNICODE_STRING cdo_syb = RTL_CONSTANT_STRING(CWK_CDO_SYB_NAME);

    KdBreakPoint();

	// ����һ�������豸����
	status = IoCreateDeviceSecure(
		driver,
		0,&cdo_name,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,&sddl,
		(LPCGUID)&CWK_GUID_CLASS_MYCDO,
		&g_cdo);
	if(!NT_SUCCESS(status))
		return status;

	// ���ɷ�������.
	IoDeleteSymbolicLink(&cdo_syb);
	status = IoCreateSymbolicLink(&cdo_syb,&cdo_name);
	if(!NT_SUCCESS(status))
	{
		IoDeleteDevice(g_cdo);
		return status;
	}

    // ��ʼ���¼�����������ͷ��
    KeInitializeEvent(&g_cwk_event,SynchronizationEvent,TRUE); 
    KeInitializeSpinLock(&g_cwk_lock);
    InitializeListHead(&g_cwk_str_list);

	// ���еķַ����������ó�һ���ġ�
	for(i=0;i<IRP_MJ_MAXIMUM_FUNCTION;i++)
	{
		driver->MajorFunction[i] = cwkDispatch;
	}

	// ֧�ֶ�̬ж�ء�
    driver->DriverUnload = cwkUnload;
	// ��������豸�ĳ�ʼ����ǡ�
	g_cdo->Flags &= ~DO_DEVICE_INITIALIZING;
	return STATUS_SUCCESS;
}
