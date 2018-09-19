#include "Declare.h"

NTSTATUS Dispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDeviceObjectExtension pDeviceObjectExt = (PDeviceObjectExtension)pDeviceObject->DeviceExtension;
	return SendToNextDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
}

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath)
{
	NTSTATUS status = STATUS_SUCCESS;
	int nIndex = 0;

	for(nIndex = 0; nIndex <= IRP_MJ_MAXIMUM_FUNCTION; nIndex++)
	{
		pDriverObject->MajorFunction[nIndex] = Dispatch;
	}

	pDriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;
	pDriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPNP;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchControl;
	pDriverObject->MajorFunction[IRP_MJ_READ] = DispatchReadWrite;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = DispatchReadWrite;

	pDriverObject->DriverExtension->AddDevice = DriverAddDevice;

	pDriverObject->DriverUnload = DriverUnload;

	IoRegisterBootDriverReinitialization(
		pDriverObject,
		NULL,
		NULL
		);

	return status;
}





