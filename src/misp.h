#ifndef MISP_H
#define MISP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
  uint64_t dt;
  uint8_t mt;
} cell_t;

typedef enum
{
  MISP_PANIC_NO = 0,
  MISP_PANIC_TYPE_ERROR = 1,
  MISP_PANIC_OUT_OF_BOUNDS = 2,
  MISP_PANIC_INVALID_OPC = 3,
  MISP_PANIC_BAD_NODE = 4,
  MISP_PANIC_BAD_NODE_PARAMS = 5,
} misp_panic_type_t;

typedef struct
{
  misp_panic_type_t type;
  cell_t node;
} misp_panic_t;

typedef struct
{
  /* MEMORY */
  uint8_t *mem;
  size_t mem_size;

  /* CONTROL FLOW */
  cell_t env;
  bool trapped;
  bool halted;

  misp_panic_t panic_code;
} misp_t;

void misp_init (misp_t *M, uint8_t *mem, size_t mem_size, cell_t init);

void misp_execute (misp_t *M);

// NOTE: Need automatic garbage collector in order to ensure safety. A free
// keyword cannot be implemented, because we cannot be sure that there are no
// references to list, unless reference counted
void misp_gc (misp_t *M);

#endif
