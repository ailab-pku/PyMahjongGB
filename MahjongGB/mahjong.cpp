#include <Python.h>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include "mahjong-algorithm/fan_calculator.h"
#include "mahjong-algorithm/shanten.h"

using namespace std;

static unordered_map<string, mahjong::tile_t> str2tile;
static const char* tile2str[34] = {
"W1","W2","W3","W4","W5","W6","W7","W8","W9",
"T1","T2","T3","T4","T5","T6","T7","T8","T9",
"B1","B2","B3","B4","B5","B6","B7","B8","B9",
"F1","F2","F3","F4","J1","J2","J3"
};

static void MahjongInit() {
	for(int i = 1; i <= 9; ++i) {
		str2tile["W" + to_string(i)] = mahjong::make_tile(TILE_SUIT_CHARACTERS, i);
		str2tile["B" + to_string(i)] = mahjong::make_tile(TILE_SUIT_DOTS, i);
		str2tile["T" + to_string(i)] = mahjong::make_tile(TILE_SUIT_BAMBOO, i);
	}
	for(int i = 1; i <= 4; ++i)
		str2tile["F" + to_string(i)] = mahjong::make_tile(TILE_SUIT_HONORS, i);
	for(int i = 1; i <= 3; ++i)
		str2tile["J" + to_string(i)] = mahjong::make_tile(TILE_SUIT_HONORS, i + 4);
}

static const char *doc_calculator = "Calculate Mahjong Fans.\n"
"Parameters:\n"
"\tpack - A tuple of fixed packs, each of which is a tuple of form (\"CHI\"/\"PENG\"/\"GANG\", tile, offer:0..3);\n"
"\thand - A tuple of standing tiles;\n"
"\twinTile - Winning Tile;\n"
"\tflowerCount - Number of flower tiles;\n"
"\tisSelfDrawn - bool indicate self drawn;\n"
"\tis4thTile - bool indicate 4th tile;\n"
"\tisAboutKong - bool indicate about kong;\n"
"\tisWallLast - bool indicate wall last;\n"
"\tseatWind - seat wind of 0..3 indicate east/south/west/north;\n"
"\tprevalentWind - prevalent wind of 0..3 indicate east/south/west/north.\n"
"\tverbose - (Optional) bool control return format, default to be False.\n"
"Returns:\n"
"\tA tuple of fans, each of which is a tuple of form (fan_count, fan_name).\n"
"\tIf verbose is set to be True, form (fan_point, cnt, fan_name, fan_name_en) is used instead.\n"
"Raises:"
"\tTypeError - If any invalid input is encountered.\n";

static const char *doc_shanten = "Calculate Mahjong Shanten.\n"
"Parameters:\n"
"\tpack - A tuple of fixed packs, each of which is a tuple of form (\"CHI\"/\"PENG\"/\"GANG\", tile, offer:0..3);\n"
"\thand - A tuple of standing tiles;\n"
"Returns:\n"
"\tAn integer of shanten.\n"
"Raises:"
"\tTypeError - If any invalid input is encountered.\n";

#define SHANTEN_DOC_TEMPLATE(name, func) \
static const char *doc_##func = "Calculate Mahjong Shanten For " #name ".\n" \
"Parameters:\n" \
"\thand - A tuple of standing tiles;\n" \
"Returns:\n" \
"\tA tuple of (shanten, useful), where shanten is an integer, useful is a tuple of useful tiles.\n" \
"Raises:" \
"\tTypeError - If any invalid input is encountered.\n";

SHANTEN_DOC_TEMPLATE(Thirteen Orphans, thirteen_orphans_shanten)
SHANTEN_DOC_TEMPLATE(Seven Pairs, seven_pairs_shanten)
SHANTEN_DOC_TEMPLATE(Honors And Knitted Tiles, honors_and_knitted_tiles_shanten)
SHANTEN_DOC_TEMPLATE(Knitted Straight, knitted_straight_shanten)
SHANTEN_DOC_TEMPLATE(Regular Form, basic_form_shanten)

