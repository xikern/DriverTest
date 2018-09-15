#include <ntddk.h>
#include <Wdmsec.h>

KSPIN_LOCK lck;

typedef struct UT
{
	KTIMER timer;
	KDPC dpc;
	PKDEFERRED_ROUTINE pRoutine;
	PVOID	pPrivateContext;
	LARGE_INTEGER nDelay;
	ULONG nTimes;
}UserTimer,*PUserTimer;

typedef struct BN
{
	LIST_ENTRY  listEntry;
	PVOID		pBuffer;
	ULONG		nBufLen;
}BufNode, *PBufNode;

PDEVICE_OBJECT g_pDeviceObject = NULL;

NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING	pObjectName,
	ULONG attributes,
	PACCESS_STATE pAccessState,
	ACCESS_MASK accessMask,
	POBJECT_TYPE pObjectType,
	KPROCESSOR_MODE pAccessMode,
	PVOID pParseContext,
	PVOID *pObject);


VOID DriverUnload(PDRIVER_OBJECT driver)
{
	UNICODE_STRING strSymbolName = RTL_CONSTANT_STRING(L"\\??\\symboltestname");
	IoDeleteSymbolicLink(&strSymbolName);
	if(g_pDeviceObject != NULL)
	{
		if(((DEVOBJ_EXTENSION*)(g_pDeviceObject->DeviceExtension))->AttachedTo != NULL)
		{
			IoDetachDevice(((DEVOBJ_EXTENSION*)(g_pDeviceObject->DeviceExtension))->AttachedTo);
			((DEVOBJ_EXTENSION*)(g_pDeviceObject->DeviceExtension))->AttachedTo = NULL;
		}
		IoDeleteDevice(g_pDeviceObject);
	}

	DbgPrint("fist driver\r\n");
}

VOID GetTime(PULONG nMillSecond)
{
	LARGE_INTEGER nTickCount = {0};
	ULONG nInc = 0;
	LARGE_INTEGER nGreenTime = {0};
	LARGE_INTEGER nLocalTime = {0};
	TIME_FIELDS timeFields = {0};
	WCHAR arrTime[64] = {0};

	nInc = KeQueryTimeIncrement();
	KeQueryTickCount(&nTickCount);
	nTickCount.QuadPart *= nInc;
	nTickCount.QuadPart /= 10000;

	*nMillSecond = nTickCount.LowPart;

	KeQuerySystemTime(&nGreenTime);
	ExSystemTimeToLocalTime(&nGreenTime, &nLocalTime);
	RtlTimeToTimeFields(&nLocalTime, &timeFields);
	
}

VOID SetTimer(PUserTimer pUserTimer)
{	
	KeSetTimer(&pUserTimer->timer, pUserTimer->nDelay, &pUserTimer->dpc);
}


VOID CustomDpc(struct _KDPC *pDpc, PVOID pDeferredContext, 
			   PVOID pSystemArgument1, PVOID pSystemArgument2)
{
	PUserTimer pUserTimer = (PUserTimer)pDeferredContext;

	DbgPrint("timer\r\n");

	pUserTimer->nTimes--;
	if(pUserTimer->nTimes == 0)
		KeCancelTimer(&pUserTimer->timer);
	else
		SetTimer(pUserTimer);
}

NTSTATUS  IOComplete(
     PDEVICE_OBJECT DeviceObject,
     PIRP Irp,
    PVOID Context
    )
{
	_asm int 3
	return STATUS_SUCCESS; 
}


