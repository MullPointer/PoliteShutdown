# PoliteShutdown

Archived software, released for portfolio reasons, but originally built for Windows XP and not reliable with current versions of Windows.

For shutting down Windows computers only if they are not in use and have no maintenance tasks to run.

Configured by group policy or registry settings - these specify shutdown conditions and checks to run before shutdown is allowed. Can check that:
- no users are logged in
- processes specified are not running
- no scheduled tasks are running
- no Windows updates are in progress or scheduled to run
- system has run at least the specified time

Can be run as an immediate check or create a scheduled task to check regularly between certain hours.

While it compiles and superficially works on Windows 11, there are major bugs. Use of standby is now a better option for the original use case of saving power on computer lab workstations.

Code includes a lightweight wrapper around Windows API, labelled EasyWin for separate distribution, to provide more idiomatic RAII interface.


## Known Issues on Windows 10 and 11
- Some service accounts (eg Window Manager\DWM-1, Font Driver Host\UMFD-1) are detected as logged in users, so shutdowns will not occur if the check for logged in users is applied. An option to ignore these would be needed.
- Scheduled tasks do not properly update if details are changed. Using a newer interface to the task scheduler may be necessary.
- Though apparently created correctly, the scheduled task to trigger shutdown checks gets triggered throughout the day ignoring limits set. Shutdowns are prevented by a safety check in the program itself. The code contains an incomplete implementation of an alternative approach using a background service instead of the task scheduler.




