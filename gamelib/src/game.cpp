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

// needed for floodfill:
// map:         pointer auf terrainType
// terrainType: enum    -> which terrain
// entityType:  enum    -> which type of Actor, (which destination (collidable?? dann nicht enum))
// entity:      struct  -> entityType, position, collides with what terrainTypes, (destination, better: type gets changed, so no destination in entity...)
// look-up of directions to destinations: one for each possible dest? -> pointer uint8_t? 6 werte for directions, 8 werte 1 fuer nicht erreichbar + 1 war schon jmd hier, passen in 3 bit -> uint64: 21 ziele    2 look-ups: wechelsn immer welcher aktualisiert wird


// ziele die in die map floodfillen wie man zum naechsten ziel des typen kommt
// floodfill look-up map -> sagt pro kachel pro ziel welche richtung

void initializeLevel();
void mapInit(const size_t width, const size_t height);
void updateFloodfill();
void setTerrain();
lsResult spawnActors();

constexpr size_t _FloodFillSteps = 100;

bool floodfill(queue<fill_step> &pathfindQueue, uint8_t *pDirectionLookup, const terrain_element *pMap);
void floodfill_suggestNextTarget(queue<fill_step> &pathfindQueue, uint8_t *pDirectionLookup, const size_t nextIndex, const direction dir, const uint8_t parentElevation, const terrain_element *pMap);

size_t worldPosToTileIndex(vec2f pos);
vec2f tileIndexToWorldPos(const size_t tileIndex);
void movementActor_move();

//////////////////////////////////////////////////////////////////////////

static pool<lumberjack_actor> _LumberjackActors; // Do we need this in the header?

//////////////////////////////////////////////////////////////////////////

// fillterrain and switch maps:
// fill terrain map with enums
// per ressource:
//  either: floodfill for x steps
//  or: if floodfill is ready: fill queue with targets, reset other map to not been there + collidable

// in floodfill: only need to check for not been there -> is the same as checking for !collidable && !alreadyBeenThere

//////////////////////////////////////////////////////////////////////////

void mapInit(const size_t width, const size_t height/*, bool *pCollidableMask*/)
{
  _Game.levelInfo.map_size = { width, height };

  lsAllocZero(&_Game.levelInfo.pMap, height * width);
}

void setTerrain()
{
  rand_seed seed = rand_seed(2, 2); // 2, 2 ist cool am anfang.

  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
  {
    //_Game.levelInfo.pMap[i] = (terrain_type)(lsGetRand() % tT_Count);
    _Game.levelInfo.pMap[i].terrainType = (lsGetRand(seed) & 15) < 12 ? tT_grass : tT_mountain;
    //_Game.levelInfo.pMap[i].terrainType = tT_grass;
    //_Game.levelInfo.pMap[i].elevationLevel = 0;
    _Game.levelInfo.pMap[i].elevationLevel = lsGetRand(seed) % 3;
  }

  //_Game.levelInfo.pMap[size_t(_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y * 0.5 + _Game.levelInfo.map_size.x * 0.5)].terrainType = tT_sand;
  //_Game.levelInfo.pMap[size_t(_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y * 0.5 + _Game.levelInfo.map_size.x * 0.5)].elevationLevel = 3;
  //_Game.levelInfo.pMap[size_t(_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y * 0.5 + _Game.levelInfo.map_size.x * 0.5) + 1].elevationLevel = 2;
  //_Game.levelInfo.pMap[size_t(_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y * 0.5 + _Game.levelInfo.map_size.x * 0.5) + 2].elevationLevel = 1;
  //_Game.levelInfo.pMap[size_t(16 * 16 - 18)] = tT_sand;
  //_Game.levelInfo.pMap[size_t(16 * 2 - 2)] = tT_water;
  //_Game.levelInfo.pMap[34] = tT_grass;
  //_Game.levelInfo.pMap[16 * 16 - (16 + 14)] = tT_grass;

  for (size_t i = 0; i < 3; i++)
  {
    _Game.levelInfo.pMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].terrainType = tT_sand;
    _Game.levelInfo.pMap[lsGetRand(seed) % (_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y)].terrainType = tT_water;
  }

  // Setting borders to tT_mountain, so they're collidable
  {
    for (size_t y = 0; y < _Game.levelInfo.map_size.y; y++)
    {
      _Game.levelInfo.pMap[y * _Game.levelInfo.map_size.x].terrainType = tT_mountain;
      _Game.levelInfo.pMap[(y + 1) * _Game.levelInfo.map_size.x - 1].terrainType = tT_mountain;
    }

    for (size_t x = 0; x < _Game.levelInfo.map_size.x; x++)
    {
      _Game.levelInfo.pMap[x].terrainType = tT_mountain;
      _Game.levelInfo.pMap[x + (_Game.levelInfo.map_size.y - 1) * _Game.levelInfo.map_size.x].terrainType = tT_mountain;
    }
  }
}

