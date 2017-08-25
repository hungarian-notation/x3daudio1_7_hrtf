#pragma once

#include "../IHrtfData.h"
#include <cstdint>
#include <vector>
#include <iostream>


class MhrHrtfData : public IHrtfData
{
public:
	MhrHrtfData(std::istream & stream);

	void GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData & ref_data_left, DirectionData & ref_data_right) const override;
	void SampleDirection(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float& value_left, float& delay_left, float& value_right, float& delay_right) const override;

	uint32_t GetSampleRate() const override { return _sample_rate; }
	uint32_t GetResponseLength() const override { return _response_length; }
	uint32_t GetLongestDelay() const override { return _longest_delay; }

private:
	void GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData & ref_data) const;
	void SampleDirection(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float& value, float& delay) const;
private:
	struct ElevationData
	{
		std::vector<DirectionData> azimuths;
	};


	uint32_t _sample_rate;
	uint32_t _response_length;
	uint32_t _longest_delay;
	std::vector<ElevationData> _elevations;
};