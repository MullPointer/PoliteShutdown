#include "EasyWinReg.h"
#include <iostream>
#include "auto_handle.h"
#include <cassert>
#include <limits>

using namespace EasyWin;
using namespace std;


#define CREATED_NEW_KEY 0x00000001L
	//as specified http://msdn.microsoft.com/en-us/library/ms724844(VS.85).aspx - not sure what header am supposed to get from
#undef max  //from windows.h, as are going to use max method of numeric_limits

RegistryKey::RegistryKey():
	m_key(NULL)
{}


RegistryKey::RegistryKey(wstring path, bool create, REGSAM access):
	m_key(NULL)
{
	if (create)
	{
		Create(path, access);
	}
	else
	{
		if(!Open(path, access))
		{
			throw KeyNotFoundException("Registry key requested to be opened does not exist", (DWORD) ERROR_FILE_NOT_FOUND, path);
		}
	}
}


RegistryKey::~RegistryKey()
{
	try
	{
		Close();
	}
	catch(exception &)
	{} //eat all exceptions - as do not want to propagate exceptions during destruction
}


//also modifies path and returns rootName
HKEY ExtractRootKey(wstring & path, wstring & rootName)
{
	size_t slashPos = path.find('\\');
	if (slashPos == string::npos)
	{
		rootName = path;
		path = L"";
	}
	else
	{
		rootName = path.substr(0, slashPos);
		path = path.substr(slashPos + 1);
	}

	HKEY rootKey = NULL;
	if(rootName == wstring(L"HKEY_CLASSES_ROOT") || rootName == wstring(L"HKC"))
	{
		rootKey = HKEY_CLASSES_ROOT;
	}
	else if (rootName == wstring(L"HKEY_CURRENT_CONFIG") || rootName == wstring(L"HKCC"))
	{
		rootKey = HKEY_CURRENT_CONFIG;
	}
	else if (rootName == wstring(L"HKEY_CURRENT_USER") || rootName == wstring(L"HKCU"))
	{
		rootKey = HKEY_CURRENT_USER;
	}
	else if (rootName == wstring(L"HKEY_LOCAL_MACHINE") || rootName == wstring(L"HKLM"))
	{
		rootKey = HKEY_LOCAL_MACHINE;
	}
	else if (rootName == wstring(L"HKEY_USERS") || rootName == wstring(L"HKU"))
	{
		rootKey = HKEY_USERS;
	}
	//possibly ought to include performance data root keys - see http://msdn.microsoft.com/en-us/library/ms724836(VS.85).aspx
	//DEBUG - wcout << L"Root name: " << rootName << endl;
	//DEBUG - cout << "Root key: " << rootKey << endl;
	return rootKey;
}

bool RegistryKey::Open(wstring path, REGSAM access)
{
	Close();

	wstring rootName;
	HKEY rootKey = ExtractRootKey(path, rootName);
	if (rootKey)
	{
		long result = RegOpenKeyEx(rootKey, path.c_str(), 0, access, &m_key);
		if (result == ERROR_SUCCESS && m_key)
		{
			return true;
		}
		else if (result == ERROR_FILE_NOT_FOUND)
		{
			return false;
		}
		else
		{
			throw WinException("Unable to open registry key", (DWORD) result, path);
		}
	}
	else
	{
		throw WinException("Invalid root registry key specified to be opened", 0, rootName);
	}
}



//TODO - possibly this should expose more of the options of RegCreateKeyEx like security descriptor
bool RegistryKey::Create(wstring path, REGSAM access)
{
	Close();

	wstring rootName;
	HKEY rootKey = ExtractRootKey(path, rootName);
	DWORD disposition = 0;
	if (rootKey)
	{
		long result = RegCreateKeyEx(rootKey, path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, access, NULL, &m_key, &disposition);
		if (result != ERROR_SUCCESS || !m_key)
		{
			throw WinException("Unable to open or create registry key", (DWORD) result, path);
		}
	}
	else
	{
		throw WinException("Invalid root registry key specified to be opened", 0, rootName);
	}
	return (disposition == CREATED_NEW_KEY);
}


