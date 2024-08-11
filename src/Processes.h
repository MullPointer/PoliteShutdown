#ifndef PROCESSES_H
#define PROCESSES_H


#include <string>
#include <vector>
#include "EasyWinSID.h"

struct ProcessInfo
{
	ProcessInfo(DWORD aID, const std::wstring & aName, const EasyWin::SIDclass & aSid):
		id(aID), name(aName), sid(aSid) {}

	DWORD id;
	std::wstring name; //name of the executable only
	EasyWin::SIDclass sid;
};


std::wostream & operator<<(std::wostream & o, const ProcessInfo & process);

//appends to parameter vector, descriptions of all the currently running processes on the local computer
//System process and System Idle process are not included
//note requires terminal services to be running
void GetRunningProcesses(std::vector<ProcessInfo> & processes);

//predicate functor associated with some process name at constrction
//then returns whether given process info structures have that name (case-insensitive comparison)
//for use with STL algorithms like find_if
class IsNamedProcess
{
	public:
	IsNamedProcess(const std::wstring & name):
		m_name(name) {}

	bool operator() (const ProcessInfo & proc)
	{
		return EasyWin::ToLower(m_name) == EasyWin::ToLower(proc.name);
	}

	private:
	std::wstring m_name; //could sacrifice robustness and reusability for efficiency by making this a pointer
};



#endif // PROCESSES_H

