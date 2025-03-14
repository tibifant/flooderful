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

// for rendering map have enum values for what should be rendered there that simply have the same enum value. food (and possibly other stuff) that consists of several targetables needs to be handled seperatly.
enum pathfinding_target_type // 32 different terrain_types possible.
{
  ptT_grass,
  ptT_water,
  ptT_sand,
  ptT_sapling,
  ptT_tree,
  ptT_trunk,
  ptT_wood,

  _ptt_multi_types,
  _ptT_nutrition_start = _ptt_multi_types,
  ptT_vitamin = _ptT_nutrition_start,
  ptT_protein,
  ptT_carbohydrates,
  ptT_fat,
  _ptT_nutrition_end = ptT_fat, // always +1 for nutrition_count!

  ptT_collidable,

  ptT_Count,
};

enum resource_type
{
  tT_grass,
  tT_water,
  tT_sand,
  tT_sapling,
  tT_tree,
  tT_trunk, // we probably don't need trunk as resource, as we just walk to the sawmill, once the tree is felled...
  tT_wood,

  _tile_type_food_begin, 
  tT_tomato = _tile_type_food_begin,
  tT_bean,
  tT_wheat,
  tT_sunflower,
  tT_meal, // insert meal types here
  _tile_type_food_last = tT_meal,

  tT_mountain,

  tT_count,
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

struct pathfinding_info
{
  direction dir;
  uint8_t dist;
};

struct pathfinding_element // hmm i don't like this name
{
  uint8_t elevationLevel;
};

// TODO: gameplay element: ressource count, bool house?
struct gameplay_element
{
  resource_type tileType;
  size_t ressourceCount;
  size_t maxRessourceCount;
  bool hasHouse;
};

// TODO: render element: texture, height, etc
struct render_element
{
  // texture
  // rotation
  uint8_t elevationLevel;
};
// render map

struct fill_step
{
  size_t index;
  size_t dist;

  inline fill_step() = default;
  inline fill_step(const size_t idx, const size_t dist) : index(idx), dist(dist) { lsAssert(index < 16 * 16); } // Change if map size changes!
};

struct level_info
{
  struct resource_info
  {
    queue<fill_step> pathfinding_queue;
    pathfinding_info *pDirectionLookup[2] = {}; // add elevation level to this map
    size_t write_direction_idx = 0;
  } resources[ptT_Count];

  pathfinding_element *pPathfindingMap = nullptr; // don't needed anymore as we will conclude from the ressurce type to the pathfinfing target type in templated func (see notes)
  gameplay_element *pGameplayMap = nullptr; // change name, but like that?
  render_element *pRenderMap = nullptr; // change name, but like that?
  vec2s map_size;
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
  pathfinding_target_type target;
  bool atDestination = false;
  vec2f direction = vec2f(0);
  size_t lastTickTileIdx = 0;
};

struct lifesupport_actor
{
  entity_type type;
  size_t entityIndex;
  uint8_t nutritions[(_ptT_nutrition_end + 1) - _ptT_nutrition_start];
  uint8_t lunchbox[(_tile_type_food_last + 1) - _tile_type_food_begin]; // either this simply counts the amount of nutrition and we eat it as we need it or it contains different foods?
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

  size_t tickRate = 60;
  size_t lastEventIndex, eventUpdateCutoffIndex;
};

size_t worldPosToTileIndex(vec2f pos);

lsResult game_init();
lsResult game_tick();

void game_setPlayerMapIndex(const bool left);
void game_playerSwitchTiles(const resource_type terrainType);

game *game_getGame();

bool game_hasAnyEvent(game *pGame);
gameEvent game_getNextEvent(game *pGame);
size_t game_getTickRate();
size_t game_getLevelSize();

lsResult game_serialize(_Out_ data_blob *pBlob);
lsResult game_deserialze(_Out_ game *pGame, _In_ const void *pData, const size_t size);
