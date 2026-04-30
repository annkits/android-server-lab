#include "osm_map.h"
#include "imgui.h"
#include "implot.h"
#include "tile_manager.h"
#include "curl_utils.h"

#include <iostream>
#include <cmath>
#include <algorithm>

using namespace std;

OsmMapState osm_map;
TileManager tile_manager;

void init_osm_map() {
    osm_map.zoom = 15;
    osm_map.center_lat = 55.0421;
    osm_map.center_lon = 82.9784;
    osm_map.track_points.clear();
    osm_map.visible_tiles.clear();
    osm_map.last_width = 0;
    osm_map.last_height = 0;
    
    cout << "OSM карта инициализирована (Новосибирск: 55.0421, 82.9784)\n";
}

void update_visible_tiles() {
    lock_guard<mutex> lock(osm_map.map_mutex);
    osm_map.visible_tiles.clear();
    
    TileCoords center = OsmUtils::geoToTile(osm_map.center_lat, osm_map.center_lon, osm_map.zoom);
    
    int tiles_x = max(2, min(10, (int)ceil(osm_map.last_width / 256.0f) + 2));
    int tiles_y = max(2, min(10, (int)ceil(osm_map.last_height / 256.0f) + 2));
    
    cout << "[OSM] Обновление тайлов: центр=(" << center.x << "," << center.y << "), зум=" << osm_map.zoom << ", сетка=" << tiles_x << "x" << tiles_y << "\n";
    
    for (int dy = -tiles_y/2; dy <= tiles_y/2; ++dy) {
        for (int dx = -tiles_x/2; dx <= tiles_x/2; ++dx) {
            TileCoords t;
            t.x = center.x + dx;
            t.y = center.y + dy;
            t.z = osm_map.zoom;
            osm_map.visible_tiles.push_back(t);
        }
    }
    
    if (osm_map.zoom < 19) {
        TileCoords center_next = OsmUtils::geoToTile(osm_map.center_lat, osm_map.center_lon, osm_map.zoom + 1);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                TileCoords t;
                t.x = center_next.x + dx;
                t.y = center_next.y + dy;
                t.z = osm_map.zoom + 1;
                tile_manager.getTileTexture(t);
            }
        }
    }
}

void preload_adjacent_tiles() {
    lock_guard<mutex> lock(osm_map.map_mutex);
    
    if (osm_map.visible_tiles.empty()) return;
    
    TileCoords center_tile = OsmUtils::geoToTile(osm_map.center_lat, osm_map.center_lon, osm_map.zoom);
    
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            bool is_visible = false;
            for (const auto& vt : osm_map.visible_tiles) {
                if (vt.x == center_tile.x + dx && vt.y == center_tile.y + dy && vt.z == osm_map.zoom) {
                    is_visible = true;
                    break;
                }
            }
            
            if (!is_visible) {
                TileCoords t;
                t.x = center_tile.x + dx;
                t.y = center_tile.y + dy;
                t.z = osm_map.zoom;
                tile_manager.getTileTexture(t);
            }
        }
    }
}

void render_osm_map_implot() {
    tile_manager.update();
    
    ImGui::Begin("OSM Map - ImPlot View");
    
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 100) avail.x = 400;
    if (avail.y < 100) avail.y = 400;
    
    if (avail.x != osm_map.last_width || avail.y != osm_map.last_height) {
        osm_map.last_width = avail.x;
        osm_map.last_height = avail.y;
        update_visible_tiles();
    }
    
    static double last_min_lon = 0, last_max_lon = 0;
    static double last_min_lat = 0, last_max_lat = 0;
    static bool first_frame = true;
    
    if (ImPlot::BeginPlot("##OSM_Map", avail, 
        ImPlotFlags_NoTitle | ImPlotFlags_NoMenus)) {
        
        ImPlot::SetupAxes("Долгота", "Широта", 
                         ImPlotAxisFlags_None, ImPlotAxisFlags_None);
        
        if (first_frame) {
            double range_lon = 360.0 / pow(2.0, osm_map.zoom) * 3.0;
            double aspect_ratio = avail.x / avail.y;
            double range_lat = range_lon / aspect_ratio;
            
            last_min_lon = osm_map.center_lon - range_lon/2;
            last_max_lon = osm_map.center_lon + range_lon/2;
            last_min_lat = osm_map.center_lat - range_lat/2;
            last_max_lat = osm_map.center_lat + range_lat/2;
            
            ImPlot::SetupAxisLimits(ImAxis_X1, last_min_lon, last_max_lon);
            ImPlot::SetupAxisLimits(ImAxis_Y1, last_min_lat, last_max_lat);
            first_frame = false;
        }
        
        ImPlotRect limits = ImPlot::GetPlotLimits();
        double current_min_lon = limits.X.Min;
        double current_max_lon = limits.X.Max;
        double current_min_lat = limits.Y.Min;
        double current_max_lat = limits.Y.Max;
        
        bool view_changed = 
            abs(current_min_lon - last_min_lon) > 0.0001 || 
            abs(current_max_lon - last_max_lon) > 0.0001 ||
            abs(current_min_lat - last_min_lat) > 0.0001 || 
            abs(current_max_lat - last_max_lat) > 0.0001;
        
        if (view_changed) {
            osm_map.center_lon = (current_min_lon + current_max_lon) / 2.0;
            osm_map.center_lat = (current_min_lat + current_max_lat) / 2.0;

            double range_lon = current_max_lon - current_min_lon;
            
            int new_zoom = static_cast<int>(round(log2(720.0 / range_lon)));
            
            new_zoom = max(10, min(19, new_zoom));
            
            if (new_zoom != osm_map.zoom) {
                osm_map.zoom = new_zoom;
                cout << "[OSM] Зум: " << osm_map.zoom << " (диапазон: " << range_lon << "°)\n";
            }
            
            update_visible_tiles();
            
            last_min_lon = current_min_lon;
            last_max_lon = current_max_lon;
            last_min_lat = current_min_lat;
            last_max_lat = current_max_lat;
        }
        
        {
            lock_guard<mutex> lock(osm_map.map_mutex);
            
            for (const auto& tile_coord : osm_map.visible_tiles) {
                GLuint tex = tile_manager.getTileTexture(tile_coord);
                
                if (tex != 0) {
                    double min_lon = OsmUtils::tilex2long(tile_coord.x, tile_coord.z);
                    double max_lon = OsmUtils::tilex2long(tile_coord.x + 1, tile_coord.z);
                    double max_lat = OsmUtils::tiley2lat(tile_coord.y, tile_coord.z);
                    double min_lat = OsmUtils::tiley2lat(tile_coord.y + 1, tile_coord.z);
                    
                    ImPlot::PlotImage(("##tile_" + to_string(tile_coord.x) + "_" + to_string(tile_coord.y)).c_str(),
                                     (ImTextureID)(intptr_t)tex,
                                     ImPlotPoint(min_lon, min_lat),
                                     ImPlotPoint(max_lon, max_lat));
                }
            }
        }
        
        ImPlot::EndPlot();
    }
    
    ImGui::Text("Центр: %.4f, %.4f | Zoom: %d | Тайлов: %zu", 
                osm_map.center_lat, osm_map.center_lon, osm_map.zoom, osm_map.visible_tiles.size());
    
    if (ImGui::Button("Сбросить вид")) {
        osm_map.center_lat = 55.0421;
        osm_map.center_lon = 82.9784;
        osm_map.zoom = 15;
        first_frame = true;
        update_visible_tiles();
    }
    
    ImGui::End();
}