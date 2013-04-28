/*
 * WebSocketHeader.h
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#ifndef WEBSOCKETHEADER_H_
#define WEBSOCKETHEADER_H_

#include <vector>
#include <stdint.h>
#include "DataOutputStream.h"

namespace websocket {

enum Type {
	WS_CONTINUATION = 0, WS_TEXTFRAME = 1, WS_BINARY = 2, WS_CLOSE = 8, WS_PING = 9, WS_PONG = 10
};

struct ReadResult {
	bool hasClosed;
	std::vector<char> readBytes;
	ReadResult(std::vector<char> chars, bool closed) : hasClosed(closed), readBytes(chars) {
	}
};

struct WebSocketReader_Impl;
class WebSocketReader {
private:
	WebSocketReader_Impl * const _Impl;
public:
	WebSocketReader();
	~WebSocketReader();
	ReadResult processInput(const char * const DataPtr, const uint32_t Size);
};

class WebSocketHeader {
private:
	const Type _Type;
	const bool _FinalFrame;
	const bool _Masked;
	const uint64_t _PayloadSize;
	std::vector<uint8_t> _Mask;
	uint8_t _MaskIndex;

	void writeFirstFlags(DataOutputStream& dataOut) const throw (int);
	void writeMaskAndSize(DataOutputStream& dataOut) const throw (int);
	void writeMaskBytes(DataOutputStream& dataOut) const throw (int);
public:
	//TODO: Add back in and complete;
	WebSocketHeader(Type type, bool isFinalFrame, bool isMasked,
			uint64_t payloadSize);

	void writeOut(DataOutputStream& dataOut) const throw (int);
};

} /* namespace websocket */
#endif /* WEBSOCKETHEADER_H_ */

