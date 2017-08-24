#pragma once

#include "HrtfTypes.h"

class IHrtfData
{
public:
	virtual ~IHrtfData() = default;

	virtual void GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData & refData) const = 0;
	virtual void GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData & refDataLeft, DirectionData & refDataRight) const = 0;
	// Get only once IR sample at given direction. The delay returned is the delay of IR's beginning, not the sample's!
	virtual void SampleDirection(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float & value, float & delay) const = 0;
	// Get only once IR sample at given direction for both channels. The delay returned is the delay of IR's beginning, not the sample's!
	virtual void SampleDirection(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float & valueLeft, float & delayLeft, float & valueRight, float & delayRight) const = 0;

	virtual uint32_t GetSampleRate() const = 0;
	virtual uint32_t GetRespooneLength() const = 0;
	virtual uint32_t GetLongestDelay() const = 0;
};