lsResult spawnActors()
{
  lsResult result = lsR_Success;

  for (size_t i = 0; i < 5; i++)
  {
    movement_actor actor;
    actor.target = tT_sand; //(terrain_type)(lsGetRand() % (tT_Count - 1));
    actor.pos = vec2f((float_t)((1 + i * 3) % _Game.levelInfo.map_size.x), (float_t)((i * 3 + 1) % _Game.levelInfo.map_size.y));

    while (_Game.levelInfo.pMap[worldPosToTileIndex(actor.pos)].terrainType == tT_mountain)
      actor.pos.x = (float_t)(size_t(actor.pos.x + 1) % _Game.levelInfo.map_size.x);

    size_t _unused;
    LS_ERROR_CHECK(pool_add(&_Game.movementActors, &actor, &_unused));
  }

  goto epilogue;

epilogue:
  return result;
}

//////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG
__declspec(__forceinline)
#endif
void floodfill_suggestNextTarget(queue<fill_step> &pathfindQueue, uint8_t *pDirectionLookup, const size_t nextIndex, const direction dir, const uint8_t parentElevation, const terrain_element *pMap)
{
  direction nextTile = (direction)pDirectionLookup[nextIndex];

  if (nextTile != d_unfillable && nextTile == d_unreachable && lsAbs((int8_t)parentElevation - (int8_t)pMap[nextIndex].elevationLevel) <= 1)
  {
    pDirectionLookup[nextIndex] = dir;
    queue_pushBack(&pathfindQueue, fill_step(nextIndex));
  }
}

bool floodfill(queue<fill_step> &pathfindQueue, uint8_t *pDirectionLookup, const terrain_element *pMap)
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

    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, current.index - 1, d_left, pMap[current.index].elevationLevel, pMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, current.index + 1, d_right, pMap[current.index].elevationLevel, pMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, bottomLeftIndex, d_bottomLeft, pMap[current.index].elevationLevel, pMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, bottomLeftIndex + 1, d_bottomRight, pMap[current.index].elevationLevel, pMap); // bottomRight and bottomLeft are flipped to not give the right side all the paths
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, topLeftIndex, d_topLeft, pMap[current.index].elevationLevel, pMap);
    floodfill_suggestNextTarget(pathfindQueue, pDirectionLookup, topLeftIndex + 1, d_topRight, pMap[current.index].elevationLevel, pMap);

    stepCount++;
  }

  // if queue is empty: floodfill completed
  return true;
}

//////////////////////////////////////////////////////////////////////////

// TODO: have a special map for rendering to know if something needs to be rotated etc.

void initializeLevel()
{
  mapInit(16, 16);
  setTerrain();

  // Set up floodfill queue and lookup
  for (size_t i = 0; i < tT_Count - 1; i++) // Skipping tT_mountain, as this is our collidable stuff atm.
  {
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[0], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[1], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

    for (size_t j = 0; j < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; j++)
    {
      if (_Game.levelInfo.pMap[j].terrainType == i)
      {
        queue_pushBack(&_Game.levelInfo.resources[i].pathfinding_queue, fill_step(j));
        _Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx][j] = d_unfillable;
      }

      if (_Game.levelInfo.pMap[j].terrainType == tT_mountain)
        _Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx][j] = d_unfillable;
    }
  }

  lsAssert(spawnActors() == lsR_Success);
}

