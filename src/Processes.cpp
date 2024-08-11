
#include "EasyWin.h"
#include "Processes.h"
#include <Wtsapi32.h> //for WTSEnumerateProcesses
#include <iostream>
#include <sstream>
#include "EasyWinPrivilege.h"



using namespace std;
using namespace EasyWin;

const DWORD IDLE_PROCESS_ID = 0;
const DWORD SYSTEM_PROCESS_ID = 4; //True for XP, vista, server 2003 - apparently not true on windows 2000 or NT - we do not support those operating systems yet
bool IsSystemProcess(DWORD processID) //this function will need to be altered to detect version of windows in order to support windows 2000, and possibly for future versions
{
	return processID == IDLE_PROCESS_ID || processID == SYSTEM_PROCESS_ID;
}


void WTSCleanup(WTS_PROCESS_INFO * d)
{
	WTSFreeMemory(d);
}

void GetRunningProcesses(vector<ProcessInfo> & processes)
{
	//REMOVED to outside method - TempPrivilege debugPrivilege(SE_DEBUG_NAME);

	WTS_PROCESS_INFO * processesWTS = NULL;
	DWORD processCount = 0;
	BOOL success = WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, //we want processes on local machine
											0, 1, //reserved values
											&processesWTS,
											&processCount);

	if (!success)
	{
		//TODO specific message for error indicating terminal services down
		throw WinException("Failed to obtain list of processes", GetLastError());
	}
	auto_ptrx<WTS_PROCESS_INFO, WTSCleanup> processesWTSCleanup(processesWTS);

	for (unsigned int i = 0; i < processCount; i++)
	{
		if (!IsSystemProcess(processesWTS[i].ProcessId)) //we don't include system processes like Idle Process - these cause problems, like having null SIDs
		{
			if (processesWTS[i].pUserSid)
			{
				ProcessInfo process(processesWTS[i].ProcessId,
									wstring(processesWTS[i].pProcessName),
									SIDclass(processesWTS[i].pUserSid));
				processes.push_back(process);
			}
			else //ie if process has null SID
			{
				throw WinException("Unable to retrieve the SID identifying the user to whom a process belongs", 0 , wstring(processesWTS[i].pProcessName));
				//should probably include process id in error message too - but this code should be redone not to throw an exception here anyway
			}
		}
	}
}



std::wostream & operator<<(std::wostream & o, const ProcessInfo & process)
{
	o << process.name << L" ";
	o << L"ID: " << process.id << L" ";
	o << process.sid.ToString();
	return o;
}


