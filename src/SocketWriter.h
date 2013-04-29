/*
 * SocketWriter.h
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#ifndef SOCKETWRITER_H_
#define SOCKETWRITER_H_

#include <stdint.h>
#include <unistd.h>
#include "DataOutputStream.h"
#include "config.h"
#include <fcntl.h>
#include <iostream>
#include <assert.h>

namespace websocket {

class SocketWriter: public DataOutputStream {
private:
	const SOCKET _MySocket;
	std::vector<uint8_t> _BufferedData;
	uint_fast32_t _BufferIndex;

	void sendBuffer() throw (int) {
		bool continueTrying = true;
		while (continueTrying) {
			// TODO: Improve this to reduce blocking behaviour as the whole program is single thread.
			const int WriteResult = ::write(_MySocket, _BufferedData.data(),
					_BufferIndex);
			const int writeError = errno;
			if (WriteResult == -1) {
				if (writeError != EAGAIN) {
					throw writeError;
				}
				else {
					usleep(100);
				}
			} else {
				continueTrying = false;
			}
		}
		_BufferIndex = 0;
	}
public:
	SocketWriter(const SOCKET socket, const uint32_t BufferSize = 6000U) :
			_MySocket(socket), _BufferedData(BufferSize), _BufferIndex(0U) {

	}

	virtual void write(uint8_t dataIn) throw (int) {
		assert(_BufferIndex < _BufferedData.size());
		_BufferedData[_BufferIndex] = dataIn;
		_BufferIndex += 1U;

		if (_BufferIndex == 500) {
			std::cout << "test" << std::endl;
			std::cout.flush();
		}
		if (_BufferIndex >= _BufferedData.size()) {
			sendBuffer();
		}
	}

	void flushAll() throw (int) {
		sendBuffer();
	}
};

}
#endif /* SOCKETWRITER_H_ */
