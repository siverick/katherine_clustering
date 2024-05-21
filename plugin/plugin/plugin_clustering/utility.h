/**
 * @utility.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once
//#include <debugapi.h>
#include <chrono>
#include <iostream>
#include <string>
#include <ctime>
#include <sstream>

#ifdef _MSC_VER
//#include <Windows.h>
#endif

/// <summary>
/// utility.h is used for timing purposes. 
/// 
/// ScopeTimer: Just create instance of it in beginning of scope and it will automatically print out data at the end of the scope when 
///				desctructor of ScopeTimer is called.
/// 
/// ContinualTimer: Call Start() and end where-ever you want. The Stop() returns a double for printing or adding.
/// 
/// Utility Class: Has utility functions to print out debug or profiling data.
/// </summary>

template <typename T>
std::string NumToString(const T& number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

struct ScopeTimer
{
	std::chrono::_V2::high_resolution_clock::time_point start;
	std::chrono::_V2::high_resolution_clock::time_point end;

	ScopeTimer()
	{
		start = std::chrono::high_resolution_clock::now();
	}

	~ScopeTimer()
	{
		Stop();
	}

	void Stop()
	{
		end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<float> duration = end - start;
		float elapsedms = duration.count() * 1000.0f;

		wchar_t text_buffer[40] = { 0 }; //temporary buffer
		swprintf(text_buffer, 40, L"%.1f ms, %.1f us \n", elapsedms, elapsedms*1000); // convert
		std::cout << text_buffer;
	}
};

struct ContinualTimer
{
	std::chrono::_V2::high_resolution_clock::time_point start;

	void Start()
	{
		start = std::chrono::high_resolution_clock::now();
	}

	// Return: time elapsed in microseconds
	double Stop()	
	{
		std::chrono::_V2::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<float> duration = end - start;
		float elapsed = duration.count() * 1000.0f;
		return elapsed;	// Elapsed ms
	}

	double ElapsedUs()
	{
		std::chrono::_V2::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<float> duration = end - start;
		float elapsed = duration.count();
		return elapsed;	// Elapsed us
	}

	double ElapsedMs()
	{
		std::chrono::_V2::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<float> duration = end - start;
		float elapsed = duration.count() * 1000.0f;
		return elapsed;	// Elapsed ms
	}
};

class utility 
{
public:
	/// <summary>
	/// Print time or other info to immediate window (OutputDebugString)
	/// </summary>
	/// <param name="title">Title</param>
	/// <param name="unit">Unit for title</param>
	/// <param name="time">Value in double</param>
	static std::string print_time_info(std::string title, std::string unit, double value)
	{
		wchar_t text_buffer[40] = { 0 }; //temporary buffer
		std::cout << title.c_str();
		std::string info = " [" + unit + "] : ";
		std::cout << info.c_str();
		swprintf(text_buffer, 40, L"%.1f \n", value); // convert
		std::cout << text_buffer;

		std::string output = title;
		output.append(" [");
		output.append(unit);
		output.append("] : ");

		output.append(NumToString(value));
		output.append("\r\n");
		return output;
	}

	static std::string print_info(std::string text, uint32_t value)
	{
		if (value == 0)
			std::cout << text.c_str();
		else
		{
			std::cout << text.c_str();
			wchar_t text_buffer[40] = { 0 }; //temporary buffer
			swprintf(text_buffer, 40, L"%d \n", value); // convert
		}

		std::cout << "\n";

		std::string output = text;
		output.append(NumToString((int32_t)value));
		output.append("\r\n");
		return output;
	}
};
