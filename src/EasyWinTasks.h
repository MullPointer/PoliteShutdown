
#ifndef EASYWINTASKS_H
#define EASYWINTASKS_H


#include "EasyWin.h" //must come before mstask.h to ensure UNICODE defined
#include <initguid.h> //must come before mstask.h
#include <mstask.h>
#include <string>
#include <vector>
#include <exception>

namespace EasyWin {

//Note, as a library, this still has many obvious ommissions

//Note trying to use an invalid object will cause an exception to be thrown
//though this is a runtime error I felt there was too much risk of it appearing in release to rely on cassert assertions

class Scheduler;
class Task;
class TaskTrigger;
class TaskTriggerTiming;
class SchedulerException;


enum TaskStatus
{
	ANY,
	READY = (int) SCHED_S_TASK_READY,
	RUNNING = (int) SCHED_S_TASK_RUNNING,
	NOT_SCHEDULED = (int) SCHED_S_TASK_NOT_SCHEDULED,
	HAS_NOT_RUN = (int) SCHED_S_TASK_HAS_NOT_RUN,
	TASK_DISABLED = (int) SCHED_S_TASK_DISABLED,
	NO_MORE_RUNS = (int) SCHED_S_TASK_NO_MORE_RUNS,
	NO_VALID_TRIGGERS = (int) SCHED_S_TASK_NO_VALID_TRIGGERS
};
//I probably shouldn't store the constants in this way, but it avoids a conversion function


/**
* Manages an interface to a computer's task scheduler.
*/
class Scheduler
{
	public:
	/**
	* Creates a scheduler targeted on the local computer
	*/
	Scheduler();
	Scheduler(const Scheduler & copied);
	~Scheduler();
	Scheduler & operator=(const Scheduler & copied);

	/**
	* Return a reference to the underlying ITaskScheduler pointer
	* The reference count on the pointer is incremented, so the pointer must be released
	* with its Release() method.
	*/
	ITaskScheduler * GetITaskScheduler() const; //not sure this should be const
	/**
	* Release the underlying ITaskScheduler pointer.
	* Leaving the Scheduler in an unusable state
	* - further methods of the receiver must not be called until another scheduler is assigned to it.
	*/
	void Release();

	/**
	* Appends the names of all tasks to the given vector.
	* \throw SchedulerException on failure to retrieve the task names
	*/
	void GetTaskNames(std::vector<std::wstring>& taskNames);
	/**
	* Appends Task objects representing all tasks to the given tasks vector.
	* \param status append only those tasks with the given status, or any status if ANY specified
	* \throw SchedulerException on failure to retrieve the task names or task objects
	*/
	void GetTasks(std::vector<Task>& tasks, TaskStatus status = ANY);

	/**
	* Returns a Task object corresponding with the specified file name.
	* If no task with the given name exists, returns an invalid task object,
	* the isValid method of which will return false.
	* \throw SchedulerException on other failures
	*/
	Task GetTask(const std::wstring & taskName);
	/**
	* Returns a new Task object with the given file name.
	* Note that the task file is not actually created until the Save method of the Task object is called.
	* This means it is possible for a separate task of the given file name to be created and saved before this one.
	* \throw SchedulerException on failure to create new task, including if there is already a task with the specified file name.
	*/
	Task NewTask(const std::wstring & taskName);


	protected:
	//throw an exception if m_pTS is null
	void AssertValid() const;

	private:
	ITaskScheduler * m_pTS;
	static const int BATCH_TO_RETRIEVE = 10; //number of task names to retrieve in each batch when calling IEnumWorkItems::Next

};



//Task object represents a handle on a snapshot of information on task at time object created using Scheduler::GetTask
class Task
{
	public:
	Task();
	Task(ITask * pITask, const std::wstring & name); //note does not increment pITask reference count - takes ownership of the pointer passed
	Task(const Task & copied);
	~Task();
	Task & operator=(const Task & copied);

	//increments ITask ref count
	ITask * GetITask() const;

	void Release();

	bool IsValid() const;
	std::wstring GetName() const;
	TaskStatus GetStatus() const;

	WORD GetTriggerCount() const;
	void GetTriggers(std::vector<TaskTrigger>& taskTriggers);
	TaskTrigger CreateTrigger(WORD & triggerIndex);
	TaskTrigger CreateTrigger(); //discards the taskIndex
	void DeleteTrigger(WORD triggerIndex);
	void DeleteTrigger(TaskTrigger trigger); //search for trigger then delete
	void DeleteAllTriggers();

	void SetApplicationName(std::wstring applicationName);
	std::wstring GetApplicationName() const;
	void SetWorkingDirectory(std::wstring workingDir = L"");
	std::wstring GetWorkingDirectory() const;
	//command line parameters
	void SetParameters(std::wstring parameters = L"");
	std::wstring GetParameters() const;

