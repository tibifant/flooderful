#include "render.h"

#include "pool.h"
#include "texture.h"
#include "vertexBuffer.h"
#include "framebuffer.h"
#include "shader.h"
#include "obj_reader.h"
#include "data_blob.h"

//////////////////////////////////////////////////////////////////////////

extern const char _Attrib_Pos[] = "position";
extern const char _Attrib_Offs[] = "offset";
extern const char _Attrib_Normal[] = "normal";
extern const char _Attrib_TexCoord[] = "texCoord";
extern const char _Attrib_Color[] = "color";
extern const char _Attrib_Matrix[] = "matrix";
extern const char _Attrib_Rot[] = "rotation";

//////////////////////////////////////////////////////////////////////////

static struct
{
  struct
  {
    shader shader;
    vertexBuffer<vb_attribute_float<2, _Attrib_Pos>> buffer;
  } plane;

  struct
  {
    shader shader;
    vertexBuffer<vb_attribute_float<2, _Attrib_Pos>> buffer;
  } colored_plane;

  struct
  {
    shader shader;
    vertexBuffer<vb_attribute_float<2, _Attrib_Pos>> buffer;
    framebuffer framebuffer;
  } screenQuad;

  struct
  {
    shader shader;
    vertexBuffer<vb_attribute_float<2, _Attrib_Pos>> buffer;
  } hex;

  pool<texture> textures;
  vec3f lookAt, up, cameraDistance;
  matrix vp, vpFar;
  vec2s windowSize;
  float_t frameRatio;
  int64_t lastFrameStartNs;
  float_t ticksSinceOrigin;
} _Render;

//////////////////////////////////////////////////////////////////////////

lsResult render_init(lsAppState *pAppState)
{
  lsResult result = lsR_Success;

  _Render.windowSize = pAppState->windowSize;
  _Render.cameraDistance = vec3f(0, 0, 5.f);
  render_setLookAt(vec2f(0), vec2f(0, 1));
  _Render.lastFrameStartNs = lsGetCurrentTimeNs();

  // Create Screen Quad.
  {
    LS_ERROR_CHECK(shader_createFromFile(&_Render.screenQuad.shader, "shaders/screen.vert", "shaders/screen.frag"));

    float_t renderData[] = { 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0 };
    LS_ERROR_CHECK(vertexBuffer_create(&_Render.screenQuad.buffer, &_Render.screenQuad.shader));
    LS_ERROR_CHECK(vertexBuffer_setVertexBuffer(&_Render.screenQuad.buffer, renderData, LS_ARRAYSIZE(renderData)));

    LS_ERROR_CHECK(framebuffer_create(&_Render.screenQuad.framebuffer, _Render.windowSize, 8, true));
  }

  // Create Plane.
  {
    LS_ERROR_CHECK(shader_createFromFile(&_Render.colored_plane.shader, "shaders/plane.vert", "shaders/colored_plane.frag"));

    float_t renderData[] = { 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0 };
    LS_ERROR_CHECK(vertexBuffer_create(&_Render.colored_plane.buffer, &_Render.colored_plane.shader));
    LS_ERROR_CHECK(vertexBuffer_setVertexBuffer(&_Render.colored_plane.buffer, renderData, LS_ARRAYSIZE(renderData)));
  }

  // Create Colored Plane.
  {
    LS_ERROR_CHECK(shader_createFromFile(&_Render.plane.shader, "shaders/plane.vert", "shaders/plane.frag"));

    float_t renderData[] = { 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0 };
    LS_ERROR_CHECK(vertexBuffer_create(&_Render.plane.buffer, &_Render.plane.shader));
    LS_ERROR_CHECK(vertexBuffer_setVertexBuffer(&_Render.plane.buffer, renderData, LS_ARRAYSIZE(renderData)));
  }

  // Create Default Texture.
  {
    texture defaultTexture;
    LS_ERROR_CHECK(texture_create(&defaultTexture, true));

    const uint32_t textureData[] = { 0xFF333333, 0xFFAAAAAA, 0xFFAAAAAA, 0xFF333333 };
    LS_ERROR_CHECK(texture_set(&defaultTexture, reinterpret_cast<const uint8_t *>(textureData), vec2s(2)));

    size_t _unused;
    LS_ERROR_CHECK(pool_add(&_Render.textures, defaultTexture, &_unused));
  }

  // Create Hexagon.
  {
    LS_ERROR_CHECK(shader_createFromFile(&_Render.hex.shader, "shaders/hex.vert", "shaders/hex.frag"));

    float_t renderData[] = { 0.5f, -0.5f, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0.5, 1.5, 0, 1, 0, 0 };
    LS_ERROR_CHECK(vertexBuffer_create(&_Render.hex.buffer, &_Render.hex.shader));
    LS_ERROR_CHECK(vertexBuffer_setVertexBuffer(&_Render.hex.buffer, renderData, LS_ARRAYSIZE(renderData)));
  }

  // Load textures.
  {
    texture tex;

    LS_ERROR_CHECK(texture_create(&tex, "textures/arrow.png"));
    LS_ERROR_CHECK(pool_insertAt(&_Render.textures, tex, rTI_arrow));
    lsZeroMemory(&tex);

    LS_ERROR_CHECK(texture_create(&tex, "textures/pupu.jpg"));
    LS_ERROR_CHECK(pool_insertAt(&_Render.textures, tex, rTI_pupu));
    lsZeroMemory(&tex);
  }

epilogue:
  return result;
}

