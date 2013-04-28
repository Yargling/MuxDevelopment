/*
 * Utils.cpp
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#include "Utils.h"

std::string& trimEnd(std::string& inputString) {
	const int endTrailIndex = inputString.find_last_not_of(" \t");
	if (endTrailIndex < inputString.size() - 1) {
		inputString.erase(endTrailIndex);
	}
}
std::string& trimBeginning(std::string& inputString) {
	const int beginTrailIndex = inputString.find_first_not_of(" \t");
	if (beginTrailIndex != std::string::npos) {
		inputString.erase(0, beginTrailIndex);
	}
}

std::string& trim(std::string& inputString) {
	trimEnd(inputString);
	trimBeginning(inputString);

	return inputString;
}

std::string trim(const std::string& InputString) {
	std::string localCopy(InputString);

	return trim(localCopy);
}

