#include "stdafx.h"
#include "MhrHrtfData.h"
#include "Endianness.h"
#include <algorithm>
#include <array>
#include <limits>

typedef uint8_t file_impulse_response_length_t;
typedef uint8_t file_elevations_count_t;
typedef uint8_t file_azimuth_count_t;
typedef uint32_t file_sample_rate_t;
typedef int16_t file_sample_t;
typedef uint8_t file_delay_t;

constexpr double Pi = 3.1415926535897932385;

template <typename T>
void read_stream(std::istream & stream, T & value)
{
	stream.read(reinterpret_cast<std::istream::char_type*>(&value), sizeof(value));
	from_little_endian_inplace(value);
}

template <typename T>
constexpr float get_normalization_factor()
{
	return 1.0f / std::max(-float(std::numeric_limits<T>::min()), float(std::numeric_limits<T>::max()));
}

MhrHrtfData::MhrHrtfData(std::istream & stream)
{
	static_assert(sizeof(char) == 1, "sizeof(char) is expected to be 1 byte");
	constexpr std::array<char, 8> requiredMagic { 'M', 'i', 'n', 'P', 'H', 'R', '0', '1' };
	std::array<char, requiredMagic.size()> actualMagic;

	stream.read(actualMagic.data(), actualMagic.size());
	if (!std::equal(std::begin(requiredMagic), std::end(requiredMagic), std::begin(actualMagic), std::end(actualMagic)))
	{
		throw std::logic_error("Bad file format.");
	}

	file_sample_rate_t sampleRate;
	file_impulse_response_length_t impulseResponseLength;
	file_elevations_count_t elevationsCount;

	read_stream(stream, sampleRate);
	read_stream(stream, impulseResponseLength);
	read_stream(stream, elevationsCount);

	std::vector<ElevationData> elevations(elevationsCount);

	for (file_elevations_count_t i = 0; i < elevationsCount; i++)
	{
		file_azimuth_count_t azimuthCount;
		read_stream(stream, azimuthCount);
		elevations[i].azimuths.resize(azimuthCount);
	}

	constexpr float normalizationFactor = get_normalization_factor<file_sample_t>();

	for (auto& elevation : elevations)
	{
		for (auto& azimuth : elevation.azimuths)
		{
			azimuth.impulse_response.resize(impulseResponseLength);
			for (auto& sample : azimuth.impulse_response)
			{
				file_sample_t sampleFromFile;
				read_stream(stream, sampleFromFile);
				sample = sampleFromFile * normalizationFactor;
			}
		}
	}

	file_delay_t longestDelay = 0;
	for (auto& elevation : elevations)
	{
		for (auto& azimuth : elevation.azimuths)
		{
			file_delay_t delay;
			read_stream(stream, delay);
			azimuth.delay = delay;
			longestDelay = std::max(longestDelay, delay);
		}
	}

	_elevations = std::move(elevations);
	_response_length = impulseResponseLength;
	_sample_rate = sampleRate;
	_longest_delay = longestDelay;
}

void MhrHrtfData::GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData& refData) const
{
	_ASSERT(elevation >= -angle_t(Pi * 0.5));
	_ASSERT(elevation <= angle_t(Pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * Pi));
	_ASSERT(azimuth <= angle_t(2.0 * Pi));

	const float azimuthMod = std::fmod(azimuth + angle_t(Pi * 2.0), angle_t(Pi * 2.0));

	const angle_t elevationScaled = (elevation + angle_t(Pi * 0.5)) * (_elevations.size() - 1) / angle_t(Pi);
	const size_t elevationIndex0 = static_cast<size_t>(elevationScaled);
	const size_t elevationIndex1 = std::min(elevationIndex0 + 1, _elevations.size() - 1);
	const float elevationFractionalPart = elevationScaled - std::floor(elevationScaled);

	const angle_t azimuthScaled0 = azimuthMod * _elevations[elevationIndex0].azimuths.size() / angle_t(2 * Pi);
	const size_t azimuthIndex00 = static_cast<size_t>(azimuthScaled0) % _elevations[elevationIndex0].azimuths.size();
	const size_t azimuthIndex01 = static_cast<size_t>(azimuthScaled0 + 1) % _elevations[elevationIndex0].azimuths.size();
	const float azimuthFractionalPart0 = azimuthScaled0 - std::floor(azimuthScaled0);

	const angle_t azimuthScaled1 = azimuthMod * _elevations[elevationIndex1].azimuths.size() / angle_t(2 * Pi);
	const size_t azimuthIndex10 = static_cast<size_t>(azimuthScaled1) % _elevations[elevationIndex1].azimuths.size();
	const size_t azimuthIndex11 = static_cast<size_t>(azimuthScaled1 + 1) % _elevations[elevationIndex1].azimuths.size();
	const float azimuthFractionalPart1 = azimuthScaled1 - std::floor(azimuthScaled1);

	const float blendFactor00 = (1.0f - elevationFractionalPart) * (1.0f - azimuthFractionalPart0);
	const float blendFactor01 = (1.0f - elevationFractionalPart) * azimuthFractionalPart0;
	const float blendFactor10 = elevationFractionalPart * (1.0f - azimuthFractionalPart1);
	const float blendFactor11 = elevationFractionalPart * azimuthFractionalPart1;

	refData.delay =
		_elevations[elevationIndex0].azimuths[azimuthIndex00].delay * blendFactor00
		+ _elevations[elevationIndex0].azimuths[azimuthIndex01].delay * blendFactor01
		+ _elevations[elevationIndex1].azimuths[azimuthIndex10].delay * blendFactor10
		+ _elevations[elevationIndex1].azimuths[azimuthIndex11].delay * blendFactor11;

	if (refData.impulse_response.size() < _response_length)
		refData.impulse_response.resize(_response_length);

	for (size_t i = 0; i < _response_length; i++)
	{
		refData.impulse_response[i] =
			_elevations[elevationIndex0].azimuths[azimuthIndex00].impulse_response[i] * blendFactor00
			+ _elevations[elevationIndex0].azimuths[azimuthIndex01].impulse_response[i] * blendFactor01
			+ _elevations[elevationIndex1].azimuths[azimuthIndex10].impulse_response[i] * blendFactor10
			+ _elevations[elevationIndex1].azimuths[azimuthIndex11].impulse_response[i] * blendFactor11;
	}
}

