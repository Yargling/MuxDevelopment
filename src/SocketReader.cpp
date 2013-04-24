/*
 * SocketReader.cpp
 *
 *  Created on: 22 Apr 2013
 *      Author: douglas
 */

#include "SocketReader.h"
#include <string.h>
#include <sstream>
#include <unistd.h>

namespace websocket {

static const uint32_t BufferSize = 200U;

#define MIN(a, b) (a < b ? a : b)

SocketReader::SocketReader(const SOCKET MySocket) : _MySocket(MySocket), _BufferedData(new uint8_t[BufferSize]), _BufferIndex(0U) {
}
SocketReader::~SocketReader() {
	delete[] _BufferedData;
}

std::string SocketReader::readLine() throw (int) {
	std::stringbuf buff;

	bool continueFlag = true;
	while (continueFlag) {
		const char CurrentChar = getNextChar();

		if (CurrentChar == '\n') {
			continueFlag = false;
		}
		else if (CurrentChar != '\r') {
			buff.sputc(CurrentChar);
		}
	}

	return buff.str();
}

int SocketReader::readAvailable(uint8_t * bufferOut,
		const uint_fast32_t BufferSize) {
	try {
		const int readBytes = getAvailableData(bufferOut, BufferSize);
		return readBytes;
	}
	catch (int e) {
		return e;
	}
}

int SocketReader::bufferData() {

	if (_BufferIndex < BufferSize) {
		const int ReadAmount = ::read(_MySocket, &_BufferedData[_BufferIndex], BufferSize - _BufferIndex);

		if (ReadAmount > 0U) {
			_BufferIndex += ReadAmount;
		}

		return ReadAmount;
	}
	else {
		return 0;
	}
}

uint32_t SocketReader::getAvailableData(uint8_t * dataOut, const uint32_t MaxOut) throw (int) {
	const int bufferResult = bufferData();

	if (bufferResult < 0 && _BufferIndex == 0U) {
		throw bufferResult;
	}
	else if (_BufferIndex == 0U) {
		throw 0;
	}
	else {
		const uint32_t BytesToTake = MIN(_BufferIndex, MaxOut);
		memcpy(dataOut, _BufferedData, BytesToTake);

		_BufferIndex -= BytesToTake;

		return BytesToTake;
	}
}


char SocketReader::getNextChar() throw (int) {
	uint8_t charByte;
	if (_BufferIndex == 0U) {
		(void)getAvailableData(&charByte, 1);
	}
	else {
		// For efficiency - prevents mass polling of socket during read-line.
		_BufferIndex -= 1U;
		charByte = _BufferedData[_BufferIndex];
	}
	return char(charByte);
}

} /* namespace websocket */
