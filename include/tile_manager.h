#pragma once

#include <GL/glew.h>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "osm_map.h"

struct TileTexture {
    GLuint id = 0;
    bool loaded = false;
    std::vector<unsigned char> png_data; 
};

class TileManager {
public:
    TileManager();
    ~TileManager();
    
    GLuint getTileTexture(const TileCoords& tile);
    void update();  
    
private:
    std::map<std::string, TileTexture> texture_cache;
    std::queue<TileCoords> download_queue;
    std::mutex queue_mutex;
    std::mutex cache_mutex;
    std::atomic<bool> running;
    std::thread worker_thread;
    std::condition_variable cv;
    
    void workerFunction();
    void downloadAndCreateTexture(const TileCoords& tile);
    bool loadFromCache(const TileCoords& tile, std::vector<unsigned char>& data);
    void saveToCache(const TileCoords& tile, const std::vector<unsigned char>& data);
    GLuint createTextureFromData(const std::vector<unsigned char>& data);
};

extern TileManager tile_manager;

size_t onPullResponse(void* data, size_t size, size_t nmemb, void* userp);