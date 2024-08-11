#ifndef EASYWINSID_H
#define EASYWINSID_H

#include "EasyWin.h" //for windows.h for PSID - tried to restrict to WTypes.h but this caused bug due to unicode flag

namespace EasyWin
{

class SIDclass
{
	public:
	SIDclass(PSID pSID);
	SIDclass(const SIDclass & copied);
	~SIDclass();
	SIDclass & operator=(const SIDclass & copied);

	std::wstring ToString() const; //would be more efficient to return by reference, but too messy
	void Set(PSID pSID);

	//passes out a pointer to the buffer containing the SID - is not const as windows API functions do not let it be, but should not be deallocated or have its length changed
	PSID GetPSID();

	bool operator==(const SIDclass & o);
	bool operator!=(const SIDclass & o);

	private:
	void SIDclass::Set(PSID pSID, unsigned int length);
	bool Release();
	PSID m_buf;
	unsigned int m_length;
};

} //end namespace EasyWin

#endif // EASYWINSID_H

