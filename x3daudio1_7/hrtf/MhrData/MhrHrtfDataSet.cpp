#include "stdafx.h"
#include "MhrHrtfDataSet.h"
#include <fstream>
#include <algorithm>

MhrHrtfDataSet::MhrHrtfDataSet(const std::vector<std::wstring> & data_files)
{
	for (auto& data_file_name : data_files)
	{
		std::ifstream file(data_file_name, std::fstream::binary);

		if (!file.is_open())
			throw std::logic_error("Cannot open file");

		MhrHrtfData data(file);
		_data.emplace(data.GetSampleRate(), std::move(data));
	}
}

bool MhrHrtfDataSet::HasSampleRate(uint32_t sample_rate) const
{
	return _data.find(sample_rate) != _data.end();
}

const IHrtfData & MhrHrtfDataSet::GetSampleRateData(uint32_t sampl_rate) const
{
	return _data.at(sampl_rate);
}
