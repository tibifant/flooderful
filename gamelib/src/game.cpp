#include "game.h"
#include "gameClient.h"

#include "box2d/box2d.h"

//////////////////////////////////////////////////////////////////////////

static game _Game;
static bool _HasAuthority = true;
static bool _IsLocal = true;
static size_t _AsteroidCount = 0;
static size_t _AsteroidTargetCount = 200;
static size_t _LevelSize = 175;
static queue<size_t> _IndicesToRemove;
static b2World *_pBox2DWorld = nullptr;
static pool<b2Body *> _Box2DObjects;

constexpr bool SwitchPositionsEnabled = true;
constexpr bool UseBox2DPhysics = true;

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local();
lsResult game_tick_local();
lsResult game_observe_local(_Out_ game *pGame);

void game_spaceship_thruster_set_state_local(const size_t playerIndex, const size_t thrusterIndex, const bool enabled);
void game_spaceship_fire_local(const size_t playerIndex);
void game_projectile_thruster_set_state_local(const size_t playerIndex, const size_t thrusterIndex, const bool enabled);
void game_projectile_trigger_local(const size_t playerIndex);
void game_switch_local(const size_t playerIndex);
void game_spaceship_laser_set_state_local(const size_t playerIndex, const bool laserOn);
size_t game_addPlayer_local();

//////////////////////////////////////////////////////////////////////////

size_t game_addEntity_internal(entity *pEntity, gameObject *pGameObject);
void game_tryAddAsteroid_internal();
void game_removeEntity_internal(const size_t entityIndex);
void game_addComponentToEntity_internal(const size_t entityIndex, component_type component, const size_t index);
void game_addEvent_internal(gameEvent *pEvent);
void game_handleLaser_internal(const size_t entityIndex, actor *pActor, gameObject *pObject, playerData *pPlayerData, const float_t simFactor);

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
  lsResult result = lsR_Success;

  game *pSrc = const_cast<game *>(pOrigin);

  pDst->gameStartTimeNs = pSrc->gameStartTimeNs;
  pDst->lastUpdateTimeNs = pDst->lastPredictTimeNs = pSrc->lastUpdateTimeNs;

  pool_clear(&pDst->entities);

  for (auto e : pSrc->entities)
    LS_ERROR_CHECK(pool_insertAt(&pDst->entities, e.pItem, e.index, true));

  lsAssert(pSrc->entities.count == pDst->entities.count);

  pool_clear(&pDst->gameObjects);

  for (auto o : pSrc->gameObjects)
    LS_ERROR_CHECK(pool_insertAt(&pDst->gameObjects, o.pItem, o.index, true));

  lsAssert(pSrc->gameObjects.count == pDst->gameObjects.count);

  pool_clear(&pDst->players);

  for (auto p : pSrc->players)
    LS_ERROR_CHECK(pool_insertAt(&pDst->players, p.pItem, p.index, true));

  lsAssert(pSrc->players.count == pDst->players.count);

  pool_clear(&pDst->actors);

  for (auto a : pSrc->actors)
    LS_ERROR_CHECK(pool_insertAt(&pDst->actors, a.pItem, a.index, true));

  lsAssert(pSrc->actors.count == pDst->actors.count);

  // Copy over newer events.
  {
    for (size_t i = 0; i < pSrc->events.count; i++)
    {
      gameEvent *pEvnt = nullptr;
      LS_ERROR_CHECK(queue_getPtr(&pSrc->events, i, &pEvnt));

      if (pEvnt->index > pDst->eventUpdateCutoffIndex)
        LS_ERROR_CHECK(queue_pushBack(&pDst->events, pEvnt));
    }
  }

  pDst->movementFriction = pSrc->movementFriction;
  pDst->turnFriction = pSrc->turnFriction;
  pDst->tickRate = pSrc->tickRate;

epilogue:
  return result;
}

void game_predict(game *pGame)
{
  const int64_t now = lsGetCurrentTimeNs();
  const float_t predictingTicks = (float_t)(now - pGame->lastPredictTimeNs) / (1e+9f / (float_t)pGame->tickRate);
  pGame->lastPredictTimeNs = now;

  for (auto o : pGame->gameObjects)
  {
    o.pItem->position += o.pItem->velocity * predictingTicks;
    o.pItem->rotation += o.pItem->angularVelocity * predictingTicks;

    //o.pItem->velocity *= powf(_Game.movementFriction, predictingTicks);
    o.pItem->angularVelocity *= powf(_Game.turnFriction, predictingTicks);
  }
}

void game_set_authority(const bool hasAuthority)
{
  _HasAuthority = hasAuthority;
}

void game_set_local(const bool isLocal)
{
  _IsLocal = isLocal;
}

void game_spaceship_thruster_set_state(const size_t playerIndex, const size_t thrusterIndex, const bool enabled)
{
  if (_IsLocal)
    return game_spaceship_thruster_set_state_local(playerIndex, thrusterIndex, enabled);
  else
    return game_spaceship_thruster_set_state_client(playerIndex, thrusterIndex, enabled);
}

void game_spaceship_fire(const size_t playerIndex)
{
  if (_IsLocal)
    return game_spaceship_fire_local(playerIndex);
  else
    return game_spaceship_fire_client(playerIndex);
}

void game_projectile_thruster_set_state(const size_t playerIndex, const size_t thrusterIndex, const bool enabled)
{
  if (_IsLocal)
    return game_projectile_thruster_set_state_local(playerIndex, thrusterIndex, enabled);
  else
    return game_projectile_thruster_set_state_client(playerIndex, thrusterIndex, enabled);
}

void game_projectile_trigger(const size_t playerIndex)
{
  if (_IsLocal)
    return game_projectile_trigger_local(playerIndex);
  else
    return game_projectile_trigger_client(playerIndex);
}

void game_switch(const size_t playerIndex)
{
  if (_IsLocal)
    return game_switch_local(playerIndex);
  else
    return game_switch_client(playerIndex);
}

void game_spaceship_laser_set_state(const size_t playerIndex, const bool laserOn)
{
  if (_IsLocal)
    return game_spaceship_laser_set_state_local(playerIndex, laserOn);
  else
    return game_spaceship_laser_set_state_client(playerIndex, laserOn);
}

