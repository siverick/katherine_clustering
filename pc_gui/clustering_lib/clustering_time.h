
/**
 * @clustering_time.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once
#include "clusering_base.h"
#include <queue>
#include <thread>

class clustering_time_parallelisation : public clustering_base, public cluster_definition
{
	struct thread_data
	{
		uint64_t firstToA;		// Which clusters to keep for merge
		uint16_t thread_num;	// For evaluating with which thread we have to merge
	};

public:
	clustering_time_parallelisation(volatile bool& abrt) : abort(abrt) {};

	void do_clustering(std::string& lines, const ClusteringParams& new_params, volatile bool& abort);

private:
	void ProcessDataQueue(uint16_t thread_num, std::string& thread_lines, const ClusteringParams params);
	void ProcessPixel(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters);
	void ProcessPixel(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters, std::queue<OnePixel>& clusters_for_merge, const thread_data& thread_first_toa);
	void ProcessPixelAlgorithm(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters);
	void SendToMergeStation(std::queue<OnePixel>& open_clusters_front, Clusters& open_clusters_back, uint16_t thread_num);
	void MergeMiddleClusters(std::queue<OnePixel>& pixelData, Clusters& openClusters);
	void AfterRunClstrCheck();
	std::vector<Clusters> clustersToMerge;					// Clusters from end to merge
	std::vector<std::queue<OnePixel>> pixelsToMerge;		// Pixels from begin to merge
	uint16_t numOfThreads = 1;
	std::mutex mtx;
	ClusteringParams params;
	volatile bool& abort;

	void test_saved_clusters()
	{
		for (const auto& clstr : doneClusters) {
			for (const auto& pixs : clstr.pix) {
				stat_lines_saved++;
				assert("Limits do not match included pixels" && (pixs.x > clstr.xMax || pixs.x < clstr.xMin || pixs.y > clstr.yMax || pixs.y < clstr.yMin) == false);
			}
		}
	}

	/// <summary>
	/// Extract number of hits from the input file - its in the couple of last characters of file
	/// </summary>
	size_t GetNumOfHits(std::string& lines)
	{
		size_t hits = -1;

		size_t lines_size = lines.size();	// for extracting last 300 chars from string
		if (lines_size < 300) return -1;	// error, file is too short
		else
			lines_size -= 300;				// go to pos -300 from end to get last 300 chars

		std::string str = lines.substr(lines_size);	// get last 300 chars from string
		std::string keyword = "Hits:";				// keyword to search for
		size_t pos = str.find(keyword); // find the position of "Hits:" in the string
		if (pos != std::string::npos) { // if "Hits:" is found in the string
			pos += keyword.length(); // move the position to the end of "Hits:"
			std::string hits_str = str.substr(pos); // extract the substring starting from the end of "Hits:"
			size_t comma_pos = hits_str.find(","); // find the position of the comma
			pos = comma_pos;					   // save pos for if (comma_pos - pos > 3)

			size_t num_of_threedigits = 1;
			while (comma_pos != std::string::npos)
			{ // if the comma is found in the substring
				if (comma_pos - pos > 3) break;	// dont continue of it skipped more than 3 digits
				num_of_threedigits++;
				hits_str.erase(comma_pos, 1); // remove the comma from the substring
				pos = comma_pos;				// save old pos for if (comma_pos - pos > 3)
				comma_pos = hits_str.find(","); // find the position of the comma
			}
			num_of_threedigits = (num_of_threedigits * 3) + 2;	// *3 for calculating the offset, add 2 in case the first threedigits is one number only
			hits_str = hits_str.substr(0, num_of_threedigits);	// take only the nuber itself and cut the rest

			hits = stoi(hits_str); // convert the substring to an integer
			//log_append(utility::print_time_info("Num of hits:", "Hts", hits));
		}

		return hits;
	}

	/// <summary>
	/// Finds end of line in a given file provided by str_it
	/// </summary>
	void FindEndOfLine(size_t& read_until, std::string::iterator& str_it)
	{
		int num_loop = 0;
		while (true)	// now we jumped at some point. we find end of the line
		{
			if (*str_it == '\n')	// end of the line for both /r/n and /n
			{
				read_until++;
				break;
			}
			else
			{
				read_until++;
				str_it++;
			}

			// return from dead loop - error
			num_loop++;
			if (num_loop > 1000)
				return;
		}
	}

	/// <summary>
	/// Separates the original file content "lines" to equal portions in number of threads "numOfThreads".
	/// </summary>
	void SeparateFileST(std::string& lines, std::vector<std::string>& inputs_for_threads)
	{
		const int sizeStep = lines.size() / numOfThreads;

		for (int i = 0; i < numOfThreads; i++)
		{
			std::string substr;
			if (i < numOfThreads - 1)
			{
				// Jump the sizeStep with iterator
				auto str_it = lines.begin() + sizeStep;
				size_t read_until = sizeStep;
				FindEndOfLine(read_until, str_it);

				// emplace chunk of file to vector and delete it from input
				substr = lines.substr(0, read_until);
				inputs_for_threads.emplace_back(substr);
				lines.erase(lines.begin(), lines.begin() + read_until);
				lines.shrink_to_fit();
			}
			else // last substr should be rest of the file
			{
				// emplace rest of the file to vector
				substr = lines;
				inputs_for_threads.emplace_back(substr);
				lines.clear();
				lines.shrink_to_fit();
			}
		}
	}

	bool m_get_my_line_MT(std::string& str, std::string& oneLine, bool& rn_delim, size_t& last_pos, size_t& pos)
	{
		pos = str.find('\n', pos + 1);

		if (rn_delim) oneLine = str.substr(last_pos, pos - last_pos - 1);
		else oneLine = str.substr(last_pos, pos - last_pos);

		last_pos = pos + 1;

		if (pos != std::string::npos) return true;
		else return false;
	}

};

