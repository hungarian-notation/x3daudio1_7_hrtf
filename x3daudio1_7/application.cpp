#include "stdafx.h"
#include "application.h"

#include "ISpatializationGlue.h"

#include <memory>
#include "x3daudio-hook/Matrix/MatrixEncodingX3DAudioProxy.h"
#include "x3daudio-hook/Matrix/Sound3DRegistry.h"
#include "x3daudio-hook/Sequenced/SequencedX3DAudioProxy.h"
#include "proxy/XAudio2Proxy.h"
#include "hrtf/MhrData/MhrHrtfDataSet.h"

#define GLUE_SEQUENCED
//#define CREATE_AUTHENTIC_DEBUG_XAUDIO

std::unique_ptr<ISpatializationGlue> s_glue;


void EnsureInitialized()
{
	if (s_glue == nullptr)
	{
#if defined(GLUE_MATRIX_ENCODING)
		s_glue = std::make_unique<MatrixEncodingX3DAudioProxy>(&Sound3DRegistry::GetInstance());
#elif defined(GLUE_SEQUENCED)
		s_glue = std::make_unique<SequencedX3DAudioProxy>();
#endif
	}
}

ISpatializedDataExtractor & get_spatialized_data_extractor()
{
	EnsureInitialized();
	return *s_glue;
}

IX3DAudioProxy & get_x3daudio_proxy()
{
	EnsureInitialized();
	return *s_glue;
}

HRESULT create_xaudio_proxy(ATL::CComPtr<IUnknown> original, const IID& riid, void** ppv)
{
#if defined(CREATE_AUTHENTIC_DEBUG_XAUDIO)
	return XAudio2Proxy::CreateActualDebugInstance(originalObject.Detach(), riid, ppv);
#else

	std::vector<std::wstring> dataFiles;

	WIN32_FIND_DATAW fileData;
	const HANDLE hFind = FindFirstFileW(L"hrtf\\*.mhr", &fileData);

	if (hFind == INVALID_HANDLE_VALUE)
		throw std::logic_error("No mhr files found in hrtf directory.");

	do
	{
		dataFiles.push_back(std::wstring(L"hrtf\\") + fileData.cFileName);
	} while (FindNextFileW(hFind, &fileData));

	FindClose(hFind);

	return XAudio2Proxy::CreateInstance(original.Detach(), riid, ppv, std::make_shared<MhrHrtfDataSet>(dataFiles), get_spatialized_data_extractor());
#endif
}
