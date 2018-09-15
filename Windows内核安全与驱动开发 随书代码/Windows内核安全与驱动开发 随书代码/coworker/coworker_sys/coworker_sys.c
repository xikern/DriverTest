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

void cwkUnload(PDRIVER_OBJECT driver)
{
	UNICODE_STRING cdo_syb = RTL_CONSTANT_STRING(CWK_CDO_SYB_NAME);
    ASSERT(g_cdo != NULL);
   	IoDeleteSymbolicLink(&cdo_syb);
    IoDeleteDevice(g_cdo);
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
		    switch(irpsp->Parameters.DeviceIoControl.IoControlCode)
		    {
            case CWK_DVC_SEND_STR:
                ASSERT(buffer != NULL);
                ASSERT(inlen > 0);
                ASSERT(outlen == 0);
                DbgPrint((char *)buffer);
                // �Ѿ���ӡ���ˣ���ô���ھͿ�����Ϊ��������Ѿ��ɹ���
                break;
            case CWK_DVC_RECV_STR:
            default:
                // ������������ǲ����ܵ�����δ֪������һ�ɷ��طǷ���������
                status = STATUS_INVALID_PARAMETER;
                break;
            }
        }
        break;
    }
    // ������������ǲ����ܵ�����δ֪������һ�ɷ��طǷ���������
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
