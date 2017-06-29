#pragma once

#include "math/math_types.h"

#include <X3DAudio.h>
#include <crtdbg.h>
#include "graph/ISpatializedDataExtractor.h"
#include "logger.h"

inline float sample_volume_curve(const X3DAUDIO_EMITTER* pEmitter, float distance)
{
	const float normalized_distance = distance / pEmitter->CurveDistanceScaler;

	if (pEmitter->pVolumeCurve == nullptr)
	{
		const float clamped_distance = std::max(1.0f, normalized_distance);
		return 1.0f / (clamped_distance * clamped_distance);
	}

	if (pEmitter->pVolumeCurve->PointCount == 0)
	{
		logger::logRelease("Warning: no points in the volume curve");
		return 0;
	}

	const std::size_t pointCount = pEmitter->pVolumeCurve->PointCount;
	const auto points = pEmitter->pVolumeCurve->pPoints;

	if (pointCount < 2)
		throw std::exception();

	if (normalized_distance >= 1.0f)
		return points[pointCount - 1].DSPSetting;

	std::size_t furtherPointIndex = pointCount - 1;
	for (std::size_t i = 1; i < pointCount; i++)
	{
		if (points[i].Distance >= normalized_distance)
		{
			furtherPointIndex = i;
			break;
		}
	}

	float t = (normalized_distance - points[furtherPointIndex - 1].Distance) / (points[furtherPointIndex].Distance - points[furtherPointIndex - 1].Distance);
	return points[furtherPointIndex - 1].DSPSetting  * (1.0f - t) + points[furtherPointIndex].Distance * t;

	const auto greater_point = std::upper_bound(pEmitter->pVolumeCurve->pPoints, pEmitter->pVolumeCurve->pPoints + pEmitter->pVolumeCurve->PointCount, normalized_distance, [=](const auto & dist, const auto & point) { return dist < point.Distance; });
	if (greater_point != pEmitter->pVolumeCurve->pPoints + pEmitter->pVolumeCurve->PointCount)
	{
		if (greater_point > &pEmitter->pVolumeCurve->pPoints[0])
		{
			const auto * less_point = greater_point - 1;

			const float fraction = (normalized_distance - less_point->Distance) / (greater_point->Distance - less_point->Distance);
			return less_point->DSPSetting + (greater_point->DSPSetting - less_point->DSPSetting) * fraction;
		}
		else
		{
			return greater_point->DSPSetting;
		}
	}
	else
	{
		return (greater_point - 1)->DSPSetting;
	}
}

inline SpatialData CommonX3DAudioCalculate(const X3DAUDIO_LISTENER * pListener, const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS * pDSPSettings)
{
	if (Flags & X3DAUDIO_CALCULATE_DELAY)
	{
		_ASSERT(pDSPSettings->pDelayTimes != nullptr);
		// The ITD is simulated with HRTF DSP, so here we just take the minimum one.
		pDSPSettings->pDelayTimes[0] = pDSPSettings->pDelayTimes[1] = std::min(pDSPSettings->pDelayTimes[0], pDSPSettings->pDelayTimes[1]);
	}

	// changing left-hand to ortodox right-hand
	const math::vector3 listener_position(pListener->Position.x, pListener->Position.y, pListener->Position.z);
	const math::vector3 emitter_position(pEmitter->Position.x, pEmitter->Position.y, pEmitter->Position.z);

	const auto listener_to_emitter = emitter_position - listener_position;

	const math::vector3 listener_frame_front(pListener->OrientFront.x, pListener->OrientFront.y, pListener->OrientFront.z);
	const math::vector3 listener_frame_up(pListener->OrientTop.x, pListener->OrientTop.y, pListener->OrientTop.z);
	const auto listener_frame_right = math::cross(listener_frame_up, listener_frame_front);

	const math::matrix3 listener_basis(listener_frame_right, listener_frame_up, listener_frame_front);
	const auto world_to_listener_matrix = (listener_basis);

	SpatialData spatialData;

	const auto relative_position = world_to_listener_matrix * listener_to_emitter;
	const auto distance = math::length(listener_to_emitter);

	spatialData.present = true;
	spatialData.volume_multiplier = sample_volume_curve(pEmitter, distance);
	spatialData.azimuth = distance > std::numeric_limits<float>::epsilon() ? std::atan2(relative_position[0], relative_position[2]) : 0.0f;
	spatialData.elevation = distance > std::numeric_limits<float>::epsilon() ? std::asin(math::normalize(relative_position)[1]) : 0.0f;
	spatialData.distance = distance;

	return spatialData;
}