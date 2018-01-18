#include "ntddk.h"

#pragma pack(1)
typedef struct ServiceDescriptorEntry {
	unsigned int *ServiceTableBase;
	unsigned int *ServiceCounterTableBase;
	unsigned int NumberOfServices;
	unsigned char *ParamTableBase;
} ServiceDescriptorTableEntry_t, *PServiceDescriptorTableEntry_t;
#pragma pack()

NTSTATUS
PsLookupProcessByProcessId(
	IN HANDLE ProcessId,
	OUT PEPROCESS *Process
);
__declspec(dllimport) ServiceDescriptorTableEntry_t KeServiceDescriptorTable;

typedef NTSTATUS(*MYNTOPENPROCESS)(
	OUT PHANDLE             ProcessHandle,
	IN ACCESS_MASK          AccessMask,
	IN POBJECT_ATTRIBUTES   ObjectAttributes,
	IN PCLIENT_ID           ClientId);//����һ��ָ�뺯�������������O_NtOpenProcess����ǿ��ת��
ULONG O_NtOpenProcess;

BOOLEAN ProtectProcess(HANDLE ProcessId, char *str_ProtectObjName)
{
	NTSTATUS status;
	PEPROCESS process_obj;
	if (!MmIsAddressValid(str_ProtectObjName))//��������������ж�Ŀ��������Ƿ���Ч
	{
		return FALSE;
	}
	if (ProcessId == 0)//��������������ų�System Idle Process���̵ĸ���
	{
		return FALSE;
	}
	status = PsLookupProcessByProcessId(ProcessId, &process_obj);//���������ȡĿ����̵�EPROCESS�ṹ
	if (!NT_SUCCESS(status))
	{
		KdPrint(("�Ҵ��ˣ�����Ǵ����:%X---����ǽ���ID:%d", status, ProcessId));
		return FALSE;
	}
	if (!strcmp((char *)process_obj + 0x174, str_ProtectObjName))//���бȽ�
	{
		ObDereferenceObject(process_obj);//�����������1��Ϊ�˻ָ�������������������ڻ���
		return TRUE;
	}
	ObDereferenceObject(process_obj);
	return FALSE;
}

NTSTATUS MyNtOpenProcess(
	__out PHANDLE ProcessHandle,
	__in ACCESS_MASK DesiredAccess,
	__in POBJECT_ATTRIBUTES ObjectAttributes,
	__in_opt PCLIENT_ID ClientId
)
{
	KdPrint(("OpenProcess�Ľ���ID�ǣ�%lu\n",(ULONG)ClientId->UniqueProcess));
	return STATUS_UNSUCCESSFUL;

	//KdPrint(("%s",(char *)PsGetCurrentProcess()+0x174));
	if (ProtectProcess(ClientId->UniqueProcess, "calc.exe"))
	{
		KdPrint(("%s������𣿲����ܡ���������", (char *)PsGetCurrentProcess() + 0x174));
		return STATUS_UNSUCCESSFUL;
	}
	//KdPrint(("Hook Success!"));
	return ((MYNTOPENPROCESS)O_NtOpenProcess)(ProcessHandle,//�������Լ�������󣬵���ԭ���ĺ�����������������������
		DesiredAccess,
		ObjectAttributes,
		ClientId);
}

UINT32 ServiceFunctionID;

void PageProtectOff()//�ر�ҳ�汣��
{
	__asm {
		cli
		mov  eax, cr0
		and  eax, not 10000h
		mov  cr0, eax
	}
}

void PageProtectOn()//��ҳ�汣��
{
	__asm {
		mov  eax, cr0
		or eax, 10000h
		mov  cr0, eax
		sti
	}
}

void UnHookSsdt()
{
	PageProtectOff();
	KeServiceDescriptorTable.ServiceTableBase[ServiceFunctionID] = O_NtOpenProcess;//�ָ�ssdt��ԭ���ĺ�����ַ
	PageProtectOn();
}

NTSTATUS ssdt_hook()
{
	//int i;
	//for(i=0;i<KeServiceDescriptorTable.NumberOfServices;i++)
	//{
	//  KdPrint(("NumberOfService[%d]-------%x",i,KeServiceDescriptorTable.ServiceTableBase[i]));
	//}

	UNICODE_STRING ServiceName = RTL_CONSTANT_STRING(L"NtOpenProcess");

	PVOID  ServiceFunction = MmGetSystemRoutineAddress(&ServiceName);

	for (UINT32 i = 0; TRUE; ++i)
	{
		if ((UINT32)ServiceFunction == KeServiceDescriptorTable.ServiceTableBase[i])
		{
			ServiceFunctionID = i;
			break;
		}
	}

	O_NtOpenProcess = KeServiceDescriptorTable.ServiceTableBase[ServiceFunctionID];//����ԭ���ĺ�����ַ
	PageProtectOff();
	//��ԭ��ssdt����Ҫhook�ĺ�����ַ���������Լ��ĺ�����ַ
	KeServiceDescriptorTable.ServiceTableBase[ServiceFunctionID] = (unsigned int)MyNtOpenProcess;
	PageProtectOn();
	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	UnHookSsdt();
	KdPrint(("Driver Unload Success !"));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegsiterPath)
{
	DbgPrint("This is My First Driver!");
	ssdt_hook();
	pDriverObject->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}