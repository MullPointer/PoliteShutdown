#include "PoliteShutdown.h"
#include "EasyWinShutdown.h"
#include "EasyWinTasks.h"
#include "LoggedOnUsers.h"
#include "winUpdate.h"
#include "version.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <ctime>

using namespace std;
using namespace EasyWin;

#include "DefaultSettings.h"



PoliteShutdown::PoliteShutdown():
	m_lastCheckOutcome(CHECK_UNKNOWN),
	m_aborting(true, false)
{
	SetDefaultSettings();
	m_settings.SetKeyPath(SETTINGS_REG_KEY); //ignore false return value if settings key not found as we can proceed with default settings

	//add log targets
	LogExcepInfo exceptions[NUM_OF_LOGS];
	AddLog(L"Log1", false, exceptions[0]);
	AddLog(L"Log2", false, exceptions[1]);
	AddLog(L"LogStdOut", true, exceptions[2]);

	//deal with any exceptions thrown in adding log targets
	for (unsigned int i = 0; i < NUM_OF_LOGS; ++i)
	{
		if (exceptions[i].name != L"")
		{
			wostringstream s;
			s << "Error in setting up ";
			if (exceptions[i].name == L"LogStdOut") //convert standard output log name to user friendly one for error message
			{
				s << "standard output log";
			}
			else
			{
				s << exceptions[i].name;
			}
			s << ". This log will not be used.";

			if (exceptions[i].message != L"")
			{
				s << L" " << exceptions[i].message;
			}

			if (exceptions[i].exception.get())
			{
				m_logger.Log(s.str(), *(exceptions[i].exception), LOG_ERROR);
			}
			else
			{
				m_logger.Log(s.str(), LOG_ERROR);
			}
		}
	}

	//startup message
	wostringstream s;
	s << VER_PRODUCTNAME_STR << " v" << VER_PRODUCTVERSION_STR << " starting";
	m_logger.Log(s.str(), LOG_DEBUG);

}

void PoliteShutdown::AddLog(const wstring & logName, bool isStdOut, LogExcepInfo & exceptInfo)
{
	try
	{
		if (m_settings.GetInt(logName) == 1)
		{
			wstring logPath = isStdOut ? L"" : m_settings.GetString(logName + L"Path"); //an empty log path indicates standard output to Logger
			if (isStdOut || logPath != L"") //for anything other than a log explicitly declared to be a std out log we do not log if given an empty path
			{
				int logLevel = (int) m_settings.GetInt(logName + L"Level");
				Logger::LogDetail logDetail = Logger::FULL_DETAIL;
				if (m_settings.GetInt(logName + L"Detail") == 0)
				{
					logDetail = Logger::LOW_DETAIL;
				}
				else
				{
					logDetail = Logger::FULL_DETAIL;
				}
				//DEBUG - if (logName != L"Log2") throw bad_alloc(); //WinException("Test Exception", 14);
				m_logger.AddTarget(logPath, logLevel, logDetail, LOG_ERROR);
			}
		}
	}
	catch (WinException & e)
	{
		exceptInfo.name = logName;
		exceptInfo.message = L"Error accessing settings."; //only the settings retrieval functions should actually throw
		exceptInfo.exception.reset(e.Clone());
		//DEBUG - e.OutputCombinedMessage(wcout);
	}
	catch (exception & e)
	{
		wostringstream s;
		s << e.what(); //widening hack
		exceptInfo.name = logName;
		exceptInfo.message = s.str();
		exceptInfo.exception.reset(NULL);
	}

}

PoliteShutdown::~PoliteShutdown()
{
	m_logger.Log(L"Closing PoliteShutdown nicely.", PoliteShutdown::LOG_DEBUG);
}

Logger & PoliteShutdown::GetLogger()
{
	return m_logger;
}

bool PoliteShutdown::IsAborting()
{
	return m_aborting.IsSet();
}

void PoliteShutdown::ResetAborting()
{
	m_aborting.Reset();
}

void PoliteShutdown::Abort()
{
	m_logger.Log(L"Requested to abort check whether should shutdown.", LOG_INFORMATION);
	m_aborting.Set();
	m_updateSearch.Abort();
}

void PoliteShutdown::CheckIfAborting()
{
	if (IsAborting())
	{
		throw AbortedException("Action aborted");
	}
}

