#ifndef MISP_PARSER_H
#define MISP_PARSER_H
#include "misp.h"

typedef enum
{
  MISP_PARSER_ERROR_OK = 0,
  MISP_PARSER_INVALID,
} misp_parser_error_type_t;

misp_parser_error_type_t misp_parse_string (const char *s, uint8_t *tree[],
                                            size_t *tree_size, cell_t *root);

#endif
