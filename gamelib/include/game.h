#pragma once

#include "core.h"
#include "pool.h"
#include "queue.h"

//////////////////////////////////////////////////////////////////////////

enum pathfinding_target_type : uint8_t
{
  // all values match their corresponding `resource_type` up until `_ptt_multi_types`
  ptT_grass,
  ptT_soil,
  ptT_water,
  ptT_sand,
  ptT_sapling,
  ptT_tree,
  ptT_trunk,
  ptT_wood,
  ptT_fire,
  ptT_fire_pit,

  _ptT_nutrient_sources_first, // nutrient sources match the corresponding nutritient! (e.g.: tomato plant + (nutrition first - nutrient sources first) = vitamin)
  ptT_tomato_plant = _ptT_nutrient_sources_first,
  ptT_bean_plant,
  ptT_wheat_plant,
  ptT_sunflower_plant,
  _ptT_nutrient_sources_last = ptT_sunflower_plant, // up until here (including) the types match the corresponding `resource_types`!
  // do not insert anything here! We depend on the plants matching up with their nutrients!
  _ptt_multi_types, // from here on no longer necessarily matching up with `resource_type`s!
  _ptT_nutrition_first = _ptt_multi_types,
  ptT_vitamin = _ptT_nutrition_first,
  ptT_protein,
  ptT_carbohydrates,
  ptT_fat,
  _ptT_nutrition_last = ptT_fat,

  ptT_collidable, // ptT_collidable always has to be last!

  ptT_Count
};

enum resource_type : uint8_t
{
  // all values match their corresponding `pathfinding_target_type` up until `_tile_type_food_first`
  tT_grass,
  tT_soil,
  tT_water,
  tT_sand,
  tT_sapling,
  tT_tree,
  tT_trunk,
  tT_wood,
  tT_fire,
  tT_fire_pit,

  _tile_type_food_resources_first, // food resources match the corresponding food! (e.g.: tomato plant + (food first - food resources first) = tomato)
  tT_tomato_plant = _tile_type_food_resources_first, // same order as the food
  tT_bean_plant, // same order as the food
  tT_wheat_plant, // same order as the food
  tT_sunflower_plant, // same order as the food
  _tile_type_food_resources_last = tT_sunflower_plant, // up until here (including) the types match the corresponding `pathfinding_target_type`!
  // do not insert anything here! We depend on the plants matching up with their nutrients!
  _tile_type_multi_types,// from here on no longer necessarily matching up with `pathfinding_target_type`s!
  _tile_type_food_first = _tile_type_multi_types, // food resources match the corresponding food! (e.g.: tomato plant + (food first - food resources first) = tomato)
  tT_tomato = _tile_type_food_first, // same order as the food resources
  tT_bean, // same order as the food resources
  tT_wheat, // same order as the food resources
  tT_sunflower, // same order as the food resources
  tT_meal, // insert meal types here
  _tile_type_food_last = tT_meal,

  tT_mountain,

  tT_count
};

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

static constexpr uint8_t FireMax = 9;
static const uint8_t MaxResourceCounts[] = { 1, 1, 1, 0, 1, 1, 1, 4, FireMax, FireMax, 12, 12, 12, 12, 192, 192, 192, 192, 192, 1};
static_assert(LS_ARRAYSIZE(MaxResourceCounts) == tT_count);

struct gameplay_element
{
  resource_type tileType;
  uint8_t resourceCount;
  uint8_t maxResourceCount;
  //bool hasHouse;

  gameplay_element() = default;
  gameplay_element(const resource_type type, const uint8_t count) : tileType(type), resourceCount(count), maxResourceCount(MaxResourceCounts[type]) {}
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
  eT_fire_actor,

  eT_count
};

struct movement_actor
{
  vec2f pos;
  pathfinding_target_type target;
  bool atDestination = false;
  vec2f direction = vec2f(0);
  size_t lastTickTileIdx = 0;
  bool enteredNewTileLastTick = false;
  bool survivalActorActive = false;
};

struct lifesupport_actor
{
  entity_type type;
  size_t entityIndex;
  uint8_t nutritions[(_ptT_nutrition_last + 1) - _ptT_nutrition_first];
  uint8_t lunchbox[(_tile_type_food_last + 1) - _tile_type_food_first];
  uint8_t temperature;
};

//////////////////////////////////////////////////////////////////////////

enum lumberjack_actor_state
{
  laS_plant,
  laS_getWater,
  laS_water,
  laS_chop,
  laS_cut,

  laS_count
};

struct lumberjack_actor
{
  lumberjack_actor_state state;
  size_t index;
  bool hasItem; // Maybe change this to several inevntory spots if needed.
  resource_type item;
};

//////////////////////////////////////////////////////////////////////////

enum farmer_actor_state // actually not necessary to have a state if there's only one...
{
  faaS_plant,
  // maybe add watering in the future
  faaS_count
};

struct farmer_actor
{
  farmer_actor_state state; // TODO: Starting State in the actor?
  size_t index;
};

//////////////////////////////////////////////////////////////////////////

// only temporary
enum cook_actor_state
{
  caS_check_inventory,
  caS_plant,
  caS_harvest,
  caS_cook,

  caS_count
};

struct cook_actor
{
  cook_actor_state state; // TODO: Starting State in the actor?
  size_t index;
  resource_type currentCookingItem;
  pathfinding_target_type searchingPlant; // actor's don't always search for items so this won't always represent what the actor is currently up to.
  uint8_t inventory[(_ptT_nutrition_last + 1) - _ptT_nutrition_first];
};

//////////////////////////////////////////////////////////////////////////

enum fire_actor_state
{
  faS_get_wood,
  faS_start_fire,
  faS_get_water,
  faS_extinguish_fire,

  faS_count
};

struct fire_actor
{
  fire_actor_state state;
  size_t index;
  uint8_t wood_inventory;
  uint8_t water_inventory;
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

  bool isNight = false;
  vec2i16 playerPos;

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

  level_info levelInfo;
  pool<movement_actor> movementActors;
  pool<lifesupport_actor> lifesupportActors;

  size_t tickRate = 60;
};

lsResult game_init();
lsResult game_tick();

void game_setPlayerMapIndex(const direction dir);
void game_playerSwitchTiles(const resource_type terrainType);

game *game_getGame();
size_t game_getTickRate();