void PoliteShutdown::WaitUnlessAborting(DWORD milliseconds)
{
	bool aborting = m_aborting.Wait(milliseconds);
	if (aborting)
	{
		throw AbortedException("Action aborted");
	}
}

void PoliteShutdown::LogException(const exception & e, LogLevel level) throw()
{
	try
	{
		wostringstream s;
		s << L"An error occured. ";
		s << e.what(); //this is a hack - widening ASCII to unicode
		m_logger.Log(s.str(), level);
	}
	catch (exception &)
	{} //eat exceptions - LogException may be called in exception handler
}

void PoliteShutdown::LogException(const WinException & e, LogLevel level) throw()
{
	m_logger.Log(e, level);
}


bool PoliteShutdown::Shutdown()
{
	CheckIfAborting();

	DWORD shutdownType = m_settings.GetInt(L"ShutdownType");
	bool isComplete = false;
	switch (shutdownType)
	{
		case SHUTDOWN_SIMULATED:
			m_logger.Log(L"System would be shut down at this point, but program set to only simulate shutdowns.", LOG_INFORMATION);
			isComplete = true;
			break;
		case SHUTDOWN_SHUTDOWN:
			m_logger.Log(L"Shutting down system.", LOG_INFORMATION);
			ShutdownLocal(	m_settings.GetInt(L"ShutdownReasonCode"),
							m_settings.GetString(L"ShutdownReason"),
							m_settings.GetInt(L"ShutdownTimeout"),
							true, //force apps closed
							false); //do not reboot
			isComplete = false;
			break;
		default:
			wostringstream s;
			s << shutdownType;
			throw WinException("Invalid shutdown type specified", 0, s.str());
	}

	return isComplete;
}


bool PoliteShutdown::sameAsPrev(CheckIdentifier check)
{
	bool same = (check == m_lastCheckOutcome);
	m_lastCheckOutcome = check;
	return same;
}


DWORD PoliteShutdown::GetSettingInt(const wstring & name)
{
	return m_settings.GetInt(name);
}


bool PoliteShutdown::CheckReady()
{
	try //ensure reason set as error if function throws
	{
		CheckIfAborting();

		if (m_settings.GetInt(L"ShutdownsDisabled") == 1)
		{
			m_logger.Log(L"Shutdowns are disabled.", LOG_INFORMATION);
			return false;
		}

		m_logger.Log(L"Checking if shutdown permitted...", LOG_DEBUG);

		bool checkLoggedOnUsers = (m_settings.GetInt(L"CheckLoggedOnUsers") != 0);
		bool checkLoggedOnByProcess = (m_settings.GetInt(L"CheckLoggedOnByProcesses") != 0);
		bool checkProcesses = (m_settings.GetInt(L"CheckProcesses") != 0);
		bool checkRunningTasks = (m_settings.GetInt(L"CheckRunningSchedTasks") != 0);
		bool checkUpdatesInProgress = (m_settings.GetInt(L"CheckWindowsUpdatesInProgress") != 0);
		bool checkUpdatesToBeInstalled = (m_settings.GetInt(L"CheckWindowsUpdatesToBeInstalled") != 0);
		bool checkUpdatesToBeUninstalled = (m_settings.GetInt(L"CheckWindowsUpdatesToBeUninstalled") != 0);

		vector<ProcessInfo> processes;
		if ((checkLoggedOnUsers && checkLoggedOnByProcess) || checkProcesses)
		{
			TempPrivilege debugPrivilege;
			try
			{
				debugPrivilege.Acquire(SE_DEBUG_NAME);
			}
			catch(WinException & e)
			{
				m_logger.Log(L"Unable to acquire seDebug privilege to ensure access to process information.", e, LOG_DEBUG);
			}//carry on even without the privilege as GetRunningProcesses will work okay without if run from system account
			GetRunningProcesses(processes);
			CheckIfAborting();
		}

		if (checkLoggedOnUsers)
		{
			wostringstream s;
			s << L"Checking if there are logged on users";
			if (checkLoggedOnByProcess) s << L" who have processes running";
			s << L"...";
			m_logger.Log(s.str(), LOG_DEBUG);
			if (!CheckLoggedOn(checkLoggedOnByProcess, processes))
			{
				return false;
			}
			CheckIfAborting();
		}

		if (checkProcesses)
		{
			m_logger.Log(L"Checking if there are any processes that should not be interupted...", LOG_DEBUG);

			if (!CheckProcesses(processes))
			{
				return false;
			}
			CheckIfAborting();
		}

		if (checkRunningTasks)
		{
			m_logger.Log(L"Checking if there are any scheduled tasks running...", LOG_DEBUG);
			if (!CheckRunningTasks())
			{
				return false;
			}
			CheckIfAborting();
		}

		if (checkUpdatesInProgress)
		{
			m_logger.Log(L"Checking if windows updates are in progress...", LOG_DEBUG);
			if (IsBusyUpdating())
			{
				m_logger.Log(L"Windows updates are in progress",
							sameAsPrev(CHECK_UPDATES_IN_PROGRESS) ? LOG_DEBUG : LOG_INFORMATION);
				return false;
			}
			CheckIfAborting();
		}

		if (checkUpdatesToBeInstalled)
		{
			m_logger.Log(L"Checking if there are windows updates to be installed...", LOG_DEBUG);
			if (!CheckUpdates(m_settings.GetString(L"WindowsUpdatesInstallCriteria"),
								CHECK_UPDATES_TO_BE_INSTALLED, L"installed"))
			{
				return false;
			}
		}

		if (checkUpdatesToBeUninstalled)
		{
			m_logger.Log(L"Checking if there are windows updates to be uninstalled...", LOG_DEBUG);
			if (!CheckUpdates(m_settings.GetString(L"WindowsUpdatesUninstallCriteria"),
								CHECK_UPDATES_TO_BE_UNINSTALLED, L"uninstalled"))
			{
				return false;
			}
		}
	}
	catch(exception &)
	{
		m_lastCheckOutcome = CHECK_ERROR;
		throw;
	}
	return true;
}




