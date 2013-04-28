/*
 * OutputParser.h
 *
 *  Created on: 27 Apr 2013
 *      Author: douglas
 */

#ifndef OUTPUTPARSER_H_
#define OUTPUTPARSER_H_

#include "SocketWriter.h"
#include "config.h"
#include <list>

namespace websocket {

struct OutputParse_Impl;
class Token;

class OutputParser {
private:
	OutputParse_Impl * const pImpl;

	bool isDataEmpty();
	void processInEscapeSequence(std::list<Token>& tokens, uint8_t byte);
	void processInNegotiate(uint8_t byte);
	void processInIACStart(uint8_t byte);
	void processInNormal(std::list<Token>& tokens,
			const uint8_t Byte);
public:
	OutputParser(SocketWriter& Writer);
	virtual ~OutputParser();

	void parseAndSend(const char * TextPtr, const size_t Size);
};

} /* namespace websocket */
#endif /* OUTPUTPARSER_H_ */
