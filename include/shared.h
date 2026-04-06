#ifndef SHARED_H
#define SHARED_H

#include <string>
#include <mutex>        
#include <atomic>
#include <vector>

using namespace std;

struct LocationInfo {
    float latitude;
    float longitude;
    float altitude;
    float accuracy;
    string time;
};

struct CellInfo {
    string type;
    bool registered;

    int dbm;
    int level;

    int rsrp;
    int rsrq;
    int sinr;

    int timing_advance;

    int64_t ci;
    int pci;
    int tac;
    string mcc;
    string mnc;
    string operator_name;
};

struct PacketData {
    int id;
    string timestamp;

    LocationInfo location;
    vector<CellInfo> cells;
};

struct CellHistory {
    vector<float> times;          
    vector<float> rsrp;
    vector<float> dbm;          
    vector<float> sinr;            
    int pci = -1;
    string label;
};

struct SignalPlotData {
    vector<CellHistory> cells;
    size_t max_points = 300;        
    float elapsed_time = 0.0f;
};

extern SignalPlotData rsrp_history;

extern PacketData latest_packet;
extern vector<PacketData> history;

extern mutex history_mutex;
extern mutex latest_packet_mutex;

extern atomic<bool> global_running;

#endif