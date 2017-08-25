#pragma once

#include "HrtfTypes.h"

class IHrtfData
{
public:
	virtual ~IHrtfData() = default;

	virtual void GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData & refDataLeft, DirectionData & refDataRight) const = 0;

	virtual uint32_t GetSampleRate() const = 0;
	virtual uint32_t GetResponseLength() const = 0;
	virtual uint32_t GetLongestDelay() const = 0;
};