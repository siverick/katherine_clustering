#ifndef CLUSTERER_CLUSTERER_H_
#define CLUSTERER_CLUSTERER_H_

/**
 * @file clusterer.h
 * @author Lukas Meduna (meduna@medunalukas.com)
 * @brief The core class that performs combining of pixels into clusters based on their time and
 * spacial coincidence.
 * @date 01/11/2018
 *
 * @copyright Lukas Meduna (c) 2018
 *
 */

#include <map>
#include <sstream>
#include <thread>
#include <vector>

#include "m_cluster_data.h"

template <size_t chip_size> class Clusterer {
public:
  Clusterer() : log_("Clusterer") {
    last_time_ = 0.;
    run_ = true;
    counter_ = 0;
    test_output.open("leafs.txt");
  }
  ~Clusterer(){};

  /**
   * @brief Called by Orchestrator and performs the clustering and flushing of buffers.
   *
   */
  void Run() {
    run_ = true;
    while (run_) {
      if (!queue_.isEmpty()) {
        auto item = queue_.Pop();
        ProcessPixelData(item.first, item.second);
      } else if (counter_ > 10000) {
        queue_.Flush();
        output_.Flush();
        counter_ = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      } else {
        ++counter_;
      }
    }
    // Orchestrator told us to end, we will just clean the remains.
    while (!queue_.isEmpty()) {
      if (!queue_.isEmpty()) {
        auto item = queue_.Pop();
        ProcessPixelData(item.first, item.second);
      }
    }
    // Clouse all clusters and dispatch them
    CloseClusters();
    // flush buffers
    output_.Flush();
    test_output.close();
  }
  /**
   * @brief Waitch for signal from Orchestrator to end.
   *
   */
  void End() { run_ = false; }

  /**
   * @brief Throw everything away and end.
   *
   */
  void Abort() {
    run_ = false;
    mtx_.lock();
    queue_.Clear();
    mtx_.unlock();
  }

  void Reset() { run_ = true; }
  /**
   * @brief
   *
   * @param pixeldata
   * @param reader_index
   */
  void ProcessPixelData(PixelData &pixel_data, size_t reader_index) {

    // Check if we should close something
    CloseClusters(max_delay);
    last_time_ = pixel_data.toa;

    // Finds or creates corresponding structure for pixels from detector with ID.
    if (open_clusters_.find(reader_index) == open_clusters_.end())
      open_clusters_.emplace(std::make_pair(reader_index, std::vector<ClusterDataBuilder>()));

    // Assume we have not added pixel to cluster
    was_added_to_cluster_ = false;

    // We will go through all clusters and try to add the pixel there. We need iterator to previous
    // cluster, in case we would need to join the clusters.
    joining_cluster_ = open_clusters_.at(reader_index).begin();
    // Store x,y,toa temporary
    double toa = pixel_data.toa;
    size_t x = pixel_data.pixel_x;
    size_t y = pixel_data.pixel_y;

    // Go through all open clusters
    for (auto cluster_it = open_clusters_.at(reader_index).begin();
         cluster_it != open_clusters_.at(reader_index).end();) {
      // Deoes the pixel lies in neighbour of the cluster?
      if ((*cluster_it).CanBeAdded(x, y, toa, max_cluster_length)) {
        // Is it first time adding this pixel to cluster?
        if (!was_added_to_cluster_) {
          // Note iterator
          joining_cluster_ = cluster_it;
          // Add the pixel
          (*cluster_it).AddPixel(pixel_data);
          was_added_to_cluster_ = true;
        } else {
          // Pixel was already added, so we need to join the cluster together and delete the remains
          // of joined cluster
          (*joining_cluster_).JoinWith(*cluster_it);
          cluster_it = open_clusters_.at(reader_index).erase(cluster_it);
          continue;
        }
      }
      ++cluster_it;
    }
    // If we have not added pixel to cluster, create new one.
    if (!was_added_to_cluster_) {
      ClusterDataBuilder cluster;
      cluster.AddPixel(pixel_data);
      open_clusters_.at(reader_index).emplace_back(std::move(cluster));
    }
  }

  /**
   * @brief Sets the destionation of completed clusters.
   *
   * @param receiver
   */
  void RegisterReceiver(std::shared_ptr<Receiver<chip_size>> receiver) {
    receiver->SetQueue(&output_);
    receiver_ = receiver;
  };
  volatile size_t max_delay = 500000;
  volatile size_t max_cluster_length = 2000;
  MTQueueBuffered<std::pair<PixelData, size_t>, 50> queue_;
  MTQueueBuffered<ClusterData, 50> output_;

private:
  Logger log_;

  // Disable copy
  Clusterer(const Clusterer &a);

  /**
   * @brief Close clusters that are further from the last time we got pixel than the time dalay
   * specified. Dispatches them to Receiver.
   *
   * @param time_delay
   */
  void CloseClusters(size_t time_delay) {
    for (auto readers_it = open_clusters_.begin(); readers_it != open_clusters_.end();
         ++readers_it) {
      for (auto open_cluster = readers_it->second.begin();
           open_cluster != readers_it->second.end();) {
        if (last_time_ - (*open_cluster).first_toa > time_delay) {
          // Peel of the unnecesary members
          test_output << open_cluster->neighbors_;
          ClusterData cdp = static_cast<ClusterData>(*open_cluster);
          output_.PushBack(std::move(cdp));
          open_cluster = readers_it->second.erase(open_cluster);
        } else
          ++open_cluster;
      }
    }
  }
  /**
   * @brief Closes clusters and dispatches them to Receiver.
   *
   */
  void CloseClusters() {
    for (auto readers_it = open_clusters_.begin(); readers_it != open_clusters_.end();
         ++readers_it) {
      for (auto open_cluster = readers_it->second.begin();
           open_cluster != readers_it->second.end();) {
        // Peel of the unnecesary members
        test_output << open_cluster->neighbors_;
        ClusterData cdp = static_cast<ClusterData>(*open_cluster);
        output_.PushBack(std::move(cdp));
        open_cluster = readers_it->second.erase(open_cluster);
      }
    }
  }

  volatile std::atomic<bool> run_;

  std::chrono::time_point<std::chrono::system_clock> auto_flush_;
  // Destination for the data.
  std::shared_ptr<Receiver<chip_size>> receiver_;
  std::vector<ClusterDataBuilder>::iterator joining_cluster_;
  std::map<size_t, std::vector<ClusterDataBuilder>> open_clusters_;
  bool was_added_to_cluster_ = false;
  double last_time_; // time of last recieved message
  std::mutex mtx_;
  std::size_t counter_;
  std::ofstream test_output;
};

#endif // CLUSTERER_CLUSTERER_H_