	//default is for local account
	//TODO this does not handle the setting of a null password if have TASK_FLAG_RUN_ONLY_IF_LOGGED_ON set
	void SetAccountInformation(std::wstring userName = L"", std::wstring password = L""); //note insecure as does not zero password memory after use
	//returns SYSTEM_ACCOUNT_NAME for local system account
	std::wstring GetAccountName() const;
	static const std::wstring SYSTEM_ACCOUNT_NAME; //value L""

	//flags is a bitfield - see http://msdn.microsoft.com/en-us/library/aa381283(VS.85).aspx
	void SetFlags(DWORD flags);
	DWORD GetFlags() const;

	//time task is allowed to run before being forceably terminated
	void SetMaxRunTime(DWORD millisecondsRunTime);
	DWORD GetMaxRunTime() const;

	void Run();
	void Terminate();
	void Save();

	protected:
	//throw an exception if m_pITask is null
	void AssertValid() const;

	ITask * m_pITask;
	std::wstring m_name;
};

//output name
std::wostream & operator<< (std::wostream & out, const Task & task);


//handle of a trigger
class TaskTrigger
{
	public:
	TaskTrigger();
	explicit TaskTrigger(ITaskTrigger * pITaskTrigger); //note does not increment pITaskTrigger reference count - takes ownership of the pointer passed
	TaskTrigger(const TaskTrigger & copied);
	~TaskTrigger();
	TaskTrigger & operator=(const TaskTrigger & copied);

	//increments ITaskTrigger ref count
	ITaskTrigger * GetITaskTrigger() const;

	void Release();

	//custom trigger from struct - see http://msdn.microsoft.com/en-us/library/aa383618(VS.85).aspx
	//note if set start date earlier than it can cope with 1900? get a task file in the task folder that does not appear in the list of tasks. Though if you are running this program in 1900, you probably have other problems.
	void Set(const TASK_TRIGGER & triggerStruct);
	void Set(const TaskTriggerTiming & triggerTiming);

	TASK_TRIGGER GetTriggerStruct() const;
	std::wstring GetTriggerString() const;

	bool HasEquivalentTiming(const TaskTriggerTiming & timing);

	protected:
	//throw an exception if m_pITaskTrigger is null
	void AssertValid() const;

	ITaskTrigger * m_pITaskTrigger;
	friend Task;
};

//wrapper for TASK_TRIGGER struct
// possible timing information for a task trigger
class TaskTriggerTiming
{
	public:
	TaskTriggerTiming();
	explicit TaskTriggerTiming(const TASK_TRIGGER & trigStruct);
	//default destrution and copy

	//resets trigger as a daily trigger starting from now
	void SetDaily(WORD startHour, WORD startMinute, WORD daysInterval = 1);

	//set the existing trigger to repeat after its trigger times
	//repeat at intervals of minutesInterval for a duration of minutesDuration
	//throws if minutesDuration <= minutesInterval
	void AddRepeats(DWORD minutesInterval, DWORD minutesDuration, bool killAtEndDuration = false);

	DWORD GetRepeatInterval();
	DWORD GetRepeatDuration();

	//returns whether the triggers are identical in all but properties that are ignored for that type of task
	bool operator==(const TaskTriggerTiming & other) const;

	//returns whether the triggers are functionally identical at the current time
	//main difference from == is that if the day set to begin is in past in both tasks, they are treated as the same
	// similarly with the end day
	//Behaviour could use testing and improvement
	bool Equivalent(const TaskTriggerTiming & other) const;

	//return wrapped TASK_TRIGGER
	const TASK_TRIGGER & GetTASK_TRIGGER() const;


	protected:
	//returns whether
	bool EqualExceptBeginAndEndDay(const TaskTriggerTiming & other) const;

	//if both begin times of receiver and other are in the past returns true, otherwise returns true only if receiver and other set to begin on same day
	bool HasEquivalentBeginDay(const TaskTriggerTiming & other) const;

	//if both end times of receiver and other are in the past returns true, otherwise returns true only if receiver and other set to end on same day
	bool HasEquivalentEndDay(const TaskTriggerTiming & other) const;

	private:
	TASK_TRIGGER m_trigStruct;
};



class SchedulerException: public WinException
{
	public:
	//takes errorCode as HRESULT to agree with error code type of scheduler functions
	SchedulerException(std::string message, HRESULT errorCode = (HRESULT) 0, std::wstring unicodeMessage = L""):
		WinException(message, (DWORD) errorCode, unicodeMessage) {}
	virtual WinException * Clone() const;
};


class IsTaskNamed
{
	public:
	IsTaskNamed(const std::wstring & taskName):
		m_taskName(taskName) {}

	bool operator() (const Task & task)
	{
		return m_taskName == task.GetName();
	}

	private:
	std::wstring m_taskName;
};

} //end namespace EasyWin

#endif // EASYWINTASKS_H
