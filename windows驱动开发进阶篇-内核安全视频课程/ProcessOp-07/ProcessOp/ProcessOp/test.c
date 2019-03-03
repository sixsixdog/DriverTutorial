#include <ntifs.h>
#include <ntddk.h>
#include <windef.h>

typedef struct _PEB_LDR_DATA {
	BYTE Reserved1[8];
	PVOID Reserved2[3];
	LIST_ENTRY InMemoryOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _LDR_DATA_TABLE_ENTRY {
	PVOID Reserved1[2];
	LIST_ENTRY InMemoryOrderLinks;
	PVOID Reserved2[2];
	PVOID DllBase;
	PVOID Reserved3[2];
	UNICODE_STRING FullDllName;
	BYTE Reserved4[8];
	PVOID Reserved5[3];
	union {
		ULONG CheckSum;
		PVOID Reserved6;
	} DUMMYUNIONNAME;
	ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;



typedef struct _PEB {
	BYTE Reserved1[2];
	BYTE BeingDebugged;
	BYTE Reserved2[1];
	PVOID Reserved3[2];
	PPEB_LDR_DATA Ldr;
	PVOID ProcessParameters;
	PVOID Reserved4[3];
	PVOID AtlThunkSListPtr;
	PVOID Reserved5;
	ULONG Reserved6;
	PVOID Reserved7;
	ULONG Reserved8;
	ULONG AtlThunkSListPtr32;
	PVOID Reserved9[45];
	BYTE Reserved10[96];
} PEB, *PPEB;

NTSTATUS ZwOpenThread(
	_Out_  PHANDLE ThreadHandle,
	_In_   ACCESS_MASK DesiredAccess,
	_In_   POBJECT_ATTRIBUTES ObjectAttributes,
	_In_   PCLIENT_ID ClientId);
PCHAR PsGetProcessImageFileName(IN PEPROCESS Process);
HANDLE PsGetProcessInheritedFromUniqueProcessId(IN PEPROCESS Process);
PPEB PsGetProcessPeb(PEPROCESS Process);
NTSTATUS PsSuspendProcess(PEPROCESS Process);
NTSTATUS PsResumeProcess(PEPROCESS Process);
VOID EnumModule(PEPROCESS Process);
typedef NTSTATUS (*pfnPsSuspendProcess)(PEPROCESS Process);
typedef NTSTATUS(*pfnPsResumeProcess)(PEPROCESS Process);
typedef NTSTATUS(/*__fastcall */*ZWTERMINATETHREAD)(
	HANDLE hThread,
	ULONG uExitCode);
pfnPsSuspendProcess xppfnPsSuspendProcess = (pfnPsSuspendProcess)0x805d585e;
pfnPsResumeProcess xppfnPsResumeProcess = (pfnPsResumeProcess)0x805d5710;
//0xfffff80003ec2640
ZWTERMINATETHREAD pfnZwTerminateThread = (ZWTERMINATETHREAD)0x83e4c450;
VOID ResumeProcess(HANDLE ProcessId);
KEVENT Event;
PETHREAD Thread;
VOID Unload(IN PDRIVER_OBJECT DriverObject)
{
	//ResumeProcess((HANDLE)1036);
	NTSTATUS status;
	KeSetEvent(&Event, IO_NO_INCREMENT, FALSE);
	status = KeWaitForSingleObject(Thread, Executive, KernelMode, FALSE, NULL);
	if (NT_SUCCESS(status))
	{
		KdPrint(("线程已退出\n"));
	}
	KdPrint(("驱动卸载\n"));
}

VOID EnumThread(PEPROCESS Process)
{
	PETHREAD Thread =  NULL;
	NTSTATUS status;
	for (int i = 4; i < 100000;i+=4)
	{
		status  = PsLookupThreadByThreadId((HANDLE)i, &Thread);
		if (NT_SUCCESS(status))
		{
			if (IoThreadToProcess(Thread) == Process)
			{
				KdPrint(("%s\t%d:%d\n", PsGetProcessImageFileName(Process), PsGetProcessId(Process), i));
			}
			
		}


	}


	

}


VOID EnumProcess()
{
	ULONG i = 0;
	PEPROCESS Process;
	NTSTATUS status;
	for (i = 4; i < 20000; i += 4)
	{
		status = PsLookupProcessByProcessId((HANDLE)i, &Process);
		if (NT_SUCCESS(status))
		{
			KdPrint(("parentid:%d\t%d\t%s\n", PsGetProcessInheritedFromUniqueProcessId(Process), i, PsGetProcessImageFileName(Process)));
			EnumThread(Process);
			EnumModule(Process);
		
		}
	}

}

VOID SuspendProcess(HANDLE ProcessId)
{
	PEPROCESS Process;
	NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &Process);
	if (NT_SUCCESS(status))
	{
		if (SharedUserData->NtMajorVersion == 5)
		{
			xppfnPsSuspendProcess(Process);
		}
// 		else
// 		{
// 			PsSuspendProcess(Process);
// 		}
		
	}


}

VOID ResumeProcess(HANDLE ProcessId)
{
	PEPROCESS Process;
	NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &Process);
	if (NT_SUCCESS(status))
	{
		if (SharedUserData->NtMajorVersion == 5)
		{
			xppfnPsResumeProcess(Process);
		}
// 		else
// 		{
// 			PsResumeProcess(Process);
// 		}
		
	}


}


