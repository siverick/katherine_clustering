/**
 * @serializer.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#ifndef PLUGIN_MAIN_SERIALIZER_H_
#define PLUGIN_MAIN_SERIALIZER_H_

 /*
  * 									SERIALIZER
  * Class for serializing clustered data structures into a form thats possible
  * to be transmitted over the ethernet.
  *
  * */

#include <string>
#include <vector>
#include "cluster_definition.h"

enum dataframe_types
{
	pixels,
	clusters,
	energies,
	pixel_counts,
	messages,
	errors,
	command,
	config,
	acknowledge
};


  /*
   * Prepends of the dataframes
   * 'C' - cluster frame
   * 'P' - pixel frame
   * 'H' - energy frame (histogram)
   * 'N' - pixel_counts (only coordinates)
   * 'M' - message
   * 'E' - error
   * 'K' - command (to client)
   * 'V' - config - should get acknowledge
   * 'A' - acknowledge (from client)
   */

class serializer
{
public:
	// Attach message header
	static void attach_header(std::string& input, dataframe_types type);

	// Deattach message header
	static void deattach_header(std::string& input);

	// Get type from message
	static dataframe_types get_type(const std::string& message);

	// Semicolon separated clusters
	// Comma separated pixels
	// \t separated elements of pixel
	static std::string serialize_clusters(const std::vector<CompactClusterType>& clusters);

	// Semicolon separated clusters
	// Comma separated pixels
	// \t separated elements of pixel
	static std::vector<CompactClusterType> deserialize_clusters(const std::string& input);

	// Comma ',' separated pixels
	// \t separated elements of pixel
	// Semicolon ';' after the last pixel - ignored, only as a size reference
	// Comma ',' even after the last pixel (before semicolon) - to tell last pixel
	static std::string serialize_pixels(const std::vector<OnePixel>& pixels);

	// Comma ',' separated pixels
	// \t separated elements of pixel
	// Semicolon ';' after the last pixel
	static std::vector<OnePixel> deserialize_pixels(const std::string& input);

	// Comma ',' separated pixels
	// \t separated elements of pixel
	// Semicolon ';' after the last pixel - ignored, only as a size reference
	// Comma ',' even after the last pixel (before semicolon) - to tell last pixel
	static std::string serialize_pixel_counts(const std::vector<OnePixelCount>& pixelCounts);

	// Comma ',' separated pixels
	// \t separated elements of pixel
	// Semicolon ';' after the last pixel
	static std::vector<OnePixelCount> deserialize_pixel_counts(const std::string& input);

	// TODO: Design this function also
	static std::string serialize_histograms(const std::vector<uint16_t>& histograms, size_t pixel_count);

	// TODO: Design this functions also
	static std::vector<uint16_t> deserialize_histograms(const std::string& input, size_t& out_pixel_count);

	static std::string serialize_params(ClusteringParamsOnline params);

	static ClusteringParamsOnline deserialize_params(std::string input);
};

#endif /* PLUGIN_MAIN_SERIALIZER_H_ */


