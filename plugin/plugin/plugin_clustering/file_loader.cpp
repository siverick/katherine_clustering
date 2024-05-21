/**
 * @file_loader.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "file_loader.h"
#include <fstream>

std::string file_loader::loadDefaultFile()
{
    std::string ret = "";
    loadPixelData("../clustering/konvick.txt", ret);
    if (ret == "fault")
        loadPixelData("/bin/katherine/konvick.txt", ret); // Try reading from file inside katherine
    return ret;
}

std::string file_loader::loadDeg60()
{
    std::string ret = "";
    loadPixelData("../default_pixel_data/deg60_5.txt", ret);
    if (ret == "fault")
        loadPixelData("/bin/katherine/deg60_5.txt", ret);   // Try read in katherine
    return ret;
}

void file_loader::loadPixelData(std::string path, std::string& input)
{   
    if (input != "")
    {
        input.clear();
        input.resize(0);  // This actually releases the memory!
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
