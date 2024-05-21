
/**
 * @clustering_time_embed.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once
#include "clusering_base.h"
#include <queue>
#include <thread>

/*
*	Possible model of parallelisation on FPGA
*/

class clustering_time_embed : public clustering_base, public cluster_definition
{
	// frame number used for determining if we should merge or not
	enum FrameNumber {
		first,
		last,
		middle
	};

	struct thread_data
	{
		uint64_t firstToA;		// Which clusters to keep for merge
		uint16_t thread_num;	// For evaluating with which thread we have to merge
	};

public:
	clustering_time_embed(volatile bool& abrt) : abort(abrt) {};

	void do_clustering(std::string& lines, const ClusteringParams& new_params, volatile bool& abort);

private:
	void ProcessDispatchStation(uint16_t thread_num, const ClusteringParams params);
	void ProcessDataFrame(uint16_t thread_num, std::queue<std::string>& thread_lines, const ClusteringParams params, FrameNumber frameNum);
	void ProcessPixelAlgorithm(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters, Clusters& open_clusters_front, const thread_data& thread_first_toa);
	void ProcessPixelMerge(OnePixel& pixel_data, Clusters& open_clusters, Clusters& thread_done_clusters);
	bool IsFrontMergeCluster(double clusterMinToa, const thread_data& thread_first_toa);
	void SendToMergeStation(std::queue<OnePixel>& open_clusters_front, Clusters& open_clusters_back, uint16_t thread_num, FrameNumber frameNum);
	void MergeMiddleClusters(std::queue<std::queue<OnePixel>>& pixelData, std::queue<Clusters>& openClusters);
	void MergeTwoFrames(std::queue<OnePixel>& pixelData, Clusters& openClusters);
	void AfterRunClstrCheck();
	std::vector<std::queue<Clusters>> clustersToMerge;					// Clusters from end to merge
	std::vector<std::queue<std::queue<OnePixel>>> pixelsToMerge;		// Pixels from begin to merge
	uint16_t numOfThreads = 0;
	std::mutex mtx;
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
	/// Finds end of line in a given file provided by str_it
	/// </summary>
	void FindEndOfLine(size_t& read_until, std::string::iterator& str_it)
	{
		unsigned int num_loop = 0;
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

			num_loop++;
			if (num_loop > 1000) // return from dead loop - error
				return;
		}
	}

	std::queue<std::string> dataFromDetector;
	std::vector<std::queue<std::queue<std::string>>> continualThreadInputs;
	std::vector<bool> clustering_threads_finished;
	volatile std::atomic<bool> signal_run = false;
	volatile std::atomic<bool> signal_clusterers_finished = false;
	volatile std::atomic<bool> signal_separator_finished = false;
	volatile std::atomic<bool> signal_end_frame_sent = false;
	std::mutex parsingMtx;
	std::mutex sendingMtx;

	void PassLineToSeparator(std::string& lines)
	{
		std::string oneLine;
		while (get_my_line(lines, oneLine, params.rn_delim))
		{
			std::lock_guard<std::mutex> lock(parsingMtx); // TODO: Otestuju jestli to tahle jde out of scope
			dataFromDetector.emplace(oneLine);
			if (abort) return;
		}

		parsingMtx.lock();
		signal_run = false;			// File end, end run
		parsingMtx.unlock();
	}

	template<typename T>
	void ClearQu(std::queue<T>& q)
	{
		std::queue<T> empty;
		std::swap(q, empty);
	}

