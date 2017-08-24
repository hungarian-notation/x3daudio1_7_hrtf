#pragma once

#include "IHrtfDataSet.h"
#include "HrtfData.h"
#include <map>
#include <vector>

class HrtfDataSet : public IHrtfDataSet
{
public:
	explicit HrtfDataSet(const std::vector<std::wstring> & dataFiles);

	bool HasSampleRate(uint32_t sampleRate) const override;
	const IHrtfData & GetSampleRateData(uint32_t sampleRate) const override;
private:
	std::map<uint32_t, HrtfData> _data; // sample rate - data pairs
};
