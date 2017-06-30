#include "stdafx.h"
#include "SequencedX3DAudioProxy.h"
#include "../common.h"

SpatialData SequencedX3DAudioProxy::ExtractSpatialData(XAudio2VoiceProxy * source, IXAudio2Voice * pDestinationProxy)
{
	const auto spatialDataCopy = _lastSpatialData;
	_lastSpatialData.present = false;
	return spatialDataCopy;
}


void SequencedX3DAudioProxy::X3DAudioCalculate(const X3DAUDIO_LISTENER * pListener, const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS * pDSPSettings)
{
	//pDSPSettings->DopplerFactor = 1.0f;
	//pDSPSettings->ReverbLevel = 0.0f;
	_lastSpatialData = CommonX3DAudioCalculate(pListener, pEmitter, Flags, pDSPSettings);
}

