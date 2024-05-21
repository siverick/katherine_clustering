#pragma once

#include "file_loader.h"
#include "clusering_base.h"

class clustering_spatial_quadtree : public clustering_base, public cluster_definition
{
	struct Node {
		Node* one;
		Node* two;
		Node* three;
		Node* four;

		Clusters data;

		Node(Clusters begin) : data(begin)
		{
			one = nullptr;
			two = nullptr;
			three = nullptr;
			four = nullptr;
		}
	};

#define MAX_DEPTH 1


	class QuadTree {
	public:
		static uint8_t depth;

		Clusters data;
		static Clusters doneClusters;
		static ClusteringParams params;

		QuadTree* one;
		QuadTree* two;
		QuadTree* three;
		QuadTree* four;

		QuadTree(const ClusteringParams& params)
		{
			if (depth < MAX_DEPTH) {
				depth++;
				one = new QuadTree(params);
				two = new QuadTree(params);
				three = new QuadTree(params);
				four = new QuadTree(params);
			}
			else {
				one = nullptr;
				two = nullptr;
				three = nullptr;
				four = nullptr;
			}

			this->params = params;
		}

		~QuadTree()
		{
			depth = 0;
			delete one;
			delete two;
			delete three;
			delete four;
		}

		QuadTree(Clusters begin, const ClusteringParams& params) : data(begin)
		{
			one = nullptr;
			two = nullptr;
			three = nullptr;
			four = nullptr;
			this->params = params;
		}

		void GetDoneClusters(Clusters& container)
		{
			// Finalize done clusters
			doneClusters.insert(doneClusters.end(), one->data.begin(), one->data.end());
			doneClusters.insert(doneClusters.end(), two->data.begin(), two->data.end());
			doneClusters.insert(doneClusters.end(), three->data.begin(), three->data.end());
			doneClusters.insert(doneClusters.end(), four->data.begin(), four->data.end());
			doneClusters.insert(doneClusters.end(), data.begin(), data.end());

			// TODO: Test whether deletion works as expected
			// TODO: After algorithm is done, do a workaround and use only one doneClusters, not here in QuadTree to free memory!
			container = doneClusters;
			doneClusters.clear();
			doneClusters.shrink_to_fit();
			data.clear();
			data.shrink_to_fit();
			one->data.clear();
			one->data.shrink_to_fit();
			two->data.clear();
			two->data.shrink_to_fit();
			three->data.clear();
			three->data.shrink_to_fit();
			four->data.clear();
			four->data.shrink_to_fit();
		}

		void ProcessPixel(const OnePixel& pix)
		{
			if (pix.x > 127 && pix.y > 127)			// 1. Quadrant: Nothing boundary
			{
				bool isBoundary = pix.x == 128 || pix.y == 128;		// according to defined boundary - not universal now
				AddThePixel(pix, one, isBoundary);
			}
			else if (pix.x <= 127 && pix.y > 127)		// 2. Quadrant: X is boundary
			{
				bool isBoundary = pix.x == 127 || pix.y == 128;
				AddThePixel(pix, two, isBoundary);
			}
			else if (pix.x <= 127 && pix.y <= 127)	// 3. Quadrant: X and Y is boundary
			{
				bool isBoundary = pix.x == 127 || pix.y == 127;
				AddThePixel(pix, three, isBoundary);
			}
			else if (pix.x > 127 && pix.y <= 127)		// 4. Quadrant: Y is boundary
			{
				bool isBoundary = pix.x == 128 || pix.y == 127;
				AddThePixel(pix, four, isBoundary);
			}
		}

	private:

