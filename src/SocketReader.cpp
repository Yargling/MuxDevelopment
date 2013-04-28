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
#include <fcntl.h>

#include "svdhash.h"

namespace websocket {

static const uint32_t BufferSize = 200U;

#define MIN(a, b) (a < b ? a : b)

SocketReader::SocketReader(const SOCKET MySocket) :
		_MySocket(MySocket), _BufferedData(new uint8_t[BufferSize]), _BufferIndex(
				0U), _BufferReadIndex(0U) {
}
SocketReader::~SocketReader() {
	delete[] _BufferedData;
}

bool SocketReader::isBufferEmpty() {
	return (_BufferIndex == 0U) || (_BufferIndex == _BufferReadIndex);
}

std::string SocketReader::readLine() throw (int) {
	std::stringbuf buff;

	bool continueFlag = true;
	while (continueFlag) {
		const char CurrentChar = getNextChar();

		if (CurrentChar == '\n') {
			continueFlag = false;
		} else if (CurrentChar != '\r') {
			buff.sputc(CurrentChar);
		}
	}

	Log.WriteString(buff.str().c_str());
	Log.WriteString("\n");

	return buff.str();
}

int SocketReader::readAvailable(uint8_t * bufferOut,
		const uint_fast32_t BufferSize) {
	try {
		const int readBytes = getAvailableData(bufferOut, BufferSize);
		return readBytes;
	} catch (int e) {
		return e;
	}
}

int SocketReader::bufferData() {
	if (isBufferEmpty()) {
		const int ReadAmount = ::read(_MySocket, _BufferedData, BufferSize);
		const int ReadError = errno;

		if (ReadAmount > 0U) {
			_BufferIndex = ReadAmount;
			_BufferReadIndex = 0U;
		}

		return ReadAmount;
	} else {
		return 0;
	}
}

uint32_t SocketReader::getAvailableData(uint8_t * dataOut,
		const uint32_t MaxOut) throw (int) {

	if (isBufferEmpty()) {
		const int bufferResult = bufferData();

		if (bufferResult < 0 && _BufferIndex == 0U) {
			throw bufferResult;
		} else if (_BufferIndex == 0U) {
			throw 0;
		}
	}

	const uint32_t BytesToTake = MIN(_BufferIndex - _BufferReadIndex, MaxOut);
	memcpy(dataOut, &_BufferedData[_BufferReadIndex], BytesToTake);

	_BufferReadIndex += BytesToTake;

	return BytesToTake;
}

char SocketReader::getNextChar() throw (int) {
	uint8_t charByte;
	(void) getAvailableData(&charByte, 1);
	return char(charByte);
}

uint8_t SocketReader::read() throw (int) {
	return uint8_t(getNextChar());
}

} /* namespace websocket */
