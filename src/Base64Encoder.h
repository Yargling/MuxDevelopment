/*
 * Base64Encoder2.h
 *
 *  Created on: 22 Apr 2013
 *      Author: douglas
 */

#ifndef BASE64ENCODER_H_
#define BASE64ENCODER_H_

#include <stdint.h>
#include <string>

typedef unsigned int uint;

class Base64Encoder {
private:
	static const uint GroupSize;

	Base64Encoder(const unsigned char * DataIn, const uint DataInSize,
			char * dataOut) :
			DataIn(DataIn), DataInSize(DataInSize), dataOut(dataOut), outputIndex(
					0U) {

	}

	const unsigned char * DataIn;
	const uint DataInSize;
	char * dataOut;
	uint outputIndex;

	uint32_t getValuePadded(const uint Index);
	uint32_t getNextGrouping(const uint StartIndex);
	char bits6ToCharacter(const uint8_t Bits6In);
	char getNextCharFromGrouping(uint32_t& grouping);

	void appendCharToOutput(const char CharToAppend);

	void toDoEncoding();
public:
	static void toBase64String(const unsigned char * DataIn,
			const uint DataInSize, char * dataOut);
};

#endif /* BASE64ENCODER_H_ */
