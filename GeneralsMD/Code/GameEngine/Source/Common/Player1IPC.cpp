#include "PreRTS.h"
#include "Common/Player1IPC.h"
#include "GameLogic/GameLogic.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "GameLogic/Object.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/GlobalData.h"
#include "Common/Energy.h"
#include "Common/Money.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "Common/BuildAssistant.h"
#include "GameClient/TerrainVisual.h"

#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "ws2_32.lib")

Player1IPC *ThePlayer1IPC = nullptr;

Player1IPC::Player1IPC()
	: m_listenSocket(INVALID_SOCKET)
	, m_clientSocket(INVALID_SOCKET)
	, m_initialized(false)
	, m_lastSendTime(0)
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

				if (ThePlayerList && TheGameLogic && TheGameLogic->isInGame())
				{
					Player *p1 = ThePlayerList->getNthPlayer(0);
					if (p1)
					{
						// 1. BUILD_STRUCTURE Command
						if (strstr(payloadBuf, "\"BUILD_STRUCTURE\""))
						{
							char structType[128] = {0};
							unsigned int dozerID = 0;
							float tx = 0.0f, ty = 0.0f;

							char *sTypePtr = strstr(payloadBuf, "\"structure_type\":\"");
							if (sTypePtr) sscanf(sTypePtr, "\"structure_type\":\"%127[^\"]\"", structType);

							char *dozerPtr = strstr(payloadBuf, "\"dozer_id\":");
							if (dozerPtr) sscanf(dozerPtr, "\"dozer_id\":%u", &dozerID);

							char *posXPtr = strstr(payloadBuf, "\"x\":");
							if (posXPtr) sscanf(posXPtr, "\"x\":%f", &tx);

							char *posYPtr = strstr(payloadBuf, "\"y\":");
							if (posYPtr) sscanf(posYPtr, "\"y\":%f", &ty);

							if (strlen(structType) > 0 && TheThingFactory)
							{
								const ThingTemplate *tt = TheThingFactory->findTemplate(AsciiString(structType));
								if (tt)
								{
									// Find specified Dozer/Worker by ID or fallback search
									Object *dozerObj = nullptr;
									if (dozerID > 0)
									{
										Object *candidate = TheGameLogic->findObjectByID(static_cast<ObjectID>(dozerID));
										if (candidate && candidate->getControllingPlayer() == p1 && !candidate->isDestroyed())
										{
											dozerObj = candidate;
										}
									}

									if (!dozerObj)
									{
										for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject())
										{
											if (obj->getControllingPlayer() == p1 && !obj->isDestroyed())
											{
												AIUpdateInterface *ai = obj->getAIUpdateInterface();
												if (ai && ai->getDozerAIInterface() != nullptr)
												{
													dozerObj = obj;
													break;
												}
											}
										}
									}

									// Determine build coordinates: use passed (tx, ty) or offset near dozer
									Coord3D buildPos;
									buildPos.x = tx; buildPos.y = ty; buildPos.z = 0.0f;

									if ((tx == 0.0f && ty == 0.0f) && dozerObj && dozerObj->getPosition())
									{
										buildPos = *dozerObj->getPosition();
										buildPos.x += 40.0f;
										buildPos.y += 40.0f;
									}

									// Bypass strict BuildAssistant logic by passing isRebuild=true.
									// This makes the engine handle all terrain flattening, health setting,
									// and pathfinder map insertion cleanly while ignoring Human placement restrictions.
									if (dozerObj)
									{
										AIUpdateInterface *ai = dozerObj->getAIUpdateInterface();
										if (ai)
										{
											DozerAIInterface *dozerAI = ai->getDozerAIInterface();
											if (dozerAI)
											{
												// Validate or adjust buildPos with a spiral search
												Coord3D validPos = buildPos;
												Bool valid = TheBuildAssistant->isLocationLegalToBuild(&validPos, tt, 0.0f, BuildAssistant::NO_OBJECT_OVERLAP | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH, nullptr, p1) == LBC_OK;
												
												if (!valid)
												{
													Real searchRadius = 20.0f;
													for (int step = 1; step <= 6; ++step)
													{
														Real offset = searchRadius * step;
														Coord3D testPos;
														for (int dx = -1; dx <= 1; ++dx)
														{
															for (int dy = -1; dy <= 1; ++dy)
															{
																if (dx == 0 && dy == 0) continue;
																testPos.x = buildPos.x + (dx * offset);
																testPos.y = buildPos.y + (dy * offset);
																testPos.z = 0.0f;
																valid = TheBuildAssistant->isLocationLegalToBuild(&testPos, tt, 0.0f, BuildAssistant::NO_OBJECT_OVERLAP | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH, nullptr, p1) == LBC_OK;
																if (valid)
																{
																	validPos = testPos;
																	break;
																}
															}
															if (valid) break;
														}
														if (valid) break;
													}
												}

												if (TheTerrainVisual)
												{
													TheTerrainVisual->removeAllBibs(); // Cleanup visual feedback added by isLocationLegalToBuild
												}

												// Stop any pending movements (e.g. ATTACK_MOVE) so the AI state machine enters IDLE and starts the task
												ai->aiIdle(CMD_FROM_PLAYER);

												Object *scaffolding = dozerAI->construct(tt, &validPos, 0.0f, p1, true);
												if (scaffolding)
												{
													// Since we used isRebuild=true, it skips money withdrawal, so we do it manually
													if (p1->getMoney())
													{
														p1->getMoney()->withdraw(tt->calcCostToBuild(p1));
													}
													DEBUG_LOG(("Player1IPC: Forced construct for '%s' (ID %u) at (%.1f, %.1f) via Dozer ID %u",
														structType, (unsigned int)scaffolding->getID(), validPos.x, validPos.y, (unsigned int)dozerObj->getID()));
												}
												else
												{
													DEBUG_LOG(("Player1IPC: Failed to construct '%s' at (%.1f, %.1f)", structType, validPos.x, validPos.y));
												}
											}
										}
									}
								}
							}
						}
						// 2. PRODUCE_UNIT Command
						else if (strstr(payloadBuf, "\"PRODUCE_UNIT\""))
						{
							char unitType[128] = {0};
							char *uTypePtr = strstr(payloadBuf, "\"unit_type\":\"");
							if (uTypePtr) sscanf(uTypePtr, "\"unit_type\":\"%127[^\"]\"", unitType);

							if (strlen(unitType) > 0 && TheThingFactory)
							{
								const ThingTemplate *tt = TheThingFactory->findTemplate(AsciiString(unitType));
								if (tt)
								{
									float spawnX = 200.0f, spawnY = 200.0f;
									for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject())
									{
										if (obj->getControllingPlayer() == p1 && obj->isKindOf(KINDOF_STRUCTURE) && !obj->isDestroyed())
										{
											const Coord3D *bPos = obj->getPosition();
											if (bPos)
											{
												spawnX = bPos->x + 25.0f;
												spawnY = bPos->y + 25.0f;
											}
											break;
										}
									}

									Object *newUnit = TheGameLogic->friend_createObject(tt, OBJECT_STATUS_NONE, p1->getDefaultTeam());
									if (newUnit)
									{
										Coord3D pos;
										pos.x = spawnX;
										pos.y = spawnY;
										pos.z = 0.0f;
										newUnit->setPosition(&pos);
										DEBUG_LOG(("Player1IPC Produced unit '%s' for Player 1 at (%.1f, %.1f)", unitType, spawnX, spawnY));
									}
								}
							}
						}
						// 3. MOVE_UNITS / ATTACK_MOVE Command
						else if (strstr(payloadBuf, "\"MOVE_UNITS\"") || strstr(payloadBuf, "\"ATTACK_MOVE\""))
						{
							bool isAttackMove = strstr(payloadBuf, "\"ATTACK_MOVE\"") != nullptr;
							float tx = 400.0f, ty = 400.0f;

							char *posXPtr = strstr(payloadBuf, "\"x\":");
							if (!posXPtr) posXPtr = strstr(payloadBuf, "\"target_x\":");
							if (posXPtr) sscanf(posXPtr, "%*[^0-9.-]%f", &tx);

							char *posYPtr = strstr(payloadBuf, "\"y\":");
							if (!posYPtr) posYPtr = strstr(payloadBuf, "\"target_y\":");
							if (posYPtr) sscanf(posYPtr, "%*[^0-9.-]%f", &ty);

							Coord3D targetPos;
							targetPos.x = tx;
							targetPos.y = ty;
							targetPos.z = 0.0f;

							int movedCount = 0;
							for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject())
							{
								if (obj->getControllingPlayer() == p1 && !obj->isDestroyed())
								{
									if (!obj->isKindOf(KINDOF_STRUCTURE))
									{
										AIUpdateInterface *ai = obj->getAIUpdateInterface();
										if (ai)
										{
											if (isAttackMove)
												ai->aiAttackMoveToPosition(&targetPos, 0, CMD_FROM_PLAYER);
											else
												ai->aiMoveToPosition(&targetPos, CMD_FROM_PLAYER);
											movedCount++;
										}
									}
								}
							}
							DEBUG_LOG(("Player1IPC Executed %s for %d units to (%.1f, %.1f)",
								isAttackMove ? "ATTACK_MOVE" : "MOVE_UNITS", movedCount, tx, ty));
						}
					}
				}

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

	// Stop streaming when game match is closed or in main menu shell
	if (!TheGameLogic || !TheGameLogic->isInGame() || TheGameLogic->isInShellGame())
		return;

	unsigned int now = GetTickCount();
	if (now - m_lastSendTime < 50) // 20 Hz telemetry rate
		return;
	m_lastSendTime = now;

	int frame = TheGameLogic ? TheGameLogic->getFrame() : 0;
	std::string playersJson = "[";

	if (ThePlayerList)
	{
		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player *p = ThePlayerList->getNthPlayer(i);
			if (p && !p->isPlayerObserver() && p != ThePlayerList->getNeutralPlayer())
			{
				int pMoney = p->getMoney() ? p->getMoney()->countMoney() : 0;
				int pPower = p->getEnergy() ? p->getEnergy()->getProduction() : 0;
				int pMaxPower = p->getEnergy() ? (p->getEnergy()->getProduction() + p->getEnergy()->getConsumption()) : 0;

				std::string unitsJson = "[";
				std::string buildingsJson = "[";

				for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject())
				{
					if (obj->getControllingPlayer() == p && !obj->isDestroyed())
					{
						const Coord3D *pos = obj->getPosition();
						const char *typeName = (obj->getTemplate() != nullptr) ? obj->getTemplate()->getName().str() : "Unknown";

						float px = pos ? pos->x : 0.0f;
						float py = pos ? pos->y : 0.0f;

						char itemBuf[256];
						snprintf(itemBuf, sizeof(itemBuf),
								 "{\"id\":%u,\"type\":\"%s\",\"x\":%.1f,\"y\":%.1f}",
								 (unsigned int)obj->getID(), typeName, px, py);

						if (obj->isKindOf(KINDOF_STRUCTURE))
						{
							if (buildingsJson.length() > 1) buildingsJson += ",";
							buildingsJson += itemBuf;
						}
						else
						{
							if (unitsJson.length() > 1) unitsJson += ",";
							unitsJson += itemBuf;
						}
					}
				}
				unitsJson += "]";
				buildingsJson += "]";

				if (playersJson.length() > 1)
					playersJson += ",";

				std::string pStr = "{\"index\":" + std::to_string((int)i) +
								   ",\"is_ai\":" + (i > 0 ? "true" : "false") +
								   ",\"money\":" + std::to_string(pMoney) +
								   ",\"power\":" + std::to_string(pPower) +
								   ",\"max_power\":" + std::to_string(pMaxPower) +
								   ",\"owned_units\":" + unitsJson +
								   ",\"owned_buildings\":" + buildingsJson + "}";
				playersJson += pStr;
			}
		}
	}
	playersJson += "]";

	std::string jsonBuf = "{\"frame\":" + std::to_string(frame) + ",\"players\":" + playersJson + "}";

	unsigned int len = (unsigned int)jsonBuf.length();
	unsigned char header[4];
	header[0] = (unsigned char)((len >> 24) & 0xFF);
	header[1] = (unsigned char)((len >> 16) & 0xFF);
	header[2] = (unsigned char)((len >> 8) & 0xFF);
	header[3] = (unsigned char)(len & 0xFF);

	send(m_clientSocket, (const char *)header, 4, 0);
	send(m_clientSocket, jsonBuf.c_str(), len, 0);
}

void Player1IPC::update()
{
	if (!m_initialized)
		return;

	acceptConnection();
	processIncomingCommands();
	sendTelemetryState();
}
