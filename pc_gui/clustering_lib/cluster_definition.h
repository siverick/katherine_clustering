
/**
 * @cluster_definition.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

/************************************************************************/
/* Definition of storage containers for Clusters and pixel related data */
/* Definition of Cluster manipulation functions shared amongst all      */
/************************************************************************/

#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <mutex>
#include <math.h>
#include <cmath>
#include <array>
#include <cassert>

struct OnePixel		// Pixel data
{
	uint16_t x, y;
	int ToT;
	double ToA;

	OnePixel(uint16_t x, uint16_t y, int ToT, double ToA)
		: x(x), y(y), ToT(ToT), ToA(ToA)
	{
	};
};

struct ClusterType
{
	std::vector<OnePixel> pix;
	double minToA;
	double maxToA;
	uint16_t xMax;
	uint16_t xMin;
	uint16_t yMax;
	uint16_t yMin;

	ClusterType(std::vector<OnePixel> pix, double minToA, double maxToA,
		uint16_t xMax, uint16_t xMin, uint16_t yMax, uint16_t yMin)
		: pix(pix), minToA(minToA), maxToA(maxToA), xMax(xMax), xMin(xMin), yMax(yMax), yMin(yMin)
	{
	};
};

// Cluster type for sending stuff over sockets
struct CompactClusterType
{
	std::vector<OnePixel> pix;

	CompactClusterType(std::vector<OnePixel> pix) : pix(pix)
	{
	};
};

enum SortType {
	bigFirst, smallFirst
};

struct ClusteringParams
{
	bool calibReady;		
	int maxClusterDelay;
	int maxClusterSpan;
	int clusterFilterSize;
	bool filterBiggerClusters;
	int outerFilterSize;
	bool rn_delim;
	int no_lines;
};

struct ClusteringParamsOnline
{
	int64_t maxClusterDelay;
	int64_t maxClusterSpan;
	int clusterFilterSize;
	bool filterBiggerClusters;
	int outerFilterSize;
};

struct OnePixelCount
{
	uint16_t x, y;

	OnePixelCount(uint16_t x, uint16_t y)
		: x(x), y(y)
	{
	};
};

struct PixelCounts
{
	uint64_t counts[256][256];
	uint64_t maxCount;
	uint64_t minCount;

	PixelCounts()
	{
		maxCount = 0;
		minCount = 0;

		for (int x = 0; x < 256; x++)
			for (int y = 0; y < 256; y++)
				counts[x][y] = 0;
	};

	void Clear()
	{
		maxCount = 0;
		minCount = 0;

		for (int x = 0; x < 256; x++)
			for (int y = 0; y < 256; y++)
				counts[x][y] = 0;
	}
};

class cluster_definition
{
public:
	typedef std::vector<OnePixel> PixelCluster;
	typedef ClusterType Cluster;
	typedef std::vector<ClusterType> Clusters;

	/// <summary>
	/// Sort doneClusters and keep them
	/// </summary>
	/// <param name="type">Specify bif or small first</param>
	/// <returns>Sorted Clusters</returns>
	void sort_clusters(SortType type)
	{
		if (type == SortType::bigFirst)
		{
			std::sort(doneClusters.begin(), doneClusters.end(),
				[](const ClusterType& a, const ClusterType& b) {
					return a.pix.size() > b.pix.size();
				});
		}
		else if (type == smallFirst)
		{
			std::sort(doneClusters.begin(), doneClusters.end(),
				[](const ClusterType& a, const ClusterType& b) {
					return a.pix.size() < b.pix.size();
				});
		}

		return;
	}

	void sort_clusters(SortType type, Clusters& done)
	{
		if (type == SortType::bigFirst)
		{
			std::sort(done.begin(), done.end(),
				[](const ClusterType& a, const ClusterType& b) {
					return a.pix.size() > b.pix.size();
				});
		}
		else if (type == smallFirst)
		{
			std::sort(done.begin(), done.end(),
				[](const ClusterType& a, const ClusterType& b) {
					return a.pix.size() < b.pix.size();
				});
		}
	}

	void sort_clusters_toa(SortType type, Clusters& done)
	{
		// Fill in minToa values if they are zero
		for (auto& cluster : done)
		{
			if (cluster.minToA != 0)	break;
			cluster.minToA = cluster.pix[0].ToA;	// Approximate ToA
			cluster.maxToA = cluster.pix[0].ToA;	// Approximate ToA
		}

		if (type == SortType::bigFirst)
		{
			std::sort(done.begin(), done.end(),
				[](const ClusterType& a, const ClusterType& b) {
					return a.minToA > b.minToA;
				});
		}
		else if (type == smallFirst)
		{
			std::sort(done.begin(), done.end(),
				[](const ClusterType& a, const ClusterType& b) {
					return a.minToA < b.minToA;
				});
		}
	}

	/// <summary>
	/// Getter for clusters that are complete (finished).
	/// </summary>
	Clusters& get_done_clusters()
	{
		return doneClusters;
	}

	/// <summary>
	/// Erase clusters and free the memory.
	/// </summary>
	void erase_done_clusters()
	{
		doneClusters.clear();
		doneClusters.shrink_to_fit();
	}

	size_t get_cluster_size()
	{
		if (doneClusters.empty()) return 0;

		return doneClusters.size();
	}

protected:
	Clusters clusters;
	Clusters doneClusters;	// Storage for clusters that are finished and ready to be displayed.
};