bool PoliteShutdown::CheckLoggedOn(bool byProcess, const vector<ProcessInfo> & processes)
{
	//DEBUG - throw WinException("Test Exception", 10);

	//string streams to compile text lists of logged on users
	wostringstream strListFull;	//for debug log
	wostringstream strListReduced; //for information log

	bool usersLoggedOn = false;
	vector<LogonSessionInfo> users;
	GetLoggedOnUsers(users);

	if (!byProcess)
	{
		if (users.empty())
		{
			usersLoggedOn = false;
		}
		else
		{
			usersLoggedOn = true;
			for (vector<LogonSessionInfo>::iterator iUser = users.begin(); iUser != users.end(); iUser++) //for each user...
			{
				strListReduced << iUser->userDomain << L"\\" << iUser->userName << L", ";
				strListFull << "\n    " << *iUser;
			}
		}
	}
	else
	{
		//this algorithm could be more efficient, and probably neater - though lack of copy_if in standard library does not help
		for (vector<LogonSessionInfo>::iterator iUser = users.begin(); iUser != users.end(); iUser++) //for each user...
		{
			vector<ProcessInfo> usersProcesses;

			//...find the processes belonging to that user...
			for (vector<ProcessInfo>::const_iterator iProc = processes.begin(); iProc != processes.end(); iProc++)
			{
				if(iUser->sid == iProc->sid)
				{
					usersProcesses.push_back(*iProc);
				}
			}

			//... and if there are any record them
			if (!usersProcesses.empty())
			{
				usersLoggedOn = true;

				strListReduced << iUser->userDomain << L"\\" << iUser->userName << L", ";
				strListFull << "\n    " << *iUser << L"\n    Processes running: ";
				for (vector<ProcessInfo>::const_iterator iUProc = usersProcesses.begin(); iUProc != usersProcesses.end(); ++iUProc)
				{
					strListFull << iUProc->name << ", ";
				}
			}
		}
	}

	if (usersLoggedOn)
	{
		m_logger.Log(wstring(L"Users are logged on: ") + strListReduced.str(),
					sameAsPrev(CHECK_LOGGED_ON_USERS) ? LOG_DEBUG : LOG_INFORMATION);
		m_logger.Log(wstring(L"Logged on user details:") + strListFull.str(), LOG_DEBUG);
	}

	return !usersLoggedOn;

}



