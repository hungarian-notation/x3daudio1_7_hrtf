#pragma once

struct HrtfXapoParam
{
	// sound volume [0; 1]
	float VolumeMultiplier;

	// elevation, in listener's local space, in radians in the range [-pi/2; +pi/2] where 0 is horizontal direction.
	float Elevation;

	// azimuth, in listener's local space, in radians in the range [-2pi; +2pi] where 0 is forward direction.
	float Azimuth;

	// distance to the source [0; +inf)
	float Distance;
};