static PyObject *MahjongFanCalculator(PyObject *self, PyObject *args, PyObject *kwargs) {
	try {
		// Parse arguments
		static char *kwlist[] = {"pack", "hand", "winTile", "flowerCount"
			, "isSelfDrawn", "is4thTile", "isAboutKong", "isWallLast"
			, "seatWind", "prevalentWind", "verbose", nullptr};
		PyObject *packs = nullptr, *hands = nullptr;
		const char *winTile = nullptr;
		int flowerCount, isSelfDrawn, is4thTile, isAboutKong, isWallLast, seatWind, prevalentWind, verbose = 0;
		if(!PyArg_ParseTupleAndKeywords(args, kwargs, "OOsippppii|p", kwlist
			, &packs, &hands, &winTile, &flowerCount
			, &isSelfDrawn, &is4thTile, &isAboutKong, &isWallLast, &seatWind, &prevalentWind, &verbose))
			return nullptr;
		// Prepare params
		mahjong::calculate_param_t calculate_param = {};
		// Parse pack tuple
		if(!PyTuple_Check(packs)) throw "Param `pack` must be a tuple!";
		int packSize = PyTuple_Size(packs);
		calculate_param.hand_tiles.pack_count = packSize;
		for(int i = 0; i < packSize; ++i) {
			// Parse pack info
			PyObject *pack = PyTuple_GET_ITEM(packs, i);
			if(!PyTuple_Check(pack)) throw "Param `pack` must be a tuple of tuples!";
			const char *type = nullptr, *tile = nullptr;
			int offer, packCode;
			if(!PyArg_ParseTuple(pack, "ssi", &type, &tile, &offer)) return nullptr;
			if(str2tile.find(tile) == str2tile.end()) throw "ERROE_WRONG_TILE_CODE";
			if(offer < 0 || offer >= 4) throw "ERROE_WRONG_OFFER_CODE";
			if(!strcmp(type, "PENG")) packCode = PACK_TYPE_PUNG;
			else if(!strcmp(type, "GANG")) packCode = PACK_TYPE_KONG;
			else if(!strcmp(type, "CHI")) packCode = PACK_TYPE_CHOW;
			else throw "ERROE_WRONG_PACK_CODE";
			calculate_param.hand_tiles.fixed_packs[i] = mahjong::make_pack(offer, packCode, str2tile[tile]);
		}
		// Parse hand tuple
		if(!PyTuple_Check(hands)) throw "Param `hand` must be a tuple!";
		int handSize = PyTuple_Size(hands);
		calculate_param.hand_tiles.tile_count = handSize;
		for(int i = 0; i < handSize; ++i) {
			// Parse hand tile
			PyObject *hand = PyTuple_GET_ITEM(hands, i);
			if(!PyUnicode_Check(hand)) throw "Param `hand` must be a tuple of strs!";
			const char *tile = PyUnicode_AsUTF8(hand);
			if(str2tile.find(tile) == str2tile.end()) throw "ERROE_WRONG_TILE_CODE";
			calculate_param.hand_tiles.standing_tiles[i] = str2tile[tile];
		}
		// Other params
		if(str2tile.find(winTile) == str2tile.end()) throw "ERROE_WRONG_TILE_CODE";
		calculate_param.win_tile = str2tile[winTile];
		calculate_param.flower_count = flowerCount;
		calculate_param.win_flag = 
			isSelfDrawn * WIN_FLAG_SELF_DRAWN +
			is4thTile * WIN_FLAG_4TH_TILE +
			isAboutKong * WIN_FLAG_ABOUT_KONG +
			isWallLast * WIN_FLAG_WALL_LAST;
		calculate_param.seat_wind = (mahjong::wind_t)seatWind;
		calculate_param.prevalent_wind = (mahjong::wind_t)prevalentWind;
		// Prepare results
		mahjong::fan_table_t fan_table = {};
		int re = mahjong::calculate_fan(&calculate_param, &fan_table);
		switch(re) {
			case -1: throw "ERROR_WRONG_TILES_COUNT";
			case -2: throw "ERROR_TILE_COUNT_GREATER_THAN_4";
			case -3: throw "ERROR_NOT_WIN";
		}
		int l = 0;
		for(int i = 0; i < mahjong::FAN_TABLE_SIZE; i++)
			if(fan_table[i]) ++l;
		PyObject *ans = PyTuple_New(l);
		l = 0;
		for(int i = 0; i < mahjong::FAN_TABLE_SIZE; i++)
			if(fan_table[i]) {
				PyObject *item;
				if(!verbose) item = Py_BuildValue("is", mahjong::fan_value_table[i] * fan_table[i], mahjong::fan_name[i]);
				else item = Py_BuildValue("iiss", mahjong::fan_value_table[i], fan_table[i], mahjong::fan_name[i], mahjong::fan_name_en[i]);
				PyTuple_SetItem(ans, l++, item);
			}
		return ans;
	} catch (const char *msg) {
		PyErr_SetString(PyExc_TypeError, msg);
		return nullptr;
	}
}