void trim(wstring & s)
{
	const wchar_t delims[] = L"\n\r \t"; //this does not take into account the huge range of possible whitespace in unicode - http://www.cs.tut.fi/~jkorpela/chars/spaces.html
	s.erase(0, s.find_first_not_of(delims));
	s.erase(s.find_last_not_of(delims) + 1, s.length());
}


bool PoliteShutdown::CheckProcesses(const vector<ProcessInfo> & processes)
{
	wstring soughtStrng = m_settings.GetString(L"ProcessesNotToInterupt");

	if (soughtStrng.empty())
	{
		return true; //if there are no processes not to Interupt
	}

	try
	{
		wistringstream soughtStrm(soughtStrng);
		soughtStrm.exceptions(wostream::failbit | wostream::badbit);

		vector<ProcessInfo> foundProcesses;
		while (!soughtStrm.eof())
		{
			wstring name;
			getline(soughtStrm, name, L',');
			trim(name);
			if (!name.empty()) //preclude empty process names
			{
				m_logger.Log(wstring(L"Checking for process not to be interupted: ") + name, LOG_DEBUG);

				vector<ProcessInfo>::const_iterator iProc
					= find_if(processes.begin(), processes.end(), IsNamedProcess(name));
				if (iProc != processes.end())
				{
					foundProcesses.push_back(*iProc);
				}
			}
		}

		if (foundProcesses.empty())
		{
			return true;
		}
		else
		{
			wostringstream s;
			s << L"Processes are running that are not to be interupted:\n    ";
			for (vector<ProcessInfo>::const_iterator iProc = foundProcesses.begin(); iProc != foundProcesses.end(); ++iProc)
			{
				s << iProc->name << ", ";
			}
			m_logger.Log(s.str(), sameAsPrev(CHECK_PROCESSES) ? LOG_DEBUG : LOG_INFORMATION);
			return false;
		}
	}
	catch (wostream::failure &)
	{
		throw WinException("Error interpreting list of processes not to interupt", 0, soughtStrng);
	}

}

bool PoliteShutdown::CheckRunningTasks()
{
	//get names of all running tasks appart from PoliteShutdown's own task
	vector<Task> runningTasks;
	Scheduler sc;
	sc.GetTasks(runningTasks, RUNNING);
	wstring ownTaskName = m_settings.GetString(L"TaskName");
	vector<Task>::iterator newEnd = remove_if(runningTasks.begin(), runningTasks.end(), IsTaskNamed(ownTaskName));
	runningTasks.erase(newEnd, runningTasks.end()); //remove the old elements of the vector left at the end by remove_if for each element it removes

	if (runningTasks.empty())
	{
		m_logger.Log(L"There are no scheduled tasks running.", LOG_DEBUG);
		return true;
	}
	else
	{
		wostringstream s;
		s << L"The following scheduled tasks are running: ";
		copy(runningTasks.begin(), runningTasks.end(), ostream_iterator<Task, wchar_t>(s, L", "));
		m_logger.Log(s.str(), sameAsPrev(CHECK_TASKS) ? LOG_DEBUG : LOG_INFORMATION);
		return false;
	}
}


bool PoliteShutdown::CheckUpdates(const std::wstring & criteria, CheckIdentifier check, const wstring & message)
{
	vector<wstring> titles;
	bool success = m_updateSearch.Search(titles, criteria);
	if (!success)
	{
		throw AbortedException("Update check aborted");
	}

	if (titles.empty())
	{
		wostringstream s;
		s << L"There are no updates to be " << message << ".";
		m_logger.Log(s.str(), LOG_DEBUG);
		return true;
	}
	else
	{
		wostringstream s;
		s << L"There are updates to be " << message << ": ";
		copy(titles.begin(), titles.end(), ostream_iterator<wstring, wchar_t>(s, L", "));
		m_logger.Log(s.str(), sameAsPrev(check) ? LOG_DEBUG : LOG_INFORMATION);
		return false;
	}
}


void outputTime(wostream & s, DWORD hour, DWORD minute)
{
	wchar_t origFill = s.fill(L'0');
	s << right;
	s.width(2);
	s << hour << L":";
	s.width(2);
	s << minute;
	s.fill(origFill);
}

