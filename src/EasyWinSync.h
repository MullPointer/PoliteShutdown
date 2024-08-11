#ifndef EASY_WIN_SYNC_H
#define EASY_WIN_SYNC_H

#include "EasyWin.h" //for CRITICAL_SECTION

namespace EasyWin {


//expected that Leave() called once for each Enter() before destruction
class CriticalSection
{
	public:
	CriticalSection(); //Windows Server 2003 and Windows XP/2000: can raise a STATUS_NO_MEMORY exception
	~CriticalSection();

	void Enter(); //can raise EXCEPTION_POSSIBLE_DEADLOCK - do not attempt to handle
	void Leave(); //must form a pair with a previous call of Enter()

	private:
	CriticalSection(const CriticalSection &); //no copy
	CriticalSection & operator=(const CriticalSection &);

	CRITICAL_SECTION m_critSecStruct;
};


class CritSectHandle //TODO better name
{
	public:
	CritSectHandle();
	CritSectHandle(CriticalSection & critSect);
	~CritSectHandle();

	void Enter(CriticalSection & critSect);
	void Leave();

	private:
	CritSectHandle(const CritSectHandle &); //no copy
	CritSectHandle & operator=(const CritSectHandle &); //no assignment

	CriticalSection * m_critSec;
};



class Event
{
	public:
	//see http://msdn.microsoft.com/en-us/library/ms682396(VS.85).aspx for details on name
	//if event of given name already exists, newly created object refers to that same event
	//throws WinException on error
	Event(bool manualReset = true, bool initiallySet = false, const std::wstring & name = L"");
	~Event();

	//set the event to signalled
	//throws WinException on error
	void Set();
	//reset the event to unsignalled
	//throws WinException on error
	void Reset();
	//wait for the event to be signalled
	//timeout is in milliseconds
	//returns true if event occurred, false if timed out
	//throws WinException on error
	bool Wait(DWORD timeout = INFINITE);
	//return whether is signalled
	//throws WinException on error
	bool IsSet();

	//return a handle to the associated windows event object
	//this handle will be closed when Event object destroyed
	HANDLE GetHandle() const;


	private:
	Event(const Event &); //no copy constructor  //TODO - should support copying using duplicate handle
	Event & operator=(const Event &); //no assignment

	HANDLE m_event;
	//possibly should also store name
};



} //end namespace EasyWin

#endif // EASY_WIN_SYNC_H

