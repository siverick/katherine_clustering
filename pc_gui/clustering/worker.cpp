
/**
 * @worker.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "stdafx.h"
#include "worker.h"
#include <algorithm>
#include <QElapsedTimer>
#include <QDebug>
#include <sstream>
#include "postprocesing.h"
#include <thread>

main_worker::main_worker()
{
	abort = false;
	input = "";
	idx = 0;
	set_cluster_span(200);
	set_cluster_delay(200);
	filterSize = 0;
	calibs_loaded = false;
	calib_enabled = false;

	m_baseline = new clustering_baseline;
	m_quadtree = new clustering_quadtree;
	m_time_parallelisation = new clustering_time_parallelisation(abort);
	m_time_embed = new clustering_time_embed(abort);
	m_benchmark = new cluster_benchmark;

	mode = plugins::idle;

	params.calibReady = calibs_loaded && calib_enabled;
	params.maxClusterSpan = maxClusterSpan;
	params.maxClusterDelay = maxClusterDelay;
	params.rn_delim = false;
	params.outerFilterSize = filterSize;
	params.no_lines = 0;
}

main_worker::~main_worker()
{
	delete m_baseline;
	delete m_quadtree;
	delete m_time_parallelisation;
	delete m_time_embed;
	delete m_benchmark;
	if (net != nullptr)
	{
		delete net;
	}


	// set online running false to later join the threads
	if (online_running == true) online_running = false;
	if (t_online.joinable()) t_online.join();
	if (t_online_stats.joinable()) t_online_stats.join();
}

int parse_calib_file(float* calib_matrix, std::string& calib_input) {
	char* context = nullptr;
	char* token = strtok_s(calib_input.data(), "\r\n\t ", &context);
	int i = 0;

	try {
		calib_matrix[i++] = std::stof(token);
	}
	catch (const std::exception&) { // Exception out-of-range stod() if token is a word
		return -1;
	}

	while (token)
	{
		token = strtok_s(NULL, "\r\n\t ", &context);
		if (token)
		{
			try {
				calib_matrix[i++] = std::stof(token);
			}
			catch (const std::exception&) {
				return -1;
			}
		}
	}

	return i;
}

void main_worker::calib_load(std::string fileName, FileType type)
{
	/* Loading flags */
	static bool a_loaded = false;
	static bool b_loaded = false;
	static bool c_loaded = false;
	static bool t_loaded = false;

	std::string calib_in = file_loader::loadCalibration(fileName);
	if (calib_in == "fault") emit update_calib_status(false, type);
	int i = 0;

	switch (type)
	{
	case FileType::calibA:
		i = parse_calib_file(cal_a, calib_in);
		break;
	case FileType::calibB:
		i = parse_calib_file(cal_b, calib_in);
		break;
	case FileType::calibC:
		i = parse_calib_file(cal_c, calib_in);
		break;
	case FileType::calibT:
		i = parse_calib_file(cal_t, calib_in);
		break;
	case FileType::inputData:
		return;
	}

	if (i != 65536) {   // TODO: emit error to user
		emit update_calib_status(false, type);
	}
	else   // Load succesful - set flag
	{
		if (type == FileType::calibA) a_loaded = true;
		if (type == FileType::calibB) b_loaded = true;
		if (type == FileType::calibC) c_loaded = true;
		if (type == FileType::calibT) t_loaded = true;

		calibs_loaded = a_loaded && b_loaded && c_loaded && t_loaded;
		emit update_calib_status(true, type);
	}
}

void main_worker::load_file_path(QString path, FileType type)
{
	if (type == FileType::inputData) file_loader::loadPixelData(path.toStdString(), input);
	else calib_load(path.toStdString(), type);

	if (input == "fault") input = "";
	else if (type == FileType::inputData) pathToLastInput = path.toStdString();
}

/* Detect delimiter /r/n for get_my_line() */
bool det_delim(const std::string& str) {
	std::string start = str.substr(0, 500);
	size_t f_n = start.find('\n', 0);
	size_t f_r = start.find('\r', 0);

	if (f_n - f_r == 1) return true;
	else return false;
}

void main_worker::show_progress(bool newOp, int no_lines) {
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
			emit update_progress(prog_val);
		}
	}
	else
	{
		emit update_progress(0);
		prog = 0;
		last_prog = 0;
		prog_val = 0;
	}
}

void CompareDoneClusters(std::vector<ClusterType> right, std::vector<ClusterType> toCheck)
{
	size_t sizeRight = right.size();
	bool foundRight = false;
	size_t numOfBad = 0;

	for (int index = 0; index < sizeRight; index++)
	{
		foundRight = false;
		for (auto& cluster : toCheck)
		{
			if (right[index].minToA == cluster.minToA)
			{
				foundRight = true;

				if ((right[index].pix.size() != cluster.pix.size()))
				{
					numOfBad++;
					qDebug() << QString::fromStdString(utility::print_time_info("False cluster at: ", "%", (static_cast<double>(index) / static_cast<double>(sizeRight)) * 100));
					qDebug() << QString::fromStdString(utility::print_time_info("Diff in size", "pixels", static_cast<double>(right[index].pix.size()) - static_cast<double>(cluster.pix.size())));
				}
			}
		}

		if (foundRight == false)
		{
			numOfBad++;
			qDebug() << QString::fromStdString(utility::print_time_info("Didnt find at all in: ", "%", (static_cast<double>(index) / static_cast<double>(sizeRight)) * 100));
		}
	}

	qDebug() << QString::fromStdString((utility::print_time_info("Bad clusters:", "cl", numOfBad)));
}

