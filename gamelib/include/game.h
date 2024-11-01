#pragma once

#include "core.h"
#include "pool.h"
#include "queue.h"
#include "data_blob.h"

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

// TODO: change name: target
// for rendering map have enum values for what should be rendered there that simply have the same enum value. food (and possibly other stuff) that consists of several targetables needs to be handled seperatly.
enum target_type // 32 different terrain_types possible.
{
  tT_grass,
  tT_water,
  tT_sand,
  tT_sapling,
  tT_tree,
  tT_trunk,
  tT_wood,

  nutrition_start,
  tT_vitamin = nutrition_start,
  tT_protein,
  tT_fat,
  tT_carbohydrates,
  nutrition_end =  tT_carbohydrates, // always +1 for nutrition_count!

  tT_mountain, // is used as collidable

  tT_Count,
};

struct terrain_element
{
  target_type terrainType : 5; // 32 different terrain_types.
  uint8_t elevationLevel : 3; // 8 different elevationLevel.
};

// TODO: gameplay element: ressource count, bool house?
struct gameplay_element
{
  size_t ressourceCount;
  size_t maxRessourceCount;
  bool hasHouse;
};
// gameplay map in level_info

// TODO: render element: texture, height, etc
struct render_element
{
  // texture
  uint8_t elevationLevel;
};
// render map

struct fill_step
{
  size_t index;

  inline fill_step() = default;
  inline fill_step(const size_t idx) : index(idx) { lsAssert(index < 16 * 16); } // Change if map size changes!
};

struct level_info
{
  struct resource_info
  {
    queue<fill_step> pathfinding_queue;
    uint8_t *pDirectionLookup[2] = {};
    size_t write_direction_idx = 0;
  } resources[tT_Count - 1]; // change to ressource-count once they exist

  terrain_element *pMap = nullptr;
  gameplay_element *pGPMap = nullptr; // change name, but like that?
  render_element *pRMap = nullptr; // change name, but like that?
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

enum entity_type
{
  eT_lumberjack,
  eT_stonemason,

  eT_count,
};

struct movement_actor
{
  vec2f pos;
  target_type target;
  bool atDestination = false;
  vec2f direction = vec2f(0);
  size_t lastTickTileIdx = 0;
};

struct lifesupport_actor
{
  entity_type type;
  size_t entityIndex;
  size_t nutritions[nutrition_end + 1 - nutrition_start];
  size_t lunchbox[nutrition_end + 1 - nutrition_start]; // either this simply counts the amount of nutrition and we eat it as we need it or it contains different foods?
};

enum lumberjack_actor_state
{
  laS_plant,
  laS_water,
  laS_chop,
  laS_cut,

  laS_count,
};

struct lumberjack_actor
{
  lumberjack_actor_state state;
  movement_actor *pActor;
};

struct game
{
  uint64_t lastUpdateTimeNs, gameStartTimeNs, lastPredictTimeNs;
  pool<entity> entities; // every entity must have a game object with the same index.
  pool<gameObject> gameObjects; // every game object must have a entity with the same index.
  queue<gameEvent> events;

  level_info levelInfo;
  pool<movement_actor> movementActors;
  pool<lifesupport_actor> lifesupportActors;

  float_t movementFriction = 0.965, turnFriction = 0.9;
  size_t tickRate = 60;
  size_t lastEventIndex, eventUpdateCutoffIndex;
};

size_t worldPosToTileIndex(vec2f pos);

lsResult game_init();
lsResult game_tick();

void game_setPlayerMapIndex(bool left);
void game_playerSwitchTiles(target_type terrainType);

game *game_getGame();

bool game_hasAnyEvent(game *pGame);
gameEvent game_getNextEvent(game *pGame);
size_t game_getTickRate();
size_t game_getLevelSize();

lsResult game_serialize(_Out_ data_blob *pBlob);
lsResult game_deserialze(_Out_ game *pGame, _In_ const void *pData, const size_t size);
