#pragma once
#include <cstdint>

class IHrtfData;

class IHrtfDataSet
{
public:
	virtual ~IHrtfDataSet() = default;
	virtual bool HasSampleRate(uint32_t sampleRate) const = 0;
	virtual const IHrtfData & GetSampleRateData(uint32_t sampleRate) const = 0;
};