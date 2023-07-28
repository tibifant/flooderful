#include "gameClient.h"
#include "net.h"

#include <mutex>

//////////////////////////////////////////////////////////////////////////

static struct
{
  bool connected = false;
  tcpClient socket;

  std::mutex messageMutex;
  queue<game_message> messages;

  game_client_packet *pPacket = nullptr;
  size_t packetCapacity = 0;

  uint8_t *pResponse = nullptr;
  size_t responseCapacity = 0;

  game game;
  size_t playerIndex;
  size_t rtt;

  std::mutex gameMutex;
  std::thread communicationThread;
} _Client;

//////////////////////////////////////////////////////////////////////////

static lsResult _CommunicateWithServerLoop();

//////////////////////////////////////////////////////////////////////////

lsResult game_connect_client(const ipAddress *pAddress, const uint16_t port)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pAddress == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(_Client.connected, lsR_ResourceAlreadyExists);

  char ip[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")];
  LS_ERROR_CHECK(ipAddress_toString(pAddress, ip, sizeof(ip)));

  printf("Attempting to connect to [%s]:%" PRIu16 "\n", ip, port);

  LS_ERROR_CHECK(tcpClient_create(&_Client.socket, pAddress, port));
  LS_ERROR_CHECK(tcpClient_disableSendDelay(&_Client.socket));
  //LS_ERROR_CHECK(tcpClient_setSendTimeout(&_Client.socket, 100000));
  //LS_ERROR_CHECK(tcpClient_setReceiveTimeout(&_Client.socket, 100000));

  uint64_t packet = game_protocolVersion;
  LS_ERROR_CHECK(tcpClient_send(&_Client.socket, &packet, sizeof(packet)));

  uint8_t response[2 + sizeof(uint64_t)];
  LS_ERROR_CHECK(tcpClient_receive(&_Client.socket, response, sizeof(response)));

  LS_ERROR_IF(response[0] != 'O' || response[1] != 'K', lsR_ResourceIncompatible);

  _Client.communicationThread = std::thread(_CommunicateWithServerLoop);

  game_set_local(false);

  _Client.connected = true;

epilogue:
  return result;
}

lsResult  game_observe_client(_Out_ game *pGame)
{
  lsResult result = lsR_Success;

  _Client.gameMutex.lock();

  if (pGame->lastUpdateTimeNs == _Client.game.lastUpdateTimeNs)
    goto epilogue;
  
  LS_ERROR_CHECK(game_clone(&_Client.game, pGame));

  static int64_t lastMs = lsGetCurrentTimeMs();
  int64_t nowMs = lsGetCurrentTimeMs();

  if (nowMs - lastMs > 5000)
  {
    lastMs = nowMs;
    printf("RTT: %f ms\n", _Client.rtt * 1e-6f);
  }

epilogue:
  _Client.gameMutex.unlock();
  return result;
}

//////////////////////////////////////////////////////////////////////////

static lsResult _GetMessageData(uint8_t **ppWriteBuffer, size_t *pCapacity, _Out_ size_t *pSize, _Out_ uint64_t *pNowNs)
{
  lsResult result = lsR_Success;

  _Client.messageMutex.lock();

  const size_t requiredSize = sizeof(game_client_packet) + _Client.messages.count * sizeof(game_message);

  if (*pCapacity < requiredSize)
  {
    const size_t newCapacity = (requiredSize + 1023) & ~(size_t)1023;
    LS_ERROR_CHECK(lsRealloc(ppWriteBuffer, newCapacity));
    *pCapacity = newCapacity;
  }

  game_client_packet *pPacket = reinterpret_cast<game_client_packet *>(*ppWriteBuffer);

  const uint64_t nowNs = lsGetCurrentTimeNs();

  pPacket->structSize = requiredSize;
  pPacket->messageCount = _Client.messages.count;
  pPacket->nowNs = nowNs;
  pPacket->type = gcpt_update_if_update;

  for (size_t i = 0; i < _Client.messages.count; i++)
    memcpy(&pPacket->messages[i], &queue_get(&_Client.messages, i), sizeof(game_message));
  
  queue_clear(&_Client.messages);

  *pSize = requiredSize;
  *pNowNs = nowNs;

epilogue:
  _Client.messageMutex.unlock();
  return result;
}

static lsResult _CommunicateWithServerLoop()
{
  lsResult result = lsR_Success;

  puts("Communication Thread Running.");

  uint8_t *pReadBuffer = nullptr;
  size_t readCapacity = 0;
  uint8_t *pWriteBuffer = nullptr;
  size_t writeCapacity = 0;

  while (true)
  {
    uint64_t writeSize, nowNs;
    LS_ERROR_CHECK(_GetMessageData(&pWriteBuffer, &writeCapacity, &writeSize, &nowNs));

    LS_ERROR_CHECK(tcpClient_send(&_Client.socket, pWriteBuffer, writeSize));

    uint64_t readSize;
    LS_ERROR_CHECK(tcpClient_receive(&_Client.socket, &readSize, sizeof(readSize)));

    uint64_t readSizeFlag = readSize >> 32;

    if (readSizeFlag != 0xBADCA712)
    {
      printf("Incorrect Size Found. Recovering... (0x%" PRIX64 ")\n", readSize);

      while (true) // oh noes, sum fing wong... go into recovery mode.
      {
        LS_ERROR_CHECK(tcpClient_getReadableBytes(&_Client.socket, &readSize));

        if (readSize == 0)
          break;

        if (readSize > readCapacity)
        {
          const size_t newCapacity = (readSize + 1023) & ~(size_t)1023;
          LS_ERROR_CHECK(lsRealloc(&pReadBuffer, newCapacity));
          readCapacity = newCapacity;
        }

        size_t bytesRead;
        LS_ERROR_CHECK(tcpClient_receive(&_Client.socket, pReadBuffer, readSize, &bytesRead));

        if (bytesRead != readSize)
          break;
      }

      continue;
    }
    else
    {
      readSize &= 0xFFFFFFFF;
    }

    if (readSize == 0)
    {
      printf(".");
      Sleep(1);
      continue;
    }

    printf("Correct Size Found. (%" PRIu64 ")\n", readSize);

    if (readSize > readCapacity)
    {
      const size_t newCapacity = (readSize + 1023) & ~(size_t)1023;
      LS_ERROR_CHECK(lsRealloc(&pReadBuffer, newCapacity));
      readCapacity = newCapacity;
    }

    memcpy(pReadBuffer, &readSize, sizeof(readSize));

    size_t offset = sizeof(readSize);

    while (offset < readSize)
    {
      size_t bytesReceived = 0;
      LS_ERROR_CHECK(tcpClient_receive(&_Client.socket, pReadBuffer + offset, readSize - offset, &bytesReceived));
      offset += bytesReceived;
    }

    _Client.rtt = lsGetCurrentTimeNs() - nowNs;

    _Client.gameMutex.lock();

    const lsResult error = game_deserialze(&_Client.game, pReadBuffer, readSize);

    _Client.game.lastUpdateTimeNs += _Client.rtt / 2;
    _Client.game.gameStartTimeNs += _Client.rtt / 2;
    _Client.game.lastPredictTimeNs += _Client.rtt / 2;

    _Client.gameMutex.unlock();

    LS_ERROR_CHECK(error);
  }

epilogue:
  _Client.connected = false;
  lsFreePtr(&pReadBuffer);
  lsFreePtr(&pWriteBuffer);
  printf("Communication Thread exited with error code 0x%" PRIX64 ".\n", (uint64_t)result);

  return result;
}
