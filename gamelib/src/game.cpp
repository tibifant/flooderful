#include "game.h"

#include "box2d/box2d.h"

//////////////////////////////////////////////////////////////////////////

static game _Game;

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local();
lsResult game_tick_local();

//////////////////////////////////////////////////////////////////////////

constexpr size_t _FloodFillSteps = 100;

//////////////////////////////////////////////////////////////////////////

static pool<lumberjack_actor> _LumberjackActors; // Do we need this in the header?
static pool<farmer_actor> _FarmerActors; // Do we need this in the header?
static pool<cook_actor> _CookActors; // Do we need this in the header?
static pool<fire_actor> _FireActors; // Do we need this in the header?

//////////////////////////////////////////////////////////////////////////

template <pathfinding_target_type p>
struct match_resource;

template<>
struct match_resource<pathfinding_target_type::ptT_grass>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_grass; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_grass) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_soil>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_soil; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_soil) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_water>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_water; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_water) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sand>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_sand; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_sand) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sapling>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_sapling; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_sapling) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_tree>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_tree; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_tree) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_trunk>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_trunk; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_trunk) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_wood>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_wood; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_wood) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_fire>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_fire; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_fire) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_fire_pit>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_fire_pit; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_fire_pit) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_tomato_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_tomato_plant; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_tomato_plant) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_bean_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_bean_plant; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_bean_plant) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_wheat_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_wheat_plant; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_wheat_plant) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sunflower_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_sunflower_plant; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_sunflower_plant) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_vitamin>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { return resourceType == tT_tomato || (resourceType == tT_meal && count > 0); };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_tomato) > 0 || *local_list_get(pCounts, tT_meal) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_protein>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { return resourceType == tT_bean || (resourceType == tT_meal && count > 0); };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_bean) > 0 || *local_list_get(pCounts, tT_meal) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_carbohydrates>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { return resourceType == tT_wheat || (resourceType == tT_meal && count > 0); };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_wheat) > 0 || *local_list_get(pCounts, tT_meal) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_fat>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { return resourceType == tT_sunflower || (resourceType == tT_meal && count > 0); };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_sunflower) > 0 || *local_list_get(pCounts, tT_meal) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_tomato_drop_off>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_tomato; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_tomato) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_bean_drop_off>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_bean; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_bean) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_wheat_drop_off>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_wheat; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_wheat) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sunflower_drop_off>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_sunflower; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_sunflower) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_meal_drop_off>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_meal; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { return *local_list_get(pCounts, tT_meal) > 0; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_market>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_market; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { (void)pCounts; return true; }; // Only correct as long as markets are the onlty multi resource tiles!
};

template<>
struct match_resource<pathfinding_target_type::ptT_collidable>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType, const uint8_t count) { (void)count; return resourceType == tT_mountain; };
  FORCEINLINE static bool resourceAttribute_matches_resource(const local_list<uint8_t, tT_count> *pCounts) { (void)pCounts; return false; }; // Multi tiles should never be collidable 
};

template<pathfinding_target_type p>
void fill_resource_info(pathfinding_info *pDirectionLookup, queue<fill_step> &pathfindQueue, gameplay_element *pMap)
{
  lsZeroMemory(pDirectionLookup, _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
  {
    const gameplay_element e = pMap[i];

    if ((e.resourceCountIndex > -1 && match_resource<p>::resourceAttribute_matches_resource(list_get(&_Game.levelInfo.multiResourceCounts, e.resourceCountIndex))) || (match_resource<p>::resourceAttribute_matches_resource(e.tileType, e.resourceCount)))
    {
      queue_pushBack(&pathfindQueue, fill_step(i, 0));
      pDirectionLookup[i].dir = d_atDestination;
    }
    else
    {
      pDirectionLookup[i].dir = (direction)(d_unfillable * (direction)(match_resource<ptT_collidable>::resourceAttribute_matches_resource(e.tileType, e.resourceCount)));
    }
  }
}

void rebuild_resource_info(pathfinding_info *pDirectionLookup, queue<fill_step> &pathfindQueue, gameplay_element *pResourceMap, const pathfinding_target_type type)
{
  switch (type)
  {
  case ptT_grass: fill_resource_info<ptT_grass>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_soil: fill_resource_info<ptT_soil>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_water: fill_resource_info<ptT_water>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_sand: fill_resource_info<ptT_sand>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_sapling: fill_resource_info<ptT_sapling>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_tree: fill_resource_info<ptT_tree>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_trunk: fill_resource_info<ptT_trunk>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_wood: fill_resource_info<ptT_wood>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_fire: fill_resource_info<ptT_fire>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_fire_pit: fill_resource_info<ptT_fire_pit>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_tomato_plant: fill_resource_info<ptT_tomato_plant>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_bean_plant: fill_resource_info<ptT_bean_plant>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_wheat_plant: fill_resource_info<ptT_wheat_plant>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_sunflower_plant: fill_resource_info<ptT_sunflower_plant>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_vitamin: fill_resource_info<ptT_vitamin>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_protein: fill_resource_info<ptT_protein>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_carbohydrates: fill_resource_info<ptT_carbohydrates>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_fat: fill_resource_info<ptT_fat>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_tomato_drop_off: fill_resource_info<ptT_tomato_drop_off>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_bean_drop_off: fill_resource_info<ptT_bean_drop_off>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_wheat_drop_off: fill_resource_info<ptT_wheat_drop_off>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_sunflower_drop_off: fill_resource_info<ptT_sunflower_drop_off>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_meal_drop_off: fill_resource_info<ptT_meal_drop_off>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_market: fill_resource_info<ptT_market>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  case ptT_collidable: fill_resource_info<ptT_collidable>(pDirectionLookup, pathfindQueue, pResourceMap); break;
  default: lsFail(); // not implemented.
  }
}

//////////////////////////////////////////////////////////////////////////

void mapInit(const size_t width, const size_t height/*, bool *pCollidableMask*/)
{
  _Game.levelInfo.map_size = { width, height };

  lsAllocZero(&_Game.levelInfo.pPathfindingMap, height * width);
  lsAlloc(&_Game.levelInfo.pGameplayMap, height * width);
  //lsAllocZero(&_Game.levelInfo.pRenderMap, height * width);
}

void setMapBorder()
{
  // Setting borders to ptT_collidable
  {
    for (size_t y = 0; y < _Game.levelInfo.map_size.y; y++)
    {
      _Game.levelInfo.pGameplayMap[y * _Game.levelInfo.map_size.x] = gameplay_element(tT_mountain, 0);
      _Game.levelInfo.pGameplayMap[(y + 1) * _Game.levelInfo.map_size.x - 1] = gameplay_element(tT_mountain, 0);
    }

    for (size_t x = 0; x < _Game.levelInfo.map_size.x; x++)
    {
      _Game.levelInfo.pGameplayMap[x] = gameplay_element(tT_mountain, 0);
      _Game.levelInfo.pGameplayMap[x + (_Game.levelInfo.map_size.y - 1) * _Game.levelInfo.map_size.x] = gameplay_element(tT_mountain, 0);
    }
  }
}

lsResult setGameplayTile(const size_t index, const resource_type type, const uint8_t resourceCount)
{
  lsResult result = lsR_Success;

  lsAssert(type < tT_count);
  lsAssert(index < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(resourceCount <= MaxResourceCounts[type]);

  int16_t resourceCountIndex = -1;

  if (type == tT_market)
  {
    lsAssert(_Game.levelInfo.multiResourceCounts.count < lsMaxValue<int16_t>());

    local_list<uint8_t, tT_count> l;

    for (size_t i = 0; i < tT_count; i++)
      LS_ERROR_CHECK(local_list_add(&l, (uint8_t)0));

    LS_ERROR_CHECK(list_add(_Game.levelInfo.multiResourceCounts, l));
    resourceCountIndex = (int16_t)_Game.levelInfo.multiResourceCounts.count - 1;
  }

  _Game.levelInfo.pGameplayMap[index] = gameplay_element(type, resourceCount, resourceCountIndex);

epilogue:
  return result;
}

lsResult setTile(const size_t index, const resource_type type, const uint8_t resourceCount, const uint8_t elevationLevel)
{
  lsAssert(index < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  _Game.levelInfo.pPathfindingMap[index].elevationLevel = elevationLevel;

  return setGameplayTile(index, type, resourceCount);
}

lsResult fillTerrain(const resource_type type)
{
  lsResult result = lsR_Success;

  lsAssert(type < tT_count);

  rand_seed seed = rand_seed(2, 2);

  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
    LS_ERROR_CHECK(setTile(i, type, MaxResourceCounts[type], lsGetRand(seed) % 3));

  setMapBorder();

epilogue:
  return result;
}

lsResult setTerrain()
{
  lsResult result = lsR_Success;

  // TODO: Terrain Generation

  rand_seed seed = rand_seed(2, 2); // 2, 2 ist cool am anfang.

  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
  {
    const resource_type type = (lsGetRand(seed) & 15) < 12 ? tT_grass : tT_mountain;
    //_Game.levelInfo.pGameplayMap[i].tileType = (lsGetRand(seed) & 15) < 1 ? tT_soil : _Game.levelInfo.pGameplayMap[i].tileType;
    LS_ERROR_CHECK(setTile(i, type, 1, lsGetRand(seed) % 3));
  }

  for (size_t i = 0; i < 20; i++)
    _Game.levelInfo.pGameplayMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)] = gameplay_element(tT_soil, 1);

  for (size_t i = 0; i < 3; i++)
  {
    _Game.levelInfo.pGameplayMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)] = gameplay_element(tT_sand, 1);
    _Game.levelInfo.pGameplayMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)] = gameplay_element(tT_water, 1);
  }

  //for (size_t i = _tile_type_food_first; i < _tile_type_food_last; i++) // without tt_meal for testing
  //{
  //  const size_t index = i + 5;
  //  _Game.levelInfo.pGameplayMap[index].tileType = resource_type(i);
  //  _Game.levelInfo.pGameplayMap[index].ressourceCount = 64;
  //  _Game.levelInfo.pPathfindingMap[index].elevationLevel = 1;
  //}

  //_Game.levelInfo.pGameplayMap[120].tileType = tT_fire;
  //_Game.levelInfo.pGameplayMap[120].ressourceCount = 255;
  _Game.levelInfo.pGameplayMap[121] = gameplay_element(tT_fire_pit, 4); // TODO: do i even want to change these?
  _Game.levelInfo.pGameplayMap[132] = gameplay_element(tT_fire_pit, 4);
  _Game.levelInfo.pGameplayMap[145] = gameplay_element(tT_fire_pit, 4);

  // TODO: food drop off!
  _Game.levelInfo.pGameplayMap[213] = gameplay_element(tT_tomato, 4);
  _Game.levelInfo.pGameplayMap[214] = gameplay_element(tT_bean, 4);
  _Game.levelInfo.pGameplayMap[215] = gameplay_element(tT_wheat, 4);
  _Game.levelInfo.pGameplayMap[216] = gameplay_element(tT_sunflower, 4);
  _Game.levelInfo.pGameplayMap[217] = gameplay_element(tT_meal, 4);

  LS_ERROR_CHECK(setGameplayTile(8 * 8 + 8, tT_market, 0));

  setMapBorder();

