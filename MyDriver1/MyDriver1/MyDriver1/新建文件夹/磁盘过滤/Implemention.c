#include "Declare.h"

static UCHAR bitmapMask[8] =
{
	//需要用到的bitmap的位掩码
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

PDeviceObjectExtension g_pDeviceObjectExt = NULL;

VOID ReInitializationRoutine(PDRIVER_OBJECT pDriverObject, PVOID pContext, ULONG nCount)
{
	NTSTATUS status;
	WCHAR arrFileName[] = L"\\??\\C:\\tem.dat";
	UNICODE_STRING fileName;
	IO_STATUS_BLOCK ioStatusBlock = {0};
	OBJECT_ATTRIBUTES attr = {0};
	FILE_END_OF_FILE_INFORMATION fileEndInfo = {0};

	RtlInitUnicodeString(&fileName, arrFileName);
	InitializeObjectAttributes(&attr, &fileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL, NULL);
	status = ZwCreateFile(&g_pDeviceObjectExt->hTempFile, GENERIC_READ | GENERIC_WRITE,
		&attr, &ioStatusBlock, NULL,
		FILE_ATTRIBUTE_NORMAL, 0, 		FILE_OVERWRITE_IF,
		FILE_NON_DIRECTORY_FILE |
		FILE_RANDOM_ACCESS |
		FILE_SYNCHRONOUS_IO_NONALERT |
		FILE_NO_INTERMEDIATE_BUFFERING,
		NULL,
		0);

	status = ZwDeviceIoControlFile(g_pDeviceObjectExt->hTempFile, NULL, NULL, NULL,
		&ioStatusBlock,  
		CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 49, METHOD_BUFFERED, FILE_SPECIAL_ACCESS),
		NULL, 0, NULL, 0);
	if(!NT_SUCCESS(status))
	{
		goto end;
	}

	fileEndInfo.EndOfFile.QuadPart = g_pDeviceObjectExt->nTotalSizeInByte.QuadPart +
									 10 * 1024 * 1024;
	status = ZwSetInformationFile(g_pDeviceObjectExt->hTempFile, &ioStatusBlock,
		 &fileEndInfo, sizeof(FILE_END_OF_FILE_INFORMATION), FileEndOfFileInformation);
	if(!NT_SUCCESS(status))
	{
		goto end;
	}
	g_pDeviceObjectExt->bProtect = TRUE;
	return;
end:
	DbgPrint("Create File Fail\n");
	return;
}