bool PoliteShutdown::EnsureSchedTask()
{
	bool taskToBeCorrected = false;

	CheckIfAborting();
	m_logger.Log(L"Checking PoliteShutdown scheduled task...", LOG_DEBUG);

	DWORD shutdownHour = m_settings.GetInt(L"ShutdownHour");
	DWORD shutdownMinute = m_settings.GetInt(L"ShutdownMinute");


	if (shutdownHour < 24 && shutdownMinute < 60) //when time valid, otherwise do nothing
	{
		wstring processName = GetProcessPath(); //possibly should be expectedAppName. Similarly other variables here
		wstring workingDir = processName;	//possibly should just use empty string for working dir
		RemoveLastPathComponent(workingDir);
		wstring taskName = m_settings.GetString(L"TaskName");
		wstring parameters = m_settings.GetString(L"SchedTaskParameters");
		DWORD maxRun = m_settings.GetInt(L"SchedTaskMaxRun");
		DWORD flags = m_settings.GetInt(L"SchedTaskFlags");

		TaskTriggerTiming requiredTiming;
		requiredTiming.SetDaily((WORD) shutdownHour, (WORD) shutdownMinute);
		requiredTiming.AddRepeats(m_settings.GetInt(L"ShutdownCheckPeriod"),
				m_settings.GetInt(L"ShutdownCutOffInterval"));

		Scheduler sc;
		Task task = sc.GetTask(taskName);
		if (task.IsValid())
		{
			CheckIfAborting();

			wstring actualAppName = task.GetApplicationName();
			if (ToLower(actualAppName) != ToLower(processName))
			{
				wostringstream s;
				s << L"Application set to run as scheduled task is \"" << actualAppName << L"\"";
				s << L". Correcting application path to \"" << processName << L"\"";
				m_logger.Log(s.str(), LOG_INFORMATION);

				task.SetApplicationName(processName);
				taskToBeCorrected = true;
			}

			wstring actualWorkingDir = task.GetWorkingDirectory();
			if (actualWorkingDir != workingDir)
			{
				wostringstream s;
				s << L"Working directory of scheduled task is \"" << actualWorkingDir << L"\"";
				s << L". Correcting working directory to \"" << workingDir << L"\"";
				m_logger.Log(s.str(), LOG_INFORMATION);

				task.SetWorkingDirectory(workingDir);
				taskToBeCorrected = true;
			}

			wstring actualParameters = task.GetParameters();
			if (actualParameters != parameters)
			{
				wostringstream s;
				s << L"Command line parameters of scheduled task set to \"" << actualParameters << L"\"";
				s << L". Correcting parameters to \"" << parameters << L"\"";
				m_logger.Log(s.str(), LOG_INFORMATION);

				task.SetParameters(parameters);
				taskToBeCorrected = true;
			}

			DWORD actualMaxRunMilliseconds = task.GetMaxRunTime();
			DWORD actualMaxRun = actualMaxRunMilliseconds / MILLISECONDS_IN_MINUTE; //task scheduler stores max run time in milliseconds, but we store it in minutes
			if (actualMaxRun != maxRun)
			{
				wostringstream s;
				s << L"Allowed maximum run time of scheduled task set to " << actualMaxRun << L" minutes.";
				s << L" Correcting to " << maxRun << L" minutes.";
				m_logger.Log(s.str(), LOG_INFORMATION);

				task.SetMaxRunTime(maxRun * MILLISECONDS_IN_MINUTE);
				taskToBeCorrected = true;
			}

			DWORD actualFlags = task.GetFlags();
			if (actualFlags != flags)
			{
				wostringstream s;
				s.flags(ios::hex);
				s << L"Scheduled task options being corrected from 0x" << actualFlags; //printing values is not very meaningful to user, but it is a lot quicker than printing out all flags
				s << L" to 0x" << flags;
				m_logger.Log(s.str(), LOG_INFORMATION);

				task.SetFlags(flags);
				taskToBeCorrected = true;
			}

			wstring actualAccountName;
			try
			{
				actualAccountName = task.GetAccountName();
			}
			catch (SchedulerException &) //if account name is invalid, we just want to reset it
			{
				actualAccountName = L"INVALID_VALUE";
				assert(Task::SYSTEM_ACCOUNT_NAME != L"INVALID_VALUE");
			}
			if (actualAccountName != Task::SYSTEM_ACCOUNT_NAME)
			{
				wostringstream s;
				s << L"Scheduled task is set to run in account \"" << actualAccountName << L"\"";
				s << L". Correcting to use system account";
				m_logger.Log(s.str(), LOG_INFORMATION);

				task.SetAccountInformation(Task::SYSTEM_ACCOUNT_NAME);
				taskToBeCorrected = true;
			}




			if (task.GetTriggerCount() > 1)
			{
				m_logger.Log(L"Scheduled task has more than one trigger set. Removing triggers.", LOG_INFORMATION); //TODO this condition will lead to 3 information messages - this one, no triggers, time wrong - probably this should be fixed
				task.DeleteAllTriggers();
				taskToBeCorrected = true;
			}

			if (task.GetTriggerCount() == 0)
			{
				m_logger.Log(L"Scheduled task has no triggers set. Adding suitable trigger.", LOG_INFORMATION);
				task.CreateTrigger();
				taskToBeCorrected = true;
			}

			vector <TaskTrigger> taskTriggers;
			task.GetTriggers(taskTriggers);
			if (taskTriggers.size() != 1)
			{
				throw WinException("Unable to set correct number of triggers on PoliteShutdown Scheduled task");
			}
			TaskTrigger trigger = taskTriggers[0];


			if (!trigger.HasEquivalentTiming(requiredTiming))
			{
				wostringstream s;
				s << L"Scheduled task not scheduled to run at the right time. Scheduling daily at ";
				outputTime(s, shutdownHour, shutdownMinute);
				s << L" then at " << requiredTiming.GetRepeatInterval() << L" minute intervals for ";
				s << requiredTiming.GetRepeatDuration() << " minutes.";
				m_logger.Log(s.str(), LOG_INFORMATION);
				trigger.Set(requiredTiming);
				taskToBeCorrected = true;
			}

		}
		else
		{
			wostringstream s;
			s << L"Setting new scheduled task to run daily at "; //possibly should output more information on the task created here
			outputTime(s, shutdownHour, shutdownMinute);
			s << " repeating every " << requiredTiming.GetRepeatInterval() << L" minutes for ";
			s << requiredTiming.GetRepeatDuration() << " minutes.";
			m_logger.Log(s.str(), LOG_INFORMATION);
			task = sc.NewTask(taskName);
			task.SetApplicationName(processName);
			task.SetWorkingDirectory(workingDir);
			task.SetParameters(parameters);
			task.SetMaxRunTime(maxRun * MILLISECONDS_IN_MINUTE);
			task.SetFlags(flags);
			task.SetAccountInformation(Task::SYSTEM_ACCOUNT_NAME);
			TaskTrigger trigger = task.CreateTrigger();
			trigger.Set(requiredTiming);
			taskToBeCorrected = true;
		}

		CheckIfAborting();
		if (taskToBeCorrected)
		{
			m_logger.Log(L"Saving scheduled task", LOG_DEBUG);
			task.Save();
		}
	}
	else
	{
		m_logger.Log(L"A valid time to schedule shutdown has not been set in group policy.", LOG_DEBUG);
	}

	return taskToBeCorrected;
}

