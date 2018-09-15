#pragma once
#include <ntddk.h>

#define MAKELONG(low, high) \
	((uint_32)(((uint_16)((uint_32)(low) & 0xffff)) | \
	((uint_32)((uint_16)((uint_32)(high) & 0xffff))) << 16))

#define LOW16_OF_32(data) \
	((uint_16)(((uint_32)data) & 0xffff))

#define HIGH16_OF_32(data) \
	((uint_16)(((uint_32)data) >> 16))

#define OBUFFER_FULL 0x02
#define IBUFFER_FULL 0x01

extern POBJECT_TYPE *IoDriverObjectType;
extern ULONG g_nKeyCount;

typedef unsigned char uint_8;
typedef unsigned short uint_16;
typedef unsigned long uint_32;

#pragma pack(push, 1)

typedef struct _IDR
{
	uint_16 nLimit;
	uint_32 nBase;
}IDR, *PIDR;

typedef struct _IDTEntry
{
	uint_16 nOffsetLow;
	uint_16 nSelector;
	uint_8 nReserved;
	uint_8 nType:4;
	uint_8 nAlways0:1;
	uint_8 nDpl:2;
	uint_8 nPresent:1;
	uint_16 nOffestHigh;
}IDTEntry, *PIDTEntry;

#pragma pack(pop)

typedef struct _KeyBoardInputData
{
	USHORT nUnitId;
	USHORT nMakeCode;
	USHORT nFlags;
	USHORT nReserved;
	ULONG nExtraInformation;
}KeyBoardInputData, *PKeyBoardInputData;

typedef struct Device_Object_Extension
{
	//DEVOBJ_EXTENSION
	PDEVICE_OBJECT pOwnDeviceObject;
	PDEVICE_OBJECT pTargetDeviceObject;
	PDEVICE_OBJECT pLowerDeviceObject;

}DeviceObjectExt, *PDeviceObjectExt;

extern PVOID g_OldFuncAddr;


NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING pObjectName,
	ULONG nAttributes,
	PACCESS_STATE pAccessState,
	ACCESS_MASK desiredAccess,
	POBJECT_TYPE pObjectType,
	KPROCESSOR_MODE pAccessMode,
	PVOID pParseContext,
	PVOID *pObject
			);



PVOID GetIDT();

VOID HookInt93(BOOLEAN bHookOrUnHook);

ULONG WaitForKbRead();
ULONG WaitForKbWrite();
VOID KeyBoardFilter();

NTSTATUS  AttachDevices(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath);

NTSTATUS ReadComplete(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext);


