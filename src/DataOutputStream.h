/*
 * DataOutputStream.h
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#ifndef DATAOUTPUTSTREAM_H_
#define DATAOUTPUTSTREAM_H_

#include <stdint.h>
#include <vector>

class DataOutputStream {
private:
	template<uint64_t Bytes, typename InputType>
	void writeUInt(InputType value) throw (int) {
		uint8_t bytesToSend[Bytes];
		const InputType MaskValue = 0xFFU;
		for (unsigned int i = 0; i < Bytes; i++) {
			uint8_t currentByte = uint8_t(value & MaskValue);
			bytesToSend[Bytes - i - 1U] = currentByte;
			value >>= 8U;
		}
		for (unsigned int i = 0; i < Bytes; i++) {
			write(bytesToSend[i]);
		}
	}
public:
	virtual ~DataOutputStream() {

	}

	virtual void write(uint8_t value) throw (int) = 0;

	void write16(uint16_t value) throw (int) {
		writeUInt<2, uint16_t>(value);
	}

	void write32(uint32_t value) throw (int) {
		writeUInt<4, uint32_t>(value);
	}

	void write64(uint64_t value) throw (int) {
		writeUInt<8, uint64_t>(value);
	}
};

#endif /* DATAOUTPUTSTREAM_H_ */
