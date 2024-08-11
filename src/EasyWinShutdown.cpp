
#include "EasyWinShutdown.h"
#include "EasyWin.h"

namespace EasyWin
{

using namespace std;


void ShutdownLocal(DWORD reasonCode, const std::wstring & message, DWORD timeout, bool forceAppsClosed, bool reboot)
{
	const size_t SHUTDOWN_MESSAGE_LIMIT = 3072; //for XP and server 2003 - as stated at http://msdn.microsoft.com/en-us/library/aa376874(VS.85).aspx

	wstring limitedMessage = message.substr(0, SHUTDOWN_MESSAGE_LIMIT - 1);
	TCHAR messageCStr[SHUTDOWN_MESSAGE_LIMIT]; //necessary as InitiateSystemShutdownEx requires non-const argument
	wcscpy_s(messageCStr, SHUTDOWN_MESSAGE_LIMIT, limitedMessage.c_str());

	if (timeout > MAX_SHUTDOWN_TIMEOUT) timeout = MAX_SHUTDOWN_TIMEOUT;

	try
	{
		AlterPrivilege(SE_SHUTDOWN_NAME, true);
	}
	catch (WinException & e)
	{
		e.SetMessage("Unable to obtain the privileges to shutdown the local computer - " + e.GetMessage());
		throw e;
	}
	BOOL success = InitiateSystemShutdownEx(NULL, //local computer shutdown
							messageCStr,
							timeout,
							forceAppsClosed,
							reboot,
							reasonCode);
	if (!success)
	{
		DWORD errorCode = GetLastError();
		try
		{
			AlterPrivilege(SE_SHUTDOWN_NAME, false);
		}
		catch (WinException &)
		{} //eat exceptions as are about to throw a more important exception
		throw WinException("Failed to shut the local computer down", errorCode);
	}
	else
	{
		try
		{
			AlterPrivilege(SE_SHUTDOWN_NAME, false);
		}
		catch (WinException & e)
		{
			e.SetMessage("Problem reverting privileges after initiating shutdown of the local computer - " + e.GetMessage());
			throw e;
		}
	}
}


void AbortShutdownLocal()
{
	try
	{
		AlterPrivilege(SE_SHUTDOWN_NAME, true);
	}
	catch (WinException & e)
	{
		e.SetMessage("Unable to obtain the privileges to abort shutdown of local computer - " + e.GetMessage());
		throw e;
	}

	BOOL success = AbortSystemShutdown(NULL);

	if (!success)
	{
		DWORD errorCode = GetLastError();
		try
		{
			AlterPrivilege(SE_SHUTDOWN_NAME, false);
		}
		catch (WinException &)
		{} //eat exceptions as are about to throw a more important exception
		throw WinException("Failed to abort the shutdown of the local computer", errorCode);
	}
	else
	{
		try
		{
			AlterPrivilege(SE_SHUTDOWN_NAME, false);
		}
		catch (WinException & e)
		{
			e.SetMessage("Problem reverting privileges after aborting shutdown of the local computer - " + e.GetMessage());
			throw e;
		}
	}
}








} //end namespace EasyWin