size_t game_addPlayer()
{
  if (_IsLocal)
    return game_addPlayer_local();
  else
    return game_getPlayerIndex_client();
}

//////////////////////////////////////////////////////////////////////////

size_t game_getFirstComponentIndex(game *pGame, const size_t entityIndex, const component_type compoenent)
{
  entity *pEntity = pool_get(&pGame->entities, entityIndex);

  for (size_t i = 0; i < LS_ARRAYSIZE(pEntity->components); i++)
    if (pEntity->components[i] == compoenent)
      return pEntity->ids[i];

  lsAssert(false);

  return (size_t)-1;
}

actor *game_getActiveActorForPlayer(game *pGame, const size_t playerIndex, _Out_ size_t *pEntityIndex /* = nullptr */)
{
  const size_t playerEntityIndex = pool_get(&pGame->players, playerIndex)->entityIndex;
  
  actor *pActor = pool_get(&pGame->actors, game_getFirstComponentIndex(pGame, playerEntityIndex, ct_actor));
  size_t lastEntityIndex = playerEntityIndex;

  while (!pActor->hasFocus && pActor->partnerEntityIndex != playerEntityIndex) // in case we have some strange linked list of projectiles.
  {
    lsAssert(pActor->partnerEntityIndex);
    lastEntityIndex = pActor->partnerEntityIndex;
    pActor = pool_get(&pGame->actors, game_getFirstComponentIndex(pGame, lastEntityIndex, ct_actor));
  }

  lsAssert(pActor->hasFocus); // otherwise nobody has focus.

  if (pEntityIndex != nullptr)
    *pEntityIndex = lastEntityIndex;

  return pActor;
}

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

  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.lastUpdateTimeNs));
  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.gameStartTimeNs));
  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.lastPredictTimeNs));

  LS_ERROR_CHECK(pool_serialize(pBlob, &_Game.entities));
  LS_ERROR_CHECK(pool_serialize(pBlob, &_Game.gameObjects));
  LS_ERROR_CHECK(pool_serialize(pBlob, &_Game.players));
  LS_ERROR_CHECK(pool_serialize(pBlob, &_Game.actors));
  LS_ERROR_CHECK(queue_serialize(pBlob, &_Game.events));

  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.movementFriction));
  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.turnFriction));
  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.tickRate));
  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.lastEventIndex));
  LS_ERROR_CHECK(dataBlob_appendValue(pBlob, _Game.eventUpdateCutoffIndex));

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

  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->lastUpdateTimeNs));
  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->gameStartTimeNs));
  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->lastPredictTimeNs));

  LS_ERROR_CHECK(pool_deserialize(&blob, &pGame->entities));
  LS_ERROR_CHECK(pool_deserialize(&blob, &pGame->gameObjects));
  LS_ERROR_CHECK(pool_deserialize(&blob, &pGame->players));
  LS_ERROR_CHECK(pool_deserialize(&blob, &pGame->actors));
  LS_ERROR_CHECK(queue_deserialize(&blob, &pGame->events));

  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->movementFriction));
  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->turnFriction));
  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->tickRate));
  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->lastEventIndex));
  LS_ERROR_CHECK(dataBlob_read(&blob, &pGame->eventUpdateCutoffIndex));

epilogue:
  return result;
}

//////////////////////////////////////////////////////////////////////////

lsResult game_init_local()
{
  lsResult result = lsR_Success;

  entity nullEntity;
  lsZeroMemory(&nullEntity);

  gameObject nullGameObject;
  lsZeroMemory(&nullGameObject);
  
  size_t zero;

  LS_ERROR_CHECK(pool_add(&_Game.entities, &nullEntity, &zero));
  lsAssert(zero == 0);

  LS_ERROR_CHECK(pool_add(&_Game.gameObjects, &nullGameObject, &zero));
  lsAssert(zero == 0);

  if constexpr (UseBox2DPhysics)
  {
    _pBox2DWorld = new b2World(b2Vec2_zero);

    pool_add(&_Box2DObjects, (b2Body *)nullptr, &zero);
    lsAssert(zero == 0);
  }

  _Game.gameStartTimeNs = _Game.lastUpdateTimeNs = lsGetCurrentTimeNs();

  for (size_t i = 0; i < _AsteroidTargetCount; i++)
    game_tryAddAsteroid_internal();

  goto epilogue;
epilogue:
  return result;
}

