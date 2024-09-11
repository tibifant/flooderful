#include "obj_writer.h"
#include "io.h"

lsResult obj_writer_create(obj_writer &writer, const char *objectName)
{
  lsResult result = lsR_Success;

  LS_ERROR_CHECK(string_append(writer.str, sformat("# X Engine\no ", objectName, "\n")));

epilogue:
  return result;
}

lsResult obj_writer_store(const obj_writer &writer, const char *filename)
{
  lsAssert(writer.str.bytes > 0);
  return lsWriteFile(filename, writer.str.text, writer.str.bytes - 1);
}

lsResult obj_writer_add_vertex(obj_writer &writer, const vec3f pos)
{
  return string_append(writer.str, sformat("v ", pos.x, ' ', pos.y, ' ', pos.z, '\n'));
}

lsResult obj_writer_add_vertex_with_color(obj_writer &writer, const vec3f pos, const vec3f color)
{
  return string_append(writer.str, sformat("v ", pos.x, ' ', pos.y, ' ', pos.z, ' ', color.x, ' ', color.y, ' ', color.z, "\n"));
}

lsResult obj_writer_add_vertex_normal(obj_writer &writer, const vec3f pos)
{
  return string_append(writer.str, sformat("vn ", pos.x, ' ', pos.y, ' ', pos.z, '\n'));
}

lsResult obj_writer_add_triangle(obj_writer &writer, const uint32_t vertIdx[3])
{
  return string_append(writer.str, sformat("f ", vertIdx[0] + 1, ' ', vertIdx[1] + 1, ' ', vertIdx[2] + 1, '\n'));
}

lsResult obj_writer_add_triangle_with_normals(obj_writer &writer, const uint32_t vertIdx[3], const uint32_t normalIdx[3])
{
  return string_append(writer.str, sformat("f ", vertIdx[0] + 1, "//", normalIdx[0] + 1, ' ', vertIdx[1] + 1, "//", normalIdx[1] + 1, ' ', vertIdx[2] + 1, "//", normalIdx[2] + 1, '\n'));
}
