#include "game.h"
#include "gameClient.h"

#include "box2d/box2d.h"

//////////////////////////////////////////////////////////////////////////

static game _Game;
static bool _IsLocal = true;
static size_t _LevelSize = 175;
static b2World *_pBox2DWorld = nullptr;
static pool<b2Body *> _Box2DObjects;

constexpr bool UseBox2DPhysics = true;

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local();
lsResult game_tick_local();
lsResult game_observe_local(_Out_ game *pGame);

//////////////////////////////////////////////////////////////////////////

size_t game_addEntity_internal(entity *pEntity, gameObject *pGameObject);
void game_addEvent_internal(gameEvent *pEvent);

//////////////////////////////////////////////////////////////////////////

void game_phys_addVelocity(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f force);
void game_phys_addAngularForce(const size_t entityIndex, OPTIONAL gameObject *pObj, const float_t force);
void game_phys_setTransform(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f position, const float_t rotation);
void game_phys_setTransformAndImpulse(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f position, const float_t rotation, const vec2f velocity, const float_t angularVelocity);

//////////////////////////////////////////////////////////////////////////

// needed for floodfill:
// map:         pointer auf terrainType
// terrainType: enum    -> which terrain
// entityType:  enum    -> which type of Actor, (which destination (collidible?? dann nicht enum))
// entity:      struct  -> entityType, position, collides with what terrainTypes, (destination, better: type gets changed, so no destination in entity...)
// look-up of directions to destinations: one for each possible dest? -> pointer uint8_t? 6 werte for directions, 8 werte 1 fuer nicht erreichbar + 1 war schon jmd hier, passen in 3 bit -> uint64: 21 ziele    2 look-ups: wechelsn immer welcher aktualisiert wird


// ziele die in die map floodfillen wie man zum naechsten ziel des typen kommt
// floodfill look-up map -> sagt pro kachel pro ziel welche richtung

struct floodfillObject
{
  size_t index;
};

constexpr uint8_t _DirectionsBits = 4;

void initializeFloodfill();
void mapInit(const size_t width, const size_t height);
void updatePathFinding();
void setTerrain(bool *pCollidibleMask);

size_t _FloodFillSteps = 10;

bool floodfill(uint64_t *pPathFindMap, const size_t targetIndex, std::vector<vec2u32> *pDestinations, const bool *pCollidibleMask);
void floodfill_suggestNextTarget(uint64_t *pPathFindMap, const bool *pCollidibleMask, const size_t &nextIndex, const size_t &targetIndex, queue<floodfillObject> &q);


//////////////////////////////////////////////////////////////////////////

// fillterrain and switch maps:
// fill terrain map with enums
// per ressource:
//  either: floodfill for x steps
//  or: if floodfill is ready: fill queue with targets, reset other map to not been there + collidible

// in floodfill: only need to check for not been there -> is the same as checking for !collidible && !alreadyBeenThere

//////////////////////////////////////////////////////////////////////////

void mapInit(const size_t width, const size_t height/*, bool *pCollidibleMask*/)
{
  _Game.mapHeight = height;
  _Game.mapWidth = width;
  
  lsAllocZero(&_Game.pMap, _Game.mapHeight * _Game.mapWidth);
}

#ifndef _DEBUG
__declspec(__forceinline)
#endif
void floodfill_suggestNextTarget(uint64_t *pPathFindMap, queue<floodfillObject> &q, const size_t nextIndex, const bool *pCollidibleMask, const size_t targetIndexShift, const direction dir)
{  
  if (!pCollidibleMask[nextIndex] && !((_Game.pPathFindMap[nextIndex] >> targetIndexShift) & d_atDestination))
  {
    _Game.pPathFindMap[nextIndex] |= ((uint64_t)dir << targetIndexShift);
    lsAssert(_Game.pPathFindMap[nextIndex] < ((uint64_t)0b1000 << targetIndexShift));
    queue_pushBack(&q, { nextIndex });
  }
}

