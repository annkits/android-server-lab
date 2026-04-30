#include "curl_utils.h"
#include <curl/curl.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <math.h>

namespace fs = filesystem;
using namespace std;

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    vector<unsigned char>* data = static_cast<vector<unsigned char>*>(userp);
    size_t total_size = size * nmemb;
    data->insert(data->end(), static_cast<unsigned char*>(contents), 
                 static_cast<unsigned char*>(contents) + total_size);
    return total_size;
}

string get_tile_url(int zoom, int x, int y) {
    return "https://a.tile.openstreetmap.org/" 
           + to_string(zoom) + "/" 
           + to_string(x) + "/" 
           + to_string(y) + ".png";
}

string get_tile_cache_path(int zoom, int x, int y) {
    return "build/" + to_string(zoom) + "/" 
           + to_string(x) + "/" 
           + to_string(y) + ".png";
}

bool download_tile(int zoom, int x, int y, vector<unsigned char>& png_data) {
    png_data.clear();

    string url = get_tile_url(zoom, x, y);

    string cache_path = get_tile_cache_path(zoom, x, y);

    if (fs::exists(cache_path)) {
        ifstream file(cache_path, ios::binary | ios::ate);

        if (file.is_open()) {
            size_t size = file.tellg();
            png_data.resize(size);
            file.seekg(0, ios::beg);
            file.read(reinterpret_cast<char*>(png_data.data()), size);
            return true;
        }
    }

    static CURL* shared_curl = nullptr;
    static mutex curl_init_mutex;

    {
        lock_guard<mutex> lock(curl_init_mutex);
        if (!shared_curl) {
            shared_curl = curl_easy_init();
            if (shared_curl) {
                curl_easy_setopt(shared_curl, CURLOPT_USERAGENT, "MyOSMViewer/1.0");
                curl_easy_setopt(shared_curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(shared_curl, CURLOPT_ACCEPT_ENCODING, "gzip");
            }
        }
    }

    if (!shared_curl) return false;
    CURL* curl = curl_easy_duphandle(shared_curl);
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &png_data);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "[OSM] CURL error: " << curl_easy_strerror(res) << "\n";
        return false;
    }

    if (png_data.empty()) return false;

    fs::create_directories(fs::path(cache_path).parent_path());

    ofstream file(cache_path, ios::binary);

    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(png_data.data()),
            png_data.size()
        );
    }

    return true;
}