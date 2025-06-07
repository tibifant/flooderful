#include "game.h"

#include "box2d/box2d.h"

//////////////////////////////////////////////////////////////////////////

static game _Game;
static size_t _LevelSize = 175;
static b2World *_pBox2DWorld = nullptr;
static pool<b2Body *> _Box2DObjects;

constexpr bool UseBox2DPhysics = true;

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local();
lsResult game_tick_local();

//////////////////////////////////////////////////////////////////////////

void game_addEvent_internal(gameEvent *pEvent);

//////////////////////////////////////////////////////////////////////////

constexpr size_t _FloodFillSteps = 100;

//////////////////////////////////////////////////////////////////////////

static pool<lumberjack_actor> _LumberjackActors; // Do we need this in the header?
static pool<cook_actor> _CookActors; // Do we need this in the header?
static pool<fire_actor> _FireActors; // Do we need this in the header?

//////////////////////////////////////////////////////////////////////////

template <pathfinding_target_type p>
struct match_resource;

template<>
struct match_resource<pathfinding_target_type::ptT_grass>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_grass; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_soil>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_soil; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_water>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_water; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sand>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_sand; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sapling>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_sapling; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_tree>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_tree; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_trunk>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_trunk; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_wood>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_wood; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_fire>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_fire; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_fire_pit>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_fire_pit; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_tomato_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_tomato_plant; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_bean_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_bean_plant; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_wheat_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_wheat_plant; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sunflower_plant>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_sunflower_plant; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_vitamin>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_tomato || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_protein>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_bean || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_carbohydrates>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_wheat || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_fat>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_sunflower || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_collidable>
{
  FORCEINLINE static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_mountain; };
};

