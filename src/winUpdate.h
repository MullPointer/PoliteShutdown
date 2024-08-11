
#ifndef WINUPDATE_H
#define WINUPDATE_H

#include <vector>
#include <string>
#include "EasyWinSync.h"
#include <Wuapi.h>

class UpdateSearch
{
	public:
	UpdateSearch():
		m_searchJob(NULL) {}

	//the titles of updates meeting the given criteria are appended to titles
	// see http://msdn.microsoft.com/en-us/library/aa386526(VS.85).aspx for description of possible criteria
	//expects COM to be initialised with CoInitialize already
	//throws WinExceptions on error
	//returns true if succeeds, false if aborted
	//Search should not be called again simultaineously for the same UpdateSearch object - Abort will only abort the last call
	bool Search(std::vector<std::wstring> & titles, const std::wstring & criteria);

	//aborts the search if it is in progress
	//only aborts during the searching phase - which takes up most of time Search method takes to run
	//throws WinExceptions on error
	void Abort();

	private:
	UpdateSearch(const UpdateSearch &); //no copy constructor
	UpdateSearch & operator=(const UpdateSearch &); //no assignment

	void SetSearchJob(ISearchJob * searchJob); //synchronized access to m_searchJob

	ISearchJob * m_searchJob; //set only while search is in progress and can be aborted
	EasyWin::CriticalSection m_critSect;

};

//Return whether the windows update API reports it is busy performing an update
bool IsBusyUpdating();

//L"DeploymentAction='Installation' AND IsInstalled=0 AND IsAssigned=1"
//L"DeploymentAction='Uninstallation' AND IsInstalled=1"
//L"IsInstalled=0 and Type='Software'"


#endif // WINUPDATE_H
