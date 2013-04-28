/*
 * Websockets.cpp
 *
 *  Created on: 26 Apr 2013
 *      Author: douglas
 */

#include <sys/time.h>
#include <sys/socket.h>
#include "SocketReader.h"
#include "SocketWriter.h"
#include "HandshakeHeader.h"

#include <stdlib.h>
#include <unistd.h>

#include "sha1_web.h"
#include "Base64Encoder.h"

#include "WebSocketHeader.h"
#include "svdhash.h"

#include <iostream>

using namespace websocket;
using namespace websocket::handshaking;
using namespace std;

#define WS_VERSION_INT (13U)
#define WS_VERSION_STR "13"
#define WS_KEY "Sec-WebSocket-Key"
#define WS_KEY_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
//258EAFA5-E914-47DA-95CA-C5AB0DC85B11

static bool checkWebSocketVersion(const HandshakeHeader& InputHeader);
static void issueReply(const SOCKET& SocketToUse,
		const HandshakeHeader& InputHeader) throw (int);
static HandshakeHeader makeReplyHeader(const std::string& Key);
static void sendWSPacket(websocket::SocketWriter& writer, const Type& PacketType, const uint8_t *DataPointer,
		const uint32_t Size)  throw (int);

void setSocketTimeout(const SOCKET& socket, const int Seconds) {
	struct timeval tv;

	tv.tv_sec = Seconds; /* n Secs Timeout */
	tv.tv_usec = 0;

	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
}

void clearSocketTimeout(const SOCKET& socket) {
	struct timeval tv;

	tv.tv_sec = 0; /* 0 Secs Timeout - causes the rcv-timeout to switch off */
	tv.tv_usec = 0;

	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
}

bool conductHandshake(const SOCKET& SocketToUse) throw (int) {
	bool worked = true;
	setSocketTimeout(SocketToUse, 30);

	try {
		SocketReader reader(SocketToUse);
		const HandshakeHeader inputHeader(reader);
		if (checkWebSocketVersion(inputHeader)) {
			issueReply(SocketToUse, inputHeader);
		} else {
			worked = false;
		}
	} catch (int e) {
		worked = false;
	}
	clearSocketTimeout(SocketToUse);

	return worked;
}

static bool checkWebSocketVersion(const HandshakeHeader& InputHeader) {
	const std::string * const ReadString = InputHeader.getValue(
			"Sec-WebSocket-Version");

	if (ReadString != NULL) {
		const int ReadVersion = atoi(ReadString->c_str());
		return ReadVersion >= WS_VERSION_INT;
	} else {
		return false;
	}
}

static void issueReply(const SOCKET& SocketToUse,
		const HandshakeHeader& InputHeader) throw (int) {
	const std::string ReplyText =
			makeReplyHeader(*InputHeader.getValue(WS_KEY)).toString();
	const int WriteResult = ::write(SocketToUse, ReplyText.c_str(),
			ReplyText.size());
	Log.WriteString("Web socket handshake complete: Input Header:\n");
	Log.WriteString(InputHeader.toString().c_str());
	Log.WriteString("Output Header:\n");
	Log.WriteString(ReplyText.c_str());

	if (WriteResult != ReplyText.size()) {
		throw WriteResult;
	}
}

static std::string keyInToReplyKey(const std::string& KeyIn) {
	std::string fullKey(KeyIn);
	fullKey.append(WS_KEY_MAGIC);

	std::vector<uint8_t> hashValue(20);
	sha1::calc(fullKey.c_str(), fullKey.length(), hashValue.data());

	std::vector<char> stringOutFin(50);
	Base64Encoder::toBase64String(hashValue.data(), 20U, stringOutFin.data());

	const std::string finalKey(stringOutFin.data());

	Log.tinyprintf("Websocket key: In: %s, Out: %s\n", KeyIn.c_str(),
			finalKey.c_str());

	return finalKey;
}

static HandshakeHeader makeReplyHeader(const std::string& Key) {
	HandshakeHeader replyHeader("HTTP/1.1 101 Switching Protocols");
	replyHeader.setValue("Upgrade", "websocket");
	replyHeader.setValue("Connection", "Upgrade");
	replyHeader.setValue("Sec-WebSocket-Version", WS_VERSION_STR);
	replyHeader.setValue("Sec-WebSocket-Accept", keyInToReplyKey(Key));

	return replyHeader;
}

void sendWSText(websocket::SocketWriter& writer,
		const std::string& StringToWrite) {
	sendWSPacket(writer, WS_TEXTFRAME, (uint8_t *)StringToWrite.data(), StringToWrite.size());
}

void sendWSText(websocket::SocketWriter& writer,
		const char * const StringToWrite, const uint32_t Size) throw (int) {
	sendWSPacket(writer, WS_TEXTFRAME, (uint8_t *)StringToWrite, Size);
}

void sendWSData(websocket::SocketWriter& writer, const uint8_t *DataPointer,
		const uint32_t Size) throw (int) {
	sendWSPacket(writer, WS_BINARY, DataPointer, Size);
}

void sendWSPacket(websocket::SocketWriter& writer, const Type& PacketType, const uint8_t *DataPointer,
		const uint32_t Size) throw (int) {
	WebSocketHeader headerToSend(PacketType, true, false, Size);

	headerToSend.writeOut(writer);

	for (int i = 0; i < Size; i++) {
		writer.write(DataPointer[i]);
	}
	writer.flushAll();
}
