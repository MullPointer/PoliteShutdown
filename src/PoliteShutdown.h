#ifndef POLITESHUTDOWN_H
#define POLITESHUTDOWN_H

#include <string>
#include <memory>
#include "Logger.h"
#include "SettingsSource.h"
#include "Processes.h"
#include "winUpdate.h"


class AbortedException: public EasyWin::WinException //possibly should be child of exception
{
	public:
	explicit AbortedException(std::string message, DWORD errorCode = (DWORD) 0, std::wstring unicodeMessage=L""):
		WinException(message, errorCode, unicodeMessage) {}
	virtual WinException * Clone() const //allow polymorphic copying
	{
		return new AbortedException(*this);
	}
	virtual ~AbortedException() {}
};

class PoliteShutdown
{
	public:
	//throws on error accessing settings (other than they are not found), but not if unable to access log targets
	PoliteShutdown(); //must not use COMLibrary
	~PoliteShutdown();


	//all methods throw when a critical error occurs

	//methods that must be called from MAIN THREAD and throw AbortedException
	//	as soon as possible if PoliteShutdown set to abort:

	//quiet determines whether starting and ending messages are shown as information messages or debug messages
	void ShutdownIfReady(bool onlyIfInShutdownInterval = false, bool quiet = false);

	enum ActionOnIssue
	{
		ACTION_IGNORE,
		ACTION_END,
		ACTION_WAIT
	};

	//will log and continue on a number of errors specified by setting in shutdown or check for shutdown, then will throw exception describing last
	void ShutdownWhenReady(ActionOnIssue whenOutShutdownInterval = ACTION_IGNORE);

	//perform an action dependent on Scheduling Mode
	//	first always checks scheduled task and service setup correctly for selected mode
	//	Task - Run a shutdown check
	//	Service - Nothing else
	//	Task Service - Starts the service
	void PerformScheduledAction();

	//does nothing if ShutdownHour, ShutdownMinute settings not set or not valid
	//does not correct the "run only if logged on" option
	//returns whether the task was created or corrected
	bool EnsureSchedTask();

	//return whether the computer has been running long enough, according to settings, to allow a shutdown check
	bool HadSufficientUptime();

	//wait until the computer has been running long enough, according to settings, to allow a shutdown check
	void WaitForSufficientUptime();

	//return specified setting from SettingSource - logs any exceptions, and rethrows them
	DWORD GetSettingInt(const std::wstring & name);

	//methods that may be called from ANY THREAD (after construction):
	Logger & GetLogger();
	void Abort();
	void ResetAborting(); //reset the aborting indicator, so new calls to ShutdownIfReady
	bool IsAborting();

	enum LogLevel
	{
		LOG_DEBUG = 0,
		LOG_INFORMATION = 100,
		LOG_ERROR = 150,
		LOG_CRITICAL_ERROR = 200
	};
	void LogException(const std::exception & e, LogLevel level) throw();
	void LogException(const EasyWin::WinException & e, LogLevel level) throw();

	static const std::wstring SETTINGS_REG_KEY;
	static const DWORD TIME_NOT_SET = 100;
	static const DWORD LOGGED_ON_IGNORE = 0;
	static const DWORD LOGGED_ON_WAIT = 1;
	//static const DWORD LOGGED_ON_ASK = 2; - not implemented
	static const DWORD SHUTDOWN_SIMULATED = 0;
	static const DWORD SHUTDOWN_SHUTDOWN = 1;
	//static const DWORD SHUTDOWN_HIBERNATE = 2; - not implemented
	enum SchedulingMode
	{
		SCHEDULING_TASK = 0,
		SCHEDULING_SERVICE = 1,
		SCHEDULING_TASK_SERVICE = 2
	};
	static const DWORD MILLISECONDS_IN_SECOND = 1000;
	static const DWORD SECONDS_IN_MINUTE = 60;
	static const DWORD MILLISECONDS_IN_MINUTE = MILLISECONDS_IN_SECOND * SECONDS_IN_MINUTE;


	protected: //methods do not log errors, and throw exceptions
	enum CheckIdentifier //the reasons CheckReady() might have a particular outcome - mainly identifiers for different checks that might have failed
	{
		CHECK_UNKNOWN, //initial value when CheckReady() not yet called
		CHECK_LOGGED_ON_USERS,
		CHECK_PROCESSES,
		CHECK_TASKS,
		CHECK_UPDATES_IN_PROGRESS,
		CHECK_UPDATES_TO_BE_INSTALLED,
		CHECK_UPDATES_TO_BE_UNINSTALLED,
		CHECK_SUCCESS, //all checks passed - ready for shutdown
		CHECK_ERROR //error occured on last call to CheckReady()
	};

	void SetDefaultSettings(); //implemented in DefaultSettings.h
	void WaitUnlessAborting(DWORD milliseconds);

	//returns whether shutdown operation completed (only actually likely to be true for simulated shutdowns. If false, shutdown should be retried later in case shutdown fails)
	bool Shutdown();
	bool CheckReady(); //throws AbortedException if Abort called before finished
	void CheckIfAborting();

	//return true if each aspect ready for shutdown
	bool CheckLoggedOn(bool byProcess, const std::vector<ProcessInfo> & processes);
	bool CheckProcesses(const std::vector<ProcessInfo> & processes);
	bool CheckRunningTasks();
	bool CheckUpdates(const std::wstring & criteria, CheckIdentifier check, const std::wstring & message);

	//returns false if shutdown disabled
	//TODO - not really sure how this copes with daylight savings changes
	//		should probably be rewritten with a decent date library
	bool CheckInShutdownInterval();

	SettingsSource m_settings;
	Logger m_logger;
	CheckIdentifier m_lastCheckOutcome;




	private:
	PoliteShutdown(const PoliteShutdown &); //no copy	- as SettingsSource can't be copied
	PoliteShutdown & operator= (const PoliteShutdown &); //no assignment

	struct LogExcepInfo
	{
		std::wstring name;
		std::wstring message;
		std::auto_ptr<EasyWin::WinException> exception;
	};

	EasyWin::Event m_aborting;
	UpdateSearch m_updateSearch; //used only in CheckUpdates
	static const unsigned int NUM_OF_LOGS = 3; //not all necessarily written to
	//if an exception occurs it is packaged as a LogExcepInfo and placed in the location given
	void AddLog(const std::wstring & logName, bool isStdOut, LogExcepInfo & exceptInfo);
	//return whether parameter is same as lastCheckOutcome and update lastCheckOutcome to parameter
	bool sameAsPrev(CheckIdentifier check);
};

const LPWSTR SERVICE_NAME = L"PoliteShutdown"; //TODO should be shorter name?


#endif // POLITESHUTDOWN_H
