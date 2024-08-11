#ifndef SETTINGSSOURCE_H
#define SETTINGSSOURCE_H


#include <string>
#include <map>
#include <memory>
#include "EasyWinReg.h"
#include "EasyWinSync.h"

//TODO - possibly should take a reference to a Logger and then log any settings exceptions itself - return default settings
	//then have problem of how to get Log path setting

//TODO methods to read defaults

//TODO possibly should throw if setting accessed for which is no reg key or defined default - detect programmer error

//TODO SetKeyPath should intercept exceptions and improve their message

//TODO there is a lot of repetition in the get and set methods (two of the settings source methods are identical) - can find way to refactor? - template methods?

//thread-safe
//setting names case insensitive
class SettingsSource
{
	public:
	SettingsSource();
	virtual ~SettingsSource() {}

	//returns false if key not found, WinException on access failure
	virtual bool SetKeyPath(const std::wstring & keyPath);
	//reopens the settings registry key
	virtual void ReOpenKey();

	//return value from regkey if exists, then value from defaults, then empty string or 0
	//may throw on reg read error
	//effectively const, apart from syncronization
	virtual std::wstring GetString(const std::wstring & name);
	virtual DWORD GetInt(const std::wstring & name);

	//can add default string and int with same name without error, although attempting to get a value of one type when registry value of other exists will cause exception
	virtual void SetStringDefault(const std::wstring & name, const std::wstring & value);
	virtual void SetIntDefault(const std::wstring & name, DWORD value);

	protected:
	//the access this class requires to the registry key - to be altered by overriding classes
	virtual REGSAM GetKeyAccess()
		{return KEY_READ;}
	//whether this class will create the registry key if it does not exist - to be altered by overriding classes
	virtual bool WhetherCreateKey()
		{return false;}

	protected:
	std::auto_ptr<EasyWin::RegistryKey> m_key;
	std::wstring m_keyPath;
	std::map<std::wstring, std::wstring> m_stringDefaults;
	std::map<std::wstring, DWORD> m_intDefaults;
	EasyWin::CriticalSection m_critSect;

	private:
	SettingsSource(const SettingsSource &); //no copy constructor
	const SettingsSource & operator=(const SettingsSource &); //no assignment
};


//just like a SettingsStore, but can also write settings back to registry
//setKeyPath will create settings key if does not exist - now always returns true (or throws exception)
//	note write access required to registry key
class SettingsStore: public SettingsSource
{
	public:
	SettingsStore() {}
	virtual ~SettingsStore() {}

	virtual void SetString(const std::wstring & name, const std::wstring & value);
	virtual void SetInt(const std::wstring & name, DWORD value);

	protected:
	virtual REGSAM GetKeyAccess()
		{return KEY_READ | KEY_WRITE;}
	virtual bool WhetherCreateKey()
		{return true;}

};

//adds ability to write settings with a corresponding timestamp, if a setting is too old it is treated as non-existant - the default is retrieved instead
//Timestamp value is a string named <SettingName>TimeStamp - do not name another setting with this name
class SettingsStoreExpiring: public SettingsStore
{
	public:
	SettingsStoreExpiring() {}
	virtual ~SettingsStoreExpiring() {}

};



#endif // SETTINGSSOURCE_H