void main_worker::testbench(std::string input, ClusteringParams params)
{
	if (online_running || connecting)
	{
		emit show_popup("Tip.", "For processing offline file, please cancel online clustering.\nFor processing online measured data, click process online pixels button", QMessageBox::Information);
		return;
	}

	idx = 0;
	m_baseline->erase_done_clusters();
	m_quadtree->erase_done_clusters();
	m_time_parallelisation->erase_done_clusters();
	m_time_embed->erase_done_clusters();
	m_benchmark->erase_done_clusters();

	if (input == "")  // Load file for clustering
	{
		if (pathToLastInput == "") input = file_loader::loadDefaultFile();
		else file_loader::loadPixelData(&pathToLastInput[0], input);
		if (input == "fault")
		{
			input = "";
			return;
		}
	}

	bool rn_delim = det_delim(input);   // Detect type of delimiter - /n  ... /r/n
	int no_lines = input.size() / 24;
	show_progress(true, no_lines);

	params.calibReady = calibs_loaded && calib_enabled;
	params.maxClusterSpan = maxClusterSpan;
	params.maxClusterDelay = maxClusterDelay;
	params.rn_delim = rn_delim;
	params.outerFilterSize = filterSize;
	params.no_lines = no_lines;

	if (params.calibReady)	// If calib loaded && enabled, set it
	{
		m_baseline->set_calibs(cal_a, cal_b, cal_c, cal_t);
		m_quadtree->set_calibs(cal_a, cal_b, cal_c, cal_t);
		m_time_parallelisation->set_calibs(cal_a, cal_b, cal_c, cal_t);
		m_time_parallelisation->set_calibs(cal_a, cal_b, cal_c, cal_t);
		m_benchmark->set_calibs(cal_a, cal_b, cal_c, cal_t);
	}

	qDebug() << "---------- Filter: " << params.outerFilterSize << " | Calib: " << params.calibReady << " ----------";

	//m_spatial_parallelisation->do_clustering(input, params, abort);
	//m_quadtree->do_clustering(input, params, abort);
	m_baseline->do_clustering(input, params, abort);
	//m_time_embed->do_clustering(input, params, abort);
	//m_time_parallelisation->do_clustering(input, params, abort);
	std::string stats = "";

	//auto doneRight = m_baseline->sort_clusters_toa(SortType::smallFirst);
	//auto doneLeft = m_time_embed->sort_clusters_toa(SortType::smallFirst);
	//CompareDoneClusters(doneRight, doneLeft);

	input.clear();
	input.shrink_to_fit();

	// Save clusters and plot them
	m_baseline->sort_clusters(SortType::bigFirst);
	done_clusters.Erase(done_clusters.begin(), done_clusters.end());
	done_clusters.Insert(m_baseline->get_done_clusters());
	auto temp = done_clusters.Get_All();
	postprocesing::plot_whole_image(picture, painter, temp);

	/*
	TESTBENCH
	*/

	/* //TEST LUT sizes according to depth
	std::this_thread::sleep_for(std::chrono::seconds(2));
	m_benchmark->create_LUT_test(1000);
	std::this_thread::sleep_for(std::chrono::seconds(2));
	m_benchmark->create_LUT_test(1500);
	std::this_thread::sleep_for(std::chrono::seconds(2));
	m_benchmark->create_LUT_test(2000);
	std::this_thread::sleep_for(std::chrono::seconds(2));

	return;
	*/

	m_benchmark->parse_data(input, params, abort);

	for (int i = 0; i < 0; i++)
	{
		m_benchmark->do_clustering_A(input, params, abort);
		emit show_clustering_stats("STAGE1: ");
		stats = m_benchmark->stat_print();
		emit show_clustering_stats(stats);
		qDebug() << "STAGE1: " << QString::fromStdString(stats);
	}

	for (int i = 0; i < 0; i++)
	{
		m_benchmark->do_clustering_B(input, params, abort);
		emit show_clustering_stats("STAGE2: ");
		stats = m_benchmark->stat_print();
		emit show_clustering_stats(stats);
		qDebug() << "STAGE2: " << QString::fromStdString(stats);
	}

	for (int i = 0; i < 0; i++)
	{
		m_benchmark->do_clustering_C(input, params, abort);
		emit show_clustering_stats("STAGE3: ");
		stats = m_benchmark->stat_print();
		emit show_clustering_stats(stats);
		qDebug() << "STAGE3: " << QString::fromStdString(stats);
	}

	for (int i = 0; i < 0; i++)
	{
		m_benchmark->do_clustering_D(input, params, abort);
		emit show_clustering_stats("STAGENOW: ");
		stats = m_benchmark->stat_print();
		emit show_clustering_stats(stats);
		qDebug() << "STAGENOW: " << QString::fromStdString(stats);

		/*		auto& clusters = m_benchmark->get_done_clusters();
				int maxSize = 0;
				int minSize = 0;
				int avgSize = 0;

				for (auto& cluster : clusters)
				{
					if (minSize == 0) minSize = cluster.pix.size();
					if (minSize > cluster.pix.size()) minSize = cluster.pix.size();
					if (maxSize < cluster.pix.size()) maxSize = cluster.pix.size();
					avgSize += cluster.pix.size();
				}

				avgSize = avgSize / clusters.size();

				qDebug() << "minsize: " << minSize;
				qDebug() << "maxsize: " << maxSize;
				qDebug() << "avgsize: " << avgSize;*/
	}

	for (int i = 0; i < 0; i++)
	{
		m_benchmark->do_clustering_E(input, params, abort);
		emit show_clustering_stats("IMPROVED: ");
		stats = m_benchmark->stat_print();
		emit show_clustering_stats(stats);
		qDebug() << "IMPROVED: " << QString::fromStdString(stats);
	}

	for (int i = 0; i < 0; i++)
	{
		m_benchmark->do_clustering_F(input, params, abort);
		emit show_clustering_stats("MINMAX CALIB: ");
		stats = m_benchmark->stat_print();
		emit show_clustering_stats(stats);
		qDebug() << "MINMAX CALIB: " << QString::fromStdString(stats);
	}

	for (int i = 0; i < 0; i++)
	{
		m_benchmark->do_clustering_G(input, params, abort);
		emit show_clustering_stats("MINMAX LUT: ");
		stats = m_benchmark->stat_print();
		emit show_clustering_stats(stats);
		qDebug() << "MINMAX LUT: " << QString::fromStdString(stats);
	}

	/*
	TESTBENCH
	*/


	// Test whether loaded clusters match saved ones
	//auto loaded = file_saver::loadClustersFromFile(savedFile);

	/* Performance and stats */
	emit show_clustering_stats("MINMAX: ");
	stats = m_baseline->stat_print();
	emit show_clustering_stats(stats);
	qDebug() << "MINMAX: " << QString::fromStdString(stats);

	/*
	emit show_clustering_stats("QUADTREE: ");
	stats = m_quadtree->stat_print();
	emit show_clustering_stats(stats);
	qDebug() << "QUADTREE: " << QString::fromStdString(stats);
	*/

	/*
	qDebug() << "EMBED LOG: " << QString::fromStdString(m_time_embed->log_return());
	emit show_clustering_stats("EMBED: ");
	stats = m_time_embed->stat_print();
	emit show_clustering_stats(stats);
	qDebug() << "EMBED: " << QString::fromStdString(stats);
	*/
	/*
	qDebug() << "TIME LOG: " << QString::fromStdString(m_time_parallelisation->log_return());
	emit show_clustering_stats("TIME: ");
	stats = m_time_parallelisation->stat_print();
	emit show_clustering_stats(stats);
	qDebug() << "TIME: " << QString::fromStdString(stats);
	*/
	/* Render clusters and info */
	emit render_all(picture);
	emit cluster_info(0, 0, 0, 0, 0);
	emit update_progress(100);
	is_frame_rendered = false;
}

