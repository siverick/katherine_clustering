/**
 * @serializer.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "serializer.h"

// Attach message header
void serializer::attach_header(std::string& input, dataframe_types type)
{
	std::string prepend = "";

	// Prepare the string to prepend before serialized
	switch (type)
	{
	case dataframe_types::clusters:
		prepend = 'C';
		break;
	case dataframe_types::pixels:
		prepend = 'P';
		break;
	case dataframe_types::energies:
		prepend = 'H';
		break;
	case dataframe_types::pixel_counts:
		prepend = 'N';
		break;
	case dataframe_types::messages:
		prepend = 'M';
		break;
	case dataframe_types::errors:
		prepend = 'E';
		break;
	case dataframe_types::command:
		prepend = 'K';
		break;
	case dataframe_types::config:
		prepend = 'V';
		break;
	case dataframe_types::acknowledge:
		prepend = 'A';
		break;
	default:
		// Default behaviour: Send as a message
		prepend = 'M';
		break;
	}

	// Get size of message in bytes
	size_t bytes = input.size() + 1 + 1 + 1;	// Take into account #, ;,'type'
	std::string sNumber = std::to_string(bytes);

	// Double sample to get the true size in bytes
	bytes = sNumber.size() + bytes;
	sNumber = std::to_string(bytes);
	bytes = sNumber.size() + input.size() + 1 + 1 + 1;
	sNumber = std::to_string(bytes);
	bytes = sNumber.size() + input.size() + 1 + 1 + 1;

	// Append length after 'type'
	prepend.append("#");
	prepend.append(std::to_string(bytes));
	prepend.append(";");

	// Prepend to input
	input.insert(0, prepend);
}

void serializer::deattach_header(std::string& input)
{
	size_t offset = input.find('#');
	if (offset == std::string::npos) return;	// We dont have header anymore

	offset = input.find(';') + 1;	// Go one after ';'
	if (offset == std::string::npos) return;	// Unepected error
	input.erase(0, offset);		// Erase header from input
}

dataframe_types serializer::get_type(const std::string& message)
{
	size_t offset = message.find('#');
	if (offset == std::string::npos || offset <= 0) return dataframe_types::messages;	// Unexpected

	char type = message[(offset - 1)];	// Type is always one place before #
	switch (type)
	{
	case 'C':
		return dataframe_types::clusters;
	case 'P':
		return dataframe_types::pixels;
	case 'H':
		return dataframe_types::energies;
	case 'N':
		return dataframe_types::pixel_counts;
	case 'M':
		return dataframe_types::messages;
	case 'E':
		return dataframe_types::errors;
	case 'K':
		return dataframe_types::command;
	case 'V':
		return dataframe_types::config;
	case 'A':
		return dataframe_types::acknowledge;
	default:
		return dataframe_types::messages;
	}

	return dataframe_types::messages;	// Messages are default type
}


 // Semicolon separated clusters
 // Comma separated pixels
 // \t separated elements of pixel
std::string serializer::serialize_clusters(const std::vector<CompactClusterType>& clusters)
{
	std::string ret;
	size_t pixNum = 0;

	for (auto& cluster : clusters)
	{
		for (auto& pix : cluster.pix)
		{
			pixNum++;
			ret.append(std::to_string(pix.x));
			ret.append("\t");
			ret.append(std::to_string(pix.y));
			ret.append("\t");
			ret.append(std::to_string(static_cast<uint32_t>(pix.ToT)));
			ret.append("\t");
			ret.append(std::to_string(static_cast<uint64_t>(pix.ToA)));

			// Separate pixels with comma only inbetween the pixels
			if (pixNum != cluster.pix.size())
			{
				ret.append(",");
			}
		}

		// Put the ; even after the last cluster
		ret.append(";");

		// Reset variables
		pixNum = 0;
	}

	return ret;
}

// Semicolon separated clusters
// Comma separated pixels
// \t separated elements of pixel
std::vector<CompactClusterType> serializer::deserialize_clusters(const std::string& input)
{
	std::vector<CompactClusterType> clusters;
	std::vector<OnePixel> tempCl;
	OnePixel tempPix = { 0,0,0,0 };
	size_t size = input.size();
	size_t offset = 0;
	size_t end_cl = 0;
	size_t end_pix = 0;
	size_t end_num = 0;

	std::string temp = "";

	// When end_cl equals size-1(or bigger), its end of serialized string ...;
	while (end_cl < (size - 1))
	{
		// Find end of cluster - we gather pixel till that time
		end_cl = input.find(';', offset);

		// Gather pixels till the end of this cluster
		while (offset < end_cl)
		{
			// save end of this, or beginning of next pixel
			end_pix = input.find(',', offset);

			end_num = input.find('\t', offset);
			temp = input.substr(offset, end_num - offset);
			tempPix.x = std::atoi(temp.c_str());
			offset = end_num + 1;	// +1 to ingore the separator

			end_num = input.find('\t', offset);
			temp = input.substr(offset, end_num - offset);
			tempPix.y = std::atoi(temp.c_str());
			offset = end_num + 1;

			end_num = input.find('\t', offset);
			temp = input.substr(offset, end_num - offset);
			tempPix.ToT = static_cast<int32_t>(std::atoi(temp.c_str()));
			offset = end_num + 1;

			// if beginning of next pixel is not after end of this cluster,
			// take the end_pix as a end of ToA (last number)
			if (end_pix < end_cl)
			{
				temp = input.substr(offset, end_pix - offset);
				offset = end_pix + 1;
			}
			// if there in no more pixels in this cluster, get the ToA
			// number till the end_cl (cluster end)
			else
			{
				temp = input.substr(offset, end_cl - offset);
				offset = end_cl + 1;
			}
			tempPix.ToA = std::atoll(temp.c_str());

			// Add pixel to cluster
			tempCl.emplace_back(tempPix);
		}

		// Add cluster to clusters
		clusters.emplace_back(tempCl);
		tempCl.clear();
	}

	return clusters;
}

// Comma ',' separated pixels
// \t separated elements of pixel
// Semicolon ';' after the last pixel - ignored, only as a size reference
// Comma ',' even after the last pixel (before semicolon) - to tell last pixel
std::string serializer::serialize_pixels(const std::vector<OnePixel>& pixels)
{
	std::string ret;
	size_t pixNum = 0;

	for (const auto& pix : pixels)
	{
		pixNum++;
		ret.append(std::to_string(pix.x));
		ret.append("\t");
		ret.append(std::to_string(pix.y));
		ret.append("\t");
		ret.append(std::to_string(static_cast<uint32_t>(pix.ToT)));
		ret.append("\t");
		ret.append(std::to_string(static_cast<uint64_t>(pix.ToA)));

		// Separate pixels with comma
		ret.append(",");
	}

	// Put the ; after last pixel
	ret.append(";");

	return ret;
}

std::vector<OnePixel> serializer::deserialize_pixels(const std::string& input)
{
	std::vector<OnePixel> pixels;
	OnePixel tempPix = { 0,0,0,0 };
	size_t offset = 0;
	size_t end_frame = 0;
	size_t end_pix = 0;
	size_t end_num = 0;

	std::string temp = "";

	// Find end of cluster - we gather pixel till that time
	// Note: Here its (';'_pos - 1), because last pixel has ',' behind it and then even 'q',
	// so thats why end is -1, because we ignore the last char ';'
	end_frame = (input.find(';', offset) - 1);

	// Gather pixels till the end of this cluster
	while (offset < end_frame)
	{
		// save end of this, or beginning of next pixel
		end_pix = input.find(',', offset);

		end_num = input.find('\t', offset);
		temp = input.substr(offset, end_num - offset);
		tempPix.x = std::atoi(temp.c_str());
		offset = end_num + 1;	// +1 to ingore the separator

		end_num = input.find('\t', offset);
		temp = input.substr(offset, end_num - offset);
		tempPix.y = std::atoi(temp.c_str());
		offset = end_num + 1;

		end_num = input.find('\t', offset);
		temp = input.substr(offset, end_num - offset);
		tempPix.ToT = static_cast<int32_t>(std::atoi(temp.c_str()));
		offset = end_num + 1;

		// if beginning of next pixel is not after end of this cluster,
		// take the end_pix as a end of ToA (last number)
		if (end_pix < end_frame)
		{
			temp = input.substr(offset, end_pix - offset);
			offset = end_pix + 1;
		}
		// if there in no more pixels in this cluster, get the ToA
		// number till the end_cl (cluster end)
		else
		{
			temp = input.substr(offset, end_frame - offset);
			offset = end_frame + 1;
		}
		tempPix.ToA = std::atof(temp.c_str());

		// Add pixel to cluster
		pixels.emplace_back(tempPix);
	}

	return pixels;
}

std::string serializer::serialize_pixel_counts(const std::vector<OnePixelCount>& pixelCounts)
{
	std::string ret;
	size_t pixNum = 0;

	for (const auto& pix : pixelCounts)
	{
		pixNum++;
		ret.append(std::to_string(pix.x));
		ret.append("\t");
		ret.append(std::to_string(pix.y));
		// Separate pixels with comma
		ret.append(",");
	}

	// Put the ; after last pixel
	ret.append(";");

	return ret;
}

std::vector<OnePixelCount> serializer::deserialize_pixel_counts(const std::string& input)
{
	std::vector<OnePixelCount> pixelCounts;
	OnePixelCount tempPix = { 0,0 };
	size_t offset = 0;
	size_t end_frame = 0;
	size_t end_pix = 0;
	size_t end_num = 0;

	std::string temp = "";

	// Find end of cluster - we gather pixel till that time
	// Note: Here its (';'_pos - 1), because last pixel has ',' behind it and then even 'q',
	// so thats why end is -1, because we ignore the last char ';'
	end_frame = (input.find(';', offset) - 1);

	// Gather pixels till the end of this cluster
	while (offset < end_frame)
	{
		// save end of this, or beginning of next pixel
		end_pix = input.find(',', offset);

		end_num = input.find('\t', offset);
		temp = input.substr(offset, end_num - offset);
		tempPix.x = std::atoi(temp.c_str());
		offset = end_num + 1;	// +1 to ingore the separator

		// if beginning of next pixel is not after end of this cluster,
		// take the end_pix as a end of ToA (last number)
		if (end_pix < end_frame)
		{
			temp = input.substr(offset, end_pix - offset);
			offset = end_pix + 1;
		}
		// if there in no more pixels in this cluster, get the ToA
		// number till the end_cl (cluster end)
		else
		{
			temp = input.substr(offset, end_frame - offset);
			offset = end_frame + 1;
		}
		tempPix.y = std::atoi(temp.c_str());

		// Add pixel to cluster
		pixelCounts.emplace_back(tempPix);
	}

	return pixelCounts;
}

std::string serializer::serialize_histograms(const std::vector<uint16_t>& histograms, size_t pixel_count)
{
	std::string ret;

	ret.append(std::to_string(pixel_count));
	ret.append(",");

	for (const auto& energy : histograms)
	{
		ret.append(std::to_string(energy));
		// Separate pixels with comma
		ret.append(",");
	}

	// Put the ; after last pixel
	ret.append(";");

	return ret;
}

std::vector<uint16_t> serializer::deserialize_histograms(const std::string& input, size_t& out_pixel_count)
{
	std::vector<uint16_t> energies;

	size_t offset = 0;
	size_t end_frame = 0;
	size_t end_pix = 0;

	std::string temp = "";
	uint16_t tempEnergy = 0;
	// Find end of cluster - we gather pixel till that time
	// Note: Here its (';'_pos - 1), because last pixel has ',' behind it and then even 'q',
	// so thats why end is -1, because we ignore the last char ';'
	end_frame = (input.find(';', offset) - 1);

	// First fill out out_pixel_count
	end_pix = input.find(',', offset);
	temp = input.substr(offset, end_pix - offset);
	offset = end_pix + 1;
	out_pixel_count = std::atoi(temp.c_str());

	// Gather pixels till the end of this cluster
	while (offset < end_frame)
	{
		// save end of this, or beginning of next pixel
		end_pix = input.find(',', offset);

		// if beginning of next pixel is not after end of this cluster,
		// take the end_pix as a end of ToA (last number)
		if (end_pix < end_frame)
		{
			temp = input.substr(offset, end_pix - offset);
			offset = end_pix + 1;
		}
		// if there in no more pixels in this cluster, get the ToA
		// number till the end_cl (cluster end)
		else
		{
			temp = input.substr(offset, end_frame - offset);
			offset = end_frame + 1;
		}
		tempEnergy = std::atoi(temp.c_str());

		// Add pixel to cluster
		energies.emplace_back(tempEnergy);
	}

	return energies;
}


std::string serializer::serialize_params(ClusteringParamsOnline params)
{
	std::string ret = "";

	ret.append(std::to_string(params.maxClusterDelay));
	ret.append(",");
	ret.append(std::to_string(params.maxClusterSpan));
	ret.append(",");
	ret.append(std::to_string(params.outerFilterSize));
	ret.append(",");
	ret.append(std::to_string(params.clusterFilterSize));
	ret.append(",");
	ret.append(std::to_string((params.filterBiggerClusters ? 1 : 0)));
	ret.append(",");	// column even after last parameter for robust deserialization

	// Put the ; after last param
	ret.append(";");

	return ret;
}

ClusteringParamsOnline serializer::deserialize_params(std::string input)
{
	ClusteringParamsOnline ret{};

	size_t offset = 0;
	size_t end_frame = 0;
	size_t end_par = 0;
	std::string temp;

	// Find end_frame
	end_frame = (input.find(';', offset) - 1);

	// First fill out out_pixel_count
	end_par = input.find(',', offset);
	temp = input.substr(offset, end_par - offset);
	offset = end_par + 1;
	ret.maxClusterDelay = std::atoll(temp.c_str());

	end_par = input.find(',', offset);
	temp = input.substr(offset, end_par - offset);
	offset = end_par + 1;
	ret.maxClusterSpan = std::atoll(temp.c_str());

	end_par = input.find(',', offset);
	temp = input.substr(offset, end_par - offset);
	offset = end_par + 1;
	ret.outerFilterSize = std::atoi(temp.c_str());

	end_par = input.find(',', offset);
	temp = input.substr(offset, end_par - offset);
	offset = end_par + 1;
	ret.clusterFilterSize = std::atoi(temp.c_str());

	end_par = input.find(',', offset);
	temp = input.substr(offset, end_frame - offset);
	offset = end_par + 1;
	ret.filterBiggerClusters = (std::atoi(temp.c_str()) == 1) ? true : false;

	return ret;
}
