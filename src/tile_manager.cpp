#include "tile_manager.h"
#include <curl/curl.h>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = filesystem;

size_t onPullResponse(void* data, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    auto& blob = *static_cast<vector<unsigned char>*>(userp);
    auto const* const dataptr = static_cast<unsigned char*>(data);
    blob.insert(blob.cend(), dataptr, dataptr + realsize);
    return realsize;
}

TileManager::TileManager() : running(true) {
    worker_thread = thread(&TileManager::workerFunction, this);
}

TileManager::~TileManager() {
    running = false;
    cv.notify_one();
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    
    for (auto& [path, tex] : texture_cache) {
        if (tex.id != 0) {
            glDeleteTextures(1, &tex.id);
        }
    }
}

GLuint TileManager::getTileTexture(const TileCoords& tile) {
    string path = OsmUtils::getTilePath(tile);
    
    {
        lock_guard<mutex> lock(cache_mutex);
        auto it = texture_cache.find(path);
        if (it != texture_cache.end() && it->second.loaded) {
            return it->second.id;
        }
    }
    
    bool already_in_queue = false;
    {
        lock_guard<mutex> lock(queue_mutex);
        queue<TileCoords> temp = download_queue;
        while (!temp.empty()) {
            TileCoords q = temp.front();
            temp.pop();
            if (q.x == tile.x && q.y == tile.y && q.z == tile.z) {
                already_in_queue = true;
                break;
            }
        }
        
        if (!already_in_queue) {
            {
                lock_guard<mutex> lock2(cache_mutex);
                if (texture_cache.find(path) == texture_cache.end()) {
                    TileTexture new_tex;
                    new_tex.loaded = false;
                    texture_cache[path] = new_tex;
                    download_queue.push(tile);
                    cv.notify_one();
                }
            }
        }
    }
    
    return 0;
}

void TileManager::update() {
    lock_guard<mutex> lock(cache_mutex);
    
    for (auto& [path, tex] : texture_cache) {
        if (!tex.loaded && !tex.png_data.empty()) {
            tex.id = createTextureFromData(tex.png_data);
            tex.png_data.clear();
            tex.loaded = true;
        }
    }
}

void TileManager::workerFunction() {
    while (running) {
        TileCoords tile;
        bool has_tile = false;
        
        {
            unique_lock<mutex> lock(queue_mutex);
            if (download_queue.empty()) {
                cv.wait_for(lock, chrono::milliseconds(100));
            }
            if (!download_queue.empty()) {
                tile = download_queue.front();
                download_queue.pop();
                has_tile = true;
            }
        }
        
        if (has_tile) {
            downloadAndCreateTexture(tile);
        }
    }
}

void TileManager::downloadAndCreateTexture(const TileCoords& tile) {
    vector<unsigned char> png_data;
    string path = OsmUtils::getTilePath(tile);
    
    if (!loadFromCache(tile, png_data)) {
        string url = OsmUtils::getTileUrl(tile);
        
        CURL* curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl");
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &png_data);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onPullResponse);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
            
            if (curl_easy_perform(curl) == CURLE_OK && !png_data.empty()) {
                saveToCache(tile, png_data);
                cout << "[TileManager] Загружен: " << tile.x << "," << tile.y << "," << tile.z << "\n";
            } else {
                cerr << "[TileManager] Ошибка загрузки: " << tile.x << "," << tile.y << "\n";
            }
            curl_easy_cleanup(curl);
        }
    } else {
        cout << "[TileManager] Из кэша: " << tile.x << "," << tile.y << "," << tile.z << "\n";
    }
    
    if (!png_data.empty()) {
        string path = OsmUtils::getTilePath(tile);
        lock_guard<mutex> lock(cache_mutex);
        auto it = texture_cache.find(path);
        if (it != texture_cache.end()) {
            it->second.png_data = move(png_data);
        }
    }
}

bool TileManager::loadFromCache(const TileCoords& tile, vector<unsigned char>& data) {
    string path = OsmUtils::getTilePath(tile);
    
    if (!fs::exists(path)) {
        return false;
    }
    
    ifstream file(path, ios::binary | ios::ate);
    if (!file.is_open()) {
        return false;
    }
    
    size_t size = file.tellg();
    data.resize(size);
    file.seekg(0, ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return true;
}

void TileManager::saveToCache(const TileCoords& tile, const vector<unsigned char>& data) {
    string path = OsmUtils::getTilePath(tile);
    fs::create_directories(fs::path(path).parent_path());
    
    ofstream file(path, ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
}

GLuint TileManager::createTextureFromData(const vector<unsigned char>& data) {
    if (data.empty()) return 0;
    
    int width, height, channels;
    unsigned char* image = stbi_load_from_memory(data.data(), data.size(), &width, &height, &channels, 4);
    
    if (!image) {
        cerr << "[TileManager] Ошибка декодирования PNG\n";
        return 0;
    }
    
    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    
    stbi_image_free(image);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture_id;
}