void main_worker::start_offline_clustering()
{
	if (online_running || connecting)
	{
		emit show_popup("Tip.", "For processing offline file, please cancel online clustering.\nFor processing online measured data, click process online pixels button", QMessageBox::Information);
		return;
	}

	idx = 0;
	m_baseline->erase_done_clusters();

	if (input == "")  // Load file for clustering
	{
		if (pathToLastInput == "") input = file_loader::loadDefaultFile();
		else file_loader::loadPixelData(&pathToLastInput[0], input);
		if (input == "fault")
		{
			input = "";
			return;
		}
	}

	bool rn_delim = det_delim(input);   // Detect type of delimiter - /n  ... /r/n
	int no_lines = input.size() / 24;
	show_progress(true, no_lines);

	params.calibReady = calibs_loaded && calib_enabled;
	params.maxClusterSpan = maxClusterSpan;
	params.maxClusterDelay = maxClusterDelay;
	params.rn_delim = rn_delim;
	params.outerFilterSize = filterSize;
	params.no_lines = no_lines;

	if (params.calibReady)	// If calib loaded && enabled, set it
	{
		m_baseline->set_calibs(cal_a, cal_b, cal_c, cal_t);
	}

	// Cluster the specified data
	m_baseline->do_clustering(input, params, abort);
	std::string stats = "";

	// Release input memory
	input.clear();
	input.shrink_to_fit();

	// Save clusters and plot them
	m_baseline->sort_clusters(SortType::bigFirst);
	done_clusters.Erase(done_clusters.begin(), done_clusters.end());
	done_clusters.Insert(m_baseline->get_done_clusters());
	auto temp = done_clusters.Get_All();
	postprocesing::plot_whole_image(picture, painter, temp);

	// Testbench
	//testbench(input, params);

	/* Performance and stats */
	emit show_clustering_stats("MINMAX: ");
	stats = m_baseline->stat_print();
	emit show_clustering_stats(stats);
	qDebug() << "MINMAX: " << QString::fromStdString(stats);

	/* Render clusters and info */
	emit render_all(picture);
	emit cluster_info(0, 0, 0, 0, 0);
	emit update_progress(100);
	is_frame_rendered = false;
}

void main_worker::start_online_file_clustering()
{
	idx = 0;
	m_baseline->erase_done_clusters();

	if (input == "")  // Load file for clustering
	{
		if (pathToLastInput == "") emit show_popup("Loading file error!", "Please select path to file.", QMessageBox::Warning);
		else file_loader::loadPixelData(&pathToLastInput[0], input);
		if (input == "fault")
		{
			emit show_popup("Loading file error!", "Please select correct path to file.", QMessageBox::Warning);
		}
	}

	bool rn_delim = det_delim(input);   // Detect type of delimiter - /n  ... /r/n
	int no_lines = input.size() / 24;
	show_progress(true, no_lines);

	params.calibReady = calibs_loaded && calib_enabled;
	params.maxClusterSpan = maxClusterSpan;
	params.maxClusterDelay = maxClusterDelay;
	params.rn_delim = rn_delim;
	params.outerFilterSize = filterSize;
	params.no_lines = no_lines;

	if (params.calibReady)	// If calib loaded && enabled, set it
	{
		m_baseline->set_calibs(cal_a, cal_b, cal_c, cal_t);
	}

	m_baseline->do_online_file_clustering(input, params, abort);
	std::string stats = "";

	input.clear();
	input.shrink_to_fit();

	// Save clusters and plot them
	m_baseline->sort_clusters(SortType::bigFirst);
	done_clusters.Erase(done_clusters.begin(), done_clusters.end());
	done_clusters.Insert(m_baseline->get_done_clusters());
	auto temp = done_clusters.Get_All();

	emit show_clustering_stats("Results: ");
	stats = m_baseline->stat_print();
	emit show_clustering_stats(stats);
	stats = "Clusters found: " + std::to_string(done_clusters.Size());
	emit show_clustering_stats(stats);

	std::lock_guard<std::mutex> lock(paint_lock);
	postprocesing::plot_whole_image(picture, painter, temp);

	/* Render clusters and info */
	emit render_all(picture);
	emit cluster_info(0, 0, 0, 0, 0);
	is_frame_rendered = false;
}

void main_worker::next_cluster()
{
	//auto doneCl = m_bruteforce->get_done_clusters();
	auto doneCl = done_clusters.Size();

	if (idx < doneCl - 1)
	{
		idx++;
	}

	show_cluster(idx);
}

void main_worker::prev_cluster()
{
	if (idx > 0) {
		idx--;
	}

	show_cluster(idx);
}

bool main_worker::specific_cluster(int index)
{
	bool result = show_cluster(index);
	if (result) idx = index;

	return result;
}

