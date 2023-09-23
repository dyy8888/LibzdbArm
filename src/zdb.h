/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.
 */


#ifndef ZDB_INCLUDED
#define ZDB_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Include this interface in your C code to import the libzdb API.
 *
 * @file
 */

/* -------------------- Version: major * 1000000 + minor * 1000 + revision */

#define LIBZDB_VERSION          "3.2.3"
#define LIBZDB_VERSION_NUMBER   3002003

/* ------------------------------------------------- libzdb API interfaces */

#include <SQLException.h>
#include <URL.h>
#include <ResultSet.h>
#include <PreparedStatement.h>
#include <Connection.h>
#include <ConnectionPool.h>

#ifdef __cplusplus
}
#endif
#endif
