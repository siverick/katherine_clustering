/**
 * @cluster_minmax.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#include <clustering_base.h>
#include "file_loader.h"

class offline_clustering : public clustering_base, public cluster_definition
{
	public:
		void do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort);

	private:

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