void PoliteShutdown::ShutdownIfReady(bool onlyIfInShutdownInterval, bool quiet)
{
	if (onlyIfInShutdownInterval && !CheckInShutdownInterval())
	{
		m_logger.Log(L"It is outside the shutdown interval. There is no need to check if system ready to shutdown.", (quiet ? LOG_DEBUG : LOG_INFORMATION));
		return;
	}

	m_logger.Log(L"Shutting the system down if it is ready...", (quiet ? LOG_DEBUG : LOG_INFORMATION));
	if (CheckReady())
	{
		Shutdown();
	}
	else
	{
		m_logger.Log(L"System not ready to shut down, so exiting with no action.", (quiet ? LOG_DEBUG : LOG_INFORMATION));
	}
}

void PoliteShutdown::ShutdownWhenReady(ActionOnIssue whenOutShutdownInterval)
{
	unsigned int exceptionsBeforeEnd = 1;
	unsigned int exceptionCount = 0;
	unsigned int shutdownCheckPeriod = 30;
	bool tooManyErrors = false;
	bool shutdownComplete = false;

	switch (whenOutShutdownInterval)
	{
		case ACTION_IGNORE:
			m_logger.Log(L"Shutting the system down when it is ready...", LOG_INFORMATION);
			break;
		case ACTION_END:
			m_logger.Log(L"If it is within the shutdown interval, the system will be shut down when it is ready...", LOG_INFORMATION);
			break;
		case ACTION_WAIT:
			m_logger.Log(L"Whenever it is within the shutdown interval, the system will be shut down if it is ready...", LOG_INFORMATION);
			break;
		default:
			throw WinException("Programmer Error - method ShutdownWhenReady called with invalid whenOutShutdownInterval argument", ERROR_INVALID_PARAMETER);
	}
	exceptionsBeforeEnd = m_settings.GetInt(L"ErrorsBeforeEnd");
	shutdownCheckPeriod = m_settings.GetInt(L"ShutdownCheckPeriod");

	while(!tooManyErrors && !shutdownComplete)
	{
		if (whenOutShutdownInterval == ACTION_END)
		{
			if (!CheckInShutdownInterval()) //recheck on each loop as we may move out of shutdown period
			{
				m_logger.Log(L"It is now outside the shutdown interval. Exiting.", LOG_INFORMATION);
				break;
			}
		}
		else if (whenOutShutdownInterval == ACTION_WAIT)
		{
			while (!CheckInShutdownInterval()) //wait until in shutdown period to perform checks
			{
				m_logger.Log(L"It is now outside the shutdown interval. Waiting...", LOG_INFORMATION);
				WaitUnlessAborting(MILLISECONDS_IN_MINUTE * shutdownCheckPeriod); //is not ideal to wake up to check in shutdown interval so regularly - should get CheckInShutdownInterval() to return how much longer to wait
			}
		}

		try //on errors in the check and shutdown, check is retried later (until error limit set in settings is reached)
		{
			if (CheckReady())
			{
				shutdownComplete = Shutdown();
				//note that if shutdownComplete false, loop continues after shutdown started, so shutdown will be retried - though this does mean the program will have to be forced to exit by windows
			}

		}
		catch (AbortedException &) {throw;}
		catch (WinException & e)
		{
			LogException(e, LOG_ERROR);
			++exceptionCount;
		}
		catch (exception & e)
		{
			LogException(e, LOG_ERROR);
			++exceptionCount;
		}

		if (exceptionCount >= exceptionsBeforeEnd)
		{
			tooManyErrors = true;
		}

		if (!shutdownComplete && !tooManyErrors)
		{
			WaitUnlessAborting(MILLISECONDS_IN_MINUTE * shutdownCheckPeriod);
		}
	}

	if (tooManyErrors)
	{
		wostringstream s;
		s << L"Ending program as errors occured on the last " << exceptionCount;
		s << L" attempts to check if shutdown permitted";
		m_logger.Log(s.str(), LOG_CRITICAL_ERROR);
		throw WinException("Ending program as too many errors have occured.");
	}
}


