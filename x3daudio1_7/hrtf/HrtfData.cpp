
#include "HrtfData.h"
#include "Endianness.h"
#include <algorithm>
#include <cmath>

typedef uint8_t file_impulse_response_length_t;
typedef uint8_t file_channel_type_t;
typedef uint8_t file_distances_count_t;
typedef uint8_t file_elevations_count_t;
typedef uint8_t file_azimuth_count_t;
typedef uint32_t file_sample_rate_t;
typedef struct { union { int32_t i:24; uint8_t b[3]; } u; } file_sample_t;
typedef uint8_t file_delay_t;
typedef uint16_t file_distance_t;

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
	file_channel_type_t channel_type;
	file_impulse_response_length_t impulse_response_length;
	file_distances_count_t distances_count;

	read_stream(stream, sample_rate);
	read_stream(stream, channel_type);
	read_stream(stream, impulse_response_length);
	read_stream(stream, distances_count);
	
	if (channel_type != 0 && channel_type != 1)
	{
		throw std::logic_error("Bad file format.");
	}

	size_t channel_count = channel_type == 1 ? 2 : 1;

	std::vector<DistanceData> distances(distances_count);
	
	for (file_distances_count_t i = 0; i < distances_count; i++)
	{
		file_distance_t distance;
		read_stream(stream, distance);
		distances[i].distance = float(distance) / 1000.0;
		
		file_elevations_count_t elevations_count;
		read_stream(stream, elevations_count);
		distances[i].elevations.resize(elevations_count);
		
		for (file_elevations_count_t j = 0; j < elevations_count; j++)
		{
			file_azimuth_count_t azimuth_count;
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
				azimuth.impulse_response.resize(impulse_response_length * channel_count);
				for (auto& sample : azimuth.impulse_response)
				{
					file_sample_t sample_from_file;
					read_stream(stream, sample_from_file);
					sample = sample_from_file.u.i * normalization_factor;
				}
			}
		}
	}

	file_delay_t longest_delay = 0;
	for (auto& distance : distances)
	{
		for (auto& elevation : distance.elevations)
		{
			for (auto& azimuth : elevation.azimuths)
			{
				file_delay_t delay;
				read_stream(stream, delay);
				azimuth.delay = delay;
				longest_delay = std::max(longest_delay, delay);
				if (channel_type == 1)
				{
					read_stream(stream, delay);
					azimuth.delay_right = delay;
					longest_delay = std::max(longest_delay, delay);
				}
			}
		}
	}
	
	std::sort(distances.begin(), distances.end(),
			  [](const DistanceData &lhs, const DistanceData &rhs) noexcept
			  { return lhs.distance > rhs.distance; });

	m_distances = std::move(distances);
	m_channel_count = channel_count;
	m_response_length = impulse_response_length;
	m_sample_rate = sample_rate;
	m_longest_delay = longest_delay;
}

