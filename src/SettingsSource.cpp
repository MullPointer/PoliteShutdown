
#include "SettingsSource.h"
#include <iostream>
#include <sstream>

using namespace std;
using namespace EasyWin;




SettingsSource::SettingsSource():
	m_keyPath(L""), m_key(NULL)
{}

bool SettingsSource::SetKeyPath(const wstring & keyPath)
{
	CritSectHandle cs(m_critSect);
	auto_ptr<RegistryKey> newKey;
	try
	{
		newKey.reset(new RegistryKey(keyPath, WhetherCreateKey(), GetKeyAccess())); //create new key before replacing old or altering stored key path - an exception might be thrown
	}
	catch (WinException & e)
	{
		if (e.GetErrorCode() == 2) //if the error is that the key path is not found (error code 2)
		{
			return false;
		}
		else
		{
			e.SetMessage(string("Unable to access registry key for settings. ") + e.GetMessage());
			throw e;
		}
	}
	m_keyPath = keyPath;
	m_key = newKey;
	return true;
}

void SettingsSource::ReOpenKey()
{
	CritSectHandle cs(m_critSect);
	auto_ptr<RegistryKey> newKey(new RegistryKey(m_keyPath, WhetherCreateKey(), GetKeyAccess()));
	m_key = newKey;
}

wstring SettingsSource::GetString(const wstring & name)
{
	CritSectHandle cs(m_critSect);
	wstring value;
	bool valueSet = false;

	if (m_key.get())
	{
		try
		{
			valueSet = m_key->RetrieveValueString(value, name);
		}
		catch (WinException & e)
		{
			if (e.GetErrorCode() == ERROR_KEY_DELETED) //the key has been deleted, it may have been deleted and recreated by a group policy update in which case we can recover by reopening
			{
				ReOpenKey();
				valueSet = m_key->RetrieveValueString(value, name);
			}
			else
			{
				throw;
			}
		}
	}

	if (!valueSet) //if we haven't got the value yet, use a default
	{
		map<wstring,wstring>::const_iterator def = m_stringDefaults.find(ToLower(name));
		if (def != m_stringDefaults.end())
		{
			value.assign(def->second);
		}
	}

	return value;
}

DWORD SettingsSource::GetInt(const wstring & name)
{
	CritSectHandle cs(m_critSect);
	DWORD value = 0;
	bool valueSet = false;

	if (m_key.get())
	{
		try
		{
			valueSet = m_key->RetrieveValueDWORD(value, name);
		}
		catch (WinException & e) //wish I could think of a good way to factor this bit out
		{
			if (e.GetErrorCode() == ERROR_KEY_DELETED) //see above note on key deletions
			{
				ReOpenKey();
				valueSet = m_key->RetrieveValueDWORD(value, name);
			}
			else
			{
				throw;
			}
		}
	}

	if (!valueSet) //if we haven't got the value yet, use a default
	{
		map<wstring,DWORD>::const_iterator def = m_intDefaults.find(ToLower(name));
		if (def != m_intDefaults.end())
		{
			value = def->second;
		}
	}

	return value;
}

void SettingsSource::SetStringDefault(const std::wstring & name, const std::wstring & value)
{
	CritSectHandle cs(m_critSect);
	m_stringDefaults[ToLower(name)] = value;
}

void SettingsSource::SetIntDefault(const std::wstring & name, DWORD value)
{
	CritSectHandle cs(m_critSect);
	m_intDefaults[ToLower(name)] = value;
}



void SettingsStore::SetString(const wstring & name, const wstring & value)
{
	CritSectHandle cs(m_critSect);
	if (m_key.get())
	{
		try
		{
			m_key->SetValue(name, value);
		}
		catch (WinException & e)
		{
			if (e.GetErrorCode() == ERROR_KEY_DELETED) //see above note on key deletions
			{
				ReOpenKey();
				m_key->SetValue(name, value);
			}
			else
			{
				throw;
			}
		}
	}
}

void SettingsStore::SetInt(const wstring & name, DWORD value)
{
	CritSectHandle cs(m_critSect);
	if (m_key.get())
	{
		try
		{
			m_key->SetValue(name, value);
		}
		catch (WinException & e)
		{
			if (e.GetErrorCode() == ERROR_KEY_DELETED) //see above note on key deletions
			{
				ReOpenKey();
				m_key->SetValue(name, value);
			}
			else
			{
				throw;
			}
		}
	}
}

