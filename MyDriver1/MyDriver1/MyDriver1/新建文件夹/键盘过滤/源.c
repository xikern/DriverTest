#include "Declare.h"

PVOID g_OldFuncAddr = NULL;

NTSTATUS DispatchPower(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	PDeviceObjectExt pDeviceObjectExt = (PDeviceObjectExt)(pDeviceObject->DeviceExtension);
	PoStartNextPowerIrp(pIrp);
	IoSkipCurrentIrpStackLocation(pIrp);
	if(pDeviceObjectExt != NULL && 
		pDeviceObjectExt->pLowerDeviceObject != NULL)
		PoCallDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
	
	return STATUS_SUCCESS;
}

NTSTATUS DispatchPnp(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	PDeviceObjectExt pDeviceObjectExt = (PDeviceObjectExt)(pDeviceObject->DeviceExtension);
	PIO_STACK_LOCATION pIrpStackLocation = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	KIRQL irql;
	KEVENT event;

	pIrpStackLocation = IoGetNextIrpStackLocation(pIrp);
	switch (pIrpStackLocation->MinorFunction)
	{
	case IRP_MN_REMOVE_DEVICE:
		IoSkipCurrentIrpStackLocation(pIrp);
		if(pDeviceObjectExt != NULL && 
		pDeviceObjectExt->pLowerDeviceObject != NULL)
		{
			status = IoCallDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
			IoDetachDevice(pDeviceObjectExt->pLowerDeviceObject);
		}
		IoDeleteDevice(pDeviceObject);
		break;
	default:
		if(pDeviceObjectExt != NULL && 
		pDeviceObjectExt->pLowerDeviceObject != NULL)
		{
			status = IoCallDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
		}
		break;
	}

	return status;
}

NTSTATUS DispatchRead(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDeviceObjectExt pDeviceObjectExt = (PDeviceObjectExt)pDeviceObject->DeviceExtension;
	PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);
	KEVENT event;

	if(pDeviceObject == NULL)
		return status;

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	if(pIrp->CurrentLocation == 1)
	{
		ULONG nRet = 0;
		status = STATUS_INVALID_DEVICE_REQUEST;
		pIrp->IoStatus.Status = status;
		pIrp->IoStatus.Information = nRet;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return status;
	}

	g_nKeyCount++;
	IoCopyCurrentIrpStackLocationToNext(pIrp);
	IoSetCompletionRoutine(pIrp, ReadComplete, pDeviceObject, TRUE, TRUE, TRUE);
	return IoCallDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
}

NTSTATUS Dispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	if(pDeviceObject != NULL)
	{
		PDeviceObjectExt pDeviceObjectExt = (PDeviceObjectExt)(pDeviceObject->DeviceExtension);
		IoSkipCurrentIrpStackLocation(pIrp);
		if(pDeviceObjectExt != NULL && 
			pDeviceObjectExt->pLowerDeviceObject != NULL)
			IoCallDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
	}


	return STATUS_SUCCESS;
}

VOID UnloadDriver(PDRIVER_OBJECT pDriverObject)
{
	HookInt93(FALSE);

	/*
	LARGE_INTEGER nDelay = {0};
	PRKTHREAD pCurrentThread = NULL;
	PDEVICE_OBJECT pFilterDeviceObject = NULL;
	PDeviceObjectExt pDeviceObjectExt = NULL;
	
	nDelay = RtlConvertLongToLargeInteger(-10000 * 100);
	pCurrentThread = KeGetCurrentThread();
	KeSetPriorityThread(pCurrentThread, LOW_REALTIME_PRIORITY);
	UNREFERENCED_PARAMETER(pDriverObject);

	pFilterDeviceObject = pDriverObject->DeviceObject;
	while(pFilterDeviceObject != NULL)
	{
		pDeviceObjectExt = (PDeviceObjectExt)pFilterDeviceObject->DeviceExtension;
		if(pDeviceObjectExt != NULL && pDeviceObjectExt->pLowerDeviceObject != NULL)
			IoDetachDevice(pDeviceObjectExt->pLowerDeviceObject);
		pFilterDeviceObject = pFilterDeviceObject->NextDevice;
	}

	while(g_nKeyCount > 0)
	{
		KeDelayExecutionThread(KernelMode, FALSE, &nDelay);
	}
	*/
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath)
{

	ULONG nIndex = 0;
	NTSTATUS status;

	for(nIndex = 0; nIndex < IRP_MJ_MAXIMUM_FUNCTION; nIndex++)
	{
		pDriverObject->MajorFunction[nIndex] = Dispatch;
	}

	//pDriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;
	//pDriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
	//pDriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;
	pDriverObject->DriverUnload = UnloadDriver;

	//AttachDevices(pDriverObject, pRegPath);
	HookInt93(TRUE);
	return STATUS_SUCCESS;
}




