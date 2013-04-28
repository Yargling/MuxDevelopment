/*
 * WebSocketHeader.cpp
 *
 *  Created on: 22 Apr 2013
 *      Author: douglas
 */

#include "HandshakeHeader.h"
#include <sstream>
#include "Utils.h"

namespace websocket {
namespace handshaking {

HandshakeHeader::HandshakeHeader(const std::string& FirstLine) :
		_FirstLine(FirstLine), _HeaderValuePairs() {
}
HandshakeHeader::HandshakeHeader(SocketReader& dataSource) throw (int) :
		_FirstLine(dataSource.readLine()), _HeaderValuePairs() {
	// TODO: COmplete!

	bool complete = false;
	while (!complete) {
		const std::string currentLine = dataSource.readLine();
		if (currentLine == "") {
			complete = true;
		} else {
			size_t dividerPos = currentLine.find(':');
			if (dividerPos == std::string::npos) {
				throw 100;
			}
			else {
				const std::string headerType(currentLine, 0, dividerPos);
				const std::string headerValue(currentLine, dividerPos+1);
				_HeaderValuePairs[headerType] = trim(headerValue);
			}
		}
	}
}

const std::string * HandshakeHeader::getValue(const std::string& Key) const {
	std::map<std::string, std::string>::const_iterator foundValue = _HeaderValuePairs.find(Key);
	if (foundValue != _HeaderValuePairs.end()) {
		return &foundValue->second;
	}
	else {
		return NULL;
	}
}

HandshakeHeader& HandshakeHeader::setValue(const std::string& Key, const std::string& Value) {
	_HeaderValuePairs[Key] = trim(std::string(Value));
	return *this;
}

std::string HandshakeHeader::toString() const {
	std::stringstream stringOutStream;
	stringOutStream << _FirstLine << "\r\n";

	std::map<std::string, std::string>::const_iterator itr = _HeaderValuePairs.begin();
	const std::map<std::string, std::string>::const_iterator itrEnd = _HeaderValuePairs.end();

	while (itr != itrEnd) {
		stringOutStream << itr->first << ": " << itr->second << "\r\n";
		itr++;
	}

	stringOutStream << "\r\n";

	return stringOutStream.str();
}

}
} /* namespace websocket */
