#pragma once
#include <ntddk.h>
#include <ntddvol.h>

#define BITMAP_ERR_OUTOFRANGE	-1
#define BITMAP_ERR_ALLOCMEMORY	-2
#define BITMAP_SUCCESS			0
#define BITMAP_BIT_SET			1
#define BITMAP_BIT_CLEAR		2
#define BITMAP_BIT_UNKNOW		3
#define BITMAP_RANGE_SET		4
#define BITMAP_RANGE_CLEAR		5
#define BITMAP_RANGE_BLEND		6
#define BITMAP_RANGE_SIZE		25600
#define BITMAP_RANGE_SIZE_SMALL 256
#define BITMAP_RANGE_SIZE_MAX	51684
#define BITMAP_RANGE_AMOUNT		16*1024


#pragma pack(1)
typedef struct _NtfsBootSector
{
	UCHAR arrJump[3];
	UCHAR arrFsid[8];
	USHORT nBytesPerSector;
	UCHAR cSectorsPerCluster;
	USHORT nReservedSectors;
	UCHAR cMbz1;
	USHORT cMbz2;
	UCHAR cMediaDesc;
	USHORT nMbz3;
	USHORT nSectorsPerTrack;
	USHORT nHeads;
	ULONG nHiddenSectors;
	ULONG arrReserved2[2];
	ULONGLONG nTotalSectors;
	ULONGLONG MftStartLcn;
	ULONGLONG Mft2StartLcn;
}NtfsBootSector, *PNtfsBootSector;

typedef struct _Fat32BootSector
{
	UCHAR		cJMPInstruction[3];
	UCHAR		arrOEM[8];
	USHORT		nBytesPerSector;
	UCHAR		cSectorsPerCluster;
	USHORT		nReservedSectors;
	UCHAR		cNumberOfFATs;
	USHORT		nRootEntries;
	USHORT		nSectors;
	UCHAR		cMediaDescriptor;
	USHORT		nSectorsPerFAT;
	USHORT		nSectorsPerTrack;
	USHORT		nHeads;
	DWORD32		nHiddenSectors;
	DWORD32		nLargeSectors;
	DWORD32		nLargeSectorsPerFAT;
	UCHAR		arrData[24];
	UCHAR		cPhysicalDriveNumber;
	UCHAR		cCurrentHead;
} Fat32BootSector, *PFat32BootSector;

typedef struct _Fat16BootSector
{
	UCHAR		arrJMPInstruction[3];
	UCHAR		arrOEM[8];
	USHORT		nBytesPerSector;
	UCHAR		cSectorsPerCluster;
	USHORT		nReservedSectors;
	UCHAR		cNumberOfFATs;
	USHORT		nRootEntries;
	USHORT		nSectors;
	UCHAR		cMediaDescriptor;
	USHORT		nSectorsPerFAT;
	USHORT		nSectorsPerTrack;
	USHORT		nHeads;
	DWORD32		nHiddenSectors;
	DWORD32		nLargeSectors;
	UCHAR		cPhysicalDriveNumber;
	UCHAR		cCurrentHead;
} Fat16BootSector, *PFat16BootSector;

typedef struct _BitMap
{
	ULONG nSectorSize;
	ULONG nByteSize;
	ULONG nRegionSize;
	ULONG nRegionNumber;
	ULONG nRegionReferSize;
	LONGLONG nBitMapReferSize;
	UCHAR **pBitMap;
	PVOID pLockBitMap;
}BitMap, *PBitMap;
#pragma pack()

typedef struct _DeviceObjectExtension
{
	WCHAR wVolumeLetter;
	BOOLEAN bProtect;
	LARGE_INTEGER nTotalSizeInByte;
	DWORD32 nClusterSizeInByte;
	DWORD32 nSectorSizeInByte;
	PDEVICE_OBJECT pFilterDeviceObject;
	PDEVICE_OBJECT pLowerDeviceObject;
	PDEVICE_OBJECT pPhysicDeviceObject;
	BOOLEAN bInitializeCompleted;
	PBitMap pBitMap;
	HANDLE hTempFile;
	LIST_ENTRY lstReq;
	KSPIN_LOCK lckReq;
	KEVENT eventReq;
	PVOID pThreadHandle;
	BOOLEAN bThreadTermFlag;
	KEVENT eventPagingPathCountEvent;
	LONG nPagingPathCount;

}DeviceObjectExtension, *PDeviceObjectExtension;

typedef struct _VolumeOnlineContext
{
	//在volume_online的DeviceIoControl中传给完成函数的设备扩展
	PDeviceObjectExtension		pDeviceObjectExt;
	//在volume_online的DeviceIoControl中传给完成函数的同步事件
	PKEVENT						pEvent;
}VolumeOnlineContext, *PVolumeOnlineContext;

NTSTATUS BitMapInit(PBitMap * pBitmap, unsigned long nSectorSize,
	unsigned long nByteSize, unsigned long nRegionSize,
	unsigned long nRegionNumber);

long BitMapTest(PBitMap pBitMap, LARGE_INTEGER offset, unsigned long nLength);

NTSTATUS BitMapGet(PBitMap pBitMap, LARGE_INTEGER offset, ULONG nLength, PVOID pBufInOut, PVOID pBufIn);

NTSTATUS BitMapSet(PBitMap pBitMap, LARGE_INTEGER offset, ULONG nLength);

void BitMapFree(PBitMap pBitMap);

NTSTATUS IrpCompletionRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext);

NTSTATUS ForwardIrpSync(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS SendToNextDriver(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS VolumeOnlieCompletionRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext);

NTSTATUS QueryVolumeInformationCompletionRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext);

NTSTATUS QueryVolumeInformation(PDEVICE_OBJECT pDeviceObject, LARGE_INTEGER *pTotalSize, DWORD32 *pClusterSize, DWORD32 *pSectorSize);

VOID ReadWriteThread(PVOID pContext);

VOID ReInitializationRoutine(PDRIVER_OBJECT pDriverObject, PVOID pContext, ULONG nCount);

NTSTATUS DriverAddDevice(PDRIVER_OBJECT pDriverObject, PDEVICE_OBJECT pDeviceObject);

NTSTATUS DispatchPower(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS DispatchPNP(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS DispatchControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);

NTSTATUS DispatchReadWrite(PDEVICE_OBJECT pDeviceObjec, PIRP pIrp);

