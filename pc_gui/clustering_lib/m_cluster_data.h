#ifndef CLUSTERER_CLUSTER_DATA_H_
#define CLUSTERER_CLUSTER_DATA_H_
/**
 * @file cluster_data.h
 * @author Lukas Meduna (meduna@medunalukas.com)
 * @brief Stores Cluster Data and helper class that is used during creation of clusters, which
 * includes quadtree for fost addition of pixels to custer.
 * @date 01/11/2018
 *
 * @copyright Lukas Meduna (c) 2018
 *
 */

#include <cmath>
#include <limits>
#include <list>
#include <ostream>
#include <sstream>
#include <vector>
#include <cstring>

// #include <inttypes.h>

#include "m_picture_tree.h"

/**
 * @brief Basic cluster information.
 *
 */
class ClusterData : public cluster_definition {
public:
    PixelCluster cluster;
    // Earlies pixel time of arrival
    double first_toa = std::numeric_limits<double>::max();

    /**
     * @brief Sums the energy of whole cluster
     *
     * @return double Energy in keV
     */
    double GetEnergy() {
        double res = 0;
        for (auto& px : cluster)
            res += px.ToT;
        return res;
    }
};

/**
 * @brief Used to build up the clusters. Additional member is PictureTree (Quadtree) that is used
 * to determine if pixel to be added is neigbour of current cluster.
 *
 */
class ClusterDataBuilder : public ClusterData {
public:
  /**
   * @brief Checks if pixel can be added to current cluster.
   *
   * @param x
   * @param y
   * @param toa Time of arrival of the pixel.
   * @param max_cluster_lenght Maximal time between arrival of first pixel.
   * @return true
   * @return false
   */

  bool CanBeAdded(const int &x, const int &y, const double &toa, const size_t max_cluster_lenght) {
    if (std::abs(first_toa - toa) > max_cluster_lenght)
      return false;
    return neighbors_.Contains(x, y);
  }
  /**
   * @brief Combines two clusters into one.
   *
   * @param cd Cluster that will be added to this cluster. The state of cd& is undefined after
   * this operation, should be destroyed.
   */
  void JoinWith(ClusterDataBuilder &cd) {
    cluster.reserve(cluster.size() + cd.cluster.size()); // preallocate memory
    cluster.insert(cluster.end(), std::make_move_iterator(cd.cluster.begin()),
                   std::make_move_iterator(cd.cluster.end()));
    first_toa = std::min(first_toa, cd.first_toa);
    neighbors_.JoinWith(cd.neighbors_);
  }
  /**
   * @brief Adds pixel to cluster, no check if is pixel spacially connected to cluster is
   * performed!
   *
   * @param p Pixel to be added
   */
  void AddPixel(OnePixel &p) {
    if (first_toa > p.ToA)
      first_toa = p.ToA;
    // We are tracking neighbouring pixels, so we add 8-neighbour to the pixel we want to add to
    // cluster.
    neighbors_.AddPoint(p.x - 1, p.y - 1);
    neighbors_.AddPoint(p.x - 1, p.y);
    neighbors_.AddPoint(p.x - 1, p.y + 1);
    neighbors_.AddPoint(p.x, p.y + 1);
    neighbors_.AddPoint(p.x, p.y - 1);
    neighbors_.AddPoint(p.x + 1, p.y - 1);
    neighbors_.AddPoint(p.x + 1, p.y);
    neighbors_.AddPoint(p.x + 1, p.y + 1);

    cluster.emplace_back(std::move(p));
  }

  PictureTree<256, 16> neighbors_;
};
#endif // CLUSTERER_CLUSTER_DATA_H_