lsResult game_tick_local()
{
  lsResult result = lsR_Success;

  const int64_t lastTick = _Game.lastUpdateTimeNs;
  const int64_t tick = lsGetCurrentTimeNs();
  const float_t simFactor = (float_t)(tick - lastTick) / (1e+9f / (float_t)_Game.tickRate);
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
  
  // process player interactions.
  {
    for (auto p : _Game.players)
    {
      playerData *pPlayer = p.pItem;
      const size_t playerEntityIndex = pPlayer->entityIndex;
      actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, playerEntityIndex, ct_actor));
      gameObject *pObject = pool_get(&_Game.gameObjects, playerEntityIndex);

      {
        const float_t accelTurnPenalty = (1.f - lsClamp(pObject->velocity.Length() * 2.f, 0.f, .9f));

        if (pActor->thrusterTurnLeft)
        {
          game_phys_addAngularForce(playerEntityIndex, pObject, pPlayer->rotationSpeed * accelTurnPenalty * simFactor);
          game_phys_addVelocity(playerEntityIndex, pObject, vec2f(cosf(pObject->rotation + pObject->angularVelocity), sinf(pObject->rotation + pObject->angularVelocity)) * pPlayer->turnBonusSpeed * simFactor);
        }

        if (pActor->thrusterTurnRight)
        {
          game_phys_addAngularForce(playerEntityIndex, pObject, -pPlayer->rotationSpeed * accelTurnPenalty * simFactor);
          game_phys_addVelocity(playerEntityIndex, pObject, vec2f(cosf(pObject->rotation + pObject->angularVelocity), sinf(pObject->rotation + pObject->angularVelocity)) * pPlayer->turnBonusSpeed * simFactor);
        }

        if (pActor->thrusterForward)
          game_phys_addVelocity(playerEntityIndex, pObject, vec2f(cosf(pObject->rotation), sinf(pObject->rotation)) * pPlayer->speed * simFactor);

        if (pActor->thrusterBack)
          game_phys_addVelocity(playerEntityIndex, pObject, -vec2f(cosf(pObject->rotation), sinf(pObject->rotation)) * pPlayer->reverseSpeed * simFactor);

        if (pActor->hasLaser && pActor->laserOn)
          game_handleLaser_internal(playerEntityIndex, pActor, pObject, pPlayer, simFactor);
      }

      size_t nextProjectileEntityIndex = pActor->partnerEntityIndex;

      while (nextProjectileEntityIndex)
      {
        pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, nextProjectileEntityIndex, ct_actor));
        pObject = pool_get(&_Game.gameObjects, nextProjectileEntityIndex);
        const float_t accelTurnPenalty = (1.f - lsClamp(pObject->velocity.Length() * 5.f, 0.f, .9f));

        game_phys_addVelocity(nextProjectileEntityIndex, pObject, vec2f(cosf(pObject->rotation), sinf(pObject->rotation)) * pPlayer->projectileAutoSpeed * simFactor);

        if (pActor->thrusterTurnLeft)
        {
          game_phys_addAngularForce(nextProjectileEntityIndex, pObject, pPlayer->projectileRotationSpeed * accelTurnPenalty * simFactor);
          game_phys_addVelocity(nextProjectileEntityIndex, pObject, vec2f(cosf(pObject->rotation + pObject->angularVelocity), sinf(pObject->rotation + pObject->angularVelocity)) * pPlayer->turnBonusSpeed * simFactor);
        }

        if (pActor->thrusterTurnRight)
        {
          game_phys_addAngularForce(nextProjectileEntityIndex, pObject, -pPlayer->projectileRotationSpeed * accelTurnPenalty * simFactor);
          game_phys_addVelocity(nextProjectileEntityIndex, pObject, vec2f(cosf(pObject->rotation + pObject->angularVelocity), sinf(pObject->rotation + pObject->angularVelocity)) * pPlayer->turnBonusSpeed * simFactor);
        }

        if (pActor->thrusterForward)
          game_phys_addVelocity(nextProjectileEntityIndex, pObject, vec2f(cosf(pObject->rotation), sinf(pObject->rotation)) * pPlayer->projectileSpeed * simFactor);

        if (pActor->thrusterBack)
          game_phys_addVelocity(nextProjectileEntityIndex, pObject, vec2f(cosf(pObject->rotation), sinf(pObject->rotation)) * pPlayer->projectileReverseSpeed * simFactor);

        if (pActor->hasLaser && pActor->laserOn)
          game_handleLaser_internal(nextProjectileEntityIndex, pActor, pObject, pPlayer, simFactor);

        if (pActor->partnerEntityIndex == playerEntityIndex)
          break;
      }
    }
  }

  // physics tick.
  {
    struct _internal
    {
      static void anihilateProjectile(const size_t index, gameObject *pGameObject)
      {
        lsAssert(pGameObject->type == goT_projectile);

        playerData *pParentPlayer = pool_get(&_Game.players, pGameObject->associatedData.playerIndex);
        actor *pParentPlayerActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pParentPlayer->entityIndex, ct_actor));

        pParentPlayerActor->hasFocus = true;
        pParentPlayerActor->partnerEntityIndex = 0;

        gameEvent evnt;
        lsZeroMemory(&evnt);
        evnt.position = pGameObject->position;
        evnt.type = geT_projectileExplosion;

        game_addEvent_internal(&evnt);

        queue_pushBack(&_IndicesToRemove, index);
      }

      static void scorePoint(gameObject *pPlayer, gameObject *pProjectile, const size_t projectileIndex)
      {
        lsAssert(pPlayer->type == goT_spaceship);
        lsAssert(pProjectile->type == goT_projectile);

        if (pPlayer->associatedData.playerIndex == pProjectile->associatedData.playerIndex)
          return;

        playerData *pScoringPlayer = pool_get(&_Game.players, pProjectile->associatedData.playerIndex);
        pScoringPlayer->score++;

        printf("Player %" PRIu64 " now has %" PRIu64 " points.\n", pPlayer->associatedData.playerIndex, pScoringPlayer->score);

        anihilateProjectile(projectileIndex, pProjectile);
      }

      static void onCollision(const size_t entityIndexA, const size_t entityIndexB, gameObject *pA, gameObject *pB)
      {
        if (pA->type == goT_projectile && pB->type == goT_spaceship)
        {
          scorePoint(pB, pA, entityIndexA);
        }
        else if (pA->type == goT_spaceship && pB->type == goT_projectile)
        {
          scorePoint(pA, pB, entityIndexB);
        }
        else if (pA->type == goT_projectile && pB->type == goT_projectile)
        {
          anihilateProjectile(entityIndexA, pA);
          anihilateProjectile(entityIndexB, pB);
        }
      }
    };

    if constexpr (UseBox2DPhysics)
    {
      static class : public b2ContactListener
      {
        void BeginContact(b2Contact *pContact) override
        {
          const size_t entityIndexA = pContact->GetFixtureA()->GetBody()->GetUserData().pointer;
          const size_t entityIndexB = pContact->GetFixtureB()->GetBody()->GetUserData().pointer;

          if (entityIndexA == 0 || entityIndexB == 0)
            return;

          _internal::onCollision(entityIndexA, entityIndexB, pool_get(&_Game.gameObjects, entityIndexA), pool_get(&_Game.gameObjects, entityIndexB));
        }
      } listener;

      _pBox2DWorld->SetContactListener(&listener);
      _pBox2DWorld->Step(simFactor, 8, 3);

      for (const auto _item : _Box2DObjects)
      {
        b2Body *pBody = (*_item.pItem);

        if (pBody == nullptr)
          continue;

        gameObject *pObj = pool_get(&_Game.gameObjects, _item.index);

        pObj->position = vec2f(pBody->GetPosition().x, pBody->GetPosition().y);
        pObj->velocity = vec2f(pBody->GetLinearVelocity().x, pBody->GetLinearVelocity().y);
        pObj->rotation = pBody->GetAngle();
        pObj->angularVelocity = pBody->GetAngularVelocity();
      }
    }
    else
    {
      for (auto o : _Game.gameObjects)
      {
        if (o.pItem->position.x < 0)
          o.pItem->velocity.x = o.pItem->velocity.x * 0.9f + 0.01f;

        if (o.pItem->position.y < 0)
          o.pItem->velocity.y = o.pItem->velocity.y * 0.9f + 0.01f;

        if (o.pItem->position.x > (_LevelSize + 1.f))
          o.pItem->velocity.x = o.pItem->velocity.x * 0.9f - 0.01f;

        if (o.pItem->position.y > (_LevelSize + 1.f))
          o.pItem->velocity.y = o.pItem->velocity.y * 0.9f - 0.01f;

        if (lsAbs(o.pItem->velocity.x) > 1 || lsAbs(o.pItem->velocity.y) > 1)
          o.pItem->velocity.Normalize();

        if (o.pItem->position.x < -20.f || o.pItem->position.y < -20.f || o.pItem->position.x >(_LevelSize + 20.f) || o.pItem->position.y >(_LevelSize + 20.f))
        {
          o.pItem->position = vec2f(lsClamp(o.pItem->position.x, 0.f, (float_t)_LevelSize), lsClamp(o.pItem->position.y, 0.f, (float_t)_LevelSize));
          o.pItem->velocity = vec2f(0);

          gameEvent _event;
          _event.type = geT_teleportation;
          _event.position = o.pItem->position;

          game_addEvent_internal(&_event);
        }

        o.pItem->position += o.pItem->velocity * simFactor;
        o.pItem->velocity *= powf(_Game.movementFriction, simFactor);

        o.pItem->rotation += o.pItem->angularVelocity * simFactor;
        o.pItem->angularVelocity *= powf(_Game.turnFriction, simFactor);
      }

      // Collision handling.
      {
        for (auto a : _Game.gameObjects)
        {
          if (a.pItem->shape == goS_notCollidable)
            continue;

          lsAssert(a.pItem->shape == goS_sphere);

          auto iteratorContainer = _Game.gameObjects.IterateFromIteratedIndex(a.index, a._iteratedIndex);
          auto it = iteratorContainer.begin();
          ++it; // skip the first one, since this is the one we already have.

          for (; it != iteratorContainer.end(); ++it)
          {
            auto b = *it;

            if (b.pItem->shape == goS_notCollidable)
              continue;

            lsAssert(b.pItem->shape == goS_sphere);

            const float_t combinedRadii = a.pItem->shapeData.sphereRadius + b.pItem->shapeData.sphereRadius;
            const float_t combinedRadiiSquared = combinedRadii * combinedRadii;

            const vec2f distAB = a.pItem->position + a.pItem->velocity - b.pItem->position + b.pItem->velocity;
            const float_t distABSquared = distAB.LengthSquared();

            // do we collide?
            if (distABSquared < combinedRadiiSquared)
            {
              const vec2f n = -distAB * lsMin(1.f, _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(distABSquared + 0.001f))));
              const float_t p = 2 * (a.pItem->velocity.x * n.x + b.pItem->velocity.y * n.y - b.pItem->velocity.x * n.x - a.pItem->velocity.y * n.y) / (a.pItem->shapeData.sphereRadiusSquared + b.pItem->shapeData.sphereRadiusSquared);

              a.pItem->velocity -= p * a.pItem->shapeData.sphereRadiusSquared * n;
              b.pItem->velocity += p * b.pItem->shapeData.sphereRadiusSquared * n;

              _internal::onCollision(a.index, b.index, a.pItem, b.pItem);

              break;
            }
          }
        }
      }
    }

    // Remove stuff.
    {
      while (queue_any(&_IndicesToRemove))
      {
        size_t index;
        LS_ERROR_CHECK(queue_popFront(&_IndicesToRemove, &index));

        game_removeEntity_internal(index); // don't panic if this this somehow fails because the object doesn't exist any more. may happen in some cases. gone is gone.
      }
    }
  }

  if (_HasAuthority)
  {
    game_tryAddAsteroid_internal();
  }

  goto epilogue;
