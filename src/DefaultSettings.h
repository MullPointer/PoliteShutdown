
//THIS SHOULD ONLY BE INCLUDED IN POLITESHUTDOWN.CPP
//placed in header file for convenient alteration

const std::wstring PoliteShutdown::SETTINGS_REG_KEY = L"HKLM\\Software\\Policies\\PoliteShutdown";

void PoliteShutdown::SetDefaultSettings()
{
	m_settings.SetIntDefault(L"Log1", 0);
	m_settings.SetStringDefault(L"Log1Path", L""); //if path is empty, logging is disabled even if LogToLog1 setting is 1
	m_settings.SetIntDefault(L"Log1Level", LOG_ERROR);
	m_settings.SetIntDefault(L"Log1Detail", 0); //low detail, any other number for full
	m_settings.SetIntDefault(L"Log2", 0); //if path is empty, logging is disabled even if LogToLog2 setting is 1
	m_settings.SetStringDefault(L"Log2Path", L"");
	m_settings.SetIntDefault(L"Log2Level", LOG_ERROR);
	m_settings.SetIntDefault(L"Log2Detail", 0);
	m_settings.SetIntDefault(L"LogStdOut", 1);
	#ifdef DEBUG
		m_settings.SetIntDefault(L"LogStdOutLevel", LOG_DEBUG);
		m_settings.SetIntDefault(L"LogStdOutDetail", 1);
	#else
		m_settings.SetIntDefault(L"LogStdOutLevel", LOG_INFORMATION);
		m_settings.SetIntDefault(L"LogStdOutDetail", 0);
	#endif


	m_settings.SetIntDefault(L"SchedulingMode", SCHEDULING_TASK); // option should be from enum SchedulingMode - whether checks will be scheduled by running a background service, from a windows scheduled task or combination of both
	m_settings.SetIntDefault(L"ShutdownHour", TIME_NOT_SET);
	m_settings.SetIntDefault(L"ShutdownMinute", TIME_NOT_SET);
	m_settings.SetIntDefault(L"ShutdownsDisabled", 0);
	m_settings.SetIntDefault(L"ShutdownCutOffInterval", 10*60); //in minutes - if this set to equal or greater than 24 x 60 there is no cut off - even on 25 hour days (due to daylight savings)
	//m_settings.SetIntDefault(L"ShutdownCutOffHour", TIME_NOT_SET); - not implemented - would take priority over ShutdownCutOffPeriod
	//m_settings.SetIntDefault(L"ShutdownCutOffMinute", TIME_NOT_SET);
	m_settings.SetIntDefault(L"ShutdownCheckPeriod", 15); //time in minutes to wait between checks to see if permittable to shutdown
	m_settings.SetIntDefault(L"ShutdownUptimeRequired", 10*60); //time in seconds that computer must have been running before shutdown checks start
	m_settings.SetStringDefault(L"TaskName", L"Polite Shutdown.job"); //to be altered with caution - do not make easily settable - always include .job
	m_settings.SetStringDefault(L"SchedTaskParameters", L"/t");
	m_settings.SetIntDefault(L"SchedTaskMaxRun", 2*60); //in minutes
	m_settings.SetIntDefault(L"SchedTaskFlags", 0);
	m_settings.SetIntDefault(L"DoubleCheckSchedRun", 1); //whether program performs own check is in period specified by group policy when run as scheduled task

	m_settings.SetStringDefault(L"ShutdownReason", L"PoliteShutdown scheduled shutdown");
	m_settings.SetIntDefault(L"ShutdownReasonCode", SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
	#ifdef DEBUG
		m_settings.SetIntDefault(L"ShutdownType", SHUTDOWN_SIMULATED);
		m_settings.SetIntDefault(L"ShutdownTimeout", 30); //gives time for shutdowns during debugging to be cancelled
	#else
		m_settings.SetIntDefault(L"ShutdownType", SHUTDOWN_SHUTDOWN);
		m_settings.SetIntDefault(L"ShutdownTimeout", 0); //timeout in seconds given to windows for shutdown operation
	#endif

	m_settings.SetIntDefault(L"CheckLoggedOnUsers", LOGGED_ON_WAIT);
	m_settings.SetIntDefault(L"CheckLoggedOnByProcesses", 1);
	m_settings.SetIntDefault(L"CheckProcesses", 1);
	m_settings.SetStringDefault(L"ProcessesNotToInterupt", L"msiexec.exe,package.exe,ntbackup.exe"); //comma separated, whitespace at start and end of each name trimmed - trimming implies can't cope with processes with names with leading or trailing spaces - should possibly be multi-String, but perhaps not if want to set by adm
	m_settings.SetIntDefault(L"CheckRunningSchedTasks", 1);
	m_settings.SetIntDefault(L"CheckWindowsUpdatesInProgress", 1);
	m_settings.SetIntDefault(L"CheckWindowsUpdatesToBeInstalled", 1);
	m_settings.SetIntDefault(L"CheckWindowsUpdatesToBeUninstalled", 1);

	m_settings.SetIntDefault(L"ErrorsBeforeEnd", 3);

	//advanced settings not to be readily alterable
	m_settings.SetStringDefault(L"WindowsUpdatesInstallCriteria", L"DeploymentAction='Installation' AND IsInstalled=0 AND IsAssigned=1 AND IsHidden=0");
	m_settings.SetStringDefault(L"WindowsUpdatesUninstallCriteria", L"DeploymentAction='Uninstallation' AND IsInstalled=1");

}