#define CWK_DVC_RECV_STR  (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED,FILE_READ_DATA)
NTSTATUS Dispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	DEVOBJ_EXTENSION *pExt = ((DEVOBJ_EXTENSION*)(pDeviceObject->DeviceExtension));
	PDEVICE_OBJECT pAttachTo = NULL;
	PFILE_OBJECT pFile = NULL;

	PIO_STACK_LOCATION pIrpStackLoc = NULL;
	//IoSetCompletionRoutine(pIrp, IOComplete, NULL, TRUE, TRUE, TRUE);
	pIrpStackLoc = IoGetCurrentIrpStackLocation(pIrp);

	if(pIrpStackLoc != NULL)
	{
		pFile = pIrpStackLoc->FileObject;
		if(pFile != NULL)
		{
			if(pFile->FileName.Buffer != NULL)
			{
				
				DbgPrint("%wZ\r\n", pFile->FileName.Buffer);
			}
		}
	}



	//pIrpStackLoc->CompletionRoutine = IOComplete;


	IoCopyCurrentIrpStackLocationToNext(pIrp);
	if(pDeviceObject == g_pDeviceObject)
	{

		if(pIrpStackLoc->MajorFunction == IRP_MJ_CREATE || pIrpStackLoc->MajorFunction == IRP_MJ_CLOSE)
		{
			//_asm int 3
			DbgPrint("Create\r\n");
		}
		else //if(pIrpStackLoc->MajorFunction == IRP_MJ_DEVICE_CONTROL)
		{
			PVOID pBuffer = pIrp->AssociatedIrp.SystemBuffer;
			ULONG nInBufLen = pIrpStackLoc->Parameters.DeviceIoControl.InputBufferLength;
			ULONG nOutBufLen = pIrpStackLoc->Parameters.DeviceIoControl.OutputBufferLength;
			
			if(nInBufLen != 0)
			{
				//((char*)pBuffer)[nInBufLen-1] = '\0';
				//((char*)pBuffer)[0] = 'a';				
			}
			switch (pIrpStackLoc->Parameters.DeviceIoControl.IoControlCode)
			{
			case CWK_DVC_RECV_STR:
				break;
			default:
				break;
			}
			//DbgPrint("%x--%d--%s--Control\r\n", pBuffer, nInBufLen, (char*)pBuffer);
			
			DbgPrint("116\r\n");
			//pBuffer = ExAllocatePoolWithTag(NonPagedPool, 150, 'test');
			//ExFreePoolWithTag(pBuffer, 'test');
		}
	}

	if(pExt != NULL)
		pAttachTo = pExt->AttachedTo;

	//_asm int 3
	if(pAttachTo != NULL)
		IoCallDriver(((DEVOBJ_EXTENSION*)(pDeviceObject->DeviceExtension))->AttachedTo, pIrp);

	//pIrp->IoStatus.Information = 0;
	//pIrp->IoStatus.Status = STATUS_SUCCESS;
	//IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	int i = 0;
	UserTimer userTimer;
	HANDLE hFile = NULL;
	LARGE_INTEGER nInterval;
	OBJECT_ATTRIBUTES attr;
	NTSTATUS status;
	IO_STATUS_BLOCK ioStatus;
	PDEVICE_OBJECT pDeviceObject = NULL;
	PDEVICE_OBJECT pTargetDevice = NULL;
	UNICODE_STRING strSddl = RTL_CONSTANT_STRING(L"D:P(A;;GA;;;WD)");
	UNICODE_STRING strDOName = RTL_CONSTANT_STRING(L"\\Device\\testname");
	UNICODE_STRING strFileName = RTL_CONSTANT_STRING(L"\\??\\C:\\test.txt");
	UNICODE_STRING strSymbolName = RTL_CONSTANT_STRING(L"\\??\\symboltestname");
	UNICODE_STRING strTargetDeviceName = RTL_CONSTANT_STRING(L"\\Device\\HarddiskVolume2");
	InitializeObjectAttributes(&attr, &strFileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	status = ZwCreateFile(&hFile,  GENERIC_READ | GENERIC_WRITE, &attr, &ioStatus, 0, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN_IF, 
		FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_ALERT, NULL, 0);
	if(NT_SUCCESS(status))
	{
		LARGE_INTEGER nOffset = {0};
		status = ZwWriteFile(hFile, NULL, NULL, NULL, &ioStatus, L"hello", 10, &nOffset, NULL);
		//ZwReadFile(hFile);
		DbgPrint("OpenSucess\r\n");
		if(NT_SUCCESS(status))
			DbgPrint("%wZ\r\n", &strFileName);
		ZwClose(hFile);
	}
	//_asm int 3
	else
		DbgPrint("Operation Fail\r\n");
	driver->DriverUnload =  DriverUnload;

	nInterval.QuadPart = -10000*20000;
	

	userTimer.nTimes = 10;
	userTimer.pRoutine = CustomDpc;
	userTimer.nDelay.QuadPart = -10000000;
	userTimer.pPrivateContext = &userTimer;
	//KeInitializeTimer(&userTimer.timer);
	//KeInitializeDpc(&userTimer.dpc, userTimer.pRoutine, userTimer.pPrivateContext);
	//SetTimer(&userTimer);

	//KeDelayExecutionThread(KernelMode, 0, &nInterval);
	DbgPrint("Over\r\n");

	status = IoCreateDevice(driver, sizeof(DEVOBJ_EXTENSION), &strDOName, FILE_DEVICE_UNKNOWN,
		0, FALSE, &pDeviceObject);
	if(NT_SUCCESS(status))
	{
		pDeviceObject->Type = 3;
		g_pDeviceObject = pDeviceObject;
		status = IoCreateSymbolicLink(&strSymbolName, &strDOName);
		((DEVOBJ_EXTENSION*)(pDeviceObject->DeviceExtension))->AttachedTo = NULL;
		status = IoAttachDevice(pDeviceObject, &strTargetDeviceName, &pTargetDevice);
		if(NT_SUCCESS(status))
		{
			((DEVOBJ_EXTENSION*)(pDeviceObject->DeviceExtension))->AttachedTo = pTargetDevice;
			DbgPrint("%ud--attach device success\r\n", ((DEVOBJ_EXTENSION*)(pDeviceObject->DeviceExtension))->AttachedTo);
		}
		else 
			DbgPrint("attach device fail\r\n");
		
	}

	_asm int 3

	for(; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		driver->MajorFunction[i] = Dispatch;
	}

	return STATUS_SUCCESS;
}