void render_startFrame(lsAppState *pAppState)
{
  const int64_t now = lsGetCurrentTimeNs();
  _Render.frameRatio = (float_t)(now - _Render.lastFrameStartNs) / (1e9f / 60.f);
  _Render.lastFrameStartNs = now;
  _Render.windowSize = pAppState->windowSize;

  SDL_GL_MakeCurrent(pAppState->pWindow, pAppState->glContext);
  glViewport(0, 0, (GLsizei)pAppState->windowSize.x, (GLsizei)pAppState->windowSize.y);

  render_clearColor(vec4f(0, 0, 0, 1));
  render_clearDepth();

  render_setDepthMode(rCR_Less);
  render_setBlendEnabled(false);
  render_setDepthTestEnabled(false);

  render_setLookAt(_Render.lookAt, _Render.up);
}

void render_endFrame(lsAppState *pAppState)
{
  (void)pAppState;
}

void render_destroy()
{
  for (auto _item : _Render.textures)
    texture_destroy(_item.pItem);

  pool_destroy(&_Render.textures);

  vertexBuffer_destroy(&_Render.plane.buffer);
  shader_destroy(&_Render.plane.shader);
}

void render_setCameraOffset(const vec3f offset)
{
  _Render.cameraDistance = offset;
  render_setLookAt(_Render.lookAt, _Render.up);
}

void render_setLookAt(const vec2f position, const vec2f up)
{
  render_setLookAt(vec3f(position, 0), vec3f(up, 0));
}

void render_setLookAt(const vec3f position, const vec3f up)
{
  _Render.lookAt = position;
  _Render.up = up.Normalize();

  const matrix v = matrix::LookAtLH(vec(_Render.lookAt - _Render.cameraDistance), vec(_Render.lookAt), vec(_Render.up));

  _Render.vp = v * matrix::PerspectiveFovLH(lsHALFPIf, vec2f(_Render.windowSize).AspectRatio(), 1, 50);
  _Render.vpFar = v * matrix::PerspectiveFovLH(lsHALFPIf, vec2f(_Render.windowSize).AspectRatio(), 10, 1000);
}

void render_setTicksSinceOrigin(const float_t ticksSinceOrigin)
{
  _Render.ticksSinceOrigin = ticksSinceOrigin;
}

void render_drawQuad(const matrix &model, const render_textureId textureIndex)
{
  texture *pTex = pool_get(&_Render.textures, textureIndex);
  texture_bind(pTex, 0);
  shader_bind(&_Render.plane.shader);
  shader_setUniform(&_Render.plane.shader, "texture", pTex);
  shader_setUniform(&_Render.plane.shader, "matrix", model);
  vertexBuffer_render(&_Render.plane.buffer);
}

void render_drawColoredQuad(const matrix &model, const vec4f color, const render_textureId textureIndex)
{
  texture *pTex = pool_get(&_Render.textures, textureIndex);
  texture_bind(pTex, 0);
  shader_bind(&_Render.colored_plane.shader);
  shader_setUniform(&_Render.colored_plane.shader, "color", color);
  shader_setUniform(&_Render.colored_plane.shader, "texture", pTex);
  shader_setUniform(&_Render.colored_plane.shader, "matrix", model);
  vertexBuffer_render(&_Render.colored_plane.buffer);
}

