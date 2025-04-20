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

//////////////////////////////////////////////////////////////////////////

enum pathfinding_target_type // 32 different terrain_types possible.
{
  ptT_grass, // matches the corresponding `resource_type`
  ptT_water, // matches the corresponding `resource_type`
  ptT_sand, // matches the corresponding `resource_type`
  ptT_sapling, // matches the corresponding `resource_type`
  ptT_tree, // matches the corresponding `resource_type`
  ptT_trunk, // matches the corresponding `resource_type`
  ptT_wood, // matches the corresponding `resource_type`

  _ptT_nutrient_sources_first,
  ptT_tomato_plant = _ptT_nutrient_sources_first,
  ptT_bean_plant,
  ptT_wheat_plant,
  ptT_sunflower_plant,
  _ptT_nutrient_sources_last = ptT_sunflower_plant,
  // do not insert anything here!
  _ptt_multi_types, // up until here the types match the corresponding `resource_types`
  _ptT_nutrition_first = _ptt_multi_types,
  ptT_vitamin = _ptT_nutrition_first,
  ptT_protein,
  ptT_carbohydrates,
  ptT_fat,
  _ptT_nutrition_last = ptT_fat, // always +1 for nutrition_count!

  ptT_collidable, // ptT_collidable always has to be last!

  ptT_Count,
};

enum resource_type
{
  tT_grass, // matches the corresponding `pathfinding_target_type`
  tT_water, // matches the corresponding `pathfinding_target_type`
  tT_sand, // matches the corresponding `pathfinding_target_type`
  tT_sapling, // matches the corresponding `pathfinding_target_type`
  tT_tree, // matches the corresponding `pathfinding_target_type`
  tT_trunk, // matches the corresponding `pathfinding_target_type`
  tT_wood, // matches the corresponding `pathfinding_target_type`

  _tile_type_food_resources_first,
  tT_tomato_plant = _tile_type_food_resources_first,
  tT_bean_plant,
  tT_wheat_plant,
  tT_sunflower_plant,
  _tile_type_food_resources_last = tT_sunflower_plant,
  // do not insert anything here!
  _tile_type_food_first,
  tT_tomato = _tile_type_food_first,
  tT_bean,
  tT_wheat,
  tT_sunflower,
  tT_meal, // insert meal types here
  _tile_type_food_last = tT_meal,

  tT_mountain,

  tT_count,
};

// food thoughts:
// there should be the nutrient sources and the differnt foods
// e.g. sunflower -> sunflower seed sandwich (idk lol)
// so the cook will walk to different sources and then put them together to food at a kitchen.
// i'm sorry i can't concentrate and want to commit at least something
// maybe some food can be eaten directly like tomatoes and other stuff has to be in meals, like salt?

// TODO: fires to go to in the dark?

//////////////////////////////////////////////////////////////////////////

enum direction : uint8_t
{
  d_unreachable,

  d_topRight,
  d_right,
  d_bottomRight,
  d_bottomLeft,
  d_left,
  d_topLeft,

  d_atDestination,
  d_unfillable,
};

struct pathfinding_info
{
  direction dir;
  uint8_t dist;
};

struct fill_step
{
  size_t index;
  size_t dist;

  inline fill_step() = default;
  inline fill_step(const size_t idx, const size_t dist) : index(idx), dist(dist) { lsAssert(index < 16 * 16); } // Change if map size changes!
};

//////////////////////////////////////////////////////////////////////////

struct pathfinding_element
{
  uint8_t elevationLevel;
};

struct gameplay_element
{
  resource_type tileType;
  uint8_t ressourceCount;
  const uint8_t maxRessourceCount = 16;
  bool hasHouse;
};

// TODO: render element: texture, height, etc
struct render_element
{
  // texture
  // rotation
  uint8_t elevationLevel;
};

//////////////////////////////////////////////////////////////////////////

enum entity_type
{
  eT_lumberjack,
  eT_stonemason,
  eT_cook,

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
  uint8_t nutritions[(_ptT_nutrition_last + 1) - _ptT_nutrition_first];
  uint8_t lunchbox[(_tile_type_food_last + 1) - _tile_type_food_first];
};

//////////////////////////////////////////////////////////////////////////

enum lumberjack_actor_state
{
  laS_plant,
  laS_getWater,
  laS_water,
  laS_chop,
  laS_cut,

  laS_count,
};

struct lumberjack_actor
{
  lumberjack_actor_state state;
  size_t index;
  bool hasItem; // Maybe change this to several inevntory spots if needed.
  resource_type item;
};

//////////////////////////////////////////////////////////////////////////

// only temporary
enum cook_actor_state
{
  caS_check_inventory,
  caS_plant,
  caS_getWater,
  caS_water,
  caS_harvest_plant,
  caS_cook,

  caS_count,
};

struct cook_actor
{
  cook_actor_state state; // TODO: Starting State and maybe the look up in the actor?
  resource_type currentCookingItem;
  size_t index;
  uint8_t inventory[(_ptT_nutrition_last + 1) - _ptT_nutrition_first];
};

//////////////////////////////////////////////////////////////////////////

struct level_info
{
  struct resource_info
  {
    queue<fill_step> pathfinding_queue;
    pathfinding_info *pDirectionLookup[2] = {};
    size_t write_direction_idx = 0;
  } resources[ptT_Count - 1]; // Skipping ptT_collidable - ptT_collidable always has to be last!

  pathfinding_element *pPathfindingMap = nullptr;
  gameplay_element *pGameplayMap = nullptr;
  render_element *pRenderMap = nullptr;
  vec2s map_size;
};

size_t worldPosToTileIndex(vec2f pos);

//////////////////////////////////////////////////////////////////////////

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