NTSTATUS BitMapInit(PBitMap * ppBitMap, unsigned long nSectorSize,
	unsigned long nByteSize, unsigned long nRegionSize,
	unsigned long nRegionNumber)
{
	int nIndex = 0;
	PBitMap pBitMap = NULL;

	if(pBitMap == NULL || nSectorSize == 0 || nByteSize == 0 || nRegionSize == 0 || nRegionNumber == 0)
		return STATUS_UNSUCCESSFUL;
	
	pBitMap = (PBitMap)ExAllocatePoolWithTag(NonPagedPool, sizeof(BitMap), 'btm');
	if(pBitMap == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(pBitMap, sizeof(BitMap));
	pBitMap->nSectorSize = nSectorSize;
	pBitMap->nByteSize = nByteSize;
	pBitMap->nRegionSize = nRegionSize;
	pBitMap->nRegionNumber = nRegionNumber;
	pBitMap->nRegionReferSize = nByteSize * nRegionSize * nSectorSize;
	pBitMap->nBitMapReferSize = (__int64)nRegionNumber * (__int64)nByteSize * (__int64)nRegionSize * (__int64)nSectorSize;

	pBitMap->pBitMap = (UCHAR**)ExAllocatePoolWithTag(NonPagedPool, sizeof(UCHAR*) * nRegionNumber, 'btmp');
	if(pBitMap->pBitMap == NULL)
	{
		ExFreePoolWithTag(pBitMap, 'btm');
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(pBitMap->pBitMap, sizeof(UCHAR*) * nRegionNumber);
	*ppBitMap = pBitMap;
	return STATUS_SUCCESS;
}

void BitMapFree(PBitMap pBitMap)
{
	DWORD32 nIndex = 0;
	if(pBitMap != NULL)
	{
		if(pBitMap->pBitMap != NULL)
		{
			for(nIndex = 0; nIndex < pBitMap->nRegionNumber; nIndex++)
			{
				if(*(pBitMap->pBitMap + nIndex) != NULL)
				{
					ExFreePool(*(pBitMap->pBitMap + nIndex));
				}
			}
			ExFreePool(pBitMap->pBitMap);
		}
		ExFreePool(pBitMap);
	}
}

long BitMapTest(PBitMap pBitMap, LARGE_INTEGER offset, unsigned long nLength)
{
	char cFlag = 0;
	unsigned long nIndex = 0;
	unsigned long nRegion = 0;
	unsigned long nRegionOffset = 0;
	unsigned long nByteOffset = 0;
	unsigned long nBitPos = 0;
	long nRet = BITMAP_BIT_UNKNOW;

	__try
	{
		if(pBitMap == NULL || offset.QuadPart < 0 || offset.QuadPart + nLength > pBitMap->nBitMapReferSize)
		{
			nRet = BITMAP_BIT_UNKNOW;
			__leave;
		}

		for(nIndex = 0; nIndex < nLength; nIndex += pBitMap->nSectorSize)
		{
			nRegion = (unsigned long)(offset.QuadPart + (__int64)nIndex) / (__int64)pBitMap->nRegionReferSize;
			nRegionOffset =  (unsigned long)((offset.QuadPart + (__int64)nIndex) % (__int64)pBitMap->nRegionReferSize);
			nByteOffset = nRegionOffset / pBitMap->nByteSize / pBitMap->nSectorSize;
			nBitPos = (nRegionOffset / pBitMap->nSectorSize) % pBitMap->nByteSize;

			if(*(pBitMap->pBitMap + nRegion) && (*(*(pBitMap->pBitMap + nRegion) + nByteOffset)
				& bitmapMask[nBitPos]))
			{
				cFlag |= 0x2;
			}
			else
			{
				cFlag |= 0x1;
			}
			if(cFlag == 0x3)
				break;
		}

		if(cFlag == 0x2)
			nRet = BITMAP_RANGE_SET;
		else if(cFlag == 0x1)
			nRet = BITMAP_RANGE_CLEAR;
		else if(cFlag == 0x3)
			nRet = BITMAP_RANGE_BLEND;

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		nRet = BITMAP_BIT_UNKNOW;
	}
	return nRet;
}

NTSTATUS BitMapGet(PBitMap pBitMap, LARGE_INTEGER offset, ULONG nLength, PVOID pBufInOut, PVOID pBufIn)
{
	unsigned long nIndex = 0;
	unsigned long nRegion = 0;
	unsigned long nRegionOffset = 0;
	unsigned long nByteOffset = 0;
	unsigned long nBitPos = 0;
	NTSTATUS status = STATUS_SUCCESS;

	__try
	{
		if(pBitMap == NULL || offset.QuadPart < 0 || offset.QuadPart + nLength > pBitMap->nBitMapReferSize)
		{
			status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		if(offset.QuadPart % pBitMap->nSectorSize != 0 || nLength % pBitMap->nSectorSize != 0)
		{
			status = STATUS_INVALID_PARAMETER;
			__leave;		
		}

		for(nIndex = 0; nIndex < nLength; nIndex += pBitMap->nSectorSize)
		{
			nRegion = (ULONG)(((offset.QuadPart) + (__int64)nIndex) / (__int64)pBitMap->nRegionReferSize);
			nRegionOffset = (ULONG)((offset.QuadPart + (__int64)nIndex) % (__int64)pBitMap->nRegionReferSize);
			nByteOffset = nByteOffset = nRegionOffset / pBitMap->nByteSize / pBitMap->nSectorSize;
			nBitPos = (nRegionOffset / pBitMap->nSectorSize) % pBitMap->nByteSize;

			if(*(pBitMap->pBitMap + nRegion) != NULL && (*(*(pBitMap->pBitMap + nRegion) + nByteOffset) & bitmapMask[nBitPos]))
			{
				memcpy((PUCHAR)pBufInOut + nIndex, (PUCHAR)pBufIn + nIndex, pBitMap->nSectorSize);
			}
		}
		status = STATUS_SUCCESS;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		status = STATUS_UNSUCCESSFUL;
	}
	return status;
}

NTSTATUS BitMapSet(PBitMap pBitMap, LARGE_INTEGER offset, ULONG nLength)
{
	__int64 nIndex = 0;
	ULONG nRegion = 0, nRegionEnd = 0;
	ULONG nRegionOffset = 0, nRegionOffsetEnd = 0;
	ULONG nByteOffset = 0, nByteOffsetEnd = 0;
	ULONG nBitPos = 0;
	NTSTATUS status = STATUS_SUCCESS;
	LARGE_INTEGER setBegin = {0}, setEnd = {0};

	__try
	{
		if(pBitMap == NULL || offset.QuadPart < 0)
		{
			status = STATUS_INVALID_PARAMETER;
			__leave;
		}
		if(offset.QuadPart % pBitMap->nSectorSize != 0 || nLength % pBitMap->nSectorSize != 0)
		{
			status = STATUS_INVALID_PARAMETER;
			__leave;			
		}
		nRegion = (ULONG)(offset.QuadPart / (__int64)pBitMap->nRegionReferSize);
		nRegionEnd = (ULONG)((offset.QuadPart + (__int64)nLength) / (__int64)pBitMap->nRegionReferSize);
		for(nIndex = nRegion; nIndex <= nRegionEnd; nIndex++)
		{
			if(*(pBitMap->pBitMap + nIndex) == NULL)
			{
					*(pBitMap->pBitMap + nIndex) = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, sizeof(UCHAR) * 
					pBitMap->nRegionSize, 'btm');
				if(*(pBitMap->pBitMap + nIndex) == NULL)
				{
					status = STATUS_INVALID_PARAMETER;
					__leave;					
				}
				else
				{
					memset(*(pBitMap->pBitMap + nIndex), 0, sizeof(UCHAR) * pBitMap->nRegionSize);
				}		
			}
		}

		for(nIndex = offset.QuadPart; nIndex < offset.QuadPart + (__int64)nLength; nIndex += pBitMap->nSectorSize)
		{
			nRegion = (ULONG)(nIndex / (__int64)pBitMap->nRegionReferSize);
			nRegionOffset = (ULONG)(nIndex % (__int64)pBitMap->nRegionReferSize);
			nByteOffset = nRegionOffset / pBitMap->nByteSize / pBitMap->nSectorSize;
			nBitPos = (nRegionOffset / pBitMap->nSectorSize) % pBitMap->nByteSize;
			if(nBitPos == 0)
			{
				setBegin.QuadPart = nIndex;
				break;
			}
			*(*(pBitMap->pBitMap + nRegion) + nByteOffset) |= bitmapMask[nBitPos];
		}

		if(nIndex >= offset.QuadPart + (__int64)nLength)
		{
			status = STATUS_SUCCESS;
			__leave;
		}

		for(nIndex = offset.QuadPart + (__int64)nLength - pBitMap->nSectorSize; nIndex >= offset.QuadPart;
			nIndex -= pBitMap->nSectorSize)
		{
			nRegion = (unsigned long)(nIndex / (__int64)pBitMap->nRegionReferSize);
			nRegionOffset = (unsigned long)(nIndex % (__int64)pBitMap->nRegionReferSize);
			nByteOffset = nRegionOffset / pBitMap->nByteSize / pBitMap->nSectorSize;
			nBitPos = (nRegionOffset / pBitMap->nSectorSize) % pBitMap->nByteSize;
			if(nBitPos == 7)
			{
				setEnd.QuadPart = nIndex;
				break;
			}
			*(*(pBitMap->pBitMap + nRegion) + nByteOffset) |= bitmapMask[nBitPos];
		}
		if(nIndex < offset.QuadPart || setEnd.QuadPart == setBegin.QuadPart)
		{
			status = STATUS_SUCCESS;
			__leave;
		}

		nRegionEnd = (unsigned long)(setEnd.QuadPart / (__int64)pBitMap->nRegionReferSize);
		for(nIndex = setBegin.QuadPart; nIndex <= setEnd.QuadPart;)
		{
			nRegion = (unsigned long)(nIndex / (__int64)pBitMap->nRegionReferSize);
			nRegionOffset = (unsigned long)(nIndex % (__int64)pBitMap->nRegionReferSize);
			nByteOffset = nRegionOffset / pBitMap->nByteSize / pBitMap->nSectorSize;
			if(nRegion == nRegionEnd)
			{
				nRegionOffsetEnd = (unsigned long)(setEnd.QuadPart % (__int64)pBitMap->nRegionReferSize);
				nByteOffsetEnd = nRegionOffsetEnd / pBitMap->nByteSize / pBitMap->nSectorSize;
				memset(*(pBitMap->pBitMap + nRegion) + nByteOffset, 0xff, nByteOffsetEnd - nByteOffset + 1);
				break;
			}
			else
			{
				nRegionOffsetEnd = pBitMap->nRegionReferSize;
				nByteOffsetEnd = nRegionOffsetEnd / pBitMap->nByteSize / pBitMap->nSectorSize;
				memset(*(pBitMap->pBitMap + nRegion) + nByteOffset, 0xff, nByteOffsetEnd - nByteOffset);
				nIndex += (nByteOffsetEnd - nByteOffset) * pBitMap->nByteSize * pBitMap->nSectorSize;
			}
		}
		status = STATUS_SUCCESS;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		status = STATUS_UNSUCCESSFUL;
	}

	if(!NT_SUCCESS(status))
	{
	
	}
	return status;
}

NTSTATUS SendToNextDriver(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{	
	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(pDeviceObject, pIrp);
}

NTSTATUS QueryVolumeInformationCompletionRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext)
{
	KeSetEvent((PKEVENT)pContext, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS QueryVolumeInformation(PDEVICE_OBJECT pDeviceObject, LARGE_INTEGER *pTotalSize, DWORD32 *pClusterSize, DWORD32 *pSectorSize)
{
#define FILE_SYSTEM_NAME_LENGTH 64
#define FAT16_SIG_OFFSET 54
#define FAT32_SIG_OFFSET 82
#define NTFS_SIG_OFFSET 3

	const UCHAR arrFat16Flag[4] = {'F', 'A', 'T', '1'};
	const UCHAR arrFat32Flag[4] = {'F', 'A', 'T', '3'};
	const UCHAR arrNtfsFlag[4] = {'N', 'T', 'F', 'S'};

	NTSTATUS status = STATUS_SUCCESS;
	UCHAR arrDbr[512] = {0};
	ULONG nDbrLength = 512;
	PNtfsBootSector pNtfsBootSector = (PNtfsBootSector)arrDbr;
	PFat32BootSector pFat32BootSector = (PFat32BootSector)arrDbr;
	PFat16BootSector pFat16BootSector = (PFat16BootSector)arrDbr;
	LARGE_INTEGER readOffset = {0};
	IO_STATUS_BLOCK ios;
	KEVENT event;
	PIRP pIrp = NULL;

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	pIrp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ, pDeviceObject, arrDbr, nDbrLength,
		&readOffset, &ios);
	if(pIrp == NULL)
	{
		goto end;
	}

	IoSetCompletionRoutine(pIrp, QueryVolumeInformationCompletionRoutine, &event,
		TRUE, TRUE, TRUE);

	status = IoCallDriver(pDeviceObject, pIrp);
	if(status == STATUS_PENDING)
	{
		status = KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		status = pIrp->IoStatus.Status;
		if(!NT_SUCCESS(status))
		{
			goto end;
		}
	}

	if(*(DWORD32*)arrNtfsFlag == *(DWORD32*)&arrDbr[NTFS_SIG_OFFSET])
	{
		*pSectorSize = (DWORD32)(pNtfsBootSector->nBytesPerSector);
		*pClusterSize = (*pSectorSize) * (DWORD32)(pNtfsBootSector->cSectorsPerCluster);
		pTotalSize->QuadPart = (LONGLONG)(*pSectorSize) * (LONGLONG)(pNtfsBootSector->nTotalSectors);
	}
	else if (*(DWORD32*)arrFat32Flag == *(DWORD32*)&arrDbr[FAT32_SIG_OFFSET])
	{
		//通过比较标志发现这个卷是一个ntfs文件系统的卷，下面根据ntfs卷的DBR定义来对各种需要获取的值进行赋值操作
		*pSectorSize = (DWORD32)(pFat32BootSector->nBytesPerSector);
		*pClusterSize = (*pSectorSize) * (DWORD32)(pFat32BootSector->cSectorsPerCluster);    
		pTotalSize->QuadPart = (LONGLONG)(*pSectorSize) * 
			(LONGLONG)(pFat32BootSector->nLargeSectors + pFat32BootSector->nSectors);
	}
	else if (*(DWORD32*)arrFat16Flag == *(DWORD32*)&arrDbr[FAT16_SIG_OFFSET])
	{
		//通过比较标志发现这个卷是一个ntfs文件系统的卷，下面根据ntfs卷的DBR定义来对各种需要获取的值进行赋值操作
		*pSectorSize = (DWORD32)(pFat16BootSector->nBytesPerSector);
		*pClusterSize = (*pSectorSize) * (DWORD32)(pFat16BootSector->cSectorsPerCluster);    
		pTotalSize->QuadPart = (LONGLONG)(*pSectorSize) * 
			(LONGLONG)(pFat16BootSector->nLargeSectors + pFat16BootSector->nSectors);
	}
	else
	{
		status = STATUS_UNSUCCESSFUL;
	}
end:
	if(pIrp != NULL)
	{
		IoFreeIrp(pIrp);
	}
	return status;
}

NTSTATUS VolumeOnlieCompletionRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING strDosName = {0};
	PVolumeOnlineContext pVolumeOnlineContext = (PVolumeOnlineContext)pContext;

	status = IoVolumeDeviceToDosName(pVolumeOnlineContext->pDeviceObjectExt->pPhysicDeviceObject, &strDosName);
	if(!NT_SUCCESS(status))
		goto end;
	pVolumeOnlineContext->pDeviceObjectExt->wVolumeLetter = strDosName.Buffer[0];
	if(pVolumeOnlineContext->pDeviceObjectExt->wVolumeLetter > L'Z')
		pVolumeOnlineContext->pDeviceObjectExt->wVolumeLetter -= (L'a' - L'A');

	if(pVolumeOnlineContext->pDeviceObjectExt->wVolumeLetter == L'D')
	{
		status = QueryVolumeInformation(pVolumeOnlineContext->pDeviceObjectExt->pPhysicDeviceObject,
			&(pVolumeOnlineContext->pDeviceObjectExt->nTotalSizeInByte),
			&(pVolumeOnlineContext->pDeviceObjectExt->nClusterSizeInByte),
			&(pVolumeOnlineContext->pDeviceObjectExt->nSectorSizeInByte));
	
		if(!NT_SUCCESS(status))
		{
			goto end;
		}

		status = BitMapInit(&pVolumeOnlineContext->pDeviceObjectExt->pBitMap, 
			pVolumeOnlineContext->pDeviceObjectExt->nSectorSizeInByte,
			8, 25600, (DWORD32)(pVolumeOnlineContext->pDeviceObjectExt->nTotalSizeInByte.QuadPart / 
			(LONGLONG)(25600 * 8 * pVolumeOnlineContext->pDeviceObjectExt->nSectorSizeInByte)) + 1);

		if(!NT_SUCCESS(status))
			goto end;

	}
end:
	if(!NT_SUCCESS(status))
	{
		if (NULL != pVolumeOnlineContext->pDeviceObjectExt->pBitMap)
		{
			ExFreePoolWithTag(pVolumeOnlineContext->pDeviceObjectExt->pBitMap->pBitMap, 'btmp');
			ExFreePoolWithTag(pVolumeOnlineContext->pDeviceObjectExt->pBitMap, 'btm');
		}
		if (NULL != pVolumeOnlineContext->pDeviceObjectExt->hTempFile)
		{
			ZwClose(pVolumeOnlineContext->pDeviceObjectExt->hTempFile);
		}
	}
	if(strDosName.Buffer != NULL)
		ExFreePool(strDosName.Buffer);

	KeSetEvent(pVolumeOnlineContext->pEvent, 0, FALSE);

	return status;
}

NTSTATUS IrpCompletionRoutine(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID pContext)
{
	PKEVENT pEvent = (PKEVENT)pContext;
	UNREFERENCED_PARAMETER(pDeviceObject);
	UNREFERENCED_PARAMETER(pIrp);

	KeSetEvent(pEvent, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS ForwardIrpSync(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	KEVENT event;
	NTSTATUS status;
	KeInitializeEvent(&event, NotificationEvent, FALSE);
	IoCopyCurrentIrpStackLocationToNext(pIrp);
	IoSetCompletionRoutine(pIrp, IrpCompletionRoutine, &event, TRUE, TRUE, TRUE);
	status = IoCallDriver(pDeviceObject, pIrp);
	if(STATUS_PENDING == status)
	{
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		status = pIrp->IoStatus.Status;
	}
	return status;
}

VOID ReadWriteThread(PVOID pContext)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDeviceObjectExtension pDeviceObjectExt = (PDeviceObjectExtension)pContext;
	PLIST_ENTRY  pListReq = NULL;
	PIRP pIrp = NULL;
	PIO_STACK_LOCATION pIoStackLocation = NULL;
	PUCHAR pSysBuf = NULL;
	ULONG nLength = 0;
	LARGE_INTEGER offset = {0};
	PUCHAR pFileBuf = NULL;
	PUCHAR pDevBuf = NULL;
	IO_STATUS_BLOCK ios;

	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

	for(; ;)
	{
		KeWaitForSingleObject(&pDeviceObjectExt->eventReq, Executive, KernelMode, FALSE, NULL);

		if(pDeviceObjectExt->bThreadTermFlag)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
			return;
		}
		while(pListReq = ExInterlockedRemoveHeadList(&pDeviceObjectExt->lstReq, &pDeviceObjectExt->lckReq))
		{
			pIrp = CONTAINING_RECORD(pListReq, IRP, Tail.Overlay.ListEntry);
			pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);
			if(pIrp->MdlAddress == NULL)
				pSysBuf = (PUCHAR)pIrp->UserBuffer;
			else
				pSysBuf = (PUCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);

			if(pIoStackLocation->MajorFunction == IRP_MJ_READ)
			{
				offset = pIoStackLocation->Parameters.Read.ByteOffset;
				nLength = pIoStackLocation->Parameters.Read.Length;
			}
			else if(pIoStackLocation->MajorFunction == IRP_MJ_WRITE)
			{
				offset = pIoStackLocation->Parameters.Write.ByteOffset;
				nLength = pIoStackLocation->Parameters.Write.Length;
			}
			else
			{
				offset.QuadPart = 0;
				nLength = 0;
			}

			if(pSysBuf == NULL || nLength == 0)
			{
				goto errornext;
			}

			if(pIoStackLocation->MajorFunction == IRP_MJ_READ)
			{
				long nResult = BitMapTest(pDeviceObjectExt->pBitMap, offset, nLength);
				switch(nResult)
				{
				case BITMAP_RANGE_CLEAR:
					goto errornext;
				case BITMAP_RANGE_SET:
					if((pFileBuf = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, nLength, 'xypd')) == NULL)
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						pIrp->IoStatus.Information = 0;
						goto error;
					}
					RtlZeroMemory(pFileBuf, nLength);
					status = ZwReadFile(pDeviceObjectExt->hTempFile, NULL, NULL, NULL,
						&ios, pFileBuf, nLength, &offset, NULL);
					if(NT_SUCCESS(status))
					{
						pIrp->IoStatus.Information = nLength;
						RtlCopyMemory(pSysBuf, pFileBuf, pIrp->IoStatus.Information);
						goto end;
					}
					else
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						pIrp->IoStatus.Information = 0;
						goto error;
					}
					break;
				case BITMAP_RANGE_BLEND:
					if((pFileBuf = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, nLength, 'xypd')) == NULL)
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						pIrp->IoStatus.Information = 0;
						goto error;
					}
					RtlZeroMemory(pFileBuf, nLength);
					status = ZwReadFile(pDeviceObjectExt->hTempFile, NULL, NULL, NULL, &ios,
						pFileBuf, nLength, &offset, NULL);
					if(!NT_SUCCESS(status))
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						pIrp->IoStatus.Information = 0;
						goto error;
					}
					status = ForwardIrpSync(pDeviceObjectExt->pLowerDeviceObject, pIrp);
					if(!NT_SUCCESS(status))
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						pIrp->IoStatus.Information = 0;
						goto error;					
					}
					memcpy(pDevBuf, pSysBuf, pIrp->IoStatus.Information);
					status = BitMapGet(pDeviceObjectExt->pBitMap, offset, nLength, pDevBuf, pFileBuf);
					if(!NT_SUCCESS(status))
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						pIrp->IoStatus.Information = 0;
						goto error;							
					}
					memcpy(pSysBuf, pDevBuf, pIrp->IoStatus.Information);
					goto end;
				default:
					status = STATUS_INSUFFICIENT_RESOURCES;
					goto error;
				}
			}
			else
			{
				status = ZwWriteFile(pDeviceObjectExt->hTempFile, NULL, NULL, NULL, &ios, pSysBuf, nLength,
					&offset, NULL);
				if(!NT_SUCCESS(status))
				{
					status = STATUS_INSUFFICIENT_RESOURCES;
					goto error;
				}
				else
				{
					status = BitMapSet(pDeviceObjectExt->pBitMap, offset, nLength);
					if(NT_SUCCESS(status))
					{
						goto end;
					}
					else
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						goto error;
					}
				}
			}
