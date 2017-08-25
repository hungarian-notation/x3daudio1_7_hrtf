#pragma once

#include <atlbase.h>

class ISpatializedDataExtractor;
class IX3DAudioProxy;

IX3DAudioProxy & get_x3daudio_proxy();
HRESULT create_xaudio_proxy(ATL::CComPtr<IUnknown> original, REFIID riid, void ** ppv);
