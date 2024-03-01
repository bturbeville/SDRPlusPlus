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

namespace wfsync {
    class SyncService {
    public:
        SyncService();

        void close();
        void start(std::shared_ptr<net::rigctl::Client> _rigClient);
        void stop();

    private:
        void worker();

        double lastFreq = 0;
        struct timespec lastCall = {0,0};
        bool running = false;

        std::thread workerThread;
        std::shared_ptr<net::rigctl::Client> rigClient;
    };

    std::shared_ptr<SyncService> open(); 
}