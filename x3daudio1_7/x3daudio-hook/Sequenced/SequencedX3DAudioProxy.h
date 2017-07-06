#pragma once
#include "ISpatializationGlue.h"
#include "ChannelMatrix.h"

class SequencedX3DAudioProxy final : public ISpatializationGlue
{
public:
	SequencedX3DAudioProxy() = default;
	SpatialData ExtractSpatialData(XAudio2VoiceProxy * source, IXAudio2Voice * pDestinationProxy) override;

	void X3DAudioInitialize(UINT32 SpeakerChannelMask, FLOAT32 SpeedOfSound) override;
	void X3DAudioCalculate(const X3DAUDIO_LISTENER * pListener, const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS * pDSPSettings) override;

private:
	void CorrectVolume(SpatialData & spatialDataCopy, const ChannelMatrix & clientMatrix) const;
private:
	SpatialData _lastSpatialData;
	float _speedOfSound;

public:
	SequencedX3DAudioProxy(const SequencedX3DAudioProxy& other) = delete;
	SequencedX3DAudioProxy(SequencedX3DAudioProxy&& other) noexcept = default;
	SequencedX3DAudioProxy& operator=(const SequencedX3DAudioProxy& other) = delete;
	SequencedX3DAudioProxy& operator=(SequencedX3DAudioProxy&& other) noexcept = default;
};
