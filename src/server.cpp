#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <format>
#include <csignal>

#include "shared.h"
#include "osm_map.h"
#include <zmq.hpp>
#include <json.hpp>
#include <libpq-fe.h>

using json = nlohmann::json;
using namespace std;

PGconn* db_conn = nullptr;

bool init_database() {
    const char* conninfo = "dbname=telecom_data user=postgres password=telecom_pass host=localhost port=5432";
    
    db_conn = PQconnectdb(conninfo);
    
    if (PQstatus(db_conn) != CONNECTION_OK) {
        cerr << "Ошибка подключения к PostgreSQL: " << PQerrorMessage(db_conn) << endl;
        PQfinish(db_conn);
        db_conn = nullptr;
        return false;
    }
    
    cout << "Подключено к базе данных PostgreSQL\n";
    
    const char* create_table = R"(
    CREATE TABLE IF NOT EXISTS cell_measurements (
        id SERIAL PRIMARY KEY,
        packet_id INTEGER NOT NULL,
        timestamp TIMESTAMP NOT NULL,
        
        latitude DOUBLE PRECISION,
        longitude DOUBLE PRECISION,
        altitude DOUBLE PRECISION,
        accuracy DOUBLE PRECISION,
        
        cell_type VARCHAR(10) NOT NULL,
        registered BOOLEAN,
        
        dbm INTEGER,
        level INTEGER,
        
        rsrp INTEGER,
        rsrq INTEGER,
        sinr INTEGER,
        
        timing_advance INTEGER,
        
        pci INTEGER,
        ci BIGINT,
        tac INTEGER,
        mcc VARCHAR(10),
        mnc VARCHAR(10),
        operator_name VARCHAR(100),
        
        earfcn INTEGER,
        nrarfcn INTEGER,
        
        CONSTRAINT unique_measurement UNIQUE (packet_id, ci, timestamp)
    );
    
    CREATE INDEX IF NOT EXISTS idx_packet_id ON cell_measurements(packet_id);
    CREATE INDEX IF NOT EXISTS idx_timestamp ON cell_measurements(timestamp);
    CREATE INDEX IF NOT EXISTS idx_pci ON cell_measurements(pci);
    )";
    
    PGresult* res = PQexec(db_conn, create_table);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        cerr << "Ошибка создания таблицы: " << PQerrorMessage(db_conn) << endl;
        PQclear(res);
        return false;
    }
    PQclear(res);
    
    cout << "Таблица cell_measurements готова (поддерживает LTE, GSM, NR)\n";
    return true;
}

