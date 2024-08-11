#ifndef EASYWINSHUTDOWN_H
#define EASYWINSHUTDOWN_H


#include <string>
#include "EasyWinPrivilege.h"
typedef unsigned long DWORD;

namespace EasyWin
{


//TODO remote shutdown


//caps timeout and length of message to their maximums
//leaves the process without the SE_SHUTDOWN_NAME priviledge
//throws WinExceptions
void ShutdownLocal(DWORD reasonCode,
				const std::wstring & message,
				DWORD timeout = 0,
				bool forceAppsClosed = false,
				bool reboot = false);

//abort shutdown on local machine
//leaves the process without the SE_SHUTDOWN_NAME priviledge
//throws WinExceptions
void AbortShutdownLocal();


} //end namespace EasyWin


#endif // EASYWINSHUTDOWN_H