void PoliteShutdown::PerformScheduledAction()
{
	bool taskWasCorrected = EnsureSchedTask();

	switch (GetSettingInt(L"SchedulingMode"))
	{
		case PoliteShutdown::SCHEDULING_TASK:
			if (GetSettingInt(L"DoubleCheckSchedRun") == 0)
			{
				if (!taskWasCorrected && HadSufficientUptime()) //if task was corrected, we might not be running at the right time, so, as we aren't going to check is correct time within program, don't do anything
				{
					ShutdownIfReady(false, true);
				}
			}
			else
			{
				if (HadSufficientUptime())
				{
					ShutdownIfReady(true, true);
				}
			}
			break;
		case PoliteShutdown::SCHEDULING_SERVICE:
			m_logger.Log(L"PoliteShutdown scheduling mode set to \"Service\", so no shutdown check performed from scheduled program execution.", LOG_DEBUG);
			break;
		case PoliteShutdown::SCHEDULING_TASK_SERVICE:
			//StartService();
			break;
		default:
			WinException e("Invalid scheduling mode specified", ERROR_INVALID_PARAMETER);
			m_logger.Log(e, PoliteShutdown::LOG_CRITICAL_ERROR);
	}
}



bool PoliteShutdown::HadSufficientUptime()
{
	bool result = false;
	DWORD systemUptime = GetSystemUptime();
	result = (systemUptime > m_settings.GetInt(L"ShutdownUptimeRequired"));

	if (!result)
	{
		wstringstream s;
		s << L"This system has been running " << systemUptime;
		s << " seconds, not long enough to check for shutdown.";
		m_logger.Log(s.str(), LOG_DEBUG);
	}
	return result;
}


