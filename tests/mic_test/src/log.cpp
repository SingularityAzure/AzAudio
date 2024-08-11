/*
	File: log.cpp
	Author: singularity
*/

#include "log.hpp"

namespace sys {

	out::out() : fstream("stdout.log") , log(true) {
		if (!fstream.is_open()) {
			std::cout << "Failed to open log.txt for writing" << std::endl;
			log = false;
		}
	}

	out& out::operator<<(stream_function func) {
		func(std::cout);
		if (log)
			func(fstream);
		return *this;
	}

	out _cout;
	out& cout = _cout;

}
