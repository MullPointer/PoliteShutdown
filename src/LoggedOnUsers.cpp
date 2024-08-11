
#include "EasyWin.h"
#include "LoggedOnUsers.h"
#include "auto_handle.h"
#include <Ntsecapi.h>
#include <iostream>

using namespace EasyWin;
using namespace std;

//todo define a custom exception type with NTSTATUS value - LsaNtStatusToWinError


//hopefully equivalent to the windows macro from ntdef.h (which doesn't seem to be in SDK) - see http://msdn.microsoft.com/en-us/library/aa489609.aspx
inline bool NT_ERROR(NTSTATUS status)
{
	return status >= 0xC0000000 && status <= 0xFFFFFFFF;
}

//convert lsaStr to a wstring and append it to wStr
// the buffer of a LSA_UNICODE_STRING may not be null terminated
// not very efficient
// TODO - possibly should do something if lsaStr is odd length - pos not
void LSAStringToWString(LSA_UNICODE_STRING lsaStr, wstring & wStr)
{
	wchar_t buf[2];
	buf[1] = '\0'; //null terminator
	USHORT lsaLength = lsaStr.Length / 2; //as lsaStr.Length is in bytes and each unicode character is 2 bytes
	wStr.reserve(wStr.size() + lsaLength);
	for (USHORT i = 0; i < lsaLength; i++)
	{
		buf[0] = (lsaStr.Buffer)[i];
		wStr.append(buf);
	}
}


void CleanupLsaBuffer(void * p)
{
	LsaFreeReturnBuffer(p);
}

void GetLoggedOnUsers(vector<LogonSessionInfo> & users)
{
	NTSTATUS status = 0;

	unsigned long logonSessionCount = 0;
	PLUID logonSessions = NULL;
	status = LsaEnumerateLogonSessions(&logonSessionCount, &logonSessions);
	if (NT_ERROR(status) || !logonSessions) //TODO not sure if logonSessions is null if no one logged on. Possibly also ought to be catching warnings as well as errors
	{
		throw WinException("Failed to enumerate logon sessions.", (DWORD) status);
	}
	auto_handle<void *, CleanupLsaBuffer> logonSessionsAutoCleanup(logonSessions); //free sessions buffer even if exception thrown

	for (unsigned long i = 0; i < logonSessionCount; ++i)
	{
		PSECURITY_LOGON_SESSION_DATA logonData;
		status = LsaGetLogonSessionData(&(logonSessions[i]), &logonData);
		if (NT_ERROR(status) || !logonData)
		{
			throw WinException("Failed to obtain information on logon session.", (DWORD) status);
		}
		auto_handle<void *, CleanupLsaBuffer> logonDataAutoCleanup(logonData); //free session data buffer even if exception thrown

		SECURITY_LOGON_TYPE logonType = (SECURITY_LOGON_TYPE) logonData->LogonType;
		if (logonType == Interactive || logonType == RemoteInteractive)
		{
			wstring userName;
			wstring userDomain;
			LSAStringToWString(logonData->UserName, userName);
			LSAStringToWString(logonData->LogonDomain, userDomain);
			if (!logonData->Sid) //don't think this is possible
			{
				throw WinException("Found interactive user with null SID.", (DWORD) status);
			}
			SIDclass sid(logonData->Sid);
			LogonSessionInfo session(userName,
								userDomain,
								sid,
								logonSessions[i],
								(logonType == Interactive));
			users.push_back(session);
		}
	}
}



wostream & operator<<(wostream & o, const LogonSessionInfo & session)
{
	o << session.userDomain << L"\\" << session.userName << L" ";
	o << session.sid.ToString() << L" ";
	printLUID(o, session.sessionLUID);
	o << L" ";
	o << (session.local ? L"Local" : L"Remote");
	return o;
}
