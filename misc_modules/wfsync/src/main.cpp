/* 
  WFview SDR++ Sync for using SDR++ as panadapter
  Bob Turbeville KN4B
*/
#include <utils/proto/rigctl.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <signal_path/signal_path.h>
#include <gui/widgets/frequency_select.h>
#include <radio_interface.h>
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <thread>

#include "wfsync.h"

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "wfsync",
    /* Description:     */ "WFView SDR++ sync controller",
    /* Author:          */ "Bob Turbeville KN4B",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class WFSyncModule: public ModuleManager::Instance {
public:
    WFSyncModule(std::string name) {
        this->name = name;

        strcpy(host, "127.0.0.1");

        config.acquire();
        if (config.conf[name].contains("host")) {
            std::string h = config.conf[name]["host"];
            strcpy(host, h.c_str());
        }
        if (config.conf[name].contains("port")) {
            port = config.conf[name]["port"];
            port = std::clamp<int>(port, 1, 65535);
        }
        config.release();

        gui::menu.registerEntry(name, menuHandler, this, NULL);

    }

    ~WFSyncModule() {
        gui::menu.removeEntry(name);
    }

    void postInit() {
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    static void start(void *ctx) {
        flog::debug("Starting Sync");
        WFSyncModule* _this = (WFSyncModule*)ctx;
        std::lock_guard<std::recursive_mutex> lck(_this->mtx);
        if (_this->running) { return; }

        // Connect to wfview rigctl server
        try {
            _this->client = net::rigctl::connect(_this->host, _this->port);
        }
        catch (const std::exception& e) {
            flog::error("Could not connect: {}", e.what());
            return;
        }

        try {
            if(!_this->syncClient)
                _this->syncClient = wfsync::open();
            _this->syncClient->start(_this->client);
        }
        catch (const std::exception& e) {
            flog::error("Could not start sync: {}", e.what());
            return;
        }

        _this->running = true;
    }

    static void stop(void *ctx) {
        WFSyncModule* _this = (WFSyncModule*)ctx;
        std::lock_guard<std::recursive_mutex> lck(_this->mtx);
        if (!_this->running) { return; }

        // Disconnect from wfview rigctl server
        _this->syncClient->stop();
        _this->client->close();

        _this->running = false;
    }

private:

    static void menuHandler(void* ctx) {
        WFSyncModule* _this = (WFSyncModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (_this->running) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_wfsync_cli_host_", _this->name), _this->host, 1023)) {
            config.acquire();
            config.conf[_this->name]["host"] = std::string(_this->host);
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_wfsync_cli_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf[_this->name]["port"] = _this->port;
            config.release(true);
        }
        if (_this->running) { style::endDisabled(); }

        ImGui::FillWidth();
        if (_this->running && ImGui::Button(CONCAT("Stop##_wfsync_cli_stop_", _this->name), ImVec2(menuWidth, 0))) {
            _this->stop(ctx);
        }
        else if (!_this->running && ImGui::Button(CONCAT("Start##_wfsync_cli_stop_", _this->name), ImVec2(menuWidth, 0))) {
            _this->start(ctx);
        }

        ImGui::TextUnformatted("Status:");
        ImGui::SameLine();
        if (_this->client && _this->client->isOpen() && _this->running) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), "Connected");
        }
        else if (_this->client && _this->running) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Disconnected");
        }
        else {
            ImGui::TextUnformatted("Idle");
        }

    }
#if defined (_WIN32) || defined(_WIN64)
/*
    #define CLOCK_MONOTONIC_RAW  4
    struct timespec { 
        long tv_sec;
        long tv_nsec; 
    }; 

    int clock_gettime(int, struct timespec *spec) {  
    # define CLOCK_MONOTONIC_RAW  4
        __int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime);
        wintime      -=116444736000000000i64;  //1jan1601 to 1jan1970
        spec->tv_sec  =wintime / 10000000i64;           //seconds
        spec->tv_nsec =wintime % 10000000i64 *100;      //nano-seconds
        return 0;
    }
*/
#endif
    
    std::string name;
    std::recursive_mutex mtx;

    bool enabled = false;
    bool running = false;
    char host[1024];
    int port = 4532;
    std::shared_ptr<net::rigctl::Client> client;
    std::shared_ptr<wfsync::SyncService> syncClient;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/wfsync_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new WFSyncModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (WFSyncModule *)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
