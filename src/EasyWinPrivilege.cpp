#include "EasyWinPrivilege.h"
#include "EasyWin.h"

namespace EasyWin
{

using namespace std;


void CleanupHandle(HANDLE h)
{
	CloseHandle(h);
}



void AlterPrivilege(const wchar_t * privName, bool enable)
{
	const string ACTION = enable ? "enable" : "disable";
	HANDLE token = NULL;
	BOOL success = OpenProcessToken(GetCurrentProcess(), //this current process handle need not be closed
									TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
									&token);
	if (!success)
	{
		throw WinException(string("Unable to open process token to ") + ACTION + " privilege.", GetLastError(), privName);
	}
	auto_handle<HANDLE, CleanupHandle> tokenCleanup(token);


	LUID luid;
	success = LookupPrivilegeValue(NULL, //search on local system
					privName,
					&luid);
	if (!success)
	{
		throw WinException(string("Invalid name specified for priviledge to ") + ACTION + ".",
							GetLastError(), privName);
	}

	TOKEN_PRIVILEGES privileges; //the size of the Privileges array is ANYSIZE_ARRAY, which equals 1 - however I expect could actually define a structure with a different value
	privileges.PrivilegeCount = 1;
	privileges.Privileges[0].Luid = luid;
	privileges.Privileges[0].Attributes = (DWORD) (enable ? SE_PRIVILEGE_ENABLED : 0); //0 for privilege disabled
		//Some examples suggest you should retrieve previous Attributes property for this privilege,
		//  alter the SE_PRIVILEGE_ENABLED flag then set that as the Attributes property
		// (see one of the two implementations in example http://support.microsoft.com/kb/131065 , but not the example here http://msdn.microsoft.com/en-us/library/aa446619(VS.85).aspx )
		//  however this does not seem necessary as it seems SE_PRIVILEGE_ENABLED and SE_PRIVILEGE_REMOVED
		//  are the only flags, both of which we want to set
		//  - possibly the idea was future proofing against additional properties

	success = AdjustTokenPrivileges(token,
					FALSE, //don't disable all priviledges
					&privileges,
					sizeof(TOKEN_PRIVILEGES),
					NULL, NULL); //we don't want to retrieve the previous state
	if (!success || GetLastError() != ERROR_SUCCESS) //think function may fail to assign all priviledges and still return true
	{
		throw WinException(string("Unable to adjust process privileges to ") + ACTION + " privilege.", GetLastError(), privName);
	}
}



TempPrivilege::TempPrivilege():
	m_privName(NULL)
{}

TempPrivilege::TempPrivilege(const wchar_t * privName):
	m_privName(NULL)
{
	Acquire(privName);
}


TempPrivilege::~TempPrivilege()
{
	try
	{
		Release();
	}
	catch(exception &)
	{} //do nothing - we do not want to propagate exceptions during destruction
}

void TempPrivilege::Acquire(const wchar_t * privName)
{
	if (m_privName)
	{
		Release();
	}
	AlterPrivilege(privName, true);
	m_privName = privName;
}

void TempPrivilege::Release()
{
	if (m_privName)
	{
		AlterPrivilege(m_privName, false);
		m_privName = NULL;
	}
}

const wchar_t * TempPrivilege::GetPrivName() const
{
	return m_privName;
}


} //end namespace EasyWin