void main_worker::set_cluster_span(int val)
{
	maxClusterSpan = val;
}

void main_worker::set_cluster_delay(int val)
{
	maxClusterDelay = val * 1000;    // Mul by 1000 to convert us to ns
}

void main_worker::set_cluster_size(int filter, bool filterBiggerCheck)
{
	clusterFilterSize = filter;
	filterBiggerClusters = filterBiggerCheck;
}

void main_worker::set_filter_size(int val)
{
	filterSize = val;
}

void main_worker::set_calib_state(int state)
{
	if (state > 0) calib_enabled = true;
	else calib_enabled = false;
}

void main_worker::toggle_histogram()
{
	// Render online histograms
	if (online_running == true && meas_running == false && mode == plugins::clustering_energies)
	{
		emit show_histogram_online(postprocesing::convert_energy_to_histogram(done_energies), true);
		return;
	}

	// return in case of running energies
	if (online_running == true && meas_running == true && mode == plugins::clustering_energies)
	{
		return;
	}

	std::vector<uint16_t> energies;
	uint16_t cluster_energy = 0;
	auto doneCl = online_running ? done_clusters.Get_All() : m_baseline->get_done_clusters();

	/* Create energies vector where x = energie and y = their count in Clusters */
	for (const auto& cluster : doneCl) {
		cluster_energy = 0;
		for (const auto& pix : cluster.pix)     // SUM up THIS cluster energy
		{
			cluster_energy += pix.ToT;
			assert(pix.ToT >= 0, "TOT cannot be negative");
		}

		energies.emplace_back(cluster_energy);
	}

	emit show_histogram_online(postprocesing::convert_energy_to_histogram(energies), true);  // Let the GUI thread handle the data
}

void main_worker::sortClustersBig()
{
	if (meas_running) 
	{
		show_popup("Sorting unsucessful", "Stop online measurement before sorting clusters.", QMessageBox::Warning);
		return;
	}

	MTVector<ClusterType>* clusters;
	if (is_frame_rendered == true)
	{
		clusters = &frame;
	}
	else
	{
		clusters = &done_clusters;
	}

	auto cl = clusters->Get_All_And_Erase();
	m_baseline->sort_clusters(SortType::bigFirst, cl);
	clusters->Insert(cl);
	size_t number = clusters->Size();
	QString message = "Sorted  clusters.";
	message.insert(7, QString::number(number));
	show_popup("Sorting sucessful", message, QMessageBox::Information);
	return;
}

void main_worker::sortClustersSmall()
{
	if (meas_running)
	{
		show_popup("Sorting unsucessful", "Stop online measurement before sorting clusters.", QMessageBox::Warning);
		return;
	}

	MTVector<ClusterType>* clusters;
	if (is_frame_rendered == true)
	{
		clusters = &frame;
	}
	else
	{
		clusters = &done_clusters;
	}

	auto cl = clusters->Get_All_And_Erase();
	m_baseline->sort_clusters(SortType::smallFirst, cl);
	clusters->Insert(cl);
	size_t number = clusters->Size();
	QString message = "Sorted  clusters.";
	message.insert(7, QString::number(number));
	show_popup("Sorting sucessful", message, QMessageBox::Information);
	return;
}

void main_worker::sortClustersNew()
{
	if (meas_running)
	{
		show_popup("Sorting unsucessful", "Stop online measurement before sorting clusters.", QMessageBox::Warning);
		return;
	}

	MTVector<ClusterType>* clusters;
	if (is_frame_rendered == true)
	{
		clusters = &frame;
	}
	else
	{
		clusters = &done_clusters;
	}

	auto cl = clusters->Get_All_And_Erase();
	m_baseline->sort_clusters_toa(SortType::bigFirst, cl);
	clusters->Insert(cl);
	size_t number = clusters->Size();
	QString message = "Sorted  clusters.";
	message.insert(7, QString::number(number));
	show_popup("Sorting sucessful", message, QMessageBox::Information);
	return;
}

void main_worker::sortClustersOld()
{
	if (meas_running)
	{
		show_popup("Sorting unsucessful", "Stop online measurement before sorting clusters.", QMessageBox::Warning);
		return;
	}

	MTVector<ClusterType>* clusters;
	if (is_frame_rendered == true)
	{
		clusters = &frame;
	}
	else
	{
		clusters = &done_clusters;
	}

	auto cl = clusters->Get_All_And_Erase();
	m_baseline->sort_clusters_toa(SortType::smallFirst, cl);
	clusters->Insert(cl);
	size_t number = clusters->Size();
	QString message = "Sorted  clusters.";
	message.insert(7, QString::number(number));
	show_popup("Sorting sucessful", message, QMessageBox::Information);
	return;
}

void main_worker::clearClusters()
{
	size_t number = done_clusters.Size();
	done_clusters.ClearAndFit();
	QString message = "Cleared  clusters.";
	message.insert(8, QString::number(number));
	emit show_popup("Clear Successful", message, QMessageBox::Information);

	std::lock_guard<std::mutex> lock(paint_lock);
	auto empty = done_clusters.Get_All();
	postprocesing::plot_whole_image(picture, painter, empty);
	emit render_all(picture);
	emit render_one(picture, 0);
}

void main_worker::toggle_cluster_frame()
{
	if (is_frame_rendered == false)
	{
		is_frame_rendered = true;
		frame.ClearAndFit();

		int added_pixels_to_frame = 0;

		auto clusters = done_clusters.Get_All_And_Erase();
		m_baseline->sort_clusters_toa(SortType::smallFirst, clusters);
		done_clusters.Insert(clusters);

		auto this_toa = done_clusters[idx].minToA;
		for (size_t i = idx; i < done_clusters.Size(); i++)
		{
			frame.Push_Back(done_clusters[i]);
			added_pixels_to_frame += done_clusters[i].pix.size();

			if (added_pixels_to_frame > 10000) break;
		}

		if (added_pixels_to_frame < 10000)
		{
			for (size_t i = ((idx-1) < 0) ? 0 : (idx - 1); i > 0; i--)
			{
				frame.Push_Back(done_clusters[i]);
				added_pixels_to_frame += done_clusters[i].pix.size();

				if (added_pixels_to_frame > 10000) break;
			}
		}

		auto temp = frame.Get_All();
		std::lock_guard<std::mutex> lock(paint_lock);
		postprocesing::plot_whole_image(picture, painter, temp);
		emit render_all(picture);
	}
	else
	{
		frame.ClearAndFit();

		// Rerender the doneClusters
		is_frame_rendered = false;
		auto temp = done_clusters.Get_All();
		postprocesing::plot_whole_image(picture, painter, temp);
		temp.clear();
		temp.shrink_to_fit();

		show_cluster(idx);
	}
}

