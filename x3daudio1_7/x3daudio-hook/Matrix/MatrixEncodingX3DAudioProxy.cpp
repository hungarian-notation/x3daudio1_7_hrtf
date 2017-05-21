#include "stdafx.h"
#include "MatrixEncodingX3DAudioProxy.h"
#include "ISound3DRegistry.h"
#include "ChannelMatrixMagic.h"

#include "math/math_types.h"

#include "logger.h"
#include <limits>
#include "proxy/XAudio2VoiceProxy.h"


MatrixEncodingX3DAudioProxy::MatrixEncodingX3DAudioProxy(ISound3DRegistry * registry)
	: m_registry(registry)
{
}


void MatrixEncodingX3DAudioProxy::X3DAudioCalculate(const X3DAUDIO_LISTENER * pListener, const X3DAUDIO_EMITTER * pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS * pDSPSettings)
{
	if (Flags & X3DAUDIO_CALCULATE_DELAY)
	{
		_ASSERT(pDSPSettings->pDelayTimes != nullptr);
		// The ITD is simulated with HRTF DSP, so here we just take the minimum one.
		pDSPSettings->pDelayTimes[0] = pDSPSettings->pDelayTimes[1] = std::min(pDSPSettings->pDelayTimes[0], pDSPSettings->pDelayTimes[1]);
	}

	// changing left-hand to ortodox right-hand
	math::vector3 listener_position(pListener->Position.x, pListener->Position.y, pListener->Position.z);
	math::vector3 emitter_position(pEmitter->Position.x, pEmitter->Position.y, pEmitter->Position.z);

	const auto listener_to_emitter = emitter_position - listener_position;

	math::vector3 listener_frame_front(pListener->OrientFront.x, pListener->OrientFront.y, pListener->OrientFront.z);
	math::vector3 listener_frame_up(pListener->OrientTop.x, pListener->OrientTop.y, pListener->OrientTop.z);
	const auto listener_frame_right = math::cross(listener_frame_up, listener_frame_front);

	math::matrix3 listener_basis(listener_frame_right, listener_frame_up, listener_frame_front);
	const auto world_to_listener_matrix = (listener_basis);

	Sound3DEntry entry;

	const auto relative_position = world_to_listener_matrix * listener_to_emitter;
	const auto distance = math::length(listener_to_emitter);

	entry.volume_multiplier = sample_volume_curve(pEmitter, distance);
	entry.azimuth = distance > std::numeric_limits<float>::epsilon() ? std::atan2(relative_position[0], relative_position[2]) : 0.0f;
	entry.elevation = distance > std::numeric_limits<float>::epsilon() ? std::asin(math::normalize(relative_position)[1]) : 0.0f;
	entry.distance = distance;

	const auto id = m_registry->CreateEntry(entry);

	embed_sound_id(pDSPSettings->pMatrixCoefficients, pDSPSettings->SrcChannelCount, pDSPSettings->DstChannelCount, id);
}



SpatialData MatrixEncodingX3DAudioProxy::ExtractSpatialData(XAudio2VoiceProxy * source, IXAudio2Voice * pDestinationProxy)
{
	const auto clientMatrix = source->getOutputMatrix(pDestinationProxy);

	if (does_matrix_contain_id(clientMatrix))
	{
		const sound_id id = extract_sound_id(clientMatrix);

		const auto sound3d = m_registry->GetEntry(id);

		SpatialData spatialData;
		spatialData.present = true;
		spatialData.volume_multiplier = sound3d.volume_multiplier;
		spatialData.elevation = sound3d.elevation;
		spatialData.azimuth = sound3d.azimuth;
		spatialData.distance = sound3d.distance;

		return spatialData;
	}
	else
	{
		SpatialData spatialData;
		spatialData.present = false;
		return spatialData;
	}
}

float MatrixEncodingX3DAudioProxy::sample_volume_curve(const X3DAUDIO_EMITTER* pEmitter, float distance)
{
	const float normalized_distance = distance / pEmitter->CurveDistanceScaler;

	if (pEmitter->pVolumeCurve == nullptr)
	{
		const float clamped_distance = std::max(1.0f, normalized_distance);
		return 1.0f / clamped_distance;
	}

	if (pEmitter->pVolumeCurve->PointCount == 0)
	{
		logger::logRelease("Warning: no points in the volume curve");
		return 0;
	}

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
