#pragma once

#include "../IHrtfDataSet.h"
#include "MhrHrtfData.h"
#include <map>
#include <vector>

class MhrHrtfDataSet : public IHrtfDataSet
{
public:
	explicit MhrHrtfDataSet(const std::vector<std::wstring> & dataFiles);

	bool HasSampleRate(uint32_t sampleRate) const override;
	const IHrtfData & GetSampleRateData(uint32_t sampleRate) const override;
private:
	std::map<uint32_t, MhrHrtfData> _data; // sample rate - data pairs
};
