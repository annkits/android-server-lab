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
            
            rsrp INTEGER,      -- LTE: rsrp, NR: ssRsrp
            rsrq INTEGER,      -- LTE: rsrq, NR: ssRsrq
            sinr INTEGER,      -- LTE: rssnr, NR: ssSinr
            
            timing_advance INTEGER,
            
            pci INTEGER,
            ci BIGINT,
            tac INTEGER,
            mcc VARCHAR(10),
            mnc VARCHAR(10),
            operator_name VARCHAR(100),
            
            earfcn INTEGER,    -- только для LTE
            nrarfcn INTEGER    -- только для NR
        );
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

bool insert_cell_data(int packet_id, const std::string& timestamp, const LocationInfo& loc, const CellInfo& cell) {
    if (!db_conn) return false;

    const char* query = R"(
        INSERT INTO cell_measurements 
        (packet_id, timestamp, latitude, longitude, altitude, accuracy,
         cell_type, registered, dbm, level, rsrp, rsrq, sinr, timing_advance,
         pci, ci, tac, mcc, mnc, operator_name, earfcn, nrarfcn)
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10,
                $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22)
    )";

    char packet_id_str[32];
    snprintf(packet_id_str, sizeof(packet_id_str), "%d", packet_id);

    const char* paramValues[22] = {
        packet_id_str,
        timestamp.c_str(),
        std::to_string(loc.latitude).c_str(),
        std::to_string(loc.longitude).c_str(),
        std::to_string(loc.altitude).c_str(),
        std::to_string(loc.accuracy).c_str(),
        cell.type.c_str(),
        cell.registered ? "true" : "false",
        std::to_string(cell.dbm).c_str(),
        std::to_string(cell.level).c_str(),
        std::to_string(cell.rsrp).c_str(),
        std::to_string(cell.rsrq).c_str(),
        std::to_string(cell.sinr).c_str(),
        std::to_string(cell.timing_advance).c_str(),
        std::to_string(cell.pci).c_str(),
        std::to_string(cell.ci).c_str(),
        std::to_string(cell.tac).c_str(),
        cell.mcc.c_str(),
        cell.mnc.c_str(),
        cell.operator_name.c_str(),
        "0",        // earfcn
        "0"         // nrarfcn
    };

    PGresult* res = PQexecParams(db_conn, query,
                                 22, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        cerr << "Ошибка вставки в БД: " << PQerrorMessage(db_conn) << endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

bool load_data(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Не найден файл: " << filename << "\n";
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (...) {
        std::cerr << "Ошибка чтения JSON файла\n";
        return false;
    }

    if (!j.is_array()) {
        std::cerr << "Файл не является массивом!\n";
        return false;
    }

    std::cout << "Загружаю " << j.size() << " записей...\n";

    {
        std::lock_guard<std::mutex> lock(history_mutex);

        for (const auto& item : j) {
            try {
                PacketData p;
                p.id = history.size() + 1;

                long long tm = 0;
                if (item.contains("time") && !item["time"].is_null()) {
                    tm = item["time"].get<long long>();
                }
                p.timestamp = std::to_string(tm);

                p.location.longitude = item.value("longitude", 0.0f);
                p.location.latitude  = item.value("latitude",  0.0f);
                p.location.altitude  = item.value("altitude",  0.0f);
                p.location.accuracy  = item.value("accuracy",  0.0f);
                p.location.time      = p.timestamp;

                CellInfo cell;
                cell.type = "LTE";

                std::string cell_str = item.value("cell", "");

                if (cell_str.find("rsrp=") != std::string::npos) {
                    size_t pos = cell_str.find("rsrp=");
                    std::string substr = cell_str.substr(pos + 5);
                    size_t end = substr.find_first_not_of("0123456789-");
                    if (end != std::string::npos) {
                        substr = substr.substr(0, end);
                    }
                    try {
                        cell.rsrp = std::stoi(substr);
                        cell.dbm  = cell.rsrp;       
                    } catch (...) {
                        cell.rsrp = 0;
                        cell.dbm  = 0;
                    }
                }

                if (cell_str.find("mPci=") != std::string::npos) {
                    size_t pos = cell_str.find("mPci=");
                    std::string substr = cell_str.substr(pos + 5);
                    size_t end = substr.find_first_not_of("0123456789-");
                    if (end != std::string::npos) {
                        substr = substr.substr(0, end);
                    }
                    try {
                        cell.pci = std::stoi(substr);
                    } catch (...) {
                        cell.pci = -1;
                    }
                }

                p.cells.push_back(cell);

                history.push_back(p);

                float t = rsrp_history.elapsed_time + 1.0f;
                rsrp_history.elapsed_time = t;

                if (cell.rsrp != 0 && cell.pci >= 0) {
                    auto it = std::find_if(rsrp_history.cells.begin(), rsrp_history.cells.end(),
                        [pci = cell.pci](const CellHistory& ch){ return ch.pci == pci; });

                    CellHistory* hist;
                    if (it == rsrp_history.cells.end()) {
                        CellHistory newh;
                        newh.pci = cell.pci;
                        newh.label = "PCI " + std::to_string(cell.pci);
                        rsrp_history.cells.push_back(std::move(newh));
                        hist = &rsrp_history.cells.back();
                    } else {
                        hist = &(*it);
                    }

                    hist->times.push_back(t);
                    hist->rsrp.push_back(static_cast<float>(cell.rsrp));

                    if (hist->times.size() > rsrp_history.max_points) {
                        hist->times.erase(hist->times.begin());
                        hist->rsrp.erase(hist->rsrp.begin());
                    }
                }
            }
            catch (const json::exception& e) {
                std::cerr << "Ошибка парсинга записи #" << (history.size() + 1) << "\n";
                std::cerr << "Сообщение: " << e.what() << "\n";
                std::cerr << "Проблемная запись:\n" << item.dump(2) << "\n\n";
                continue;
            }
        }
    }

    std::cout << "Данные загружены (" << history.size() << " записей)\n";
    return true;
}

void run_server() {
    try {
        if (!init_database()) {
            cerr << "Не удалось инициализировать базу данных. Продолжаем без БД.\n";
        }

        load_data("data.json");

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
                packet.timestamp = timestamp;

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
                    std::lock_guard<std::mutex> lock(history_mutex);

                    rsrp_history.elapsed_time += 1.0f;  
                    float t = rsrp_history.elapsed_time;

                    for (const auto& cell : packet.cells) {
                        if (cell.pci < 0) continue;

                        auto it = std::find_if(
                            rsrp_history.cells.begin(),
                            rsrp_history.cells.end(),
                            [pci = cell.pci](const CellHistory& ch) { return ch.pci == pci; }
                        );

                        CellHistory* hist;
                        if (it == rsrp_history.cells.end()) {
                            CellHistory new_hist;
                            new_hist.pci = cell.pci;
                            new_hist.label = "PCI " + std::to_string(cell.pci);
                            if (!cell.mcc.empty() && !cell.mnc.empty()) {
                                new_hist.label += " (" + cell.mcc + "-" + cell.mnc + ")";
                            }
                            rsrp_history.cells.push_back(std::move(new_hist));
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
        cerr << "ZQM-ошибка: " << e.what()<< " (" << e.num() << ")" << std::endl;
    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << std::endl;
    }
    catch (...) {
        if (db_conn) PQfinish(db_conn);
    }
}