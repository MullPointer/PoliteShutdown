#ifndef EASYWIN_H
#define EASYWIN_H


#include <string>
#include <exception>
#include <iosfwd>
#define UNICODE
#include <windows.h>
#include "auto_handle.h"

#undef GetMessage //stop windows.h define stomping on WinException method

//TODO - nicer string conversion in OutputCombinedMessage
	// add OutputMessage(wostream) function - MultiByteToWideChar - http://www.codeproject.com/KB/string/UtfConverter.aspx
	// might also be able to use widen function of ostream
//TODO - option to not include hex version of error code
//TODO - not really sure WinException should have setter methods


namespace EasyWin {

class WinException: public std::exception
{
	public:
	explicit WinException(std::string message, DWORD errorCode = (DWORD) 0, std::wstring unicodeMessage=L""):
		m_message(message), m_errorCode(errorCode), m_unicodeMessage(unicodeMessage) {}
	//default copy and assignment
	virtual WinException * Clone() const; //allow polymorphic copying
	virtual ~WinException() {}

	virtual DWORD GetErrorCode() const;
	virtual const std::string & GetMessage() const;
	virtual const std::wstring & GetUnicodeMessage() const;
	virtual const char * what() const throw();

	virtual void SetErrorCode(DWORD errorCode);
	virtual void SetMessage(const std::string & message);
	virtual void SetUnicodeMessage(const std::wstring & message);

	//outputs the windows message for the error code obtained using FormatMessage - http://msdn.microsoft.com/en-us/library/ms679351(VS.85).aspx
	//outputs "system error message not found" if FormatMessage fails
	virtual void OutputSystemMessage(std::wostream & out) const;

	//outputs the error code in decimal and hex
	virtual void OutputErrorCode(std::wostream & out) const;

	//output composite message of message and unicode message suitable for error log
	// with incErrorCode true and a non-zero error code, appends error code
	// with incSystemMessage true and a non-zero error code, appends windows message for the error code
	//assumes message has no full stop, but that system error messages do.
	virtual void OutputCombinedMessage(std::wostream & out, bool incErrorCode = false, bool incSystemMessage = false) const;

	protected:
	std::wstring m_unicodeMessage;
	std::string m_message;
	DWORD m_errorCode;
};


//an instance should be created and have initialize method run in each thread before any other COM commands used
//Initialize method throws exception on failure
//should be destructed after all COM resources released - will be destroyed last if created on the stack before any other objects
class COMLibrary
{
	public:
	COMLibrary();
	~COMLibrary();
	void Initialize(DWORD initOptions = COINIT_APARTMENTTHREADED);
	void Uninitialize();
	private:
	COMLibrary(const COMLibrary &); //NO COPYING
	COMLibrary & operator=(const COMLibrary &);
	bool m_initialized;
};

inline void CleanupIUnknownPtr(IUnknown * p)
{
	p->Release();
}
//an IUnknown Ptr that is automatically released on leaving scope
typedef auto_ptrx<IUnknown, CleanupIUnknownPtr> AutoIUnknownPtr;


//return the file name of the current process
std::wstring GetProcessPath();

//Start a process
//do not wait for process to exit
//cmdLine must not have a path to executable longer than MAX_PATH
//empty working dir implies same working dir as current process
void RunProcess(const std::wstring & cmdLine, const std::wstring & workingDir = L"");

//remove the last component of the given path including the slash
//assume backslashes only
//if path has only one component return empty string
void RemoveLastPathComponent(std::wstring & path);

//Print an LUID to given output stream
void printLUID(std::wostream & out, const LUID & luid);

std::wstring ToLower(const std::wstring & in);

//return system uptime in seconds
//owing to the limitations of GetTickCount() this will wrap around and reset to 0 after 49.7 days continuous uptime
//should probably be rewritten to use performance counter
DWORD GetSystemUptime();

} //end namespace EasyWin


#endif // EASYWIN_H
