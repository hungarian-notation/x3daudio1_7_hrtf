#include "stdafx.h"

#include "HrtfEffect.h"
#include "logger.h"
#include <algorithm>

#include <intrin.h>

constexpr UINT32 OutputChannelCount = 2;

XAPO_REGISTRATION_PROPERTIES HrtfXapoEffect::_regProps = {
	__uuidof(HrtfXapoEffect),
	L"HRTF Effect",
	L"Copyright (C)2015 Roman Zavalov",
	1,
	0,
	XAPO_FLAG_FRAMERATE_MUST_MATCH
	| XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH
	| XAPO_FLAG_BUFFERCOUNT_MUST_MATCH
	| XAPO_FLAG_INPLACE_SUPPORTED,
	1, 1, 1, 1 };

HrtfXapoEffect::HrtfXapoEffect(const std::shared_ptr<IHrtfDataSet> & hrtfData) :
	CXAPOParametersBase(&_regProps, reinterpret_cast<BYTE*>(_params), sizeof(HrtfXapoParam), FALSE)
	, _timePerFrame(0)
	, _hrtfDataSet(hrtfData)
	, _hrtfData(nullptr)
	, _invalidBuffersCount(0)
	, _buffersPerHistory(0)
	, _historySize(0)
{
	_params[0] = { };
	_params[1] = { };
	_params[2] = { };
	_signal.reserve(192);
	_hrtfDataLeft.impulse_response.reserve(128);
	_hrtfDataRight.impulse_response.reserve(128);
}

HRESULT HrtfXapoEffect::LockForProcess(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS * pInputLockedParameters, UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS * pOutputLockedParameters)
{
	_ASSERT(pInputLockedParameters[0].pFormat->nChannels == 1 || pInputLockedParameters[0].pFormat->nChannels == 2);
	_ASSERT(pOutputLockedParameters[0].pFormat->nChannels == 2);

	const HRESULT hr = CXAPOParametersBase::LockForProcess(
		InputLockedParameterCount,
		pInputLockedParameters,
		OutputLockedParameterCount,
		pOutputLockedParameters);

	if (SUCCEEDED(hr))
	{
		memcpy(&_inputFormat, pInputLockedParameters[0].pFormat, sizeof(_inputFormat));
		memcpy(&_outputFormat, pOutputLockedParameters[0].pFormat, sizeof(_outputFormat));

		_timePerFrame = 1.0f / float(_inputFormat.nSamplesPerSec);

		//_ASSERT(_hrtf_data_set->has_sample_rate(_input_format.nSamplesPerSec));

		if (_hrtfDataSet->HasSampleRate(_inputFormat.nSamplesPerSec))
		{
			_hrtfData = &_hrtfDataSet->GetSampleRateData(_inputFormat.nSamplesPerSec);
			_historySize = _hrtfData->GetLongestDelay() + _hrtfData->GetResponseLength();
			_buffersPerHistory = _historySize / (pInputLockedParameters[0].MaxFrameCount + _historySize - 1);
			_invalidBuffersCount = _buffersPerHistory;

			_signal.resize(_historySize + pInputLockedParameters[0].MaxFrameCount);
			std::fill(std::begin(_signal), std::begin(_signal) + _historySize, 0.0f);
		}
		else
		{
			logger::logRelease("ERROR [HRTF]: There's no dataset for sample rate ", _inputFormat.nSamplesPerSec, " Hz");
			_hrtfData = nullptr;
		}
	}
	return hr;
}

void HrtfXapoEffect::ProcessValidBuffer(const float * pInput, float * pOutput, const UINT32 frameCount, const HrtfXapoParam & params)
{
	const float volume = params.VolumeMultiplier;

	if (_inputFormat.nChannels == 1)
	{
		std::transform(pInput, pInput + frameCount, std::begin(_signal) + _historySize, [=](float value) { return value * volume; });
	}
	else if (_inputFormat.nChannels == 2)
	{
		for (UINT32 i = 0; i < frameCount; i++)
		{
			_signal[i + _historySize] = (pInput[i * 2 + 0] + pInput[i * 2 + 1]) * volume;
		}
	}

	for (UINT32 i = 0; i < frameCount; i++)
	{
		ProcessFrame(pOutput[i * OutputChannelCount + 0], pOutput[i * OutputChannelCount + 1], i);
	}

	std::copy(std::end(_signal) - _historySize, std::end(_signal), std::begin(_signal));
	_invalidBuffersCount = 0;
}

void HrtfXapoEffect::ProcessInvalidBuffer(float * pOutput, const UINT32 framesToWriteCount, UINT32 & validFramesCounter, const HrtfXapoParam & params)
{
	std::fill(std::begin(_signal) + _historySize, std::end(_signal), 0.0f);

	for (UINT32 i = 0; i < framesToWriteCount; i++)
	{
		ProcessFrame(pOutput[i * OutputChannelCount + 0], pOutput[i * OutputChannelCount + 1], i);
	}
	validFramesCounter = framesToWriteCount;

	std::copy(std::end(_signal) - _historySize, std::end(_signal), std::begin(_signal));
	_invalidBuffersCount++;
}