void render_draw2DQuad(const matrix &model, const render_textureId textureIndex)
{
  render_drawQuad(model * matrix::Scale(2.f / _Render.windowSize.x, 2.f / _Render.windowSize.y, 0) * matrix::Translation(-1.f, -1.f, 0) * matrix::Scale(1, -1, 0), textureIndex);
}

void render_drawColored2DQuad(const matrix &model, const vec4f color, const render_textureId textureIndex)
{
  render_drawColoredQuad(model * matrix::Scale(2.f / _Render.windowSize.x, 2.f / _Render.windowSize.y, 0) * matrix::Translation(-1.f, -1.f, 0) * matrix::Scale(1, -1, 0), color, textureIndex);
}

void render_draw3DQuad(const matrix &model, const render_textureId textureIndex)
{
  render_drawQuad(model * _Render.vp, textureIndex);
}

void render_drawHex(const matrix &model, const vec4f color)
{
  shader_bind(&_Render.hex.shader);
  shader_setUniform(&_Render.hex.shader, "color", color);
  shader_setUniform(&_Render.hex.shader, "matrix", model);
  vertexBuffer_render(&_Render.hex.buffer);
}

void render_drawHex2D(const matrix &model, const vec4f color)
{
  render_drawHex(model * matrix::Scale(2.f / _Render.windowSize.x, 2.f / _Render.windowSize.y, 0) * matrix::Translation(-1.f, -1.f, 0) * matrix::Scale(1, -1, 0), color);
}

void render_drawHex3D(const matrix &model, const vec4f color)
{
  render_drawHex(model * _Render.vp, color);
}

void render_drawArrow(size_t x, size_t y, direction dir)
{
  const matrix mat = matrix::Translation(-0.5f, -0.5f, 0) * matrix::Scale(-1.f, -1.f, 0) * matrix::RotationZ((-1.f - 3.f + 2 * dir) / 6.f * lsPIf) * matrix::Translation(0.5f, 0.5f, 0) * matrix::Scale(30.f, 30.f, 0);

  if (y % 2 == 0)
    render_draw2DQuad(mat * matrix::Translation(75.f + x * 66.f, 80.f + y * 65.f, 0), rTI_arrow);
  else
    render_draw2DQuad(mat * matrix::Translation(110.f + x * 66.f, 80.f + y * 65.f, 0), rTI_arrow);
}

size_t r = 0;
float_t z = 0;