void HrtfData::get_direction_data(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t channel, DirectionData& ref_data) const
{
	assert(elevation >= -angle_t(pi * 0.5));
	assert(elevation <= angle_t(pi * 0.5));
	assert(azimuth >= -angle_t(2.0 * pi));
	assert(azimuth <= angle_t(2.0 * pi));

	const float azimuth_mod = std::fmod(azimuth + angle_t(pi * 2.0), angle_t(pi * 2.0));
	
	size_t distance_index0 = 0;
	while(distance_index0 < m_distances.size() - 1 &&
		  m_distances[distance_index0].distance > distance)
	{
		distance_index0++;
	}
	const size_t distance_index1 = std::min(distance_index0 + 1, m_distances.size() - 1);
	const distance_t distance0 = m_distances[distance_index0].distance;
	const distance_t distance1 = m_distances[distance_index1].distance;
	const distance_t distance_delta = distance0 - distance1;
	const float distance_fractional_part = distance_delta ? (distance - distance1) / distance_delta : 0;
	
	const auto &elevations0 = m_distances[distance_index0].elevations;
	const auto &elevations1 = m_distances[distance_index1].elevations;

	const angle_t elevation_scaled0 = (elevation + angle_t(pi * 0.5)) * (elevations0.size() - 1) / angle_t(pi);
	const angle_t elevation_scaled1 = (elevation + angle_t(pi * 0.5)) * (elevations1.size() - 1) / angle_t(pi);
	const size_t elevation_index00 = static_cast<size_t>(elevation_scaled0);
	const size_t elevation_index10 = static_cast<size_t>(elevation_scaled1);
	const size_t elevation_index01 = std::min(elevation_index00 + 1, elevations0.size() - 1);
	const size_t elevation_index11 = std::min(elevation_index10 + 1, elevations1.size() - 1);
	
	const float elevation_fractional_part0 = std::fmod(elevation_scaled0, 1.0);
	const float elevation_fractional_part1 = std::fmod(elevation_scaled1, 1.0);

	const angle_t azimuth_scaled00 = azimuth_mod * elevations0[elevation_index00].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index000 = static_cast<size_t>(azimuth_scaled00) % elevations0[elevation_index00].azimuths.size();
	const size_t azimuth_index001 = static_cast<size_t>(azimuth_scaled00 + 1) % elevations0[elevation_index00].azimuths.size();
	const float azimuth_fractional_part00 = std::fmod(azimuth_scaled00, 1.0);

	const angle_t azimuth_scaled10 = azimuth_mod * elevations1[elevation_index10].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index100 = static_cast<size_t>(azimuth_scaled10) % elevations1[elevation_index10].azimuths.size();
	const size_t azimuth_index101 = static_cast<size_t>(azimuth_scaled10 + 1) % elevations1[elevation_index10].azimuths.size();
	const float azimuth_fractional_part10 = std::fmod(azimuth_scaled10, 1.0);

	const angle_t azimuth_scaled01 = azimuth_mod * elevations0[elevation_index01].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index010 = static_cast<size_t>(azimuth_scaled01) % elevations0[elevation_index01].azimuths.size();
	const size_t azimuth_index011 = static_cast<size_t>(azimuth_scaled01 + 1) % elevations0[elevation_index01].azimuths.size();
	const float azimuth_fractional_part01 = std::fmod(azimuth_scaled01, 1.0);

	const angle_t azimuth_scaled11 = azimuth_mod * elevations1[elevation_index11].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index110 = static_cast<size_t>(azimuth_scaled11) % elevations1[elevation_index11].azimuths.size();
	const size_t azimuth_index111 = static_cast<size_t>(azimuth_scaled11 + 1) % elevations1[elevation_index11].azimuths.size();
	const float azimuth_fractional_part11 = std::fmod(azimuth_scaled11, 1.0);

	const float blend_factor_000 = (1.0f - elevation_fractional_part0) * (1.0f - azimuth_fractional_part00) * distance_fractional_part;
	const float blend_factor_001 = (1.0f - elevation_fractional_part0) * azimuth_fractional_part00 * distance_fractional_part;
	const float blend_factor_010 = elevation_fractional_part0 * (1.0f - azimuth_fractional_part01) * distance_fractional_part;
	const float blend_factor_011 = elevation_fractional_part0 * azimuth_fractional_part01 * distance_fractional_part;

	const float blend_factor_100 = (1.0f - elevation_fractional_part1) * (1.0f - azimuth_fractional_part10) * (1.0f - distance_fractional_part);
	const float blend_factor_101 = (1.0f - elevation_fractional_part1) * azimuth_fractional_part10 * (1.0f - distance_fractional_part);
	const float blend_factor_110 = elevation_fractional_part1 * (1.0f - azimuth_fractional_part11) * (1.0f - distance_fractional_part);
	const float blend_factor_111 = elevation_fractional_part1 * azimuth_fractional_part11 * (1.0f - distance_fractional_part);

	delay_t delay0;
	delay_t delay1;

	if(channel == 0) {
		delay0 =
			elevations0[elevation_index00].azimuths[azimuth_index000].delay * blend_factor_000
			+ elevations0[elevation_index00].azimuths[azimuth_index001].delay * blend_factor_001
			+ elevations0[elevation_index01].azimuths[azimuth_index010].delay * blend_factor_010
			+ elevations0[elevation_index01].azimuths[azimuth_index011].delay * blend_factor_011;
		
		delay1 =
			elevations1[elevation_index10].azimuths[azimuth_index100].delay * blend_factor_100
			+ elevations1[elevation_index10].azimuths[azimuth_index101].delay * blend_factor_101
			+ elevations1[elevation_index11].azimuths[azimuth_index110].delay * blend_factor_110
			+ elevations1[elevation_index11].azimuths[azimuth_index111].delay * blend_factor_111;
	} else {
		delay0 =
			elevations0[elevation_index00].azimuths[azimuth_index000].delay_right * blend_factor_000
			+ elevations0[elevation_index00].azimuths[azimuth_index001].delay_right * blend_factor_001
			+ elevations0[elevation_index01].azimuths[azimuth_index010].delay_right * blend_factor_010
			+ elevations0[elevation_index01].azimuths[azimuth_index011].delay_right * blend_factor_011;
		
		delay1 =
			elevations1[elevation_index10].azimuths[azimuth_index100].delay_right * blend_factor_100
			+ elevations1[elevation_index10].azimuths[azimuth_index101].delay_right * blend_factor_101
			+ elevations1[elevation_index11].azimuths[azimuth_index110].delay_right * blend_factor_110
			+ elevations1[elevation_index11].azimuths[azimuth_index111].delay_right * blend_factor_111;
	}
	
	ref_data.delay = delay0 + delay1;

	if (ref_data.impulse_response.size() < m_response_length)
		ref_data.impulse_response.resize(m_response_length);

	for (size_t i = 0, j = channel; i < m_response_length; i++, j += m_channel_count)
	{
		float sample0 =
			elevations0[elevation_index00].azimuths[azimuth_index000].impulse_response[j] * blend_factor_000
			+ elevations0[elevation_index00].azimuths[azimuth_index001].impulse_response[j] * blend_factor_001
			+ elevations0[elevation_index01].azimuths[azimuth_index010].impulse_response[j] * blend_factor_010
			+ elevations0[elevation_index01].azimuths[azimuth_index011].impulse_response[j] * blend_factor_011;
		float sample1 =
			elevations1[elevation_index10].azimuths[azimuth_index100].impulse_response[j] * blend_factor_100
			+ elevations1[elevation_index10].azimuths[azimuth_index101].impulse_response[j] * blend_factor_101
			+ elevations1[elevation_index11].azimuths[azimuth_index110].impulse_response[j] * blend_factor_110
			+ elevations1[elevation_index11].azimuths[azimuth_index111].impulse_response[j] * blend_factor_111;
			
		ref_data.impulse_response[i] = sample0 + sample1;
	}
}

