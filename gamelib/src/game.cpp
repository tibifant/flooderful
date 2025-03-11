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

//////////////////////////////////////////////////////////////////////////

void mapInit(const size_t width, const size_t height/*, bool *pCollidableMask*/)
{
  _Game.levelInfo.map_size = { width, height };

  lsAllocZero(&_Game.levelInfo.pPathfindingMap, height * width);
  //lsAllocZero(&_Game.levelInfo.pRenderMap, height * width);
}

void setTerrain()
{
  rand_seed seed = rand_seed(2, 2); // 2, 2 ist cool am anfang.

  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
  {
    _Game.levelInfo.pGameplayMap[i].tileType = (lsGetRand(seed) & 15) < 12 ? tT_grass : tT_mountain;
    _Game.levelInfo.pPathfindingMap[i].elevationLevel = lsGetRand(seed) % 3;
  }

  for (size_t i = 0; i < 3; i++)
  {
    _Game.levelInfo.pGameplayMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].tileType = tT_sand;
    _Game.levelInfo.pGameplayMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].tileType = tT_water;
  }

  for (size_t i = 0; i < ptT_Count; i++)
  {
    _Game.levelInfo.pGameplayMap[(i + 1 + _Game.levelInfo.map_size.x) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].tileType = resource_type(i);
    _Game.levelInfo.pPathfindingMap[(i + 1 + _Game.levelInfo.map_size.x) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].elevationLevel = 1;
  }

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

  for (size_t i = 0; i < 5; i++)
  {
    movement_actor actor;
    actor.target = ptT_sapling; //(terrain_type)(lsGetRand() % (tT_Count - 1));
    actor.pos = vec2f((float_t)((1 + i * 3) % _Game.levelInfo.map_size.x), (float_t)((i * 3 + 1) % _Game.levelInfo.map_size.y));

    while (_Game.levelInfo.pGameplayMap[worldPosToTileIndex(actor.pos)].tileType == tT_mountain)
      actor.pos.x = (float_t)(size_t(actor.pos.x + 1) % _Game.levelInfo.map_size.x);

    size_t index;
    LS_ERROR_CHECK(pool_add(&_Game.movementActors, &actor, &index));

    lumberjack_actor lj_actor;
    lj_actor.state = laS_plant;
    lj_actor.pActor = pool_get(&_Game.movementActors, index);

    size_t la_index;
    LS_ERROR_CHECK(pool_add(&_LumberjackActors, &lj_actor, &la_index));

    lifesupport_actor ls_actor;
    ls_actor.entityIndex = la_index;
    ls_actor.type = eT_lumberjack;

    for (size_t j = 0; j < LS_ARRAYSIZE(ls_actor.nutritions); j++)
      ls_actor.nutritions[j] = 0;

    for (size_t j = 0; j < LS_ARRAYSIZE(ls_actor.lunchbox); j++)
      ls_actor.lunchbox[j] = 0;

    size_t _unused;
    LS_ERROR_CHECK(pool_add(&_Game.lifesupportActors, &ls_actor, &_unused));
  }

  goto epilogue;

epilogue:
  return result;
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

//////////////////////////////////////////////////////////////////////////

template <pathfinding_target_type p>
struct match_resource;

template<>
struct match_resource<pathfinding_target_type::ptT_carbohydrates>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_wheat || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_collidable>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_mountain; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_fat>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_sunflower || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_grass>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_grass; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_protein>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_bean || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sand>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_sand; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_sapling>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_sapling; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_tree>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_tree; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_trunk>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_trunk; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_vitamin>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_tomato || resourceType == tT_meal; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_water>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_water; };
};

template<>
struct match_resource<pathfinding_target_type::ptT_wood>
{// force inline macro?
  static bool resourceAttribute_matches_resource(const resource_type resourceType) { return resourceType == tT_wood; };
};

template<pathfinding_target_type p>
void fill_resource_info(pathfinding_info *pDirectionLookup, queue<fill_step> &pathfindQueue, gameplay_element *pMap)
{
  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
  {
    if (match_resource<p>::resourceAttribute_matches_resource(pMap[i].targetType))
    {
      queue_pushBack(&pathfindQueue, fill_step(i, 0));
      pDirectionLookup[i].dir = d_unfillable;
    }
    else
    {
      pDirectionLookup[i].dir = (direction)(d_unfillable * (pPathfindingMap[i].targetType == ptT_collidable));
    }
  }
}