bool main_worker::show_cluster(int index)
{
	MTVector<ClusterType>* doneCl;
	if (is_frame_rendered == true)
	{
		doneCl = &frame;
	}
	else
	{
		doneCl = &done_clusters;
	}

	if (doneCl->Size() == 0) return false;
	if (index > doneCl->Size() - 1) return false;    // If index is out of bounds, return fail

	// Lock painter
	std::lock_guard<std::mutex> lock(paint_lock);

	/* Prepare Canvas */
	QPixmap m_pix = QPixmap(256, 256);
	m_pix.fill(Qt::black);
	QPainter m_painter;
	m_painter.begin(&m_pix);
	
	/* Prepare tools */
	QColor color;
	QPen pen(color);
	m_painter.setPen(pen);
	QPoint drawPt;

	int maxX = -1;
	int minX = -1;
	int maxY = -1;
	int minY = -1;
	int longestSide = 0;
	int totalToT = 0;

	double minToA = (*doneCl)[index].pix[0].ToA;
	double maxToA = (*doneCl)[index].pix[0].ToA;;

	for (auto& pixs : (*doneCl)[index].pix) {  // Cycle through Pixels of Cluster
		totalToT += pixs.ToT;
		drawPt.setX(pixs.x);
		drawPt.setY(pixs.y);
		color = postprocesing::get_color(pixs.ToT);
		pen.setColor(color);
		m_painter.setPen(pen);
		m_painter.drawPoint(drawPt);

		if (pixs.ToA > maxToA) maxToA = pixs.ToA;
		if (pixs.ToA < minToA) minToA = pixs.ToA;

		if ((pixs.x > maxX) || (maxX == -1)) maxX = pixs.x;
		if ((pixs.x < minX) || (minX == -1)) minX = pixs.x;

		if ((pixs.y > maxY) || (maxY == -1)) maxY = pixs.y;
		if ((pixs.y < minY) || (minY == -1)) minY = pixs.y;
	}

	if (maxY - minY >= maxX - minX)
	{
		longestSide = maxY - minY + 11;
	}
	else
	{
		longestSide = maxX - minX + 11;
	}

	m_painter.end();

	/* Crop only the cluster */
	// Origin (0,0) is TOP-LEFT corner
	QRect rect(minX - 5, minY - 5, longestSide, longestSide);
	QPixmap cut = m_pix.copy(rect);

	QPixmap locator(picture);
	painter.begin(&locator);
	color.setRgb(255, 255, 255);
	pen.setColor(color);
	pen.setWidth(2);
	pen.setJoinStyle(Qt::MiterJoin);
	painter.setPen(pen);
	painter.drawRect(rect);
	painter.end();

	emit render_one(cut, longestSide);
	emit render_all(locator);
	emit cluster_info(index, (*doneCl)[index].pix.size(), totalToT, static_cast<int>(maxToA - minToA), static_cast<uint64_t>(minToA));
	return true;
}

double main_worker::energy_calc(const int& x, const int& y, const int& ToT)
{
	double temp_energy = 0;
	double a, b, c, t;
	size_t calib_matrix_offset = static_cast<size_t>(x) + (static_cast<size_t>(y) * 256);

	a = cal_a[calib_matrix_offset];
	b = cal_b[calib_matrix_offset];
	c = cal_c[calib_matrix_offset];
	t = cal_t[calib_matrix_offset];

	double tmp = std::pow(b + (t * a) - (double)ToT, 2) + (4 * a * c);

	if (tmp > 0)
	{
		tmp = std::sqrt(tmp);
		temp_energy = ((t * a) + (double)ToT - b + tmp) / (2.0 * a);
	}
	else
		temp_energy = 0;

	return temp_energy;
}

/*
*	------------------------------------------------------------------------------------------- 
*	---------------------------------ONLINE CLUSTERING-----------------------------------------
*	-------------------------------------------------------------------------------------------
*/

void main_worker::sendParamsOnline()
{
	if (online_running == false)
	{
		emit show_popup("Send parameters online failed!", "Connect to an online device before sending parameters.", QMessageBox::Warning);
		return;
	}

	// Only send parameters when in "idle" -> to prevent race condition in device (protected by sw also)
	if (mode != plugins::idle)
	{
		emit show_popup("Send parameters online failed!", "First, set mode to IDLE before sending parameters online!", QMessageBox::Warning);
		return;
	}

	ClusteringParamsOnline params{};
	params.filterBiggerClusters = filterBiggerClusters;
	params.clusterFilterSize = clusterFilterSize;
	params.maxClusterDelay = static_cast<int64_t>(maxClusterDelay);
	params.maxClusterSpan = static_cast<int64_t>(maxClusterSpan);
	params.outerFilterSize = filterSize;

	// Serialize parameters
	std::string request = serializer::serialize_params(params);
	serializer::attach_header(request, dataframe_types::config);

	// Enable flag to send parameters when online loop is ready to
	online_requests.Emplace(request);
}

void main_worker::toggle_online_clustering(int32_t port) 
{
	// If already running, stop the thread - toggle
	if (online_running == true || connecting == true)
	{
		connecting = false;
		online_running = false;
		net->abort = true;
		if (t_online.joinable()) t_online.join();
		if (t_online_stats.joinable()) t_online_stats.join();
		return;
	}

	// Join threads if they are joinable - so they are properly ended
	if (t_online.joinable()) t_online.join();
	if (t_online_stats.joinable()) t_online_stats.join();

	// Start a new thread for clusterings
	auto call = [=]() { online_clustering_loop(port);  };
	t_online = std::thread(call);

	connecting = true;
}