bool floodfill(uint64_t *pPathFindMap, const size_t targetIndex, std::vector<size_t> *pDestinations, const bool *pCollidibleMask)
{
  // TODO: add second pPathFindMap where this gets called.

  // FIXME! the queue needs to stay consistent and can't be filled everytime we call this.

  static queue<floodfillObject> q;
  const size_t targetIndexShift = targetIndex * _DirectionsBits;

  // Enqueue all targets.
  for (const auto &_pos : *pDestinations)
  {
    if (pCollidibleMask[_pos])
      continue;

    _Game.pPathFindMap[_pos] |= ((uint64_t)d_atDestination << targetIndexShift);
    queue_pushBack(&q, { _pos }); //!!!
  }

  floodfillObject current;
  size_t stepCount = 0;
  
  while (q.count)
  {
    if (stepCount > _FloodFillSteps)
      return false;
    
    queue_popFront(&q, &current);
    lsAssert(current.index >= 0 && current.index < _Game.mapHeight *_Game.mapWidth);

    const size_t isOddBit = (current.index / _Game.mapWidth) & 1;
    const size_t topLeftIndex = current.index - _Game.mapWidth - (size_t)!isOddBit;
    const size_t bottomLeftIndex = current.index + _Game.mapWidth - (size_t)!isOddBit;

    // shouldn't we only check the neighbours where we haven't been yet to not quadruple check everything?

    floodfill_suggestNextTarget(q, current.index - 1, pCollidibleMask, targetIndexShift, d_left);
    floodfill_suggestNextTarget(q, current.index + 1, pCollidibleMask, targetIndexShift, d_right);
    floodfill_suggestNextTarget(q, bottomLeftIndex + 1, pCollidibleMask, targetIndexShift, d_bottomRight); // bottomRight and bottomLeft are flipped to not give the right side all the paths
    floodfill_suggestNextTarget(q, bottomLeftIndex, pCollidibleMask, targetIndexShift, d_bottomLeft);
    floodfill_suggestNextTarget(q, topLeftIndex, pCollidibleMask, targetIndexShift, d_topLeft);
    floodfill_suggestNextTarget(q, topLeftIndex + 1, pCollidibleMask, targetIndexShift, d_topRight);

    stepCount++;
  }

  // if queue is empty: floodfill completed
  return true;
}

//////////////////////////////////////////////////////////////////////////

void initializeFloodfill()
{
  mapInit(16, 16);

  lsAllocZero(&_Game.pathFindMaps.pPathFindMapA, _Game.mapHeight * _Game.mapWidth * sizeof(uint64_t));
  lsAllocZero(&_Game.pathFindMaps.pPathFindMapB, _Game.mapHeight * _Game.mapWidth * sizeof(uint64_t));

  _Game.writePathFindMapA = true;
}

void updatePathFinding()
{
  bool *pCollidibleMask;
  lsAllocZero(&pCollidibleMask, _Game.mapHeight * _Game.mapWidth);
  
  // FIXME! this implies that terrain can be changed every tick whilst the destinations only get updated, when the floodfill is finished!
  setTerrain(pCollidibleMask);

  std::vector<size_t> destinations; // FIXME: your damn destinations will be overwritten everytime like this. come on use yout damn brain!

  // TODO: Should get destinations that can be different from terrain_type
  for (size_t i = 1; i < tT_Count; i++) // Skipping tT_mountain for now as it is our collidible border
  {
    // TODO: handle floodfill queue outside of floodfill() or somehow mark in the floodfillmap that we've been to the destination already

    // TODO: fix whole floodfill situation, fix that we don't search for every destination but only for the one's we haven't been to yet.
    // maybe i'm dumb, but if we change the map and therefore the destinations every tick even when we didn't finish our floodfill yet. we won't every finish it.

    // switch maps
    if (_Game.writePathFindMapA)
    {
      if (!destinations.size() || floodfill(_Game.pathFindMaps.pPathFindMapA, i - 1, &destinations, pCollidibleMask))
      {
        destinations.clear();

        for (size_t j = 0; j < _Game.mapHeight * _Game.mapWidth; j++)
          if (_Game.pMap[j] == i)
            destinations.push_back(j);

        _Game.writePathFindMapA = false;
        lsZeroMemory(&_Game.pathFindMaps.pPathFindMapB, _Game.mapHeight * _Game.mapWidth * sizeof(uint64_t));
      }
    }
    else
    {
      if (!destinations.size() || floodfill(_Game.pathFindMaps.pPathFindMapB, i - 1, &destinations, pCollidibleMask))
      {
        destinations.clear();

        for (size_t j = 0; j < _Game.mapHeight * _Game.mapWidth; j++)
          if (_Game.pMap[j] == i)
            destinations.push_back(j);

        _Game.writePathFindMapA = true;
        lsZeroMemory(&_Game.pathFindMaps.pPathFindMapA, _Game.mapHeight * _Game.mapWidth * sizeof(uint64_t));
      }
    }
  }
}

