/**
 * @clustering_base.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#include "utility.h"
#include "cluster_definition.h"
#include <cmath>

class clustering_base : public utility
{
public:
	/* Calibration settings */
	void set_calib_a(double* calib);
	void set_calib_b(double* calib);
	void set_calib_c(double* calib);
	void set_calib_t(double* calib);
	void set_calibs(double* a, double* b, double* c, double* t);

	/* Public getter for log */
	std::string log_return()
	{
		return log;
	}

	/* Some perfomance related variables */
	float stat_elapsed_clustering = 0;		// (ms)
	uint64_t stat_clusters_found = 0;		// (clusters)
	uint64_t stat_lines_sorted = 0;			// (lines) - received
	uint64_t stat_lines_processed = 0;		// (lines) - processed after filtering
	uint64_t stat_lines_saved = 0;			// (lines) - saved to clusters
	float stat_mhits = 0;					// (MHits/s)

	/* Tests: return -1 if error */
	int test_all_lines_used();

	/// <summary>
	/// Return stats in std::string format
	/// </summary>
	std::string stat_print()
	{
		std::string stat = "";
		stat.append("Time = ");
		stat.append(std::to_string(static_cast<int>(stat_elapsed_clustering)));
		stat.append(" ms  Clusters = ");
		stat.append(std::to_string(stat_clusters_found));
		stat.append("   MHits = ");
		stat.append(std::to_string(stat_mhits));

		/*if (test_all_lines_used() == -1)	// lines processed == lines saved?
			stat.append("  Lines FAULT");
		else
			stat.append("  Lines OK");*/

		stat.append("\r\n");
		return stat;
	}

protected:
	bool get_my_line_MT(const std::string& str, std::string& oneLine, const bool& rn_delim, size_t& last_pos);
	bool get_my_line(const std::string& str, std::string& oneLine, const bool& rn_delim);
	double energy_calc(const int& x, const int& y, const int& ToT);
	float perf_metric(int linesProcessed, float msElapsed);

	template <typename T>
	T sort_clusters_generic(SortType type, T doneOnes);

	double calib_a[256 * 256];
	double calib_b[256 * 256];
	double calib_c[256 * 256];
	double calib_t[256 * 256];

	/// <summary>
	/// Reset all stats - before new clustering
	/// </summary>
	void stat_reset()
	{
		stat_elapsed_clustering = 0;
		stat_clusters_found = 0;
		stat_lines_sorted = 0;
		stat_lines_processed = 0;
		stat_lines_saved = 0;
		stat_mhits = 0;
	}

	void stat_save(float timeEl_ms, uint64_t foundCl)
	{
		stat_elapsed_clustering = timeEl_ms;
		stat_clusters_found = foundCl;
		stat_mhits = static_cast<float>(stat_lines_saved) / stat_elapsed_clustering / 1000.0f;
	}

	/* Functions for saving the log data from clustering and returning it */
	std::string log;

	void log_clear()
	{
		log = "";
		log.shrink_to_fit();
	}

	void log_append(std::string input)
	{
		log.append(input);
	}

	/// <summary>
	/// Energy calculation used in creation of energy LUT.
	/// </summary>
	int32_t energy_calc_lut(const int& x, const int& y, const int& ToT)
	{
		double temp_energy = 0;
		double a, b, c, t;
		size_t calib_matrix_offset = x + (y * 256);

		a = calib_a[calib_matrix_offset];
		b = calib_b[calib_matrix_offset];
		c = calib_c[calib_matrix_offset];
		t = calib_t[calib_matrix_offset];

		double tmp = std::pow(b + (t * a) - (double)ToT, 2) + (4 * a * c);

		if (tmp > 0)
		{
			tmp = std::sqrt(tmp);
			temp_energy = ((t * a) + (double)ToT - b + tmp) / (2.0 * a);
		}
		else
			temp_energy = 0;

		return static_cast<int32_t>(temp_energy);
	}

	/*
	*  IMPORTANT PERFORMANCE COMMENT
	* - inlining getline and strtoint and strtolong improves performance
	*   by circa 4%. But in Bruteforce, it worsens performance.
	*/

	inline int32_t strtoint(const char* p) {
		int32_t x = 0;
		bool neg = false;
		if (*p == '-') {
			neg = true;
			++p;
		}
		while (*p == ' ') p++;
		while (*p >= '0' && *p <= '9') {
			x = (x * 10) + static_cast<int32_t>(*p - '0');
			++p;
		}
		if (neg) {
			x = -x;
		}
		return x;
	}

	inline int64_t strtolong(const char* p) {
		int64_t x = 0;
		bool neg = false;
		if (*p == '-') {
			neg = true;
			++p;
		}
		while (*p == ' ') p++;
		while (*p >= '0' && *p <= '9') {
			x = (x * 10) + static_cast<int64_t>(*p - '0');
			++p;
		}
		if (neg) {
			x = -x;
		}
		return x;
	}

	inline int64_t m_abs(const int64_t& a)
	{
		if (a >= 0) return a;

		return -a;
	}
};

