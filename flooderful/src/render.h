#pragma once

#include "platform.h"
#include "game.h"
#include "local_list.h" // remeber to remove if nomore needed.

enum render_textureId : size_t
{
  rTI_default,
  rTI_arrow,
  rTI_pupu,
};

lsResult render_init(lsAppState *pAppState);
void render_startFrame(lsAppState *pAppState);
void render_endFrame(lsAppState *pAppState);
void render_destroy();

void render_setCameraOffset(const vec3f offset);
void render_setLookAt(const vec2f position, const vec2f up);
void render_setLookAt(const vec3f position, const vec3f up);
void render_setTicksSinceOrigin(const float_t ticksSinceOrigin);

void render_drawQuad(const matrix &model, const render_textureId textureIndex);
void render_drawColoredQuad(const matrix &model, const vec4f color, const render_textureId textureIndex);
void render_draw2DQuad(const matrix &model, const render_textureId textureIndex);
void render_drawColored2DQuad(const matrix &model, const vec4f color, const render_textureId textureIndex);
void render_draw3DQuad(const matrix &model, const render_textureId textureIndex);
void render_drawHex(const matrix &model, const vec4f color);
void render_drawHex2D(const matrix &model, const vec4f color);
void render_drawHex3D(const matrix &model, const vec4f color);
void render_drawMap(const level_info &levelInfo, lsAppState *pAppState, terrain_type debuggArrow);
void render_drawActor(const movement_actor actor, size_t index);

void render_flushRenderQueue();

void render_finalize();

void render_clearColor(const vec4f color);
void render_clearDepth();

enum render_comparisonResult
{
  rCR_Less,
  rCR_LessOrEqual,
  rCR_Equal,
  rCR_GreaterOrEqual,
  rCR_Greater,
  rCR_NotEqual,
  rCR_Always,
};

void render_setDepthMode(const render_comparisonResult mode);
void render_setDepthTestEnabled(const bool enabled);

enum render_blendFunc
{
  rBF_Additive,
  rBF_AlphaBlend,
  rBF_Premultiplied,
  rBF_Override,
  rBF_AlphaMask,
};

void render_setBlendMode(const render_blendFunc mode);
void render_setBlendEnabled(const bool enabled);
