#include "gameView.h"
#include "game.h"
#include "render.h"

//////////////////////////////////////////////////////////////////////////

struct gameView : lsAppView
{
  game game;
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
  bool lagSwitch = false;
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

  while (pView->game.entities.count == 0)
  {
    LS_ERROR_CHECK(game_observe(&pView->game));
    Sleep(1);
  }

  //size_t playerEntityIndex; // is identical to the game object index.
  //gameObject *pGameObject = pool_get(&pView->game.gameObjects, playerEntityIndex);

  //pView->lookAtPos = pGameObject->position;
  //pView->lookAtRotation = pGameObject->rotation;
  //pView->lookAtDistance = 35.f;

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
  
  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_RETURN))
    pView->lagSwitch = !pView->lagSwitch;

  LS_ERROR_CHECK(game_tick());

  if (!pView->lagSwitch)
    LS_ERROR_CHECK(game_observe(&pView->game));
  
  game_predict(&pView->game);

  // Handle Camera.

  // Handle Particles.

  // Update events.

  // Draw Scene
  {
    const float_t ticksSinceOrigin = (pView->game.lastPredictTimeNs - pView->game.gameStartTimeNs) / (1e9f / pView->game.tickRate);

    render_setTicksSinceOrigin(ticksSinceOrigin);

    // rendered objects

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