epilogue:
  return result;
}

lsResult game_observe_local(_Out_ game *pGame)
{
  return game_clone(&_Game, pGame);
}

void game_spaceship_thruster_set_state_local(const size_t playerIndex, const size_t thrusterIndex, const bool enabled)
{
  playerData *pPlayerData = pool_get(&_Game.players, playerIndex);
  actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pPlayerData->entityIndex, ct_actor));

  if (!pActor->hasFocus)
  {
    printf("Rejected to change player %" PRIu64 " thruster, because the spaceship doesn't have focus.\n", playerIndex);
    return;
  }

  switch (thrusterIndex)
  {
  case 0:
    pActor->thrusterForward = enabled;
    break;

  case 1:
    pActor->thrusterBack = enabled;
    break;

  case 2:
    pActor->thrusterTurnRight = enabled;
    break;

  case 3:
    pActor->thrusterTurnLeft = enabled;
    break;

  default:
    lsAssert(false);
    break;
  }
}

void game_spaceship_fire_local(const size_t playerIndex)
{
  playerData *pPlayerData = pool_get(&_Game.players, playerIndex);
  actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pPlayerData->entityIndex, ct_actor));

  if (!pActor->hasFocus)
  {
    printf("Rejected to fire player %" PRIu64 " spaceship, because the spaceship doesn't have focus.\n", playerIndex);
    return;
  }

  gameObject *pObject = pool_get(&_Game.gameObjects, pPlayerData->entityIndex);
  gameObject *pProjectileObject = nullptr;

  if (pActor->partnerEntityIndex != 0)
  {
    pProjectileObject = pool_get(&_Game.gameObjects, pActor->partnerEntityIndex);

    if constexpr (!SwitchPositionsEnabled)
    {
      actor *pProjectileActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pActor->partnerEntityIndex, ct_actor));
      pProjectileActor->hasFocus = true;
    }

    gameEvent _event;
    _event.type = geT_projectileExplosion;
    _event.position = pProjectileObject->position;

    game_addEvent_internal(&_event);
  }
  else
  {
    gameObject projectileObject;
    lsZeroMemory(&projectileObject);
    projectileObject.type = goT_projectile;
    projectileObject.shape = goS_sphere;
    projectileObject.shapeData.sphereRadius = 0.6f;
    projectileObject.shapeData.sphereRadiusSquared = projectileObject.shapeData.sphereRadius * projectileObject.shapeData.sphereRadius;
    projectileObject.associatedData.playerIndex = playerIndex;

    actor actor;
    lsZeroMemory(&actor);
    actor.partnerEntityIndex = pPlayerData->entityIndex;

    if constexpr (!SwitchPositionsEnabled)
      actor.hasFocus = true;

    entity entity;
    lsZeroMemory(&entity);
    entity.components[1] = ct_actor;
    pool_add(&_Game.actors, &actor, &entity.ids[1]);

    pActor->partnerEntityIndex = game_addEntity_internal(&entity, &projectileObject);

    pProjectileObject = pool_get(&_Game.gameObjects, pActor->partnerEntityIndex);
  }

  if constexpr (!SwitchPositionsEnabled)
    pActor->hasFocus = false;

  game_phys_setTransformAndImpulse(pActor->partnerEntityIndex, pProjectileObject, pObject->position, pObject->rotation, pObject->velocity + vec2f(cosf(pObject->rotation), sinf(pObject->rotation)) * (pPlayerData->projectileSpeed + pPlayerData->projectileAutoSpeed) * 50.f, pObject->angularVelocity);
}

