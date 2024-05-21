
/**
 * @cluster_benchmark.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#include "file_loader.h"
#include "clusering_base.h"
#include <queue>
#include <vector>

class cluster_benchmark : public clustering_base, public cluster_definition
{
public:
	void parse_data(std::string& lines, const ClusteringParams& params, volatile bool& abort);

	void do_clustering_A(std::string& lines, const ClusteringParams& params, volatile bool& abort);
	void do_clustering_B(std::string& lines, const ClusteringParams& params, volatile bool& abort);
	void do_clustering_C(std::string& lines, const ClusteringParams& params, volatile bool& abort);
	void do_clustering_D(std::string& lines, const ClusteringParams& params, volatile bool& abort);
	void do_clustering_E(std::string& lines, const ClusteringParams& params, volatile bool& abort);
	void do_clustering_F(std::string& lines, const ClusteringParams& params, volatile bool& abort);
	void do_clustering_G(std::string& lines, const ClusteringParams& params, volatile bool& abort);

	void create_LUT_test(int depth)
	{
		create_energy_lut(depth);
	}

	double maxToT = 0;
	double minToT = 0;
	double avgToT = 0;

private:

	std::queue<OnePixel> pixelData;		// FIFO for pixelData

	// Custom data for testing

	struct OnePixelDef		// Pixel data
	{
		uint16_t x, y;
		int ToT;
		int64_t ToA;

		OnePixelDef(uint16_t x, uint16_t y, int ToT, int64_t ToA)
			: x(x), y(y), ToT(ToT), ToA(ToA)
		{
		};
	};

	struct ClusterTypeDef
	{
		std::vector<OnePixelDef> pix;
		int64_t minToA;
		int64_t maxToA;
		uint16_t xMax;
		uint16_t xMin;
		uint16_t yMax;
		uint16_t yMin;

		ClusterTypeDef(std::vector<OnePixelDef> pix, int64_t minToA, int64_t maxToA,
			uint16_t xMax, uint16_t xMin, uint16_t yMax, uint16_t yMin)
			: pix(pix), minToA(minToA), maxToA(maxToA), xMax(xMax), xMin(xMin), yMax(yMax), yMin(yMin)
		{
		};
	};

	std::vector<ClusterTypeDef> clustersDef;
	std::vector<ClusterTypeDef> doneClustersDef;
	std::queue<OnePixelDef> pixelDataDef;

	void test_saved_clusters()
	{
		for (const auto& clstr : doneClusters) {
			for (const auto& pixs : clstr.pix) {
				stat_lines_saved++;
				assert("Limits do not match included pixels" && (pixs.x > clstr.xMax || pixs.x < clstr.xMin || pixs.y > clstr.yMax || pixs.y < clstr.yMin) == false);
			}
		}
	}

	void test_saved_clusters_simple()
	{
		for (const auto& clstr : doneClusters) {
			for (const auto& pixs : clstr.pix) {
				stat_lines_saved++;
			}
		}
	}
};