		// Add the pixel either to another cluster, or form new cluster in the given node
		inline void AddThePixel(const OnePixel& pix, QuadTree* node, bool isBoundary)
		{
			bool pixAdded = false;
			bool pixAddedParent = false;
			size_t lastAddNodeCluster = 0;
			size_t lastAddParentCluster = 0;

			pixAdded = IterateChild(pix, node, lastAddNodeCluster);	 // Iterate through clusters if there are any in the node
			pixAddedParent = IterateChild(pix, this, lastAddParentCluster);	 // Iterate through clusters if there are any in the node

			if (pixAdded && pixAddedParent)
			{
				data[lastAddParentCluster].pix.insert(data[lastAddParentCluster].pix.end(), node->data[lastAddNodeCluster].pix.begin(),
					node->data[lastAddNodeCluster].pix.end() - 1);
				merge_cluster_params(data[lastAddParentCluster], node->data[lastAddNodeCluster]);
				node->data.erase(node->data.begin() + lastAddNodeCluster);
			}
			/* There the path for pixAddedParent ends */
			else if (pixAddedParent && !pixAdded) return;
			else if (pixAdded)
			{
				if (isBoundary)	/* Just emplace boundary cluster to parent and erase it in node */
				{
					data.emplace_back(std::move(node->data[lastAddNodeCluster]));
					node->data.erase(node->data.begin() + lastAddNodeCluster);
				}
				else /* If not boundary but added, return */
				{
					return;
				}
			}
			else if (pixAdded == false)
			{
				if (isBoundary) /* Move new cluster directly to parent, nothing has been added to child */
				{
					Cluster newCluster = Cluster{ PixelCluster{pix}, pix.ToA, pix.ToA, pix.x, pix.x, pix.y, pix.y };
					data.emplace_back(std::move(newCluster));
				}
				else
				{
					Cluster newCluster = Cluster{ PixelCluster{pix}, pix.ToA, pix.ToA, pix.x, pix.x, pix.y, pix.y };
					node->data.emplace_back(std::move(newCluster));
				}
			}

			// TODO:
			//one->AddNewCluster(pix); // Later for deeper tree
		}

		// NOTE: Here, a whole cluster is promoted to parent. Means we have to iterate through every pixel of promoted cluster, not just one pixel.
				// Promoted cluster could have some old pixel, that is neighbor with another part of cluster. But because this pixel would not be interated
				// on, the another part of cluster would be forgotten.
				//pixAdded = IterateChild(pix, this, lastAddParentCluster);  // Was also added in parent node?