void setTerrain(bool *pCollidibleMask)
{
  for (size_t i = 0; i < _Game.mapHeight * _Game.mapWidth; i++)
  {
    _Game.pMap[i] = (terrain_type)(lsGetRand() % tT_Count);
    //_Game.pMap[i] = tT_grass;

    if (_Game.pMap[i] == tT_mountain)
      pCollidibleMask[i] = 1;
  }

  // Setting borders to tT_mountain, so they're collidible
  {
    for (size_t y = 0; y < _Game.mapHeight; y++)
    {
      _Game.pMap[y * _Game.mapWidth] = tT_mountain;
      _Game.pMap[(y + 1) * _Game.mapWidth - 1] = tT_mountain;
      pCollidibleMask[y * _Game.mapWidth] = true;
      pCollidibleMask[(y + 1) * _Game.mapWidth - 1] = true;
    }

    for (size_t x = 0; x < _Game.mapWidth; x++)
    {
      _Game.pMap[x] = tT_mountain;
      _Game.pMap[x + (_Game.mapHeight - 1) * _Game.mapWidth] = tT_mountain;
      pCollidibleMask[x] = true;
      pCollidibleMask[x + (_Game.mapHeight - 1) * _Game.mapWidth] = true;
    }
  }
}

//////////////////////////////////////////////////////////////////////////

lsResult game_init()
{
  if (_IsLocal)
    return game_init_local();
  else
    return lsR_Success;
}

lsResult game_tick()
{
  if (_IsLocal)
    return game_tick_local();
  else
    return lsR_Success;
}

lsResult game_observe(_Out_ game *pGame)
{
  if (_IsLocal)
    return game_observe_local(pGame);
  else
    return game_observe_client(pGame);
}

lsResult game_clone(const game *pOrigin, game *pDst)
{
  lsResult result = lsR_Success;

  pDst->pMap = pOrigin->pMap;
  pDst->mapHeight = pOrigin->mapHeight;
  pDst->mapWidth = pOrigin->mapWidth;
  pDst->pPathFindMap = pOrigin->pPathFindMap;
//  
//
//  // Copy over newer events.
//  {
//    for (size_t i = 0; i < pSrc->events.count; i++)
//    {
//      gameEvent *pEvnt = nullptr;
//      LS_ERROR_CHECK(queue_getPtr(&pSrc->events, i, &pEvnt));
//
//      if (pEvnt->index > pDst->eventUpdateCutoffIndex)
//        LS_ERROR_CHECK(queue_pushBack(&pDst->events, pEvnt));
//    }
//  }
//
//  pDst->movementFriction = pSrc->movementFriction;
//  pDst->turnFriction = pSrc->turnFriction;
//  pDst->tickRate = pSrc->tickRate;
//
//epilogue:
  return result;
}