void main_worker::online_clustering_loop(int32_t port)
{
	emit server_status_now(plugin_status::loading);
	net = new networking();	// Blocking before device connects
	net->connect(port);

	// Still not connected - error
	if (net->isConnected == false)
	{
		emit server_status_now(plugin_status::loading_error);
		online_running = false;
		return;
	}

	// Initialize before loop
	int status = 0;
	online_running = true;
	connecting = false;

	// Start new thread for online statistics
	auto stats = [=]() { statistics_thread(); };
	t_online_stats = std::thread(stats);

	std::string incoming = "";
	almost_done_clusters.clear();
	almost_done_clusters.shrink_to_fit();
	done_clusters.Erase(done_clusters.begin(), done_clusters.end());
	{
		// Lock painter
		std::lock_guard<std::mutex> lock(paint_lock);
		picture = QPixmap(256, 256);
		picture.fill(Qt::black);
		emit render_all(picture);
	}
	emit server_status_now(plugin_status::ready);
	emit server_mode_now(plugins::idle);
	while (online_running)
	{
		// receive, send, display, do shit..
		status = net->recvDataPacket(incoming);
		status |= handle_incoming_data(incoming);
		incoming = "";
		handle_mode();
		handle_other_requests();

		// Render results according to mode
		if (mode == plugins::clustering_clusters || mode == plugins::simple_receiver)
			render_pixels();
		else if (mode == plugins::pixel_counting)
			render_pixel_counts();
		else if (mode == plugins::clustering_energies)
			render_histo_only();
		
		// Handle errors
		if (status < 0)
		{
			emit server_status_now(plugin_status::loading_error);
			online_running = false;
			break;
		}
	}

	// Idle the device and delete socket connection
	net->sendData(command_idle.c_str(), command_idle.size());
	net->close();
	
	// Lock painter
	std::lock_guard<std::mutex> lock(paint_lock);

	// Display rest of data
	done_clusters.Insert(almost_done_clusters);

	// clean and release memory from online containers
	cleanup_online_data();
	
	auto cl = done_clusters.Get_All();
	postprocesing::plot_whole_image(picture, painter, cl);
	emit render_all(picture);
}

void main_worker::handle_mode()
{
	static plugins last_mode = plugins::idle;

	// change running mode in katherine
	if (last_mode != mode)
	{
		// Save memory when not needed for pixel counting
		if (last_mode == plugins::pixel_counting)
		{
			delete pixel_counts_matrix;
		}

		{
			// Lock painter
			std::lock_guard<std::mutex> lock(paint_lock);
			// Reset image - we are changing more
			picture = QPixmap(256, 256);
			picture.fill(Qt::black);
			emit render_all(picture);
		}

		// Update calibReady parameter
		params.calibReady = calibs_loaded && calib_enabled;

		// clean and release memory from online containers
		done_clusters.Erase(done_clusters.begin(), done_clusters.end());
		cleanup_online_data();

		// Set new mode in katherine
		last_mode = mode;
		std::string command = "";
		switch (mode)
		{
		case plugins::idle:
			command = command_idle;
			serializer::attach_header(command, dataframe_types::command);
			net->sendData(command.c_str(), command.size());
			break;
		case plugins::clustering_clusters:
			command = command_clustering;
			serializer::attach_header(command, dataframe_types::command);
			net->sendData(command.c_str(), command.size());
			break;
		case plugins::simple_receiver:
			command = command_simple_recv;
			serializer::attach_header(command, dataframe_types::command);
			net->sendData(command.c_str(), command.size());
			break;
		case plugins::clustering_energies:
			command = command_clustering_energy;
			serializer::attach_header(command, dataframe_types::command);
			net->sendData(command.c_str(), command.size());
			break;
		case plugins::pixel_counting:
			command = command_pixel_counting;
			serializer::attach_header(command, dataframe_types::command);
			net->sendData(command.c_str(), command.size());
			pixel_counts_matrix = new PixelCounts();
			break;
		default:
			return;
			break;
		}
	}

	return;
}

int main_worker::handle_incoming_data(std::string& input)
{
	// return if no data came
	if (input == "") return 0;

	// Decide which type of message we have: type is always 0. element
	dataframe_types type = serializer::get_type(input);

	switch (type)
	{
	case dataframe_types::clusters:
		if (mode != plugins::clustering_clusters) return 0;	// Sanity check
		serializer::deattach_header(input);
		return process_dataframe(input);
		break;
	case dataframe_types::pixels:
		if (mode != plugins::simple_receiver) return 0;		// Sanity check
		serializer::deattach_header(input);
		return process_dataframe(input);
		break;
	case dataframe_types::energies:
		if (mode != plugins::clustering_energies) return 0;	// Sanity check
		serializer::deattach_header(input);
		return process_dataframe(input);
		break;
	case dataframe_types::pixel_counts:
		if (mode != plugins::pixel_counting) return 0;	// Sanity check
		serializer::deattach_header(input);
		return process_dataframe(input);
		break;
	case dataframe_types::messages:
		serializer::deattach_header(input);
		process_message(input);
		input.insert(0, "Server: ");
		emit server_log_now(input);
		break;
	case dataframe_types::errors:
		serializer::deattach_header(input);
		input.insert(0, "ERROR: ");
		emit server_log_now(input);
		break;
	case dataframe_types::command:
		emit server_log_now("DEV WARNING: Client cannot send commands, fix it!");
		break;
	case dataframe_types::config:
	{
		serializer::deattach_header(input);
		ClusteringParamsOnline setParams = serializer::deserialize_params(input);

		std::string log = "Online params acknowledge:\n";
		log.append("maxDelay = ");
		log.append(std::to_string(setParams.maxClusterDelay));
		log.append(" ns\n");
		log.append("maxSpan = ");
		log.append(std::to_string(setParams.maxClusterSpan));
		log.append(" ns\n");
		log.append("filterSize = ");
		log.append(std::to_string(setParams.outerFilterSize));
		log.append(" pixels\n");
		log.append("clusterFilterSize = ");
		log.append(std::to_string(setParams.clusterFilterSize));
		log.append(" pixels\n");
		log.append("filterBiggerClusters = ");
		log.append(setParams.filterBiggerClusters ? "true" : "false");
		log.append("\n");
		
		emit server_log_now(log);
		break;
	}
	case dataframe_types::acknowledge:
		serializer::deattach_header(input);
		emit server_mode_now(get_command_ack(input));	// Display machine running mode
		break;
	default:
		serializer::deattach_header(input);
		input.insert(0, "DEV CHECK IMPLEMENTATION: ");
		emit server_log_now(input);
		break;
	}

	return 0;
}

