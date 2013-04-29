/*
 * WebSocketHeader.cpp
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#include "WebSocketHeader.h"
#include <stdlib.h>
#include <time.h>
#include "DataInputStream.h"
#include <limits>
#include <list>

static void initRandom() {
	static bool isInited = false;
	if (!isInited) {
		srand(time(NULL));
		isInited = true;
	}
}

namespace websocket {

static const uint32_t MaskSize = 4U;

static void parseUTF8ToASCII(std::list<char>& DataInOut) {
	//std::stringbuf buffer;
	std::list<char> buffer;

	std::list<char>::iterator dataToParseItr = DataInOut.begin();

	while (dataToParseItr != DataInOut.end()) {
		char& currentByte = *dataToParseItr;
		if ((uint8_t(currentByte) & 0x80U) != 0) {
			const char originalValue = currentByte;
			const char highBits = (originalValue & 0xC0) >> 6;
			const char lowBits = (originalValue & 0x3F);
			currentByte = 0xC0 + highBits;
			const char newByte = char(0x80) + lowBits;
			dataToParseItr++;
			DataInOut.insert(dataToParseItr, newByte);
		}
		dataToParseItr++;
	}
}

struct WebSocketReader_Impl {
	enum WebSocketState {
		FIN_AND_OPCODE,
		INITIAL_SIZE,
		SIZE_16BIT,
		SIZE_64BIT,
		MASK_BYTES,
		PAYLOAD,
	};
	enum Utf8State {
		SINGLE_BYTE, TWO_BYTE,
	};
	WebSocketState currentState;
	bool isFinal;
	bool isMasked;
	Type packetType;
	uint64_t packetSize;
	std::vector<char> mask;
	uint64_t stateCounter;
	Utf8State payloadState;
	uint8_t payloadStateCounter;
	char currentCompositeChar;

	WebSocketReader_Impl() :
			currentState(FIN_AND_OPCODE), isFinal(false), isMasked(false), packetType(), packetSize(), mask(
					MaskSize), stateCounter(0U) {

	}

	void gotoMaskOrPayload() {
		if (isMasked) {
			setState(MASK_BYTES);
		} else {
			gotoPayloadOrStart();
		}
	}

	void gotoPayloadOrStart() {
		if (packetSize == 0) {
			setState(FIN_AND_OPCODE);
		} else {
			setState(PAYLOAD);
		}
	}

	void setState(WebSocketState newState) {
		if (newState == FIN_AND_OPCODE) {
			// TODO: Packet complete - need to notify if close-packet?
		} else if (newState == PAYLOAD) {
			payloadState = SINGLE_BYTE;
			payloadStateCounter = 0U;
			currentCompositeChar = '\0';
		}
		currentState = newState;
		stateCounter = 0U;
	}

	void processFinOpcode(const uint8_t Byte) {
		isFinal = (Byte & 0x80U) != 0;
		packetType = Type(Byte & 0xFU);
		setState(INITIAL_SIZE);
	}

	void processInitialSize(const uint8_t Byte) {
		isMasked = (Byte & 0x80U) != 0;
		packetSize = (Byte & 0x7FU);

		if (packetSize == 126U) {
			packetSize = 0U;
			setState(SIZE_16BIT);
		} else if (packetSize == 127U) {
			packetSize = 0U;
			setState(SIZE_64BIT);
		} else {
			gotoMaskOrPayload();
		}
	}

	void processSize16(const uint8_t Byte) {
		packetSize <<= 8U;
		packetSize += Byte;
		stateCounter++;
		if (stateCounter == 2U) {
			gotoMaskOrPayload();
		}
	}
	void processSize64(const uint8_t Byte) {
		packetSize <<= 8U;
		packetSize += Byte;
		stateCounter++;
		if (stateCounter == 8U) {
			gotoMaskOrPayload();
		}
	}

	void processMask(const uint8_t Byte) {
		mask[stateCounter] = Byte;
		stateCounter++;
		if (stateCounter == mask.size()) {
			gotoPayloadOrStart();
		}
	}

	void processPayload(const uint8_t Byte, std::list<char>& outputChars) {
		if (packetType == WS_TEXTFRAME) {
			char decodedChar = char(Byte);
			if (isMasked) {
				decodedChar ^= mask[stateCounter % mask.size()];
			}
			processUTF8(decodedChar, outputChars);
		}
		stateCounter++;
		if (stateCounter == packetSize) {
			setState(FIN_AND_OPCODE);
		}
	}

	void switchUTF8State(Utf8State newState) {
		currentCompositeChar = '\0';
		payloadState = newState;
		payloadStateCounter = 0;
	}

	bool match(const uint8_t Byte, const uint8_t Mask, const uint8_t NegMask) {
		return ((Byte & Mask) == Mask) && (((~Byte) & NegMask) == NegMask);
	}

	void processUTF8(const uint8_t Byte, std::list<char>& outputChars) {
		switch (payloadState) {
		case SINGLE_BYTE:
			if ((Byte & 0x80U) == 0) {
				outputChars.push_back(Byte);
			} else if (match(Byte, 0xC0U, 0x20U)) {
				switchUTF8State(TWO_BYTE);
				currentCompositeChar = (Byte & 0x3U) << 6;
			}
			break;
		case TWO_BYTE:
			if (match(Byte, 0x80U, 0x40U)) {
				outputChars.push_back((Byte & 0x3F) | currentCompositeChar);
			}
			switchUTF8State(SINGLE_BYTE);
			break;
		}
	}
};

WebSocketReader::WebSocketReader() :
		_Impl(new WebSocketReader_Impl()) {
}
WebSocketReader::~WebSocketReader() {
	delete _Impl;
}

ReadResult WebSocketReader::processInput(const char * const DataPtr,
		const uint32_t Size) {
	std::list<char> readText;

	for (uint32_t i = 0U; i < Size; i++) {
		uint8_t readByte = DataPtr[i];
		switch (_Impl->currentState) {
		case WebSocketReader_Impl::FIN_AND_OPCODE:
			_Impl->processFinOpcode(readByte);
			break;
		case WebSocketReader_Impl::INITIAL_SIZE:
			_Impl->processInitialSize(readByte);
			break;
		case WebSocketReader_Impl::SIZE_16BIT:
			_Impl->processSize16(readByte);
			break;
		case WebSocketReader_Impl::SIZE_64BIT:
			_Impl->processSize64(readByte);
			break;
		case WebSocketReader_Impl::MASK_BYTES:
			_Impl->processMask(readByte);
			break;
		case WebSocketReader_Impl::PAYLOAD:
			_Impl->processPayload(readByte, readText);
			break;
		}
	}

	std::vector<char> v(readText.begin(), readText.end());

	return ReadResult(v, false);
}

/*WebSocketHeader::WebSocketHeader(SocketReader& inputSource) throw (int) {
 const uint8_t InitialFlags = inputSource.read();
 }*/