bool insert_cell_data(int packet_id, const string& timestamp_str, const LocationInfo& loc, const CellInfo& cell) {
    if (!db_conn) return false;

    const char* query = R"(
        INSERT INTO cell_measurements 
        (packet_id, timestamp, latitude, longitude, altitude, accuracy,
         cell_type, registered, dbm, level, rsrp, rsrq, sinr, timing_advance,
         pci, ci, tac, mcc, mnc, operator_name, earfcn, nrarfcn)
        VALUES ($1, to_timestamp($2::bigint / 1000.0), $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22)
        ON CONFLICT (packet_id, ci, timestamp) 
        DO NOTHING;  
    )";

    char packet_id_str[32];
    snprintf(packet_id_str, sizeof(packet_id_str), "%d", packet_id);

    string ts_ms_str;
    bool is_milliseconds = true;

    if (timestamp_str.find('-') != string::npos) {
        auto now = chrono::system_clock::now();
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
        ts_ms_str = to_string(ms);
    } else {
        ts_ms_str = timestamp_str;
    }

    const char* paramValues[22] = {
        packet_id_str,
        ts_ms_str.c_str(),
        to_string(loc.latitude).c_str(),
        to_string(loc.longitude).c_str(),
        to_string(loc.altitude).c_str(),
        to_string(loc.accuracy).c_str(),
        cell.type.c_str(),
        cell.registered ? "true" : "false",
        to_string(cell.dbm).c_str(),
        to_string(cell.level).c_str(),
        to_string(cell.rsrp).c_str(),
        to_string(cell.rsrq).c_str(),
        to_string(cell.sinr).c_str(),
        to_string(cell.timing_advance).c_str(),
        to_string(cell.pci).c_str(),
        to_string(cell.ci).c_str(),
        to_string(cell.tac).c_str(),
        cell.mcc.c_str(),
        cell.mnc.c_str(),
        cell.operator_name.c_str(),
        "0",        // earfcn
        "0"         // nrarfcn
    };

    PGresult* res = PQexecParams(db_conn, query, 22, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        cerr << "Ошибка вставки в БД: " << PQerrorMessage(db_conn) << endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

bool load_data(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Не найден файл: " << filename << "\n";
        return false;
    }

    cout << "Загрузка данных из " << filename << "...\n";

    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    string json_array = "[";
    bool first = true;
    size_t pos = 0;

    while (true) {
        size_t start = content.find('{', pos);
        if (start == string::npos) break;

        int depth = 0;
        size_t end = start;

        for (; end < content.size(); ++end) {
            if (content[end] == '{') depth++;
            else if (content[end] == '}') {
                depth--;
                if (depth == 0) {
                    end++;
                    break;
                }
            }
        }

        if (depth != 0) break;

        if (!first) json_array += ",";
        json_array += content.substr(start, end - start);
        first = false;
        pos = end;
    }
    json_array += "]";

    json data;
    try {
        data = json::parse(json_array);
    } catch (const exception& e) {
        cerr << "Ошибка парсинга JSON: " << e.what() << "\n";
        return false;
    }

    size_t loaded = 0;
    size_t skipped_no_location = 0;

    lock_guard<mutex> lock(history_mutex);

    for (const auto& item : data) {

        if (item.contains("error")) {
            skipped_no_location++;
            continue;
        }

        if (!item.contains("latitude") || !item.contains("longitude")) {
            skipped_no_location++;
            continue;
        }

        PacketData p;
        p.id = static_cast<int>(history.size() + 1);

        if (item.contains("time")) {
            if (item["time"].is_number())
                p.timestamp = to_string(item["time"].get<long long>());
            else if (item["time"].is_string())
                p.timestamp = item["time"].get<string>();
        }

        try {
            p.location.latitude = item["latitude"].get<float>();
            p.location.longitude = item["longitude"].get<float>();

            if (item.contains("altitude"))
                p.location.altitude = item["altitude"].get<float>();

            if (item.contains("accuracy"))
                p.location.accuracy = item["accuracy"].get<float>();

            p.location.time = p.timestamp;

        } catch (...) {
            skipped_no_location++;
            continue;
        }

        if (item.contains("cell") && item["cell"].is_string()) {
            string cell_str = item["cell"];

            CellInfo cell{};
            cell.type = "LTE";
            cell.pci = -1;
            cell.rsrp = 0;
            cell.dbm = 0;

            auto extract_int = [&](const string& key) -> int {
                size_t p = cell_str.find(key);
                if (p == string::npos) return 0;

                string val = cell_str.substr(p + key.size());
                size_t end = val.find_first_not_of("0123456789-");
                if (end != string::npos) val = val.substr(0, end);

                try { 
                    return stoi(val); 
                } catch (...) { 
                    return 0; 
                }
            };

            cell.rsrp = extract_int("rsrp=");
            if (cell.rsrp == 0) cell.rsrp = extract_int("rssi=");   

            cell.dbm  = (cell.rsrp != 0) ? cell.rsrp : extract_int("rssi=");
            cell.rsrq = extract_int("rsrq=");
            cell.sinr = extract_int("rssnr=") ? extract_int("rssnr=") : extract_int("sinr=");
            cell.timing_advance = extract_int("ta=");

            cell.pci = extract_int("mPci=");
            if (cell.pci == 0 || cell.pci == -1) {
                cell.pci = extract_int("Pci=");  
            }
            if (cell.pci == 0) cell.pci = -1;    
            if (cell.rsrp != 0 || cell.pci != -1) {
                p.cells.push_back(cell);
            }
        }

        history.push_back(move(p));
        loaded++;

        float t = rsrp_history.elapsed_time + 1.0f;
        rsrp_history.elapsed_time = t;

        for (const auto& cell : p.cells) {
            if (cell.rsrp == 0) continue;

            int key = (cell.pci >= 0) ? cell.pci : p.id;

            auto it = find_if(rsrp_history.cells.begin(), rsrp_history.cells.end(),
                [key](const CellHistory& ch) { return ch.pci == key; });

            CellHistory* hist;
            if (it == rsrp_history.cells.end()) {
                CellHistory new_hist;
                new_hist.pci = key;
                new_hist.label = (cell.pci >= 0) 
                    ? "PCI " + to_string(cell.pci)
                    : "Unknown " + to_string(p.id);

                rsrp_history.cells.push_back(move(new_hist));
                hist = &rsrp_history.cells.back();
            } else {
                hist = &(*it);
            }

            hist->times.push_back(t);
            hist->rsrp.push_back((float)cell.rsrp);
            hist->dbm.push_back((float)cell.dbm);
            hist->sinr.push_back((float)cell.sinr);
        

            if (hist->times.size() > rsrp_history.max_points) {
                hist->times.erase(hist->times.begin());
                hist->rsrp.erase(hist->rsrp.begin());
                hist->dbm.erase(hist->dbm.begin());
                hist->sinr.erase(hist->sinr.begin());
            }
        }
    }

    cout << "Загружено: " << loaded << "\n";
    cout << "Пропущено (без координат): " << skipped_no_location << "\n";
    cout << "Всего записей в истории: " << history.size() << "\n";
    cout << "Всего сот в графиках: " << rsrp_history.cells.size() << "\n";

    osm_map.track_points.clear();

    for (const auto& p : history) {
        if (abs(p.location.latitude  - 37.421997) < 0.001 &&
            abs(p.location.longitude - (-122.084)) < 0.001) {
            continue;   
        }

        if (p.location.latitude != 0.0f && p.location.longitude != 0.0f) {
            osm_map.track_points.push_back(p.location);
        }
    }

    cout << "Для карты собрано трековых точек: " << osm_map.track_points.size() << "\n";

    if (!osm_map.track_points.empty()) {
        osm_map.center_lat = osm_map.track_points[0].latitude;
        osm_map.center_lon = osm_map.track_points[0].longitude;
        cout << "[OSM] Центр карты установлен на первую точку: "
             << osm_map.center_lat << ", " << osm_map.center_lon << "\n";
    }

    if (db_conn) {
        cout << "Сохранение загруженных данных из data.json в PostgreSQL...\n";
        
        int saved_count = 0;
        int skipped_count = 0;
        
        for (const auto& p : history) {
            for (const auto& cell : p.cells) {
                bool ok = insert_cell_data(p.id, p.timestamp, p.location, cell);
                if (ok) saved_count++;
                else skipped_count++;
            }
        }
        
        cout << "В базу сохранено новых записей: " << saved_count << "\n";
        if (skipped_count > 0) {
            cout << "Пропущено (уже существуют): " << skipped_count << "\n";
        }
    }

    cout << "Загрузка из data.json завершена.\n";

    update_visible_tiles();

    return loaded > 0;
}