int main_worker::process_dataframe(const std::string& input)
{
	switch (mode)
	{
	case plugins::idle:
		// Do nothing
		break;

	case plugins::clustering_clusters:
		online_clusters = serializer::deserialize_clusters(input);
		if (online_clusters.empty()) return 0;	// In case input wasnt cluster frame

		// Count clusters for stats
		cluster_counter += online_clusters.size();

		// Emplace all clusters into doneClusters
		for (auto& cluster : online_clusters)
		{
			// Calibrate pixels if calibReady - device cannot calibrate
			if (params.calibReady)
			{
				for (auto& pix : cluster.pix)
				{
					pix.ToT = energy_calc(pix.x, pix.y, pix.ToT);
				}
			}
			
			almost_done_clusters.emplace_back(ClusterType{ cluster.pix, 0,0,0,0,0,0 });
			pixel_counter += cluster.pix.size();
		}
		break;

	case plugins::simple_receiver:
		online_pixels = serializer::deserialize_pixels(input);
		if (online_pixels.empty()) return 0;	// In case input wasnt pixel frame

		// Count pixels for hitrate statistics
		pixel_counter += online_pixels.size();

		// if empty, insert the first cluster
		if (almost_done_clusters.empty())
		{
			// Calibrate pixels before emplacing them
			if (params.calibReady)
			{
				for (auto& pix : online_pixels)
				{
					pix.ToT = energy_calc(pix.x, pix.y, pix.ToT);
				}
			}

			almost_done_clusters.emplace_back(ClusterType{ online_pixels, 0,0,0,0,0,0 });
		}
		// if not empty, then just append pixels to the ONE AND ONLY cluster
		else
		{
			// Maintain only one cluster
			for (auto& pix : online_pixels)
			{
				// Calibrate pixels befire emplacing them
				if (params.calibReady)
				{
					pix.ToT = energy_calc(pix.x, pix.y, pix.ToT);
				}
				almost_done_clusters.back().pix.emplace_back(pix);
			}
		}
		break;

	case plugins::clustering_energies:
	{
		size_t pixel_count = 0;
		online_energies = serializer::deserialize_histograms(input, pixel_count);
		if (online_energies.empty()) return 0;	// In case input wasnt online energies

		cluster_counter += online_energies.size();
		pixel_counter += pixel_count;

		// Add online energies to the done_energies vector
		almost_done_energies.insert(almost_done_energies.end(), online_energies.begin(), online_energies.end());
		online_energies.clear();
	}
		break;

	case plugins::pixel_counting:
		online_pixel_counts = serializer::deserialize_pixel_counts(input);
		if (online_pixel_counts.empty()) return 0;	// In case input wasnt pixel frame
		
		// Count pixels for hitrate statistics
		pixel_counter += online_pixel_counts.size();

		// Maintain only one cluster
		for (auto& pix : online_pixel_counts)
		{
			pixel_counts_matrix->counts[pix.x][pix.y] += 1;
			if (pixel_counts_matrix->counts[pix.x][pix.y] > pixel_counts_matrix->maxCount)	pixel_counts_matrix->maxCount = pixel_counts_matrix->counts[pix.x][pix.y];
			if (pixel_counts_matrix->counts[pix.x][pix.y] < pixel_counts_matrix->minCount)	pixel_counts_matrix->minCount = pixel_counts_matrix->counts[pix.x][pix.y];
		}
		break;
	default:
		return -1;	 // error
		break;
	}

	return 0;
}

void main_worker::process_message(const std::string& input)
{
	if (input == "MEAS STARTED")
	{
		meas_running = true;
		pixel_counter = 0;
		cluster_counter = 0;

		// Reset pixel counts matrix - only in pixel counting mode, otherwise deleted field
		if (mode == plugins::pixel_counting)
		{
			pixel_counts_matrix->Clear();
		}

		if (mode == plugins::clustering_energies)
		{
			done_energies.clear();
			done_energies.shrink_to_fit();
			emit show_histogram_online(postprocesing::convert_energy_to_histogram(done_energies), true);
		}

		// Lock painter
		std::lock_guard<std::mutex> lock(paint_lock);
		std::vector<ClusterType> empty;	// Just empty vector to paint black background
		postprocesing::plot_whole_image(picture, painter, empty);
	}
	else if (input == "MEAS FINISHED")
	{
		// Lock painter
		std::lock_guard<std::mutex> lock(paint_lock);

		// Display rest of data
		if (mode == plugins::clustering_clusters || mode == plugins::simple_receiver)
		{
			done_clusters.Insert(almost_done_clusters);
			almost_done_clusters.clear();
			almost_done_clusters.shrink_to_fit();

			auto cl = done_clusters.Get_All();
			postprocesing::plot_whole_image(picture, painter, cl);
			emit render_all(picture);
		}
		else if (mode == plugins::pixel_counting)
		{
			postprocesing::plot_whole_image(picture, painter, pixel_counts_matrix);
			emit render_all(picture);
		}
		else if (mode == plugins::clustering_energies)
		{
			emit show_histogram_online(postprocesing::convert_energy_to_histogram(almost_done_energies), false);  // Let the GUI thread handle the data
			// Emplace almost done energies
			done_energies.insert(done_energies.end(), almost_done_energies.begin(), almost_done_energies.end());
			almost_done_energies.clear();
			almost_done_energies.shrink_to_fit();
		}
		
		meas_running = false;
		emit show_popup("Online measurement finished!", "Online device finished measuring. You may process and save your data now.", QMessageBox::Information);
	}

	return;
}

