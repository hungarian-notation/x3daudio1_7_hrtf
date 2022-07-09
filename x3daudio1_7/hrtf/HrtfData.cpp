#include "stdafx.h"
#include "HrtfData.h"
#include "Endianness.h"
#include <algorithm>

typedef uint8_t file_impulse_response_length_t;
typedef uint8_t file_elevations_count_t;
typedef uint8_t file_azimuth_count_t;
typedef uint32_t file_sample_rate_t;
typedef int16_t file_sample_t;
typedef uint8_t file_delay_t;

const double pi = 3.1415926535897932385;

template <typename T>
void read_stream(std::istream & stream, T & value)
{
	stream.read(reinterpret_cast<std::istream::char_type*>(&value), sizeof(value));
	from_little_endian_inplace(value);
}

HrtfData::HrtfData(std::istream & stream)
{
	const char required_magic[] = { 'M', 'i', 'n', 'P', 'H', 'R', '0', '3' };
	char actual_magic[sizeof(required_magic) / sizeof(required_magic[0])];

	stream.read(actual_magic, sizeof(actual_magic));
	if (!std::equal(std::begin(required_magic), std::end(required_magic), std::begin(actual_magic), std::end(actual_magic)))
	{
		throw std::logic_error("Bad file format.");
	}

	file_sample_rate_t sample_rate;
	file_impulse_response_length_t impulse_response_length;
	file_distances_count_t distances_count;

	read_stream(stream, sample_rate);
	read_stream(stream, impulse_response_length);
	read_stream(stream, distances_count);
	
	std::vector<DistanceData> distances(distances_count);
	
	for (file_distances_count_t i = distances_count - 1; i < distances_count; i--)
	{
		file_elevations_count_t elevations_count;
		read_stream(stream, elevations_count);
		distances[i].elevations.resize(elevations_count);
		
		for (file_elevations_count j = 0; j < elevations_count; j++)
		{
			file_azimuth_count azimuth_count;
			read_stream(stream, azimuth_count);
			distances[i].elevations[j].azimuths.resize(azimuth_count);
		}
	}

	const float normalization_factor = 1.0f / float(1 << (sizeof(file_sample_t) * 8 - 1));

	for (auto& distance : distances)
	{
	        for (auto& elevation : distance.elevations)
	        {
		        for (auto& azimuth : elevation.azimuths)
		        {
			        azimuth.impulse_response.resize(impulse_response_length);
			        for (auto& sample : azimuth.impulse_response)
			        {
				        file_sample_t sample_from_file;
				        read_stream(stream, sample_from_file);
				        sample = sample_from_file * normalization_factor;
				}
			}
		}
	}

	file_delay_t longest_delay = 0;
	for (auto& distance : distances)
	        for (auto& elevation : distance.elevations)
	        {
		        for (auto& azimuth : elevation.azimuths)
		        {
			        file_delay_t delay;
			        read_stream(stream, delay);
			        azimuth.delay = delay;
			        longest_delay = std::max(longest_delay, delay);
			}
		}
	}

	m_distances = std::move(distances);
	m_response_length = impulse_response_length;
	m_sample_rate = sample_rate;
	m_longest_delay = longest_delay;
}

void HrtfData::get_direction_data(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData& ref_data) const
{
	_ASSERT(elevation >= -angle_t(pi * 0.5));
	_ASSERT(elevation <= angle_t(pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * pi));
	_ASSERT(azimuth <= angle_t(2.0 * pi));

	const float azimuth_mod = std::fmod(azimuth + angle_t(pi * 2.0), angle_t(pi * 2.0));

	const angle_t elevation_scaled = (elevation + angle_t(pi * 0.5)) * (m_elevations.size() - 1) / angle_t(pi);
	const size_t elevation_index0 = static_cast<size_t>(elevation_scaled);
	const size_t elevation_index1 = std::min(elevation_index0 + 1, m_elevations.size() - 1);
	const float elevation_fractional_part = elevation_scaled - std::floor(elevation_scaled);

	const angle_t azimuth_scaled0 = azimuth_mod * m_elevations[elevation_index0].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index00 = static_cast<size_t>(azimuth_scaled0) % m_elevations[elevation_index0].azimuths.size();
	const size_t azimuth_index01 = static_cast<size_t>(azimuth_scaled0 + 1) % m_elevations[elevation_index0].azimuths.size();
	const float azimuth_fractional_part0 = azimuth_scaled0 - std::floor(azimuth_scaled0);

	const angle_t azimuth_scaled1 = azimuth_mod * m_elevations[elevation_index1].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index10 = static_cast<size_t>(azimuth_scaled1) % m_elevations[elevation_index1].azimuths.size();
	const size_t azimuth_index11 = static_cast<size_t>(azimuth_scaled1 + 1) % m_elevations[elevation_index1].azimuths.size();
	const float azimuth_fractional_part1 = azimuth_scaled1 - std::floor(azimuth_scaled1);

	const float blend_factor_00 = (1.0f - elevation_fractional_part) * (1.0f - azimuth_fractional_part0);
	const float blend_factor_01 = (1.0f - elevation_fractional_part) * azimuth_fractional_part0;
	const float blend_factor_10 = elevation_fractional_part * (1.0f - azimuth_fractional_part1);
	const float blend_factor_11 = elevation_fractional_part * azimuth_fractional_part1;

	ref_data.delay =
		m_elevations[elevation_index0].azimuths[azimuth_index00].delay * blend_factor_00
		+ m_elevations[elevation_index0].azimuths[azimuth_index01].delay * blend_factor_01
		+ m_elevations[elevation_index1].azimuths[azimuth_index10].delay * blend_factor_10
		+ m_elevations[elevation_index1].azimuths[azimuth_index11].delay * blend_factor_11;

	if (ref_data.impulse_response.size() < m_response_length)
		ref_data.impulse_response.resize(m_response_length);

	for (size_t i = 0; i < m_response_length; i++)
	{
		ref_data.impulse_response[i] =
			m_elevations[elevation_index0].azimuths[azimuth_index00].impulse_response[i] * blend_factor_00
			+ m_elevations[elevation_index0].azimuths[azimuth_index01].impulse_response[i] * blend_factor_01
			+ m_elevations[elevation_index1].azimuths[azimuth_index10].impulse_response[i] * blend_factor_10
			+ m_elevations[elevation_index1].azimuths[azimuth_index11].impulse_response[i] * blend_factor_11;
	}
}

