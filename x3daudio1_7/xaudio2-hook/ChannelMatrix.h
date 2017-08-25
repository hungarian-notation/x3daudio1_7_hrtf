#pragma once

#include <vector>
#include <algorithm>

class ChannelMatrix
{
public:
	ChannelMatrix()
		: _sourceCount(0)
		, _destinationCount(0)
	{

	}

	ChannelMatrix(unsigned int sourceCount, unsigned int destinationCount)
		: _sourceCount(sourceCount)
		, _destinationCount(destinationCount)
		, _values(sourceCount * destinationCount)
	{
		
	}

	ChannelMatrix(const float * values, unsigned int sourceCount, unsigned int destinationCount)
		: _sourceCount(sourceCount)
		, _destinationCount(destinationCount)
		, _values(sourceCount * destinationCount)
	{
		const auto size = sourceCount * destinationCount;
		std::copy(values, values + size, std::begin(_values));
	}

	unsigned int GetSourceCount() const
	{
		return _sourceCount;
	}

	unsigned int GetDestinationCount() const
	{
		return _destinationCount;
	}

	float GetValue(unsigned int source, unsigned int destination) const
	{
		_ASSERT(source < _sourceCount || destination < _destinationCount);

		return _values[destination * _sourceCount + source];
	}

	void SetValue(unsigned int source, unsigned int destination, float value)
	{
		_ASSERT(source < _sourceCount || destination < _destinationCount);

		_values[destination * _sourceCount + source] = value;
	}

private:
	unsigned int _sourceCount;
	unsigned int _destinationCount;
	std::vector<float> _values;
};