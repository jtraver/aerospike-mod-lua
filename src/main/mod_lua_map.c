/******************************************************************************
 * Copyright 2008-2013 by Aerospike.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

#include <aerospike/as_iterator.h>
#include <aerospike/as_map.h>
#include <aerospike/as_map_iterator.h>
#include <aerospike/as_val.h>

#include <aerospike/mod_lua_val.h>
#include <aerospike/mod_lua_map.h>
#include <aerospike/mod_lua_iterator.h>
#include <aerospike/mod_lua_reg.h>

#include "internal.h"

/*******************************************************************************
 * MACROS
 ******************************************************************************/

#define OBJECT_NAME "map"
#define CLASS_NAME  "Map"

/*******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

 as_map * mod_lua_tomap(lua_State * l, int index) {
    mod_lua_box * box = mod_lua_tobox(l, index, CLASS_NAME);
    return (as_map *) mod_lua_box_value(box);
}

as_map * mod_lua_pushmap(lua_State * l, as_map * map) {
    mod_lua_box * box = mod_lua_pushbox(l, MOD_LUA_SCOPE_LUA, map, CLASS_NAME);
    return (as_map *) mod_lua_box_value(box);
}

static as_map * mod_lua_checkmap(lua_State * l, int index) {
    mod_lua_box * box = mod_lua_checkbox(l, index, CLASS_NAME);
    return (as_map *) mod_lua_box_value(box);
}

static int mod_lua_map_gc(lua_State * l) {
    mod_lua_freebox(l, 1, CLASS_NAME);
    return 0;
}



static int mod_lua_map_size(lua_State * l) {
    as_map *    map     = mod_lua_checkmap(l, 1);
    uint32_t    size    = as_map_size(map);
    lua_pushinteger(l, size);
    return 1;
}

static int mod_lua_map_new(lua_State * l) {
    as_map * map = (as_map *) as_hashmap_new(320);
    int n = lua_gettop(l);
    if ( n == 2 && lua_type(l, 2) == LUA_TTABLE) {
        lua_pushnil(l);
        while ( lua_next(l, 2) != 0 ) {
            // this will leak or crash if these are not as_val, or k is and v isn't
            as_val * k = mod_lua_takeval(l, -2);
            as_val * v = mod_lua_takeval(l, -1);
            if ( !k || !v ) {
                as_val_destroy(k);
                as_val_destroy(v);
            }
            else {
                as_map_set(map, k, v);
            }
            lua_pop(l, 1);
        }
    }
    mod_lua_pushmap(l, map);
    return 1;
}

static int mod_lua_map_index(lua_State * l) {
    mod_lua_box *   box     = mod_lua_checkbox(l, 1, CLASS_NAME);
    as_map *        map     = (as_map *) mod_lua_box_value(box);
    as_val *        val     = NULL;

    if ( map ) {
        as_val * key = mod_lua_takeval(l, 2);
        if ( key ) {
            val = as_map_get(map, key);
			as_val_destroy(key);
        }
    }

    if ( val ) {
        mod_lua_pushval(l, val);
    }
    else {
        lua_pushnil(l);
    }

    return 1;
}

static int mod_lua_map_newindex(lua_State * l) {
    as_map * map = mod_lua_checkmap(l, 1);
    if ( map ) {
        as_val * key = mod_lua_takeval(l, 2);
        as_val * val = mod_lua_takeval(l, 3); 
        if ( !key || !val ) {
            as_val_destroy(key);
            as_val_destroy(val);
        }
        else {
            as_map_set(map, key, val);
        }
    }
    return 0;
}

static int mod_lua_map_len(lua_State * l) {
    mod_lua_box *   box     = mod_lua_checkbox(l, 1, CLASS_NAME);
    as_map *        map     = (as_map *) mod_lua_box_value(box);
    if ( map ) {
        lua_pushinteger(l, as_map_size(map));
    }
    else {
        lua_pushinteger(l, 0);
    }
    return 1;
}

static int mod_lua_map_tostring(lua_State * l) {
    mod_lua_box *   box     = mod_lua_checkbox(l, 1, CLASS_NAME);
    as_val *        val     = mod_lua_box_value(box);
    char *          str     = NULL;

    if ( val ) {
        str = as_val_tostring(val);
    }

    if ( str ) {
        lua_pushstring(l, str);
        free(str);
    }
    else {
        lua_pushstring(l, "Map()");
    }

    return 1;
}

/**
 * Generator for map.pairs()
 */
