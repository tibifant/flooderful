#pragma once

#include "core.h"
#include "pool.h"
#include "queue.h"
#include "dataBlob.h"

enum gameObject_type
{
  goT_invalid,
  goT_spaceship,
  goT_projectile,
  goT_asteroid,
};

enum gameObject_shape
{
  goS_notCollidable,
  goS_sphere,
};

enum gameObject_asteroidType
{
  goAT_stone,
  goAT_copper,
  goAT_salt,
  goAT_ice,
  goAT_gold,

  _goAT_count
};

enum gameObject_asteroidSize
{
  goAS_Large,
  goAS_Medium,
  goAS_Small
};

struct gameObject
{
  vec2f position, velocity;
  float_t rotation, angularVelocity;
  gameObject_type type;
  gameObject_shape shape;
  
  union
  {
#pragma warning(push)
#pragma warning(disable: 4201)
    struct
    {
      float_t sphereRadius, sphereRadiusSquared;
    };
#pragma warning(pop)
  } shapeData;

  union
  {
#pragma warning(push)
#pragma warning(disable: 4201)
    struct
    {
      size_t playerIndex;
    };

    struct
    {
      gameObject_asteroidType asteroidType;
      float_t asteroidHealth;
      gameObject_asteroidSize asteroidSize;
    };
#pragma warning(pop)
  } associatedData;
};

struct actor
{
  bool thrusterForward : 1, thrusterBack : 1, thrusterTurnLeft : 1, thrusterTurnRight : 1, isPlayer : 1, hasFocus : 1, hasLaser : 1, laserOn : 1, hasLaserHit : 1;
  float_t currentLaserLength;
  size_t partnerEntityIndex;
};

struct playerData
{
  size_t entityIndex;
  float_t speed, rotationSpeed, reverseSpeed;
  float_t projectileSpeed, projectileRotationSpeed, projectileReverseSpeed;
  float_t turnBonusSpeed, projectileTurnBonusSpeed;
  float_t projectileAutoSpeed, projectileKillRadiusSquared;
  size_t score;
  float_t maxLaserLength, laserStrength;
  size_t resourceCount[_goAT_count];
};

enum component_type : uint8_t
{
  ct_none,
  ct_gameObject,
  ct_actor,
  ct_playerData,
};

struct entity
{
  static constexpr size_t MaxComponents = 7;

  component_type components[MaxComponents] = { };
  size_t ids[MaxComponents] = { };
};

enum gameEvent_type
{
  geT_invalid,
  geT_projectileExplosion,
  geT_teleportation,
  geT_asteroidDestroyed,
};

struct gameEvent
{
  size_t index;
  uint64_t timeNs;
  vec2f position;
  gameEvent_type type;

  union
  {
#pragma warning(push)
#pragma warning(disable: 4201)
    struct
    {
      gameObject_asteroidSize asteroidSize;
    };
#pragma warning(pop)
  } associatedData;
};

struct game
{
  uint64_t lastUpdateTimeNs, gameStartTimeNs, lastPredictTimeNs;
  pool<entity> entities; // every entity must have a game object with the same index.
  pool<gameObject> gameObjects; // every game object must have a entity with the same index.
  pool<playerData> players;
  pool<actor> actors;
  queue<gameEvent> events;
  
  float_t movementFriction = 0.965, turnFriction = 0.9;
  size_t tickRate = 60;
  size_t lastEventIndex, eventUpdateCutoffIndex;
};

lsResult game_init();
lsResult game_tick();
lsResult game_observe(_Out_ game *pGame);
lsResult game_clone(const game *pSrc, _Out_ game *pDst);
void game_predict(game *pGame);

void game_set_authority(const bool hasAuthority);
void game_set_local(const bool isLocal);

void game_spaceship_thruster_set_state(const size_t playerIndex, const size_t thrusterIndex, const bool enabled);
void game_spaceship_fire(const size_t playerIndex);
void game_projectile_thruster_set_state(const size_t playerIndex, const size_t thrusterIndex, const bool enabled);
void game_projectile_trigger(const size_t playerIndex);
void game_switch(const size_t playerIndex);
void game_spaceship_laser_set_state(const size_t playerIndex, const bool laserOn);
size_t game_addPlayer();

size_t game_getFirstComponentIndex(game *pGame, const size_t entityIndex, const component_type compoenent);
actor *game_getActiveActorForPlayer(game *pGame, const size_t playerIndex, _Out_ size_t *pEntityIndex = nullptr);
bool game_hasAnyEvent(game *pGame);
gameEvent game_getNextEvent(game *pGame);
size_t game_getTickRate();
size_t game_getLevelSize();

lsResult game_serialize(_Out_ dataBlob *pBlob);
lsResult game_deserialze(_Out_ game *pGame, _In_ const void *pData, const size_t size);