void MhrHrtfData::GetDirectionData(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData& refDataLeft, DirectionData& refDataRight) const
{
	_ASSERT(elevation >= -angle_t(Pi * 0.5));
	_ASSERT(elevation <= angle_t(Pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * Pi));
	_ASSERT(azimuth <= angle_t(2.0 * Pi));

	GetDirectionData(elevation, azimuth, distance, refDataLeft);
	GetDirectionData(elevation, -azimuth, distance, refDataRight);
}

void MhrHrtfData::SampleDirection(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float& value, float& delay) const
{
	_ASSERT(elevation >= -angle_t(Pi * 0.5));
	_ASSERT(elevation <= angle_t(Pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * Pi));
	_ASSERT(azimuth <= angle_t(2.0 * Pi));

	const float azimuthMod = std::fmod(azimuth + angle_t(Pi * 2.0), angle_t(Pi * 2.0));

	const angle_t elevationScaled = (elevation + angle_t(Pi * 0.5)) * (_elevations.size() - 1) / angle_t(Pi);
	const size_t elevationIndex0 = static_cast<size_t>(elevationScaled);
	const size_t elevationIndex1 = std::min(elevationIndex0 + 1, _elevations.size() - 1);
	const float elevationFractionalPart = elevationScaled - std::floor(elevationScaled);

	const angle_t azimuthScaled0 = azimuthMod * _elevations[elevationIndex0].azimuths.size() / angle_t(Pi * 2.0);
	const size_t azimuthIndex00 = static_cast<size_t>(azimuthScaled0) % _elevations[elevationIndex0].azimuths.size();
	const size_t azimuthIndex01 = static_cast<size_t>(azimuthScaled0 + 1) % _elevations[elevationIndex0].azimuths.size();
	const float azimuthFractionalPart0 = azimuthScaled0 - std::floor(azimuthScaled0);

	const angle_t azimuthScaled1 = azimuthMod * _elevations[elevationIndex1].azimuths.size() / angle_t(Pi * 2.0);
	const size_t azimuthIndex10 = static_cast<size_t>(azimuthScaled1) % _elevations[elevationIndex1].azimuths.size();
	const size_t azimuthIndex11 = static_cast<size_t>(azimuthScaled1 + 1) % _elevations[elevationIndex1].azimuths.size();
	const float azimuthFractionalPart1 = azimuthScaled1 - std::floor(azimuthScaled1);

	const float blendFactor00 = (1.0f - elevationFractionalPart) * (1.0f - azimuthFractionalPart0);
	const float blendFactor01 = (1.0f - elevationFractionalPart) * azimuthFractionalPart0;
	const float blendFactor10 = elevationFractionalPart * (1.0f - azimuthFractionalPart1);
	const float blendFactor11 = elevationFractionalPart * azimuthFractionalPart1;

	delay =
		_elevations[elevationIndex0].azimuths[azimuthIndex00].delay * blendFactor00
		+ _elevations[elevationIndex0].azimuths[azimuthIndex01].delay * blendFactor01
		+ _elevations[elevationIndex1].azimuths[azimuthIndex10].delay * blendFactor10
		+ _elevations[elevationIndex1].azimuths[azimuthIndex11].delay * blendFactor11;

	value =
		_elevations[elevationIndex0].azimuths[azimuthIndex00].impulse_response[sample] * blendFactor00
		+ _elevations[elevationIndex0].azimuths[azimuthIndex01].impulse_response[sample] * blendFactor01
		+ _elevations[elevationIndex1].azimuths[azimuthIndex10].impulse_response[sample] * blendFactor10
		+ _elevations[elevationIndex1].azimuths[azimuthIndex11].impulse_response[sample] * blendFactor11;
}

void MhrHrtfData::SampleDirection(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float& valueLeft, float& delayLeft, float& valueRight, float& delayRight) const
{
	_ASSERT(elevation >= -angle_t(Pi * 0.5));
	_ASSERT(elevation <= angle_t(Pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * Pi));
	_ASSERT(azimuth <= angle_t(2.0 * Pi));

	SampleDirection(elevation, azimuth, distance, sample, valueLeft, delayLeft);
	SampleDirection(elevation, -azimuth, distance, sample, valueRight, delayRight);
}