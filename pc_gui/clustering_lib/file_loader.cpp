
/**
 * @file_loader.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "file_loader.h"
#include <fstream>

// load default file which should yield 2570 clusters
std::string file_loader::loadDefaultFile()
{
    std::string ret = "";
    loadPixelData("../clustering/konvick.txt", ret);
    return ret;
}

// Generic function for loading pixel data specified by path, data are emplaced into "input" reference
void file_loader::loadPixelData(std::string path, std::string& input)
{   
    if (input != "")
    {
        input.clear();
        input.shrink_to_fit();  // This actually releases the memory!
    }
    
    std::ifstream in(path, std::ios::in | std::ios::binary);
    
    if (in)
    {
        unsigned int off = 0;
        in.seekg(0, std::ios::end);
        int size = static_cast<int>(in.tellg());
        in.seekg(0, std::ios::beg);
        while (in.peek() != '#') {
            in.seekg(++off, std::ios::beg); // run to the first '#' char - some rubbish was added to front of string
            if (off > 10000) return;        // Error
        }
        input.resize(size - off);           // We have to resize the string first - or size == 0 -> undefined behaviour
        in.read(&input[0], size - off);
        in.close();
    }
    else {
        input = "fault"; // Error
    }
    
    return;
}

// Load calibration file specified by path
std::string file_loader::loadCalibration(std::string path)
{
    std::string calib;

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (in)
    {
        in.seekg(0, std::ios::end);
        int size = static_cast<int>(in.tellg());
        in.seekg(0, std::ios::beg);
        calib.resize(size);        // We have to resize the string first - or size == 0 -> undefined behaviour
        in.read(&calib[0], size);
        in.close();
    }
    else {
        return "fault";
    }

    return calib;
}
