/*
	File: log.hpp
	Author: singularity
	Simply a way to log console output without any extra steps.
	May support its own mutex for multithreaded applications.
	This mutex can be removed with
		#define SYS_LOG_NO_MUTEX
*/

#ifndef SYS_LOG_HPP
#define SYS_LOG_HPP

#include <iostream>
#include <fstream>

#ifndef SYS_LOG_NO_MUTEX
#include <mutex>
#endif

namespace sys {
	/*  struct: out
		Author: singularity
		Use this to write any and all debugging/status text.
		Use it the same way you would std::cout.
		Ex: sys::cout << "Say it ain't so!!" << std::endl;
		Entries through this will be printed to terminal and log.txt   */
	struct out {
		std::ofstream fstream;
		bool log; // If we're using a log file (in case opening our file fails)

		#ifndef SYS_LOG_NO_MUTEX
		std::mutex mutex;
		#endif

		out();
		// Because it's a template method, we have to implement it here,
		// Or explicitly state all the versions we'll need in the .cpp
		// For ease, I'll do it here
		template<typename T> out& operator<<(const T& something) {
			std::cout << something;
			if (log)
				fstream << something;
			return *this;
		}
		typedef std::ostream& (*stream_function)(std::ostream&);
		out& operator<<(stream_function func);
	};

	extern out& cout; // Externally-defined reference to a singular global object
}

#endif // SYS_LOG_HPP