void game_projectile_thruster_set_state_local(const size_t playerIndex, const size_t thrusterIndex, const bool enabled)
{
  playerData *pPlayerData = pool_get(&_Game.players, playerIndex);
  actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pPlayerData->entityIndex, ct_actor));

  if (pActor->hasFocus)
  {
    printf("Rejected to change player %" PRIu64 " projectile thruster, because the spaceship has focus.\n", playerIndex);
    return;
  }

  if (pActor->partnerEntityIndex == 0)
  {
    printf("Rejected to change player %" PRIu64 " projectile thruster, because they have no projectiles.\n", playerIndex);
    return;
  }

  pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pActor->partnerEntityIndex, ct_actor));

  if (!pActor->hasFocus)
  {
    printf("Rejected to change player %" PRIu64 " projectile thruster, because the projectile doesn't have focus.\n", playerIndex);
    return;
  }

  switch (thrusterIndex)
  {
  case 0:
    pActor->thrusterForward = enabled;
    break;

  case 1:
    pActor->thrusterBack = enabled;
    break;

  case 2:
    pActor->thrusterTurnRight = enabled;
    break;

  case 3:
    pActor->thrusterTurnLeft = enabled;
    break;

  default:
    lsAssert(false);
    break;
  }
}

void game_projectile_trigger_local(const size_t playerIndex)
{
  playerData *pPlayerData = pool_get(&_Game.players, playerIndex);
  actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pPlayerData->entityIndex, ct_actor));

  if (pActor->hasFocus)
  {
    printf("Rejected to trigger player %" PRIu64 " projectile, because the spaceship has focus.\n", playerIndex);
    return;
  }

  if (pActor->partnerEntityIndex == 0)
  {
    printf("Rejected to trigger player %" PRIu64 " projectile, because they have no projectiles.\n", playerIndex);
    return;
  }

  actor *pProjectileActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pActor->partnerEntityIndex, ct_actor));

  if (!pProjectileActor->hasFocus)
  {
    printf("Rejected to trigger player %" PRIu64 " projectile, because the projectile doesn't have focus.\n", playerIndex);
    return;
  }

  gameObject *pProjectileObject = pool_get(&_Game.gameObjects, pActor->partnerEntityIndex);

  // Add Event.
  {
    gameEvent _event;
    _event.type = geT_projectileExplosion;
    _event.position = pProjectileObject->position;

    game_addEvent_internal(&_event);
  }

  for (auto player : _Game.players)
  {
    const size_t otherPlayerIndex = player.index;
    const size_t otherPlayerEntityIndex = player.pItem->entityIndex;
    gameObject *pOtherPlayerObject = pool_get(&_Game.gameObjects, otherPlayerEntityIndex);

    const float_t distSq = (pProjectileObject->position - pOtherPlayerObject->position).LengthSquared();
    const bool close = distSq < pPlayerData->projectileKillRadiusSquared;

    if (close)
      game_phys_addVelocity(otherPlayerEntityIndex, pOtherPlayerObject, (pOtherPlayerObject->position - pProjectileObject->position) / lsMax(distSq, 0.3f) * 0.66f);

    if (otherPlayerIndex == playerIndex)
      continue;

    if (close)
      pPlayerData->score++;

    actor *pOtherPlayerActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, otherPlayerEntityIndex, ct_actor));

    if (pOtherPlayerActor->partnerEntityIndex == 0)
      continue;

    gameObject *pOtherPlayerProjectileObject = pool_get(&_Game.gameObjects, pOtherPlayerActor->partnerEntityIndex);

    if ((pProjectileObject->position - pOtherPlayerProjectileObject->position).LengthSquared() < pPlayerData->projectileKillRadiusSquared)
    {
      pPlayerData->score++;

      // Add event for other projectile.
      {
        gameEvent _event;
        _event.type = geT_projectileExplosion;
        _event.position = pOtherPlayerProjectileObject->position;

        game_addEvent_internal(&_event);
      }

      game_removeEntity_internal(pOtherPlayerActor->partnerEntityIndex);

      pOtherPlayerActor->hasFocus = true;
      pOtherPlayerActor->partnerEntityIndex = 0;
    }
  }

  game_removeEntity_internal(pActor->partnerEntityIndex);
  pActor->partnerEntityIndex = 0;
  pActor->hasFocus = true;
}

