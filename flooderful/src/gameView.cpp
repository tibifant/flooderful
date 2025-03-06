#include "gameView.h"
#include "game.h"
#include "render.h"

//////////////////////////////////////////////////////////////////////////

struct gameView : lsAppView
{
  game *pGame;
  vec2f lookAtPos;
  float_t lookAtRotation;
  float_t lookAtDistance;

#pragma pack(1)
  struct particle
  {
    vec2f position;
    float_t transparency;
  } particles[512];

  _STATIC_ASSERT(sizeof(particle) == sizeof(vec3f));
  _STATIC_ASSERT(sizeof(particle) == sizeof(float_t) * 3);

  vec2f particleVelocities[LS_ARRAYSIZE_C_STYLE(particles)];

  size_t lastParticleIndex = 0;
};

//////////////////////////////////////////////////////////////////////////

lsResult gameView_update(lsAppView *pSelf, lsAppView **ppNext, lsAppState *pAppState);
void gameView_destroy(lsAppView **ppSelf, lsAppState *pAppState);

//////////////////////////////////////////////////////////////////////////

lsResult gameView_init(_Out_ lsAppView **ppView, lsAppState *pAppState)
{
  lsResult result = lsR_Success;

  gameView *pView = nullptr;

  LS_ERROR_IF(ppView == nullptr || pAppState == nullptr, lsR_ArgumentNull);

  LS_ERROR_CHECK(lsAllocZero(&pView));

  pView->pUpdate = gameView_update;
  pView->pDestroy = gameView_destroy;

  *ppView = pView;

  LS_ERROR_CHECK(game_init());
  // added players here

  //while (pView->game.entities.count == 0)
  //{
  //  LS_ERROR_CHECK(game_observe(&pView->game));
  //  Sleep(1);
  //}

  //size_t playerEntityIndex; // is identical to the game object index.
  //gameObject *pGameObject = pool_get(&pView->game.gameObjects, playerEntityIndex);

  //pView->lookAtPos = pGameObject->position;
  //pView->lookAtRotation = pGameObject->rotation;
  //pView->lookAtDistance = 35.f;

  pView->pGame = game_getGame();

epilogue:
  if (LS_FAILED(result))
    lsFreePtr(&pView);

  return result;
}

//////////////////////////////////////////////////////////////////////////

lsResult gameView_update(lsAppView *pSelf, lsAppView **ppNext, lsAppState *pAppState)
{
  lsResult result = lsR_Success;

  gameView *pView = static_cast<gameView *>(pSelf);

  (void)ppNext;

  render_startFrame(pAppState);


  LS_ERROR_CHECK(game_tick());

  // Handle Camera.

  // Handle Particles.

  // Update events.

  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_LEFT))
    game_setPlayerMapIndex(true);

  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_RIGHT))
    game_setPlayerMapIndex(false);

  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_1))
    game_playerSwitchTiles(ptT_grass);

  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_2))
    game_playerSwitchTiles(ptT_water);

  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_3))
    game_playerSwitchTiles(ptT_sand);

  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_4))
    game_playerSwitchTiles(ptT_collidable);

  // Draw Scene
  {
    const float_t ticksSinceOrigin = (pView->pGame->lastPredictTimeNs - pView->pGame->gameStartTimeNs) / (1e9f / pView->pGame->tickRate);

    render_setTicksSinceOrigin(ticksSinceOrigin);

    // rendered objects
    pathfinding_element e;
    e.tileType = ptT_protein;
    e.elevationLevel = 1;

    render_drawMap(pView->pGame->levelInfo, pAppState, e);

    for (const auto &&_actor : pView->pGame->movementActors)
      render_drawActor(*_actor.pItem, _actor.index);

    render_flushRenderQueue();
  }

  render_endFrame(pAppState);

  // Draw UI

epilogue:
  return result;
}

void gameView_destroy(lsAppView **ppSelf, lsAppState *pAppState)
{
  (void)pAppState;

  lsFreePtr(ppSelf);
}