epilogue:
  return result;
}

lsResult spawnActors() // TODO: clean up with a function `spawnActor(type, pos, _Out_ index)`
{
  lsResult result = lsR_Success;

  // Lumberjacks
  //for (size_t i = 0; i < 5; i++)
  {
    movement_actor actor;
    actor.target = ptT_sapling; //(terrain_type)(lsGetRand() % (tT_Count - 1));
    //actor.pos = vec2f((float_t)((1 + i * 3) % _Game.levelInfo.map_size.x), (float_t)((i * 3 + 1) % _Game.levelInfo.map_size.y));
    actor.pos = vec2f((float_t)(4 % _Game.levelInfo.map_size.x), (float_t)(4 % _Game.levelInfo.map_size.y));

    //while (_Game.levelInfo.pGameplayMap[worldPosToTileIndex(actor.pos)].tileType == tT_mountain)
    //  actor.pos.x = (float_t)(size_t(actor.pos.x + 1) % _Game.levelInfo.map_size.x);

    size_t index;
    LS_ERROR_CHECK(pool_add(&_Game.movementActors, &actor, &index));

    lumberjack_actor lj_actor;
    lj_actor.state = laS_plant;
    lj_actor.index = index;
    lj_actor.hasItem = false;

    LS_ERROR_CHECK(pool_insertAt(&_LumberjackActors, &lj_actor, index));

    lifesupport_actor ls_actor;
    ls_actor.entityIndex = index;
    ls_actor.type = eT_lumberjack;
    ls_actor.temperature = 255;

    lsZeroMemory(ls_actor.nutritions, LS_ARRAYSIZE(ls_actor.nutritions));
    lsZeroMemory(ls_actor.lunchbox, LS_ARRAYSIZE(ls_actor.lunchbox));

    LS_ERROR_CHECK(pool_insertAt(&_Game.lifesupportActors, &ls_actor, index));
  }

  // Farmer
  {
    movement_actor actor;
    actor.target = ptT_soil;
    actor.pos = vec2f(12.f, 10.f);

    size_t f_index;
    LS_ERROR_CHECK(pool_add(&_Game.movementActors, actor, &f_index));

    farmer_actor f;
    f.index = f_index;

    LS_ERROR_CHECK(pool_insertAt(&_FarmerActors, f, f_index));

    lifesupport_actor ls_actor;
    ls_actor.entityIndex = f_index;
    ls_actor.type = eT_farmer;
    ls_actor.temperature = 255;

    lsZeroMemory(ls_actor.nutritions, LS_ARRAYSIZE(ls_actor.nutritions));
    lsZeroMemory(ls_actor.lunchbox, LS_ARRAYSIZE(ls_actor.lunchbox));

    LS_ERROR_CHECK(pool_insertAt(&_Game.lifesupportActors, &ls_actor, f_index));
  }

  // Cook
  {
    movement_actor foodActor;
    foodActor.target = _ptT_nutrient_first;
    foodActor.pos = vec2f(12.f, 10.f);

    size_t f_index;
    LS_ERROR_CHECK(pool_add(&_Game.movementActors, foodActor, &f_index));

    cook_actor cook;
    cook.state = caS_check_inventory;
    cook.currentCookingItem = tT_tomato;
    lsZeroMemory(cook.inventory, LS_ARRAYSIZE(cook.inventory));

    cook.index = f_index;

    lsZeroMemory(cook.inventory, LS_ARRAYSIZE(cook.inventory));

    LS_ERROR_CHECK(pool_insertAt(&_CookActors, cook, f_index));

    lifesupport_actor ls_actor;
    ls_actor.entityIndex = f_index;
    ls_actor.type = eT_cook;
    ls_actor.temperature = 255;

    lsZeroMemory(ls_actor.nutritions, LS_ARRAYSIZE(ls_actor.nutritions));
    lsZeroMemory(ls_actor.lunchbox, LS_ARRAYSIZE(ls_actor.lunchbox));

    LS_ERROR_CHECK(pool_insertAt(&_Game.lifesupportActors, &ls_actor, f_index));
  }

  // Fire Actor
  {
    movement_actor actor;
    actor.target = _ptT_nutrient_first;
    actor.pos = vec2f(13.f, 13.f);

    size_t f_index;
    LS_ERROR_CHECK(pool_add(&_Game.movementActors, actor, &f_index));

    fire_actor fireActor;
    fireActor.state = faS_start_fire;
    fireActor.wood_inventory = 0;
    fireActor.water_inventory = 0;

    fireActor.index = f_index;

    LS_ERROR_CHECK(pool_insertAt(&_FireActors, fireActor, f_index));

    lifesupport_actor ls_actor;
    ls_actor.entityIndex = f_index;
    ls_actor.type = eT_fire_actor;
    ls_actor.temperature = 255;

    lsZeroMemory(ls_actor.nutritions, LS_ARRAYSIZE(ls_actor.nutritions));
    lsZeroMemory(ls_actor.lunchbox, LS_ARRAYSIZE(ls_actor.lunchbox));

    LS_ERROR_CHECK(pool_insertAt(&_Game.lifesupportActors, &ls_actor, f_index));
  }

  goto epilogue;

epilogue:
  return result;
}

