/*
 * Websockets.h
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#ifndef WEBSOCKETS_H_
#define WEBSOCKETS_H_

#include "config.h"
#include "SocketReader.h"
#include "SocketWriter.h"
#include <string>

bool conductHandshake(const SOCKET& SocketToUse) throw (int);
void sendWSText(websocket::SocketWriter& writer, const std::string& StringToWrite) throw (int);
void sendWSText(websocket::SocketWriter& writer,
		const char * const StringToWrite, const uint32_t Size) throw (int);
void sendWSData(websocket::SocketWriter& writer, const uint8_t *DataPointer,
		const uint32_t Size) throw (int);

#endif /* WEBSOCKETS_H_ */