void HrtfData::get_direction_data(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData& ref_data_left, DirectionData& ref_data_right) const
{
	_ASSERT(elevation >= -angle_t(pi * 0.5));
	_ASSERT(elevation <= angle_t(pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * pi));
	_ASSERT(azimuth <= angle_t(2.0 * pi));

	get_direction_data(elevation, azimuth, distance, ref_data_left);
	get_direction_data(elevation, -azimuth, distance, ref_data_right);
}

void HrtfData::sample_direction(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float& value, float& delay) const
{
	_ASSERT(elevation >= -angle_t(pi * 0.5));
	_ASSERT(elevation <= angle_t(pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * pi));
	_ASSERT(azimuth <= angle_t(2.0 * pi));

	const float azimuth_mod = std::fmod(azimuth + angle_t(pi * 2.0), angle_t(pi * 2.0));

	const angle_t elevation_scaled = (elevation + angle_t(pi * 0.5)) * (m_elevations.size() - 1) / angle_t(pi);
	const size_t elevation_index0 = static_cast<size_t>(elevation_scaled);
	const size_t elevation_index1 = std::min(elevation_index0 + 1, m_elevations.size() - 1);
	const float elevation_fractional_part = elevation_scaled - std::floor(elevation_scaled);

	const angle_t azimuth_scaled0 = azimuth_mod * m_elevations[elevation_index0].azimuths.size() / angle_t(pi * 2.0);
	const size_t azimuth_index00 = static_cast<size_t>(azimuth_scaled0) % m_elevations[elevation_index0].azimuths.size();
	const size_t azimuth_index01 = static_cast<size_t>(azimuth_scaled0 + 1) % m_elevations[elevation_index0].azimuths.size();
	const float azimuth_fractional_part0 = azimuth_scaled0 - std::floor(azimuth_scaled0);

	const angle_t azimuth_scaled1 = azimuth_mod * m_elevations[elevation_index1].azimuths.size() / angle_t(pi * 2.0);
	const size_t azimuth_index10 = static_cast<size_t>(azimuth_scaled1) % m_elevations[elevation_index1].azimuths.size();
	const size_t azimuth_index11 = static_cast<size_t>(azimuth_scaled1 + 1) % m_elevations[elevation_index1].azimuths.size();
	const float azimuth_fractional_part1 = azimuth_scaled1 - std::floor(azimuth_scaled1);

	const float blend_factor_00 = (1.0f - elevation_fractional_part) * (1.0f - azimuth_fractional_part0);
	const float blend_factor_01 = (1.0f - elevation_fractional_part) * azimuth_fractional_part0;
	const float blend_factor_10 = elevation_fractional_part * (1.0f - azimuth_fractional_part1);
	const float blend_factor_11 = elevation_fractional_part * azimuth_fractional_part1;

	delay =
		m_elevations[elevation_index0].azimuths[azimuth_index00].delay * blend_factor_00
		+ m_elevations[elevation_index0].azimuths[azimuth_index01].delay * blend_factor_01
		+ m_elevations[elevation_index1].azimuths[azimuth_index10].delay * blend_factor_10
		+ m_elevations[elevation_index1].azimuths[azimuth_index11].delay * blend_factor_11;

	value =
		m_elevations[elevation_index0].azimuths[azimuth_index00].impulse_response[sample] * blend_factor_00
		+ m_elevations[elevation_index0].azimuths[azimuth_index01].impulse_response[sample] * blend_factor_01
		+ m_elevations[elevation_index1].azimuths[azimuth_index10].impulse_response[sample] * blend_factor_10
		+ m_elevations[elevation_index1].azimuths[azimuth_index11].impulse_response[sample] * blend_factor_11;
}

void HrtfData::sample_direction(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float& value_left, float& delay_left, float& value_right, float& delay_right) const
{
	_ASSERT(elevation >= -angle_t(pi * 0.5));
	_ASSERT(elevation <= angle_t(pi * 0.5));
	_ASSERT(azimuth >= -angle_t(2.0 * pi));
	_ASSERT(azimuth <= angle_t(2.0 * pi));

	sample_direction(elevation, azimuth, distance, sample, value_left, delay_left);
	sample_direction(elevation, -azimuth, distance, sample, value_right, delay_right);
}