void RegistryKey::Close()
{
	if(m_key)
	{
		LONG result = RegCloseKey(m_key);
		if (result != ERROR_SUCCESS)
		{
			throw WinException("Unable to close registry key", (DWORD) result);
		}
		m_key = 0;
	}
}


std::auto_ptr<RegistryValue> RegistryKey::GetValue(wstring name) const
{
	const DWORD VALUE_BUFFER_PADDING = sizeof(wchar_t) * 2; //empty space at the end of the buffer containing the value, to ensure string values are null terminated
	const DWORD VALUE_BUFFER_MAX = numeric_limits<DWORD>::max() - VALUE_BUFFER_PADDING; //size of buffer, not including padding
	assert(m_key != HKEY_PERFORMANCE_DATA); //not sure if this actually works, but it documents the issue
	if (m_key)
	{
		DWORD type = 0;
		auto_array_ptr<byte>::type data;
		DWORD length = 10;
		LONG result = RegQueryValueEx(m_key, name.c_str(), NULL, NULL, NULL, &length); //get length of value
		if (result == ERROR_SUCCESS)
		{
			do
			{
				data.reset(new byte[length + VALUE_BUFFER_PADDING]);
				result = RegQueryValueEx(m_key, name.c_str(), NULL, &type, data.get(), &length);
			} while (result == ERROR_MORE_DATA && length < VALUE_BUFFER_MAX); //if the length of the value increased since last call, try again with new buffer length set by RegQueryValueEx until we succeed or some other error occurs - unless the buffer is getting too large
		}

		if (result == ERROR_SUCCESS && data.get())
		{
			RegistryValue * regVal = new RegistryValue(data.release(), length, type);
			return auto_ptr<RegistryValue>(regVal);
		}
		else if (result == ERROR_FILE_NOT_FOUND)
		{
			return auto_ptr<RegistryValue>(NULL);
		}
		else
		{
			throw WinException("Unable to read registry value", (DWORD) result, name); //would be nice to include key path too but we don't store it
		}
	}
	else
	{
		throw WinException("Attempt to read a registry value from an undefined key", 0, name);
	}
}

wstring RegistryKey::GetValueString(const wstring & name, WhetherExpand expandEnvVars) const
{
	wstring value;
	if (RetrieveValueString(value, name, expandEnvVars))
	{
		return value;
	}
	else
	{
		throw KeyNotFoundException("Unable to find String registry value", 0, name);
	}
}

wstring RegistryKey::GetValueString(const wstring & name, const wstring & defaultVal, WhetherExpand expandEnvVars) const
{
	wstring value;
	if (RetrieveValueString(value, name, expandEnvVars))
	{
		return value;
	}
	else
	{
		return defaultVal;
	}
}


DWORD RegistryKey::GetValueDWORD(const wstring & name) const
{
	DWORD value = 0;
	if (RetrieveValueDWORD(value, name))
	{
		return value;
	}
	else
	{
		throw KeyNotFoundException("Unable to find DWORD registry value", 0, name);
	}
}

DWORD RegistryKey::GetValueDWORD(const wstring & name, DWORD defaultVal) const
{
	DWORD value = 0;
	if (RetrieveValueDWORD(value, name))
	{
		return value;
	}
	else
	{
		return defaultVal;
	}
}



