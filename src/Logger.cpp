
#include "Logger.h"
#include <fstream>
#include <iostream>
#include <ctime>


using namespace std;
using namespace EasyWin;

const int Logger::NO_LOG_ON_LOG_ERROR = std::numeric_limits<int>::max();


void Logger::AddTarget(const wstring & fileName, int level, LogDetail detail, int logErrorLevel)
{
	CritSectHandle cs(m_critSect);
	LogTarget lt;
	lt.name = fileName;
	lt.level = level;
	lt.detail = detail;
	lt.failing = false;
	lt.logErrorLevel = logErrorLevel;
	lt.logError.reset(NULL);
	m_logTargets.push_back(lt);
}



bool Logger::Log(const wstring & message, int level)
{
	return Log(&message, NULL, level);
}


bool Logger::Log(const WinException & winExp, int level)
{
	return Log(NULL, &winExp, level);
}


bool Logger::Log(const wstring & message, const WinException & winExp, int level)
{
	return Log(&message, &winExp, level);
}


void WriteTimeStamp(wostream & out)
{
	//TODO I don't think this is best way to get time - function depreciated - perhaps use windows functions
	SYSTEMTIME systemTime, localTime;
    GetSystemTime(&systemTime);
    GetLocalTime(&localTime);

	out << (unsigned int) localTime.wDay << L"/";
	out << (unsigned int) localTime.wMonth << L"/";
	out << (unsigned int) localTime.wYear << L" ";

	//streamsize origWidth = out.width();
	wchar_t origFill = out.fill();
	ios_base::fmtflags origFlags = out.flags();

	out.fill(L'0');
	out << right;
	out.width(2);
	out << (unsigned int) localTime.wHour << L":";
	out.width(2);
	out << (unsigned int) localTime.wMinute << L":";
	out.width(2);
	out << (unsigned int) localTime.wSecond;

	//out.width(origWidth);
	out.fill(origFill);
	out.flags(origFlags);
}

//throws wostream::failure on write failure
void Write(wostream & out, const wstring * message, const WinException * winExp, Logger::LogDetail detail)
{
	ios_base::iostate origExceptionMask = out.exceptions();
	out.exceptions(wostream::failbit | wostream::badbit);

	out << L"\n"; //endl is not a valid unicode line terminator
	WriteTimeStamp(out);
	out << L" ";

	if (message)
	{
		out << *message;
	}
	else
	{
		out << L"Error: ";
	}

	//DEBUG - static int a = 1;
	//DEBUG - ++a;
	//DEBUG - if ((a % 10) == 0) throw wostream::failure("Test failure");

	if (winExp)
	{
		if (detail == Logger::LOW_DETAIL)
		{
			winExp->OutputCombinedMessage(out, false, false);
		}
		else
		{
			winExp->OutputCombinedMessage(out, true, true);
		}
	}

	out.exceptions(origExceptionMask);
}


bool Logger::Log(const wstring * message, const WinException * winExp, int level)
{
	CritSectHandle cs(m_critSect);
	bool isSuccess = true;
	for (vector<LogTarget>::iterator i = m_logTargets.begin(); i != m_logTargets.end(); ++i)
	{
		try
		{
			if (level >= i->level) //record only sufficiently high level messages for the target
			{
				if (i->name.length() == 0) //if name is empty
				{
					//TODO - logging to std output will fail if characters not in standard encoding used
					Write(wcout, message, winExp, i->detail);
				}
				else
				{
					//using _wfopen_s is really ugly, but I can't find any other way to cope with unicode file names
					FILE * pFile = NULL;
					errno_t error = _wfopen_s(&pFile, i->name.c_str(), L"at, ccs=UTF-16LE");
					if (error != 0 || pFile == NULL)
					{
						throw LogFailedException("Failed to open log file", i->name, error);
					}

					try //ensure pFile closed
					{
						wofstream of(pFile);
						Write(of, message, winExp, i->detail);
					}
					catch(exception &)
					{
						if (pFile) fclose(pFile);
						throw;
					}
					if (pFile) error = fclose(pFile);
					if (error != 0)
					{
						//questionable should treat this as a full blown error - but useful for debugging purposes
						throw LogFailedException("Failed to close log file", i->name, error);
					}
				}
				i->failing = false; //if we reached here no exception has been thrown, so we succeeded
			}

		}
		catch(WinException & e)
		{
			i->logError.reset(e.Clone());
			isSuccess = false;
		}
		catch(exception & e)
		{
			i->logError.reset(new LogFailedException("Failed to write message to a log", i->name, e.what()));
			isSuccess = false;
		}

	}

	for (vector<LogTarget>::iterator i = m_logTargets.begin(); i != m_logTargets.end(); ++i)
	{
		if (!(i->failing) && i->logError.get()) //ie if this is first failure of log or first failure after success
		{
			i->failing = true;
			auto_ptr<WinException> logError(i->logError.release()); //need to transfer pointer to our control as logging the error may change i->logError (and destroy exception we are trying to record) when an attempt is made to log to target i
			Log(*(logError), i->logErrorLevel); //this log attempt will probably fail to the log whose failure we wish to record, however this failure will not itself be logged as variable failing now true for that target
		}
	}

	return isSuccess;
}

