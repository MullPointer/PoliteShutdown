#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <memory>
#include <limits>
#include <iosfwd>
#include "EasyWin.h"
#undef max //to prevent max macro from windows.h stomping on the numeric_limits::max() method
#include "EasyWinSync.h"

//TODO
//method to allow retrieval of exceptions for individual logs
	//function to access LogTargets - would need to clean them up for public access
//possibly would be better taking streams, but I think I want to close the file between logging
//possibly add some kind of buffering so fewer file opens
//would be nice to make this completely generic - separate from EasyWin - could make a Writable class which gives message based on level of detail given
//possibly should indicate on error messages the level of the message - error, warning etc
//ensure no possibility of Log throwing - should use throw() on Log?
//LogFailedException should perhaps include name of log in main message, not just in extended information


//never actually publically thrown - stored as last error
class LogFailedException: public EasyWin::WinException
{
	public:
	LogFailedException(const std::string & message, const std::wstring & logPath, DWORD errorCode):
		WinException(message, errorCode, logPath), m_systemMessage(""), m_errorCodeCRT(0) {}
	LogFailedException(const std::string & message, const std::wstring & logPath, const std::string & systemMessage):
		WinException(message, 0, logPath), m_systemMessage(systemMessage), m_errorCodeCRT(0)  {}
	LogFailedException(const std::string & message, const std::wstring & logPath, errno_t errorCode):
		WinException(message, 0, logPath), m_systemMessage(""), m_errorCodeCRT(errorCode) {}
	//default copy constructor and assignment
	virtual WinException * Clone() const; //allow polymorphic copying
	virtual ~LogFailedException();

	//outputs the explicitly specified system message if one specified,
	// otherwise if m_errorCodeCRT is non-zero outputs error message based on that
	// otherwise outputs the windows error message based on errorCode
	virtual void OutputSystemMessage(std::wostream & out) const;

	//outputs error message adapted to additional information stored
	virtual void OutputCombinedMessage(std::wostream & out, bool incErrorCode = false, bool incSystemMessage = false) const;

	//outputs CRT error code if there is one rather than windows error code
	virtual void OutputErrorCode(std::wostream & out) const;

	virtual void OutputCRTMessage(std::wostream & out) const;
	virtual errno_t GetErrorCodeCRT() const;

	private:
	std::string m_systemMessage;
	errno_t m_errorCodeCRT;
};


//thread-safe
class Logger
{
	public:
	Logger(){}
	~Logger(){}

	enum LogDetail
	{
		LOW_DETAIL,
		FULL_DETAIL
	};
	static const int NO_LOG_ON_LOG_ERROR;
	/**
	* Add a file name of target file to append any message with level at least that specified.
	* empty fileName indicates standard output
	* @param detail level of detail output if an exception object is passed
	* @param logErrorLevel level of error messages automatically posted to logs the first time a log fails.
	* 		If set to NO_LOG_ON_LOG_ERROR no messages are logged.
	*		Further log instructions will still attempt to write to the failed log, but no further message will be posted to other logs if these attempts fail.
	*/
	void AddTarget(const std::wstring & fileName, int level, LogDetail detail, int logErrorLevel = NO_LOG_ON_LOG_ERROR);

	//new line at start of each entry, and not after
	//returns whether succeeds in writing to all logs
	//nothing happens if no log targets set
	//should not throw
	bool Log(const std::wstring & message, int level);
	bool Log(const std::wstring & message, const EasyWin::WinException & winExp, int level);
	bool Log(const EasyWin::WinException & winExp, int level);

	private:
	Logger(const Logger &); //no copy constructor
	Logger & operator=(const Logger &); //no assignment

	struct LogTarget
	{
		LogTarget() {}
		LogTarget(const LogTarget & other);
		LogTarget & operator=(const LogTarget & other);
		~LogTarget() {}

		std::wstring name;
		int level;
		LogDetail detail;

		bool failing; //whether writing to this log failed on the last attempt - used to determine whether a log failure is the first (or first after a successful log) so should itself be logged
		int logErrorLevel; //level at which errors in writing log are themselves logged
		std::auto_ptr<EasyWin::WinException> logError; //pointer to exception that occurred on last log attempt to this target - null if succeeded
	};
	bool Log(const std::wstring * message, const EasyWin::WinException * winExp, int level);

	std::vector<LogTarget> m_logTargets;
	EasyWin::CriticalSection m_critSect;
};


#endif // LOGGER_H

