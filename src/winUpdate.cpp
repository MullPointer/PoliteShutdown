


//#include <cassert>
#include "EasyWin.h"
#include "winUpdate.h"
#include <comdef.h>
#include <sstream>
#include <iostream>
#include "auto_handle.h"


using namespace EasyWin;
using namespace std;

HANDLE g_searchEndEvent = INVALID_HANDLE_VALUE;

//Callback to use with BeginSearch - does not do anything useful as we do no need the callback
class SearchCompletedCallback: public ISearchCompletedCallback
{
	public:
	HRESULT __stdcall QueryInterface(REFIID iid, void** ppvObject)
	{
		if (ppvObject == NULL)
		{
			cout << "NULL pointer" << endl;
			return E_POINTER;
		}
		else
		{
			*ppvObject = NULL;

			//DEBUG - wchar_t iidStr[1000];
			//DEBUG - int bytes = StringFromGUID2(iid,iidStr,1000);
			//DEBUG - wcout << iidStr << endl;
			//DEBUG - wcout.flush();

			if (iid == IID_IUnknown)
			{
				*ppvObject = (IUnknown*)(this);
			}

			if (iid == IID_ISearchCompletedCallback)
			{
				*ppvObject = (ISearchCompletedCallback*)(this);
			}

			if (*ppvObject == NULL)
			{
				return E_NOINTERFACE;
			}
			else
			{
				AddRef();
				return S_OK;
			}
		}
	}

	ULONG __stdcall AddRef()
	{
		return InterlockedIncrement(&m_count) ;
	}

	ULONG __stdcall Release()
	{
		InterlockedDecrement(&m_count);
		if (m_count == 0)
		{
			delete this;
			return 0;
		}
		return m_count;
	}

	HRESULT __stdcall Invoke(ISearchJob * searchJob, ISearchCompletedCallbackArgs *)
	{
		//Do nothing
		searchJob; //parameter not used
		return S_OK;
	}


	private:
	LONG m_count;
};

void CleanupCallback(SearchCompletedCallback * p)
{
	p->Release();
}

void CleanupBSTR(BSTR * p)
{
	SysFreeString(*p);
}



