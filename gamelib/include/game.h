#pragma once

#include "core.h"
#include "pool.h"
#include "queue.h"
#include "dataBlob.h"

enum gameObject_type
{
  goT_invalid,
};

enum gameObject_shape
{
  goS_notCollidable,
  goS_sphere,
};

struct gameObject
{
  vec2f position, velocity;
  float_t rotation, angularVelocity;
  gameObject_type type;
  gameObject_shape shape;
  
  union
  {
#pragma warning(push)
#pragma warning(disable: 4201)
    struct
    {
      float_t sphereRadius, sphereRadiusSquared;
    };
#pragma warning(pop)
  } shapeData;
};

enum component_type : uint8_t
{
  ct_none,
};

struct entity
{
  static constexpr size_t MaxComponents = 7;

  component_type components[MaxComponents] = { };
  size_t ids[MaxComponents] = { };
};

enum gameEvent_type
{
  geT_invalid,
};

struct gameEvent
{
  size_t index;
  uint64_t timeNs;
  vec2f position;
  gameEvent_type type;
};

enum terrain_type
{
  tT_grass,
  tT_water,
  tT_sand,
  tT_mountain, // is used as collidable

  tT_Count,
};

struct fill_step
{
  size_t index;
};

struct level_info
{
  struct resource_info
  {
    queue<fill_step> pathfinding_queue;
    uint8_t *pDirectionLookup[2] = {};
    size_t write_direction_idx = 0;
  } resources[tT_Count - 1]; // change to ressource-count once they exist

  terrain_type *pMap = nullptr;
  vec2s map_size;
};

enum direction : uint8_t
{
  d_unreachable,

  d_topRight,
  d_right,
  d_bottomRight,
  d_bottomLeft,
  d_left,
  d_topLeft,

  d_unfillable,
};

struct game
{
  uint64_t lastUpdateTimeNs, gameStartTimeNs, lastPredictTimeNs;
  pool<entity> entities; // every entity must have a game object with the same index.
  pool<gameObject> gameObjects; // every game object must have a entity with the same index.
  queue<gameEvent> events;

  level_info levelInfo;

  float_t movementFriction = 0.965, turnFriction = 0.9;
  size_t tickRate = 60;
  size_t lastEventIndex, eventUpdateCutoffIndex;
};

lsResult game_init();
lsResult game_tick();
lsResult game_observe(_Out_ game *pGame);
lsResult game_clone(const game *pSrc, _Out_ game *pDst);
void game_predict(game *pGame);

void game_set_local(const bool isLocal);
//size_t game_addPlayer();

void game_setPlayerMapIndex(bool left);
void game_playerSwitchTiles(terrain_type terrainType);

bool game_hasAnyEvent(game *pGame);
gameEvent game_getNextEvent(game *pGame);
size_t game_getTickRate();
size_t game_getLevelSize();

lsResult game_serialize(_Out_ dataBlob *pBlob);
lsResult game_deserialze(_Out_ game *pGame, _In_ const void *pData, const size_t size);
