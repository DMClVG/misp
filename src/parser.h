/*************************************************************************/
/* MISP                                                                  */
/* Copyright (C) 2023                                                    */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/*************************************************************************/

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