bool UpdateSearch::Search(vector<wstring> & titles, const wstring & criteria)
{
	HRESULT hr = S_OK;
	IUpdateSession * session = NULL;
	hr = CoCreateInstance(CLSID_UpdateSession,
							NULL,
							CLSCTX_INPROC_SERVER, //not sure this is right
							IID_IUpdateSession,
							(void **) &session);
	if (FAILED(hr) || !session)
	{
		throw WinException("Failed to create IUpdateSession Interface.", (DWORD) hr);
	}

	IUpdateSearcher * searcher = NULL;
	hr = session->CreateUpdateSearcher(&searcher);
	AutoIUnknownPtr searcherCleanup(searcher);
	session->Release();
	if (FAILED(hr) || !searcher)
	{
		throw WinException("Failed to retrieve IUpdateSearcher Interface", (DWORD) hr);
	}

	hr = searcher->put_Online(VARIANT_FALSE); //With online property set true (the default) search function failed on test computers using WSUS with error code 0x80244022
	if (FAILED(hr))
	{
		throw WinException("Failed to set update search not to search online", (DWORD) hr);
	}

	BSTR searchCriteria = SysAllocString(criteria.c_str());
	auto_ptrx<BSTR, CleanupBSTR> searchCriteriaCleanup(&searchCriteria);

	auto_ptrx<SearchCompletedCallback, CleanupCallback> scCallback;
	scCallback = new SearchCompletedCallback;
	scCallback->AddRef();
	VARIANT empty;
	VariantInit(&empty); //init to empty variant
	ISearchJob * searchJob = NULL;
	hr = searcher->BeginSearch(searchCriteria, scCallback.get(), empty, &searchJob);
	if (FAILED(hr) || !searchJob)
	{
		throw WinException("Failed to start search for updates", (DWORD) hr);
	}

	SetSearchJob(searchJob); //possibly ought to increment reference count on searchJob pointer here - but m_searchJob, should not outlive searchJob, so I don't think its a problem

	ISearchResult * searchResult = NULL;
	hr = searcher->EndSearch(searchJob, &searchResult); //waits until search completes
	SetSearchJob(NULL);
	if (FAILED(hr) || !searchResult)
	{
		throw WinException("Search for updates failed", (DWORD) hr);
	}
	AutoIUnknownPtr searchJobCleanup(searchJob);
	AutoIUnknownPtr searchResultCleanup(searchResult);
	//possibly ought to call searchJob->CleanUp() here but believe this done by EndSearch


	OperationResultCode searchResultCode = orcFailed;
	hr = searchResult->get_ResultCode(&searchResultCode);
	if (FAILED(hr))
	{
		throw WinException("Failed to retrieve result code of search for updates", (DWORD) hr);
	}
	if (searchResultCode == orcAborted)
	{
		return false;
	}
	else if (searchResultCode != orcSucceeded)
	{
		ostringstream s;
		s << "Search for updates failed with status code: ";
		s << (int) searchResultCode;
		throw WinException(s.str().c_str(), (DWORD) hr);
	}

	IUpdateCollection * updates = NULL;
	hr = searchResult->get_Updates(&updates);
	if (FAILED(hr) || !updates)
	{
		throw WinException("Failed to retrieve any information on updates found in search", (DWORD) hr);
	}
	AutoIUnknownPtr updatesCleanup(updates);

	long numUpdates = -1;
	hr = updates->get_Count(&numUpdates);
	if (FAILED(hr) || numUpdates < 0)
	{
		throw WinException("Failed to count number of updates found in search", (DWORD) hr);
	}

	for (int i = 0; i < numUpdates; ++i)
	{
		IUpdate * update = NULL;
		hr = updates->get_Item(i, &update);
		if (FAILED(hr) || !update)
		{
			throw WinException("Failed to get any information on an update found in search", (DWORD) hr);
		}

		BSTR title = NULL;
		hr = update->get_Title(&title);
		update->Release();
		auto_ptrx<BSTR, CleanupBSTR> titleCleanup(&title); //need auto cleanup in case wstring constructor throws
		if (FAILED(hr))
		{
			throw WinException("Failed to read title of update found in search", (DWORD) hr);
		}

		titles.push_back(wstring((wchar_t*)title)); //possibly ought to do something about BSTRs with embedded nulls - at the moment will only get up to first null
	}
	//DEBUG - titles.push_back(L"dummy update");
	return true;
}

void UpdateSearch::SetSearchJob(ISearchJob * searchJob)
{
	CritSectHandle cs(m_critSect);
	m_searchJob = searchJob;
}


void UpdateSearch::Abort()
{
	CritSectHandle cs(m_critSect);
	if (m_searchJob)
	{
		HRESULT hr = m_searchJob->RequestAbort();
		if (FAILED(hr))
		{
			throw WinException("Failed to abort windows update search", (DWORD) hr);
		}
	}
}



bool IsBusyUpdating()
{
	HRESULT hr = S_OK;
	IUpdateSession * session = NULL;
	hr = CoCreateInstance(CLSID_UpdateSession,
							NULL,
							CLSCTX_INPROC_SERVER, //not sure this is right
							IID_IUpdateSession,
							(void **) &session);
	if (FAILED(hr) || !session)
	{
		throw WinException("Failed to create IUpdateSession Interface", (DWORD) hr);
	}

	IUpdateInstaller * installer = NULL;
	hr = session->CreateUpdateInstaller(&installer);
	session->Release();
	if (FAILED(hr) || !installer)
	{
		throw WinException("Failed to retrieve IUpdateInstaller Interface", (DWORD) hr);
	}

	VARIANT_BOOL busy = VARIANT_FALSE;
	hr = installer->get_IsBusy(&busy);
	installer->Release();
	if (FAILED(hr))
	{
		throw WinException("Failed to discover whether windows updates were being installed", (DWORD) hr);
	}
	return (busy == VARIANT_TRUE);
}