void run_server() {
    try {
        if (!init_database()) {
            cerr << "Не удалось инициализировать базу данных. Продолжаем без БД.\n";
        }

        load_data("data.json");

        if (!history.empty()) {
            {
                lock_guard<mutex> lock(latest_packet_mutex);
                latest_packet = history.back();
            }
            update_visible_tiles();
            cout << "[OSM] Карта обновлена по данным из data.json\n";
        }

        zmq::context_t context(1);
        zmq::socket_t socket(context, zmq::socket_type::rep);
        socket.set(zmq::sockopt::rcvtimeo, 10000);
        socket.bind("tcp://0.0.0.0:6000");

        cout << "Сервер запущен на порту 6000\n";
        cout << "Ожидание сообщений..\n";

        int next_id = 1;

        while (global_running) {
            zmq::message_t request;
            auto res = socket.recv(request, zmq::recv_flags::none);

            if (res) {
                string message = request.to_string();

                cout << "Полный JSON от клиента:\n" << message << "\n\n";

                auto now = chrono::system_clock::now();
                auto timestamp = format("{:%Y-%m-%d %H:%M:%S}", now);

                PacketData packet;
                packet.id = next_id++;
                auto now_ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
                packet.timestamp = to_string(now_ms);

                try {
                    json parsed = json::parse(message);
                        
                    if (parsed.contains("location") && parsed["location"].is_object()) {
                        auto loc = parsed["location"];

                        packet.location.latitude  = loc.value("latitude",  0.0f);
                        packet.location.longitude = loc.value("longitude", 0.0f);
                        packet.location.altitude  = loc.value("altitude",  0.0f);
                        packet.location.accuracy  = loc.value("accuracy",  0.0f);
                        packet.location.time = to_string(loc.value("time", 0L));
                        
                    }

                    if (parsed.contains("telephony") && parsed["telephony"].is_object()) {
                        auto& tel = parsed["telephony"];

                        if (tel.contains("cells") && tel["cells"].is_array()) {
                            for (const auto& cell_json : tel["cells"]) {
                                CellInfo cell;

                                cell.type = cell_json.value("type", string("unknown"));
                                cell.registered = cell_json.value("registered", false);

                                if (cell_json.contains("signal") && cell_json["signal"].is_object()) {
                                    auto& sig = cell_json["signal"];

                                    cell.dbm = sig.value("dbm", 0);
                                    cell.level = sig.value("level", 0);

                                    if (cell.type == "LTE") {
                                        cell.rsrp = sig.value("rsrp", 0);
                                        cell.rsrq = sig.value("rsrq", 0);
                                        cell.sinr = sig.value("rssnr", 0);   
                                        cell.timing_advance = sig.value("timingAdvance", 0);
                                    }
                                    else if (cell.type == "NR") {
                                        cell.rsrp = sig.value("ssRsrp", 0);     
                                        cell.rsrq = sig.value("ssRsrq", 0);
                                        cell.sinr = sig.value("ssSinr", 0);     
                                        cell.timing_advance = 0;            
                                    }
                                    else if (cell.type == "GSM") {
                                        cell.rsrp = 0;
                                        cell.rsrq = 0;
                                        cell.sinr = 0;
                                        cell.timing_advance = sig.value("timingAdvance", 0);
                                    }

                                    if (cell.rsrq == 2147483647) cell.rsrq = 0;
                                    if (cell.sinr == 2147483647) cell.sinr = 0;
                                }

                                if (cell_json.contains("identity") && cell_json["identity"].is_object()) {
                                    auto& id = cell_json["identity"];
                                    cell.ci = id.value("ci", -1LL);
                                    cell.pci = id.value("pci", -1);
                                    cell.tac = id.value("tac", -1);
                                    cell.mcc = id.value("mcc", "");
                                    cell.mnc = id.value("mnc", "");
                                    cell.operator_name = id.value("operator", "");
                                }

                                packet.cells.push_back(cell);
                            }
                        }
                    }
                } 
                catch (const json::parse_error& e) {
                    cerr << "Ошибка парсинга JSON: " << e.what()
                        << " (позиция " << e.byte << ")\n";
                }
                catch (const exception& e) {
                    cerr << "Ошибка при обработке пакета: " << e.what() << "\n";
                }

                {
                lock_guard<mutex> lock(latest_packet_mutex);
                latest_packet = packet;
                }

                update_visible_tiles();

                {
                lock_guard<mutex> lock(history_mutex);
                history.push_back(packet);
                }

                if (db_conn) {
                    for (const auto& cell : packet.cells) {
                        bool ok = insert_cell_data(packet.id, packet.timestamp, packet.location, cell);
                        
                        if (!ok) {
                            cerr << "Не удалось сохранить соту PCI=" << cell.pci << " из пакета #" << packet.id << endl;
                        }
                    }
                }

                {
                    lock_guard<mutex> lock(history_mutex);

                    rsrp_history.elapsed_time += 1.0f;  
                    float t = rsrp_history.elapsed_time;

                    for (const auto& cell : packet.cells) {
                        if (cell.pci < 0 && cell.ci <= 0) continue;

                        auto it = find_if(
                            rsrp_history.cells.begin(),
                            rsrp_history.cells.end(),
                            [pci = cell.pci](const CellHistory& ch) { return ch.pci == pci; }
                        );

                        CellHistory* hist;
                        if (it == rsrp_history.cells.end()) {
                            CellHistory new_hist;
                            new_hist.pci = cell.pci;
                            new_hist.label = "PCI " + to_string(cell.pci);
                            if (!cell.mcc.empty() && !cell.mnc.empty()) {
                                new_hist.label += " (" + cell.mcc + "-" + cell.mnc + ")";
                            }
                            rsrp_history.cells.push_back(move(new_hist));
                            hist = &rsrp_history.cells.back();
                        } else {
                            hist = &(*it);
                        }

                        hist->times.push_back(t);

                        hist->rsrp.push_back(static_cast<float>(cell.rsrp));
                        hist->dbm.push_back(static_cast<float>(cell.dbm));
                        hist->sinr.push_back(static_cast<float>(cell.sinr));

                        if (hist->times.size() > rsrp_history.max_points) {
                            hist->times.erase(hist->times.begin());
                            hist->rsrp.erase(hist->rsrp.begin());
                            hist->dbm.erase(hist->dbm.begin());
                            hist->sinr.erase(hist->sinr.begin());
                        }
                    }
                }

                string reply = "Ok!";
                socket.send(zmq::buffer(reply), zmq::send_flags::none);

                cout << "Получен пакет #" << packet.id << "  " << timestamp << "\n";
                if (packet.location.latitude != 0.0f || packet.location.longitude != 0.0f) {
                    cout << "  Location: "
                         << packet.location.latitude << ", "
                         << packet.location.longitude
                         << " (точность " << packet.location.accuracy << " м)\n";
                }
                if (!packet.cells.empty()) {
                    cout << "  Telephony: " << packet.cells.size() << " cells\n";
                }
                cout << "\n";
            }
        }
        if (db_conn) {
            PQfinish(db_conn);
        }

        cout << "Поток сервера завершает работу\n";
    }
    catch (const zmq::error_t& e) {
        cerr << "ZQM-ошибка: " << e.what()<< " (" << e.num() << ")" << endl;
    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
    }
    catch (...) {
        if (db_conn) PQfinish(db_conn);
    }
}