void main_worker::handle_other_requests()
{
	if (online_requests.Empty() == true) return;

	std::string request;
	while (online_requests.Empty() == false)
	{
		// pop request
		request = online_requests.Pop();

		// Send request only if it containts something
		if (request != "" || request.size() > 0) net->sendData(request.c_str(), request.size());
	}
}

void main_worker::enable_reset_screen(bool enabled)
{
	reset_picture_enable = enabled;
}

void main_worker::update_reset_period(int period)
{
	if (period < 200)	period = 200;
	reset_period = period;
}

void main_worker::save_done_clusters()
{
	auto clusters = done_clusters.Get_All();

	if (mode == plugins::simple_receiver)
	{
		file_saver::savePixelFile(clusters);
	}
	else
	{
		file_saver::savePixelFile(clusters);
		std::string savedFile = file_saver::saveClusterFile(clusters);
	}
	doneSaving = true;
	return;
}

void main_worker::select_online_mode(plugins plug)
{
	if (online_running)	mode = plug;
	else mode = plugins::idle;
}

void main_worker::render_pixels()
{
	// Clock for rendering timer
	static std::chrono::time_point<std::chrono::steady_clock> last_render = std::chrono::steady_clock::now();
	static std::chrono::time_point<std::chrono::steady_clock> last_reset = std::chrono::steady_clock::now();

	// Rendering timer
	if ((std::chrono::steady_clock::now() - last_render) > std::chrono::milliseconds(200) && meas_running)
	{
		// Once every 200 ms or specified reset period
		last_render = std::chrono::steady_clock::now();

		// Lock painter
		std::lock_guard<std::mutex> lock(paint_lock);

		// Reset image if its enabled
		if (((std::chrono::steady_clock::now() - last_reset) > std::chrono::milliseconds(reset_period)) && reset_picture_enable)
		{
			last_reset = last_render;
			postprocesing::plot_whole_image(picture, painter, almost_done_clusters);
		}
		else
		{
			if (almost_done_clusters.size() > 0) postprocesing::plot_more_image(picture, painter, almost_done_clusters);
		}

		// finally insert almost done clusters to done clusters
		done_clusters.Insert(almost_done_clusters);
		almost_done_clusters.clear();

		emit render_all(picture);
	}
}

void main_worker::render_pixel_counts()
{
	// Clock for rendering timer
	static std::chrono::time_point<std::chrono::steady_clock> last_render = std::chrono::steady_clock::now();

	// Rendering timer
	if ((std::chrono::steady_clock::now() - last_render) > std::chrono::milliseconds(1000) && meas_running)
	{	
		// Once every 1000 ms
		last_render = std::chrono::steady_clock::now();
		
		// Lock painter
		std::lock_guard<std::mutex> lock(paint_lock);

		// Plot whole image -> there is no option to plot only one part of image, because dynamic range of all pixels has to be recalculated
		postprocesing::plot_whole_image(picture, painter, pixel_counts_matrix);
		emit render_all(picture);
	}
}

void main_worker::render_histo_only()
{
	// Clock for rendering timer
	static std::chrono::time_point<std::chrono::steady_clock> last_render = std::chrono::steady_clock::now();

	// Rendering timer
	if ((std::chrono::steady_clock::now() - last_render) > std::chrono::milliseconds(1000) && meas_running)
	{
		// Once every 1000 ms
		last_render = std::chrono::steady_clock::now();

		// Little computationaly expensive on UI thread option -> chart creating is done solely in UI thread
		emit show_histogram_online(postprocesing::convert_energy_to_histogram(almost_done_energies), false);  // Let the GUI thread handle the data
		
		// Emplace almost done energies
		done_energies.insert(done_energies.end(), almost_done_energies.begin(), almost_done_energies.end());
		almost_done_energies.clear();
	}

}

void main_worker::render_progress()
{
	// Clock for rendering timer
	static std::chrono::time_point<std::chrono::steady_clock> last_render = std::chrono::steady_clock::now();
	static int prog = 0;

	// Rendering timer
	if ((std::chrono::steady_clock::now() - last_render) > std::chrono::milliseconds(100) && meas_running)
	{
		// Once every 1000 ms
		last_render = std::chrono::steady_clock::now();

		if (prog == 100) prog = 0;
		emit update_progress(prog++);
	}

	if (meas_running == false)
	{
		emit update_progress(0);
	}
}

void main_worker::statistics_thread()
{
	size_t last_cluster_count = 0;
	size_t last_pixel_count = 0;
	size_t max_pps = 0;
	size_t max_clps = 0;
	bool last_meas_running = false;
	int i = 0;

	while (online_running)
	{
		// Maintain responsivity if online_running is canceled
		if (i >= 10)
		{
			// Reset counts after start of new measurement
			if (meas_running == true && last_meas_running != meas_running)
			{
				max_pps = 0;
				max_clps = 0;
				last_cluster_count = 0;
				last_pixel_count = 0;
			}
			last_meas_running = meas_running;

			i = 0;
			auto clps = cluster_counter - last_cluster_count;
			auto pps = pixel_counter - last_pixel_count;

			// Clip to high number in case of error calculation (bad synchro between threads)
			if (clps > max_clps && clps < 100000000) max_clps = clps;
			if (pps > max_pps && pps < 100000000) max_pps = pps;

			emit clusters_per_second(clps);
			emit max_clusters_per_second(max_clps);
			emit hitrate(pps);
			emit max_hitrate(max_pps);
			emit clusters_received(cluster_counter);
			emit pixels_received(pixel_counter);

			if (last_cluster_count != cluster_counter) last_cluster_count = cluster_counter;
			if (last_pixel_count != pixel_counter) last_pixel_count = pixel_counter;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		i++;
	}

	emit clusters_per_second(0);
	emit hitrate(0);
	emit clusters_received(last_cluster_count);
	emit pixels_received(last_pixel_count);
}