static PyObject *MahjongShanten(PyObject *self, PyObject *args, PyObject *kwargs) {
	try {
		// Parse arguments
		static char *kwlist[] = {"pack", "hand", nullptr};
		PyObject *packs = nullptr, *hands = nullptr;
		if(!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kwlist, &packs, &hands))
			return nullptr;
		// Parse pack tuple
		if(!PyTuple_Check(packs)) throw "Param `pack` must be a tuple!";
		int packSize = PyTuple_Size(packs);
		mahjong::hand_tiles_t hand_tiles;
		hand_tiles.pack_count = packSize;
		for(int i = 0; i < packSize; ++i) {
			// Parse pack info
			PyObject *pack = PyTuple_GET_ITEM(packs, i);
			if(!PyTuple_Check(pack)) throw "Param `pack` must be a tuple of tuples!";
			const char *type = nullptr, *tile = nullptr;
			int offer, packCode;
			if(!PyArg_ParseTuple(pack, "ssi", &type, &tile, &offer)) return nullptr;
			if(str2tile.find(tile) == str2tile.end()) throw "ERROE_WRONG_TILE_CODE";
			if(offer < 0 || offer >= 4) throw "ERROE_WRONG_OFFER_CODE";
			if(!strcmp(type, "PENG")) packCode = PACK_TYPE_PUNG;
			else if(!strcmp(type, "GANG")) packCode = PACK_TYPE_KONG;
			else if(!strcmp(type, "CHI")) packCode = PACK_TYPE_CHOW;
			else throw "ERROE_WRONG_PACK_CODE";
			hand_tiles.fixed_packs[i] = mahjong::make_pack(offer, packCode, str2tile[tile]);
		}
		// Parse hand tuple
		if(!PyTuple_Check(hands)) throw "Param `hand` must be a tuple!";
		int handSize = PyTuple_Size(hands);
		hand_tiles.tile_count = handSize;
		for(int i = 0; i < handSize; ++i) {
			// Parse hand tile
			PyObject *hand = PyTuple_GET_ITEM(hands, i);
			if(!PyUnicode_Check(hand)) throw "Param `hand` must be a tuple of strs!";
			const char *tile = PyUnicode_AsUTF8(hand);
			if(str2tile.find(tile) == str2tile.end()) throw "ERROE_WRONG_TILE_CODE";
			hand_tiles.standing_tiles[i] = str2tile[tile];
		}
		int re = numeric_limits<int>::max();
		re = min(re, mahjong::thirteen_orphans_shanten(hand_tiles.standing_tiles, hand_tiles.tile_count, nullptr));
		re = min(re, mahjong::seven_pairs_shanten(hand_tiles.standing_tiles, hand_tiles.tile_count, nullptr));
		re = min(re, mahjong::honors_and_knitted_tiles_shanten(hand_tiles.standing_tiles, hand_tiles.tile_count, nullptr));
		re = min(re, mahjong::knitted_straight_shanten(hand_tiles.standing_tiles, hand_tiles.tile_count, nullptr));
		re = min(re, mahjong::basic_form_shanten(hand_tiles.standing_tiles, hand_tiles.tile_count, nullptr));
		if (re == numeric_limits<int>::max()) throw "ERROR_INVALID_HAND_OR_PACK";
		PyObject *ans = Py_BuildValue("i", re);
		return ans;
	} catch (const char *msg) {
		PyErr_SetString(PyExc_TypeError, msg);
		return nullptr;
	}
}

