#ifndef LOGGED_ON_USERS
#define LOGGED_ON_USERS


#include <vector>
#include <string>
#include <iosfwd>
#include "EasyWinSID.h"

struct LogonSessionInfo
{
	LogonSessionInfo(const std::wstring & aName,
					const std::wstring & aDomain,
					EasyWin::SIDclass aSID,
					const LUID & aSessionLUID,
					bool aLocal):
		userName(aName), userDomain(aDomain), sid(aSID), sessionLUID(aSessionLUID), local(aLocal) {}

	std::wstring userName;
	std::wstring userDomain;
	EasyWin::SIDclass sid;
	LUID sessionLUID;
	bool local; //true if local interactive user, false if remote interactive user
};

std::wostream & operator<<(std::wostream & o, const LogonSessionInfo & user);


//objects containing some details of interactive users currently logged on are appended to the vector users
void GetLoggedOnUsers(std::vector<LogonSessionInfo> & users);




#endif // LOGGED_ON_USERS