void initializeLevel()
{
  mapInit(16, 16);
  lsAssert(setTerrain() == lsR_Success);
  //fillTerrain(tT_grass);

  // Set up floodfill queue and lookup
  for (size_t i = 0; i < ptT_Count - 1; i++) // Skip ptT_collidable
  {
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[0], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[1], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

    rebuild_resource_info(_Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx], _Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.pGameplayMap, (pathfinding_target_type)(i));
  }

  lsAssert(spawnActors() == lsR_Success);
  _Game.levelInfo.playerPos = vec2i16((int16_t)(_Game.levelInfo.map_size.x * 0.5), (int16_t)(_Game.levelInfo.map_size.y * 0.5));
}

//////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG
__declspec(__forceinline)
#endif
void floodfill_suggestNextTarget(queue<fill_step> &pathfindQueue, pathfinding_info *pDirectionLookup, const size_t nextIndex, const direction dir, const uint8_t parentElevation, const size_t parentDist, const pathfinding_element *pPathfindingMap)
{
  direction nextTile = (direction)pDirectionLookup[nextIndex].dir;

  if (nextTile == d_unreachable && lsAbs((int8_t)parentElevation - (int8_t)pPathfindingMap[nextIndex].elevationLevel) <= 1)
  {
    pathfinding_info p;
    p.dir = dir;
    p.dist = (uint8_t)lsClamp((uint16_t)(parentDist + 1), (uint16_t)0, (uint16_t)255);
    pDirectionLookup[nextIndex] = p;
    queue_pushBack(&pathfindQueue, fill_step(nextIndex, p.dist));
  }
}

bool floodfill(queue<fill_step> &pathfindQueue, pathfinding_info *pDirectionLookup, const pathfinding_element *pPathfindingMap)
{
  fill_step current;
  size_t stepCount = 0;

  while (pathfindQueue.count)
  {
    if (stepCount > _FloodFillSteps)
      return false;

    queue_popFront(&pathfindQueue, &current);
    lsAssert(current.index >= 0 && current.index < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

    const size_t isOddBit = (current.index / _Game.levelInfo.map_size.x) & 1;
    const size_t topLeftIndex = current.index - _Game.levelInfo.map_size.x - (size_t)!isOddBit;
    const size_t bottomLeftIndex = current.index + _Game.levelInfo.map_size.x - (size_t)!isOddBit;

    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, current.index - 1, d_left, pPathfindingMap[current.index].elevationLevel, current.dist, pPathfindingMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, current.index + 1, d_right, pPathfindingMap[current.index].elevationLevel, current.dist, pPathfindingMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, bottomLeftIndex, d_bottomLeft, pPathfindingMap[current.index].elevationLevel, current.dist, pPathfindingMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, bottomLeftIndex + 1, d_bottomRight, pPathfindingMap[current.index].elevationLevel, current.dist, pPathfindingMap); // bottomRight and bottomLeft are flipped to not give the right side all the paths
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, topLeftIndex, d_topLeft, pPathfindingMap[current.index].elevationLevel, current.dist, pPathfindingMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, topLeftIndex + 1, d_topRight, pPathfindingMap[current.index].elevationLevel, current.dist, pPathfindingMap);

    stepCount++;
  }

  // if queue is empty: floodfill completed
  return true;
}

void updateFloodfill()
{
  for (size_t i = 0; i < ptT_Count - 1; i++) // Skip ptT_collidable
  {
    size_t writeIndex = _Game.levelInfo.resources[i].write_direction_idx;

    if (floodfill(_Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.resources[i].pDirectionLookup[writeIndex], _Game.levelInfo.pPathfindingMap))
    {
      lsAssert(!_Game.levelInfo.resources[i].pathfinding_queue.count);

      size_t newWriteIndex = 1 - writeIndex;
      _Game.levelInfo.resources[i].write_direction_idx = newWriteIndex;

      lsZeroMemory(_Game.levelInfo.resources[i].pDirectionLookup[newWriteIndex], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

      rebuild_resource_info(_Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx], _Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.pGameplayMap, (pathfinding_target_type)i);
    }
  }
}

//////////////////////////////////////////////////////////////////////////

size_t worldPosToTileIndex(const vec2f pos)
{
  const float_t x_even = lsFloor(lsClamp(pos.x, 0.f, (float_t)_Game.levelInfo.map_size.x) + 0.5f);
  const float_t y_even = lsFloor(lsClamp(pos.y, 0.f, (float_t)_Game.levelInfo.map_size.y) * 0.5f + 0.5f) * 2.f;

  const float_t x_odd = lsFloor(lsClamp(pos.x, 0.f, (float_t)_Game.levelInfo.map_size.x)); // x_even is shifted by -0.5 compared to the world pos.
  const float_t y_odd = lsFloor(lsClamp((pos.y - 1) * 0.5f, 0.f, (float_t)_Game.levelInfo.map_size.y) + 0.5f) * 2.f + 1.f;

  //const float_t dist_even = lsAbs((x_even + y_even) - (pos.x + pos.y));
  //const float_t dist_odd = lsAbs((x_odd + 0.5f + y_odd) - (pos.x + pos.y));

  const float_t dist_even = vec2f(x_even - pos.x, y_even - pos.y).LengthSquared();
  const float_t dist_odd = vec2f(x_odd - (pos.x - 0.5f), y_odd - pos.y).LengthSquared();

  if (dist_even < dist_odd)
    return (size_t)lsRound(y_even * _Game.levelInfo.map_size.x + x_even);
  else
    return (size_t)lsRound(y_odd * _Game.levelInfo.map_size.x + x_odd);
}