error:
			if(pFileBuf != NULL)
			{
				ExFreePool(pFileBuf);
				pFileBuf = NULL;
			}
			if(pDevBuf != NULL)
			{
				ExFreePool(pDevBuf);
				pDevBuf = NULL;
			}
			pIrp->IoStatus.Status = status;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			continue;
errornext:
			if(pFileBuf != NULL)
			{
				ExFreePool(pFileBuf);
				pFileBuf = NULL;
			}
			if(pDevBuf != NULL)
			{
				ExFreePool(pDevBuf);
				pDevBuf = NULL;
			}
			SendToNextDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
			continue;
end:
			if(pFileBuf != NULL)
			{
				ExFreePool(pFileBuf);
				pFileBuf = NULL;
			}
			if(pDevBuf != NULL)
			{
				ExFreePool(pDevBuf);
				pDevBuf = NULL;
			}
			pIrp->IoStatus.Status = STATUS_SUCCESS;
			IoCompleteRequest(pIrp, IO_DISK_INCREMENT);
			continue;
		}
	}
}

NTSTATUS DriverAddDevice(PDRIVER_OBJECT pDriverObject, PDEVICE_OBJECT pDeviceObject)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDeviceObjectExtension pDeviceObjectExt = NULL;
	PDEVICE_OBJECT pFilterDeviceObject = NULL;
	PDEVICE_OBJECT pLowerDeviceObject = NULL;
	HANDLE hThreadHandle = NULL;

	status = IoCreateDevice(pDriverObject, sizeof(PDeviceObjectExtension), NULL,
		FILE_DEVICE_DISK, FILE_DEVICE_SECURE_OPEN, FALSE, &pFilterDeviceObject);
	if(!NT_SUCCESS(status))
	{
		goto end;
	}

	pDeviceObjectExt =(PDeviceObjectExtension)pFilterDeviceObject->DeviceExtension;
	g_pDeviceObjectExt = pDeviceObjectExt;
	RtlZeroMemory(pDeviceObjectExt, sizeof(PDeviceObjectExtension));
	pLowerDeviceObject = IoAttachDeviceToDeviceStack(pFilterDeviceObject, pDeviceObject);
	if(pLowerDeviceObject == NULL)
	{
		status = STATUS_NO_SUCH_DEVICE;
		goto end;
	}

	KeInitializeEvent(&pDeviceObjectExt->eventPagingPathCountEvent,
		NotificationEvent, TRUE);
	pFilterDeviceObject->Flags = pLowerDeviceObject->Flags;
	pFilterDeviceObject->Flags |=  DO_POWER_PAGABLE;
	pDeviceObjectExt->pFilterDeviceObject = pFilterDeviceObject;
	pDeviceObjectExt->pLowerDeviceObject = pLowerDeviceObject;
	pDeviceObjectExt->pPhysicDeviceObject = pDeviceObject;

	InitializeListHead(&pDeviceObjectExt->lstReq);
	KeInitializeSpinLock(&pDeviceObjectExt->lckReq);

	KeInitializeEvent(&pDeviceObjectExt->eventReq, SynchronizationEvent, FALSE);

	pDeviceObjectExt->bThreadTermFlag = FALSE;

	status = PsCreateSystemThread(&hThreadHandle, (ACCESS_MASK)0, NULL, NULL, NULL,
		ReadWriteThread, pDeviceObjectExt);

	if(!NT_SUCCESS(status))
	{
		goto end;
	}

	status = ObReferenceObjectByHandle(hThreadHandle, THREAD_ALL_ACCESS, NULL, KernelMode, 
		&pDeviceObjectExt->pThreadHandle, NULL);

	if(!NT_SUCCESS(status))
	{
		pDeviceObjectExt->bThreadTermFlag = TRUE;
		KeSetEvent(&pDeviceObjectExt->eventReq, (KPRIORITY)0, FALSE);
		goto end;
	}
	