	private:
		bool IterateChild(const OnePixel& elem, QuadTree* node, size_t& lastAddCluster)
		{
			bool relX, relY = false;
			bool prevAdded = false;
			size_t index = 0;
			lastAddCluster = 0;

			for (auto clstr = node->data.begin(); clstr != node->data.end(); clstr++) {

				if (node == this)	// Move the cluster to doneClusters
				{
					if ((elem.ToA - clstr->maxToA) > (params.maxClusterDelay + 1000000))
					{
						index = clstr - node->data.begin();
						if (index < 1) continue;

						doneClusters.emplace_back(*clstr);
						clstr--;
						node->data.erase(node->data.begin() + index);

						continue;
						
						int64_t idx = IterateParent(data, *clstr);
						if (idx < 0)	// New cluster was formed in parent
						{
							doneClusters.emplace_back(*(data.end() - 1));
							node->data.erase(data.end() - 1);
						}
						else   // Added to cluster in parent
						{
							doneClusters.emplace_back(data[idx]);
							clstr--;
							node->data.erase(node->data.begin() + idx);
						}

						continue;
					}
				}

				if (node != this)	// Move the cluster to parent
				{
					if ((elem.ToA - clstr->maxToA) > params.maxClusterDelay)
					{
						index = clstr - node->data.begin();
						if (index < 1) continue;

						IterateParent(data, *clstr);  // Either add or merge to parent cluster
						//this->data.emplace_back(*clstr);
						clstr--;
						node->data.erase(node->data.begin() + index);	// Remove from child

						continue;
					}
				}

				/* Decide if its worth to go through this cluster */
				/* ISNT 0 && IS TOO FAR UNDER || IS TOO FAR UP */
				if ((elem.ToA - clstr->minToA) > params.maxClusterSpan || (elem.ToA - clstr->minToA) < -params.maxClusterSpan) { continue; }
				if (elem.y < (clstr->yMin - 1) || elem.y >(clstr->yMax + 1)) { continue; }
				if (elem.x < (clstr->xMin - 1) || elem.x >(clstr->xMax + 1)) { continue; }

				for (const auto& pixs : clstr->pix) {  // Cycle through Pixels of Cluster

					relX = ((elem.x) == (pixs.x + 1)) || ((elem.x) == (pixs.x - 1)) || ((elem.x) == (pixs.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
					relY = ((elem.y) == (pixs.y + 1)) || ((elem.y) == (pixs.y - 1)) || ((elem.y) == (pixs.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)

					if (prevAdded && relX && relY)  // Join clusters
					{
						// Join current Cluster into LastAddedTo cluster and Erase the current one
						node->data[lastAddCluster].pix.insert(node->data[lastAddCluster].pix.end(), clstr->pix.begin(), clstr->pix.end());

						/* Join min max values of clusters */
						merge_cluster_params(node->data[lastAddCluster], *clstr);

						index = clstr - node->data.begin();
						clstr--;                                          // NO Cluster was reduced by 1
						node->data.erase(node->data.begin() + index);     // Erase the current Cluster
					}
					else if (relX && relY)            // Simply Add Pixel
					{
						/* Update Min Max coord values */
						if (elem.x > clstr->xMax) clstr->xMax = elem.x;
						else if (elem.x < clstr->xMin) clstr->xMin = elem.x;
						if (elem.y > clstr->yMax) clstr->yMax = elem.y;
						else if (elem.y < clstr->yMin) clstr->yMin = elem.y;
						if (clstr->minToA > elem.ToA) clstr->minToA = elem.ToA; // Save ToA min
						if (clstr->maxToA < elem.ToA) clstr->maxToA = elem.ToA; // Save ToA max

						clstr->pix.emplace_back(OnePixel{ (uint16_t)elem.x, (uint16_t)elem.y, elem.ToT, elem.ToA });
						lastAddCluster = clstr - node->data.begin();;
						prevAdded = true;
					}

					break;  // Pixel was added to cluster => try NEXT cluster
				}

			}

			return prevAdded;
		}

		int64_t IterateParent(Clusters& parent, Cluster& child)	// Cluster was added to parent, now iterate if there is a neighbor
		{
			bool clusterJoined = false;
			bool relX, relY = false;
			bool breakFromLoop = false;
			int64_t firstJoinedCluster = -1;
			size_t index = 0;

			for (auto parClstr = parent.begin(); parClstr != parent.end(); parClstr++)
			{
				/* Cannot pass further if: out of bounds TIME, out of bounds AREA  */
				if ((child.minToA - parClstr->minToA) > params.maxClusterSpan || (child.minToA - parClstr->minToA) < -params.maxClusterSpan) { continue; }
				if (child.xMax < (parClstr->xMin - 1) || child.xMin >(parClstr->xMax + 1)
					|| child.yMax < (parClstr->yMin - 1) || child.yMin >(parClstr->yMax + 1)) continue;

				for (const auto& childPix : child.pix)	// Every pixel in moved lastly to parent
				{
					/* Cannot pass further of childPix doesnt fit into parClstr AREA */
					if ((childPix.y < (parClstr->yMin - 1)) || childPix.y > (parClstr->yMax + 1)) { continue; }
					if ((childPix.x < (parClstr->xMin - 1)) || childPix.x > (parClstr->xMax + 1)) { continue; }

					for (const auto& parPix : parClstr->pix)
					{
						relX = ((childPix.x) == (parPix.x + 1)) || ((childPix.x) == (parPix.x - 1)) || ((childPix.x) == (parPix.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
						relY = ((childPix.y) == (parPix.y + 1)) || ((childPix.y) == (parPix.y - 1)) || ((childPix.y) == (parPix.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)
						//assert(firstJoinedCluster != parClstr - parent.begin());

						if (relX && relY && clusterJoined)    // If Neighbor, add to cluster
						{
							// Join current Cluster into LastAddedTo cluster and Erase the current one
							parent[firstJoinedCluster].pix.insert(parent[firstJoinedCluster].pix.end(), parClstr->pix.begin(), parClstr->pix.end());
							/* Join min max values of clusters */
							merge_cluster_params(parent[firstJoinedCluster], *parClstr);

							index = parClstr - parent.begin();
							// Co ses stane, kdyz se vsechny clustery prochazeji znova s jinym pixelem a ten sousedni cluster ma
							// nizsi index, nez ten FirstJoined? 
							parClstr--;                                 // NO Cluster was reduced by 1
							parent.erase(parent.begin() + index);       // Erase the current Cluster
							breakFromLoop = true;
							break;
						}
						else if (relX && relY)
						{
							// Insert last MovedToParent cluster to FOUNDCLUSTER and erase the MovedToParent cluster
							parClstr->pix.insert(parClstr->pix.end(), child.pix.begin(), child.pix.end());
							merge_cluster_params(*parClstr, child);

							firstJoinedCluster = parClstr - parent.begin();
							clusterJoined = true;
							breakFromLoop = true;
							break;
						}
					}
					if (breakFromLoop)
					{
						breakFromLoop = false;
						break;
					}
				}
			}

			if (clusterJoined == false)	parent.emplace_back(child);
			return firstJoinedCluster;
		}

		void CallIterateParentClose(QuadTree* node, int index, Cluster* clstr)
		{
			std::vector<size_t> nigh;
			IterateParentAndClose(data, *clstr, nigh, index);	// Moved to existing cluster or new cluster
			// Sort neighbors from biggest to smallest index
			//std::sort(nigh.begin(), nigh.end(), std::greater<int>());
			// Merge found neighbors to this cluster
			for (auto& neighbor : nigh)
			{
				clstr->pix.insert(clstr->pix.end(), node->data[neighbor].pix.begin(), node->data[neighbor].pix.end());

				if (clstr->minToA > node->data[neighbor].minToA) clstr->minToA = node->data[neighbor].minToA; // Save ToA min
				if (clstr->maxToA < node->data[neighbor].maxToA) clstr->maxToA = node->data[neighbor].maxToA; // Save ToA max
				if (node->data[neighbor].xMax > clstr->xMax) clstr->xMax = node->data[neighbor].xMax;
				if (node->data[neighbor].xMin < clstr->xMin) clstr->xMin = node->data[neighbor].xMin;
				if (node->data[neighbor].yMax > clstr->yMax) clstr->yMax = node->data[neighbor].yMax;
				if (node->data[neighbor].yMin < clstr->yMin) clstr->yMin = node->data[neighbor].yMin;

				// TODO: Try this
				//clstr->minToA = std::min(clstr->minToA, node->data[neighbor].minToA);
			}
			for (const auto& neighbor : nigh)
			{
				node->data.erase(node->data.begin() + neighbor);
				//if (node->data.size() == 1) clstr = node->data.begin();
				//else if (index > neighbor) clstr--;
			}
			// Finish clstr and erase it
			if (node->data.size() != 1)
			{
				doneClusters.emplace_back(*clstr);
				clstr--;
				node->data.erase(node->data.begin() + index);
			}
		}

		void IterateParentAndClose(Clusters& parent, Cluster& current, std::vector<size_t>& neighbors, const size_t& firstCurrent)	// Cluster was added to parent, now iterate if there is a neighbor
		{
			bool clusterJoined = false;
			bool breakLoop = false;
			bool relX, relY = false;

			int64_t firstJoinedCluster = -1;
			size_t index = 0;

			for (auto parClstr = parent.begin(); parClstr != parent.end(); parClstr++)
			{
				if ((current.minToA - parClstr->minToA) > params.maxClusterSpan || (current.minToA - parClstr->minToA) < -params.maxClusterSpan) { continue; }

				index = parClstr - parent.begin();
				if (index == firstCurrent) continue;
				for (size_t idx : neighbors)
				{
					if (idx == index)
					{
						breakLoop = true;
						break;
					}
				}

				if (breakLoop)
				{
					breakLoop = false;
					continue;
				}

				for (const auto& currentPix : current.pix)	// Every pixel in moved lastly to parent
				{
					// TODO: check current to parClstr X and Y min maxes. If unrelated, break here or continue in upper for loop
					if ((currentPix.y < (parClstr->yMin - 1)) || currentPix.y > (parClstr->yMax + 1)) { continue; }
					if ((currentPix.x < (parClstr->xMin - 1)) || currentPix.x > (parClstr->xMax + 1)) { continue; }

					for (const auto& parPix : parClstr->pix)
					{
						relX = ((currentPix.x) == (parPix.x + 1)) || ((currentPix.x) == (parPix.x - 1)) || ((currentPix.x) == (parPix.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
						relY = ((currentPix.y) == (parPix.y + 1)) || ((currentPix.y) == (parPix.y - 1)) || ((currentPix.y) == (parPix.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)

						if (relX && relY)
						{
							// Here is a problem (return): second neighbor of the cluster cannot be found because the functions returns immideately after that
							neighbors.emplace_back(parClstr - parent.begin());
							IterateParentAndClose(parent, *parClstr, neighbors, firstCurrent);
							breakLoop = true;
							break;
							//return;
						}
					}

					if (breakLoop)
					{
						breakLoop = false;
						break;
					}
				}
			}
		}

		void IterateParentV2(Clusters& parent, Cluster& child)	// Cluster was added to parent, now iterate if there is a neighbor
		{
			bool clusterJoined = false;
			bool thisClusterJoined = false;
			bool relX, relY = false;

			size_t firstJoinedCluster = 0;
			size_t index = 0;

			for (auto parClstr = parent.begin(); parClstr != parent.end(); parClstr++)
			{
				if ((child.minToA - parClstr->minToA) > params.maxClusterSpan || (child.minToA - parClstr->minToA) < -params.maxClusterSpan) { continue; }
				if ((child.yMax + 1) < parClstr->yMin || (child.yMin - 1) > parClstr->yMax) continue;
				if ((child.xMax + 1) < parClstr->xMin || (child.xMin - 1) > parClstr->xMax) continue;

				/*relX = (child.xMin >= (parClstr->xMin - 1) && child.xMin <= (parClstr->xMax + 1)) ||
					(child.xMax >= (parClstr->xMin - 1) && child.xMax <= (parClstr->xMax + 1));
				relY = (child.yMin >= (parClstr->yMin - 1) && child.yMin <= (parClstr->yMax + 1)) ||
					(child.yMax >= (parClstr->yMin - 1) && child.yMax <= (parClstr->yMax + 1));
				if (!relX || !relY) continue;*/

				for (const auto& childPix : child.pix)	// Every pixel in moved lastly to parent
				{
					// NOTE: These 2 if dont bring any improvement it seems
					if (childPix.y < (parClstr->yMin - 1) || childPix.y >(parClstr->yMax + 1)) continue;
					if (childPix.x < (parClstr->xMin - 1) || childPix.x >(parClstr->xMax + 1)) continue;

					for (const auto& parPix : parClstr->pix)
					{
						relX = ((childPix.x) == (parPix.x + 1)) || ((childPix.x) == (parPix.x - 1)) || ((childPix.x) == (parPix.x));    // is (X + 1 == my_X) OR (X - 1 == my_X)
						relY = ((childPix.y) == (parPix.y + 1)) || ((childPix.y) == (parPix.y - 1)) || ((childPix.y) == (parPix.y));    // is (Y + 1 == my_Y) OR (Y - 1 == my_Y)

						if (relX && relY && clusterJoined)    // If Neighbor, add to cluster
						{
							// Join current Cluster into LastAddedTo cluster and Erase the current one
							parent[firstJoinedCluster].pix.insert(parent[firstJoinedCluster].pix.end(), parClstr->pix.begin(), parClstr->pix.end());

							/* Join min max values of clusters */
							if (parClstr->xMax > parent[firstJoinedCluster].xMax) parent[firstJoinedCluster].xMax = parClstr->xMax;
							if (parClstr->xMin < parent[firstJoinedCluster].xMin) parent[firstJoinedCluster].xMin = parClstr->xMin;
							if (parClstr->yMax > parent[firstJoinedCluster].yMax) parent[firstJoinedCluster].yMax = parClstr->yMax;
							if (parClstr->yMin < parent[firstJoinedCluster].yMin) parent[firstJoinedCluster].yMin = parClstr->yMin;
							if (parClstr->minToA < parent[firstJoinedCluster].minToA) parent[firstJoinedCluster].minToA = parClstr->minToA; // Merge ToA min
							if (parClstr->maxToA > parent[firstJoinedCluster].maxToA) parent[firstJoinedCluster].maxToA = parClstr->maxToA; // Merge ToA max

							index = parClstr - parent.begin();
							parClstr--;                                 // NO Cluster was reduced by 1
							parent.erase(parent.begin() + index);       // Erase the current Cluster
							thisClusterJoined = true;
							break;
						}
						else if (relX && relY)
						{
							// Insert last MovedToParent cluster to FOUNDCLUSTER and erase the MovedToParent cluster
							parClstr->pix.insert(parClstr->pix.end(), child.pix.begin(), child.pix.end());

							if (parClstr->minToA > child.minToA) parClstr->minToA = child.minToA; // Save ToA min
							if (parClstr->maxToA < child.maxToA) parClstr->maxToA = child.maxToA; // Save ToA max
							if (child.xMax > parClstr->xMax) parClstr->xMax = child.xMax;
							if (child.xMin < parClstr->xMin) parClstr->xMin = child.xMin;
							if (child.yMax > parClstr->yMax) parClstr->yMax = child.yMax;
							if (child.yMin < parClstr->yMin) parClstr->yMin = child.yMin;

							firstJoinedCluster = parClstr - parent.begin();
							clusterJoined = true;
							thisClusterJoined = true;
							break;
						}
					}

					if (thisClusterJoined)
					{
						thisClusterJoined = false;
						break;
					}
				}
			}

			if (clusterJoined == false)	parent.emplace_back(child);
			return;
		}

		void merge_cluster_params(Cluster& original, Cluster& joined)
		{
			original.minToA = std::min(original.minToA, joined.minToA);
			original.maxToA = std::max(original.maxToA, joined.maxToA);
			original.xMax = std::max(original.xMax, joined.xMax);
			original.yMax = std::max(original.yMax, joined.yMax);
			original.xMin = std::min(original.xMin, joined.xMin);
			original.yMin = std::min(original.yMin, joined.yMin);
		}

		inline int64_t m_abs(const int64_t& a)
		{
			if (a >= 0) return a;

			return -a;
		}
	};

public:
	void do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort);

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
