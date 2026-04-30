#pragma once
#include <vector>
#include <string>

using namespace std;

bool download_tile(int zoom, int x, int y, vector<unsigned char>& png_data);
string get_tile_url(int zoom, int x, int y);
string get_tile_cache_path(int zoom, int x, int y);