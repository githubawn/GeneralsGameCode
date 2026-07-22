#pragma once

#include "Common/SubsystemInterface.h"
#include <winsock2.h>
#include <ws2tcpip.h>

class Player1IPC : public SubsystemInterface
{
public:
	Player1IPC();
	virtual ~Player1IPC();

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;

	void startServer(unsigned short port = 9999);
	void stopServer();

	bool isClientConnected() const { return m_clientSocket != INVALID_SOCKET; }

private:
	void acceptConnection();
	void processIncomingCommands();
	void sendTelemetryState();

	SOCKET m_listenSocket;
	SOCKET m_clientSocket;
	bool m_initialized;
	unsigned int m_lastSendTime;
};

extern Player1IPC *ThePlayer1IPC;
