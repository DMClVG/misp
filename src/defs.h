#ifndef MISP_DEFS_H
#define MISP_DEFS_H

#include "misp.h"

#define TYPE_NUM 0
#define TYPE_LIST 1

#define CELL(data, type) ((cell_t){ data, type & 0x1 })
#define CELL_TYPE(c) ((c).mt & 0x1)

#define IS_LIST(c) (CELL_TYPE (c) == TYPE_LIST)
#define IS_NUM(c) (CELL_TYPE (c) == TYPE_NUM)

#define LIST(len, p) CELL (((uint64_t)(p) << 32) | (uint64_t)(len), TYPE_LIST)

#define LIST_LEN(c) (uint64_t) ((c).dt & 0xFFFFFFFF)
#define LIST_PTR(c) (uint64_t) (((c).dt >> 32) & 0xFFFFFFFF)

#define NUM_VAL(c) (int64_t) (c.dt)
#define NUM(c) CELL ((uint64_t)(c), TYPE_NUM)

#define LIST_NULL LIST (0, 0)
#define CELL_SIZE 9

#define CELL_WRITE(b, c)                                                      \
  {                                                                           \
    memcpy (b, &c, CELL_SIZE);                                                \
  }

#define CELL_READ(b, c)                                                       \
  {                                                                           \
    memcpy (c, b, CELL_SIZE);                                                 \
  }

#endif
