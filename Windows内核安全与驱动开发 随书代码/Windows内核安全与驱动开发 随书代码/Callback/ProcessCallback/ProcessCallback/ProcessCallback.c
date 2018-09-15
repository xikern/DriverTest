#include "ntddk.h"

/*����ԭ������*/
void  DriverUnload(__in struct _DRIVER_OBJECT  *DriverObject);

VOID CreateProcessNotifyEx(__inout PEPROCESS  Process,
						   __in HANDLE  ProcessId,
						   __in_opt PPS_CREATE_NOTIFY_INFO  CreateInfo);


typedef NTSTATUS (_stdcall *PPsSetCreateProcessNotifyRoutineEx)(
								  IN PCREATE_PROCESS_NOTIFY_ROUTINE_EX  NotifyRoutine,
								  IN BOOLEAN  Remove
								  );

/*ȫ�ֱ�������*/
PPsSetCreateProcessNotifyRoutineEx	g_pPsSetCreateProcessNotifyRoutineEx = NULL;
BOOLEAN g_bSuccRegister = FALSE;

NTSTATUS DriverEntry( __in struct _DRIVER_OBJECT* DriverObject, __in PUNICODE_STRING RegistryPath )
{
	NTSTATUS nStatus = STATUS_UNSUCCESSFUL;
	//_asm int 3;
	do 
	{
		UNICODE_STRING uFuncName = {0};
		DriverObject->DriverUnload = DriverUnload;
		RtlInitUnicodeString(&uFuncName,L"PsSetCreateProcessNotifyRoutineEx");
		g_pPsSetCreateProcessNotifyRoutineEx = (PPsSetCreateProcessNotifyRoutineEx)MmGetSystemRoutineAddress(&uFuncName);
		if( g_pPsSetCreateProcessNotifyRoutineEx == NULL )
		{
			break;
		}
		if( STATUS_SUCCESS != g_pPsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx,FALSE) )
		{
			break;
		}
		g_bSuccRegister = TRUE;
		nStatus = STATUS_SUCCESS;
	} 
	while (FALSE);
	
	return nStatus;
}


void  DriverUnload(__in struct _DRIVER_OBJECT  *DriverObject)
{
	if( g_bSuccRegister && g_pPsSetCreateProcessNotifyRoutineEx )
	{
		g_pPsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyEx,TRUE);
		g_bSuccRegister = FALSE;
	}
	return;
}

VOID CreateProcessNotifyEx(
					  __inout PEPROCESS  Process,
					  __in HANDLE  ProcessId,
					  __in_opt PPS_CREATE_NOTIFY_INFO  CreateInfo
					  )
{
	HANDLE hParentProcessID = NULL;/*������ID*/
	HANDLE hPareentThreadID = NULL;/*�����̵��߳�ID*/
	HANDLE hCurrentThreadID = NULL;/*�ص�����CreateProcessNotifyEx��ǰ�߳�ID*/
	hCurrentThreadID = PsGetCurrentThreadId();
	if( CreateInfo == NULL )
	{
		/*�����˳�*/
		DbgPrint("CreateProcessNotifyEx [Destroy][CurrentThreadId: 0x%x][ProcessId = 0x%x]\n",hCurrentThreadID,ProcessId);
		return;
	}
	/*��������*/
	hParentProcessID = CreateInfo->CreatingThreadId.UniqueProcess;
	hPareentThreadID = CreateInfo->CreatingThreadId.UniqueThread;
	
	DbgPrint("CreateProcessNotifyEx [Create][CurrentThreadId: 0x%x][ParentID 0x%x:0x%x][ProcessId = 0x%x,ProcessName=%wZ]\n",
					hCurrentThreadID,hParentProcessID,hPareentThreadID,ProcessId,CreateInfo->ImageFileName);
	return;
}