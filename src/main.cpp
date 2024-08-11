#include "PoliteShutdown.h"

#include <memory>
#include <iostream>
#include <string>
#include "version.h"


using namespace std;
using namespace EasyWin;


//globals to allow the functions that make up the service to communicate
SERVICE_STATUS_HANDLE g_serviceStatusHandle;
PoliteShutdown * g_ps = NULL;
DWORD g_serviceState = 0;


bool InitializePS(auto_ptr<PoliteShutdown> & ps);
void DisplayHelpMessage();

void WINAPI ServiceMain(DWORD numArgs, LPTSTR * args);
DWORD WINAPI ServiceCtrlHandler(DWORD controlCode, DWORD eventType, LPVOID eventData, LPVOID context);
void ServiceReportStatus(DWORD waitHint = 0);
void ServiceReportStop(DWORD exitCode);


int wmain(int argc, wchar_t * argv[])
{


	bool succeeded = true;
	if (argc > 1)
	{
		auto_ptr<PoliteShutdown> ps;
		COMLibrary comLib;

		if (InitializePS(ps))
		{

			try
			{
				wstring param (argv[1]);
				if (param.length() == 2 && (param.at(0) == '/' || param.at(0) == '-'))
				{
					wchar_t paramL = (wchar_t) tolower(param.at(1));
					switch (paramL)
					{
						case 'n':
							comLib.Initialize(COINIT_MULTITHREADED); //COM initialisation - not actually needed unless certain checks are to be performed, but easiest and safest to perform always (especially as checks performed might change while program is running)
							ps->ShutdownIfReady(false, false);
							break;
						case 'w':
							comLib.Initialize(COINIT_MULTITHREADED);
							ps->ShutdownWhenReady();
							break;
						case 's':
							comLib.Initialize(COINIT_MULTITHREADED);
							ps->EnsureSchedTask();
							break;
						case 't':
							comLib.Initialize(COINIT_MULTITHREADED);
							ps->PerformScheduledAction();
							break;
						default:
							DisplayHelpMessage();
					}
				}
				else
				{
					DisplayHelpMessage();
				}
				succeeded = true;
			}
			catch (const AbortedException &)
			{
				succeeded = true;
			}
			catch (const WinException & e)
			{
				ps->LogException(e, PoliteShutdown::LOG_CRITICAL_ERROR);
				succeeded = false;
			}
			catch (const exception & e)
			{
				ps->LogException(e, PoliteShutdown::LOG_CRITICAL_ERROR);
				succeeded = false;
			}
		}
	}
	else
	{
		SERVICE_TABLE_ENTRY st[] = {
			{SERVICE_NAME, ServiceMain},
			{NULL, NULL}
			};
		wcout << L"Testing to see if should connect to service controller to run as service...\n";
		wcout.flush();
		BOOL svcSuccess = StartServiceCtrlDispatcher(st);
		if(!svcSuccess)
		{
			DWORD error = GetLastError();
			if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) //ie if program is being run as a console application not service
			{
				wcout << L"Failed to connect to service controller.\n\n";
				DisplayHelpMessage();
				succeeded = true; //possibly this ought to be false, as this could potentially be an error state if ERROR_FAILED_SERVICE_CONTROLLER_CONNECT occurs when program is being run as service
			}
			else
			{
				succeeded = false;
			}
		}
	}
	return succeeded ? 0 : 1;
}


bool InitializePS(auto_ptr<PoliteShutdown> & ps)
{
	try
	{
		ps.reset(new PoliteShutdown());
		return true;
	}
	catch (WinException & e)
	{
		cerr << "Initialization error. ";
		e.OutputCombinedMessage(wcerr, true, true);
		return false;
	}
	catch (exception & e)
	{
		cerr << "Initialization error. ";
		cerr << e.what();
		return false;
	}
}


void DisplayHelpMessage()
{
	cout << VER_PRODUCTNAME_STR << " v" << VER_PRODUCTVERSION_STR << " - " << VER_FILESUBTITLE << endl;
	cout << VER_LEGALCOPYRIGHT_STR << endl;
	cout << endl;
	cout << "Usage: PoliteShutdown [/n | /w | /s | /t]" << endl;
	cout << endl;
	cout << "\t/n\tCheck if conditions allow a shutdown, and if so shut down" << endl;
	cout << "\t/w\tWait until conditions allow a shutdown, and then shut down" << endl;
	cout << "\t/s\tSchedule program to be run in Windows task scheduler" << endl;
	cout << "\t/t\tSchedule as /s, then check as /n." << endl;
	cout << "\t/?\tDisplay this help." << endl;
	cout << endl;
	cout << "The times at which the program is scheduled to run are specified in group policy options. ";
	cout << "At the scheduled time it is run with option /t from the location it ran when the scheduled task was set up. " << endl;
	cout << "The /t switch will not cause shutdown until the system has been running for a specified length of time.";
}


