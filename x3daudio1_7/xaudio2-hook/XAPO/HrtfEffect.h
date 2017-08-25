#pragma once

#include "hrtf/MhrData/MhrHrtfDataSet.h"

#include "math/math_types.h"
#include "HrtfXapoParam.h"

#define NOMINMAX
#include <xapobase.h>

#include <vector>
#include <memory>


class __declspec(uuid("{80D4BED4-7605-4E4C-B29C-5579C375C599}")) HrtfXapoEffect : public CXAPOParametersBase
{
public:
	explicit HrtfXapoEffect(const std::shared_ptr<IHrtfDataSet> & hrtfData);

	// Inherited via CXAPOParametersBase
	STDMETHOD(LockForProcess)(UINT32 inputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS * pInputLockedParameters, UINT32 outputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS * pOutputLockedParameters) override;
	STDMETHOD_(void, Process)(UINT32 inputProcessParameterCount, const XAPO_PROCESS_BUFFER_PARAMETERS * pInputProcessParameters, UINT32 outputProcessParameterCount, XAPO_PROCESS_BUFFER_PARAMETERS * pOutputProcessParameters, BOOL isEnabled) override;

private:
	bool HasHistory() const;
	void ProcessValidBuffer(const float * pInput, float * pOutput, const UINT32 frameCount, const HrtfXapoParam & params);
	void ProcessInvalidBuffer(float * pOutput, const UINT32 framesToWriteCount, UINT32 & validFramesCounter, const HrtfXapoParam & params);
	void Bypass(const float * pInput, float * pOutput, const UINT32 frameCount, const bool isValid);
	void Convolve(const UINT32 frameIndex, DirectionData& hrtfData, float& output);
	void ProcessFrame(float & leftOutput, float & rightOutput, const UINT32 frameIndex);
	static XAPO_REGISTRATION_PROPERTIES _regProps;

	WAVEFORMATEX _inputFormat;
	WAVEFORMATEX _outputFormat;
	HrtfXapoParam _params[3]; // ring buffer as CXAPOParametersBase requires
	float _timePerFrame;

	std::shared_ptr<IHrtfDataSet> _hrtfDataSet;
	const IHrtfData * _hrtfData;
	std::vector<float> _signal;
	UINT32 _invalidBuffersCount;
	UINT32 _buffersPerHistory;
	UINT32 _historySize;

	DirectionData _hrtfDataLeft;
	DirectionData _hrtfDataRight;
};
