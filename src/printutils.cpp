/*
 * printutils.cpp
 *
 *  Created on: 23 Apr 2013
 *      Author: douglas
 */

#include "printutils.h"

std::string toString(const PortInfo& info) {
	std::stringstream outString;
	outString << "{" << info.socket << ", " << info.port << ", " << info.type
			<< "}";
	return outString.str();
}

std::string toString(const IntArray& Array) {
	/*if (Array.pi == NULL || Array.n < 0U) {
	 std::stringstream outString;
	 outString << "{" << Array.n << ", N/A}";
	 return outString.str();
	 }
	 else {
	 return toString<int>(Array.pi, Array.n);
	 }*/
	std::stringstream outString;
	outString << "{" << Array.n << ", " << Array.pi << "}";
	return outString.str();

}

std::string toString(const int& Value) {
	std::stringstream outString;
	outString << Value;
}
