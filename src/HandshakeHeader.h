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
class HandshakeHeader {
private:
	std::string _FirstLine;
	std::map<std::string, std::string> _HeaderValuePairs;
public:
	HandshakeHeader(const std::string& FirstLine);
	HandshakeHeader(SocketReader& dataSource) throw (int);

	std::string toString();
};

}
} /* namespace websocket */
#endif /* HANDSHAKEHEADER_H_ */