bool RegistryKey::RetrieveValueString(wstring & value, const wstring & name, WhetherExpand expandEnvVars) const
{
	const DWORD EXPAND_BUFFER_MAX = 32 * 1024; //Maximum size of buffers to ExpandEnvironmentStrings function - http://msdn.microsoft.com/en-us/library/ms724265(VS.85).aspx
	auto_ptr<RegistryValue> regVal = GetValue(name);
	if (!regVal.get() || !regVal->GetValue())
	{
			return false;
	}
	else
	{
		if (regVal->GetType() != REG_SZ && regVal->GetType() != REG_EXPAND_SZ)
		{
			throw WinException("Registry value is not one of the expected types, REG_SZ or REG_EXPAND_SZ", 0, name);
		}
		else
		{
			if (expandEnvVars == ALWAYS || (expandEnvVars == EXPAND_TYPE_ONLY && regVal->GetType() == REG_EXPAND_SZ))
			{
				DWORD valueCharLen = regVal->GetSize() / (sizeof(wchar_t)/sizeof(byte)) + 1; //the number of wide characters in the retrieved value plus an extra null terminator
				if (valueCharLen > EXPAND_BUFFER_MAX)
				{
					throw WinException("Registry value is too large to expand any environment variables it may contain", 0, name);
				}

				DWORD retLength = (DWORD) min(((unsigned long) valueCharLen) * 2, (unsigned long) EXPAND_BUFFER_MAX); //use longs to avoid any possibility of valueCharLen * 2 overflowing
				DWORD length = 0;
				auto_array_ptr<wchar_t>::type expandedBuf;
				do
				{
					length = retLength;
					expandedBuf.reset(new wchar_t[length]);
					retLength = ExpandEnvironmentStrings((wchar_t *) regVal->GetValue(), expandedBuf.get(), length);
				} while (retLength > length && retLength <= EXPAND_BUFFER_MAX); //try again if the buffer wasn't large enough, unless we would need too large a buffer

				if (retLength == 0) //indicates an error has occured
				{
					throw WinException("Error occured while expanding environment variables in a registry value", GetLastError(), name);
				}
				else if (retLength > EXPAND_BUFFER_MAX)
				{
					throw WinException("Registry value with environment variables expanded is too large", 0, name);
				}
				else
				{
					value.assign(expandedBuf.get());
				}
			}
			else
			{
				value.assign((wchar_t *) regVal->GetValue()); //know it has a null terminator, so no need of length
			}
			return true;
		}
	}
}

bool RegistryKey::RetrieveValueDWORD(DWORD & value, const wstring & name) const
{
	auto_ptr<RegistryValue> regVal = GetValue(name);
	if (!regVal.get() || !regVal->GetValue())
	{
			return false;
	}
	else
	{
		if (regVal->GetType() != REG_DWORD)
		{
			throw WinException("Registry value is not of the expected type, DWORD", 0, name);
		}
		else
		{
			if (regVal->GetSize() < sizeof(DWORD))
			{
				throw WinException("Registry value is too not long enough to be of the expected type, DWORD", 0, name);
			}
			else
			{
				value = ((DWORD *) regVal->GetValue())[0];
				return true;
			}
		}
	}
}

void RegistryKey::SetValue(const wstring & name, const RegistryValue & value)
{
	LONG result = RegSetValueEx(m_key, name.c_str(), 0, value.GetType(), value.GetValue(), value.GetSize());
	if (result != ERROR_SUCCESS)
	{
		throw WinException("Failed to set registry value", result, name);
	}
}

void RegistryKey::SetValue(const wstring & name, DWORD value)
{
	DWORD size = sizeof(DWORD)/sizeof(byte);
	byte * data = new byte[size];
	byte * startOfValue = (byte *) &value;
	std::copy(startOfValue, startOfValue + size, data); 
	RegistryValue regVal(data, size, REG_DWORD);
	SetValue(name, regVal);
}

void RegistryKey::SetValue(const wstring & name, const wstring & value, bool expand)
{
	DWORD size = (value.length() + 1) * sizeof(wchar_t); //1 extra character for null terminator
	byte * data = new byte[size];
	byte * startOfValue = (byte *) value.c_str();
	std::copy(startOfValue, startOfValue + size, data);
	RegistryValue regVal(data, size, (expand ? REG_EXPAND_SZ : REG_SZ));
	SetValue(name, regVal);
}


RegistryValue::RegistryValue(byte * value, DWORD size, DWORD type):
	m_value(value), m_size(size), m_type(type)
{}

/*RegistryValue::RegistryValue(const RegistryValue & copied):
	m_size(copied.size), m_type(copied.type)
{}

RegistryValue & operator=(const RegistryValue & copied)
{}*/

RegistryValue::~RegistryValue()
{
	if (m_value)
	{
		delete[] m_value;
	}
}

const byte * RegistryValue::GetValue() const
{
	return m_value;
}

DWORD RegistryValue::GetSize() const
{
	return m_size;
}

DWORD RegistryValue::GetType() const
{
	return m_type;
}

bool RegistryValue::valid() const
{
	return m_value != 0;
}