void WINAPI ServiceMain(DWORD numArgs, LPTSTR * args)
{
	numArgs; args; //prevent compiler warning that service arguments ignored

	auto_ptr<PoliteShutdown> ps;
	bool successfulInit = InitializePS(ps); //need to initialize PoliteShutdown object at this stage to log errors
	if (!successfulInit)
	{
		return;
	}
	g_ps = ps.get();

	g_serviceStatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME,
			ServiceCtrlHandler,
			NULL); //no "context" data passed on Handler
	if (g_serviceStatusHandle == 0) //on error
	{
		WinException e("Failed to register service control handler. PoliteShutdown service may not be set up correctly.", GetLastError()); //this could possibly throw - but we will let the program end with exception in that case, as little else we can sensibly do
		ps->LogException(e, PoliteShutdown::LOG_CRITICAL_ERROR);
		return;
	}

	//if had long initialization would here set status to START_PENDING

	bool succeeded = true;
	COMLibrary comLib;
	try
	{
		comLib.Initialize(COINIT_MULTITHREADED);
		g_serviceState = SERVICE_RUNNING;
		ServiceReportStatus();



		ps->WaitForSufficientUptime();
		try
		{
			ps->EnsureSchedTask();
		}
		catch (AbortedException &) {throw;}
		catch (const WinException & e)
		{
			ps->LogException(e, PoliteShutdown::LOG_ERROR);
		}
		catch (exception & e)
		{
			ps->LogException(e, PoliteShutdown::LOG_ERROR);
		} //do not end on error exception thrown by EnsureSchedTask as we should still be able to perform shutdown check

		switch (ps->GetSettingInt(L"SchedulingMode"))
		{
			case PoliteShutdown::SCHEDULING_TASK:
				ps->GetLogger().Log(L"PoliteShutdown scheduling mode set to \"Task\", so PoliteShutdown service stopping.", PoliteShutdown::LOG_DEBUG);
				break;
			case PoliteShutdown::SCHEDULING_SERVICE:
				ps->ShutdownWhenReady(PoliteShutdown::ACTION_WAIT);
				break;
			case PoliteShutdown::SCHEDULING_TASK_SERVICE:
				ps->ShutdownWhenReady(PoliteShutdown::ACTION_END);
				break;
			default:
				WinException e("Invalid scheduling mode specified", ERROR_INVALID_PARAMETER);
				ps->LogException(e, PoliteShutdown::LOG_CRITICAL_ERROR);
				return;
		}
	}
	catch (const AbortedException &)
	{
		succeeded = true;
	}
	catch (const WinException & e)
	{
		ps->LogException(e, PoliteShutdown::LOG_CRITICAL_ERROR);
		succeeded = false;
	}
	catch (const exception & e)
	{
		ps->LogException(e, PoliteShutdown::LOG_CRITICAL_ERROR);
		succeeded = false;
	}

	g_ps = NULL;
	ps.reset();
	comLib.Uninitialize(); //ensure cleaned up before setting service stopped - as that apparently may kill process instantly
	g_serviceState = SERVICE_STOPPED;
	ServiceReportStop(succeeded ? 0 : 1); //TODO more useful error code return

}

DWORD WINAPI ServiceCtrlHandler(DWORD controlCode, DWORD eventType, LPVOID eventData, LPVOID context)
{
	eventType; eventData; context; //prevent compiler warning that these arguments ignored
	g_ps->GetLogger().Log(L"Handling service control request.", PoliteShutdown::LOG_DEBUG);

	DWORD retVal = ERROR_CALL_NOT_IMPLEMENTED;
	DWORD waitHint = 0;
	switch(controlCode)
	{
		case SERVICE_CONTROL_INTERROGATE:
			retVal = NO_ERROR;
			break;
		case SERVICE_CONTROL_STOP:
			try
			{
				g_ps->Abort();
			}
			catch (exception &) {} //ignore error exceptions - service controller should kill service after wait hint
			g_serviceState = SERVICE_STOP_PENDING;
			waitHint = 5000; //TODO tweak this
			retVal = NO_ERROR;
			break;
		default:
			retVal = ERROR_CALL_NOT_IMPLEMENTED;
	}
	ServiceReportStatus(waitHint);
	return retVal;
}


void ServiceReportStatus(DWORD waitHint) //could be extended by adding argument for check point
{
	SERVICE_STATUS status;
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = g_serviceState;
	if (g_serviceState == SERVICE_START_PENDING)
	{
		status.dwControlsAccepted = 0;
	}
	else
	{
		status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	}
	status.dwWin32ExitCode = NO_ERROR;
	status.dwServiceSpecificExitCode = 0;
	status.dwCheckPoint = 0;
	status.dwWaitHint = waitHint;

	BOOL succeeded = SetServiceStatus(g_serviceStatusHandle, &status);
	if (!succeeded)
	{
		WinException e("Failed to report service status.", GetLastError()); //this could possibly throw - but we will let the program end with exception in that case
		if (g_ps)
		{
			g_ps->LogException(e, PoliteShutdown::LOG_ERROR);
		}
	} //do not end program on error - it is possible service could still do something useful, even if its status not reported
}

void ServiceReportStop(DWORD exitCode)
{
	g_serviceState = SERVICE_STOPPED;

	SERVICE_STATUS status;
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = g_serviceState;
	status.dwControlsAccepted = 0; //SERVICE_ACCEPT_STOP;
	if (exitCode == 0)
	{
		status.dwWin32ExitCode = NO_ERROR;
		status.dwServiceSpecificExitCode = 0;
	}
	else
	{
		status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		status.dwServiceSpecificExitCode = exitCode;
	}
	status.dwCheckPoint = 0;
	status.dwWaitHint = 0;

	SetServiceStatus(g_serviceStatusHandle, &status);
	//no action taken on error - PoliteShutdown managing logging should already have been destroyed
}

