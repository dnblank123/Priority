// ProcessList.cpp: implementation of the CProcessList class.
//
//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>

#include "ProcessList.h"
#include <tchar.h>
#include <atlstr.h>
#include <TlHelp32.h>
#include <chrono>
#include <thread>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CProcessList::CProcessList(DWORD dwProcessID)
{
	m_dwProcessID = dwProcessID;
	ZeroMemory(m_szExeName, sizeof(m_szExeName));
}

CProcessList::~CProcessList()
{

}

BOOL CProcessList::OnProcess(LPCTSTR lpszFileName, DWORD ProcessID)
{
	WCHAR szBuffer[1024] = L"";

	if (ProcessID < 10)
		return TRUE;


	WCHAR lpszWorkingDir[MAX_PATH];
	GetCurrentDirectory(sizeof(lpszWorkingDir), lpszWorkingDir);

	WCHAR lpszModuleName[MAX_PATH];

	lstrcpy(lpszModuleName, lpszFileName);

	LPWSTR lpszPtr = lpszModuleName + lstrlen(lpszModuleName) - 1;
	while ((lpszPtr != lpszModuleName) && (*lpszPtr != '.'))
	{
		*lpszPtr = 0;
		lpszPtr--;
	}

	if (*lpszPtr == '.')
		*lpszPtr = 0;

	WCHAR * ptr = wcsrchr(lpszModuleName, '\\');
	if (ptr != NULL) {
		wcscpy_s(lpszModuleName, MAX_PATH, ptr + 1);
	}

	CString dir(lpszWorkingDir);

	dir += L"\\priority.ini";

#ifdef _DEBUG
	//_tprintf(TEXT("PID=%u > Filename = %s | ModuleName = %s\n"), ProcessID, LPCWSTR(lpszFileName), LPCWSTR(lpszModuleName));
	_tprintf(TEXT("INI File: %s\n"), LPCWSTR(dir));
#endif

	GetPrivateProfileString(lpszModuleName, L"priority", L"", szBuffer, sizeof(szBuffer), dir);
#ifdef _DEBUG
	if (lstrlen(szBuffer) > 0)
	{
		_tprintf(TEXT("PID=%u > Process: %s | New priority: %s\n"), ProcessID, LPCWSTR(lpszModuleName), LPCWSTR(szBuffer));
	}
#endif
	if (_wcsnicmp(szBuffer, L"NORMAL", 6) == 0)
	{
		SetPriority(ProcessID, NORMAL_PRIORITY_CLASS);
		SetAllThreadPriority(ProcessID, THREAD_PRIORITY_NORMAL);
	}
	else if (_wcsnicmp(szBuffer, L"BELOW", 5) == 0)
	{
		SetPriority(ProcessID, BELOW_NORMAL_PRIORITY_CLASS);
		SetAllThreadPriority(ProcessID, THREAD_PRIORITY_BELOW_NORMAL);
	}
	else if (_wcsnicmp(szBuffer, L"LOW", 3) == 0)
	{
		SetPriority(ProcessID, IDLE_PRIORITY_CLASS);
		SetAllThreadPriority(ProcessID, THREAD_PRIORITY_IDLE);
	}
	else if (_wcsnicmp(szBuffer, L"ABOVE", 5) == 0)
	{
		SetPriority(ProcessID, ABOVE_NORMAL_PRIORITY_CLASS);
		SetAllThreadPriority(ProcessID, THREAD_PRIORITY_ABOVE_NORMAL);
	}
	else if (_wcsnicmp(szBuffer, L"HIGH", 4) == 0)
	{
		SetPriority(ProcessID, HIGH_PRIORITY_CLASS);
		SetAllThreadPriority(ProcessID, THREAD_PRIORITY_HIGHEST);
	}
	else if (_wcsnicmp(szBuffer, L"REALTIME", 8) == 0)
	{
		SetPriority(ProcessID, REALTIME_PRIORITY_CLASS);
		SetAllThreadPriority(ProcessID, THREAD_PRIORITY_TIME_CRITICAL);
	}
	else if (_wcsnicmp(szBuffer, L"OFF", 3) == 0)
	{
		HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, ProcessID);
		if (hProcess != NULL)
		{
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
		}
	}

	return TRUE;
}

void CProcessList::SetPriority(DWORD dwID, DWORD dwPriority)
{
	HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, dwID);

	if (hProcess)
	{
#ifdef _DEBUG
		_tprintf(TEXT("PID=%u > set priority to %u\n"), dwID, dwPriority);
#endif
		BOOL bRet = SetPriorityClass(hProcess, dwPriority);

#ifdef _DEBUG
		if (!bRet) {
			_tprintf(TEXT("/!\\ Can't adjust priority, LastError: %u\n"), GetLastError());
		}
#endif


		CloseHandle(hProcess);
	}
	else
	{
#ifdef _DEBUG
		_tprintf(TEXT("Can't OpenProcess PID=%u, LastError = %u\n"), dwID, GetLastError());
#endif

	}
}
void CProcessList::SetAllThreadPriority(DWORD dwID, int nThreadPriority)
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwID);

	if (hProcess)
	{
		DWORD dwThreadIdArray[1024];
		DWORD dwThreadCount = 0;

		// Enumerate threads within the process
		if (EnumProcessThreads(dwID, dwThreadIdArray, sizeof(dwThreadIdArray), &dwThreadCount))
		{
			for (DWORD i = 0; i < dwThreadCount; i++)
			{
				HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwThreadIdArray[i]);
				if (hThread)
				{
					// Set the priority of the thread
					if (SetThreadPriority(hThread, nThreadPriority))
					{
#ifdef _DEBUG
						_tprintf(TEXT("Thread ID=%u in process PID=%u > set priority to %d\n"), dwThreadIdArray[i], dwID, nThreadPriority);
#endif
					}
					else
					{
#ifdef _DEBUG
						_tprintf(TEXT("/!\\ Can't adjust thread priority, LastError: %u\n"), GetLastError());
#endif
					}
					CloseHandle(hThread);
				}
			}
		}

		CloseHandle(hProcess);
	}
	else
	{
#ifdef _DEBUG
		_tprintf(TEXT("Can't OpenProcess PID=%u, LastError = %u\n"), dwID, GetLastError());
#endif
	}
}

BOOL CProcessList::EnumProcessThreads(DWORD dwProcessId, DWORD* pThreadIdArray, DWORD dwArraySize, DWORD* pThreadCount)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return FALSE;

	THREADENTRY32 te32;
	te32.dwSize = sizeof(THREADENTRY32);

	if (!Thread32First(hSnapshot, &te32))
	{
		CloseHandle(hSnapshot);
		return FALSE;
	}

	DWORD count = 0;

	do
	{
		if (te32.th32OwnerProcessID == dwProcessId)
		{
			if (count < dwArraySize)
				pThreadIdArray[count++] = te32.th32ThreadID;
			else
				break;
		}
	} while (Thread32Next(hSnapshot, &te32));
	CloseHandle(hSnapshot);

	*pThreadCount = count;
	Sleep(500);
	return TRUE;
}

BOOL CProcessList::OnModule(HMODULE hModule, LPCWSTR lpszModuleName, LPCWSTR lpszPathName)
{
	if (hModule)
		lstrcpy(m_szExeName, lpszPathName);

	return FALSE;
}