end:
	if(!NT_SUCCESS(status))
	{
		if(pLowerDeviceObject == NULL)
		{
			IoDetachDevice(pLowerDeviceObject);
			pDeviceObjectExt->pLowerDeviceObject = NULL;
		}

		if(pFilterDeviceObject != NULL)
		{
			IoDeleteDevice(pFilterDeviceObject);
			pDeviceObjectExt->pFilterDeviceObject = NULL;
		}
	}

	if(hThreadHandle != NULL)
		ZwClose(hThreadHandle);

	return status;
}

NTSTATUS DispatchPNP(PDEVICE_OBJECT pDeviceObjcet, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDeviceObjectExtension pDeviceObjectExt = (PDeviceObjectExtension)pDeviceObjcet->DeviceExtension;
	PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);

	switch (pIoStackLocation->MinorFunction)
	{
	case IRP_MN_REMOVE_DEVICE:
		{
			if(pDeviceObjectExt->bThreadTermFlag != TRUE && pDeviceObjectExt->pThreadHandle != NULL)
			{
				pDeviceObjectExt->bThreadTermFlag = TRUE;
				KeSetEvent(&pDeviceObjectExt->eventReq, (KPRIORITY)0, FALSE);
				KeWaitForSingleObject(pDeviceObjectExt->pThreadHandle, Executive, KernelMode,
					FALSE, NULL);
			}
			ObDereferenceObject(pDeviceObjectExt->pThreadHandle);
		
			if(pDeviceObjectExt->pBitMap != NULL)
			{
				BitMapFree(pDeviceObjectExt->pBitMap);
			}
			
			if(pDeviceObjectExt->pLowerDeviceObject != NULL)
			{
				IoDetachDevice(pDeviceObjectExt->pLowerDeviceObject);
			}
			if(pDeviceObjectExt->pFilterDeviceObject != NULL)
			{
				IoDeleteDevice(pDeviceObjectExt->pFilterDeviceObject);
			}
			break;
		}
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		{
			BOOLEAN bSetPagable;
			if(pIoStackLocation->Parameters.UsageNotification.Type != DeviceUsageTypePaging)
			{
				status = NULL;
				return status;
			}
			status = KeWaitForSingleObject(&pDeviceObjectExt->eventPagingPathCountEvent, Executive, KernelMode,
				FALSE, NULL);
			bSetPagable = FALSE;
			if(!pIoStackLocation->Parameters.UsageNotification.InPath && pDeviceObjectExt->nPagingPathCount == 1)
			{
				if(pDeviceObjcet->Flags & DO_POWER_INRUSH)
				{}
				else
				{
					pDeviceObjcet->Flags |= DO_POWER_PAGABLE;
					bSetPagable = TRUE;
				}

				status = NULL;
				if(NT_SUCCESS(status))
				{
					IoAdjustPagingPathCount(&pDeviceObjectExt->nPagingPathCount, 
						pIoStackLocation->Parameters.UsageNotification.InPath);
					if(pIoStackLocation->Parameters.UsageNotification.InPath)
					{
						if(pDeviceObjectExt->nPagingPathCount == 1)
						{
							pDeviceObjcet->Flags &= ~DO_POWER_PAGABLE;
						}
					}
				}
				else
				{
					if(bSetPagable == TRUE)
					{
						pDeviceObjcet->Flags &= ~DO_POWER_PAGABLE;
						bSetPagable = FALSE;
					}
				}
				KeSetEvent(&pDeviceObjectExt->eventPagingPathCountEvent, IO_NO_INCREMENT, FALSE);
				IoCompleteRequest(pIrp, IO_NO_INCREMENT);
				return status;
			}
		}
	}

	return status;
}

