#include <windows.h>
#include <wdbgexts.h>
#include <wct.h>
#include <tlhelp32.h>
#include <Psapi.h>

#pragma comment (lib, "psapi.lib")

LPCSTR STR_OBJECT_TYPE[] =
{
    "CriticalSection",
    "SendMessage",
    "Mutex",
    "Alpc",
    "Com",
    "ThreadWait",
	"ProcWait",
    "Thread",
    "ComActivation",
	"Unknown",
    "Max"
};

HMODULE g_hOle32;

BOOL InitCOMAccess()
{
    PCOGETCALLSTATE       CallStateCallback;
    PCOGETACTIVATIONSTATE ActivationStateCallback;

    g_hOle32 = LoadLibrary(L"ole32.dll");
    if (!g_hOle32)
    {
        dprintf("ERROR: GetModuleHandle for ole32.dll failed: 0x%x\n", GetLastError());
        return FALSE;
    }

    CallStateCallback = (PCOGETCALLSTATE)GetProcAddress(g_hOle32, "CoGetCallState");
    if (!CallStateCallback)
    {
        dprintf("ERROR: GetProcAddress for CoGetCallState failed: 0x%x\n", GetLastError());
        return FALSE;
    }

    ActivationStateCallback = (PCOGETACTIVATIONSTATE)GetProcAddress(g_hOle32, "CoGetActivationState");
    if (!ActivationStateCallback)
    {
        dprintf("ERROR: GetProcAddress for CoGetActivationState failed: 0x%x\n", GetLastError());
        return FALSE;
    }

    RegisterWaitChainCOMCallback(CallStateCallback,
                                 ActivationStateCallback);
    return TRUE;
}

EXT_API_VERSION g_ExtApiVersion = {
         5 ,
         5 ,
         EXT_API_VERSION_NUMBER ,
         0
     } ;
WINDBG_EXTENSION_APIS ExtensionApis = {0};
USHORT SavedMajorVersion = 0xF;
USHORT SavedMinorVersion = 0;

LPEXT_API_VERSION __declspec(dllexport) /*WDBGAPI*/ ExtensionApiVersion (void)
{
    return &g_ExtApiVersion;
}

VOID __declspec(dllexport) /*WDBGAPI*/ WinDbgExtensionDllInit (PWINDBG_EXTENSION_APIS 
           lpExtensionApis, USHORT usMajorVersion, 
           USHORT usMinorVersion)
{
     ExtensionApis = *lpExtensionApis;
	 if (FALSE == InitCOMAccess())
	 {
		 dprintf("Warning: COM call information will not be available\n");
	 }
	 //Note: We're leaking the global module handle for ole32.dll here
}

__declspec(dllexport) DECLARE_API (help)
{
    dprintf("\nWait Chain Traversal (WCT) debugging extension.\n");
	dprintf("    By Sasha Goldshtein, tinyurl.com/sasha.\n\n");
    dprintf("This debugging extension displays WCT information.\n");
	dprintf("Syntax:\n");
	dprintf("!wct\t\tDisplays WCT information for the entire system.\n");
	dprintf("!wct_proc [<ProcessID>]\t\tDisplays WCT information for the specified process ID or for the current process.\n");
	dprintf("!wct_thread [<ThreadID>]\t\tDisplays WCT information for the specified thread ID or for the current thread.\n\n");
}

