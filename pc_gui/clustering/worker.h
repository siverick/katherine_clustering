
/**
 * @worker.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once
#include <qobject.h>

#include <QThread>
#include <QDebug>
#include <QTimer>
#include "file_loader.h"
#include <QtCharts>
#include "clustering_baseline.h"
#include "clustering_quadtree.h"
#include "clustering_time.h"
#include "clustering_time_embed.h"
#include <string>
#include "plugin_definition.h"
#include <thread>
#include <atomic>
#include <chrono>
#include "cluster_benchmark.h"
#include "network.h"
#include "serializer.h"
#include "MTQueue.h"
#include "file_saver.h"

#define _ITERATOR_DEBUG_LEVEL 0

enum FileType {
	inputData, calibA, calibB, calibC, calibT
};

class main_worker : public QObject, private plugin_definition
{
	Q_OBJECT

public:
	main_worker();
	~main_worker();

	// Online and offline clustering thread starts
	void start_offline_clustering();
	void start_online_file_clustering();
	
	// Clusters explorer
	void next_cluster();
	void prev_cluster();
	bool specific_cluster(int val);

	// Set clustering parameters
	void set_cluster_span(int val);
	void set_cluster_delay(int val);
	void set_cluster_size(int filter, bool filterBiggerCheck);
	void set_filter_size(int val);

	// Online clustering functions
	void toggle_online_clustering(int32_t port);
	void select_online_mode(plugins plug);
	void online_clustering_loop(int32_t port);
	void handle_mode();
	int handle_incoming_data(std::string& input);
	int process_dataframe(const std::string& input);
	void process_message(const std::string& input);
	void handle_other_requests();

	// Online clustering stats
	void enable_reset_screen(bool enabled);
	void update_reset_period(int period);

	// save done clusters
	void save_done_clusters();
	size_t get_number_of_clusters()
	{
		return done_clusters.Size();
	}

	// Online clustering flags for synchronization
	volatile bool abort;
	std::atomic<bool> doneSaving = false;
	std::atomic<bool> online_running = false;
	std::atomic<bool> connecting = false;

public slots:
	void load_file_path(QString path, FileType type);
	void set_calib_state(int state);
	void toggle_histogram();
	void sortClustersBig();
	void sortClustersSmall();
	void sortClustersNew();
	void sortClustersOld();
	void toggle_cluster_frame();
	void clearClusters();
	void sendParamsOnline();

signals:
	void render_all(QPixmap image);
	void render_one(QPixmap cluster, int longestSide);
	void cluster_info(int idx, int size, int total_ToT, int time_span, uint64_t ToA);
	void show_clustering_stats(std::string stats);
	void update_progress(int val);
	void update_calib_status(bool done, FileType type);
	void show_histogram_online(std::vector<uint16_t> histo, bool restart_required);
	void server_mode_now(plugins plug);
	void server_status_now(plugin_status stat);
	void server_log_now(std::string message);
	void clusters_per_second(uint64_t val);
	void max_clusters_per_second(uint64_t val);
	void hitrate(uint64_t val);
	void max_hitrate(uint64_t val);
	void pixels_received(uint64_t val);
	void clusters_received(uint64_t val);
	void show_popup(QString title, QString message, QMessageBox::Icon type);

private:
	bool running = false;	// Program running flag

	// File input for offline clustering
	std::string input;
	std::string pathToLastInput;

	// Testing
	void testbench(std::string input, ClusteringParams params);

	// Frame rendering functionality
	bool is_frame_rendered = false;
	MTVector<ClusterType> frame;

	// Local clustering parameters
	int maxClusterSpan = 0;
	int maxClusterDelay = 0;
	int clusterFilterSize = 0;
	bool filterBiggerClusters = false;
	int filterSize = 0;

	// GUI related functions
	void show_progress(bool newOp, int no_lines);
	bool show_cluster(int index);
	int idx = 0;

	// Calibration matrixes, and clustering parameters
	float cal_a[256 * 256];
	float cal_b[256 * 256];
	float cal_c[256 * 256];
	float cal_t[256 * 256];
	bool calib_enabled;
	bool calibs_loaded;
	ClusteringParams params{};
	void calib_load(std::string fileName, FileType type);
	double energy_calc(const int& x, const int& y, const int& ToT);

	// Different clustering methods objects
	clustering_baseline* m_baseline;
	clustering_quadtree* m_quadtree;
	clustering_time_parallelisation* m_time_parallelisation;
	clustering_time_embed* m_time_embed;
	cluster_benchmark* m_benchmark;

	// Clean the online data containers and release memory
	void cleanup_online_data()
	{
		// Reset data containers
		online_clusters.clear();
		online_clusters.shrink_to_fit();
		almost_done_clusters.clear();
		almost_done_clusters.shrink_to_fit();

		// Clear energies
		done_energies.clear();
		done_energies.shrink_to_fit();
		online_energies.clear();
		online_energies.shrink_to_fit();
		almost_done_energies.clear();
		almost_done_energies.shrink_to_fit();

		// Clear pixels
		online_pixels.clear();
		online_pixels.shrink_to_fit();
		online_pixel_counts.clear();
		online_pixel_counts.shrink_to_fit();
	}

	// Online clustering
	MTVector<ClusterType> done_clusters;			// Done, used for browsing clusters
	std::vector<ClusterType> almost_done_clusters;	// Done but not displayed yet
	std::vector<CompactClusterType> online_clusters;
	std::vector<OnePixel> online_pixels;
	std::vector<OnePixelCount> online_pixel_counts;
	PixelCounts* pixel_counts_matrix;
	std::vector<uint16_t> online_energies;
	std::vector<uint16_t> done_energies;
	std::vector<uint16_t> almost_done_energies;
	std::thread t_online;
	std::thread t_online_stats;
	std::atomic<plugins> mode;
	std::atomic<bool> meas_running = false;	// Not thread safe - designed to be used only in main online thread
	networking* net = nullptr;
	MTQueue<std::string> online_requests;

	// Online stats 
	void statistics_thread();
	bool reset_picture_enable = false;
	int reset_period = 0;
	size_t cluster_counter = 0;
	size_t pixel_counter = 0;

	// Rendering of online acquired data
	void render_pixels();
	void render_pixel_counts();
	void render_histo_only();
	void render_progress();

	// Cluster rendering related variables with lock - multithreaded!
	QPixmap picture;
	QPainter painter;
	std::mutex paint_lock;
};
