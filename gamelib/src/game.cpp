#include "game.h"

#include "box2d/box2d.h"

#include "local_list.h"

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

constexpr uint8_t _DirectionsBits = 4;

void initializeLevel();
void mapInit(const size_t width, const size_t height);
void updateFloodfill();
void setTerrain();

constexpr size_t _FloodFillSteps = 10;

bool floodfill(size_t ressourceIndex);
void floodfill_suggestNextTarget(size_t ressourceIndex, const size_t nextIndex, const direction dir);


//////////////////////////////////////////////////////////////////////////

void movementActor_move();

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
  for (size_t i = 0; i < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; i++)
    _Game.levelInfo.pMap[i] = (terrain_type)(lsGetRand() % tT_Count);
  //_Game.levelInfo.pMap[i] = tT_grass;

//_Game.levelInfo.pMap[size_t(_Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y * 0.5 + _Game.levelInfo.map_size.x * 0.5)] = tT_sand;
  _Game.levelInfo.pMap[34] = tT_grass;

  // Setting borders to tT_mountain, so they're collidable
  {
    for (size_t y = 0; y < _Game.levelInfo.map_size.y; y++)
    {
      _Game.levelInfo.pMap[y * _Game.levelInfo.map_size.x] = tT_mountain;
      _Game.levelInfo.pMap[(y + 1) * _Game.levelInfo.map_size.x - 1] = tT_mountain;
    }

    for (size_t x = 0; x < _Game.levelInfo.map_size.x; x++)
    {
      _Game.levelInfo.pMap[x] = tT_mountain;
      _Game.levelInfo.pMap[x + (_Game.levelInfo.map_size.y - 1) * _Game.levelInfo.map_size.x] = tT_mountain;
    }
  }
}

#ifndef _DEBUG
__declspec(__forceinline)
#endif
void floodfill_suggestNextTarget(size_t ressourceIndex, const size_t nextIndex, const direction dir)
{
  direction nextTile = (direction)_Game.levelInfo.resources[ressourceIndex].pDirectionLookup[_Game.levelInfo.resources[ressourceIndex].write_direction_idx][nextIndex];

  if (nextTile != d_unfillable && nextTile == d_unreachable)
  {
    _Game.levelInfo.resources[ressourceIndex].pDirectionLookup[_Game.levelInfo.resources[ressourceIndex].write_direction_idx][nextIndex] = dir;
    queue_pushBack(&_Game.levelInfo.resources[ressourceIndex].pathfinding_queue, { nextIndex });
  }
}

