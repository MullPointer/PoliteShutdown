
#include "EasyWinSync.h"

using namespace std;

namespace EasyWin {

//CriticalSection IMPLEMENTATIONS

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(&m_critSecStruct);

}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&m_critSecStruct);
}

void CriticalSection::Enter()
{
	EnterCriticalSection(&m_critSecStruct);
}

void CriticalSection::Leave()
{
	LeaveCriticalSection(&m_critSecStruct);
}






//CritSectHandle IMPLEMENTATIONS
CritSectHandle::CritSectHandle():
	m_critSec(NULL)
{}

CritSectHandle::CritSectHandle(CriticalSection & critSect):
	m_critSec(NULL)
{
	Enter(critSect);
}


CritSectHandle::~CritSectHandle()
{
	Leave();
}


void CritSectHandle::Enter(CriticalSection & critSect)
{
	critSect.Enter();
	m_critSec = &critSect;
}

void CritSectHandle::Leave()
{
	if (m_critSec)
	{
		m_critSec->Leave();
		m_critSec = NULL;
	}
}



//Event IMPLEMENTATIONS
Event::Event(bool manualReset, bool initiallySet, const std::wstring & name)
{
	m_event = CreateEvent(NULL, manualReset, initiallySet, name.c_str());
	if (!m_event)
	{
		throw WinException("Failed to create synchronization event", GetLastError(), name);
	}
}

Event::~Event()
{
	CloseHandle(m_event);
}

void Event::Set()
{
	BOOL success = SetEvent(m_event);
	if (!success)
	{
		throw WinException("Failed to set synchronization event to signalled", GetLastError());
	}
}

void Event::Reset()
{
	BOOL success = ResetEvent(m_event);
	if (!success)
	{
		throw WinException("Failed to set synchronization event to unsignalled", GetLastError());
	}
}

bool Event::Wait(DWORD timeout)
{
	DWORD result = WaitForSingleObject(m_event, timeout);
	switch(result)
	{
		case WAIT_OBJECT_0:
			return true;
		case WAIT_TIMEOUT:
			return false;
		default: //should be only when value is WAIT_FAILED
			throw WinException("Failed to wait or check status of synchronization event", GetLastError());
	}
}

bool Event::IsSet()
{
	return Wait(0);
}


HANDLE Event::GetHandle() const
{
	return m_event;
}



} //end namespace EasyWin