void HrtfData::get_direction_data(angle_t elevation, angle_t azimuth, distance_t distance, DirectionData& ref_data_left, DirectionData& ref_data_right) const
{
	assert(elevation >= -angle_t(pi * 0.5));
	assert(elevation <= angle_t(pi * 0.5));
	assert(azimuth >= -angle_t(2.0 * pi));
	assert(azimuth <= angle_t(2.0 * pi));

	get_direction_data(elevation, azimuth, distance, 0, ref_data_left);
	if(m_channel_count == 1)
	{
		get_direction_data(elevation, -azimuth, distance, 0, ref_data_right);
	}
	else
	{
		get_direction_data(elevation, azimuth, distance, 1, ref_data_right);
	}
}

void HrtfData::sample_direction(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, uint32_t channel, float& value, float& delay) const
{
	assert(elevation >= -angle_t(pi * 0.5));
	assert(elevation <= angle_t(pi * 0.5));
	assert(azimuth >= -angle_t(2.0 * pi));
	assert(azimuth <= angle_t(2.0 * pi));
	
	size_t distance_index0 = 0;
	while(distance_index0 < m_distances.size() - 1 &&
		  m_distances[distance_index0].distance > distance)
	{
		distance_index0++;
	}
	const size_t distance_index1 = std::min(distance_index0 + 1, m_distances.size() - 1);
	const distance_t distance0 = m_distances[distance_index0].distance;
	const distance_t distance1 = m_distances[distance_index1].distance;
	const distance_t distance_delta = distance0 - distance1;
	const float distance_fractional_part = distance_delta ? (distance - distance1) / distance_delta : 0;
	
	const auto &elevations0 = m_distances[distance_index0].elevations;
	const auto &elevations1 = m_distances[distance_index1].elevations;
	
	const float azimuth_mod = std::fmod(azimuth + angle_t(pi * 2.0), angle_t(pi * 2.0));
	
	const angle_t elevation_scaled0 = (elevation + angle_t(pi * 0.5)) * (elevations0.size() - 1) / angle_t(pi);
	const angle_t elevation_scaled1 = (elevation + angle_t(pi * 0.5)) * (elevations1.size() - 1) / angle_t(pi);
	const size_t elevation_index00 = static_cast<size_t>(elevation_scaled0);
	const size_t elevation_index10 = static_cast<size_t>(elevation_scaled1);
	const size_t elevation_index01 = std::min(elevation_index00 + 1, elevations0.size() - 1);
	const size_t elevation_index11 = std::min(elevation_index10 + 1, elevations1.size() - 1);
	
	const float elevation_fractional_part0 = std::fmod(elevation_scaled0, 1.0);
	const float elevation_fractional_part1 = std::fmod(elevation_scaled1, 1.0);
	
	const angle_t azimuth_scaled00 = azimuth_mod * elevations0[elevation_index00].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index000 = static_cast<size_t>(azimuth_scaled00) % elevations0[elevation_index00].azimuths.size();
	const size_t azimuth_index001 = static_cast<size_t>(azimuth_scaled00 + 1) % elevations0[elevation_index00].azimuths.size();
	const float azimuth_fractional_part00 = std::fmod(azimuth_scaled00, 1.0);
	
	const angle_t azimuth_scaled10 = azimuth_mod * elevations1[elevation_index10].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index100 = static_cast<size_t>(azimuth_scaled10) % elevations1[elevation_index10].azimuths.size();
	const size_t azimuth_index101 = static_cast<size_t>(azimuth_scaled10 + 1) % elevations1[elevation_index10].azimuths.size();
	const float azimuth_fractional_part10 = std::fmod(azimuth_scaled10, 1.0);
	
	const angle_t azimuth_scaled01 = azimuth_mod * elevations0[elevation_index01].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index010 = static_cast<size_t>(azimuth_scaled01) % elevations0[elevation_index01].azimuths.size();
	const size_t azimuth_index011 = static_cast<size_t>(azimuth_scaled01 + 1) % elevations0[elevation_index01].azimuths.size();
	const float azimuth_fractional_part01 = std::fmod(azimuth_scaled01, 1.0);
	
	const angle_t azimuth_scaled11 = azimuth_mod * elevations1[elevation_index11].azimuths.size() / angle_t(2 * pi);
	const size_t azimuth_index110 = static_cast<size_t>(azimuth_scaled11) % elevations1[elevation_index11].azimuths.size();
	const size_t azimuth_index111 = static_cast<size_t>(azimuth_scaled11 + 1) % elevations1[elevation_index11].azimuths.size();
	const float azimuth_fractional_part11 = std::fmod(azimuth_scaled11, 1.0);
	
	const float blend_factor_000 = (1.0f - elevation_fractional_part0) * (1.0f - azimuth_fractional_part00) * distance_fractional_part;
	const float blend_factor_001 = (1.0f - elevation_fractional_part0) * azimuth_fractional_part00 * distance_fractional_part;
	const float blend_factor_010 = elevation_fractional_part0 * (1.0f - azimuth_fractional_part01) * distance_fractional_part;
	const float blend_factor_011 = elevation_fractional_part0 * azimuth_fractional_part01 * distance_fractional_part;
	
	const float blend_factor_100 = (1.0f - elevation_fractional_part1) * (1.0f - azimuth_fractional_part10) * (1.0f - distance_fractional_part);
	const float blend_factor_101 = (1.0f - elevation_fractional_part1) * azimuth_fractional_part10 * (1.0f - distance_fractional_part);
	const float blend_factor_110 = elevation_fractional_part1 * (1.0f - azimuth_fractional_part11) * (1.0f - distance_fractional_part);
	const float blend_factor_111 = elevation_fractional_part1 * azimuth_fractional_part11 * (1.0f - distance_fractional_part);
	
	float delay0;
	float delay1;
	
	if(channel == 0)
	{
		delay0 =
		elevations0[elevation_index00].azimuths[azimuth_index000].delay * blend_factor_000
		+ elevations0[elevation_index00].azimuths[azimuth_index001].delay * blend_factor_001
		+ elevations0[elevation_index01].azimuths[azimuth_index010].delay * blend_factor_010
		+ elevations0[elevation_index01].azimuths[azimuth_index011].delay * blend_factor_011;
		
		delay1 =
		elevations1[elevation_index10].azimuths[azimuth_index100].delay * blend_factor_100
		+ elevations1[elevation_index10].azimuths[azimuth_index101].delay * blend_factor_101
		+ elevations1[elevation_index11].azimuths[azimuth_index110].delay * blend_factor_110
		+ elevations1[elevation_index11].azimuths[azimuth_index111].delay * blend_factor_111;
	} else {
		delay0 =
		elevations0[elevation_index00].azimuths[azimuth_index000].delay_right * blend_factor_000
		+ elevations0[elevation_index00].azimuths[azimuth_index001].delay_right * blend_factor_001
		+ elevations0[elevation_index01].azimuths[azimuth_index010].delay_right * blend_factor_010
		+ elevations0[elevation_index01].azimuths[azimuth_index011].delay_right * blend_factor_011;
		
		delay1 =
		elevations1[elevation_index10].azimuths[azimuth_index100].delay_right * blend_factor_100
		+ elevations1[elevation_index10].azimuths[azimuth_index101].delay_right * blend_factor_101
		+ elevations1[elevation_index11].azimuths[azimuth_index110].delay_right * blend_factor_110
		+ elevations1[elevation_index11].azimuths[azimuth_index111].delay_right * blend_factor_111;
	}
	
	delay = delay0 + delay1;
	
	sample = sample * m_channel_count + channel;
	
	float value0 =
	elevations0[elevation_index00].azimuths[azimuth_index000].impulse_response[sample] * blend_factor_000
	+ elevations0[elevation_index00].azimuths[azimuth_index001].impulse_response[sample] * blend_factor_001
	+ elevations0[elevation_index01].azimuths[azimuth_index010].impulse_response[sample] * blend_factor_010
	+ elevations0[elevation_index01].azimuths[azimuth_index011].impulse_response[sample] * blend_factor_011;
	
	float value1 =
		elevations1[elevation_index10].azimuths[azimuth_index100].impulse_response[sample] * blend_factor_100
		+ elevations1[elevation_index10].azimuths[azimuth_index101].impulse_response[sample] * blend_factor_101
		+ elevations1[elevation_index11].azimuths[azimuth_index110].impulse_response[sample] * blend_factor_110
		+ elevations1[elevation_index11].azimuths[azimuth_index111].impulse_response[sample] * blend_factor_111;
	
	value = value0 + value1;
}

void HrtfData::sample_direction(angle_t elevation, angle_t azimuth, distance_t distance, uint32_t sample, float& value_left, float& delay_left, float& value_right, float& delay_right) const
{
	assert(elevation >= -angle_t(pi * 0.5));
	assert(elevation <= angle_t(pi * 0.5));
	assert(azimuth >= -angle_t(2.0 * pi));
	assert(azimuth <= angle_t(2.0 * pi));

	sample_direction(elevation, azimuth, distance, sample, 0, value_left, delay_left);
	if(m_channel_count == 1)
	{
		sample_direction(elevation, -azimuth, distance, sample, 0, value_right, delay_right);
	}
	else
	{
		sample_direction(elevation, azimuth, distance, sample, 1, value_right, delay_right);
	}
}
