/*
 * Base64Encoder2.cpp
 *
 *  Created on: 22 Apr 2013
 *      Author: douglas
 */

#include "Base64Encoder.h"
#include "assert.h"

const uint Base64Encoder::GroupSize = 3U;

uint32_t Base64Encoder::getValuePadded(const uint Index) {
	const uint8_t Offset = uint8_t(GroupSize - (Index % GroupSize) - 1U);
	//const uint8_t Offset = uint8_t(Index % GroupSize);

	if (Index >= DataInSize) {
		return 0U;
	} else {
		return uint32_t(uint32_t(DataIn[Index]) << (8U * Offset));
	}
}

uint32_t Base64Encoder::getNextGrouping(const uint StartIndex) {
	uint32_t byte1 = getValuePadded(StartIndex);
	byte1 += getValuePadded(StartIndex + 1U);
	byte1 += getValuePadded(StartIndex + 2U);
	return byte1;
}

char Base64Encoder::bits6ToCharacter(const uint8_t Bits6In) {
	const char Base64Digits[] = {
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };

	assert(Bits6In < 64U);

	return Base64Digits[Bits6In];
}

void Base64Encoder::appendCharToOutput(const char CharToAppend) {
	dataOut[outputIndex] = CharToAppend;
	outputIndex += 1U;
}

char Base64Encoder::getNextCharFromGrouping(uint32_t& bits24GroupValue) {
	//assert(bits24GroupValue < 0x01000000U);

	const uint8_t CurBits6 = uint8_t((bits24GroupValue & 0xFC0000U) >> 18U);

	bits24GroupValue *= 64U;

	return bits6ToCharacter(CurBits6);
}

void Base64Encoder::toDoEncoding() {

	for (uint i = 0U; i < DataInSize; i += GroupSize) {
		uint32_t CurrentByteGroup = getNextGrouping(i);

		for (uint j = 0; j < 4 && (j+i) <= DataInSize; j+=1U) {
			appendCharToOutput(getNextCharFromGrouping(CurrentByteGroup));
		}
	}

	const uint8_t remainder = DataInSize % GroupSize;
	if (remainder == 1U) {
		appendCharToOutput('=');
		appendCharToOutput('=');
	} else if (remainder == 2U) {
		appendCharToOutput('=');
	}
}

void Base64Encoder::toBase64String(const unsigned char * DataIn,
		const uint DataInSize, char * dataOut) {
	Base64Encoder object(DataIn, DataInSize, dataOut);

	object.toDoEncoding();
}