void PoliteShutdown::WaitForSufficientUptime()
{
	DWORD systemUptime = GetSystemUptime();
	DWORD uptimeRequired = m_settings.GetInt(L"ShutdownUptimeRequired");

	while (systemUptime <= uptimeRequired)
	{
		wstringstream s;
		s << L"This system has been running " << systemUptime;
		s << " seconds, not long enough to check for shutdown. Waiting...";
		m_logger.Log(s.str(), LOG_DEBUG);

		WaitUnlessAborting(MILLISECONDS_IN_SECOND * (uptimeRequired - systemUptime + 1));
		systemUptime = GetSystemUptime();
	}
}


//return whether leftHour:leftMin is strictly before rightHour:rightMin
bool HMBefore(DWORD leftHour, DWORD leftMin, DWORD rightHour, DWORD rightMin)
{
	return leftHour < rightHour || (leftHour == rightHour && leftMin < rightMin);
}

bool PoliteShutdown::CheckInShutdownInterval()
{
	CheckIfAborting();

	DWORD shutdownHour = m_settings.GetInt(L"ShutdownHour");
	DWORD shutdownMinute = m_settings.GetInt(L"ShutdownMinute");
	DWORD shutdownCutOffInterval = m_settings.GetInt(L"ShutdownCutOffInterval");

	if (shutdownHour == TIME_NOT_SET || shutdownMinute >= TIME_NOT_SET)
	{
		m_logger.Log(L"Shutdown time not specified.", LOG_INFORMATION);
		return false;
	}

	if (shutdownHour >= 24 || shutdownMinute >= 60)
	{
		wostringstream s;
		s << L"Invalid shutdown time of ";
		outputTime(s, shutdownHour,shutdownMinute);
		s << L" specified.";
		m_logger.Log(s.str(), LOG_INFORMATION);
		return false;
	}
	if (shutdownCutOffInterval == 0)
	{
		m_logger.Log(L"Shutdown cutoff interval of zero set. Scheduled shutdowns will not occur.", LOG_INFORMATION);
		return false;
	}
	if (shutdownCutOffInterval >= 24 * 60) //ie set to attempt all the time
	{
		m_logger.Log(L"Shutdown cutoff interval set to 24 hours or more. Shutdowns will be attempted indefinately.", LOG_DEBUG);
		return true;
	}


	time_t currentTime = time(NULL);
	tm currentTimeStruct;
	errno_t error = localtime_s(&currentTimeStruct, &currentTime);
	if (error)
	{
		throw WinException("Unable to get current time to determine if should shutdown.");
	}

	tm startTimeStruct = currentTimeStruct;
	startTimeStruct.tm_hour = (int) shutdownHour;
	startTimeStruct.tm_min = (int) shutdownMinute;
	startTimeStruct.tm_sec = 0;
	if (HMBefore((DWORD)currentTimeStruct.tm_hour, (DWORD)currentTimeStruct.tm_min, shutdownHour, shutdownMinute))
	{ //if the current time is strictly before the shutdown time, suppose that the cutdown time was yesterday, to see if we are still before cut off interval from then
		startTimeStruct.tm_mday--; //note mktime can cope if this takes tm_mday below 0
	}

	time_t startTime = mktime(&startTimeStruct);
	if (startTime == -1)
	{
		throw WinException("Unable to work out the shutdown interval start time.");
	}

	const DWORD TIME_STR_SIZE = 50;
	wchar_t timeStr[TIME_STR_SIZE];
	error = _wasctime_s(timeStr, TIME_STR_SIZE, &startTimeStruct); //appears _wasctime_s appends a newline - should possibly remove this
	if (error)
	{
		throw WinException("Unable to write out the shutdown interval start time.");
	}

	wostringstream s;
	s << "Time calculated as start of shutdown interval: " << timeStr;
	m_logger.Log(s.str(), LOG_DEBUG);

	time_t endTime = startTime + shutdownCutOffInterval * 60;
	bool isAfterStart = (difftime(currentTime, startTime) >= 0);
	bool isBeforeEnd = (difftime(endTime, currentTime) >= 0);
	return isAfterStart && isBeforeEnd;
}
