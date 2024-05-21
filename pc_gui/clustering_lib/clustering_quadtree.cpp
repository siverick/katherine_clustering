
/**
 * @clustering_quadtree.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 *
 * Disclaimer: Created based on diploma thesis of Lukas Meduna (meduna@medunalukas.com)
 */

#include "clustering_quadtree.h"
#include <queue>

/*
*  IMPORTANT PERFORMANCE COMMENT
* - inlining getline and strtoint and strtolong improves performance
*   by circa 4%. But in Bruteforce, it worsens performance.
*/
void clustering_quadtree::do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
	if (lines == "") return;
	if (lines[0] != '#') return;
	stat_reset();

	// PARAMETERS
	int upFilter = 255 - params.outerFilterSize;
	int doFilter = params.outerFilterSize;
	double toaLsb = 25;
	double toaFineLsb = 1.5625;

	// Loop variables
	bool prevAdded = false;

	std::string oneLine;
	char* context = nullptr;
	char* rows[4] = { 0, 0, 0, 0 };

	std::queue<OnePixel> pixelData;

	while (get_my_line(lines, oneLine, params.rn_delim)) {      // Gets lines without ending line chars - ex. "\n"

		if (oneLine[0] == '#') continue;

		if (abort)
		{
			return;
		}

		stat_lines_sorted++;

		/* Split line into rows */
		rows[0] = strtok_s(&oneLine.front(), "\t", &context); //strtok(oneLine.data(), "\t");
		rows[1] = strtok_s(NULL, "\t", &context);
		rows[2] = strtok_s(NULL, "\t", &context);
		rows[3] = strtok_s(NULL, "\t", &context);

		// ATOI 6300 ms vs STRTOINT 6100 ms with 20 PIX filter
		// ATOI CCA 200 ms slower than my Fast strtoint
		int coordX = strtoint(rows[0]) / 256; // std::atoi(rows[0]) / 256;
		int coordY = strtoint(rows[0]) % 256; // std::atoi(rows[0]) % 256;
		double toaAbsTime = (double)((strtolong(rows[1]) * toaLsb) - (strtolong(rows[2]) * toaFineLsb));  // (ns) TimeFromBeginning = 25 * ToA - 1.5625 * fineToA
		int ToTValue = strtoint(rows[3]);//std::atoi(rows[3]);
		if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) continue;   // This is faster after the inlined funkctions, not after coordY

		if (params.calibReady)
		{
			pixelData.front().ToT = energy_calc(pixelData.front().x, pixelData.front().y, pixelData.front().ToT);
			//pixelData.front().ToT = get_energy_lut(pixelData.front().x, pixelData.front().y, pixelData.front().ToT);
		}

		pixelData.push(std::move(OnePixel(coordX, coordY, ToTValue, toaAbsTime)));
		stat_lines_processed++;
	}

	if (params.calibReady) // Calibrate with LUT
	{
		//create_energy_lut(800);
	}

	ContinualTimer timer;
	timer.Start();

	while (!pixelData.empty())
	{
		ProcessPixelData(pixelData.front(), params);
		pixelData.pop();
	}
	timer.Stop();

	/* POSTPROCESS Clusters */
	CloseClusters();      // Remaining move to DONE
	openClusters.clear();
	openClusters.shrink_to_fit();
	doneClusters.shrink_to_fit();

	/* Utility functions after the clustering */
	test_saved_clusters();
	stat_save(timer.ElapsedMs(), doneClusters.size());
	return;
}

void clustering_quadtree::ProcessPixelData(OnePixel& pixel_data, const ClusteringParams& params)
{
	CloseClusters(params.maxClusterDelay);
	last_time_ = pixel_data.ToA;

	was_added_to_cluster_ = false;

	joining_cluster_ = openClusters.begin();
	double toa = pixel_data.ToA;
	size_t x = pixel_data.x;
	size_t y = pixel_data.y;

	// Go through all open clusters
	for (auto cluster_it = openClusters.begin();
		cluster_it != openClusters.end();) {
		// Deoes the pixel lies in neighbour of the cluster?
		if ((*cluster_it).CanBeAdded(x, y, toa, params.maxClusterSpan)) {
			// Is it first time adding this pixel to cluster?
			if (!was_added_to_cluster_) {
				// Note iterator
				joining_cluster_ = cluster_it;
				// Add the pixel
				(*cluster_it).AddPixel(pixel_data);
				was_added_to_cluster_ = true;
			}
			else {
				// Pixel was already added, so we need to join the cluster together and delete the remains
				// of joined cluster
				(*joining_cluster_).JoinWith(*cluster_it);
				cluster_it = openClusters.erase(cluster_it);
				continue;
			}
		}
		++cluster_it;
	}
	// If we have not added pixel to cluster, create new one.
	if (!was_added_to_cluster_) {
		ClusterDataBuilder cluster;
		cluster.AddPixel(pixel_data);
		openClusters.emplace_back(std::move(cluster));
	}
}

void clustering_quadtree::CloseClusters(size_t time_delay)
{
	for (auto clstr = openClusters.begin(); clstr != openClusters.end();) {
		if (last_time_ - (*clstr).first_toa > time_delay) {
			// Peel of the unnecesary members
			ClusterType newCluster((*clstr).cluster, (*clstr).first_toa, (*clstr).first_toa, 0, 0, 0, 0);
			// TODO: I must convert ClusterData to Clusters
			//int xmin = readers_it->neighbors_.root_->boundaries_.x_min;
			
			doneClusters.emplace_back(std::move(newCluster));
			clstr = openClusters.erase(clstr);
		}
		else
			++clstr;

	}
}

void clustering_quadtree::CloseClusters()
{
	for (auto clstr = openClusters.begin(); clstr != openClusters.end();) {
		// Peel of the unnecesary members
		ClusterType newCluster((*clstr).cluster, (*clstr).first_toa, (*clstr).first_toa, 0, 0, 0, 0);

		//int xmin = readers_it->neighbors_.root_->boundaries_.x_min;
		// TODO: I must convert ClusterData to Clusters
		doneClusters.emplace_back(std::move(newCluster));
		clstr = openClusters.erase(clstr);
	}
}

void clustering_quadtree::show_progress(bool newOp, int no_lines)
{
#define no_prog 100
#define inc (100/no_prog)
	static int prog = 0;
	static int last_prog = 0;
	static int prog_val = 0;

	/* Progress bar clutter */
	if (newOp == false)
	{
		prog++;
		if (prog - last_prog > no_lines / no_prog)
		{
			last_prog = prog;
			prog_val += inc;
			//emit obj.update_progress(prog_val);
		}
	}
	else
	{
		//emit obj.update_progress(0);
		prog = 0;
		last_prog = 0;
		prog_val = 0;
	}
}