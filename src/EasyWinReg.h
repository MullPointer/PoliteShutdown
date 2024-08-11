#ifndef EASYWINREG_H
#define EASYWINREG_H


#include <string>
#include <memory>
#include "EasyWin.h"


namespace EasyWin {


//TO ADD - GetValueMULTI_SZ function for REG_MULTI_SZ, set value functions, open subkey function, delete key functions
//TODO test paths longer than 255
//think can use RegOpenKeyEx to duplicate keys
//TODO test extremely long values
//TODO factor out environment variable expansion into separate function


class KeyNotFoundException: public EasyWin::WinException
{
	public:
	KeyNotFoundException(std::string message, DWORD errorCode = 0, std::wstring unicodeMessage=L""):
		WinException(message, errorCode, unicodeMessage) {}
	virtual WinException * Clone() const
	{
		return new KeyNotFoundException(*this);
	}
};

//encapsulates a retrived registry value in byte form together with its type
// the GetValue and RetrieveValue methods should make its direct use unnecessary if you expect the registry value to be a particular type
class RegistryValue
{
	public:
	//value should hold buffer allocated by new with length size - the object takes ownership of this and will delete it on destruction
	RegistryValue(byte * value, DWORD size, DWORD type);
	~RegistryValue();

	const byte * GetValue() const;
	DWORD GetSize() const;
	DWORD GetType() const; //possible types - http://msdn.microsoft.com/en-us/library/ms724884(VS.85).aspx
	bool valid() const;

	private:
	RegistryValue(const RegistryValue & copied); //no copy constructor
	RegistryValue & operator=(const RegistryValue & copied); //no assignment
	byte * m_value;
	DWORD m_type;
	DWORD m_size;
};

class RegistryKey
{
	public:
	RegistryKey();
	//REGSAM values - http://msdn.microsoft.com/en-us/library/ms724878(VS.85).aspx
	//throws KeyNotFoundException if key not found
	RegistryKey(std::wstring path, bool create = false, REGSAM access = KEY_ALL_ACCESS);
	~RegistryKey();

	//returns whether key found - if key not accessible throws exception
	bool Open(std::wstring path, REGSAM access = KEY_ALL_ACCESS);

	//if already exists just opens
	//returns whether new key was created (false if existing one was opened)
	bool Create(std::wstring path, REGSAM access = KEY_ALL_ACCESS);

	//does nothing if already closed, or is NULL Key
	//throws WinException on failure
	void Close();

	//returns null auto_ptr if key does not exist
	//empty string for default value
	//ensures that string values are null terminated
	//is not designed to work with the key set to HKEY_PERFORMANCE_DATA
	std::auto_ptr<RegistryValue> GetValue(std::wstring name) const;

	enum WhetherExpand {ALWAYS, EXPAND_TYPE_ONLY, NEVER};
	/**
	* Gets named registry value of receiver key converting it to a string
	* if value does not exist, if no defaultVal argument throw exception, else return defaultVal
	* if value is not of type REG_SZ or REG_EXPAND_SZ throws exception
	* expandEnvVars specifies whether environment variables should be expanded:
	* 		EXPAND_TYPE_ONLY - implies environment variables only expanded if value is of type REG_EXPAND_SZ
	*/
	std::wstring GetValueString(const std::wstring & name, WhetherExpand expandEnvVars = EXPAND_TYPE_ONLY) const;
	std::wstring GetValueString(const std::wstring & name, const std::wstring & defaultVal, WhetherExpand expandEnvVars = EXPAND_TYPE_ONLY) const;
	//return whether value found, first argument set to value
	bool RetrieveValueString(std::wstring & value, const std::wstring & name, WhetherExpand expandEnvVars = EXPAND_TYPE_ONLY) const;

	DWORD GetValueDWORD(const std::wstring & name) const;
	DWORD GetValueDWORD(const std::wstring & name, DWORD defaultVal) const;
	//return whether value found, first argument set to value
	bool RetrieveValueDWORD(DWORD & value, const std::wstring & name) const;

	void SetValue(const std::wstring & name, const RegistryValue & value);
	void SetValue(const std::wstring & name, DWORD value);
	//expandType specified whether registry value is a REG_EXPAND_SZ rather than a REG_SZ
	void SetValue(const std::wstring & name, const std::wstring & value, bool expand = false);

	private:
	RegistryKey(const RegistryKey &); //no copying - TODO may be able to improve on this
	RegistryKey & operator=(const RegistryKey &);

	HKEY m_key;
};




} //end namespace EasyWin

#endif // EASYWINREG_H