void HrtfXapoEffect::Bypass(const float * pInput, float * pOutput, const UINT32 frameCount, const bool isInputValid)
{
	const bool inPlace = (pInput == pOutput);

	if (isInputValid)
	{
		if (_inputFormat.nChannels == 1)
		{
			for (UINT32 i = 0; i < frameCount; i++)
			{
				pOutput[i * OutputChannelCount + 0] = pInput[i];
				pOutput[i * OutputChannelCount + 1] = pInput[i];
			}
		}
		else if (_inputFormat.nChannels == 2)
		{
			if (!inPlace)
			{
				for (UINT32 i = 0; i < frameCount; i++)
				{
					pOutput[i * OutputChannelCount + 0] = pInput[i * 2 + 0];
					pOutput[i * OutputChannelCount + 1] = pInput[i * 2 + 1];
				}
			}
		}
	}
}

void HrtfXapoEffect::Convolve(const UINT32 frameIndex, DirectionData& hrtfData, float& output)
{
	const int startSignalIndex = _historySize + frameIndex - static_cast<int>(hrtfData.delay);

	_ASSERT(static_cast<int>(startSignalIndex) - static_cast<int>(hrtfData.impulse_response.size()) >= 0);

#if 1

	const UINT32 wholePackCount = UINT32(hrtfData.impulse_response.size()) / 4;
	const UINT32 remainderCount = UINT32(hrtfData.impulse_response.size()) % 4;

	__m128 packedSum = _mm_setzero_ps();
	for (UINT32 i = 0; i < wholePackCount; i++)
	{
		const __m128 reversedSignal = _mm_loadu_ps(&_signal[startSignalIndex - (i * 4) - 3]);
		const __m128 signal = _mm_shuffle_ps(reversedSignal, reversedSignal, _MM_SHUFFLE(0, 1, 2, 3));
		const __m128 response = _mm_loadu_ps(&hrtfData.impulse_response[i * 4]);
		packedSum = _mm_add_ps(packedSum, _mm_mul_ps(signal, response));
	}
	packedSum = _mm_hadd_ps(_mm_hadd_ps(packedSum, packedSum), packedSum);
	float sum = _mm_cvtss_f32(packedSum);

	for (UINT32 i = 0; i < remainderCount; i++)
	{
		sum += _signal[startSignalIndex - (i + wholePackCount * 4)] * hrtfData.impulse_response[i + wholePackCount * 4];
	}
	output = sum;
#else
	float sum = 0.0f;
	const auto size = hrtf_data.impulse_response.size();
	for (UINT32 i = 0; i < size; i++)
	{
		sum += _signal[start_signal_index - i] * hrtf_data.impulse_response[i];
	}
	output = sum;
#endif
}

void HrtfXapoEffect::ProcessFrame(float& leftOutput, float& rightOutput, const UINT32 frameIndex)
{
	Convolve(frameIndex, _hrtfDataLeft, leftOutput);
	Convolve(frameIndex, _hrtfDataRight, rightOutput);
}

void HrtfXapoEffect::Process(UINT32 InputProcessParameterCount, const XAPO_PROCESS_BUFFER_PARAMETERS * pInputProcessParameters, UINT32 OutputProcessParameterCount, XAPO_PROCESS_BUFFER_PARAMETERS * pOutputProcessParameters, BOOL IsEnabled)
{
	_ASSERT(IsLocked());
	_ASSERT(InputProcessParameterCount == 1);
	_ASSERT(OutputProcessParameterCount == 1);

	const UINT32 inputFrameCount = pInputProcessParameters[0].ValidFrameCount;
	const UINT32 framesToWriteCount = pOutputProcessParameters[0].ValidFrameCount;
	const auto pInput = reinterpret_cast<const float*>(pInputProcessParameters[0].pBuffer);
	const auto pOutput = reinterpret_cast<float*>(pOutputProcessParameters[0].pBuffer);
	const bool isInputValid = pInputProcessParameters[0].BufferFlags == XAPO_BUFFER_VALID;

	const auto params = reinterpret_cast<const HrtfXapoParam *>(BeginProcess());

	if (IsEnabled && _hrtfData != nullptr)
	{
		_hrtfData->GetDirectionData(params->Elevation, params->Azimuth, params->Distance, _hrtfDataLeft, _hrtfDataRight);

		if (isInputValid)
		{
			pOutputProcessParameters[0].BufferFlags = XAPO_BUFFER_VALID;
			pOutputProcessParameters[0].ValidFrameCount = inputFrameCount;

			ProcessValidBuffer(pInput, pOutput, inputFrameCount, *params);
		}
		else if (HasHistory())
		{
			UINT32 validFramesCounter;
			ProcessInvalidBuffer(pOutput, framesToWriteCount, validFramesCounter, *params);
			pOutputProcessParameters[0].BufferFlags = XAPO_BUFFER_VALID;
			pOutputProcessParameters[0].ValidFrameCount = validFramesCounter;
		}
		else
		{
			pOutputProcessParameters[0].BufferFlags = XAPO_BUFFER_SILENT;
		}
	}
	else
	{
		pOutputProcessParameters[0].BufferFlags = pInputProcessParameters[0].BufferFlags;
		pOutputProcessParameters[0].ValidFrameCount = pInputProcessParameters[0].ValidFrameCount;

		Bypass(pInput, pOutput, inputFrameCount, isInputValid);
	}

	EndProcess();
}

bool HrtfXapoEffect::HasHistory() const
{
	return _invalidBuffersCount < _buffersPerHistory;
}
