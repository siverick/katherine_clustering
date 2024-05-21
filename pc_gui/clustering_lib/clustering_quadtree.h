
/**
 * @clustering_quadtree.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 * 
 * Disclaimer: Created based on diploma thesis of Lukas Meduna (meduna@medunalukas.com)
 */

#pragma once
#include "clusering_base.h"
#include "m_cluster_data.h"

class clustering_quadtree: public clustering_base, public ClusterData
{
	bool was_added_to_cluster_ = false;
	double last_time_; // time of last recieved message
	std::vector<ClusterDataBuilder> openClusters;
	std::vector<ClusterDataBuilder>::iterator joining_cluster_;
	std::size_t counter_;

public:
	void do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort);
	void ProcessPixelData(OnePixel& pixel_data, const ClusteringParams& params);
	void CloseClusters(size_t time_delay);
	void CloseClusters();

private:
	void show_progress(bool newOp, int no_lines);

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