VOID MyTerminateProcess(HANDLE ProcessId)
{
	CLIENT_ID Client = { 0 };
	HANDLE hProcess = NULL;
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa;

	Client.UniqueProcess = ProcessId;
	InitializeObjectAttributes(&oa, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
	status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &oa, &Client);
	if (NT_SUCCESS(status))
	{
		status = ZwTerminateProcess(hProcess, 0);
		if (NT_SUCCESS(status))
		{
			KdPrint(("进程终止\n"));
		}
	}
	

}


VOID MyTerminateThread(HANDLE ThreadId)
{
	HANDLE hThread;
	OBJECT_ATTRIBUTES oa;
	CLIENT_ID ClientId = { NULL };

	KdBreakPoint();
	InitializeObjectAttributes(&oa, NULL, 0, NULL, NULL);
// 	oa.Length = sizeof(oa);
// 	oa.RootDirectory = 0;
// 	oa.ObjectName = 0;
// 	oa.Attributes = 0;
// 	oa.SecurityDescriptor = 0;
// 	oa.SecurityQualityOfService = 0;
	ClientId.UniqueThread = ThreadId;
	NTSTATUS status = ZwOpenThread(&hThread, THREAD_TERMINATE, &oa, &ClientId);
	if (NT_SUCCESS(status) ||
		hThread != NULL)
	{
		KdPrint(("线程序打开成功\n"));
		status = pfnZwTerminateThread(hThread, 0);
		if (NT_SUCCESS(status))
		{
			KdPrint(("线程终止成功\n"));
		}
		else
		{
			KdPrint(("线程终止失败%x\n",status));
		}
	}
	else
	{
		KdPrint(("线程打开失败\n"));
	}



}

VOID ThreadProc(PVOID Context)
{
	NTSTATUS status;
	LARGE_INTEGER timeout = RtlConvertLongToLargeInteger(-10 * 1000 * 1000);
	Thread = PsGetCurrentThread();
	while (TRUE)
	{
		KdPrint(("线程运行\n"));
		status = KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, &timeout);
		if (status==STATUS_TIMEOUT)
		{
			continue;

		}
		else
		{
			KdPrint(("驱动卸载事件\n"));
			break;
		}
	}
	PsTerminateSystemThread(0);
}


VOID EnumModule(PEPROCESS Process)
{
	PLIST_ENTRY ListHeader;
	PLIST_ENTRY CurrentListEntry;
	PLDR_DATA_TABLE_ENTRY LdrDataTable;
	KAPC_STATE ApcState;
	PPEB peb = PsGetProcessPeb(Process);
	if (peb == NULL)
	{
		return;
	}

	KeStackAttachProcess(Process, &ApcState);

	ListHeader = &peb->Ldr->InMemoryOrderModuleList;

	CurrentListEntry = ListHeader->Flink;
	while (CurrentListEntry != ListHeader)
	{
		LdrDataTable = CONTAINING_RECORD(CurrentListEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
		KdPrint(("Base:%p\t%wZ\n",LdrDataTable->DllBase, &LdrDataTable->FullDllName));
		CurrentListEntry = CurrentListEntry->Flink;
	}


	KeUnstackDetachProcess(&ApcState);
}


NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	HANDLE hThread;
	KdPrint(("驱动加载\n"));
	DriverObject->DriverUnload = Unload;
	KeInitializeEvent(&Event, SynchronizationEvent, FALSE);
	EnumProcess();
// 	//SuspendProcess((HANDLE)1036);
// 	MyTerminateProcess((HANDLE)1608);

//	MyTerminateThread((HANDLE)3008);
	
	PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, ThreadProc, NULL);

	return STATUS_SUCCESS;
}
