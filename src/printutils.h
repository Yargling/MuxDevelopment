/*
 * printutils.h
 *
 *  Created on: 23 Apr 2013
 *      Author: douglas
 */

#ifndef PRINTUTILS_H_
#define PRINTUTILS_H_

#include <string>
#include <sstream>
#include "externs.h"

std::string toString(const PortInfo& info);
std::string toString(const IntArray& Array);
std::string toString(const int& Value);

template<typename T>
std::string toString(const T Array[], const uint32_t Size) {
	std::stringstream outString;
	outString << "{" << std::dec << Size << ", [";
	for (unsigned int i = 0U; i < Size; i++) {
		outString << toString(Array[i]);
		if (i != (Size - 1)) {
			outString << ", ";
		}
	}
	outString << "]}";
	return outString.str();
}


#endif /* PRINTUTILS_H_ */
