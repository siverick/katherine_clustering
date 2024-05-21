
/**
 * @file_saver.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once
#include <string>
#include "cluster_definition.h"
#include "clusering_base.h"


/*
	File saving documentation
	- informative lines start with '#'
	- then pixel always starts with number

	The format of saving pixel is:
	X \t	Y \t	ToT \t	ToA (in ns) \r\n

	If we are saving whole clusters, following is added:
	- every line preceding one cluster starts with 'C'
	- followed by number of cluster
	- ended with ';'
	- all next lines after C and before next C are pixels of presented format

	Format of indicating new cluster is:
	C1235;\r\n

	Algorithm for decoding cluster file is:
	- while read new line
	- if its '#', continue reading another line
	- if its 'C', make newCluster = true, and continue reading another line
	- then, if newCluster is True: add new cluster to array of clusters and make newCluster = False, then continue reading another line
	- if newCluster is False: add new pixel to last cluster added and update ToA and coordinates stats, then continue reading another line
	- if all lines were read, return array of clusters, ready to be processed further by human
*/


class file_saver : protected clustering_base
{
public:
	static std::string savePixelFile(std::vector<ClusterType>& doneClusters);
	static std::string saveClusterFile(std::vector<ClusterType>& doneClusters);
	static std::vector<ClusterType> loadClustersFromFile(std::string path);

private:
	static std::string createHeader(size_t numOfClusters, size_t numOfPixels, std::string datetime);
	static std::string getDateTime();
};