#define BUFFER_SIZE_FACTOR 1000	// How much we put in buffer before dispatching - min 4
#define FINAL_FRAME_SIZE 200	// Cut off at the and of measurement
#define MINIMAL_FRAME_SIZE 10	// min number of hits frame can have - also influences how frequently we check the ToA
#define MAX_FRAME_SIZE 1000000	// 10kHits equal to 350 kBytes of memory!

	/* Max frame size should be calculated on the fly. Based on actual MHIts/s and numbe of threads */
	/* If we have 1 MHit/s and 10 threads, one frame should be max 100kHits */

		// Moves all data from queue buffer to queue continualThreadInputs
	void MoveBufferToInputs(std::queue<std::string>& bufferForThreads, uint16_t t_num)
	{
		// Dispatch gathered data to thread inputs
		std::unique_lock<std::mutex> sendingLock(sendingMtx);	// Unique lock makes locking and unlocking in context possible

		// Emplace whole buffer to inputs - its making a queue of frames
		continualThreadInputs[t_num].emplace((bufferForThreads));	// emplace to the queue
		return;
	}

	/// <summary>
	/// Send the prepared frame to corresponding thread. Cycle through the threads.
	/// </summary>
	bool SendFrameToThread(std::queue<std::string>& bufferData, uint16_t& t_num)
	{
		MoveBufferToInputs(bufferData, t_num);	// Dispatch data from buffer to threads input
		ClearQu(bufferData);						// clear contents of buffer

		t_num++;	// Continue sending data to next thread
		if (t_num > (numOfThreads - 1)) t_num = 0;	// Cycle through threads

		return true;
	}

	// Parses data for treads like: data for 1. then 2. ... then last. And again. 
	// Data is always sent in a size of Frame - derived from max delay of cluster and SIZE_FACTOR
	void SeparateFileIntoThreadsCont()
	{
		uint32_t timeoutCnt = 0;
		uint32_t lineCounter = 0;
		uint32_t lineStamp = MINIMAL_FRAME_SIZE;
		uint32_t lineTimeoutCnt = 0;
		uint32_t framesSent = 0;
		std::queue<std::string> bufferForThreads;
		uint16_t t_num = 0;
		bool threadDone = false;

		// TEST vars
		uint64_t hits_processed = 0;

		// This will determine size of buffer for threads before dispatching it to threads
		//double frameSize = static_cast<double>(params.maxClusterDelay) * static_cast<double>(BUFFER_SIZE_FACTOR);	// data length for one thread
		double frameFirstToa = 0;	// Toa of first frame
		OnePixel tempPixel{ 0,0,0,0 };

		/* Frame sizes - various methods */
		// 1. Derived from NumOfThreads with assumption that every thread gets new info atleast 1x a second
		// One second in nanoseconds is divided by numOfThreads - gets us length for each frame in nanoseconds
		double frameSize = 1000000000.0 / static_cast<double>(numOfThreads);

		/* While for whole operation */
		while (!abort)
		{
			/* When almost finished, break loop to enter final frame creation */
			if (signal_run == false && (dataFromDetector.size() < 200))
				break;

			// Start of new frame
			if (!dataFromDetector.empty())
			{
				bool beginFound = false;
				std::unique_lock<std::mutex> parsingLock(parsingMtx);	// Unique lock makes locking and unlocking in context possible
				/* Run through the dataFromDetector until we finally found first "pixelData" */
				/* Note: This is only mandatory in begin of measurement, when lines begining with # are coming */
				while (!ProcessLine(dataFromDetector.front(), tempPixel))
				{
					if (abort)	return;

					dataFromDetector.pop();
					if (dataFromDetector.empty()) /* If empty, unlock the lock to let the other thread send data and wait */
					{
						parsingLock.unlock();
						std::this_thread::sleep_for(std::chrono::microseconds(500));
						parsingLock.lock();
						continue;
					}
				}

				/* emplace the first pixelData line to bufferForThreads and save firstToa */
				bufferForThreads.emplace(dataFromDetector.front());
				dataFromDetector.pop();
				parsingLock.unlock();	// unlock as fast as possible
				frameFirstToa = tempPixel.ToA;	// save first toa of frame
				hits_processed++;
			}
			else     // Wait before new data comes in
			{
				std::this_thread::sleep_for(std::chrono::microseconds(500));
				continue;
			}

			// Reset while loop variales
			threadDone = false;
			lineStamp = MINIMAL_FRAME_SIZE;
			lineCounter = 0;
			lineTimeoutCnt = 0;

			/* While for one Frame */
			while (!threadDone)
			{
				if (!dataFromDetector.empty()) {
					// Put data from detenctor into a buffer
					std::unique_lock<std::mutex> parsingLock(parsingMtx);	// Unique lock makes locking and unlocking in context possible
					bufferForThreads.emplace(dataFromDetector.front());
					dataFromDetector.pop();		// Pop from pipe
					parsingLock.unlock();

					lineCounter++;
					lineTimeoutCnt = 0;
					hits_processed++;

					if (lineCounter > lineStamp)	// Once every N (MINIMAL_FRAME_SIZE) lines check ToA of the line
					{
						lineStamp += MINIMAL_FRAME_SIZE;
						ProcessLine(bufferForThreads.back(), tempPixel);	// Process line to see ToA value of line
						double actualFrameTime = (tempPixel.ToA - frameFirstToa);

						if ((actualFrameTime > frameSize)		// is ToA value of line distant from first toa enough
							|| (actualFrameTime > (params.maxClusterDelay * 4) && (lineCounter > MAX_FRAME_SIZE))) /* Send frame early -> In case frame is too long in buffer */
						{
						    threadDone = SendFrameToThread(bufferForThreads, t_num);
							framesSent++;
						}
						/* In case buffer is full and frame is shorter than 4 x maxClusterDelay, throw away some data and gather more */
						else if (actualFrameTime < (params.maxClusterDelay * 4) && (lineCounter > MAX_FRAME_SIZE))
						{
							for (int i = 0; i < (lineCounter / 2); i++) bufferForThreads.pop();		// Throw away first half of the buffer
							lineCounter = bufferForThreads.size();
							continue;
						}
					}
				}
				else if (signal_run == false)	// End loop if run == false
					break;
				else if (lineTimeoutCnt > 500000)
				{
					threadDone = SendFrameToThread(bufferForThreads, t_num);
					framesSent++;

					if (dataFromDetector.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				else
					lineTimeoutCnt++;
			}
		}

		assert(dataFromDetector.size() < MAX_FRAME_SIZE && "Final frame size is too big - error");

		while (!dataFromDetector.empty())	/* Parse the rest of the data to buffer */
		{
			if (abort) return;

			std::lock_guard<std::mutex> lock(parsingMtx);
			bufferForThreads.emplace(dataFromDetector.front());
			dataFromDetector.pop();		// Pop from pipe
			hits_processed++;
		}

		if (!bufferForThreads.empty()) bufferForThreads.front().insert(bufferForThreads.front().begin(), 'E');	// Indicate end with 'E' char!

		// Wait for all clustering threads to finish before sending last frame
		signal_separator_finished = true;
		while (signal_clusterers_finished == false)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		MoveBufferToInputs(bufferForThreads, t_num);	// Dispatch data from buffer to threads input
		signal_end_frame_sent = true;
		ClearQu(bufferForThreads);						// Clear Queue buffer
		ClearQu(dataFromDetector);						// Clear Queue detectors

		// FOR ISOLATED DEBUGING OF THIS FUNCTION assert(hits_processed == GetNumOfHitsThreadInputs(continualThreadInputs) && "Hits in inputs do not correspond to hits_processed");
		log_append(utility::print_time_info("Processed hits by separator", "hits", hits_processed));
		// FOR ISOLATED DEBUGING OF THIS FUNCTION log_append(utility::print_time_info("Saved hits", "hits", GetNumOfHitsThreadInputs(continualThreadInputs)));
		log_append(utility::print_time_info("Frame sent:", "fr", framesSent));
	}

	bool ProcessLine(std::string oneLine, OnePixel& result)
	{
		if (oneLine[0] == '#') return false;

		// PARAMETERS
		const double toaLsb = 25;
		const double toaFineLsb = 1.5625;

		/* Split line into rows */
		char* context = nullptr;
		char* rows[4] = { 0, 0, 0, 0 };		// Temp storage for txt numerals

		/* Split line into rows */
		rows[0] = strtok_s(&oneLine.front(), "\t", &context); //strtok(oneLine.data(), "\t");
		rows[1] = strtok_s(NULL, "\t", &context);
		rows[2] = strtok_s(NULL, "\t", &context);
		rows[3] = strtok_s(NULL, "\t", &context);

		// ATOI 6300 ms vs STRTOINT 6100 ms with 20 PIX filter
		// ATOI CCA 200 ms slower than my Fast strtoint
		result.x = strtoint(rows[0]) / 256; // std::atoi(rows[0]) / 256;
		result.y = strtoint(rows[0]) % 256; // std::atoi(rows[0]) % 256;
		result.ToA = (double)((strtolong(rows[1]) * toaLsb) - (strtolong(rows[2]) * toaFineLsb));  // (ns) TimeFromBeginning = 25 * ToA - 1.5625 * fineToA
		result.ToT = strtoint(rows[3]);//std::atoi(rows[3]);

		if (params.calibReady)
			result.ToT = energy_calc(result.x, result.y, result.ToT);

		return true;
	}
};