/////////////////////////////
//LogTarget IMPLEMENTATIONS//
/////////////////////////////
Logger::LogTarget::LogTarget(const LogTarget & other):
	name(other.name), level(other.level), detail(other.detail), failing(other.failing),
	logErrorLevel(other.logErrorLevel), logError(NULL)
{
	if (other.logError.get())
	{
		logError.reset(other.logError->Clone()); //ensure that other LogTarget's logError is copied (polymorphically) and not just moved
	}
}

Logger::LogTarget & Logger::LogTarget::operator=(const LogTarget & other)
{
	name = other.name;
	level = other.level;
	detail = other.detail;
	failing = other.failing;
	logErrorLevel = other.logErrorLevel;
	if (other.logError.get())
	{
		logError.reset(other.logError->Clone()); //ensure that other LogTarget's logError is copied (polymorphically) and not just moved
	}
	else
	{
		logError.reset(NULL);
	}
	return *this;
}


//////////////////////////////////////
//LogFailedException IMPLEMENTATIONS//
//////////////////////////////////////
LogFailedException::~LogFailedException()
{
	//DEBUG - cout << "Destroying LogFailedExp: " << this << endl;
	//DEBUG - cout.flush();
}

WinException * LogFailedException::Clone() const
{
	return new LogFailedException(*this);
}

void LogFailedException::OutputSystemMessage(std::wostream & out) const
{
	if (m_systemMessage == "")
	{
		if (GetErrorCodeCRT())
		{
			OutputCRTMessage(out);
		}
		else
		{
			if (GetErrorCode())
			{
				WinException::OutputSystemMessage(out);
			}
			else
			{
				out << L"no system message";
			}
		}
	}
	else
	{
		out << m_systemMessage.c_str(); //widening hack
	}
}

void LogFailedException::OutputCRTMessage(std::wostream & out) const
{
	const size_t bufferSize = 256; //this is assumed large enough as _wcserror_s provides no feedback if it isn't - message is truncated if this is not large enough
	wchar_t buffer[bufferSize];
	errno_t result = _wcserror_s(buffer, bufferSize, GetErrorCodeCRT());
	if (result == 0) //on success
	{
		out << buffer;
	}
	else
	{
		out << L"failed to obtain system error message - attempt failed with error code: " << result;
	}

}

void LogFailedException::OutputCombinedMessage(std::wostream & out, bool incErrorCode, bool incSystemMessage) const
{
	out << GetMessage().c_str() << L"."; //Widening hack
	if (GetUnicodeMessage() == L"")
	{
		out << L" Logging to standard output failed.";
	}
	else
	{
		out << L" Log Path: " << GetUnicodeMessage();
	}

	if (incErrorCode && (GetErrorCodeCRT() || GetErrorCode()))
	{
		out << L"   Error code: ";
		OutputErrorCode(out);
	}

	if (incSystemMessage)
	{
		out << L"   System error message: ";
		OutputSystemMessage(out);
	}
}

void LogFailedException::OutputErrorCode(std::wostream & out) const
{
	if (GetErrorCodeCRT())
	{
		out << GetErrorCodeCRT();
		ios_base::fmtflags origFlags = out.flags(ios::hex | ios::showbase);
		out << L" (" << GetErrorCodeCRT() << L")";
		out.flags(origFlags);
	}
	else
	{
		WinException::OutputErrorCode(out);
	}
}

errno_t LogFailedException::GetErrorCodeCRT() const
{
	return m_errorCodeCRT;
}

