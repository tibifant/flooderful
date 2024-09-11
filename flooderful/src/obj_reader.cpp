#include "obj_reader.h"

#include "io.h"
#include "queue.h"

//////////////////////////////////////////////////////////////////////////

constexpr bool LogObjParser = true;
#define ObjParserLogPrefix "[   OBJ Parser   ] "

//////////////////////////////////////////////////////////////////////////

inline static const char * obj_reader_findLineEnd_internal(const char *pos)
{
  lsAssert(pos != nullptr);

  while (*pos != '\n' && *pos != '\r' && *pos != '\0')
    pos++;

  return pos;
}

//////////////////////////////////////////////////////////////////////////

lsResult obj_reader_load(obj_info *pInfo, const char *contents)
{
  lsResult result = lsR_Success;

  list<vec2f> texCoords;
  list<vec3f> normals;

  struct vertexData
  {
    uint32_t index[3];
  };

  queue<vertexData> indices;
  bool hasData = true;

  LS_ERROR_IF(pInfo == nullptr || contents == nullptr, lsR_ArgumentNull);

  while (hasData)
  {
    switch (*contents)
    {
      case 'v':
      case 'V':
      {
        contents++;

        switch (*contents)
        {
          case ' ':
          {
            contents++;

            obj_info_vertex vertex;
            lsZeroMemory(&vertex);

            size_t index = 0;

            while (index < 3)
            {
              while (*contents == ' ')
                contents++;

              vertex.position.asArray[index] = (float_t)lsParseFloat(contents, &contents);
              index++;
            }

            while (*contents == ' ')
              contents++;

            if (*contents != '\n' && *contents != '\r' && lsStartsWithInt(contents)) // vertex color.
            {
              index = 0;
              vertex.hasColor = true;

              while (index < 3)
              {
                while (*contents == ' ')
                  contents++;

                vertex.color.asArray[index] = (float_t)lsParseFloat(contents, &contents);
                index++;
              }
            }

            LS_ERROR_CHECK(list_add(&pInfo->vertices, &vertex));

            break;
          }

          case 'n':
          case 'N':
          {
            contents++;

            vec3f normal;

            size_t index = 0;

            while (index < 3)
            {
              while (*contents == ' ')
                contents++;

              normal.asArray[index] = (float_t)lsParseFloat(contents, &contents);
              index++;
            }

            LS_ERROR_CHECK(list_add(&normals, &normal));
            break;
          }

          case 't':
          case 'T':
          {
            contents++;

            vec2f tex;

            size_t index = 0;

            while (index < 2)
            {
              while (*contents == ' ')
                contents++;

              tex.asArray[index] = (float_t)lsParseFloat(contents, &contents);
              index++;
            }

            LS_ERROR_CHECK(list_add(&texCoords, &tex));
            break;
          }

          default:
          {
            contents = obj_reader_findLineEnd_internal(contents);
            break;
          }
        }

        contents = obj_reader_findLineEnd_internal(contents);
        break;
      }

      case 'f':
      case 'F':
      {
        contents++;

        if (*contents == ' ')
        {
          contents++;

          vertexData data;
          queue_clear(&indices);

          size_t vertexCount = 0;

          while (vertexCount < 4)
          {
            while (*contents == ' ')
              contents++;

            if (*contents == '\n' || *contents == '\r')
              break;

            for (size_t i = 0; i < 3; i++)
            {
              if (*contents == '/')
                data.index[i] = (uint32_t)-1;
              else
                data.index[i] = (uint32_t)lsParseInt(contents, &contents);
              
              if (*contents == '/')
              {
                contents++;
              }
              else if (*contents == ' ' || *contents == '\n' || *contents == '\r')
              {
                for (size_t j = i + 1; j < 3; j++)
                  data.index[j] = (uint32_t)-1;

                break;
              }
            }

            vertexCount++;

            LS_ERROR_CHECK(queue_pushBack(&indices, &data));
          }

          lsAssert(vertexCount >= 3);

          while (indices.count >= 3)
          {
            obj_triangle<obj_info_triangle_vertex> tri;
            lsZeroMemory(&tri);

            for (size_t i = 0; i < 3; i++)
            {
              vertexData j;
              LS_ERROR_CHECK(queue_get(&indices, i, &j));
              tri.v[i].vertexIdx = j.index[0] - 1;
              LS_ERROR_CHECK(list_get_safe(&pInfo->vertices, tri.v[i].vertexIdx, static_cast<obj_info_vertex *>(&tri.v[i])));

              if (j.index[1] != (uint32_t)-1)
              {
                LS_ERROR_CHECK(list_get_safe(&texCoords, j.index[1] - 1, &tri.v[i].texCoord));
                tri.v[i].hasTexCoord = true;
              }

              if (j.index[2] != (uint32_t)-1)
              {
                LS_ERROR_CHECK(list_get_safe(&normals, j.index[2] - 1, &tri.v[i].normal));
                tri.v[i].hasNormal = true;
              }
            }

            LS_ERROR_CHECK(list_add(&pInfo->triangles, &tri));

            vertexData _unused;
            LS_ERROR_CHECK(queue_popFront(&indices, &_unused));
          }
        }

        contents = obj_reader_findLineEnd_internal(contents);
        break;
      }

      case '\n':
      case '\r':
      {
        contents++;
        break;
      }

      case '\0':
      {
        hasData = false;
        break;
      }

      default:
      {
        contents = obj_reader_findLineEnd_internal(contents);        
        break;
      }
    }
  }

epilogue:
  list_destroy(&texCoords);
  list_destroy(&normals);
  queue_destroy(&indices);

  return result;
}

lsResult obj_reader_load_file(obj_info *pInfo, const char *filename)
{
  lsResult result = lsR_Success;

  size_t chars;
  char *fileStart = nullptr;
  int64_t start, end;

  LS_ERROR_IF(pInfo == nullptr || filename == nullptr, lsR_ArgumentNull);
  LS_ERROR_CHECK(lsReadFile(filename, &fileStart, &chars));

  start = lsGetCurrentTimeNs();

  LS_ERROR_CHECK(obj_reader_load(pInfo, fileStart));

  end = lsGetCurrentTimeNs();

  if constexpr (LogObjParser)
    print_log_line(ObjParserLogPrefix "Parsed '", filename, "' (", chars, " bytes) in ", (end - start) * 1e-9, " s (", FF(Frac(1))((float_t)chars / ((end - start) * 1e-9f * 1024 * 1024)), " MB/s).");

epilogue:
  lsFreePtr(&fileStart);
  return result;
}
