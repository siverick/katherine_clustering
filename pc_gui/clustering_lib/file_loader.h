
/**
 * @file_loader.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once
#include <string>

class file_loader
{

public:
	static std::string loadDefaultFile();
	static void loadPixelData(std::string path, std::string& input);
	static std::string loadCalibration(std::string path);
};

