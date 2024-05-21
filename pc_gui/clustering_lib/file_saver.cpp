
/**
 * @file_saver.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "file_saver.h"
#include "file_loader.h"
#include <fstream>
#include <iomanip>
#include <ctime>
#include <sstream>

std::string file_saver::savePixelFile(std::vector<ClusterType>& doneClusters)
{
	size_t numOfClusters = doneClusters.size();
	size_t numOfPixels = 0;
	for (auto& cluster : doneClusters)
	{
		numOfPixels += cluster.pix.size();
	}

	std::string timestamp = getDateTime();

	std::string ret;
	ret = createHeader(numOfClusters, numOfPixels, timestamp);

	for (auto& cluster : doneClusters)
	{
		for (auto& pix : cluster.pix)
		{
			ret.append(std::to_string((pix.x)));
			ret.append("\t");
			ret.append(std::to_string(pix.y));
			ret.append("\t");
			ret.append(std::to_string(pix.ToT));
			ret.append("\t");
			ret.append(std::to_string(static_cast<uint64_t>(pix.ToA)));
			ret.append("\r\n");
		}
	}

	// Create filename with datetime
	std::string filename = "../saved_pixel_data/";
	filename.append(timestamp);
	filename.append("_pixel_file.txt");

	std::ofstream out(filename);
	out << ret;
	out.close();

	return filename;
}

std::string file_saver::saveClusterFile(std::vector<ClusterType>& doneClusters)
{
	size_t numOfClusters = doneClusters.size();
	size_t numOfPixels = 0;
	for (auto& cluster : doneClusters)
	{
		numOfPixels += cluster.pix.size();
	}

	std::string timestamp = getDateTime();

	std::string ret;
	ret = createHeader(numOfClusters, numOfPixels, timestamp);

	size_t clusterNum = 0;
	for (auto& cluster : doneClusters)
	{
		ret.append("C");
		ret.append(std::to_string(clusterNum));
		ret.append(";\r\n");

		for (auto& pix : cluster.pix)
		{
			ret.append(std::to_string((pix.x)));
			ret.append("\t");
			ret.append(std::to_string(pix.y));
			ret.append("\t");
			ret.append(std::to_string(pix.ToT));
			ret.append("\t");
			ret.append(std::to_string(static_cast<uint64_t>(pix.ToA)));
			ret.append("\r\n");
		}

		clusterNum++;
	}

	// Create filename with datetime
	std::string filename = "../saved_pixel_data/";
	filename.append(timestamp);
	filename.append("_cluster_file.txt");

	std::ofstream out(filename);
	out << ret;
	out.close();

	return filename;
}

std::vector<ClusterType> file_saver::loadClustersFromFile(std::string path)
{
	std::vector<ClusterType> clusters;

	// Load the file
	std::string input;
	file_loader::loadPixelData(path, input);
	if (input == "" || input == "fault") return std::vector<ClusterType>();	// Error handling = return empty vector

	std::string oneLine;
	const double toaLsb = 25;
	const double toaFineLsb = 1.5625;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };
	bool newCluster = false;

	while (get_my_line(input, oneLine, true)) {      // Gets lines without ending line chars - ex. "\n"

		if (oneLine[0] == '#') continue;
		
		// Beginning of cluster
		if (oneLine[0] == 'C')
		{
			newCluster = true;
			continue;
		}

		/* Split line into rows */
		rows[0] = strtok_s(&oneLine.front(), "\t", &context); //strtok(oneLine.data(), "\t");
		rows[1] = strtok_s(NULL, "\t", &context);
		rows[2] = strtok_s(NULL, "\t", &context);
		rows[3] = strtok_s(NULL, "\t", &context);

		// Convert rows to variables
		int x = strtoint(rows[0]);
		int y = strtoint(rows[1]);
		int ToT = strtoint(rows[2]);
		double ToA = (double)strtolong(rows[3]);

		// Emplace pixels to clusters
		if (newCluster == true)
		{
			// Emplace whole new cluster
			clusters.emplace_back(ClusterType{ std::vector<OnePixel> { OnePixel {(uint16_t)x, (uint16_t)y, ToT, ToA} }, ToA, ToA, (uint16_t)x, (uint16_t)x, (uint16_t)y, (uint16_t)y });
			newCluster = false;
		}
		else
		{
			// emplace one pixel to last cluster
			clusters.back().pix.emplace_back(OnePixel{ (uint16_t)x, (uint16_t)y, ToT, ToA });
			
			/* Update Min Max coord values */
			if (x > clusters.back().xMax) clusters.back().xMax = x;
			else if (x < clusters.back().xMin) clusters.back().xMin = x;
			if (y > clusters.back().yMax) clusters.back().yMax = y;
			else if (y < clusters.back().yMin) clusters.back().yMin = y;
			if (clusters.back().minToA > ToA)clusters.back().minToA = ToA; // Save ToA min
			if (clusters.back().maxToA < ToA)clusters.back().maxToA = ToA; // Save ToA max
		}
	}

	return clusters;
}

// Create file header
std::string file_saver::createHeader(size_t numOfClusters, size_t numOfPixels, std::string datetime)
{
	std::string ret;
	ret.append("# Pixel save file from measurement\r\n");
	ret.append("# Date and time = ");
	ret.append(datetime);
	ret.append("\r\n");
	ret.append("# Num of clusters = ");
	ret.append(std::to_string(numOfClusters));
	ret.append("\r\n");
	ret.append("# Num of pixels = ");
	ret.append(std::to_string(numOfPixels));
	ret.append("\r\n");
	ret.append("# Detector mode: ToA & ToT\r\n");
	ret.append("# Readout mode: Data-Driven Mode\r\n");
	ret.append("# Format:	X	Y	ToT	  ToA\r\n");
	ret.append("# -----------------------------------------------------------------------------------------------------------------------\r\n");
	return ret;
}

std::string file_saver::getDateTime()
{
	// Get datetime as a string
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%d-%m-%Y_%H-%M-%S");
	return oss.str();
}