void rebuild_resource_info(pathfinding_info *pDirectionLookup, queue<fill_step> &pathfindQueue, pathfinding_element *pPathfindingMap, const pathfinding_target_type type)
{
  lsZeroMemory(pDirectionLookup, _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

  switch (type)
  {
  case ptT_carbohydrates: // call fill_resource_info
  }
}

void initializeLevel()
{
  mapInit(16, 16);
  setTerrain();

  // Set up floodfill queue and lookup
  for (size_t i = 0; i < ptT_Count; i++) // tT_mountain isn't part of pathfinfing_target_type so we don't need to skip it anymore
  {
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[0], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[1], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

    rebuild_resource_info(_Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx], _Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.pPathfindingMap, (pathfinding_target_type)i);
  }

  lsAssert(spawnActors() == lsR_Success);
}

void updateFloodfill()
{
  for (size_t i = 0; i < ptT_Count - 1; i++) // tT_mountain isn't part of pathfinfing_target_type so we don't need to skip it anymore
  {
    size_t writeIndex = _Game.levelInfo.resources[i].write_direction_idx;

    if (floodfill(_Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.resources[i].pDirectionLookup[writeIndex], _Game.levelInfo.pPathfindingMap))
    {
      lsAssert(!_Game.levelInfo.resources[i].pathfinding_queue.count);

      size_t newWriteIndex = 1 - writeIndex;
      _Game.levelInfo.resources[i].write_direction_idx = newWriteIndex;

      lsZeroMemory(_Game.levelInfo.resources[i].pDirectionLookup[newWriteIndex], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

      rebuild_resource_info(_Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx], _Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.pPathfindingMap, (pathfinding_target_type)i);
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// TODO: To conclude from the movementActor to the associated actor have a lookUp for every map tile who is on it.

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

// TODO: Spawn random things that need to be taken to random places.

static size_t r = 0;

void movementActor_move()
{
  r = (r + 1) & 63;

  for (auto _actor : _Game.movementActors)
  {
    const size_t lastTileIdx = _actor.pItem->lastTickTileIdx;

    // Reset lastTile every so often to handle map changes.
    if ((_actor.index & 63) == r)
      _actor.pItem->lastTickTileIdx = 0;

    const size_t currentTileIdx = worldPosToTileIndex(_actor.pItem->pos);
    const direction currentTileDirectionType = (direction)_Game.levelInfo.resources[_actor.pItem->target].pDirectionLookup[1 - _Game.levelInfo.resources[_actor.pItem->target].write_direction_idx][currentTileIdx].dir;

    if (currentTileIdx != _actor.pItem->lastTickTileIdx && currentTileDirectionType != d_unreachable)
    {
      if (currentTileDirectionType == d_unfillable)
      {
        if (_Game.levelInfo.pPathfindingMap[currentTileIdx].targetType == ptT_collidable) // TODO: Can we replace this with mountain or should this be different in case we add other collidable resource types?
        {
          _actor.pItem->direction = (tileIndexToWorldPos(lastTileIdx) - _actor.pItem->pos).Normalize();
        }
        else // no check if we're actually at our target as this should always be true if we're at unfillable but not at tT_mountain
        {
          //_actor.pItem->target = _actor.pItem->target == ptT_water ? ptT_sand : ptT_water;
          _actor.pItem->atDestination = true;
          _actor.pItem->direction = vec2f(0);
          continue;
        }
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

void update_lifesupportActors()
{
  // TODO: ADAPT VALUES TO MAKE SENSE!
  static const size_t EatingThreshold = 1;
  static const size_t AppetiteThreshold = 10;
  static const int16_t MaxNutritionValue = 256;
  static const int16_t MaxFoodItemCount = 256;

  static const size_t nutritionsCount = (_ptT_nutrition_end + 1) - _ptT_nutrition_start;
  static const size_t foodTypeCount = (_tile_type_food_last + 1) - _tile_type_food_begin;
  static const int8_t FoodToNutrition[foodTypeCount][nutritionsCount] = { { 50, 0, 0, 0 } /*tomato*/, {0, 50, 0, 0} /*bean*/, { 0, 0, 50, 0 } /*wheat*/,  { 0, 0, 0, 50 } /*sunflower*/, {25, 25, 25, 25} /*meal*/ }; // foodtypes and nutrition value need to be in the same order as the corresponding enums!

  for (auto _actor : _Game.lifesupportActors)
  {
    // just for testing!!!!
    for (size_t j = 0; j < nutritionsCount; j++)
      _actor.pItem->nutritions[j] = lsClamp((int16_t)((int16_t)_actor.pItem->nutritions[j] - 1), (int16_t)0, MaxNutritionValue);

    // TODO: Add range in pathfinding and walk to nearest item with best score.

    bool anyNeededNutrion = false;

    for (size_t j = 0; j < nutritionsCount; j++)
      if (_actor.pItem->nutritions[j] < EatingThreshold)
        anyNeededNutrion = true;

    if (anyNeededNutrion) // Should we actually just do this once we've obtained the food we currently want to get? so we don't calculate the best item alltough we're already on our way and we don't 
    {
      int8_t bestScore = 0;
      size_t bestIndex = 0;

      for (size_t i = 0; i < LS_ARRAYSIZE(_actor.pItem->lunchbox); i++)
      {
        if (_actor.pItem->lunchbox[i])
        {
          int8_t score = 0;

          for (size_t j = 0; j < nutritionsCount; j++)
            if (FoodToNutrition[i][j] > 0)
              score += _actor.pItem->nutritions[j] < AppetiteThreshold ? nutritionsCount : -1;

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
        for (size_t j = 0; j < nutritionsCount; j++)
        {
          const int16_t val = (int16_t)_actor.pItem->nutritions[j] + FoodToNutrition[bestIndex][j];
          _actor.pItem->nutritions[j] = (size_t)lsClamp(val, (int16_t)0, MaxNutritionValue);
        }

        // remove from lunchbox
        _actor.pItem->lunchbox[bestIndex] = (size_t)lsClamp((int16_t)(_actor.pItem->lunchbox[bestIndex] - 1), (int16_t)0, MaxFoodItemCount);
      }
      else // if no item: set actor target
      {
        size_t lowest = MaxNutritionValue;
        pathfinding_target_type lowestNutrient = ptT_Count;

        for (size_t j = 0; j < nutritionsCount; j++)
        {
          if (_actor.pItem->nutritions[j] < lowest)
          {
            lowest = _actor.pItem->nutritions[j];
            lowestNutrient = (pathfinding_target_type)(j + _ptT_nutrition_start);
          }
        }

        lsAssert(lowestNutrient < ptT_Count);

        switch (_actor.pItem->type)
        {
        case eT_lumberjack:
          lsAssert(lowestNutrient <= _ptT_nutrition_end);
          pool_get(_LumberjackActors, _actor.pItem->entityIndex)->pActor->target = lowestNutrient;
          break;
        case eT_stonemason:
        default:
          lsFail(); // not implemented.
          break;
        }
      }
    }

    // ONLY VERY TEMPORARY FOR TESTING!
    // add food to lunchbox if at dest and dest == fooditem
    switch (_actor.pItem->type)
    {
    case eT_lumberjack:
    {
      lumberjack_actor *pLa = pool_get(_LumberjackActors, _actor.pItem->entityIndex);

      if (pLa->pActor->atDestination && pLa->pActor->target <= _ptT_nutrition_end && pLa->pActor->target >= _ptT_nutrition_start)
      {
        lsAssert(pLa->pActor->target - _ptT_nutrition_start >= 0);
        _actor.pItem->lunchbox[pLa->pActor->target - _ptT_nutrition_start] = lsClamp((size_t)(_actor.pItem->lunchbox[pLa->pActor->target - _ptT_nutrition_start] + 10), (size_t)0, (size_t)MaxFoodItemCount); //THIS IS ONLY FOR TESTING!!! THIS NEEDS TO WORK WITH FOODITEMS THAT ARE ON THE map as targettypes later!
      }

      break;
    }
    case eT_stonemason:
    default:
      lsFail(); // not implemented.
      break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void update_lumberjack() // WIP I guess...
{
  static const pathfinding_target_type target_from_state[laS_count] = { ptT_sapling, ptT_tree, ptT_trunk, ptT_wood };

  for (const auto _actor : _LumberjackActors)
  {
    lumberjack_actor *pLumberjack = _actor.pItem;

    if (pLumberjack->pActor->atDestination)
    {
      if (pLumberjack->pActor->target == target_from_state[pLumberjack->state])
      {
        pLumberjack->state = (lumberjack_actor_state)((pLumberjack->state + 1) % laS_count);
        pLumberjack->pActor->target = target_from_state[pLumberjack->state]; // TODO: handle survival actor may changing the target...
        pLumberjack->pActor->atDestination = false;
      }
      else if (pLumberjack->pActor->target <= _ptT_nutrition_end && pLumberjack->pActor->target >= _ptT_nutrition_start)
      {
        pLumberjack->pActor->target = target_from_state[pLumberjack->state];
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// Changing tiles for debugging

size_t playerMapIndex = 0;

void game_playerSwitchTiles(pathfinding_target_type terrainType)
{
  lsAssert(playerMapIndex < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(terrainType < ptT_Count);

  _Game.levelInfo.pPathfindingMap[playerMapIndex].targetType = terrainType;
}

void game_setPlayerMapIndex(bool left)
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
    updateFloodfill();
    movementActor_move();
    update_lifesupportActors();
    update_lumberjack();

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