bool floodfill(size_t ressourceIndex)
{
  fill_step current;
  size_t stepCount = 0;

  while (_Game.levelInfo.resources[ressourceIndex].pathfinding_queue.count)
  {
    if (stepCount > _FloodFillSteps)
      return false;

    queue_popFront(&_Game.levelInfo.resources[ressourceIndex].pathfinding_queue, &current);
    lsAssert(current.index >= 0 && current.index < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

    const size_t isOddBit = (current.index / _Game.levelInfo.map_size.x) & 1;
    const size_t topLeftIndex = current.index - _Game.levelInfo.map_size.x - (size_t)!isOddBit;
    const size_t bottomLeftIndex = current.index + _Game.levelInfo.map_size.x - (size_t)!isOddBit;

    floodfill_suggestNextTarget(ressourceIndex, current.index - 1, d_left);
    floodfill_suggestNextTarget(ressourceIndex, current.index + 1, d_right);
    floodfill_suggestNextTarget(ressourceIndex, bottomLeftIndex + 1, d_bottomRight); // bottomRight and bottomLeft are flipped to not give the right side all the paths
    floodfill_suggestNextTarget(ressourceIndex, bottomLeftIndex, d_bottomLeft);
    floodfill_suggestNextTarget(ressourceIndex, topLeftIndex, d_topLeft);
    floodfill_suggestNextTarget(ressourceIndex, topLeftIndex + 1, d_topRight);

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

  for (size_t i = 0; i < tT_Count - 1; i++) // Skipping tT_mountain, as this is our collidable stuff atm.
  {
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[0], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
    lsAllocZero(&_Game.levelInfo.resources[i].pDirectionLookup[1], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

    for (size_t j = 0; j < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; j++)
    {
      if (_Game.levelInfo.pMap[j] == i)
      {
        queue_pushBack(&_Game.levelInfo.resources[i].pathfinding_queue, { j });
        _Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx][j] = d_unfillable;
      }

      if (_Game.levelInfo.pMap[j] == tT_mountain)
        _Game.levelInfo.resources[i].pDirectionLookup[_Game.levelInfo.resources[i].write_direction_idx][j] = d_unfillable;
    }
  }

  // Spawn Actors
  vec2f pos = { 2, 2 };
  terrain_type target = tT_sand;
  lsAssert(_Game.levelInfo.pMap[34] != tT_mountain);
  lsAssert(_Game.levelInfo.pMap[34] != target);

  //_Game.movementActors.push_back({ pos, target, false });
  _Game.actor = { pos, target, false };
}

void updateFloodfill()
{
  for (size_t i = 0; i < tT_Count - 1; i++) // Skipping tT_mountain, as this is our collidable stuff atm.
  {
    if (floodfill(i))
    {
      lsAssert(!_Game.levelInfo.resources[i].pathfinding_queue.count);

      size_t newDirectionIndex = 1 - _Game.levelInfo.resources[i].write_direction_idx;
      _Game.levelInfo.resources[i].write_direction_idx = newDirectionIndex;

      lsZeroMemory(_Game.levelInfo.resources[i].pDirectionLookup[newDirectionIndex], _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);

      for (size_t j = 0; j < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y; j++)
      {
        if (_Game.levelInfo.pMap[j] == i)
        {
          _Game.levelInfo.resources[i].pDirectionLookup[newDirectionIndex][j] = d_unfillable;
          queue_pushBack(&_Game.levelInfo.resources[i].pathfinding_queue, { j });
        }

        if (_Game.levelInfo.pMap[j] == tT_mountain)
          _Game.levelInfo.resources[i].pDirectionLookup[newDirectionIndex][j] = d_unfillable;
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// TODO: To conclude from the movementActor to the associated actor have a lookUp for every map tile who is on it.

size_t worldPosToTileIndex(vec2f pos)
{
  float_t x_even = lsFloor(lsClamp(pos.x, 0.f, (float_t)_Game.levelInfo.map_size.x) + 0.5f);
  float_t y_even = lsFloor(lsClamp(pos.y, 0.f, (float_t)_Game.levelInfo.map_size.y) * 0.5f + 0.5f) * 2.f;

  float_t x_odd = lsFloor(lsClamp(pos.x, 0.f, (float_t)_Game.levelInfo.map_size.x)); // x_even is shifted by -0.5 compared to the world pos.
  float_t y_odd = lsFloor(lsClamp((pos.y - 1) * 0.5f, 0.f, (float_t)_Game.levelInfo.map_size.y) + 0.5f) * 2.f + 1.f;

  //float_t dist_even = lsAbs((x_even + y_even) - (pos.x + pos.y));
  //float_t dist_odd = lsAbs((x_odd + 0.5f + y_odd) - (pos.x + pos.y));

  float_t dist_even = vec2f(x_even - pos.x, y_even - pos.y).LengthSquared();
  float_t dist_odd = vec2f(x_odd - (pos.x - 0.5f), y_odd - pos.y).LengthSquared();

  if (dist_even < dist_odd)
    return (size_t)(y_even * _Game.levelInfo.map_size.x + x_even);
  else
    return (size_t)(y_odd * _Game.levelInfo.map_size.x + x_odd);
}

void movementActor_move()
{
  const vec2f dir[6] = { vec2f(-0.5, 1), vec2f(-1, 0), vec2f(-0.5, -1), vec2f(0.5, -1), vec2f(1, 0), vec2f(0.5, 1) };

  //for (movementActor actor : _Game.movementActors)
  {
    size_t idx = worldPosToTileIndex(_Game.actor.pos);

    print(idx, ", ", _Game.actor.pos, '\n');

    // Check if arrived at Target
    if (_Game.levelInfo.pMap[idx] == _Game.actor.target)
    {
      //actor.target = terrain_type(lsGetRand() % (tT_Count - 1));
      _Game.actor.atDestination = true;
    }

    direction currentDir = (direction)_Game.levelInfo.resources[_Game.actor.target].pDirectionLookup[1 - _Game.levelInfo.resources[_Game.actor.target].write_direction_idx][idx];
    //lsAssert(currentDir != d_unreachable);

    const float_t velocity = 0.01;
    // Move to Target
    if (currentDir != d_unfillable && currentDir != d_unreachable) // this can get to be an assert, if we make sure the actor always gets a new target if at destination.
    {
      _Game.actor.pos += vec2f(velocity) * dir[currentDir - 1];
      //printf("%" PRIu64 " (%f, %f) (%f, %f) \n", idx, _Game.actor.pos.x, _Game.actor.pos.y, (vec2f(velocity) * dir[currentDir - 1]).x, (vec2f(velocity) * dir[currentDir - 1]).y);
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// changing tiles for debugging

size_t playerMapIndex = 0;

void game_playerSwitchTiles(terrain_type terrainType)
{
  lsAssert(playerMapIndex < _Game.levelInfo.map_size.x * _Game.levelInfo.map_size.y);
  lsAssert(terrainType < tT_Count);

  _Game.levelInfo.pMap[playerMapIndex] = terrainType;
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
