#include "EasyWin.h"
#include <Sddl.h> //for ConvertSidToStringSid
#include "EasyWinSID.h"
#include "auto_handle.h"

using namespace std;
using namespace EasyWin;

SIDclass::SIDclass(PSID pSID): m_buf(NULL), m_length(0)
{
	Set(pSID);
}

SIDclass::SIDclass(const SIDclass & copied): m_buf(NULL), m_length(0)
{
	Set(copied.m_buf, copied.m_length);
}


SIDclass::~SIDclass()
{
	Release();
}

SIDclass & SIDclass::operator=(const SIDclass & copied)
{
	Set(copied.m_buf, copied.m_length);
	return *this;
}

void CleanupSidToStringBuf(wchar_t * str)
{
	LocalFree(str);
}

wstring SIDclass::ToString() const
{
	wchar_t * str = NULL;
	wstring wStr;

	BOOL result = ConvertSidToStringSid(m_buf, &str);
	if (!result)
	{
		throw WinException("Unable to represent SID as string", GetLastError());
	}
	auto_ptrx<wchar_t, CleanupSidToStringBuf> strAutoCleanup(str); //ensure that system allocated string cleaned up, even if append throws exception

	wStr.append(str);
	return wStr;
}

void SIDclass::Set(PSID pSID)
{
	if (!IsValidSid(pSID))
	{
		throw WinException("Attempt to handle an invalid SID");
	}
	Set(pSID, GetLengthSid(pSID));
}

void SIDclass::Set(PSID pSID, unsigned int length)
{
	Release(); //not sure if should throw here if memory not released
	m_buf = (PSID) HeapAlloc(GetProcessHeap(),
				HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY,
				length);
	m_length = length;
	BOOL result = CopySid(length, m_buf, pSID);
	if (!result)
	{
		Release();
		throw WinException("Unable to copy SID", GetLastError());
	}
}

PSID SIDclass::GetPSID()
{
	return m_buf;
}

bool SIDclass::operator==(const SIDclass & o)
{
	return (EqualSid(m_buf, o.m_buf) != 0);
}

bool SIDclass::operator!=(const SIDclass & o)
{
	return !((*this)==o);
}

bool SIDclass::Release()
{
	BOOL ret = TRUE;
	if(m_buf)
	{
		ret = HeapFree(GetProcessHeap(), 0, m_buf);
		m_buf = NULL;
	}
	return (ret != 0);
}
