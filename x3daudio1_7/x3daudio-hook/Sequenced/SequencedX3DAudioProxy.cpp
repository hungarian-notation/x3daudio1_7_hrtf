#include "stdafx.h"
#include "SequencedX3DAudioProxy.h"
#include "proxy/XAudio2VoiceProxy.h"
#include "../common.h"

void SequencedX3DAudioProxy::CorrectVolume(SpatialData & spatialDataCopy, const ChannelMatrix & clientMatrix) const
{
	float volumeSum = 0.0f;
	for (unsigned int destIndex = 0; destIndex < clientMatrix.GetDestinationCount(); destIndex++)
	{
		for (unsigned int srcIndex = 0; srcIndex < clientMatrix.GetSourceCount(); srcIndex++)
		{
			volumeSum += clientMatrix.GetValue(srcIndex, destIndex);
		}
	}
	spatialDataCopy.volume_multiplier = volumeSum * 0.5f; // because we distribute power over two channels. But I'm not sure this is the right place to place this distribution.
}

SpatialData SequencedX3DAudioProxy::ExtractSpatialData(XAudio2VoiceProxy * source, IXAudio2Voice * pDestinationProxy)
{
	auto spatialDataCopy = _lastSpatialData;
	_lastSpatialData.present = false;

	const auto clientMatrix = source->getOutputMatrix(pDestinationProxy);
	
	CorrectVolume(spatialDataCopy, clientMatrix);

	return spatialDataCopy;
}

void SequencedX3DAudioProxy::X3DAudioInitialize(UINT32 SpeakerChannelMask, FLOAT32 SpeedOfSound)
{
	_speedOfSound = SpeedOfSound;
}


void SequencedX3DAudioProxy::X3DAudioCalculate(const X3DAUDIO_LISTENER * pListener, const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS * pDSPSettings)
{
	_lastSpatialData = CommonX3DAudioCalculate(_speedOfSound, pListener, pEmitter, Flags, pDSPSettings);


	const UINT32 channelCount = pDSPSettings->DstChannelCount * pDSPSettings->SrcChannelCount;
	const float factor = 1.0f / channelCount;
	for (UINT32 i = 0; i < channelCount; i++)
	{
		pDSPSettings->pMatrixCoefficients[i] = _lastSpatialData.volume_multiplier * factor;
	}
}