template<pathfinding_target_type p>
void fill_resource_info(pathfinding_info *pDirectionLookup, queue<fill_step> &pathfindQueue, gameplay_element *pMap)
{
  lsZeroMemory(pDirectionLookup, _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
  {
    if (match_resource<p>::resourceAttribute_matches_resource(pMap[i].tileType))
    {
      queue_pushBack(&pathfindQueue, fill_step(i, 0));
      pDirectionLookup[i].dir = d_atDestination;
    }
    else
    {
      pDirectionLookup[i].dir = (direction)(d_unfillable * (direction)(match_resource<ptT_collidable>::resourceAttribute_matches_resource(pMap[i].tileType)));
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

void setTerrain()
{
  rand_seed seed = rand_seed(2, 2); // 2, 2 ist cool am anfang.

  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
  {
    _Game.levelInfo.pGameplayMap[i].tileType = (lsGetRand(seed) & 15) < 12 ? tT_grass : tT_mountain;
    _Game.levelInfo.pGameplayMap[i].ressourceCount = 1;
    _Game.levelInfo.pPathfindingMap[i].elevationLevel = lsGetRand(seed) % 3;
  }

  for (size_t i = 0; i < 10; i++)
    _Game.levelInfo.pGameplayMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].tileType = tT_soil;

  for (size_t i = 0; i < 3; i++)
  {
    _Game.levelInfo.pGameplayMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].tileType = tT_sand;
    const size_t waterIdx = lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
    _Game.levelInfo.pGameplayMap[waterIdx].tileType = tT_water;
    _Game.levelInfo.pGameplayMap[waterIdx].ressourceCount = 1;
  }

  for (size_t i = _tile_type_food_first; i < _tile_type_food_last; i++) // without tt_meal for testing
  {
    const size_t index = i + 5;
    _Game.levelInfo.pGameplayMap[index].tileType = resource_type(i);
    _Game.levelInfo.pGameplayMap[index].ressourceCount = 64;
    _Game.levelInfo.pPathfindingMap[index].elevationLevel = 1;
  }

  //_Game.levelInfo.pGameplayMap[120].tileType = tT_fire;
  //_Game.levelInfo.pGameplayMap[120].ressourceCount = 255;
  _Game.levelInfo.pGameplayMap[121].tileType = tT_fire_pit;

  // Setting borders to ptT_collidable
  {
    for (size_t y = 0; y < _Game.levelInfo.map_size.y; y++)
    {
      _Game.levelInfo.pGameplayMap[y * _Game.levelInfo.map_size.x].tileType = tT_mountain;
      _Game.levelInfo.pGameplayMap[(y + 1) * _Game.levelInfo.map_size.x - 1].tileType = tT_mountain;
    }

    for (size_t x = 0; x < _Game.levelInfo.map_size.x; x++)
    {
      _Game.levelInfo.pGameplayMap[x].tileType = tT_mountain;
      _Game.levelInfo.pGameplayMap[x + (_Game.levelInfo.map_size.y - 1) * _Game.levelInfo.map_size.x].tileType = tT_mountain;
    }
  }
}

lsResult spawnActors()
{
  lsResult result = lsR_Success;

  // Lumberjacks
  //for (size_t i = 0; i < 5; i++)
  {
    movement_actor actor;
    actor.target = ptT_sapling; //(terrain_type)(lsGetRand() % (tT_Count - 1));
    //actor.pos = vec2f((float_t)((1 + i * 3) % _Game.levelInfo.map_size.x), (float_t)((i * 3 + 1) % _Game.levelInfo.map_size.y));
    actor.pos = vec2f((float_t)(4 % _Game.levelInfo.map_size.x), (float_t)(4 % _Game.levelInfo.map_size.y));

    while (_Game.levelInfo.pGameplayMap[worldPosToTileIndex(actor.pos)].tileType == tT_mountain)
      actor.pos.x = (float_t)(size_t(actor.pos.x + 1) % _Game.levelInfo.map_size.x);

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

  // Cook
  {
    movement_actor foodActor;
    foodActor.target = _ptT_nutrition_first;
    foodActor.pos = vec2f(10.f, 10.f);

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
    actor.target = _ptT_nutrition_first;
    actor.pos = vec2f(13.f, 13.f);

    size_t f_index;
    LS_ERROR_CHECK(pool_add(&_Game.movementActors, actor, &f_index));

    fire_actor fireActor;
    fireActor.state = faS_start_fire;
    fireActor.wood_inventory = 0;

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
  setTerrain();

  // Set up floodfill queue and lookup
  for (size_t i = 0; i < ptT_Count - 1; i++) // Skip ptT_collidable
  {
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[0], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[1], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

    rebuild_resource_info(_Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx], _Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.pGameplayMap, (pathfinding_target_type)(i));
  }

  lsAssert(spawnActors() == lsR_Success);
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

  if ((size_t)y_center & 1)
    tilePos.x += 0.5f;

  return tilePos;
}

static size_t r = 0;

void movementActor_move()
{
  r = (r + 1) & 63;

  for (auto _actor : _Game.movementActors)
  {
    //const size_t lastTileIdx = _actor.pItem->lastTickTileIdx;

    // Reset lastTile every so often to handle map changes.
    if ((_actor.index & 63) == r)
      _actor.pItem->lastTickTileIdx = 0;

    const size_t currentTileIdx = worldPosToTileIndex(_actor.pItem->pos);
    const direction currentTileDirectionType = _Game.levelInfo.resources[_actor.pItem->target].pDirectionLookup[1 - _Game.levelInfo.resources[_actor.pItem->target].write_direction_idx][currentTileIdx].dir;

    if (currentTileIdx != _actor.pItem->lastTickTileIdx && currentTileDirectionType != d_unreachable)
    {
      if (currentTileDirectionType == d_unfillable)
      {
        _actor.pItem->direction = (tileIndexToWorldPos(_actor.pItem->lastTickTileIdx) - _actor.pItem->pos).Normalize();
      }
      else if (currentTileDirectionType == d_atDestination)
      {
        _actor.pItem->atDestination = true;
        _actor.pItem->direction = vec2f(0);
        continue;
      }
      else
      {
        const vec2f directionLut[6] = { vec2f(-0.5, 1), vec2f(-1, 0), vec2f(-0.5, -1), vec2f(0.5, -1), vec2f(1, 0), vec2f(0.5, 1) };
        const vec2f direction = directionLut[currentTileDirectionType - 1];
        const vec2f tilePos = tileIndexToWorldPos(currentTileIdx);
        const vec2f destinationPos = tilePos + direction;

        _actor.pItem->direction = (destinationPos - _actor.pItem->pos).Normalize();
      }
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
  return value - prevVal;
}

bool change_tile_to(const resource_type targetType, const resource_type expectedPreviousType, const size_t tileIdx)
{
  // TODO: if we can conclude from the resource_type to the ptt we could add an assert, that the ptt is `at_dest` in the pathfindingMap to assert, that the `expectedType` isn't nonsense

  if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == expectedPreviousType)
  {
    // what free assert did coc talk about?
    _Game.levelInfo.pGameplayMap[tileIdx].tileType = targetType;
    return true;
  }

  return false;
}

// TODO: add ifs to every action that relies on the tile being a specific tileType
// TODO: handle setting actor.atDest to false, when if was false, so it won't be falsely true once the pathfindignLookUp updates and he walks somewhere else.
// TODO use change_tile func and ask coc about the assert - i guess coc is talking about the assert, that the tile is what we axpected, but that would definetifly still get triggered as I can't resolve the underlying issue.

//////////////////////////////////////////////////////////////////////////

void update_lifesupportActors()
{
  // TODO: ADAPT VALUES TO MAKE SENSE! --> !!!! also food actor food drop rate
  static const uint8_t EatingThreshold = 1;
  static const uint8_t AppetiteThreshold = 10;
  static const uint8_t MaxNutritionValue = 255;
  static const int64_t FoodItemGain = 8;
  static const uint8_t MaxFoodItemCount = 255;
  static const uint8_t MinFoodItemCount = 0;

  static const uint8_t ColdThreshold = 10;
  static const int64_t TemperatureIncrease = 255;
  static const uint8_t MaxTemperature = 255;

  static const size_t nutritionTypeCount = (_ptT_nutrition_last + 1) - _ptT_nutrition_first;
  static const size_t foodTypeCount = (_tile_type_food_last + 1) - _tile_type_food_first;
  static const int64_t FoodToNutrition[foodTypeCount][nutritionTypeCount] = { { 50, 0, 0, 0 } /*tomato*/, {0, 50, 0, 0} /*bean*/, { 0, 0, 50, 0 } /*wheat*/,  { 0, 0, 0, 50 } /*sunflower*/, {25, 25, 25, 25} /*meal*/ }; // foodtypes and nutrition value need to be in the same order as the corresponding enums!

  for (auto _actor : _Game.lifesupportActors)
  {
    // TODO think about actual system to nutrition and temperature usage
    // just for testing!!!!

    if (_Game.isNight)
    {
      modify_with_clamp(_actor.pItem->temperature, (int16_t)(-1));
    }
    else
    {
      for (size_t j = 0; j < nutritionTypeCount; j++)
        modify_with_clamp(_actor.pItem->nutritions[j], (int64_t)-1, (uint8_t)0, MaxNutritionValue);
    }

    movement_actor *pActor = pool_get(_Game.movementActors, _actor.pItem->entityIndex);

    if (!pActor->survivalActorActive)
    {
      if (_Game.isNight)
      {
        // TODO: Add range in pathfinding and walk to nearest item with best score.
        if (_actor.pItem->temperature < ColdThreshold) // TODO maybe we want to weigh between food and fire instead of always prefering food
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
          if (_actor.pItem->nutritions[j] < EatingThreshold)
            anyNeededNutrion = true;

        if (anyNeededNutrion)
        {
          int8_t bestScore = 0;
          size_t bestIndex = 0;

          for (size_t i = 0; i < LS_ARRAYSIZE(_actor.pItem->lunchbox); i++)
          {
            if (_actor.pItem->lunchbox[i])
            {
              int8_t score = 0;

              for (size_t j = 0; j < nutritionTypeCount; j++)
                if (FoodToNutrition[i][j] > 0)
                  score += _actor.pItem->nutritions[j] < AppetiteThreshold ? nutritionTypeCount : -1; // TODO: consider range here?

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
              modify_with_clamp(_actor.pItem->nutritions[j], FoodToNutrition[bestIndex][j], MinFoodItemCount, MaxFoodItemCount);

            // remove from lunchbox
            modify_with_clamp(_actor.pItem->lunchbox[bestIndex], (int64_t)-1, MinFoodItemCount, MaxFoodItemCount);
          }
          else // if no item: set actor target
          {
            size_t lowest = MaxNutritionValue;
            pathfinding_target_type lowestNutrient = ptT_Count;

            for (size_t j = 0; j < nutritionTypeCount; j++)
            {
              if (_actor.pItem->nutritions[j] < lowest)
              {
                lowest = _actor.pItem->nutritions[j];
                lowestNutrient = (pathfinding_target_type)(j + _ptT_nutrition_first);
              }
            }

            lsAssert(lowestNutrient <= _ptT_nutrition_last);
            pActor->survivalActorActive = true;
            pActor->target = lowestNutrient;
            pActor->atDestination = false;
          }
        }
      }
    }
    else
    {
      if (pActor->atDestination)
      {
        const size_t worldIdx = worldPosToTileIndex(pActor->pos);

        if (pActor->target >= _ptT_nutrition_first && pActor->target <= _ptT_nutrition_last)
        {
          // add food to lunchbox
          if (_Game.levelInfo.pGameplayMap[worldIdx].tileType >= _tile_type_food_first && _Game.levelInfo.pGameplayMap[worldIdx].tileType <= _tile_type_food_last)
          {
            if (_Game.levelInfo.pGameplayMap[worldIdx].ressourceCount > 0)
            {
              const resource_type tileType = _Game.levelInfo.pGameplayMap[worldIdx].tileType;
              lsAssert(tileType - _tile_type_food_first >= 0 && tileType - _tile_type_food_first <= _tile_type_food_last);
              modify_with_clamp(_actor.pItem->lunchbox[tileType - _tile_type_food_first], FoodItemGain, MinFoodItemCount, MaxFoodItemCount);

              modify_with_clamp(_Game.levelInfo.pGameplayMap[worldIdx].ressourceCount, -FoodItemGain);

              if (_Game.levelInfo.pGameplayMap[worldIdx].ressourceCount == 0)
                _Game.levelInfo.pGameplayMap[worldIdx].tileType = tT_grass; // no `change_tile_to` usage because we check earlier
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
          if (_Game.levelInfo.pGameplayMap[worldIdx].tileType == tT_fire)
          {
            if (_Game.levelInfo.pGameplayMap[worldIdx].ressourceCount > 0)
              modify_with_clamp(_actor.pItem->temperature, (int16_t)(200), (uint8_t)(0), MaxTemperature);

            // for testing: remove from fire & remove fire when empty
            lsAssert(_Game.levelInfo.pGameplayMap[worldIdx].ressourceCount > 0);
            _Game.levelInfo.pGameplayMap[worldIdx].ressourceCount--;

            if (_Game.levelInfo.pGameplayMap[worldIdx].ressourceCount == 0)
              _Game.levelInfo.pGameplayMap[worldIdx].tileType = tT_fire_pit;
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

// Can we achieve actors going to a target further away, as long as the nearest target is, full/empty/whatever? I don't know how, besides from adding loads of tile types, or maybe flags for full types, and then more direction lookups (one for all the empty once, one for the full once). As there will also be actors who would want to go to full ones.

// currently tiles have amounts, do we want to use these for pontential different watering states or should they have a different variable?
// how should we handle a sapling neeeding water anyways? it would need to be different sapling type or we can't find to unwatered ones

// TO FIX: pathfinding towards multitypes like `meals` specififcally e.g. to add more of the resource
// TO FIX: handling the `gameplay map` and the `pathfinding lookup` not matching because the tile's resourcetype was only changes recently

//////////////////////////////////////////////////////////////////////////

void update_lumberjack()
{
  static const pathfinding_target_type target_from_state[laS_count] = { ptT_soil, ptT_water, ptT_sapling, ptT_tree, ptT_trunk };

  for (const auto _actor : _LumberjackActors)
  {
    lumberjack_actor *pLumberjack = _actor.pItem;
    movement_actor *pActor = pool_get(_Game.movementActors, pLumberjack->index);

    if (pActor->atDestination)
    {
      // Handle Survival
      if (pActor->survivalActorActive)
      {
        pActor->survivalActorActive = false;
        pActor->target = target_from_state[pLumberjack->state];
        pActor->atDestination = false;
        continue;
      }

      const size_t tileIdx = worldPosToTileIndex(pActor->pos);

      switch (pLumberjack->state)
      {
      case laS_plant:
      {
        if (change_tile_to(tT_sapling, tT_soil, tileIdx))
        {
          pLumberjack->state = (lumberjack_actor_state)((pLumberjack->state + 1) % laS_count);
          pActor->target = target_from_state[pLumberjack->state];
        }

        pActor->atDestination = false;

        break;
      }
      case laS_getWater:
      {
        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_water && _Game.levelInfo.pGameplayMap[tileIdx].ressourceCount > 0)
        {
          lsAssert(!pLumberjack->hasItem);

          pLumberjack->hasItem = true;
          pLumberjack->item = tT_water;

          //if (_Game.levelInfo.pGameplayMap[tileIdx].ressourceCount == 0)
            //_Game.levelInfo.pGameplayMap[tileIdx].tileType = tT_sand;

          //_Game.levelInfo.pGameplayMap[tileIdx].ressourceCount--; // no check because the water will be set to sand once the count is 0

          pLumberjack->state = (lumberjack_actor_state)((pLumberjack->state + 1) % laS_count);
          pActor->target = target_from_state[pLumberjack->state];
        }

        pActor->atDestination = false;

        break;
      }
      case laS_water:
      {
        if (change_tile_to(tT_tree, tT_sapling, tileIdx))
        {
          pLumberjack->hasItem = false;

          pLumberjack->state = (lumberjack_actor_state)((pLumberjack->state + 1) % laS_count);
          pActor->target = target_from_state[pLumberjack->state];
        }

        pActor->atDestination = false;

        break;
      }
      case laS_chop:
      {
        if (change_tile_to(tT_trunk, tT_tree, tileIdx))
        {
          pLumberjack->state = (lumberjack_actor_state)((pLumberjack->state + 1) % laS_count);
          pActor->target = target_from_state[pLumberjack->state];
        }

        pActor->atDestination = false;

        break;
      }
      case laS_cut:
      {
        if (change_tile_to(tT_wood, tT_trunk, tileIdx))
        {
          _Game.levelInfo.pGameplayMap[tileIdx].ressourceCount = 8; // todo adapt this, but should be fine, also look after how often they go to fire

          pLumberjack->state = (lumberjack_actor_state)((pLumberjack->state + 1) % laS_count);
          pActor->target = target_from_state[pLumberjack->state];
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

void update_cook()
{
  constexpr uint8_t IngridientAmountPerFood[(_tile_type_food_last + 1) - _tile_type_food_first][(_ptT_nutrition_last + 1) - _ptT_nutrition_first] =
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
      if (pActor->atDestination)
      {
        pActor->survivalActorActive = false;
        pCook->state = caS_check_inventory;
        pActor->atDestination = false;
      }

      continue;
    }

    lsAssert(pCook->currentCookingItem >= _tile_type_food_first && pCook->currentCookingItem <= _tile_type_food_last);

    // Handle Cook States

    if (pCook->state == caS_check_inventory) // Handling `caS_check_inventory` here because it does not need to be checked for `atDestination`
    {
      bool anyItemMissing = false;

      for (size_t i = 0; i < LS_ARRAYSIZE(pCook->inventory); i++)
      {
        if (pCook->inventory[i] <= 0 && IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first][i] > 0)
        {
          anyItemMissing = true;

          const pathfinding_target_type targetPlant = (pathfinding_target_type)(i + _ptT_nutrient_sources_first);
          pCook->searchingPlant = targetPlant;

          if (_Game.levelInfo.resources[targetPlant].pDirectionLookup[1 - _Game.levelInfo.resources[targetPlant].write_direction_idx][worldPosToTileIndex(pActor->pos)].dir != d_unfillable)
          {
            if (_Game.levelInfo.resources[targetPlant].pDirectionLookup[1 - _Game.levelInfo.resources[targetPlant].write_direction_idx][worldPosToTileIndex(pActor->pos)].dir == d_unreachable)
            {
              pCook->state = caS_plant;
              pActor->target = ptT_soil;
              pActor->atDestination = false;
            }
            else
            {
              pCook->state = caS_harvest;
              pActor->target = targetPlant;
              pActor->atDestination = false;
            }
          }

          continue;
        }
      }

      // if all items in inventory
      if (!anyItemMissing)
      {
        pCook->state = caS_cook;

        // take any item as target to drop of the food
        for (size_t i = 0; i < LS_ARRAYSIZE(IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first]); i++)
          if (IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first][i])
            pActor->target = (pathfinding_target_type)(i + _ptT_nutrition_first);

        pActor->atDestination = false;
      }

      continue;
    }

    // Handling remaining Cook States
    if (pActor->atDestination)
    {
      const size_t tileIdx = worldPosToTileIndex(pActor->pos);
      lsAssert(_Game.levelInfo.resources[pActor->target].pDirectionLookup[1 - _Game.levelInfo.resources[pActor->target].write_direction_idx][tileIdx].dir == d_atDestination);

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
          if (_Game.levelInfo.pGameplayMap[tileIdx].ressourceCount > 0)
          {
            // take item
            constexpr int16_t AddedResourceAmount = 4;
            modify_with_clamp(pCook->inventory[pActor->target - _ptT_nutrient_sources_first], AddedResourceAmount);

            // remove
            modify_with_clamp(_Game.levelInfo.pGameplayMap[tileIdx].ressourceCount, -AddedResourceAmount);

            if (_Game.levelInfo.pGameplayMap[tileIdx].ressourceCount == 0)
              _Game.levelInfo.pGameplayMap[tileIdx].tileType = tT_soil; // no usage of `change_tile_to` due to earlier check of `resource_type`

            pCook->state = caS_check_inventory;
          }
          else
          {
            pCook->state = caS_plant;
            pActor->target = ptT_soil;
          }
        }

        pActor->atDestination = false;

        break;
      }

      case caS_plant:
      {
        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType == tT_soil)
        {
          lsAssert(pActor->target == ptT_soil);

          constexpr uint8_t AddedAmountToPlant = 12;
          _Game.levelInfo.pGameplayMap[tileIdx].tileType = (resource_type)(pCook->searchingPlant);
          _Game.levelInfo.pGameplayMap[tileIdx].ressourceCount = AddedAmountToPlant;

          pCook->state = caS_harvest;
          pActor->target = pCook->searchingPlant;
        }

        pActor->atDestination = false;

        break;
      }

      case caS_cook:
      {
        constexpr uint8_t AddedCookedItemAmount = 64;

        if (_Game.levelInfo.pGameplayMap[tileIdx].tileType != pCook->currentCookingItem)
        {
          if (pActor->target != ptT_grass) // if we are not at the correct foodtype.
          {
            pActor->target = ptT_grass;
            pActor->atDestination = false;

            break;
          }
          else // if we are at grass as alternative dropoff point
          {
            if (change_tile_to(pCook->currentCookingItem, tT_grass, tileIdx))
              _Game.levelInfo.pGameplayMap[tileIdx].ressourceCount = AddedCookedItemAmount;
          }
        }
        else // if we are at the correct foodtype.
        {
          modify_with_clamp(_Game.levelInfo.pGameplayMap[tileIdx].ressourceCount, AddedCookedItemAmount);
        }

        for (size_t i = 0; i < LS_ARRAYSIZE(pCook->inventory); i++)
        {
          lsAssert(pCook->inventory[i] >= IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first][i]);
          pCook->inventory[i] -= IngridientAmountPerFood[pCook->currentCookingItem - _tile_type_food_first][i];
        }

        // change to next food item
        pCook->currentCookingItem = (resource_type)((((pCook->currentCookingItem - _tile_type_food_first) + 1) % (_tile_type_food_last + 1 - _tile_type_food_first)) + _tile_type_food_first);
        pCook->state = caS_check_inventory;

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

void update_fireActor()
{
  constexpr pathfinding_target_type target_from_state[faS_count] = { ptT_wood, ptT_fire_pit, ptT_fire };

  // TODO: only start fire at night, extinguish fire when the day starts (fire should propably keep it's wood resources)

  for (const auto _actor : _FireActors)
  {
    fire_actor *pFireActor = _actor.pItem;
    movement_actor *pActor = pool_get(_Game.movementActors, pFireActor->index);

    if (pActor->atDestination)
    {
      // Handle Survival
      if (pActor->survivalActorActive)
      {
        pActor->survivalActorActive = false;
        pActor->target = target_from_state[pFireActor->state];
        pActor->atDestination = false;

        continue;
      }

      const size_t posIdx = worldPosToTileIndex(pActor->pos);
      switch (_actor.pItem->state)
      {
      case faS_get_wood:
      {
        constexpr int16_t AddedWood = 3;

        if (_Game.levelInfo.pGameplayMap[posIdx].tileType == tT_wood)
        {
          if (_Game.levelInfo.pGameplayMap[posIdx].ressourceCount > 0)
          {
            modify_with_clamp(pFireActor->wood_inventory, lsMin(AddedWood, (int16_t)_Game.levelInfo.pGameplayMap[posIdx].ressourceCount));
            modify_with_clamp(_Game.levelInfo.pGameplayMap[posIdx].ressourceCount, -AddedWood);

            if (_Game.levelInfo.pGameplayMap[posIdx].ressourceCount == 0)
              _Game.levelInfo.pGameplayMap[posIdx].tileType = tT_soil;

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

        if (_Game.isNight)
        {
          if (_Game.levelInfo.pGameplayMap[posIdx].tileType == tT_fire_pit)
          {
            if (_Game.levelInfo.pGameplayMap[posIdx].ressourceCount > WoodPerFire)
            {
              _Game.levelInfo.pGameplayMap[posIdx].tileType = tT_fire;
            }
            else
            {
              // if has wood: add wood to fire
              if (pFireActor->wood_inventory >= WoodPerFire)
              {
                pFireActor->wood_inventory -= WoodPerFire;
                _Game.levelInfo.pGameplayMap[posIdx].tileType = tT_fire;
                modify_with_clamp(_Game.levelInfo.pGameplayMap[posIdx].ressourceCount, WoodPerFire, (uint8_t)(0), _Game.levelInfo.pGameplayMap[posIdx].maxRessourceCount);
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
          pFireActor.state = faS_extinguish_fire;
          pActor->target = target_from_state[faS_extinguish_fire];
        }

        pActor->atDestination = false;

        break;
      }
      case faS_extinguish_fire:
      {
        // if day: extinguish
        // else: state to start fire

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

  if (ticksSincelastDayNightSwitch == DayLength && !_Game.isNight)
  {
    _Game.isNight = true;
    ticksSincelastDayNightSwitch = 0;
  }
  else if (ticksSincelastDayNightSwitch == NightLength && _Game.isNight)
  {
    _Game.isNight = false;
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
  update_cook();
  update_fireActor();
}

//////////////////////////////////////////////////////////////////////////

// Changing tiles for debugging

size_t playerMapIndex = 0;

void game_playerSwitchTiles(const resource_type terrainType)
{
  lsAssert(playerMapIndex < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(terrainType < tT_count);

  _Game.levelInfo.pGameplayMap[playerMapIndex].tileType = terrainType;
}

void game_setPlayerMapIndex(const bool left)
{
  if (left)
  {
    if (playerMapIndex > 0)
      playerMapIndex--;
  }
  else
  {
    if (playerMapIndex < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)
      playerMapIndex++;
  }
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

//////////////////////////////////////////////////////////////////////////

bool game_hasAnyEvent(game *pGame)
{
  return pGame != nullptr && queue_any(&pGame->events);
}

gameEvent game_getNextEvent(game *pGame)
{
  gameEvent ret;

  if (pGame != nullptr && LS_SUCCESS(queue_popFront(&pGame->events, &ret)))
    pGame->eventUpdateCutoffIndex = ret.index;
  else
    ret.type = geT_invalid;

  return ret;
}

size_t game_getTickRate()
{
  return _Game.tickRate;
}

size_t game_getLevelSize()
{
  return _LevelSize;
}

//////////////////////////////////////////////////////////////////////////

template <typename T>
lsResult pool_serialize(_Out_ data_blob *pBlob, pool<T> *pPool)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pPool == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  LS_ERROR_CHECK(data_blob_appendValue(pBlob, pPool->count));

  for (auto item : *pPool)
  {
    LS_ERROR_CHECK(data_blob_appendValue(pBlob, item.index));
    LS_ERROR_CHECK(data_blob_append(pBlob, item.pItem));
  }

epilogue:
  return result;
}

template <typename T>
lsResult pool_deserialize(data_blob *pBlob, _Out_ pool<T> *pPool)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pPool == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  pool_clear(pPool);

  size_t count;
  LS_ERROR_CHECK(data_blob_read(pBlob, &count));

  for (size_t i = 0; i < count; i++)
  {
    uint64_t index;
    LS_ERROR_CHECK(data_blob_read(pBlob, &index));

    T item;
    LS_ERROR_CHECK(data_blob_read(pBlob, &item));

    LS_ERROR_CHECK(pool_insertAt(pPool, item, index));
  }

epilogue:
  return result;
}

template <typename T>
lsResult queue_serialize(_Out_ data_blob *pBlob, queue<T> *pQueue)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  queue_clear(pQueue);

  LS_ERROR_CHECK(data_blob_appendValue(pBlob, pQueue->count));

  for (size_t i = 0; i < pQueue->count; i++)
    LS_ERROR_CHECK(data_blob_appendValue(pBlob, queue_get(pQueue, i)));

epilogue:
  return result;
}

template <typename T>
lsResult queue_deserialize(_Out_ data_blob *pBlob, queue<T> *pQueue)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  size_t count;
  LS_ERROR_CHECK(data_blob_read(pBlob, &count));

  for (size_t i = 0; i < count; i++)
  {
    T item;
    LS_ERROR_CHECK(data_blob_read(pBlob, &item));

    LS_ERROR_CHECK(queue_pushBack(pQueue, &item));
  }

epilogue:
  return result;
}

lsResult game_serialize(_Out_ data_blob *pBlob)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pBlob == nullptr, lsR_ArgumentNull);

  data_blob_reset(pBlob);

  LS_ERROR_CHECK(data_blob_appendValue(pBlob, (uint64_t)0)); // size.

  // Append data_blobs / Serialize pools.

  *reinterpret_cast<uint64_t *>(pBlob->pData) = pBlob->size; // replace size.

epilogue:
  return result;
}

lsResult game_deserialze(_Out_ game *pGame, _In_ const void *pData, const size_t size)
{
  lsResult result = lsR_Success;

  data_blob blob;

  LS_ERROR_IF(pGame == nullptr || pData == nullptr || size == 0, lsR_ArgumentNull);

  data_blob_createFromForeign(&blob, pData, size);

  size_t actualSize;
  LS_ERROR_CHECK(data_blob_read(&blob, &actualSize));

  LS_ERROR_IF(actualSize > size, lsR_ResourceInvalid);
  blob.size = actualSize;

  // Read data_blobs / Deserialize pools.

epilogue:
  return result;
}

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local()
{
  lsResult result = lsR_Success;

  initializeLevel();

  // Create NullEntity / NullObject.

  size_t zero;

  // Add them to their pools.

  if constexpr (UseBox2DPhysics)
  {
    _pBox2DWorld = new b2World(b2Vec2_zero);

    pool_add(&_Box2DObjects, (b2Body *)nullptr, &zero);
    lsAssert(zero == 0);
  }

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

  // Remove old events.
  {
    const uint64_t cutoffNs = tick - 1000000000;

    while (queue_any(&_Game.events) && queue_get(&_Game.events, 0).timeNs < cutoffNs)
    {
      gameEvent _unused;
      LS_ERROR_CHECK(queue_popFront(&_Game.events, &_unused));
    }
  }

  // stuff.
  // process player interactions.
  {
    game_update();

    goto epilogue;
  epilogue:
    return result;
  }
}

//////////////////////////////////////////////////////////////////////////

void game_removeEntity_internal(const size_t entityIndex)
{
  entity entity;
  pool_remove_safe(&_Game.entities, entityIndex, &entity);
}

//void game_addComponentToEntity_internal(const size_t entityIndex, component_type component, const size_t index)
//{
//  
//}

void game_addEvent_internal(gameEvent *pEvent)
{
  pEvent->index = ++_Game.lastEventIndex;
  pEvent->timeNs = lsGetCurrentTimeNs();

  queue_pushBack(&_Game.events, pEvent);
}

//////////////////////////////////////////////////////////////////////////

//void game_phys_addVelocity(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f force)
//{
//  //if (pObj == nullptr)
//  //  pObj = pool_get(&_Game.gameObjects, entityIndex);
//  //
//  //pObj->velocity += force;
//  //
//  //if constexpr (UseBox2DPhysics)
//  //{
//  //  b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
//  //  pBody->ApplyLinearImpulseToCenter(b2Vec2(force.x, force.y), true);
//  //}
//}

//void game_phys_addAngularForce(const size_t entityIndex, OPTIONAL gameObject *pObj, const float_t force)
//{
//  //if (pObj == nullptr)
//  //  pObj = pool_get(&_Game.gameObjects, entityIndex);
//  //
//  //pObj->angularVelocity += force;
//  //
//  //if constexpr (UseBox2DPhysics)
//  //{
//  //  b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
//  //  pBody->ApplyAngularImpulse(force, true);
//  //}
//}

//void game_phys_setTransform(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f position, const float_t rotation)
//{
//  //if (pObj == nullptr)
//  //  pObj = pool_get(&_Game.gameObjects, entityIndex);
//  //
//  //pObj->position = position;
//  //pObj->rotation = rotation;
//  //
//  //if constexpr (UseBox2DPhysics)
//  //{
//  //  b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
//  //  pBody->SetTransform(b2Vec2(position.x, position.y), rotation);
//  //}
//}

//void game_phys_setTransformAndImpulse(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f position, const float_t rotation, const vec2f velocity, const float_t angularVelocity)
//{
//  //if (pObj == nullptr)
//  //  pObj = pool_get(&_Game.gameObjects, entityIndex);
//  //
//  //pObj->position = position;
//  //pObj->rotation = rotation;
//  //pObj->velocity = velocity;
//  //pObj->angularVelocity = angularVelocity;
//  //
//  //if constexpr (UseBox2DPhysics)
//  //{
//  //  b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
//  //  pBody->SetTransform(b2Vec2(position.x, position.y), rotation);
//  //  pBody->SetLinearVelocity(b2Vec2(velocity.x, velocity.y));
//  //  pBody->SetAngularVelocity(angularVelocity);
//  //  pBody->SetAwake(true);
//  //}
//}
