/*
 * SocketReader.h
 *
 *  Created on: 22 Apr 2013
 *      Author: douglas
 */

#ifndef SOCKETREADER_H_
#define SOCKETREADER_H_

#include "config.h"
#include <vector>
#include <stdint.h>
#include <string>

namespace websocket {

class SocketReader {
private:
	const SOCKET _MySocket;
	uint8_t * const _BufferedData;
	uint_fast32_t _BufferIndex;

	int bufferData();
	uint32_t getAvailableData(uint8_t * dataOut, const uint32_t MaxOut) throw (int);
	char getNextChar() throw (int);
public:
	SocketReader(const SOCKET MySocket);
	~SocketReader();

	std::string readLine() throw (int);
	int readAvailable(uint8_t * bufferOut, const uint_fast32_t BufferSize);
};

} /* namespace websocket */
#endif /* SOCKETREADER_H_ */
