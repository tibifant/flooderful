#pragma once

#include "core.h"
#include "list.h"

template <typename T>
struct obj_triangle
{
  T v[3];
};

struct obj_info_vertex
{
  vec3f position;
  vec3f color;
  bool hasColor = false;
};

struct obj_info_triangle_vertex : obj_info_vertex
{
  vec3f normal;
  vec2f texCoord;
  uint8_t hasTexCoord : 1;
  uint8_t hasNormal : 1;
  uint32_t vertexIdx;
};

struct obj_info
{
  list<obj_triangle<obj_info_triangle_vertex>> triangles;
  list<obj_info_vertex> vertices;
};

lsResult obj_reader_load(_Out_ obj_info *pInfo, const char *contents);
lsResult obj_reader_load_file(_Out_ obj_info *pInfo, const char *filename);
