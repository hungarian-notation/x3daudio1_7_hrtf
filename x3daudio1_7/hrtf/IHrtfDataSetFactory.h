#pragma once

class IHrtfDataSet;

class IHrtfDataSetFactory
{
public:
	virtual ~IHrtfDataSetFactory() = default;

	virtual IHrtfDataSet Create() = 0;
};
