/*
 * WebSocketHeader.h
 *
 *  Created on: 22 Apr 2013
 *      Author: douglas
 */

#ifndef HANDSHAKEHEADER_H_
#define HANDSHAKEHEADER_H_

#include <map>
#include <string>
#include "SocketReader.h"

namespace websocket {
namespace handshaking {
/* Represents a websocket header during the initial handshaking; the expected format is a standard HTTP header, e.g.:
 *
 * GET / HTTP/1.1
 * Sec-WebSocket-Key: VyBVU6KslCfXQ5HS0SZ+vQ==
 *
 * Or more generally:
 * <First line>
 * <Key>: <value> -- per value
 */
class HandshakeHeader {
private:
	std::string _FirstLine;
	std::map<std::string, std::string> _HeaderValuePairs;
public:
	HandshakeHeader(const std::string& FirstLine);
	HandshakeHeader(SocketReader& dataSource) throw (int);

	const std::string * getValue(const std::string& Key) const;
	HandshakeHeader& setValue(const std::string& Key, const std::string& Value);

	std::string toString() const;
};

}
} /* namespace websocket */
#endif /* HANDSHAKEHEADER_H_ */