void game_predict(game *pGame)
{
  (void)pGame;

  //const int64_t now = lsGetCurrentTimeNs();
  //const float_t predictingTicks = (float_t)(now - pGame->lastPredictTimeNs) / (1e+9f / (float_t)pGame->tickRate);
  //pGame->lastPredictTimeNs = now;

  //for (auto o : pGame->gameObjects)
  //{
  //  o.pItem->position += o.pItem->velocity * predictingTicks;
  //  o.pItem->rotation += o.pItem->angularVelocity * predictingTicks;
  //
  //  //o.pItem->velocity *= powf(_Game.movementFriction, predictingTicks);
  //  o.pItem->angularVelocity *= powf(_Game.turnFriction, predictingTicks);
  //}
}

void game_set_local(const bool isLocal)
{
  _IsLocal = isLocal;
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
lsResult pool_serialize(_Out_ dataBlob *pBlob, pool<T> *pPool)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pPool == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, pPool->count));

  for (auto item : *pPool)
  {
    LS_ERROR_CHECK(dataBlob_appendValue(pBlob, item.index));
    LS_ERROR_CHECK(dataBlob_append(pBlob, item.pItem));
  }

epilogue:
  return result;
}

template <typename T>
lsResult pool_deserialize(dataBlob *pBlob, _Out_ pool<T> *pPool)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pPool == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  pool_clear(pPool);

  size_t count;
  LS_ERROR_CHECK(dataBlob_read(pBlob, &count));

  for (size_t i = 0; i < count; i++)
  {
    uint64_t index;
    LS_ERROR_CHECK(dataBlob_read(pBlob, &index));

    T item;
    LS_ERROR_CHECK(dataBlob_read(pBlob, &item));

    LS_ERROR_CHECK(pool_insertAt(pPool, item, index));
  }

epilogue:
  return result;
}

template <typename T>
lsResult queue_serialize(_Out_ dataBlob *pBlob, queue<T> *pQueue)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  queue_clear(pQueue);

  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, pQueue->count));

  for (size_t i = 0; i < pQueue->count; i++)
    LS_ERROR_CHECK(dataBlob_appendValue(pBlob, queue_get(pQueue, i)));

epilogue:
  return result;
}

template <typename T>
lsResult queue_deserialize(_Out_ dataBlob *pBlob, queue<T> *pQueue)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pBlob == nullptr, lsR_ArgumentNull);

  size_t count;
  LS_ERROR_CHECK(dataBlob_read(pBlob, &count));

  for (size_t i = 0; i < count; i++)
  {
    T item;
    LS_ERROR_CHECK(dataBlob_read(pBlob, &item));

    LS_ERROR_CHECK(queue_pushBack(pQueue, &item));
  }

epilogue:
  return result;
}

lsResult game_serialize(_Out_ dataBlob *pBlob)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pBlob == nullptr, lsR_ArgumentNull);

  dataBlob_reset(pBlob);

  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, (uint64_t)0)); // size.
  
  // Append dataBlobs / Serialize pools.

  *reinterpret_cast<uint64_t *>(pBlob->pData) = pBlob->size; // replace size.

epilogue:
  return result;
}

lsResult game_deserialze(game *pGame, const void *pData, const size_t size)
{
  lsResult result = lsR_Success;

  dataBlob blob;

  LS_ERROR_IF(pGame == nullptr || pData == nullptr || size == 0, lsR_ArgumentNull);

  dataBlob_createFromForeign(&blob, pData, size);

  size_t actualSize;
  LS_ERROR_CHECK(dataBlob_read(&blob, &actualSize));

  LS_ERROR_IF(actualSize > size, lsR_ResourceInvalid);
  blob.size = actualSize;

  // Read dataBlobs / Deserialize pools.

epilogue:
  return result;
}

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local()
{
  lsResult result = lsR_Success;

  initializeFloodfill();

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
    
    goto epilogue;
  epilogue:
    return result;
  }
}

lsResult game_observe_local(_Out_ game *pGame)
{
  return game_clone(&_Game, pGame);
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