void game_switch_local(const size_t playerIndex)
{
  if constexpr (!SwitchPositionsEnabled)
  {
    playerData *pPlayerData = pool_get(&_Game.players, playerIndex);
    actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pPlayerData->entityIndex, ct_actor));
  
    if (pActor->partnerEntityIndex == 0)
    {
      printf("Rejected to switch player %" PRIu64 ", because they have no projectiles.\n", playerIndex);
      return;
    }
  
    actor *pProjectileActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pActor->partnerEntityIndex, ct_actor));
  
    pActor->hasFocus = !pActor->hasFocus;
    pProjectileActor->hasFocus = !pActor->hasFocus;
  }
  else
  {
    playerData *pPlayerData = pool_get(&_Game.players, playerIndex);
    actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pPlayerData->entityIndex, ct_actor));

    if (pActor->partnerEntityIndex == 0)
    {
      printf("Rejected to switch player %" PRIu64 ", because they have no projectiles.\n", playerIndex);
      return;
    }

    gameObject *pSpaceship = pool_get(&_Game.gameObjects, pPlayerData->entityIndex);
    gameObject *pProjectile = pool_get(&_Game.gameObjects, pActor->partnerEntityIndex);

    const gameObject tmp = *pSpaceship;
    game_phys_setTransformAndImpulse(pPlayerData->entityIndex, pSpaceship, pProjectile->position, pProjectile->rotation, pProjectile->velocity, pProjectile->angularVelocity);
    game_phys_setTransformAndImpulse(pActor->partnerEntityIndex, pProjectile, tmp.position, tmp.rotation, tmp.velocity, tmp.angularVelocity);

    gameEvent _event;
    _event.type = geT_teleportation;
    _event.position = pSpaceship->position;
    game_addEvent_internal(&_event);

    _event.position = pProjectile->position;
    game_addEvent_internal(&_event);
  }
}

void game_spaceship_laser_set_state_local(const size_t playerIndex, const bool laserOn)
{
  playerData *pPlayerData = pool_get(&_Game.players, playerIndex);
  actor *pActor = pool_get(&_Game.actors, game_getFirstComponentIndex(&_Game, pPlayerData->entityIndex, ct_actor));

  if (!pActor->hasFocus)
  {
    printf("Rejected to turn player %" PRIu64 " laser on/off, because the spaceship doesn't have focus.\n", playerIndex);
    return;
  }

  if (!pActor->hasLaser)
  {
    printf("Rejected to turn player %" PRIu64 " laser on/off, because the actor doesn't have a laser.\n", playerIndex);
    return;
  }

  pActor->laserOn = laserOn;
}

size_t game_addPlayer_local()
{
  entity entity;

  gameObject obj;
  lsZeroMemory(&obj);
  obj.type = goT_spaceship;
  obj.position = vec2f((float_t)(lsGetRand() % _LevelSize), (float_t)(lsGetRand() % _LevelSize)); // TODO: Find a better starting position, or nuke the asteroids there.
  obj.shape = goS_sphere;
  obj.shapeData.sphereRadius = 0.55f;
  obj.shapeData.sphereRadiusSquared = obj.shapeData.sphereRadius * obj.shapeData.sphereRadius;

  actor actor;
  lsZeroMemory(&actor);
  actor.isPlayer = true;
  actor.hasFocus = true;
  actor.hasLaser = true;

  pool_add(&_Game.actors, &actor, &entity.ids[1]);
  entity.components[1] = ct_actor;

  playerData playerData;
  lsZeroMemory(&playerData);
  playerData.entityIndex = game_addEntity_internal(&entity, &obj);
  playerData.speed = 0.008f;
  playerData.rotationSpeed = 0.0035f;
  playerData.reverseSpeed = 0.0125f;
  playerData.projectileSpeed = 0.005f;
  playerData.projectileRotationSpeed = 0.0035f;
  playerData.projectileReverseSpeed = 0.001f;
  playerData.turnBonusSpeed = 0.005f;
  playerData.projectileTurnBonusSpeed = 0.0075f;
  playerData.projectileReverseSpeed = 0;
  playerData.projectileAutoSpeed = 0.0075f;
  playerData.projectileKillRadiusSquared = 2.2f * 2.2f;
  playerData.score = 0;
  playerData.maxLaserLength = 5.f;
  playerData.laserStrength = 1.f;

  size_t playerIndex;
  pool_add(&_Game.players, &playerData, &playerIndex);

  game_addComponentToEntity_internal(playerData.entityIndex, ct_playerData, playerIndex);

  gameObject *pGameObject = pool_get(&_Game.gameObjects, playerData.entityIndex);
  pGameObject->associatedData.playerIndex = playerIndex;
  
  return playerIndex;
}

//////////////////////////////////////////////////////////////////////////

size_t game_addEntity_internal(entity *pEntity, gameObject *pGameObject)
{
  size_t i = 0;

  for (; i < LS_ARRAYSIZE(pEntity->components); i++)
    if (pEntity->components[i] == ct_none || pEntity->components[i] == ct_gameObject)
      break;

  lsAssert(i < LS_ARRAYSIZE(pEntity->components));

  pEntity->components[i] = ct_gameObject;
  pool_add(&_Game.gameObjects, pGameObject, &pEntity->ids[i]);

  size_t entityIndex;
  pool_add(&_Game.entities, pEntity, &entityIndex);
  lsAssert(entityIndex == pEntity->ids[i]);

  if constexpr (UseBox2DPhysics)
  {
    size_t physicsIndex;

    b2Body *pBody = nullptr;

    switch (pGameObject->shape)
    {
    case goS_sphere:
    {
      b2CircleShape shape;
      shape.m_radius = pGameObject->shapeData.sphereRadius;

      b2BodyDef bd;
      bd.type = b2_dynamicBody;

      pBody = _pBox2DWorld->CreateBody(&bd);
      pBody->CreateFixture(&shape, 0.5f);

      break;
    }

    case goS_notCollidable:
      break;

    default:
      lsAssert(false);
    }

    if (pBody != nullptr)
    {
      pBody->GetUserData().pointer = entityIndex;

      pBody->SetTransform(b2Vec2(pGameObject->position.x, pGameObject->position.y), pGameObject->rotation);
      pBody->ApplyLinearImpulseToCenter(b2Vec2(pGameObject->velocity.x, pGameObject->velocity.y), true);
      pBody->ApplyAngularImpulse(pGameObject->angularVelocity, true);

      const float_t linDamp = powf(_Game.movementFriction, (float_t)game_getTickRate());
      const float_t angDamp = 1.f - powf(_Game.turnFriction, (float_t)game_getTickRate());

      switch (pGameObject->type)
      {
      case goT_asteroid:
      {
        pBody->SetLinearDamping(0.05f);
        pBody->SetAngularDamping(0.2f);

        b2MassData massData;
        pBody->GetMassData(&massData);
        massData.mass *= 0.25f;
        pBody->SetMassData(&massData);

        break;
      }

      default:
      {
        pBody->SetLinearDamping(linDamp);
        pBody->SetAngularDamping(angDamp);

        break;
      }
      }
    }

    pool_add(&_Box2DObjects, pBody, &physicsIndex);
    lsAssert(entityIndex == physicsIndex);
  }

  return entityIndex;
}

