#include "stdafx.h"
#include "SequencedX3DAudioProxy.h"
#include "proxy/XAudio2VoiceProxy.h"
#include "../common.h"

void SequencedX3DAudioProxy::CorrectVolume(SpatialData & spatialDataCopy, const ChannelMatrix & clientMatrix) const
{
	float volumeSum = 0.0f;
	for (std::size_t destIndex = 0; destIndex < clientMatrix.getDestinationCount(); destIndex++)
	{
		for (std::size_t srcIndex = 0; srcIndex < clientMatrix.getSourceCount(); srcIndex++)
		{
			volumeSum += clientMatrix.getValue(srcIndex, destIndex);
		}
	}
	spatialDataCopy.volume_multiplier = volumeSum;
}

SpatialData SequencedX3DAudioProxy::ExtractSpatialData(XAudio2VoiceProxy * source, IXAudio2Voice * pDestinationProxy)
{
	auto spatialDataCopy = _lastSpatialData;
	_lastSpatialData.present = false;

	const auto clientMatrix = source->getOutputMatrix(pDestinationProxy);
	
	CorrectVolume(spatialDataCopy, clientMatrix);

	return spatialDataCopy;
}


void SequencedX3DAudioProxy::X3DAudioCalculate(const X3DAUDIO_LISTENER * pListener, const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS * pDSPSettings)
{
	//pDSPSettings->DopplerFactor = 1.0f;
	//pDSPSettings->ReverbLevel = 0.0f;
	_lastSpatialData = CommonX3DAudioCalculate(pListener, pEmitter, Flags, pDSPSettings);


	const UINT32 channelCount = pDSPSettings->DstChannelCount * pDSPSettings->SrcChannelCount;
	const float factor = 1.0f / channelCount;
	for (UINT32 i = 0; i < channelCount; i++)
	{
		pDSPSettings->pMatrixCoefficients[i] = _lastSpatialData.volume_multiplier * factor;
	}
}

