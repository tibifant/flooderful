#pragma once

#include "core.h"
#include "obj_reader.h"
#include "raw_string.h"

struct obj_writer
{
  raw_string str;
};

lsResult obj_writer_create(obj_writer &writer, const char *objectName);
lsResult obj_writer_store(const obj_writer &writer, const char *filename);

lsResult obj_writer_add_vertex(obj_writer &writer, const vec3f pos);
lsResult obj_writer_add_vertex_with_color(obj_writer &writer, const vec3f pos, const vec3f color);

lsResult obj_writer_add_vertex_normal(obj_writer &writer, const vec3f pos);

lsResult obj_writer_add_triangle(obj_writer &writer, const uint32_t vertIdx[3]);
lsResult obj_writer_add_triangle_with_normals(obj_writer &writer, const uint32_t vertIdx[3], const uint32_t normalIdx[3]);
