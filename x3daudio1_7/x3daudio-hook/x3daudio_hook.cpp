#include "stdafx.h"
#include "x3daudio_hook.h"
#include "logger.h"
#include <string>
#include <vector>
#include <X3DAudio.h>
#include "IX3DAudioProxy.h"
#include "application.h"


typedef void (STDAPIVCALLTYPE *X3DAudioInitializeFunc)(UINT32 SpeakerChannelMask, FLOAT32 SpeedOfSound, _Out_writes_bytes_(X3DAUDIO_HANDLE_BYTESIZE) X3DAUDIO_HANDLE Instance);
typedef void (STDAPIVCALLTYPE *X3DAudioCalculateFunc)(_In_reads_bytes_(X3DAUDIO_HANDLE_BYTESIZE) const X3DAUDIO_HANDLE Instance, _In_ const X3DAUDIO_LISTENER * pListener, _In_ const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, _Inout_ X3DAUDIO_DSP_SETTINGS * pDSPSettings);


struct x3daudio1_7_dll
{
	HMODULE module;
	X3DAudioInitializeFunc X3DAudioInitialize;
	X3DAudioCalculateFunc X3DAudioCalculate;
};

x3daudio1_7_dll x3daudio1_7;

struct X3DAUDIO_CUSTOM
{
	union
	{
		IX3DAudioProxy * proxy;
		X3DAUDIO_HANDLE handle; // we won't use it anyway...
	};
};

typedef X3DAUDIO_CUSTOM * X3DAUDIO_CUSTOM_HANDLE;

namespace Hook
{
	extern "C" __declspec(dllexport) void STDAPIVCALLTYPE X3DAudioInitialize(UINT32 SpeakerChannelMask, FLOAT32 SpeedOfSound, X3DAUDIO_CUSTOM_HANDLE Instance)
	{
		logger::logDebug("X3DAudioInitialize");
		Instance->proxy = &getX3DAudioProxy();
	}

	extern "C" __declspec(dllexport) void STDAPIVCALLTYPE X3DAudioCalculate(const X3DAUDIO_CUSTOM_HANDLE Instance, _In_ const X3DAUDIO_LISTENER * pListener, _In_ const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, _Inout_ X3DAUDIO_DSP_SETTINGS * pDSPSettings)
	{
		Instance->proxy->X3DAudioCalculate(pListener, pEmitter, Flags, pDSPSettings);
	}
}

void init_x3daudio_hook()
{
	wchar_t path[MAX_PATH];
	GetSystemDirectory(path, MAX_PATH);
	std::wstring original_library_path = std::wstring(path) + L"\\x3daudio1_7.dll";

	x3daudio1_7.module = LoadLibrary(original_library_path.c_str());
	if (x3daudio1_7.module == nullptr)
	{
		MessageBoxW(nullptr, L"Unuable to load x3daudio1_7.dll!", L"X3DAudio1_7 DLL Proxy", MB_ICONERROR);
		ExitProcess(0);
	}
	x3daudio1_7.X3DAudioCalculate = reinterpret_cast<X3DAudioCalculateFunc>(GetProcAddress(x3daudio1_7.module, "X3DAudioCalculate"));
	x3daudio1_7.X3DAudioInitialize = reinterpret_cast<X3DAudioInitializeFunc>(GetProcAddress(x3daudio1_7.module, "X3DAudioInitialize"));
}

void destroy_x3daudio_hook()
{
	FreeLibrary(x3daudio1_7.module);
}
