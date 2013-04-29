/*
 * OutputParser.cpp
 *
 *  Created on: 27 Apr 2013
 *      Author: douglas
 */

#include "OutputParser.h"
#include <list>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <list>
#include <string>
#include <sstream>
#include "SocketWriter.h"
#include "Websockets.h"
#include <string.h>

namespace websocket {

/*
 */
static const uint8_t IAC = 255U;
static const uint8_t ESC = 27U;
static const uint8_t ESC_END = uint8_t('m');
static const uint8_t SUB_NEG_B = 250U;

static void append(std::list<char>& bufferOfInterest,
		std::list<char>::iterator& position,
		const std::string& StringOfInterest) {
	bufferOfInterest.insert(position, StringOfInterest.begin(),
			StringOfInterest.end());
	position = bufferOfInterest.erase(position);
}

//TODO: Make sure this works right for UTF8 encoding!
static void parseForHTML(std::list<char>& DataInOut) {
	//std::stringbuf buffer;
	std::list<char> buffer;

	std::list<char>::iterator dataToParseItr = DataInOut.begin();

	while (dataToParseItr != DataInOut.end()) {
		switch (*dataToParseItr) {
		case '&':
			append(buffer, dataToParseItr, "&amp;");
			break;
		case '\"':
			append(buffer, dataToParseItr, "&quot;");
			break;
		case '\'':
			append(buffer, dataToParseItr, "&apos;");
			break;
		case '<':
			append(buffer, dataToParseItr, "&lt;");
			break;
		case '>':
			append(buffer, dataToParseItr, "&gt;");
			break;
		default:
			dataToParseItr++;
			break;
		}
	}
}

static void parseToUTF8(std::list<char>& DataInOut) {
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

class Token {
public:
	enum TokenTypes {
		STRING, MODE_CHANGES
	};
private:
	std::vector<uint8_t> * Bytes;
	const TokenTypes _Type;

	std::vector<uint8_t> * processModeChange(const std::list<char>& DataIn) {
		const std::vector<uint8_t> VectorCopy(DataIn.begin(), DataIn.end());
		const std::list<char>::size_type Size = DataIn.size();
		if (Size >= 3) {
			if (VectorCopy[0] == '[' && VectorCopy[Size - 1] == 'm') {
				uint8_t code = VectorCopy[1] - '0';
				if (Size >= 4) {
					code = (code * 10) + VectorCopy[2] - '0';
				}
				std::vector<uint8_t>* newVector = new std::vector<uint8_t>(1);
				(*newVector)[0] = code;
				return newVector;
			}
		}
		return NULL;
	}
	std::vector<uint8_t> * processString(std::list<char>& DataIn) {

		parseForHTML(DataIn);
		if (DataIn.size() == 688) {
			std::cout << "test" << std::endl;
		}
		parseToUTF8(DataIn);
		//parsedText
		std::vector<uint8_t>* const VectorPtr = new std::vector<uint8_t>(DataIn.begin(), DataIn.end());
		return VectorPtr;
	}
public:

	Token(const TokenTypes Type, std::list<char>& DataIn) :
			Bytes(NULL), _Type(Type) {
		if (_Type == MODE_CHANGES) {
			Bytes = processModeChange(DataIn);
		} else if (_Type == STRING) {
			Bytes = processString(DataIn);
		}
	}
	Token(const Token& RHS) :
			Bytes(new std::vector<uint8_t>(*RHS.Bytes)), _Type(RHS._Type) {
	}
	~Token() {
		if (Bytes != NULL) {
			delete Bytes;
		}
	}

	void sendToWebsocket(SocketWriter& Endpoint) {
		if (Bytes->size() > 125) {
			std::cout << "test" << std::endl;
		}
		if (Bytes != NULL) {
			if (_Type == MODE_CHANGES) {
				sendWSData(Endpoint, Bytes->data(), Bytes->size());
			} else if (_Type == STRING) {
				sendWSText(Endpoint, (char*) (Bytes->data()), Bytes->size());
			}
		}
	}
};

enum MuxStates {
	NORMAL_DATA, IAC_START, IAC_CONT1, IAC_NEGOTIATE, ESC_SEQ,
};
enum MuxSingleActions {
	WILL = 251, WONT = 252, DO = 253, DONT = 254
};

bool codeIsAnAction(const uint8_t Byte) {
	bool result = false;
	switch (Byte) {
	case WILL:
	case WONT:
	case DO:
	case DONT:
		result = true;
		break;
	}
	return result;
}

struct OutputParse_Impl {
	OutputParse_Impl(SocketWriter& writer) :
			_CurrentState(NORMAL_DATA), _CurrentData(), _Writer(writer) {

	}
	MuxStates _CurrentState;
	std::list<char> _CurrentData;
	SocketWriter& _Writer;

	Token makeToken(Token::TokenTypes type);
};

Token OutputParse_Impl::makeToken(Token::TokenTypes type) {
	const Token resultToken(type, _CurrentData);
	_CurrentData.clear();
	return resultToken;
}

OutputParser::OutputParser(SocketWriter& writer) :
		pImpl(new OutputParse_Impl(writer)) {
}

OutputParser::~OutputParser() {
	delete pImpl;
}

void OutputParser::processInIACStart(uint8_t byte) {
	switch (byte) {
	case IAC:
		pImpl->_CurrentData.push_back(IAC);
		pImpl->_CurrentState = NORMAL_DATA;
		break;
	case SUB_NEG_B:
		pImpl->_CurrentState = IAC_NEGOTIATE;
		break;
	default:
		if (codeIsAnAction(byte)) {
			pImpl->_CurrentState = IAC_CONT1;
		} else {
			pImpl->_CurrentState = NORMAL_DATA;
		}
		break;
	}
}

void OutputParser::processInNegotiate(uint8_t byte) {
	if (byte == IAC) {
		pImpl->_CurrentState = IAC_START;
	}
}

void OutputParser::processInEscapeSequence(//std::list<Token>& tokens,
		uint8_t byte) {
	pImpl->_CurrentData.push_back(byte);
	if (byte == ESC_END) {
		pImpl->makeToken(Token::MODE_CHANGES).sendToWebsocket(pImpl->_Writer);
		pImpl->_CurrentState = NORMAL_DATA;
	}
}

bool OutputParser::isDataEmpty() {
	return pImpl->_CurrentData.size() <= 0;
}

//TODO: Make sure this works with UTF8!
void OutputParser::processInNormal( //std::list<Token>& tokens,
		const uint8_t Byte) {
	switch (Byte) {
	case IAC:
		if (!isDataEmpty()) {
			//tokens.push_back(pImpl->makeToken(Token::STRING));
			pImpl->makeToken(Token::STRING).sendToWebsocket(pImpl->_Writer);
		}
		pImpl->_CurrentState = IAC_START;
		break;
	case ESC:
		if (!isDataEmpty()) {
			//tokens.push_back(pImpl->makeToken(Token::STRING));
			pImpl->makeToken(Token::STRING).sendToWebsocket(pImpl->_Writer);
		}
		pImpl->_CurrentState = ESC_SEQ;
		break;
	default:
		pImpl->_CurrentData.push_back(Byte);
	}
}

void OutputParser::parseAndSend(const char * TextPtr, const size_t Size) {
	//std::list<Token> currentTokens;

	for (int i = 0; i < Size; i++) {
		uint8_t currentByte = TextPtr[i];
		switch (pImpl->_CurrentState) {
		case NORMAL_DATA:
			processInNormal(currentByte);
			break;
		case IAC_START:
			processInIACStart(currentByte);
			break;
		case IAC_CONT1:
			pImpl->_CurrentState = NORMAL_DATA;
			break;
		case IAC_NEGOTIATE:
			processInNegotiate(currentByte);
			break;
		case ESC_SEQ:
			processInEscapeSequence(currentByte);
			break;
		}
	}

	if (pImpl->_CurrentData.size() > 0 && pImpl->_CurrentState == NORMAL_DATA) {
		pImpl->makeToken(Token::STRING).sendToWebsocket(pImpl->_Writer);
	}

	/*std::list<Token>::iterator tokenItr = currentTokens.begin();
	while (tokenItr != currentTokens.end()) {
		tokenItr->sendToWebsocket(pImpl->_Writer);
		tokenItr++;
	}*/
	/*const std::string testString("Hello World");
	 sendWSText(pImpl->_Writer, testString);*/
}

} /* namespace websocket */

