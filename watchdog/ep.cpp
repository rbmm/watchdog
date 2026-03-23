#include "stdafx.h"

_NT_BEGIN

EXTERN_C_START
#include "../inc/nttp.h"
EXTERN_C_END

void WaitForProcess(HANDLE hProcess, BOOL bMain);

void StartProcess(BOOL bMain)
{
	if (PWSTR lpApplicationName = new WCHAR[MINSHORT])
	{
		GetModuleFileNameW(0, lpApplicationName, MINSHORT);

		if (!GetLastError())
		{
			HANDLE hProcess;
			STARTUPINFOEXW si = { { sizeof(si) } };

			SIZE_T s = 0;
			ULONG dwError;
			while (ERROR_INSUFFICIENT_BUFFER == (dwError = InitializeProcThreadAttributeList(
				si.lpAttributeList, 1, 0, &s) ? NOERROR : GetLastError()))
			{
				si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)alloca(s);
			}

			if (NOERROR == dwError && UpdateProcThreadAttribute(si.lpAttributeList, 0,
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
		}
		delete[] lpApplicationName;
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

	MessageBoxW(0, 0, bMain ? L"MainApp" : L"Watchdog", bMain ? MB_ICONINFORMATION : MB_ICONWARNING);
	ExitProcess(0);
}

_NT_END