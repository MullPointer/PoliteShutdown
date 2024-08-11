
#ifndef EASY_WIN_PRIVILEGE_H
#define EASY_WIN_PRIVILEGE_H

namespace EasyWin
{

//enable or disable a privilege of the current process
//throws WinExceptions
//TODO - might also add ability to remove (as opposed to disable) privileges using this method - not sure why would want to do this
void AlterPrivilege(const wchar_t * privName, bool enable = true);

//represents a temporary windows privilege that is automatically disabled when object destroyed
//expects you to maintain the string name of the privilege until destruction - eg use a constant
//TODO - possibly should not disable the privilege when destructed if it was enabled already on construction
class TempPrivilege
{
	public:
	TempPrivilege();
	TempPrivilege(const wchar_t * privName);
	~TempPrivilege();

	void Acquire(const wchar_t * privName);
	void Release();

	const wchar_t * GetPrivName() const;

	private:
	TempPrivilege(const TempPrivilege &); //non copyable
	TempPrivilege & operator=(const TempPrivilege &);

	const wchar_t * m_privName;
};


} //end namespace EasyWin

#endif // EASY_WIN_PRIVILEGE_H
