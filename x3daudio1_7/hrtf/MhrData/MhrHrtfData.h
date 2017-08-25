#pragma once

#include "../IHrtfData.h"
#include <cstdint>
#include <vector>
#include <iostream>


class MhrHrtfData : public IHrtfData
{
public:
	explicit MhrHrtfData(std::istream & stream);

	void GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData & refDataLeft, DirectionData & refDataRight) const override;

	uint32_t GetSampleRate() const override { return _sampleRate; }
	uint32_t GetResponseLength() const override { return _responseLength; }
	uint32_t GetLongestDelay() const override { return _longestDelay; }

private:
	void GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData & ref_data) const;
private:
	struct ElevationData
	{
		std::vector<DirectionData> azimuths;
	};


	uint32_t _sampleRate;
	uint32_t _responseLength;
	uint32_t _longestDelay;
	std::vector<ElevationData> _elevations;
};