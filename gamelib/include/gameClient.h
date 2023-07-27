#pragma once

#include "game.h"
#include "net.h"

//////////////////////////////////////////////////////////////////////////

enum game_message_type
{
  gmt_none,
  gmt_spaceship_thruster,
  gmt_spaceship_fire,
  gmt_projectile_thruster,
  gmt_projectile_trigger,
  gmt_switch,
  gmt_laser,
};

struct game_message
{
  game_message_type type;

  union
  {
    struct
    {
      uint8_t index;
      bool enabled;
    } thruster;

    struct
    {
      bool enabled;
    } laser;
  } data;
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

//////////////////////////////////////////////////////////////////////////

void game_spaceship_thruster_set_state_client(const size_t playerIndex, const size_t thrusterIndex, const bool enabled);
void game_spaceship_fire_client(const size_t playerIndex);
void game_projectile_thruster_set_state_client(const size_t playerIndex, const size_t thrusterIndex, const bool enabled);
void game_projectile_trigger_client(const size_t playerIndex);
void game_switch_client(const size_t playerIndex);
void game_spaceship_laser_set_state_client(const size_t playerIndex, const bool laserOn);
size_t game_getPlayerIndex_client();
