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
  (void)pOrigin;
  (void)pDst;

  lsResult result = lsR_Success;
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