vec2f tileIndexToWorldPos(const size_t tileIndex)
{
  const size_t x_center = tileIndex % _Game.levelInfo.map_size.x;
  const size_t y_center = tileIndex / _Game.levelInfo.map_size.x;

  vec2f tilePos = vec2f(vec2s(x_center, y_center));

  if (y_center & 1) // every second row is shifted by + 0.5
    tilePos.x += 0.5f;

  return tilePos;
}

static size_t r = 0;

void movementActor_move()
{
  r = (r + 1) & 63;

  for (auto _actor : _Game.movementActors)
  {
    // Reset lastTile every so often to handle map changes.
    if ((_actor.index & 63) == r)
      _actor.pItem->lastTickTileIdx = (size_t)(_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y * 0.5);

    const size_t currentTileIdx = worldPosToTileIndex(_actor.pItem->pos);
    const level_info::resource_info &info = _Game.levelInfo.resources[_actor.pItem->target];
    const direction currentTileDirectionType = info.pDirectionLookup[1 - info.write_direction_idx][currentTileIdx].dir;

    lsAssert(currentTileIdx != 0 && currentTileIdx < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
    lsAssert(_actor.pItem->pos.x > 0 && _actor.pItem->pos.x < _Game.levelInfo.map_size.x && _actor.pItem->pos.y > 0 && _actor.pItem->pos.y < _Game.levelInfo.map_size.y);

    if (currentTileDirectionType == d_unreachable)
    {
      continue;
    }
    else if (currentTileDirectionType == d_atDestination)
    {
      _actor.pItem->atDestination = true;
      _actor.pItem->direction = vec2f(0);
      continue;
    }

    if (currentTileIdx != _actor.pItem->lastTickTileIdx)
    {
      if (currentTileDirectionType == d_unfillable)
      {
        _actor.pItem->direction = (tileIndexToWorldPos(_actor.pItem->lastTickTileIdx) - _actor.pItem->pos).Normalize();
      }
      else
      {
        const vec2f tilePos = tileIndexToWorldPos(currentTileIdx);
        const vec2f nonNormalizedDir = (tilePos - _actor.pItem->pos);
        if (nonNormalizedDir != vec2f(0))
          _actor.pItem->direction = nonNormalizedDir.Normalize();

        _actor.pItem->enteredNewTileLastTick = true;
      }
    }
    else if (_actor.pItem->enteredNewTileLastTick)
    {
      const vec2f directionLut[6] = { vec2f(-0.5, 1), vec2f(-1, 0), vec2f(-0.5, -1), vec2f(0.5, -1), vec2f(1, 0), vec2f(0.5, 1) };
      const vec2f tilePos = tileIndexToWorldPos(currentTileIdx);
      const vec2f direction = directionLut[currentTileDirectionType - 1];
      const vec2f destinationPos = tilePos + direction;

      lsAssert(destinationPos - _actor.pItem->pos != vec2f(0));
      _actor.pItem->direction = (destinationPos - _actor.pItem->pos).Normalize();
      _actor.pItem->enteredNewTileLastTick = false;
    }

    _actor.pItem->pos += vec2f(0.1) * _actor.pItem->direction;
    _actor.pItem->lastTickTileIdx = currentTileIdx;
  }
}

//////////////////////////////////////////////////////////////////////////

template <typename T>
  requires (std::is_integral_v<T> && (sizeof(T) < sizeof(int64_t) || std::is_same_v<T, int64_t>))
inline T modify_with_clamp(T &value, const int64_t diff, const T min = lsMinValue<T>(), const T max = lsMaxValue<T>())
{
  const int64_t val = (int64_t)value + diff;
  const T prevVal = value;
  value = (T)lsClamp<int64_t>(val, min, max);
  return (T)lsAbs((int64_t)value - prevVal);
}

bool change_tile_to(const resource_type targetType, const resource_type expectedCurrentType, const size_t tileIdx, const uint8_t count)
{
  // TODO: if we can conclude from the resource_type to the ptt we could add an assert, that the ptt is `at_dest` in the pathfindingMap to assert, that the `expectedType` isn't nonsense

  lsAssert(tileIdx >= 0 && tileIdx < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

  if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == expectedCurrentType)
  {
    // TODO what free assert did coc talk about?
    lsAssert(setGameplayTile(tileIdx, targetType, count) == lsR_Success);
    return true;
  }

  return false;
}

uint8_t add_to_market_tile(const resource_type resource, const int16_t amount, const size_t tileIdx)
{
  lsAssert(tileIdx >= 0 && tileIdx < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(resource < tT_count);

  gameplay_element *pTile = &_Game.levelInfo.pGameplayMap[tileIdx];

  lsAssert(pTile->tileType == tT_market);
  lsAssert(pTile->resourceCountIndex > -1);

  local_list<uint8_t, tT_count> *pList = list_get(&_Game.levelInfo.multiResourceCounts, pTile->resourceCountIndex);

  return modify_with_clamp((*pList)[resource], amount);
}

uint8_t get_from_tile(const size_t tileIdx, const resource_type resource, const uint16_t amount)
{
  lsAssert(tileIdx >= 0 && tileIdx < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(resource < tT_count);

  gameplay_element *pTile = &_Game.levelInfo.pGameplayMap[tileIdx];

  if (pTile->resourceCountIndex == -1)
  {
    lsAssert(pTile->tileType == resource);
    return modify_with_clamp(pTile->resourceCount, -amount);
  }
  else
  {
    lsAssert(pTile->tileType == tT_market);
    local_list<uint8_t, tT_count> *pList = list_get(&_Game.levelInfo.multiResourceCounts, pTile->resourceCountIndex);
    return modify_with_clamp(*local_list_get(pList, resource), -amount);
  }
}

//////////////////////////////////////////////////////////////////////////

void update_lifesupportActors()
{
  static const uint8_t EatingThreshold = 3;
  static const uint8_t AppetiteThreshold = 10;
  static const uint8_t MaxNutritionValue = 255;
  static const int64_t FoodItemGain = 16;
  static const uint8_t MaxFoodItemCount = 255;
  static const uint8_t MinFoodItemCount = 0;

  static const uint8_t ColdThreshold = 10;
  static const int64_t TemperatureIncrease = 255;
  static const uint8_t MaxTemperature = 255;

  static const size_t nutritionTypeCount = (_ptT_nutrient_last + 1) - _ptT_nutrient_first;
  static const size_t foodTypeCount = (_tile_type_food_last + 1) - _tile_type_food_first;
  static const int64_t FoodToNutrition[foodTypeCount][nutritionTypeCount] = { { 50, 0, 0, 0 } /*tomato*/, {0, 50, 0, 0} /*bean*/, { 0, 0, 50, 0 } /*wheat*/,  { 0, 0, 0, 50 } /*sunflower*/, {25, 25, 25, 25} /*meal*/ }; // foodtypes and nutrition value need to be in the same order as the corresponding enums!

  for (auto _actor : _Game.lifesupportActors)
  {
    lifesupport_actor *pLifeSupport = _actor.pItem;

    // TODO think about actual system to nutrition and temperature usage
    // just for testing!!!!
    if (_Game.levelInfo.isNight)
    {
      modify_with_clamp(pLifeSupport->temperature, (int16_t)(-1));
    }
    else
    {
      for (size_t j = 0; j < nutritionTypeCount; j++)
        modify_with_clamp(pLifeSupport->nutritions[j], (int16_t)-1, (uint8_t)0, MaxNutritionValue);
    }

    movement_actor *pActor = pool_get(_Game.movementActors, pLifeSupport->entityIndex);

    if (pActor->target == ptT_fire_pit && pLifeSupport->type != eT_fire_actor) // acording to the debugger the cook had fire pit as target at some point?! not able to replicate it...
      __debugbreak();

    if (!pActor->survivalActorActive)
    {
      if (_Game.levelInfo.isNight)
      {
        if (pLifeSupport->type != eT_fire_actor && pLifeSupport->temperature < ColdThreshold)
        {
          pActor->survivalActorActive = true;
          pActor->target = ptT_fire;
          pActor->atDestination = false;
        }
      }
      else
      {
        bool anyNeededNutrion = false;

        for (size_t j = 0; j < nutritionTypeCount; j++)
        {
          if (pLifeSupport->nutritions[j] < EatingThreshold)
          {
            anyNeededNutrion = true;
            break;
          }
        }

        if (anyNeededNutrion)
        {
          int8_t bestScore = 0;
          size_t bestIndex = 0;

          for (size_t i = 0; i < LS_ARRAYSIZE(pLifeSupport->lunchbox); i++)
          {
            if (pLifeSupport->lunchbox[i])
            {
              int8_t score = 0;

              for (size_t j = 0; j < nutritionTypeCount; j++)
                if (FoodToNutrition[i][j] > 0)
                  score += pLifeSupport->nutritions[j] < AppetiteThreshold ? nutritionTypeCount : -1;

              if (score > bestScore)
              {
                bestScore = score;
                bestIndex = i;
              }
            }
          }

          // eat best item
          if (bestScore > 0)
          {
            for (size_t j = 0; j < nutritionTypeCount; j++)
              modify_with_clamp(pLifeSupport->nutritions[j], FoodToNutrition[bestIndex][j], MinFoodItemCount, MaxFoodItemCount);

            // remove from lunchbox
            modify_with_clamp(pLifeSupport->lunchbox[bestIndex], (int64_t)-1, MinFoodItemCount, MaxFoodItemCount);
          }
          else // if no item: set actor target
          {
            pathfinding_target_type lowestNutrient = ptT_Count;

            int64_t bestTargetScore = -1;
            const int64_t maxDist = (int64_t)(_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

            for (size_t j = 0; j < nutritionTypeCount; j++)
            {
              const pathfinding_target_type nutrient = (pathfinding_target_type)(j + _ptT_nutrient_first);

              const uint8_t value = pLifeSupport->nutritions[j];
              int64_t score = value < EatingThreshold ? lsMaxValue<int16_t>() : MaxNutritionValue - value;

              const level_info::resource_info &info = _Game.levelInfo.resources[nutrient];
              const pathfinding_info pathInfo = info.pDirectionLookup[1 - info.write_direction_idx][worldPosToTileIndex(pActor->pos)];

              if (pathInfo.dir != d_unreachable && value > EatingThreshold)
                score += maxDist - pathInfo.dist;

              if (score > bestTargetScore)
              {
                bestTargetScore = score;
                lowestNutrient = nutrient;
              }
            }

            lsAssert(bestTargetScore > -1 && lowestNutrient <= _ptT_nutrient_last);

            const level_info::resource_info &info = _Game.levelInfo.resources[lowestNutrient];
            if ((pLifeSupport->type == eT_farmer || pLifeSupport->type == eT_cook) && info.pDirectionLookup[1 - info.write_direction_idx][worldPosToTileIndex(pActor->pos)].dir == d_unreachable)
              continue;

            pActor->survivalActorActive = true;
            pActor->target = lowestNutrient;
            pActor->atDestination = false;

          }
        }
      }
    }
    else // TODO: do we want actors to go to fire if stuck with no food
    {
      if (pActor->atDestination)
      {
        const size_t tileIdx = worldPosToTileIndex(pActor->pos);

        if (pActor->target >= _ptT_nutrient_first && pActor->target <= _ptT_nutrient_last)
        {
          // add food to lunchbox
          if (_Game.levelInfo.pGameplayMap[tileIdx].tileType >= _tile_type_food_first && _Game.levelInfo.pGameplayMap[tileIdx].tileType <= _tile_type_food_last)
          {
            if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount > 0)
            {
              const resource_type tileType = _Game.levelInfo.pGameplayMap[tileIdx].tileType;
              lsAssert(tileType - _tile_type_food_first >= 0 && tileType - _tile_type_food_first <= _tile_type_food_last);
              modify_with_clamp(pLifeSupport->lunchbox[tileType - _tile_type_food_first], FoodItemGain, MinFoodItemCount, MaxFoodItemCount);

              modify_with_clamp(_Game.levelInfo.pGameplayMap[tileIdx].resourceCount, -FoodItemGain);

              //if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount == 0)
              //  _Game.levelInfo.pGameplayMap[tileIdx] = gameplay_element(tT_grass, 1); // no `change_tile_to` usage because we check earlier
            }
          }
          else
          {
            pActor->atDestination = false;
          }
        }
        else if (pActor->target == ptT_fire)
        {
          // warm up at fire
          if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_fire)
          {
            if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount > 0)
              modify_with_clamp(pLifeSupport->temperature, (int16_t)(200), (uint8_t)(0), MaxTemperature);

            // for testing: remove from fire & remove fire when empty
            lsAssert(_Game.levelInfo.pGameplayMap[tileIdx].resourceCount > 0);
            _Game.levelInfo.pGameplayMap[tileIdx].resourceCount--;

            if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount == 0)
              _Game.levelInfo.pGameplayMap[tileIdx].tileType = tT_fire_pit; // No usage of `change_tile_to` because of check above. Actually okay to just change the tileType as we want to keep `count` and `maxResourceCount` between `tT_fire` and `tT_fire_pit` are the same.
          }
          else
          {
            pActor->atDestination = false;
          }
        }
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// TODO: we could add ranges to operate in for the actors so they won't cross eachother's paths so much - well this does not quite work 1) because we can only ever have a range from where we currently are. 2) because actors need to go to fire/food further away. - only if the actors alwyas return to the one same place after everu step (but then every actor would need his own resource type...

// TODO: houses: add houses that conclude to several pathfindingtypes at once (e.g. a kitchen where all food drop offs and nutrient types are...). in a further step: render houses that can be bigger than just one tile.

// TODO: I really want to let things grow slowly, or have actors wait for x amount of time until a task is finished.
// we could have a state var for tiles (uint8) which e.g. indicates the grow state, the plant is only recognized for pathfinding, when there the state is the matchting number - else it is just `ptt_none_interactable` or just `ptt_unripe_fruit`...

static constexpr pathfinding_target_type Lumberjack_TargetFromState[laS_count] = { ptT_soil, ptT_water, ptT_sapling, ptT_tree, ptT_trunk, ptT_market };

void incrementLumberjackState(lumberjack_actor_state &state, pathfinding_target_type &target)
{
  lsAssert(state >= 0 && state < laS_count);
  lsAssert(target < ptT_Count);

  state = (lumberjack_actor_state)((state + 1) % laS_count);
  target = Lumberjack_TargetFromState[state];
}

void update_lumberjack()
{
  for (const auto _actor : _LumberjackActors)
  {
    lumberjack_actor *pLumberjack = _actor.pItem;
    movement_actor *pActor = pool_get(_Game.movementActors, pLumberjack->index);

    // Handle Survival
    if (pActor->survivalActorActive)
    {
      if (pActor->atDestination || (!_Game.levelInfo.isNight && pActor->target == ptT_fire))
      {
        pActor->survivalActorActive = false;
        pActor->target = Lumberjack_TargetFromState[pLumberjack->state];
        pActor->atDestination = false;
        continue;
      }
    }

    if (pActor->atDestination)
    {
      const size_t tileIdx = worldPosToTileIndex(pActor->pos);

      switch (pLumberjack->state)
      {
      case laS_plant:
      {
        if (change_tile_to(tT_sapling, tT_soil, tileIdx, MaxResourceCounts[tT_sapling]))
          incrementLumberjackState(pLumberjack->state, pActor->target);

        pActor->atDestination = false;

        break;
      }
      case laS_getWater:
      {
        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_water && _Game.levelInfo.pGameplayMap[tileIdx].resourceCount > 0)
        {
          lsAssert(!pLumberjack->hasItem);

          pLumberjack->hasItem = true;
          pLumberjack->item = tT_water;

          incrementLumberjackState(pLumberjack->state, pActor->target);
        }

        pActor->atDestination = false;

        break;
      }
      case laS_water:
      {
        lsAssert(pLumberjack->hasItem && pLumberjack->item == tT_water);

        if (change_tile_to(tT_tree, tT_sapling, tileIdx, MaxResourceCounts[tT_tree]))
        {
          pLumberjack->hasItem = false;
          incrementLumberjackState(pLumberjack->state, pActor->target);
        }

        pActor->atDestination = false;

        break;
      }
      case laS_chop:
      {
        if (change_tile_to(tT_trunk, tT_tree, tileIdx, MaxResourceCounts[tT_trunk]))
          incrementLumberjackState(pLumberjack->state, pActor->target);

        pActor->atDestination = false;

        break;
      }
      case laS_cut:
      {
        lsAssert(!pLumberjack->hasItem);

        if (change_tile_to(tT_soil, tT_trunk, tileIdx, MaxResourceCounts[tT_soil]))
        {
          pLumberjack->hasItem = true;
          pLumberjack->item = tT_wood;
          incrementLumberjackState(pLumberjack->state, pActor->target);
        }

        pActor->atDestination = false;

        break;
      }
      case laS_drop_off:
      {
        lsAssert(pLumberjack->hasItem && pLumberjack->item == tT_wood);

        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_market)
        {
          add_to_market_tile(tT_wood, 4, tileIdx);
          pLumberjack->hasItem = false;
          
          incrementLumberjackState(pLumberjack->state, pActor->target);
        }

        pActor->atDestination = false;

        break;
      }

      default:
      {
        lsFail(); // not implemented.
      }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////

void update_farmer()
{
  for (const auto _actor : _FarmerActors)
  {
    farmer_actor *pFarmer = _actor.pItem;
    movement_actor *pActor = pool_get(_Game.movementActors, pFarmer->index);

    // Handle Survival
    if (pActor->survivalActorActive)
    {
      if (pActor->atDestination || (!_Game.levelInfo.isNight && pActor->target == ptT_fire))
      {
        pActor->survivalActorActive = false;
        pActor->target = ptT_soil;
        pActor->atDestination = false;
      }

      continue;
    }

    if (pActor->atDestination)
    {
      const size_t tileIdx = worldPosToTileIndex(pActor->pos);

      //switch (pFarmer->state)
      //{
      //case faaS_plant:
      //{
      if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_soil)
      {
        pathfinding_target_type plant = ptT_Count;

        for (uint8_t i = _ptT_nutrient_sources_first; i <= _ptT_nutrient_sources_last; i++)
        {
          const level_info::resource_info &info = _Game.levelInfo.resources[i];

          if (info.pDirectionLookup[1 - info.write_direction_idx][tileIdx].dir == d_unreachable)
          {
            plant = (pathfinding_target_type)i; // TODO: Maybe we want to just increment the last plant and if its already there we choose another one that isn't
            break;
          }
        }

        if (plant == ptT_Count)
          plant = (pathfinding_target_type)(lsGetRand() % (_ptT_nutrient_sources_last - _ptT_nutrient_sources_first) + _ptT_nutrient_sources_first);

        constexpr uint8_t AddedAmountToPlant = 12;

        resource_type resource = (resource_type)plant;
        lsAssert(resource >= _tile_type_food_resources_first && resource <= _tile_type_food_resources_last);
        change_tile_to(resource, tT_soil, tileIdx, AddedAmountToPlant); // no `if` as we check above.

        pActor->atDestination = false;
      }
      //}
      //case faaS_harvest:
      //{
      //  if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == pFarmer->targetPlant)
      //  {
      //    // take if space in inventory
      //    // change tile
      //    pActor->target = ptT_market;
      //
      //    // todo: mm i have to add another ptt for the fruit for harvesting...
      //  }
      //}
      //}
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////

resource_type getNextCookItem(const resource_type currentItem, const size_t tileIdx)
{
  resource_type ret = currentItem;

  for (size_t i = _tile_type_food_first; i <= _tile_type_food_last; i++)
  {
    lsAssert(ret >= _tile_type_food_first && ret <= _tile_type_food_last);
    ret = (resource_type)((((ret - _tile_type_food_first) + 1) % (_tile_type_food_last + 1 - _tile_type_food_first)) + _tile_type_food_first);

    const level_info::resource_info &info = _Game.levelInfo.resources[(pathfinding_target_type)((ret - _tile_type_food_first) + _ptT_drop_off_first)];
    const direction d = info.pDirectionLookup[1 - info.write_direction_idx][tileIdx].dir;
    // check if there is a drop off for the item so we don't get stuck. (Maybe remove in the future, if we *want* actors to be stuck, when the right tiles weren't provided)
    if (d != d_unreachable && d != d_unfillable)
      break;
  }

  return ret;
}

void update_cook()
{
  constexpr uint8_t IngridientAmountPerFood[(_tile_type_food_last + 1) - _tile_type_food_first][(_ptT_nutrient_last + 1) - _ptT_nutrient_first] =
  { // nutrients: ptT_vitamin, ptT_protein, ptT_carbohydrates, ptT_fat
    { 1, 0, 0, 0 }, // tT_tomato
    { 0, 1, 0, 0 }, // tT_bean
    { 0, 0, 1, 0 }, // tT_wheat
    { 0, 0, 0, 1 }, // tT_sunflower
    { 1, 1, 1, 1 } // tT_meal
  };

  for (const auto _actor : _CookActors)
  {
    cook_actor *pCook = _actor.pItem;
    movement_actor *pActor = pool_get(_Game.movementActors, pCook->index);

    // Handle Survival
    if (pActor->survivalActorActive)
    {
      if (pActor->atDestination || (!_Game.levelInfo.isNight && pActor->target == ptT_fire))
      {
        pActor->survivalActorActive = false;
        pCook->state = caS_check_inventory;
        pActor->atDestination = false;
      }

      continue;
    }

    lsAssert(pCook->currentCookingItem >= _tile_type_food_first && pCook->currentCookingItem <= _tile_type_food_last);

    // Handle Cook States
    const size_t tileIdx = worldPosToTileIndex(pActor->pos);

    if (pCook->state == caS_check_inventory) // Handling `caS_check_inventory` here because it does not need to be checked for `atDestination`
    {
      bool anyItemMissing = false;

      for (size_t i = 0; i < LS_ARRAYSIZE(pCook->inventory); i++)
      {
        if (pCook->inventory[i] <= 0 && IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first][i] > 0)
        {
          anyItemMissing = true;

          pathfinding_target_type targetPlant = (pathfinding_target_type)(i + _ptT_nutrient_sources_first);

          const level_info::resource_info &info = _Game.levelInfo.resources[targetPlant];
          const direction targetPlantDir = info.pDirectionLookup[1 - info.write_direction_idx][tileIdx].dir;

          if (targetPlantDir != d_unfillable && targetPlantDir != d_unreachable)
          {
            pCook->state = caS_harvest;
            pActor->target = targetPlant;
            pActor->atDestination = false;

            break;
          }

          continue;
        }
      }

      if (anyItemMissing && pCook->state == caS_check_inventory) // if none of the required plants is available, set new target meal. (Maybe remove in the future, if we *want* actors to be stuck, when the right tiles weren't provided)
      {
        pCook->currentCookingItem = getNextCookItem(pCook->currentCookingItem, tileIdx);
        continue;
      }
      else if (!anyItemMissing) // if all items in inventory
      {
        pCook->state = caS_cook;
        const pathfinding_target_type targetNutrient = (pathfinding_target_type)(_ptT_drop_off_first + (pCook->currentCookingItem - _tile_type_food_first));

        lsAssert(targetNutrient >= _ptT_drop_off_first && targetNutrient <= _ptT_drop_off_last);

        pActor->target = targetNutrient;
        pActor->atDestination = false;
      }

      continue;
    }

    // Handling remaining Cook States
    if (pActor->atDestination)
    {
      const level_info::resource_info &info = _Game.levelInfo.resources[pActor->target];
      lsAssert(info.pDirectionLookup[1 - info.write_direction_idx][tileIdx].dir == d_atDestination);

      switch (pCook->state)
      {
      case caS_check_inventory:
      {
        lsAssert(false); // Should've been handled above.
        break;
      }

      case caS_harvest:
      {
        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType >= _tile_type_food_resources_first && _Game.levelInfo.pGameplayMap[tileIdx].tileType <= _tile_type_food_resources_last)
        {
          lsAssert(pActor->target >= _ptT_nutrient_sources_first && pActor->target <= _ptT_nutrient_sources_last);

          // check if plant has items left
          if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount > 0)
          {
            // take item
            constexpr int16_t AddedResourceAmount = 4;
            modify_with_clamp(pCook->inventory[pActor->target - _ptT_nutrient_sources_first], get_from_tile(tileIdx, _Game.levelInfo.pGameplayMap[tileIdx].tileType, AddedResourceAmount)); // todo: find out which exact resource we want. mmm this is bad news, because this is not how we handle meals when picking them up...

            if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount == 0) // TODO: why aren't we ever turning stuff back to soil???
              _Game.levelInfo.pGameplayMap[tileIdx] = gameplay_element(tT_soil, 1); // no usage of `change_tile_to` due to earlier check of `resource_type`

            pCook->state = caS_check_inventory;
          }
        }

        pActor->atDestination = false;

        break;
      }

      case caS_cook:
      {
        constexpr uint8_t AddedCookedItemAmount = 24;
        pActor->atDestination = false;

        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType != pCook->currentCookingItem || _Game.levelInfo.pGameplayMap[tileIdx].resourceCount == _Game.levelInfo.pGameplayMap[tileIdx].maxResourceCount)
          break;

        modify_with_clamp(_Game.levelInfo.pGameplayMap[tileIdx].resourceCount, AddedCookedItemAmount, uint8_t(0), _Game.levelInfo.pGameplayMap[tileIdx].maxResourceCount);

        for (size_t i = 0; i < LS_ARRAYSIZE(pCook->inventory); i++)
        {
          lsAssert(pCook->inventory[i] >= IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first][i]);
          pCook->inventory[i] -= IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first][i];
        }

        // change to next food item
        pCook->currentCookingItem = getNextCookItem(pCook->currentCookingItem, tileIdx);
        pCook->state = caS_check_inventory;

        break;
      }

      default:
      {
        lsFail(); // not implemented.
      }
      }
    }
    else
    { // (Maybe remove in the future, if we *want* actors to be stuck, when the right tiles weren't provided)
      const level_info::resource_info &info = _Game.levelInfo.resources[(resource_type)((pCook->currentCookingItem - _tile_type_food_first) + _ptT_drop_off_first)];
      const direction d = info.pDirectionLookup[1 - info.write_direction_idx][tileIdx].dir;

      if (d == d_unreachable || d == d_unfillable)
      {
        pCook->currentCookingItem = getNextCookItem(pCook->currentCookingItem, tileIdx);
        pCook->state = caS_check_inventory;
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void update_fireActor() // seems kinda sus - as if he's not always targeting all the fires but only starting some...
{
  constexpr pathfinding_target_type target_from_state[faS_count] = { ptT_wood, ptT_fire_pit, ptT_water, ptT_fire };

  for (const auto _actor : _FireActors)
  {
    fire_actor *pFireActor = _actor.pItem;
    movement_actor *pActor = pool_get(_Game.movementActors, pFireActor->index);

    // Handle Survival
    if (pActor->survivalActorActive)
    {
      if (pActor->atDestination || _Game.levelInfo.isNight)
      {
        pActor->survivalActorActive = false;
        pActor->target = target_from_state[pFireActor->state];
        pActor->atDestination = false;

        continue;
      }
    }

    if (_Game.levelInfo.isNight)
    {
      if (pFireActor->state == faS_extinguish_fire)
      {
        pFireActor->state = faS_start_fire;

        if (!pActor->survivalActorActive)
        {
          pActor->target = target_from_state[faS_start_fire];
          pActor->atDestination = false;
        }
      }
    }
    else
    {
      if (pFireActor->state == faS_start_fire)
      {
        pFireActor->state = faS_extinguish_fire;

        if (!pActor->survivalActorActive)
        {
          pActor->target = target_from_state[faS_extinguish_fire]; // TODO: do we want the actor to be able to extinguish the fire if he is stuck searching for food/wood if a new fire emerged?
          pActor->atDestination = false;
        }
      }
    }

    if (pActor->atDestination)
    {
      const size_t tileIdx = worldPosToTileIndex(pActor->pos);
      switch (pFireActor->state)
      {
      case faS_get_wood:
      {
        constexpr int16_t AddedWood = 6;

        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_market)
        {
          //if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount > 0)
          {
            modify_with_clamp(pFireActor->wood_inventory, get_from_tile(tileIdx, tT_wood, AddedWood));

            //if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount == 0)
            //  _Game.levelInfo.pGameplayMap[tileIdx] = gameplay_element(tT_soil, 1); // No usage of `change_tile_to` because of check above.

            pFireActor->state = faS_start_fire;
            pActor->target = target_from_state[faS_start_fire];
          }
        }

        pActor->atDestination = false;

        break;
      }
      case faS_start_fire:
      {
        constexpr int16_t WoodPerFire = 3;

        if (_Game.levelInfo.isNight)
        {
          if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_fire_pit)
          {
            if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount > WoodPerFire)
            {
              _Game.levelInfo.pGameplayMap[tileIdx].tileType = tT_fire; // No usage of `change_tile_to` because of check above. Actually okay to just change the tileType as we want to keep `count` and `maxResourceCount` between `tT_fire` and `tT_fire_pit` are the same.
            }
            else
            {
              // if has wood: add wood to fire
              if (pFireActor->wood_inventory >= WoodPerFire)
              {
                pFireActor->wood_inventory -= WoodPerFire;
                _Game.levelInfo.pGameplayMap[tileIdx].tileType = tT_fire; // No usage of `change_tile_to` because of check above. Actually okay to just change the tileType as we want to keep `count` and `maxResourceCount` between `tT_fire` and `tT_fire_pit` are the same.
                modify_with_clamp(_Game.levelInfo.pGameplayMap[tileIdx].resourceCount, WoodPerFire, (uint8_t)(0), _Game.levelInfo.pGameplayMap[tileIdx].maxResourceCount); // TODO
              }
              else
              {
                pFireActor->state = faS_get_wood;
                pActor->target = target_from_state[faS_get_wood];
              }
            }
          }
        }
        else
        {
          pFireActor->state = faS_extinguish_fire;
          pActor->target = target_from_state[faS_extinguish_fire];
        }

        pActor->atDestination = false;

        break;
      }
      case faS_get_water:
      {
        constexpr int16_t AddedWater = 4;

        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_water)
        {
          if (_Game.levelInfo.pGameplayMap[tileIdx].resourceCount > 0)
          {
            //modify_with_clamp(pFireActor->water_inventory, lsMin(AddedWater, (int16_t)_Game.levelInfo.pGameplayMap[tileIdx].resourceCount));
            lsAssert(pFireActor->water_inventory < 255);
            pFireActor->water_inventory += AddedWater;

            pFireActor->state = faS_extinguish_fire;
            pActor->target = target_from_state[faS_extinguish_fire];
          }
        }

        pActor->atDestination = false;

        break;
      }
      case faS_extinguish_fire:
      {
        constexpr int16_t WaterPerFire = 1;

        if (_Game.levelInfo.isNight)
        {
          pFireActor->state = faS_start_fire;
          pActor->target = target_from_state[faS_extinguish_fire];
        }
        else
        {
          if (pFireActor->water_inventory >= WaterPerFire)
          {
            if (change_tile_to(tT_fire_pit, tT_fire, tileIdx, _Game.levelInfo.pGameplayMap[tileIdx].resourceCount))
              pFireActor->water_inventory -= WaterPerFire;
          }
          else
          {
            pFireActor->state = faS_get_water;
            pActor->target = target_from_state[faS_get_water];
          }
        }

        pActor->atDestination = false;

        break;
      }
      default:
      {
        lsFail(); // not implemented.
      }
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// TODO: handle nutrition loss? maybe ok in movement actor, but maybe some more thoughtthrough way

static size_t ticksSincelastDayNightSwitch = 0;

void handle_dayNightCycle()
{
  constexpr size_t DayLength = 500;
  constexpr size_t NightLength = 300;

  if (ticksSincelastDayNightSwitch == DayLength && !_Game.levelInfo.isNight)
  {
    _Game.levelInfo.isNight = true;
    ticksSincelastDayNightSwitch = 0;
  }
  else if (ticksSincelastDayNightSwitch == NightLength && _Game.levelInfo.isNight)
  {
    _Game.levelInfo.isNight = false;
    ticksSincelastDayNightSwitch = 0;
  }

  ticksSincelastDayNightSwitch++;
}

//////////////////////////////////////////////////////////////////////////

void game_update()
{
  handle_dayNightCycle();
  updateFloodfill();
  movementActor_move();
  update_lifesupportActors();
  update_lumberjack();
  update_farmer();
  update_cook();
  update_fireActor();
}

//////////////////////////////////////////////////////////////////////////

void game_playerSwitchTiles(const resource_type terrainType)
{
  lsAssert(_Game.levelInfo.playerPos.x >= 1 && _Game.levelInfo.playerPos.x <= _Game.levelInfo.map_size.x - 2 && _Game.levelInfo.playerPos.y >= 0 && _Game.levelInfo.playerPos.y <= _Game.levelInfo.map_size.y - 2);

  const size_t idx = worldPosToTileIndex((vec2f)(_Game.levelInfo.playerPos));

  lsAssert(idx < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(terrainType < tT_count);

  lsAssert(setGameplayTile(idx, terrainType, MaxResourceCounts[terrainType]) == lsR_Success);
}

void game_setPlayerMapIndex(const direction dir)
{
  lsAssert(dir > d_unreachable && dir < d_atDestination);
  lsAssert(_Game.levelInfo.playerPos.x >= 1 && _Game.levelInfo.playerPos.x <= _Game.levelInfo.map_size.x - 2 && _Game.levelInfo.playerPos.y >= 0 && _Game.levelInfo.playerPos.y <= _Game.levelInfo.map_size.y - 2);

  static const vec2i16 EvenDir[] = { vec2i16(0, -1), vec2i16(1, 0), vec2i16(0, 1), vec2i16(-1, 1), vec2i16(-1, 0), vec2i16(-1, -1) };
  static const vec2i16 OddDir[] = { vec2i16(1, -1), vec2i16(1, 0), vec2i16(1, 1), vec2i16(0, 1), vec2i16(-1, 0), vec2i16(0, -1) };

  const vec2i16 newPos = _Game.levelInfo.playerPos.y % 2 ? _Game.levelInfo.playerPos + OddDir[dir - 1] : _Game.levelInfo.playerPos + EvenDir[dir - 1];

  if (newPos.x >= 1 && newPos.x <= _Game.levelInfo.map_size.x - 2 && newPos.y >= 1 && newPos.y <= _Game.levelInfo.map_size.y - 2)
    _Game.levelInfo.playerPos = newPos;
}

//////////////////////////////////////////////////////////////////////////

lsResult game_init()
{
  return game_init_local();
}

lsResult game_tick()
{
  return game_tick_local();
}

//////////////////////////////////////////////////////////////////////////

game *game_getGame()
{
  return &_Game;
}

size_t game_getTickRate()
{
  return _Game.tickRate;
}

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local()
{
  lsResult result = lsR_Success;

  initializeLevel();
  _Game.gameStartTimeNs = _Game.lastUpdateTimeNs = lsGetCurrentTimeNs();

  goto epilogue;
epilogue:
  return result;
}

lsResult game_tick_local()
{
  lsResult result = lsR_Success;

  //const int64_t lastTick = _Game.lastUpdateTimeNs;
  const int64_t tick = lsGetCurrentTimeNs();
  //const float_t simFactor = (float_t)(tick - lastTick) / (1e+9f / (float_t)_Game.tickRate);
  _Game.lastUpdateTimeNs = tick;

  // stuff.
  // process player interactions.
  {
    game_update();

    goto epilogue;
  epilogue:
    return result;
  }
}
