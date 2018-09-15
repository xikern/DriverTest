#include "Declare.h"


#define KBD_DRIVER_NAME L"\\Driver\\kbdclass"

ULONG g_nKeyCount = 0;


PVOID GetIDT()
{
	IDR idrt;
	_asm int 3
	_asm sidt idrt
	return (PVOID)idrt.nBase;
}

VOID _declspec(naked) InterruptProc()
{
	_asm
	{
		int 3
		pushad
		pushfd
		call KeyBoardFilter

		popfd
		popad
		jmp g_OldFuncAddr
	}
}

VOID HookInt93(BOOLEAN bHookOrUnHook)
{
	PIDTEntry pIDTEntry = (PIDTEntry)GetIDT();
	pIDTEntry += 0x93;
	if(bHookOrUnHook)
	{
		DbgPrint("Hook\r\n");
		g_OldFuncAddr = (PVOID)MAKELONG(pIDTEntry->nOffsetLow, pIDTEntry->nOffestHigh);
		pIDTEntry->nOffsetLow = LOW16_OF_32(InterruptProc);
		pIDTEntry->nOffestHigh = HIGH16_OF_32(InterruptProc);
		DbgPrint("OldAddr%u\r\n", g_OldFuncAddr);
	}
	else
	{
		DbgPrint("UnHook\r\n");
		pIDTEntry->nOffsetLow = LOW16_OF_32(g_OldFuncAddr);
		pIDTEntry->nOffestHigh = HIGH16_OF_32(g_OldFuncAddr);		
	}

}

ULONG WaitForKbRead()
{
	int nIndex = 100;
	uint_8 nChar;
	do
	{
		_asm in al, 0x64
		_asm mov nChar, al
		KeStallExecutionProcessor(50);
		if(!(nChar & OBUFFER_FULL))
			break;
	}
	while(nIndex--);

	if(nIndex != 0)
		return TRUE;
	return FALSE;
}

ULONG WaitForKbWrite()
{
	int nIndex = 100;
	uint_8 nChar;
	do
	{
		_asm in al, 0x64
		_asm mov nChar, al
		KeStallExecutionProcessor(50);
		if(!(nChar & IBUFFER_FULL))
			break;
	}
	while(nIndex--);

	if(nIndex != 0)
		return TRUE;
	return FALSE;
}

VOID KeyBoardFilter()
{
	static uint_8 nPre = 0;
	uint_8 nCh = 0;
	_asm int 3
	WaitForKbRead();
	_asm in al, 0x60
	_asm mov nCh, al
	DbgPrint("Scan Code = %2x\r\n", nCh);

	if(nPre != nCh)
	{
		nPre = nCh;
		_asm mov al, 0xd2
		_asm out 0x64, al
		WaitForKbWrite();
		_asm mov al, nCh
		_asm out 0x60, al
	}
}

NTSTATUS AttachDevices(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING strDriverName;
	PDeviceObjectExt pDeviceObjectExt = NULL;
	PDEVICE_OBJECT pFilterDeviceObject = NULL;
	PDEVICE_OBJECT pTargetDeviceObject = NULL;
	PDEVICE_OBJECT pLowerDeviceObject = NULL;
	PDRIVER_OBJECT pKbdDriverObject = NULL;

	RtlInitUnicodeString(&strDriverName, KBD_DRIVER_NAME);

	status = ObReferenceObjectByName(&strDriverName, OBJ_CASE_INSENSITIVE,
		NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&pKbdDriverObject);

	if(!NT_SUCCESS(status))
	{
		DbgPrint("Reference Fail\r\n");
		return status;
	}

	DbgPrint("Reference Success\r\n");

	pTargetDeviceObject = pKbdDriverObject->DeviceObject;

	while(pTargetDeviceObject != NULL)
	{
		status = IoCreateDevice(pDriverObject, sizeof(DeviceObjectExt), NULL, pTargetDeviceObject->DeviceType,
			pTargetDeviceObject->Characteristics, FALSE, &pFilterDeviceObject);
		if(!NT_SUCCESS(status))
		{
			DbgPrint("Create Device Failure\r\n");
			goto end;
		}
		DbgPrint("Create Device Success\r\n");

		pLowerDeviceObject = IoAttachDeviceToDeviceStack(pFilterDeviceObject, pTargetDeviceObject);
		if(pLowerDeviceObject == NULL)
		{
			DbgPrint("Attach Device Failure\r\n");
			IoDeleteDevice(pFilterDeviceObject);
			goto end;
		}
		DbgPrint("Attach Device Success\r\n");

		pDeviceObjectExt = (PDeviceObjectExt)pFilterDeviceObject->DeviceExtension;
		if(pDeviceObjectExt != NULL)
		{
			pDeviceObjectExt->pOwnDeviceObject = pFilterDeviceObject;
			pDeviceObjectExt->pTargetDeviceObject = pTargetDeviceObject;
			pDeviceObjectExt->pLowerDeviceObject = pLowerDeviceObject;
		}

		pFilterDeviceObject->DeviceType = pTargetDeviceObject->DeviceType;
		pFilterDeviceObject->Characteristics = pTargetDeviceObject->Characteristics;
		pFilterDeviceObject->StackSize = pLowerDeviceObject->StackSize + 1;
		pFilterDeviceObject->Flags |= pLowerDeviceObject->Flags & 
			(DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);

		pTargetDeviceObject = pTargetDeviceObject->NextDevice;
	}

end:
	ObDereferenceObject(pKbdDriverObject);
	return status;
}

NTSTATUS ReadComplete(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext)
{
	PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);
	PKeyBoardInputData pKeyBoardInputData = NULL;
	ULONG nKeys = 0;
	ULONG nBufLen = 0;
	PUCHAR pBuf = NULL;
	size_t nIndex = 0;

	if(NT_SUCCESS(pIrp->IoStatus.Status))
	{
		pBuf = (PUCHAR)pIrp->AssociatedIrp.SystemBuffer;
		pKeyBoardInputData = (PKeyBoardInputData)pBuf;
		nBufLen = pIrp->IoStatus.Information;
		nKeys = nBufLen / sizeof(KeyBoardInputData);
		for(nIndex = 0; nIndex < nKeys; nIndex++)
		{
			DbgPrint("nKeys:%d\r\n", nKeys);
			DbgPrint("ScanCode:%x\r\n", pKeyBoardInputData[nIndex].nMakeCode);
			DbgPrint("%s\r\n", pKeyBoardInputData[nIndex].nFlags ? "Up" : "Down");
		}
	}
	
	g_nKeyCount--;

	if(pIrp->PendingReturned)
	{
		IoMarkIrpPending(pIrp);
	}

	return pIrp->IoStatus.Status;
}