void game_tryAddAsteroid_internal()
{
  if (_AsteroidCount >= _AsteroidTargetCount)
    return;

  vec2f freePosition;
  size_t tries = 0;
  constexpr size_t MaxTries = 20;

  do
  {
    tries++;
    freePosition = vec2f((float_t)(lsGetRand() % _LevelSize), (float_t)(lsGetRand() % _LevelSize));

    bool found = false;

    for (size_t i = 0; i < _Game.players.count; i++)
    {
      size_t entityIndex;
      game_getActiveActorForPlayer(&_Game, i, &entityIndex);
      gameObject *pObject = pool_get(&_Game.gameObjects, entityIndex);
      
      if ((freePosition - pObject->position).LengthSquared() < (15.f * 15.f))
      {
        found = true;
        break;
      }
    }

    if (!found)
      break;

  } while (tries < MaxTries);

  if (tries >= MaxTries)
    return;

  entity entity;

  gameObject gameObject;
  lsZeroMemory(&gameObject);
  gameObject.type = goT_asteroid;
  gameObject.position = freePosition;
  gameObject.shape = goS_sphere;
  gameObject.shapeData.sphereRadius = 0.9f;
  gameObject.shapeData.sphereRadiusSquared = gameObject.shapeData.sphereRadius * gameObject.shapeData.sphereRadius;
  gameObject.associatedData.asteroidHealth = 80.f + (lsGetRand() % 0x10000) / (float_t)0x10000 * 10.f;
  gameObject.associatedData.asteroidSize = goAS_Large;

  constexpr size_t weightStone = 200;

  constexpr size_t maxWeightCopper = 1000;
  constexpr size_t minWeightCopper = 10;
  constexpr size_t maxDistCopper = 30;
  const vec2f maxDensityCopper = vec2f(50.f);
  const size_t distCopper = (size_t)(gameObject.position - maxDensityCopper).Length();

  const size_t weightCopper = lsClamp((maxDistCopper - lsMin(maxDistCopper, distCopper)) * maxWeightCopper / maxDistCopper, minWeightCopper, maxWeightCopper);

  constexpr size_t maxWeightSalt = 1000;
  constexpr size_t minWeightSalt = 10;
  constexpr size_t maxDistSalt = 40;
  const vec2f maxDensitySalt = vec2f(125.f, 50.f);
  const size_t distSalt = (size_t)(gameObject.position - maxDensitySalt).Length();

  const size_t weightSalt = lsClamp((maxDistSalt - lsMin(maxDistSalt, distSalt)) * maxWeightSalt / maxDistSalt, minWeightSalt, maxWeightSalt);

  static const gameObject_asteroidType types[] = { goAT_stone, goAT_copper, goAT_salt };
  const size_t weights[] = { weightStone, weightCopper, weightSalt };
  size_t rand = lsGetRand() % (weightStone + weightCopper + weightSalt);

  for (size_t i = 0; i < LS_ARRAYSIZE(weights); i++)
  {
    if (rand < weights[i])
    {
      gameObject.associatedData.asteroidType = types[i];
      break;
    }

    rand -= weights[i];
  }

  game_addEntity_internal(&entity, &gameObject);

  _AsteroidCount++;
}

void game_removeEntity_internal(const size_t entityIndex)
{
  entity entity;
  pool_remove_safe(&_Game.entities, entityIndex, &entity);

  for (size_t i = 0; i < LS_ARRAYSIZE(entity.components); i++)
  {
    switch (entity.components[i])
    {
    case ct_gameObject:
    {
      gameObject obj;
      pool_remove_safe(&_Game.gameObjects, entity.ids[i], &obj);

      if constexpr (UseBox2DPhysics)
      {
        b2Body *pBody = nullptr;
        pool_remove_safe(&_Box2DObjects, entity.ids[i], &pBody);
        _pBox2DWorld->DestroyBody(pBody);
      }
      
      break;
    }

    case ct_actor:
    {
      actor actor;
      pool_remove_safe(&_Game.actors, entity.ids[i], &actor);
      break;
    }

    case ct_playerData:
    {
      playerData data;
      pool_remove_safe(&_Game.players, entity.ids[i], &data);
      break;
    }

    case ct_none:
    {
      break;
    }

    default:
    {
      lsAssert(false); // wtf?
      break;
    }
    }
  }
}

void game_addComponentToEntity_internal(const size_t entityIndex, component_type component, const size_t index)
{
  entity *pEntity = pool_get(&_Game.entities, entityIndex);

  for (size_t i = 0; i < LS_ARRAYSIZE(pEntity->components); i++)
  {
    if (pEntity->components[i] == ct_none)
    {
      pEntity->components[i] = component;
      pEntity->ids[i] = index;

      return;
    }
  }

  lsAssert(false);
}

void game_addEvent_internal(gameEvent *pEvent)
{
  pEvent->index = ++_Game.lastEventIndex;
  pEvent->timeNs = lsGetCurrentTimeNs();

  queue_pushBack(&_Game.events, pEvent);
}

