#pragma once

#include "game.h"
#include "net.h"

//////////////////////////////////////////////////////////////////////////

enum game_message_type
{
  gmt_none,
};

struct game_message
{
  game_message_type type;
};

enum game_client_packet_type
{
  gcpt_messages_only,
  gcpt_update_if_update,
};

#pragma warning(push)
#pragma warning(disable: 4200)
struct game_client_packet
{
  size_t structSize;
  game_client_packet_type type;
  uint64_t nowNs;
  size_t messageCount;
  game_message messages[];
};
#pragma warning(pop)

enum
{
  game_protocolVersion = 1,
};

//////////////////////////////////////////////////////////////////////////

lsResult game_connect_client(const ipAddress *pAddress, const uint16_t port);
lsResult game_observe_client(_Out_ game *pGame);
