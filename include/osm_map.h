#pragma once
#include "shared.h"
#include <vector>
#include <mutex>

using namespace std;

struct TileCoords {
    int x;
    int y;
    int z;
};

struct TileCoordsFloat {
    double x, y;
};

struct OsmMapState {
    double center_lat = 55.0421;
    double center_lon = 82.9784;
    int    zoom       = 15;

    vector<LocationInfo> track_points;
    vector<TileCoords>   visible_tiles;

    float last_width = 0.0f;
    float last_height = 0.0f;

    mutex map_mutex;
};

class OsmUtils {
public:
    static TileCoords geoToTile(double lat, double lon, int zoom);
    static TileCoordsFloat geoToTileFloat(double lat, double lon, int zoom);
    static string getTilePath(const TileCoords& tile);
    static string getTileUrl(const TileCoords& tile);
    static double tilex2long(int x, int z);
    static double tiley2lat(int y, int z);
};

extern OsmMapState osm_map;

void init_osm_map();
void update_visible_tiles();
void render_osm_map_implot();