void render_drawMap(const level_info &levelInfo, lsAppState *pAppState, const pathfinding_target_type debugArrow, const vec4f lightColor)
{
  (void)pAppState;

  // TODO!!
  const vec4f colors[tT_count] = {
    vec4f(0.1f, 0.4f, 0, 0), // tT_grass
    vec4f(0.3f, 0.22f, 0.17f, 0), // tT_soil
    vec4f(0, 0.2f, 0.4f, 0), // tT_water 
    vec4f(0.4f, 0.35f, 0.2f, 0), // tT_sand
    vec4f(0.45f, 0.5f, 0.4f, 0), // tT_sapling
    vec4f(0.4f, 0.5f, 0.44f, 0), // tT_tree 
    vec4f(0.46f, 0.4f, 0.5f, 0), // tT_trunk 
    vec4f(0.5f, 0.42f, 0.4f, 0), // tT_wood
    vec4f(0.6f, 0.1f, 0.f, 0), // tT_fire
    vec4f(0.5f, 0.08f, 0.0f, 0), // tT_fire_pit
    vec4f(0.3f, 0, 0.6f, 0), // tT_tomato_plant
    vec4f(0.4f, 0, 0.5f, 0), // tT_bean_plant
    vec4f(0.5f, 0, 0.4f, 0), // tT_wheat_plant
    vec4f(0.6f, 0, 0.2f, 0), // tT_sunflower_plant
    vec4f(0.2f, 0, 0.5f, 0), // tT_tomato 
    vec4f(0.3f, 0, 0.4f, 0), // tT_bean 
    vec4f(0.4f, 0, 0.3f, 0), // tT_wheat 
    vec4f(0.5f, 0, 0.1f, 0), // tT_sunflower 
    vec4f(0.6f, 0, 0.05f, 0), // tT_meal
    vec4f(0.25f, 0.25f, 0.25f, 0), // tT_mountain
  };

  for (size_t y = 0; y < levelInfo.map_size.y; y++)
  {
    for (size_t x = 0; x < levelInfo.map_size.x; x++)
    {
      float_t v = 1.f;

      if (y % 2)
        v = 1.55f;

      render_drawHex2D(matrix::Translation(v + x * 1.1f, 2.f + y * 1.6f, 0) * matrix::Scale(60.f, 40.f, 0), colors[levelInfo.pGameplayMap[y * levelInfo.map_size.x + x].tileType] * lightColor + vec4f(0.1f, 0.1f, 0.1f, 0) * levelInfo.pPathfindingMap[y * levelInfo.map_size.x + x].elevationLevel);
    }
  }

  // Cursor
  {
    render_setBlendEnabled(true);
    render_setBlendMode(rBF_Additive);

    float_t v = 1.f;

    if (levelInfo.playerPos.y % 2)
      v = 1.55f;

    render_drawHex2D(matrix::Translation(v + levelInfo.playerPos.x * 1.1f, 2.f + levelInfo.playerPos.y * 1.6f, 0) * matrix::Scale(60.f, 40.f, 0), vec4f(1.f, 1.f, 1.f, 0.25f + 0.25f * (lsSin(lsGetCurrentTimeMs() * (lsTWOPIf / 1000.0f)))));
  }

  render_setBlendEnabled(false);

  // Draw Debug Arrows.
  {
    for (size_t j = 0; j < levelInfo.map_size.x * levelInfo.map_size.y; j++)
    {
      const direction dir = (direction)levelInfo.resources[debugArrow].pDirectionLookup[1 - levelInfo.resources[debugArrow].write_direction_idx][j].dir;

      if (dir != d_unreachable && dir < d_atDestination)
        render_drawArrow(j % levelInfo.map_size.x, j / levelInfo.map_size.x, dir);
    }
  }
}

void render_drawActor(const movement_actor actor, size_t index) // In Future: flush all actors being drawed.
{
  vec4f color = vec4f(0.1f + index * 0.2f, 1.f - index * 0.2f, 1.f, 1.f);

  const matrix mat = matrix::Translation(-0.5f, -0.5f, 0) * matrix::Scale(-1.f, -1.f, 0) * matrix::Translation(0.5f, 0.5f, 0) * matrix::Scale(30.f, 50.f, 0);
  render_drawColored2DQuad(mat * matrix::Translation(75.f + actor.pos.x * 66.f, 70.f + actor.pos.y * 65.f, 0), color, rTI_pupu);
}

void render_flushRenderQueue()
{

}

//////////////////////////////////////////////////////////////////////////

void render_clearColor(const vec4f color)
{
  glClearColor(color.x, color.y, color.z, color.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void render_clearDepth()
{
  glClear(GL_DEPTH_BUFFER_BIT);
}

void render_setDepthMode(const render_comparisonResult mode)
{
  GLenum param;

  switch (mode)
  {
  case rCR_Less: param = GL_LESS; break;
  case rCR_LessOrEqual: param = GL_LEQUAL; break;
  case rCR_Equal: param = GL_EQUAL; break;
  case rCR_GreaterOrEqual: param = GL_GEQUAL; break;
  case rCR_Greater: param = GL_GREATER; break;
  case rCR_NotEqual: param = GL_NOTEQUAL; break;
  case rCR_Always: param = GL_ALWAYS; break;
  default: return;
  }

  glDepthFunc(param);
}

void render_setDepthTestEnabled(const bool enabled)
{
  (enabled ? glEnable : glDisable)(GL_DEPTH_TEST);
}

void render_setBlendMode(const render_blendFunc mode)
{
  switch (mode)
  {
  case rBF_Additive:
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    break;

  case rBF_AlphaBlend:
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    break;

  case rBF_Premultiplied:
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    break;

  case rBF_Override:
    glBlendFunc(GL_ONE, GL_ZERO);
    glBlendEquation(GL_FUNC_ADD);
    break;

  case rBF_AlphaMask:
    glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_ZERO, GL_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    break;

  default:
    return;
  }
}

void render_setBlendEnabled(const bool enabled)
{
  (enabled ? glEnable : glDisable)(GL_BLEND);
}

void render_finalize()
{
  glFlush();
  glFinish();
}