static int mod_lua_map_pairs_next(lua_State * l) {
    as_iterator * iter  = mod_lua_toiterator(l, 1);
    if ( iter && as_iterator_has_next(iter) ) {
        as_pair * pair = (as_pair *) as_iterator_next(iter);
        if ( pair ) {
            mod_lua_pushval(l, pair->_1);
            mod_lua_pushval(l, pair->_2);
            return 2;
        }
    }
    return 0;
}

/**
 * USAGE:
 *      for k,v in map.pairs(m) do
 *      end
 * USAGE:
 *      for k,v in map.iterator(m) do
 *      end
 */
static int mod_lua_map_pairs(lua_State * l) {
    mod_lua_box *   box     = mod_lua_checkbox(l, 1, CLASS_NAME);
    as_map *        map     = (as_map *) mod_lua_box_value(box);
    if ( map ) {
        lua_pushcfunction(l, mod_lua_map_pairs_next);
        as_map_iterator * itr = (as_map_iterator *) mod_lua_pushiterator(l, sizeof(as_map_iterator));
        as_map_iterator_init(itr,map);
        return 2;
    }

    return 0;
}

/**
 * Generator for map.keys()
 */
static int mod_lua_map_keys_next(lua_State * l) {
    as_iterator * iter  = mod_lua_toiterator(l, 1);
    if ( iter && as_iterator_has_next(iter) ) {
        as_pair * pair = (as_pair *) as_iterator_next(iter);
        if ( pair ) {
            mod_lua_pushval(l, pair->_1);
            return 1;
        }
    }
    return 0;
}

/**
 * USAGE:
 *      for k in map.keys(m) do
 *      end
 */
static int mod_lua_map_keys(lua_State * l) {
    mod_lua_box *   box     = mod_lua_checkbox(l, 1, CLASS_NAME);
    as_map *        map     = (as_map *) mod_lua_box_value(box);
    if ( map ) {
        lua_pushcfunction(l, mod_lua_map_keys_next);
        as_map_iterator * iter = (as_map_iterator *) mod_lua_pushiterator(l, sizeof(as_map_iterator));
        as_map_iterator_init(iter, map);
        return 2;
    }

    return 0;
}

/**
 * Generator for map.values()
 */
static int mod_lua_map_values_next(lua_State * l) {
    as_iterator * iter  = mod_lua_toiterator(l, 1);
    if ( iter && as_iterator_has_next(iter) ) {
        as_pair * pair = (as_pair *) as_iterator_next(iter);
        if ( pair ) {
            mod_lua_pushval(l, pair->_2);
            return 1;
        }
    }
    return 0;
}

/**
 * USAGE:
 *      for v in map.values(m) do
 *      end
 */
static int mod_lua_map_values(lua_State * l) {
    mod_lua_box *   box     = mod_lua_checkbox(l, 1, CLASS_NAME);
    as_map *        map     = (as_map *) mod_lua_box_value(box);
    if ( map ) {
        lua_pushcfunction(l, mod_lua_map_values_next);
        as_map_iterator * itr = (as_map_iterator *) mod_lua_pushiterator(l, sizeof(as_map_iterator));
        as_map_iterator_init(itr, map);
        return 2;
    }

    return 0;
}

/******************************************************************************
 * OBJECT TABLE
 *****************************************************************************/

static const luaL_reg object_table[] = {
    {"iterator",        mod_lua_map_pairs},
    {"pairs",           mod_lua_map_pairs},
    {"keys",            mod_lua_map_keys},
    {"values",          mod_lua_map_values},
    {"size",            mod_lua_map_size},
    {"tostring",        mod_lua_map_tostring},
    {0, 0}
};

static const luaL_reg object_metatable[] = {
    {"__call",          mod_lua_map_new},
    {0, 0}
};

/******************************************************************************
 * CLASS TABLE
 *****************************************************************************/

static const luaL_reg class_table[] = {
    {0, 0}
};

static const luaL_reg class_metatable[] = {
    {"__index",         mod_lua_map_index},
    {"__newindex",      mod_lua_map_newindex},
    {"__len",           mod_lua_map_len},
    {"__tostring",      mod_lua_map_tostring},
    {"__gc",            mod_lua_map_gc},
    {0, 0}
};

/******************************************************************************
 * REGISTER
 *****************************************************************************/

int mod_lua_map_register(lua_State * l) {
    mod_lua_reg_object(l, OBJECT_NAME, object_table, object_metatable);
    mod_lua_reg_class(l, CLASS_NAME, NULL, class_metatable);
    return 1;
}