void updateFloodfill()
{
  for (size_t i = 0; i < tT_Count - 1; i++) // Skipping tT_mountain, as this is our collidable stuff atm.
  {
    size_t writeIndex = _Game.levelInfo.resources[i].write_direction_idx;

    if (floodfill(_Game.levelInfo.resources[i].pathfinding_queue, _Game.levelInfo.resources[i].pDirectionLookup[writeIndex], _Game.levelInfo.pMap))
    {
      lsAssert(!_Game.levelInfo.resources[i].pathfinding_queue.count);

      size_t newWriteIndex = 1 - writeIndex;
      _Game.levelInfo.resources[i].write_direction_idx = newWriteIndex;

      lsZeroMemory(_Game.levelInfo.resources[i].pDirectionLookup[newWriteIndex], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

      for (size_t j = 0; j < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; j++)
      {
        if (_Game.levelInfo.pMap[j].terrainType == i)
        {
          _Game.levelInfo.resources[i].pDirectionLookup[newWriteIndex][j] = d_unfillable;
          queue_pushBack(&_Game.levelInfo.resources[i].pathfinding_queue, fill_step(j));
        }

        if (_Game.levelInfo.pMap[j].terrainType == tT_mountain)
          _Game.levelInfo.resources[i].pDirectionLookup[newWriteIndex][j] = d_unfillable;
      }
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
    const direction currentTileDirectionType = (direction)_Game.levelInfo.resources[_actor.pItem->target].pDirectionLookup[1 - _Game.levelInfo.resources[_actor.pItem->target].write_direction_idx][currentTileIdx];

    if (currentTileIdx != _actor.pItem->lastTickTileIdx)
    {
      // Check if arrived at Target
      if (_Game.levelInfo.pMap[currentTileIdx].terrainType == _actor.pItem->target)
      {
        _actor.pItem->target = _actor.pItem->target == tT_water ? tT_sand : tT_water;
        _actor.pItem->atDestination = true;
        _actor.pItem->direction = vec2f(0);
        continue;
      }

      if (currentTileDirectionType != d_unreachable && currentTileDirectionType != d_unfillable)
      {
        const vec2f directionLut[6] = { vec2f(-0.5, 1), vec2f(-1, 0), vec2f(-0.5, -1), vec2f(0.5, -1), vec2f(1, 0), vec2f(0.5, 1) };
        const vec2f direction = directionLut[currentTileDirectionType - 1];
        const vec2f tilePos = tileIndexToWorldPos(currentTileIdx);
        const vec2f destinationPos = tilePos + direction;

        _actor.pItem->direction = (destinationPos - _actor.pItem->pos).Normalize();
      }
      else if (_Game.levelInfo.pMap[currentTileIdx].terrainType == tT_mountain)
      {
        _actor.pItem->direction = (tileIndexToWorldPos(lastTileIdx) - _actor.pItem->pos).Normalize();
      }
    }

    if (currentTileDirectionType != d_unreachable) // currentDir == unfillable should always be because we're at tT_mountain. We should never be on our target tile here.
      _actor.pItem->pos += vec2f(0.1) * _actor.pItem->direction;

    _actor.pItem->lastTickTileIdx = currentTileIdx;
  }
}

//////////////////////////////////////////////////////////////////////////

void update_lifesupportActors()
{
  static const size_t nutritionThreshold = 1;

  for (auto _actor : _Game.lifesupportActors)
  {
    for (size_t nt = 0; nt < nT_count; nt++)
    {
      if (_actor.pItem->nutritions[nt] < nutritionThreshold) // how should this work? do we just loose nutritions over time or whilst working/moving? TODO: decide and adapt this accordingly
      {
        
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void update_lumberjack() // WIP I guess...
{
  static const ressource_type target_from_state[laS_count] = { tT_sapling, tT_tree, tT_trunk, tT_wood };

  for (const auto _actor : _LumberjackActors)
  {
    lumberjack_actor *pLumberjack = _actor.pItem;

    if (pLumberjack->pActor->atDestination && pLumberjack->pActor->target == target_from_state[pLumberjack->state])
      pLumberjack->state = (lumberjack_actor_state)((pLumberjack->state + 1) % laS_count);

    pLumberjack->pActor->target = target_from_state[pLumberjack->state];
  }
}

//////////////////////////////////////////////////////////////////////////

// Changing tiles for debugging

size_t playerMapIndex = 0;

void game_playerSwitchTiles(ressource_type terrainType)
{
  lsAssert(playerMapIndex < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(terrainType < tT_Count);

  _Game.levelInfo.pMap[playerMapIndex].terrainType = terrainType;
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
