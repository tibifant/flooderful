#include "core.h"
#include "game.h"
#include "gameClient.h"
#include "net.h"

#include <mutex>

//////////////////////////////////////////////////////////////////////////

struct game_message_wrapper : game_message
{
  size_t playerIndex;
};

//////////////////////////////////////////////////////////////////////////

static lsResult gamesrv_receiveClients();
static lsResult gamesrv_updateGame();
static lsResult gamesrv_handleClient(tcpClient client);

//////////////////////////////////////////////////////////////////////////

static struct
{
  std::thread gameThread;
  std::mutex gameUpdateMutex;

  queue<game_message_wrapper> messages;
  std::mutex messageMutex;

  struct
  {
    std::mutex mutex;
    dataBlob blob;
  } serialized[2];

  volatile size_t serializedIndex = 0;
} _Server;

//////////////////////////////////////////////////////////////////////////

int32_t main(int32_t argc, char **pArgv)
{
  (void)argc;
  (void)pArgv;

  puts("Starting Server...");

  std::thread gameUpdateThread(gamesrv_updateGame);

  Sleep(1000);

  return LS_SUCCESS(gamesrv_receiveClients()) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//////////////////////////////////////////////////////////////////////////

lsResult gamesrv_receiveClients()
{
  lsResult result = lsR_Success;

  tcpServer socket;
  LS_ERROR_CHECK(tcpServer_create(&socket, (uint16_t)0x7181));

  ipAddress address;
  uint16_t port;
  LS_ERROR_CHECK(tcpServer_getLocalEndPointInfo(&socket, &address, &port));

  char ip[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")];
  LS_ERROR_CHECK(ipAddress_toString(&address, ip, sizeof(ip)));

  printf("Server launched at [%s]:%" PRIu16 "\n", ip, port);

  while (true)
  {
    tcpClient client;
    LS_ERROR_CHECK(tcpServer_listen(&socket, &client));

    new std::thread(gamesrv_handleClient, client);
  }

epilogue:
  return result;
}

lsResult gamesrv_updateGameStep()
{
  lsResult result = lsR_Success;

  _Server.gameUpdateMutex.lock();
  _Server.messageMutex.lock();

  while (queue_any(&_Server.messages))
  {
    game_message_wrapper msg;
    queue_popFront(&_Server.messages, &msg);

    switch (msg.type)
    {
    case gmt_none:
    default:
    {
      printf("Invalid Message Type 0x%" PRIX64 " from player %" PRIu64 ".\n", (uint64_t)msg.type, msg.playerIndex);
      break;
    }

    case gmt_spaceship_thruster:
    {
      game_spaceship_thruster_set_state(msg.playerIndex, msg.data.thruster.index, msg.data.thruster.enabled);
      break;
    }

    case gmt_spaceship_fire:
    {
      game_spaceship_fire(msg.playerIndex);
      break;
    }

    case gmt_projectile_thruster:
    {
      game_projectile_thruster_set_state(msg.playerIndex, msg.data.thruster.index, msg.data.thruster.enabled);
      break;
    }

    case gmt_projectile_trigger:
    {
      game_projectile_trigger(msg.playerIndex);
      break;
    }

    case gmt_switch:
    {
      game_switch(msg.playerIndex);
      break;
    }

    case gmt_laser:
    {
      game_spaceship_laser_set_state(msg.playerIndex, msg.data.laser.enabled);
      break;
    }
    }
  }

  _Server.messageMutex.unlock();

  LS_ERROR_CHECK(game_tick());

  const size_t nextIndex = (_Server.serializedIndex + 1) % LS_ARRAYSIZE(_Server.serialized);

  _Server.serialized[nextIndex].mutex.lock();

  const lsResult error = game_serialize(&_Server.serialized->blob);

  _Server.serialized[nextIndex].mutex.unlock();

  LS_ERROR_CHECK(error);

  _Server.serializedIndex = nextIndex;

epilogue:
  _Server.gameUpdateMutex.unlock();
  return result;
}

lsResult gamesrv_updateGame()
{
  lsResult result = lsR_Success;

  game_set_authority(true);

  const float_t updateTimeMs = 1000.0f / game_getTickRate();

  while (true)
  {
    const int64_t before = lsGetCurrentTimeNs();

    LS_ERROR_CHECK(gamesrv_updateGameStep());

    const int64_t after = lsGetCurrentTimeNs();
    const float_t ms = (after - before) * 1e-6f;
    const int64_t sleepMs = (int64_t)floorf(updateTimeMs - ms);

    if (sleepMs > 0)
      Sleep((DWORD)sleepMs);
  }

epilogue:
  return result;
}

lsResult gamesrv_handleClient(tcpClient client)
{
  lsResult result = lsR_Success;

  ipAddress address;
  uint16_t port = 0;
  char ip[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")] = "<undefined>";

  uint8_t *pReadBuffer = nullptr;
  size_t readCapacity = 0;
  uint8_t *pWriteBuffer = nullptr;
  size_t writeCapacity = 0;
  size_t writeSize = 0;

  LS_ERROR_CHECK(tcpClient_getRemoteEndPointInfo(&client, &address, &port));
  LS_ERROR_CHECK(ipAddress_toString(&address, ip, sizeof(ip)));

  printf("Client connected from [%s]:%" PRIu16 "\n", ip, port);

  LS_ERROR_CHECK(tcpClient_disableSendDelay(&client));
  //LS_ERROR_CHECK(tcpClient_setSendTimeout(&client, 100000));
  //LS_ERROR_CHECK(tcpClient_setReceiveTimeout(&client, 100000));

  uint64_t version;
  LS_ERROR_CHECK(tcpClient_receive(&client, &version, sizeof(version)));
  LS_ERROR_IF(version != game_protocolVersion, lsR_ResourceIncompatible);

  _Server.gameUpdateMutex.lock();
  const size_t playerIndex = game_addPlayer();
  _Server.gameUpdateMutex.unlock();

  uint8_t response[2 + sizeof(uint64_t)];
  response[0] = 'O';
  response[1] = 'K';
  memcpy(response + 2, &playerIndex, sizeof(playerIndex));

  LS_ERROR_CHECK(tcpClient_send(&client, response, sizeof(response)));

  uint64_t lastTransmittedUpdateTimeNs = 0;

  while (true)
  {
    uint64_t packetBytes = 0;
    LS_ERROR_CHECK(tcpClient_receive(&client, &packetBytes, sizeof(packetBytes)));

    if (packetBytes > readCapacity)
    {
      const size_t newCapacity = (packetBytes + 1023) & ~(size_t)1023;
      LS_ERROR_CHECK(lsRealloc(&pReadBuffer, newCapacity));
      readCapacity = newCapacity;
    }

    memcpy(pReadBuffer, &packetBytes, sizeof(packetBytes));

    LS_ERROR_CHECK(tcpClient_receive(&client, pReadBuffer + sizeof(uint64_t), packetBytes - sizeof(uint64_t)));

    const game_client_packet *pClientPacket = reinterpret_cast<const game_client_packet *>(pReadBuffer);

    if (pClientPacket->messageCount > 0)
    {
      _Server.messageMutex.lock();
      
      for (size_t i = 0; i < pClientPacket->messageCount; i++)
      {
        game_message_wrapper msg;
        memcpy(&msg, &pClientPacket->messages[i], sizeof(game_message));
        msg.playerIndex = playerIndex;

        queue_pushBack(&_Server.messages, &msg);
      }

      _Server.messageMutex.unlock();
    }

    bool sendNewerPacket = false;

    if (pClientPacket->type == gcpt_update_if_update)
    {
      const size_t serializedIndex = _Server.serializedIndex;

      _Server.serialized[serializedIndex].mutex.lock();

      if (_Server.serialized[serializedIndex].blob.size > 4 * sizeof(uint64_t))
      {
        uint64_t updateTimeNs;
        memcpy(&updateTimeNs, _Server.serialized[serializedIndex].blob.pData + sizeof(uint64_t), sizeof(updateTimeNs));

        if (updateTimeNs != lastTransmittedUpdateTimeNs)
        {
          if (_Server.serialized[serializedIndex].blob.size > writeCapacity)
          {
            const size_t newCapacity = (_Server.serialized[serializedIndex].blob.size + 1023) & ~(size_t)1023;
            LS_ERROR_CHECK(lsRealloc(&pWriteBuffer, newCapacity));
            writeCapacity = newCapacity;
          }

          writeSize = _Server.serialized[serializedIndex].blob.size;
          memcpy(pWriteBuffer, _Server.serialized[serializedIndex].blob.pData, writeSize);

          lastTransmittedUpdateTimeNs = updateTimeNs;
          sendNewerPacket = true;
        }
      }

      _Server.serialized[serializedIndex].mutex.unlock();
    }

    if (sendNewerPacket)
    {
      const uint64_t now = lsGetCurrentTimeNs();

      uint64_t *pWrite64 = reinterpret_cast<uint64_t *>(pWriteBuffer);

      pWrite64[0] |= ((uint64_t)0xBADCA712 << 32);

      for (size_t i = 1; i <= 3; i++)
        pWrite64[i] = pWrite64[i] - now + pClientPacket->nowNs;

      //printf("sending game state (%" PRIu64 " bytes)\n", writeSize);
      LS_ERROR_CHECK(tcpClient_send(&client, pWriteBuffer, writeSize));
    }
    else
    {
      uint64_t zero = 0 | ((uint64_t)0xBADCA712 << 32);
      LS_ERROR_CHECK(tcpClient_send(&client, &zero, sizeof(zero)));
    }
  }

epilogue:
  printf("Client Thread for [%s]:%" PRIu16 " exited with error code 0x%" PRIX64 ".\n", ip, port, (uint64_t)result);
  tcpClient_destroy(&client);
  lsFreePtr(&pReadBuffer);
  lsFreePtr(&pWriteBuffer);

  return result;
}
