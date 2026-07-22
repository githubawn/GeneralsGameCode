#include "PreRTS.h"
#include "Common/Player1IPC.h"
#include "GameLogic/GameLogic.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/Object.h"
#include "Common/ThingFactory.h"
#include "Common/GlobalData.h"

#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "ws2_32.lib")

Player1IPC *ThePlayer1IPC = nullptr;

Player1IPC::Player1IPC()
	: m_listenSocket(INVALID_SOCKET)
	: m_clientSocket(INVALID_SOCKET)
	: m_initialized(false)
	: m_lastSendTime(0)
{
}

Player1IPC::~Player1IPC()
{
	stopServer();
}

void Player1IPC::init()
{
	if (m_initialized)
		return;

	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		DEBUG_LOG(("Player1IPC: WSAStartup failed with code %d", result));
		return;
	}

	m_initialized = true;
	startServer(9999);
}

void Player1IPC::reset()
{
	stopServer();
	init();
}

void Player1IPC::startServer(unsigned short port)
{
	if (!m_initialized)
		return;

	m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_listenSocket == INVALID_SOCKET)
	{
		DEBUG_LOG(("Player1IPC: Failed to create socket"));
		return;
	}

	// Set non-blocking mode
	u_long mode = 1;
	ioctlsocket(m_listenSocket, FIONBIO, &mode);

	// Enable SO_REUSEADDR
	int optval = 1;
	setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval));

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_port = htons(port);

	if (bind(m_listenSocket, (SOCKADDR *)&service, sizeof(service)) == SOCKET_ERROR)
	{
		DEBUG_LOG(("Player1IPC: Socket bind failed on port %d", port));
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
		return;
	}

	if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		DEBUG_LOG(("Player1IPC: Socket listen failed"));
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
		return;
	}

	DEBUG_LOG(("Player1IPC: Listening for Python Player 1 Controller on 127.0.0.1:%d", port));
}

void Player1IPC::stopServer()
{
	if (m_clientSocket != INVALID_SOCKET)
	{
		closesocket(m_clientSocket);
		m_clientSocket = INVALID_SOCKET;
	}

	if (m_listenSocket != INVALID_SOCKET)
	{
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
	}

	if (m_initialized)
	{
		WSACleanup();
		m_initialized = false;
	}
}

void Player1IPC::acceptConnection()
{
	if (m_listenSocket == INVALID_SOCKET || m_clientSocket != INVALID_SOCKET)
		return;

	SOCKET client = accept(m_listenSocket, nullptr, nullptr);
	if (client != INVALID_SOCKET)
	{
		// Set client socket to non-blocking
		u_long mode = 1;
		ioctlsocket(client, FIONBIO, &mode);

		// Disable Nagle algorithm
		int nodelay = 1;
		setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));

		m_clientSocket = client;
		DEBUG_LOG(("Player1IPC: Python Controller connected successfully for Player 1!"));
	}
}

void Player1IPC::processIncomingCommands()
{
	if (m_clientSocket == INVALID_SOCKET)
		return;

	static char headerBuf[4];
	static int headerBytesRead = 0;
	static char *payloadBuf = nullptr;
	static unsigned int payloadLen = 0;
	static unsigned int payloadBytesRead = 0;

	// Read 4-byte big-endian header length
	if (headerBytesRead < 4)
	{
		int bytes = recv(m_clientSocket, headerBuf + headerBytesRead, 4 - headerBytesRead, 0);
		if (bytes > 0)
		{
			headerBytesRead += bytes;
			if (headerBytesRead == 4)
			{
				payloadLen = (unsigned char)headerBuf[0] << 24 |
							 (unsigned char)headerBuf[1] << 16 |
							 (unsigned char)headerBuf[2] << 8 |
							 (unsigned char)headerBuf[3];
				payloadBuf = new char[payloadLen + 1];
				payloadBytesRead = 0;
			}
		}
		else if (bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
		{
			DEBUG_LOG(("Player1IPC: Client disconnected"));
			closesocket(m_clientSocket);
			m_clientSocket = INVALID_SOCKET;
			headerBytesRead = 0;
			return;
		}
	}

	// Read payload data
	if (headerBytesRead == 4 && payloadBuf != nullptr)
	{
		int bytes = recv(m_clientSocket, payloadBuf + payloadBytesRead, payloadLen - payloadBytesRead, 0);
		if (bytes > 0)
		{
			payloadBytesRead += bytes;
			if (payloadBytesRead == payloadLen)
			{
				payloadBuf[payloadLen] = '\0';
				DEBUG_LOG(("Player1IPC Recv Command: %s", payloadBuf));

				delete[] payloadBuf;
				payloadBuf = nullptr;
				headerBytesRead = 0;
			}
		}
		else if (bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
		{
			DEBUG_LOG(("Player1IPC: Client disconnected while receiving payload"));
			delete[] payloadBuf;
			payloadBuf = nullptr;
			headerBytesRead = 0;
			closesocket(m_clientSocket);
			m_clientSocket = INVALID_SOCKET;
		}
	}
}

void Player1IPC::sendTelemetryState()
{
	if (m_clientSocket == INVALID_SOCKET)
		return;

	unsigned int now = GetTickCount();
	if (now - m_lastSendTime < 100) // 10 Hz telemetry rate
		return;
	m_lastSendTime = now;

	Player *p1 = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
	int frame = TheGameLogic ? TheGameLogic->getFrame() : 0;
	int money = p1 ? p1->getMoney().countMoney() : 0;

	char jsonBuf[1024];
	snprintf(jsonBuf, sizeof(jsonBuf),
			 "{\"frame\":%d,\"player\":1,\"money\":%d}",
			 frame, money);

	unsigned int len = (unsigned int)strlen(jsonBuf);
	unsigned char header[4];
	header[0] = (unsigned char)((len >> 24) & 0xFF);
	header[1] = (unsigned char)((len >> 16) & 0xFF);
	header[2] = (unsigned char)((len >> 8) & 0xFF);
	header[3] = (unsigned char)(len & 0xFF);

	send(m_clientSocket, (const char *)header, 4, 0);
	send(m_clientSocket, jsonBuf, len, 0);
}

void Player1IPC::update()
{
	if (!m_initialized)
		return;

	acceptConnection();
	processIncomingCommands();
	sendTelemetryState();
}
