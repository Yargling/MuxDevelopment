/*
 * WebSocketHeader.cpp
 *
 *  Created on: 22 Apr 2013
 *      Author: douglas
 */

#include "HandshakeHeader.h"
#include <sstream>

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
				const std::string headerType(currentLine, 0, dividerPos-1);
				const std::string headerValue(currentLine, dividerPos+1);
				_HeaderValuePairs[headerType] = headerValue;
			}
		}
	}
}

std::string HandshakeHeader::toString() {
	std::stringstream stringOutStream;
	stringOutStream << _FirstLine << "\r\n";

	std::map<std::string, std::string>::iterator itr = _HeaderValuePairs.begin();
	const std::map<std::string, std::string>::iterator itrEnd = _HeaderValuePairs.end();

	while (itr != itrEnd) {
		stringOutStream << itr->first << ": " << itr->second << "\r\n";
	}

	stringOutStream << "\r\n";

	return stringOutStream.str();
}

}
} /* namespace websocket */