WebSocketHeader::WebSocketHeader(Type type, bool isFinalFrame, bool isMasked,
		uint64_t payloadSize) :
		_Type(type), _FinalFrame(isFinalFrame), _Masked(isMasked), _PayloadSize(
				payloadSize), _Mask(_Masked ? MaskSize : 0U), _MaskIndex(0U) {
	initRandom();
	if (_Masked) {
		for (int i = 0; i < _Mask.size(); i++) {
			_Mask[i] = uint8_t(rand());
		}
	}
}

void WebSocketHeader::writeFirstFlags(DataOutputStream& dataOut) const
		throw (int) {
	uint8_t byteToWrite = (_FinalFrame ? 0x80U : 0);
	byteToWrite += uint8_t(_Type);
	dataOut.write(byteToWrite);
}

void WebSocketHeader::writeMaskAndSize(DataOutputStream& dataOut) const
		throw (int) {
	const uint8_t MaskFlagByte = (_Masked ? 0x80U : 0U);

	if (_PayloadSize > 125) {
		if (_PayloadSize > std::numeric_limits<uint16_t>::max()) {
			dataOut.write(uint8_t(MaskFlagByte | 127U));
			dataOut.write64(_PayloadSize);
		} else {
			dataOut.write(uint8_t(MaskFlagByte | 126U));
			dataOut.write16(uint16_t(_PayloadSize));
		}
	} else {
		dataOut.write(uint8_t(MaskFlagByte | _PayloadSize));
	}
	writeMaskBytes(dataOut);
}

void WebSocketHeader::writeMaskBytes(DataOutputStream& dataOut) const
		throw (int) {
	if (_Masked) {
		for (int i = 0; i < _Mask.size(); i++) {
			dataOut.write(_Mask[i]);
		}
	}
}

void WebSocketHeader::writeOut(DataOutputStream& dataOut) const throw (int) {
	writeFirstFlags(dataOut);
	writeMaskAndSize(dataOut);
}

} /* namespace websocket */