## Compilation
A build file for CMake is included. It can be compiled using the [free Microsoft compiler package](https://visualstudio.microsoft.com/downloads/#remote-tools-for-visual-studio-2022).

Built tested with Build Tools for Visual Studio 2022 and Windows SDK (10.0.26100). From a command prompt with these build tools added to the PATH, with working directory on the project folder, run:

```
mkdir build
cd build
cmake ../CMakeLists.txt
cmake --build .
```


## Command line

```
Usage: PoliteShutdown [/n | /w | /s | /t]

	/n	Check if conditions allow a shutdown, and if so shut down
	/w	Wait until conditions allow a shutdown, and then shut down
	/s	Schedule program to be run in Windows task scheduler
	/t	Schedule as /s, then check as /n
	/?	Display help
```
The times at which the program is scheduled to run are specified in group policy options. At the scheduled time it is run with option /t from the location it ran when the scheduled task was set up. The /t switch will not cause shutdown until the system has been running for a specified length of time.

The intended use is to copy politeshutdown.exe to each computer using group policy and then set a startup script running ``politeshutdown.exe /s``



## Settings

Group Policy settings available. If the ADM file is loaded these settings can be configured via the Group Policy Editor, though they will appear under a legacy policy settings section. Alternatively they can be set using registry values under `HKLM\Software\Policies\PoliteShutdown`

Beware that some policy settings are  "Turn off check..." where the underlying registry setting is "Check". This is where the checks are on by default so policy names need to indicate the setting must be configured to turn off the check.

### **Turn off shutdowns**
- **Registry Value:** `ShutdownsDisabled`
- **Description:** 
  - Turn off PoliteShutdown system shutdowns completely. Checks to see if the system is ready, and shutdowns, simulated or real, are not performed.
- **Values:**
  - `Enabled`: Shutdowns are disabled (`ShutdownsDisabled = 1`).
  - `Disabled`: Shutdowns are allowed (`ShutdownsDisabled = 0`).

### **Scheduled shutdown time**
- **Registry Values:**
  - `ShutdownHour`: The hour of the scheduled shutdown.
  - `ShutdownMinute`: The minute of the scheduled shutdown.
  - `ShutdownCutOffInterval`: Minutes after which shutdown checks cease.
- **Description:** 
  - Default cut off 10hrs. The cut off probably does not interact entirely sensibly if daylight savings time comes into effect while the program is running. If cut off is set to maximum, checks will continue indefinitely (even if the day actually has 25 hours).
- **Settings:**
  - `ShutdownHour`: Numeric value between 0-23. Default is `20` (8 PM).
  - `ShutdownMinute`: Numeric value between 0-59. Default is `0`.
  - `ShutdownCutOffInterval`: Numeric value between 1-1440. Default is `600` (10 hours).

### **Turn on first log**
- **Registry Values:**
  - `Log1`: Enables or disables the first log.
  - `Log1Path`: Path where the first log is stored.
  - `Log1Level`: Level of detail for the first log.
  - `Log1Detail`: Whether to include Windows error information in the log.
- **Description:** 
  - Logs operate independently. Note environment variables may be used in the log path.
- **Log Levels:**
  - `0`: Debug
  - `100`: Information
  - `150`: Error (Default)
  - `200`: Critical Error

### **Turn on second log**
- **Registry Values:**
  - `Log2`: Enables or disables the second log.
  - `Log2Path`: Path where the second log is stored.
  - `Log2Level`: Level of detail for the second log.
  - `Log2Detail`: Whether to include Windows error information in the log.
- **Description:** 
  - Logs operate independently. Note environment variables may be used in the log path.
- **Log Levels:**
  - `0`: Debug
  - `100`: Information
  - `150`: Error
  - `200`: Critical Error (Default)

### **Standard output settings**
- **Registry Values:**
  - `LogStdOutLevel`: Level of detail for standard output.
  - `LogStdOutDetail`: Whether to include Windows error information in the standard output.
- **Description:** 
  - The standard output is the text that appears at the command prompt. Note that disabling this policy will not disable the standard output: the default settings will be used. By default, information messages are logged, but not Windows error information.
- **Log Levels:**
  - `0`: Debug
  - `100`: Information (Default)
  - `150`: Error
  - `200`: Critical Error

### **Shutdown operation**
- **Registry Value:** `ShutdownType`
- **Description:** 
  - This setting determines what happens when the program has determined the computer is free to be shutdown. The default is an ordinary shutdown.
- **Options:**
  - `0`: Simulated Shutdown
  - `1`: Real Shutdown (Default)

### **Turn off check whether users are logged on**
- **Registry Value:** `CheckLoggedOnUsers`
- **Description:** 
  - Enable this policy to allow shutdown even if there are users logged on.
- **Values:**
  - `Enabled`: Check is disabled (`CheckLoggedOnUsers = 0`).
  - `Disabled`: Check is enabled (`CheckLoggedOnUsers = 1`).

### **Turn off check whether certain processes are running**
- **Registry Value:** `CheckProcesses`
- **Description:** 
  - Enable this policy to allow shutdown even if processes listed in the "Processes not to interrupt" policy are running.
- **Values:**
  - `Enabled`: Check is disabled (`CheckProcesses = 0`).
  - `Disabled`: Check is enabled (`CheckProcesses = 1`).

### **Turn off check whether scheduled tasks are running**
- **Registry Value:** `CheckRunningSchedTasks`
- **Description:** 
  - Enable this policy to allow shutdown even if scheduled tasks are running. Note that PoliteShutdown scheduled task created by using the /s task is not ignored even if this policy is disabled.
- **Values:**
  - `Enabled`: Check is disabled (`CheckRunningSchedTasks = 0`).
  - `Disabled`: Check is enabled (`CheckRunningSchedTasks = 1`).

### **Turn off check whether Windows update is in progress**
- **Registry Value:** `CheckWindowsUpdatesInProgress`
- **Description:** 
  - Enable this policy to allow shutdown even if Windows update reports updates in progress.
- **Values:**
  - `Enabled`: Check is disabled (`CheckWindowsUpdatesInProgress = 0`).
  - `Disabled`: Check is enabled (`CheckWindowsUpdatesInProgress = 1`).

### **Turn off check whether Windows updates need to be installed**
- **Registry Value:** `CheckWindowsUpdatesToBeInstalled`
- **Description:** 
  - Enable this policy to allow shutdown even if there are updates reported by Windows updates as needing installation.
- **Values:**
  - `Enabled`: Check is disabled (`CheckWindowsUpdatesToBeInstalled = 0`).
  - `Disabled`: Check is enabled (`CheckWindowsUpdatesToBeInstalled = 1`).

### **Turn off check whether Windows updates need to be uninstalled**
- **Registry Value:** `CheckWindowsUpdatesToBeUninstalled`
- **Description:** 
  - Enable this policy to allow shutdown even if there are updates reported by Windows updates as needing uninstallation.
- **Values:**
  - `Enabled`: Check is disabled (`CheckWindowsUpdatesToBeUninstalled = 0`).
  - `Disabled`: Check is enabled (`CheckWindowsUpdatesToBeUninstalled = 1`).

### **Consider users logged on even if running no processes**
- **Policy Name:** `LoggedOnNoProcess`
- **Registry Value:** `CheckLoggedOnByProcesses`
- **Description:** 
  - By default, logon sessions are considered to be stale and ignored if there are no processes belonging to that logon session. This policy disables this feature.
- **Values:**
  - `Enabled`: Disable check (`CheckLoggedOnByProcesses = 0`).
  - `Disabled`: Enable check (`CheckLoggedOnByProcesses = 1`).

### **Processes not to interrupt**
- **Registry Value:** `ProcessesNotToInterupt`
- **Description:** 
  - A list of processes which if running will cause shutdown to be delayed. The list should be separated by commas, leading and trailing whitespace is ignored. By default, the list is: msiexec.exe, package.exe, ntbackup.exe.
- **Example Setting:**
  - `ProcessesNotToInterupt`: `msiexec.exe, customprocess.exe`

### **Errors before giving up**
- **Registry Value:** `ErrorsBeforeEnd`
- **Description:** 
  - After this number of attempts to check if the system can be shutdown have caused errors, PoliteShutdown will give up and no longer make shutdown checks until the next day. By default, three attempts are made.
- **Setting:** 
  - Numeric value between 1-10000. Default is `3`.

### **Shutdown check frequency**
- **Registry Value:** `ShutdownCheckPeriod`
- **Description:** 
  - This setting determines the frequency at which checks to see if the system is ready to be shutdown are scheduled. The setting is the number of minutes between one check starting and the next starting. A lower number implies more processing load on the system, but a quicker response when the system is ready to be shutdown. By default, the interval is 15 minutes.
- **Setting:** 
  - Numeric value between 0-86400. Default is `300` (5 minutes).

### **System uptime required before shutdowns allowed**
- **Registry Value:** `ShutdownUptimeRequired`
- **Description:** 
  - If the system has been up and running for less than this number of minutes, shutdown will not occur regardless of other factors. This ensures the system is not shutdown while it is in startup, or before all services have started. If set to 0 minutes, PoliteShutdown will shutdown as soon as other checks allow. The default is 20 minutes.
- **Setting:** 
  - Numeric value between 0-86400. Default is `1200` (20 minutes).