#define SHANTEN_TEMPLATE(name, func) \
static PyObject *name(PyObject *self, PyObject *args, PyObject *kwargs) { \
	try { \
		/* Parse arguments */ \
		static char *kwlist[] = {"hand", nullptr}; \
		PyObject *hands = nullptr; \
		if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &hands)) \
			return nullptr; \
		mahjong::hand_tiles_t hand_tiles; \
		mahjong::useful_table_t useful_table; \
		/* Parse hand tuple */ \
		if(!PyTuple_Check(hands)) throw "Param `hand` must be a tuple!"; \
		int handSize = PyTuple_Size(hands); \
		hand_tiles.tile_count = handSize; \
		for(int i = 0; i < handSize; ++i) { \
			/* Parse hand tile */ \
			PyObject *hand = PyTuple_GET_ITEM(hands, i); \
			if(!PyUnicode_Check(hand)) throw "Param `hand` must be a tuple of strs!"; \
			const char *tile = PyUnicode_AsUTF8(hand); \
			if(str2tile.find(tile) == str2tile.end()) throw "ERROE_WRONG_TILE_CODE"; \
			hand_tiles.standing_tiles[i] = str2tile[tile]; \
		} \
		int re = mahjong::##func(hand_tiles.standing_tiles, hand_tiles.tile_count, &useful_table); \
		if (re == numeric_limits<int>::max()) throw "ERROR_INVALID_HAND"; \
		int usefulSize = 0, l = 0; \
		for(int i = 0; i < 34; ++i) \
			if(useful_table[mahjong::all_tiles[i]]) \
				++usefulSize; \
		PyObject *useful = PyTuple_New(usefulSize); \
		for(int i = 0; i < 34; ++i) \
			if(useful_table[mahjong::all_tiles[i]]) \
				PyTuple_SetItem(useful, l++, Py_BuildValue("s", tile2str[i])); \
		PyObject *ans = Py_BuildValue("(iO)", re, useful); \
		return ans; \
	} catch (const char *msg) { \
		PyErr_SetString(PyExc_TypeError, msg); \
		return nullptr; \
	} \
}

SHANTEN_TEMPLATE(ThirteenOrphansShanten, thirteen_orphans_shanten)
SHANTEN_TEMPLATE(SevenPairsShanten, seven_pairs_shanten)
SHANTEN_TEMPLATE(HonorsAndKnittedTilesShanten, honors_and_knitted_tiles_shanten)
SHANTEN_TEMPLATE(KnittedStraightShanten, knitted_straight_shanten)
SHANTEN_TEMPLATE(RegularShanten, basic_form_shanten)

#define SHANTEN_METHOD_TEMPLATE(name, func) \
{#name, (PyCFunction)(void(*)(void))name, METH_VARARGS | METH_KEYWORDS, doc_##func},


static PyMethodDef methods[] = {
	{"MahjongFanCalculator", (PyCFunction)(void(*)(void))MahjongFanCalculator, METH_VARARGS | METH_KEYWORDS, doc_calculator},
	{"MahjongShanten", (PyCFunction)(void(*)(void))MahjongShanten, METH_VARARGS | METH_KEYWORDS, doc_shanten},
	SHANTEN_METHOD_TEMPLATE(ThirteenOrphansShanten, thirteen_orphans_shanten)
	SHANTEN_METHOD_TEMPLATE(SevenPairsShanten, seven_pairs_shanten)
	SHANTEN_METHOD_TEMPLATE(HonorsAndKnittedTilesShanten, honors_and_knitted_tiles_shanten)
	SHANTEN_METHOD_TEMPLATE(KnittedStraightShanten, knitted_straight_shanten)
	SHANTEN_METHOD_TEMPLATE(RegularShanten, basic_form_shanten)
	{NULL, NULL, 0, NULL},
};

static PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "MahjongGB",
    "Library to calculate fans and shanten in Chinese Standard Mahjong.",
    -1,
    methods,
};
PyMODINIT_FUNC
PyInit_MahjongGB(void) {
    MahjongInit();
    return PyModule_Create(&module);
}