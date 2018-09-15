#include "Declare.h"

NTSTATUS Dispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{

	NTSTATUS status = STATUS_SUCCESS;
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

	pDriverObject->MajorFunction[IRP_MJ_POWER] = NULL;
	pDriverObject->MajorFunction[IRP_MJ_PNP] = NULL;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NULL;
	pDriverObject->MajorFunction[IRP_MJ_READ] = NULL;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = NULL;

	pDriverObject->DriverExtension->AddDevice = NULL;

	pDriverObject->DriverUnload = DriverUnload;

	IoRegisterBootDriverReinitialization(
		pDriverObject,
		NULL,
		NULL
		);

	return status;
}





