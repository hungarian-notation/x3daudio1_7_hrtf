#include "stdafx.h"
#include "HrtfDataSet.h"
#include <fstream>
#include <algorithm>

HrtfDataSet::HrtfDataSet(const std::vector<std::wstring> & data_files)
{
	for (auto& data_file_name : data_files)
	{
		std::ifstream file(data_file_name, std::fstream::binary);

		if (!file.is_open())
			throw std::logic_error("Cannot open file");

		HrtfData data(file);
		_data.emplace(data.GetSampleRate(), std::move(data));
	}
}

bool HrtfDataSet::HasSampleRate(uint32_t sample_rate) const
{
	return _data.find(sample_rate) != _data.end();
}

const IHrtfData & HrtfDataSet::GetSampleRateData(uint32_t sampl_rate) const
{
	return _data.at(sampl_rate);
}