void game_handleLaser_internal(const size_t /* entityIndex */, actor *pActor, gameObject *pObject, playerData *pPlayerData, const float_t simFactor)
{
  static_assert(UseBox2DPhysics, "Not Implemented");

  const vec2f endPosition = pObject->position + (vec2f(cosf(pObject->rotation), sinf(pObject->rotation)) * pPlayerData->maxLaserLength);

  static class : public b2RayCastCallback
  {
  public:
    bool anyHit = false;
    vec2f lastHit;
    gameObject *pLastGameObject = nullptr;
    size_t lastEntityIndex;

    float_t ReportFixture(b2Fixture *pFixture, const b2Vec2 &point, const b2Vec2 & /* normal */, const float_t fraction) override
    {
      if (pFixture == nullptr)
        return -1.f;

      b2Body *pBody = pFixture->GetBody();

      if (pBody == nullptr)
        return -1.f;

      const size_t entityIndex = pBody->GetUserData().pointer;

      if (entityIndex == 0)
        return -1.f;

      gameObject *pObject = pool_get(&_Game.gameObjects, entityIndex);

      if (pObject->type == goT_asteroid)
      {
        anyHit = true;
        lastHit = vec2f(point.x, point.y);
        pLastGameObject = pObject;
        lastEntityIndex = entityIndex;

        return fraction;
      }

      return 1.f;
    }

  } _rayCastHandler;

  _rayCastHandler.anyHit = false;
  _rayCastHandler.pLastGameObject = nullptr;
  _rayCastHandler.lastEntityIndex = 0;

  _pBox2DWorld->RayCast(&_rayCastHandler, b2Vec2(pObject->position.x, pObject->position.y), b2Vec2(endPosition.x, endPosition.y));

  pActor->hasLaserHit = _rayCastHandler.anyHit;

  if (_rayCastHandler.anyHit)
  {
    pActor->currentLaserLength = (pObject->position - _rayCastHandler.lastHit).Length();
    
    if (_rayCastHandler.pLastGameObject->type == goT_asteroid)
    {
      _rayCastHandler.pLastGameObject->associatedData.asteroidHealth -= simFactor * pPlayerData->laserStrength;
      _rayCastHandler.pLastGameObject->associatedData.asteroidHealth = lsMax(_rayCastHandler.pLastGameObject->associatedData.asteroidHealth, 0);

      if (_rayCastHandler.pLastGameObject->associatedData.asteroidHealth <= 0)
      {
        gameEvent _event;
        _event.type = geT_asteroidDestroyed;
        _event.position = _rayCastHandler.pLastGameObject->position;
        game_addEvent_internal(&_event);

        if (_rayCastHandler.pLastGameObject->associatedData.asteroidSize != goAS_Small)
        {
          const size_t count = (lsGetRand() % (5 - 3)) + 3;

          for (size_t i = 0; i < count; i++)
          {
            const float_t randAngle = ((lsGetRand() % 0x10000) / (float_t)0x10000) * lsTWOPIf;
            const vec2f direction = vec2f(cosf(randAngle), sinf(randAngle));

            entity entity;

            gameObject smallerAsteroid;
            lsZeroMemory(&smallerAsteroid);
            smallerAsteroid.type = goT_asteroid;
            smallerAsteroid.position = _rayCastHandler.pLastGameObject->position + direction * 0.4f;
            smallerAsteroid.velocity = direction * (0.015f + 0.075f * (lsGetRand() % 0x10000) / (float_t)0x10000);
            smallerAsteroid.shape = goS_sphere;
            smallerAsteroid.shapeData.sphereRadius = _rayCastHandler.pLastGameObject->shapeData.sphereRadius * 0.75f;
            smallerAsteroid.shapeData.sphereRadiusSquared = smallerAsteroid.shapeData.sphereRadius * smallerAsteroid.shapeData.sphereRadius;
            smallerAsteroid.associatedData.asteroidType = _rayCastHandler.pLastGameObject->associatedData.asteroidType;
            smallerAsteroid.associatedData.asteroidSize = (gameObject_asteroidSize)(_rayCastHandler.pLastGameObject->associatedData.asteroidSize + 1);
            smallerAsteroid.associatedData.asteroidHealth = (goAS_Small - smallerAsteroid.associatedData.asteroidSize + 1) * 25.f + 20.f + (lsGetRand() % 0x10000) / (float_t)0x10000 * 5.f;;

            game_addEntity_internal(&entity, &smallerAsteroid);
          }
        }
        else
        {
          pPlayerData->resourceCount[_rayCastHandler.pLastGameObject->associatedData.asteroidType]++;
        }

        game_removeEntity_internal(_rayCastHandler.lastEntityIndex);
        _rayCastHandler.pLastGameObject = nullptr;
        _AsteroidCount--;
      }
    }
  }
  else
  {
    pActor->currentLaserLength = pPlayerData->maxLaserLength;
  }
}

//////////////////////////////////////////////////////////////////////////

void game_phys_addVelocity(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f force)
{
  if (pObj == nullptr)
    pObj = pool_get(&_Game.gameObjects, entityIndex);

  pObj->velocity += force;

  if constexpr (UseBox2DPhysics)
  {
    b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
    pBody->ApplyLinearImpulseToCenter(b2Vec2(force.x, force.y), true);
  }
}

void game_phys_addAngularForce(const size_t entityIndex, OPTIONAL gameObject *pObj, const float_t force)
{
  if (pObj == nullptr)
    pObj = pool_get(&_Game.gameObjects, entityIndex);

  pObj->angularVelocity += force;

  if constexpr (UseBox2DPhysics)
  {
    b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
    pBody->ApplyAngularImpulse(force, true);
  }
}

void game_phys_setTransform(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f position, const float_t rotation)
{
  if (pObj == nullptr)
    pObj = pool_get(&_Game.gameObjects, entityIndex);

  pObj->position = position;
  pObj->rotation = rotation;

  if constexpr (UseBox2DPhysics)
  {
    b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
    pBody->SetTransform(b2Vec2(position.x, position.y), rotation);
  }
}

void game_phys_setTransformAndImpulse(const size_t entityIndex, OPTIONAL gameObject *pObj, const vec2f position, const float_t rotation, const vec2f velocity, const float_t angularVelocity)
{
  if (pObj == nullptr)
    pObj = pool_get(&_Game.gameObjects, entityIndex);

  pObj->position = position;
  pObj->rotation = rotation;
  pObj->velocity = velocity;
  pObj->angularVelocity = angularVelocity;

  if constexpr (UseBox2DPhysics)
  {
    b2Body *pBody = *pool_get(&_Box2DObjects, entityIndex);
    pBody->SetTransform(b2Vec2(position.x, position.y), rotation);
    pBody->SetLinearVelocity(b2Vec2(velocity.x, velocity.y));
    pBody->SetAngularVelocity(angularVelocity);
    pBody->SetAwake(true);
  }
}
