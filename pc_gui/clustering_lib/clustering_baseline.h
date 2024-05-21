
/**
 * @clustering_baseline.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#include "file_loader.h"
#include "clusering_base.h"

class clustering_baseline : public clustering_base, public cluster_definition
{
	public:
		void do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort);
		void parse_file_clusters(std::string& lines, const ClusteringParams& params, volatile bool& abort);
		void do_online_file_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort);

	private:
		// Test whether saved clusters are saved correctly
		void test_saved_clusters()
		{
			for (const auto& clstr : doneClusters) {
				for (const auto& pixs : clstr.pix) {
					stat_lines_saved++;
					assert("Limits do not match included pixels" && (pixs.x > clstr.xMax || pixs.x < clstr.xMin || pixs.y > clstr.yMax || pixs.y < clstr.yMin) == false);
				}
			}
		}
};

