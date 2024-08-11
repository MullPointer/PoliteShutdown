
#include <iostream>
#include <limits>
#include <vector>
#include <sstream>
#include <algorithm>
#include "auto_handle.h"

#include "EasyWin.h"
#undef max //stop MAX from windows.h stomping on max from limits

#define _WIN32_DCOM //for CoInitializeEx

namespace EasyWin
{
using namespace std;

WinException * WinException::Clone() const
{
	return new WinException(*this);
}

DWORD WinException::GetErrorCode() const
{
	return m_errorCode;
}

const string & WinException::GetMessage() const
{
	return m_message;
}

const char * WinException::what() const throw()
{
	return m_message.c_str();
}

const wstring & WinException::GetUnicodeMessage() const
{
	return m_unicodeMessage;
}

void WinException::SetErrorCode(DWORD errorCode)
{
	m_errorCode = errorCode;
}

void WinException::SetUnicodeMessage(const wstring & message)
{
	m_unicodeMessage = message;
}


void WinException::SetMessage(const string & message)
{
	m_message = message;
}





void CleanupMessageBuf(wchar_t * str)
{
	LocalFree(str);
}


void WinException::OutputSystemMessage(wostream & out) const
{
	const DWORD MIN_SYSTEM_ERROR_BUFFER_SIZE = 3;
	wchar_t * buf = NULL;
	DWORD result = FormatMessage(
					FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
					0,  //source
					m_errorCode,
					0,  //language ID
					(LPTSTR) &buf,
					MIN_SYSTEM_ERROR_BUFFER_SIZE, //minimum size
					NULL //arguments
					);
	auto_ptrx<wchar_t, CleanupMessageBuf> bufAutoCleanup(buf); //ensure buf freed even if exception thrown while message output

	if (result == 0) //if FormatMessage failed
	{
		out << L"System error message not found";
		//DEBUG - cout << endl << GetLastError();
	}
	else
	{
		out << buf;
	}
}

void WinException::OutputErrorCode(std::wostream & out) const
{
	out << m_errorCode;
	ios_base::fmtflags origFlags = out.flags(ios::hex | ios::showbase);
	out << L" (" << m_errorCode << L")";
	out.flags(origFlags);
}


void WinException::OutputCombinedMessage(wostream & out, bool incErrorCode, bool incSystemMessage) const
{
	//TODO - Think that outputing m_message.c_str() will work for most usual ASCII characters, but bit of a hack
	out << GetMessage().c_str();

	if (GetUnicodeMessage().length() > 0)
	{
		out << L"  Identifying information: " << GetUnicodeMessage();
	}

	if (GetErrorCode() && incErrorCode)
	{
		out << L"   Error code: ";
		OutputErrorCode(out);
	}

	if (GetErrorCode() && incSystemMessage)
	{
		out << L"   System error message: ";
		OutputSystemMessage(out);
	}
}


COMLibrary::COMLibrary():
	m_initialized(false) {}

COMLibrary::~COMLibrary()
{
	Uninitialize();
}

void COMLibrary::Initialize(DWORD initOptions)
{
	if (!m_initialized)
	{
		HRESULT hr = S_OK;
		// Call CoInitialize to initialize the COM library
		hr = CoInitializeEx(NULL, initOptions);
		if (FAILED(hr))
		{
			throw WinException("Failed to initialize COM Library", (DWORD) hr);
		}
		//DEBUG - cout << "COM Initialised \n";
		m_initialized = true;
	}
}

void COMLibrary::Uninitialize()
{
	if (m_initialized)
	{
		CoUninitialize();
		m_initialized = false;
		//DEBUG - cout << "COM Uninitialized \n";
	}
}



wstring GetProcessPath()
{
	const DWORD CUTOFF_SIZE = numeric_limits<DWORD>::max() / 4; //allocated size may not be more than twice this. Value is as large as possible but with no risk of wrap around on values below it being doubled
	DWORD allocatedSize = 50;
	DWORD actualSize = 0;
	vector<wchar_t> nameBuf;
	do
	{
		allocatedSize *= 2;
		nameBuf.assign(allocatedSize, (wchar_t) 0);
		actualSize = GetModuleFileName(NULL, &(nameBuf[0]), allocatedSize);
	} while (actualSize >= allocatedSize && allocatedSize <= CUTOFF_SIZE);

	if (actualSize >= allocatedSize)
	{
		throw WinException("File path of own process is too long to read", GetLastError()); //oddly error code is 0xb7 - which is not equal to ERROR_INSUFFICIENT_BUFFER as I think it should be
	}

	if (actualSize == 0)
	{
		throw WinException("Unable to obtain file path of own process", GetLastError());
	}

	return wstring(&(nameBuf[0]));
}


void RunProcess(const wstring & cmdLine, const wstring & workingDir)
{
	//cmdlines that are too long (32768 characters as stated at http://msdn.microsoft.com/en-us/library/ms682425(VS.85).aspx) are cropped to limit

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory( &pi, sizeof(pi) );

    vector<wchar_t> cmdLineW(cmdLine.length() + 1); //a writable copy of command line to satisfy CreateProcess, vector used for automatic memory management, +1 for null terminator
	copy(cmdLine.begin(), cmdLine.end(), cmdLineW.begin());
	cmdLineW[cmdLine.length()] = L'\0'; //null terminator

    const wchar_t * pWorkingDir = (workingDir.length() == 0) ? NULL : workingDir.c_str();

	BOOL success = CreateProcess(
					NULL, //application name specified in command name
					&cmdLineW[0],
					NULL, //process handle cannot be inherited by child processes
					NULL, //thread handle cannot be inherited by child processes
					FALSE, //newly created process does not inherit handles from creating process
					0, //no creation flags
					NULL, //environment of creating process used
					pWorkingDir,
					&si, //startup info
					&pi //process info returned
					);
	if (!success)
	{
		throw WinException("Failed to start new process requested", GetLastError(), cmdLine);
			//would be nice to include working dir in exception too
	}

	//close automatically opened handles to created process and thread
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread); //possibly should throw on failure
}



void RemoveLastPathComponent(wstring & path)
{
	size_t lastSlash = path.rfind(L"\\");
	if (lastSlash == wstring::npos)
	{
		path = L"";
	}
	else
	{
		path = path.substr(0, lastSlash);
	}
}


void printLUID(std::wostream & out, const LUID & luid)
{
	ios_base::fmtflags origFlags = out.flags(ios::hex);
	wchar_t origFill = out.fill(L'0');
	//out.width(8);
	out << luid.HighPart << L":";
	out.width(8);
	out << luid.LowPart;
	out.fill(origFill);
	out.flags(origFlags);
}


//TODO - could probably make more efficient and safer - does it work for all unicode characters?
wstring ToLower(const wstring & in)
{
	wostringstream ss;
	transform(in.begin(), in.end(), ostream_iterator<wchar_t, wchar_t>(ss), tolower);
	return ss.str();
}


DWORD GetSystemUptime()
{
	return GetTickCount() / 1000;
}


} //end namespace EasyWin
