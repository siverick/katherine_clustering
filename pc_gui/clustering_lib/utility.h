
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

struct ScopeTimer
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start;
	std::chrono::time_point<std::chrono::high_resolution_clock> end;

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

		double m_start = static_cast<double>(std::chrono::time_point_cast<std::chrono::nanoseconds>(start).time_since_epoch().count());
		double m_end = static_cast<double>(std::chrono::time_point_cast<std::chrono::nanoseconds>(end).time_since_epoch().count());

		auto elapsed = m_end - m_start;
		double us = elapsed * 0.001;
		double ms = us * 0.001;

		wchar_t text_buffer[40] = { 0 }; //temporary buffer
		swprintf(text_buffer, _countof(text_buffer), L"%.1f ms, %.1f us \n", ms, us); // convert
		std::cout << text_buffer;
	}
};

struct ContinualTimer
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start;

	void Start()
	{
		start = std::chrono::high_resolution_clock::now();
	}

	// Return: time elapsed in nanoseconds
	double Stop()	
	{
		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();

		double m_start = static_cast<double>(std::chrono::time_point_cast<std::chrono::nanoseconds>(start).time_since_epoch().count());
		double m_end = static_cast<double>(std::chrono::time_point_cast<std::chrono::nanoseconds>(end).time_since_epoch().count());

		auto elapsed = m_end - m_start;
		return elapsed;
	}

	double ElapsedUs()
	{
		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();

		double m_start = static_cast<double>(std::chrono::time_point_cast<std::chrono::microseconds>(start).time_since_epoch().count());
		double m_end = static_cast<double>(std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch().count());

		auto elapsed = m_end - m_start;
		return elapsed;
	}

	double ElapsedMs()
	{
		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();

		double m_start = static_cast<double>(std::chrono::time_point_cast<std::chrono::milliseconds>(start).time_since_epoch().count());
		double m_end = static_cast<double>(std::chrono::time_point_cast<std::chrono::milliseconds>(end).time_since_epoch().count());

		auto elapsed = m_end - m_start;
		return elapsed;
	}
};

class utility 
{
public:
	/// <summary>
	/// Print time or other info to immediate window (stdout)
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
		swprintf(text_buffer, _countof(text_buffer), L"%.1f \n", value); // convert
		std::cout << text_buffer;

		std::string output = title;
		output.append(" [");
		output.append(unit);
		output.append("] : ");
		output.append(std::to_string(value));
		output.append("\r\n");
		return output;
	}

	/// <summary>
	/// Print text with value to stdout
	/// </summary>
	/// <param name="text"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	static std::string print_info(std::string text, uint32_t value)
	{
		if (value == 0)
			std::cout << text.c_str();
		else
		{
			std::cout << text.c_str();
			wchar_t text_buffer[40] = { 0 }; //temporary buffer
			swprintf(text_buffer, _countof(text_buffer), L"%d \n", value); // convert
		}

		std::cout << "\n";

		std::string output = text;
		output.append(std::to_string((int32_t)value));
		output.append("\r\n");
		return output;
	}
};