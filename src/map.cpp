#include "osm_map.h"
#include <cmath>
#include <sstream>

using namespace std;

TileCoords OsmUtils::geoToTile(double lat, double lon, int zoom) {
    double n = pow(2.0, zoom);
    int x = static_cast<int>((lon + 180.0) / 360.0 * n);
    
    double lat_rad = lat * M_PI / 180.0;
    int y = static_cast<int>((1.0 - log(tan(lat_rad) + (1.0 / cos(lat_rad))) / M_PI) / 2.0 * n);
    
    return {x, y, zoom};
}

TileCoordsFloat OsmUtils::geoToTileFloat(double lat, double lon, int zoom) {
    double n = pow(2.0, zoom);
    double x = (lon + 180.0) / 360.0 * n;
    
    double lat_rad = lat * M_PI / 180.0;
    double y = (1.0 - log(tan(lat_rad) + (1.0 / cos(lat_rad))) / M_PI) / 2.0 * n;
    
    return {x, y};
}

string OsmUtils::getTilePath(const TileCoords& tile) {
    ostringstream ss;
    ss << "build/" << tile.z << "/" << tile.x << "/" << tile.y << ".png";
    return ss.str();
}

string OsmUtils::getTileUrl(const TileCoords& tile) {
    ostringstream ss;
    ss << "https://a.tile.openstreetmap.org/" 
       << tile.z << "/" << tile.x << "/" << tile.y << ".png";
    return ss.str();
}

double OsmUtils::tilex2long(int x, int z) {
    return x / pow(2.0, z) * 360.0 - 180.0;
}

double OsmUtils::tiley2lat(int y, int z) {
    double n = M_PI - 2.0 * M_PI * y / pow(2.0, z);
    return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}