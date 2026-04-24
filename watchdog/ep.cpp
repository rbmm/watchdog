#include "stdafx.h"

_NT_BEGIN

EXTERN_C_START
#include "../inc/nttp.h"
EXTERN_C_END

void WaitForProcess(HANDLE hProcess, BOOL bMain);

void StartProcess(BOOL bMain)
{
	STATIC_WSTRING(GLOBALROOT, "\\\\.\\Global\\GLOBALROOT");
	NTSTATUS status = STATUS_NO_MEMORY;
	if (PVOID buf = LocalAlloc(LMEM_FIXED, 0x10000))
	{
		enum {
			ecb = (sizeof(GLOBALROOT) - sizeof(WCHAR) - sizeof(UNICODE_STRING) +
				__alignof(UNICODE_STRING) - 1) & ~(__alignof(UNICODE_STRING) - 1)
		};
		SIZE_T s;
		PUNICODE_STRING ImageName = (PUNICODE_STRING)RtlOffsetToPointer(buf, ecb);
		if (0 <= (status = NtQueryVirtualMemory(NtCurrentProcess(), &__ImageBase,
			(MEMORY_INFORMATION_CLASS)MemoryMappedFilenameInformation,
			ImageName, 0x10000 - ecb - sizeof(WCHAR), &s)))
		{
			*(WCHAR*)RtlOffsetToPointer(ImageName->Buffer, ImageName->Length) = 0;
			PWSTR lpApplicationName = (PWSTR)((PBYTE)ImageName->Buffer - sizeof(GLOBALROOT) + sizeof(WCHAR));
			memcpy(lpApplicationName, GLOBALROOT, sizeof(GLOBALROOT) - sizeof(WCHAR));

			STARTUPINFOEXW si = { { sizeof(si) } };
			si.StartupInfo.dwFlags = STARTF_FORCEOFFFEEDBACK;

			s = 0;
			while (ERROR_INSUFFICIENT_BUFFER == (status = InitializeProcThreadAttributeList(
				si.lpAttributeList, 1, 0, &s) ? NOERROR : GetLastError()))
			{
				if (si.lpAttributeList)
				{
					break;
				}
				si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)alloca(s);
			}

			if (si.lpAttributeList && NOERROR == status)
			{
				HANDLE hProcess;
				if (UpdateProcThreadAttribute(si.lpAttributeList, 0, 
					PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &hProcess, sizeof(hProcess), 0, 0) &&
					DuplicateHandle(NtCurrentProcess(), NtCurrentProcess(), NtCurrentProcess(),
						&hProcess, 0, TRUE, DUPLICATE_SAME_ACCESS))
				{
					PWSTR lpCommandLine = 0;
					int len = 0;

					while (0 < (len = _snwprintf(lpCommandLine, len, L"\n%p\r*%x*", hProcess, bMain)))
					{
						if (lpCommandLine)
						{
							PROCESS_INFORMATION pi;
							if (CreateProcessW(lpApplicationName, lpCommandLine, 0, 0, TRUE,
								EXTENDED_STARTUPINFO_PRESENT, 0, 0, &si.StartupInfo, &pi))
							{
								NtClose(pi.hThread);
								WaitForProcess(pi.hProcess, bMain);
								NtClose(pi.hProcess);
							}
							break;
						}

						lpCommandLine = (PWSTR)alloca(++len * sizeof(WCHAR));
					}

					NtClose(hProcess);
				}
				DeleteProcThreadAttributeList(si.lpAttributeList);
			}
		}
		LocalFree(buf);
	}
}

VOID
NTAPI
wcb(
	_Inout_     PTP_CALLBACK_INSTANCE /*Instance*/,
	_Inout_opt_ PVOID                 Context,
	_Inout_     PTP_WAIT              Wait,
	_In_        TP_WAIT_RESULT        /*WaitResult*/
)
{
	TpReleaseWait(Wait);
	StartProcess((BOOL)(ULONG_PTR)Context);
}

void WaitForProcess(HANDLE hProcess, BOOL bMain)
{
	PTP_WAIT Wait;
	if (0 <= TpAllocWait(&Wait, wcb, (PVOID)(ULONG_PTR)bMain, 0))
	{
		TpSetWait(Wait, hProcess, 0);
	}
}

void WINAPI ep(void*)
{
	BOOL bMain = TRUE;
	if (PWSTR psz = wcschr(GetCommandLineW(), '*'))
	{
		ULONG b = wcstoul(psz + 1, &psz, 16);
		if ('*' == *psz)
		{
			bMain = b;
		}
	}

	if (PWSTR psz = wcschr(GetCommandLineW(), '\n'))
	{
		HANDLE hProcess = (HANDLE)(ULONG_PTR)_wcstoui64(psz + 1, &psz, 16);
		if ('\r' == *psz)
		{
			WaitForProcess(hProcess, !bMain);
			NtClose(hProcess);
		}
	}
	else
	{
		StartProcess(!bMain);
	}

	if (bMain)
	{
		MessageBoxW(0, 0, L"Kill Me", MB_ICONINFORMATION);
	}
	else
	{
		Sleep(INFINITE);
	}

	ExitProcess(0);
}

_NT_END