__declspec(dllexport) DECLARE_API (wct_thread)
{
	HWCT hwct = NULL;
	BOOL isCycle = FALSE;
	DWORD threadID;
	DWORD processID;
	WAITCHAIN_NODE_INFO chain[WCT_MAX_NODE_COUNT];
	DWORD nodeCount = ARRAYSIZE(chain);
	DWORD i;
	HANDLE hThread;
	BOOL haveArgument = FALSE;
	DWORD exitCode;

	if (strlen(args) > 0)
	{
		haveArgument = TRUE;
		threadID = atoi(args);
		hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadID);
		if (hThread == NULL)
		{
			dprintf("Error opening thread %d: 0x%x\n", threadID, GetLastError());
			goto Cleanup;
		}
	}
	else
	{
		haveArgument = FALSE;
		hThread = hCurrentThread;
		threadID = GetThreadId(hThread);
	}
	if (FALSE == GetExitCodeThread(hThread, &exitCode))
	{
		dprintf("Error retrieving exit code for thread %d: 0x%x\n", threadID, GetLastError());
		goto Cleanup;
	}
	if (exitCode != STILL_ACTIVE)
	{
		dprintf("Thread %d is no longer running\n", threadID);
		goto Cleanup;
	}

	processID = GetProcessIdOfThread(hThread);

	hwct = OpenThreadWaitChainSession(0, NULL);
	if (hwct == NULL)
	{
		dprintf("Error retrieving wait chain for thread %d: 0x%x\n", threadID, GetLastError());
		goto Cleanup;
	}

	if (FALSE == GetThreadWaitChain(
		hwct,
		(DWORD_PTR)NULL,
		WCTP_GETINFO_ALL_FLAGS,
		threadID,
		&nodeCount,
		chain,
		&isCycle))
	{
		dprintf("Error retrieving wait chain for thread %d: 0x%x\n", threadID, GetLastError());
		goto Cleanup;
	}

	dprintf(">>> Begin wait chain for thread %d:%d with %d nodes\n", processID, threadID, nodeCount);
	for (i = 0; i < nodeCount; ++i)
	{
		WAITCHAIN_NODE_INFO* curr = &chain[i];
		switch (curr->ObjectType)
		{
		case WctThreadType:
			dprintf("    Thread [%d:%d:%s WT=%d]",
				curr->ThreadObject.ProcessId,
				curr->ThreadObject.ThreadId,
				curr->ObjectStatus == WctStatusBlocked ? "blocked" : "running",
				curr->ThreadObject.WaitTime);
			break;
		case WctCriticalSectionType:
		case WctSendMessageType:
		case WctMutexType:
		case WctAlpcType:
		case WctComType:
		case WctThreadWaitType:
		case WctProcessWaitType:
		case WctComActivationType:
		case WctUnknownType:
		case WctSocketIoType:
		case WctSmbIoType:
			if (curr->LockObject.ObjectName[0] != L'\0')
			{
				dprintf("    %s [%S]",
					STR_OBJECT_TYPE[curr->ObjectType-1],
					curr->LockObject.ObjectName);
			}
			else
			{
				dprintf("    %s [-]",
					STR_OBJECT_TYPE[curr->ObjectType-1]);
			}
			if (curr->ObjectStatus == WctStatusAbandoned)
			{
				dprintf("<abandoned>");
			}
			break;
		default:
			break;
		}
		dprintf(" ==>\n");
	}
	if (TRUE == isCycle)
	{
		dprintf("DEADLOCK FOUND\n");
	}
	dprintf(">>> End wait chain for thread %d:%d\n", processID, threadID);

Cleanup:
	if (hwct != NULL)
		CloseThreadWaitChainSession(hwct);
	if (TRUE == haveArgument)
		CloseHandle(hThread);
}

__declspec(dllexport) DECLARE_API (wct_proc)
{
	DWORD processID;
	HANDLE hProcess;
	BOOL haveArgument = FALSE;
	HANDLE snapshot = NULL;
	THREADENTRY32 thread;
	CHAR threadArgs[10];
	WCHAR exePath[MAX_PATH];
	DWORD exePathSize;

	if (strlen(args) > 0)
	{
		haveArgument = TRUE;
		processID = atoi(args);
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processID);
		if (hProcess == NULL)
		{
			dprintf("Error opening process %d: 0x%x\n", processID, GetLastError());
			goto Cleanup;
		}
	}
	else
	{
		haveArgument = FALSE;
		hProcess = hCurrentProcess;
		processID = GetProcessId(hProcess);
	}

	exePathSize = ARRAYSIZE(exePath);
	if (TRUE == QueryFullProcessImageName(hProcess, 0, exePath, &exePathSize))
	{
		dprintf("*** Begin thread wait chain information for process %S [%d]\n", exePath, processID);
	}
	else
	{
		dprintf("*** Begin thread wait chain information for process %d\n", processID);
	}

	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, processID);
	if (snapshot == NULL)
	{
		dprintf("Error enumerating threads in process %d: 0x%x\n", processID, GetLastError());
		goto Cleanup;
	}
	thread.dwSize = sizeof(thread);
	if (Thread32First(snapshot, &thread))
	{
		do
		{
			if (thread.th32OwnerProcessID != processID)
				continue;

			_itoa_s(thread.th32ThreadID, threadArgs, ARRAYSIZE(threadArgs), 10);
			wct_thread(hProcess, NULL, 0, 0, threadArgs);
		}
		while (Thread32Next(snapshot, &thread));
	}

	dprintf("*** End thread wait chain information for process %d\n", processID);

Cleanup:
	if (TRUE == haveArgument)
		CloseHandle(hProcess);
	if (snapshot != NULL)
		CloseHandle(snapshot);
}

__declspec(dllexport) DECLARE_API (wct)
{
	DWORD processes[4096];
	DWORD bytesReturned;
	DWORD i;
	CHAR procArgs[10];
	
	if (FALSE == EnumProcesses(processes, sizeof(processes), &bytesReturned))
	{
		dprintf("Error enumerating processes: 0x%x\n", GetLastError());
		goto Cleanup;
	}

	for (i = 0; i < bytesReturned / sizeof(DWORD); ++i)
	{
		_itoa_s(processes[i], procArgs, ARRAYSIZE(procArgs), 10);
		wct_proc(NULL, NULL, 0, 0, procArgs);
	}

Cleanup: ;
}