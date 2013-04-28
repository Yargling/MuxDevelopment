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
		const std::string& StringOfInterest) {
	for (int i = 0; i < StringOfInterest.length(); i++) {
		bufferOfInterest.push_back(StringOfInterest[i]);
	}
	//bufferOfInterest.sputn(StringOfInterest.c_str(), StringOfInterest.size());
}

//TODO: Make sure this works right for UTF8 encoding!
static const char * parseForHTML(const char* Data, const uint32_t Size) {
	//std::stringbuf buffer;
	std::list<char> buffer;
	for (size_t pos = 0; pos != Size; ++pos) {
		switch (Data[pos]) {
		case '&':
			append(buffer, "&amp;");
			break;
		case '\"':
			append(buffer, "&quot;");
			break;
		case '\'':
			append(buffer, "&apos;");
			break;
		case '<':
			append(buffer, "&lt;");
			break;
		case '>':
			append(buffer, "&gt;");
			break;
		default:
			buffer.push_back(Data[pos]);
			break;
		}
	}
	char * dataOut = new char[buffer.size()+1U];
	std::list<char>::iterator bufferItr = buffer.begin();
	uint32_t i = 0;
	while (bufferItr != buffer.end()) {
		dataOut[i]=*bufferItr;
		bufferItr++;
		i++;
	}
	dataOut[i] = '\0';

	return dataOut;
}

class Token {
public:
	enum TokenTypes {
		STRING, MODE_CHANGES
	};
private:
	std::vector<uint8_t> * Bytes;
	const TokenTypes _Type;

	std::vector<uint8_t> * processModeChange(const uint8_t *DataPtr,
			const uint32_t Size) {
		if (Size >= 3) {
			if (DataPtr[0] == '[' && DataPtr[Size - 1] == 'm') {
				uint8_t code = DataPtr[1] - '0';
				if (Size >= 4) {
					code = (code * 10) + DataPtr[2] - '0';
				}
				std::vector<uint8_t>* newVector = new std::vector<uint8_t>(1);
				(*newVector)[0] = code;
				return newVector;
			}
		}
		return NULL;
	}
	std::vector<uint8_t> * processString(const uint8_t *DataPtr,
			const uint32_t Size) {

		const char* parsedText = parseForHTML((char*)DataPtr, Size);
		//parsedText
		std::vector<uint8_t>* const VectorPtr = new std::vector<uint8_t>(strlen(parsedText));
		memcpy(VectorPtr->data(), parsedText, VectorPtr->size());
		delete[] parsedText;
		return VectorPtr;
	}
public:

	Token(const TokenTypes Type, const std::vector<uint8_t>& Data) :
			Bytes(NULL), _Type(Type) {
		if (_Type == MODE_CHANGES) {
			Bytes = processModeChange(Data.data(), Data.size());
		} else if (_Type == STRING) {
			Bytes = processString(Data.data(), Data.size());
		}
	}
	Token(const Token& RHS) : Bytes(new std::vector<uint8_t>(*RHS.Bytes)), _Type(RHS._Type) {
	}
	~Token() {
		if (Bytes != NULL) {
			delete Bytes;
		}
	}

	void sendToWebsocket(SocketWriter& Endpoint) {
		if (Bytes != NULL) {
			if (_Type == MODE_CHANGES) {
				sendWSData(Endpoint, Bytes->data(), Bytes->size());
			} else if (_Type == STRING) {
				sendWSText(Endpoint,(char*) (Bytes->data()), Bytes->size());
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
	std::list<uint8_t> _CurrentData;
	SocketWriter& _Writer;

	Token makeToken(Token::TokenTypes type);
};

Token OutputParse_Impl::makeToken(Token::TokenTypes type) {
	std::vector<uint8_t> dataCopy(_CurrentData.begin(), _CurrentData.end());
	const Token resultToken(type, dataCopy);
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

void OutputParser::processInEscapeSequence(std::list<Token>& tokens,
		uint8_t byte) {
	pImpl->_CurrentData.push_back(byte);
	if (byte == ESC_END) {
		tokens.push_back(pImpl->makeToken(Token::MODE_CHANGES));
		pImpl->_CurrentState = NORMAL_DATA;
	}
}

bool OutputParser::isDataEmpty() {
	return pImpl->_CurrentData.size() <= 0;
}

//TODO: Make sure this works with UTF8!
void OutputParser::processInNormal(std::list<Token>& tokens,
		const uint8_t Byte) {
	switch (Byte) {
	case IAC:
		if (!isDataEmpty()) {
			tokens.push_back(pImpl->makeToken(Token::STRING));
		}
		pImpl->_CurrentState = IAC_START;
		break;
	case ESC:
		if (!isDataEmpty()) {
			tokens.push_back(pImpl->makeToken(Token::STRING));
		}
		pImpl->_CurrentState = ESC_SEQ;
		break;
	default:
		pImpl->_CurrentData.push_back(Byte);
	}
}

void OutputParser::parseAndSend(const char * TextPtr, const size_t Size) {
	std::list<Token> currentTokens;

	for (int i = 0; i < Size; i++) {
		uint8_t currentByte = TextPtr[i];
		switch (pImpl->_CurrentState) {
		case NORMAL_DATA:
			processInNormal(currentTokens, currentByte);
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
			processInEscapeSequence(currentTokens, currentByte);
			break;
		}
	}

	if (pImpl->_CurrentData.size() > 0 && pImpl->_CurrentState == NORMAL_DATA) {
		currentTokens.push_back(pImpl->makeToken(Token::STRING));
	}

	std::list<Token>::iterator tokenItr = currentTokens.begin();
	while (tokenItr != currentTokens.end()) {
		tokenItr->sendToWebsocket(pImpl->_Writer);
		tokenItr++;
	}
	/*const std::string testString("Hello World");
	sendWSText(pImpl->_Writer, testString);*/
}

} /* namespace websocket */

