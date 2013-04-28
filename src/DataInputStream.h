/*
 * DataInputStream.h
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#ifndef DATAINPUTSTREAM_H_
#define DATAINPUTSTREAM_H_

#include <stdint.h>

class DataInputStream {
private:
	template<uint32_t Bytes, typename ReturnType>
	ReturnType readUInt() {
		ReturnType totalSoFar = 0U;
		for (int i = 0; i < Bytes; i++) {
			totalSoFar <<= 8U;
			totalSoFar += read();
		}
	}
public:
	virtual ~DataInputStream() {

	}

	virtual uint8_t read() throw (int) = 0;

	uint16_t read16() throw (int) {
		return readUInt<2, uint16_t>();
	}

	uint32_t read32() throw (int) {
		return readUInt<4, uint32_t>();
	}

	uint64_t read64() throw (int) {
		return readUInt<8, uint64_t>();
	}
};

#endif /* DATAINPUTSTREAM_H_ */
