#pragma once

#include <atlbase.h>
#include <atlcom.h>

class
	ATL_NO_VTABLE XAudio2ProxyFactory :
	public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
	public IClassFactory
{
public:
	DECLARE_NO_REGISTRY()

	BEGIN_COM_MAP(XAudio2ProxyFactory)
		COM_INTERFACE_ENTRY(IClassFactory)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	static HRESULT CreateFactory(IClassFactory * originalFactory, void ** proxyFactory);

	STDMETHODIMP CreateInstance(IUnknown * pUnkOuter, REFIID riid, void ** ppvObject) override;
	STDMETHODIMP LockServer(BOOL fLock) override;

private:
	ATL::CComPtr<IClassFactory> _originalFactory;
};
