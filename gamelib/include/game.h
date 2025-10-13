#pragma once

#include "core.h"
#include "pool.h"
#include "queue.h"
#include "list.h"
#include "local_list.h"

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
  ptT_market,

  _ptT_nutrient_sources_first, // nutrient sources match the corresponding nutritient! (e.g.: tomato plant + (nutrition first - nutrient sources first) = vitamin)
  ptT_tomato_plant = _ptT_nutrient_sources_first,
  ptT_bean_plant,
  ptT_wheat_plant,
  ptT_sunflower_plant,
  _ptT_nutrient_sources_last = ptT_sunflower_plant, // up until here (including) the types match the corresponding `resource_types`!
  // do not insert anything here! We depend on the plants matching up with their nutrients!
  _ptt_multi_types, // from here on no longer necessarily matching up with `resource_type`s!
  _ptT_nutrient_first = _ptt_multi_types,
  ptT_vitamin = _ptT_nutrient_first,
  ptT_protein,
  ptT_carbohydrates,
  ptT_fat,
  _ptT_nutrient_last = ptT_fat,

  _ptT_drop_off_first,
  ptT_tomato_drop_off = _ptT_drop_off_first, // order matches resource_type food types
  ptT_bean_drop_off, // order matches resource_type food types
  ptT_wheat_drop_off, // order matches resource_type food types
  ptT_sunflower_drop_off, // order matches resource_type food types
  ptT_meal_drop_off, // order matches resource_type food types
  _ptT_drop_off_last = ptT_meal_drop_off,

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
  tT_market,

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

static constexpr uint8_t MaxFireResourceCount = 9;
static constexpr uint8_t MaxFoodItemResourceCount = 255;
static const uint8_t MaxResourceCounts[] = { 1, 1, 1, 1, 1, 1, 1, 4, MaxFireResourceCount, MaxFireResourceCount, 0 /*tT_market does not have a count*/, 12, 12, 12, 12, MaxFoodItemResourceCount, MaxFoodItemResourceCount, MaxFoodItemResourceCount, MaxFoodItemResourceCount, MaxFoodItemResourceCount, 1 };
static_assert(LS_ARRAYSIZE(MaxResourceCounts) == tT_count);

struct gameplay_element
{
  resource_type tileType;
  uint8_t resourceCount;
  uint8_t maxResourceCount;
  int16_t multiResourceCountIndex = -1;
  uint8_t tileStatus = 0; // can refer to different properties depending on the type of tile (e.g. growth status)
    
  //bool hasHouse;

  gameplay_element() = default;
  gameplay_element(const resource_type type, const uint8_t count, const int16_t multiResourceCountIndex = -1) : tileType(type), resourceCount(count), multiResourceCountIndex(multiResourceCountIndex)
  {
    lsAssert(type < tT_count);
    maxResourceCount = MaxResourceCounts[type];
  }
};

// TODO: maybe we want to have a struct like: 
struct gamplay_element_transition
{
  size_t tileIndex;
  uint16_t ticksUntilTransition;
  // a lambda for what to do when the timer ran out? ask coc...
};

// that we put into a list if it needs to be processed, then we call a function that will handle the waiting and increment the tiles state once the wait time is 0
// never mind it won't always be for incrementing a state but to e.g. remove wood from the fire resourceCount... so we would need to reference what needs to be done? it could just be by a big ol switch case which defines what happens for which tile, or by giving it a function as to what needs to be done. or fire just has different states?

// how to make clear which tileType and tileStatus mean what?
// enum plantState
// ah i'm not even sure anymore that we want tileStates. it would be way easier to read if we just add more resource_types. and waiting is just for something like fires where resources will be removed...

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
  eT_farmer,
  eT_cook,
  eT_fire_actor,

  eT_count
};

struct movement_actor
{
  vec2f pos;
  pathfinding_target_type target;
  bool atDestination = false;
  bool atDestinationLastTick = false;
  vec2f direction = vec2f(0);
  size_t lastTickTileIdx = 0;
  bool enteredDifferentTileLastTick = false;
  bool survivalActorActive = false;
  bool isWaiting = false;
  uint16_t ticksToWait = 0;
};

struct lifesupport_actor
{
  entity_type type;
  size_t entityIndex;
  uint8_t nutritions[(_ptT_nutrient_last + 1) - _ptT_nutrient_first];
  uint8_t lunchbox[(_tile_type_food_last + 1) - _tile_type_food_first];
  uint8_t temperature;
};

struct actor // TODO: add entity type here as well and let the ls actor inheret from it?
{
  size_t index;
};

//////////////////////////////////////////////////////////////////////////

enum lumberjack_actor_state
{
  laS_plant,
  laS_getWater,
  laS_water,
  laS_chop,
  laS_cut,
  laS_drop_off,

  laS_count
};

struct lumberjack_actor : actor
{
  lumberjack_actor_state state;
  bool hasItem; // Maybe change this to several inevntory spots if needed.
  resource_type item;
};

//////////////////////////////////////////////////////////////////////////

struct farmer_actor : actor {};

//////////////////////////////////////////////////////////////////////////

// only temporary
enum cook_actor_state
{
  caS_check_inventory,
  caS_harvest,
  caS_cook,

  caS_count
};

struct cook_actor : actor
{
  cook_actor_state state; // TODO: Starting State in the actor?
  resource_type currentCookingItem;
  uint8_t inventory[(_ptT_nutrient_last + 1) - _ptT_nutrient_first];
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

struct fire_actor : actor
{
  fire_actor_state state;
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

  list<local_list<uint8_t, tT_count>> multiResourceCounts; // We must *never* delete anything from this list, as the indizes would change otherwise!
  // TODO: the list should propably only include the resource_types up until tT_multi_types as e.g. meals cannot be pathfound to anyways.

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