NTSTATUS DispatchControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDeviceObjectExtension pDeviceObjectExt = (PDeviceObjectExtension)pDeviceObject->DeviceExtension;
	PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);
	KEVENT event;
	VolumeOnlineContext volumeOnlineContext;

	switch (pIoStackLocation->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_VOLUME_ONLINE:
		{
			KeInitializeEvent(&event, NotificationEvent, FALSE);
			volumeOnlineContext.pDeviceObjectExt = pDeviceObjectExt;
			volumeOnlineContext.pEvent = &event;
			IoCopyCurrentIrpStackLocationToNext(pIrp);
			IoSetCompletionRoutine(pIrp, VolumeOnlieCompletionRoutine, &volumeOnlineContext,
				TRUE, TRUE, TRUE);
			status = IoCallDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
			KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
			return status;
		}
	default:
		break;
	}
	status = SendToNextDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
	return status;
}

NTSTATUS DispatchPower(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	PDeviceObjectExtension pDeviceObjectExt = (PDeviceObjectExtension)pDeviceObject->DeviceExtension;
	return SendToNextDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
}

NTSTATUS DispatchReadWrite(PDEVICE_OBJECT pDeviceObjec, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDeviceObjectExtension pDeviceObjectExt = (PDeviceObjectExtension)pDeviceObjec->DeviceExtension;
	if(pDeviceObjectExt->bProtect)
	{
		IoMarkIrpPending(pIrp);
		ExInterlockedInsertTailList(&pDeviceObjectExt->lstReq, &pIrp->Tail.Overlay.ListEntry,
			&pDeviceObjectExt->lckReq);
		KeSetEvent(&pDeviceObjectExt->eventReq, 0, FALSE);
		return STATUS_PENDING;
	}
	else
	{
		return SendToNextDriver(pDeviceObjectExt->pLowerDeviceObject, pIrp);
	}
}



