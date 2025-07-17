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

  if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_W))
    game_setPlayerMapIndex(d_topLeft);
  else if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_E))
    game_setPlayerMapIndex(d_topRight);
  else if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_A))
    game_setPlayerMapIndex(d_left);
  else if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_D))
    game_setPlayerMapIndex(d_right);
  else if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_Z))
    game_setPlayerMapIndex(d_bottomLeft);
  else if (lsKeyboardState_KeyPress(&pAppState->keyboardState, SDL_SCANCODE_X))
    game_setPlayerMapIndex(d_bottomRight);

  for (int32_t i = 0; i < 11; i++)
  {
    // resource type 0 - 9
    if (lsKeyboardState_KeyPress(&pAppState->keyboardState, i + SDL_SCANCODE_1))
    {
      game_playerSwitchTiles((resource_type)i);
      break;
    }
    // resource type 10 - 19
    else if (lsKeyboardState_KeyPress(&pAppState->keyboardState, i + SDL_SCANCODE_KP_1))
    {
      game_playerSwitchTiles((resource_type)(i + 10));
      break;
    }
  }

  // Draw Scene
  {
    const float_t ticksSinceOrigin = (pView->pGame->lastPredictTimeNs - pView->pGame->gameStartTimeNs) / (1e9f / pView->pGame->tickRate);

    render_setTicksSinceOrigin(ticksSinceOrigin);

    // rendered objects
    if (pView->pGame->levelInfo.isNight)
      render_drawMap(pView->pGame->levelInfo, pAppState, pool_get(pView->pGame->movementActors, 2)->target, vec4f(0.6f, 0.6f, 0.8f, 0));
    else
      render_drawMap(pView->pGame->levelInfo, pAppState, pool_get(pView->pGame->movementActors, 2)->target, vec4f(1.f, 1.f, 1.f, 0));

    for (const auto &&_actor : pView->pGame->movementActors)
      render_drawActor(*_actor.pItem, _actor.index);

    render_flushRenderQueue();
  }

  render_endFrame(pAppState);

epilogue:
  return result;
}

void gameView_destroy(lsAppView **ppSelf, lsAppState *pAppState)
{
  (void)pAppState;

  lsFreePtr(ppSelf);
}
