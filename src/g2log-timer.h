#ifndef __TIMER_H___53DFC74D_93D0_45A4_A030_4673290CEA33___
#define __TIMER_H___53DFC74D_93D0_45A4_A030_4673290CEA33___
/*
 *	From: https://kjellkod.wordpress.com/2013/01/22/exploring-c11-part-2-localtime-and-time-again/
 *
 *  For a full high-speed asynchronous logger: https://github.com/KjellKod/g3log
 *
 */
#include <string>
#include <chrono>
#include <ctime>

namespace g2 {
	typedef std::chrono::time_point<std::chrono::system_clock>  system_time_point;

	tm localtime(const std::time_t& time)
	{
		std::tm tm_snapshot;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
		localtime_s(&tm_snapshot, &time);
#else
		localtime_r(&time, &tm_snapshot); // POSIX  
#endif
		return tm_snapshot;
	}


	// To simplify things the return value is just a string. I.e. by design!  
	std::string put_time(const std::tm* date_time, const char* c_time_format)
	{
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
		std::ostringstream oss;

		// BOGUS hack done for VS2012: C++11 non-conformant since it SHOULD take a "const struct tm*  "
		// ref. C++11 standard: ISO/IEC 14882:2011, § 27.7.1, 
		oss << std::put_time(const_cast<std::tm*>(date_time), c_time_format);
		return oss.str();

#else    // LINUX
		const size_t size = 1024;
		char buffer[size];
		auto success = std::strftime(buffer, size, c_time_format, date_time);

		if (0 == success)
			return c_time_format;

		return buffer;
#endif
	}


	// extracting std::time_t from std:chrono for "now"
	std::time_t systemtime_now()
	{
		system_time_point system_now = std::chrono::system_clock::now();
		return std::chrono::system_clock::to_time_t(system_now);
	}

} // g2-namespace
#endif // __TIMER_H___53DFC74D_93D0_45A4_A030_4673290CEA33___