#include "wfsync.h"
#include <utils/flog.h>

namespace wfsync {

    SyncService::SyncService() {
    }

    void SyncService::close() {
    }

    void SyncService::start(std::shared_ptr<net::rigctl::Client> _rigClient) {
        running = true;
        this->rigClient = _rigClient;
        workerThread = std::thread(&SyncService::worker, this);
    }


    void SyncService::stop() {
        running = false;
        if(workerThread.joinable()) {
            workerThread.join();
        }
    }

#if defined (_WIN32) || defined(_WIN64) // this is so ugly...
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
#endif      

    void SyncService::worker() {
        struct timespec currentCall = {0,0};
        int sdrmode;
        ulong delta;
        //float sdrbw;
        //int wfmode;
        //float wfbw;
        //int wfmode

        if (!this->rigClient || !this->rigClient->isOpen()) { return; }

        while(running) {
            // Limit the call rate
            clock_gettime(CLOCK_MONOTONIC_RAW, &currentCall);
            delta = (currentCall.tv_sec - lastCall.tv_sec) * 1000000 + (currentCall.tv_nsec - lastCall.tv_nsec) / 1000;
            if (delta > 200000) {
                ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
                int snapInt = vfo->snapInterval;
                double sdrFreq = int(gui::waterfall.getCenterFrequency() + sigpath::vfoManager.getOffset(gui::waterfall.selectedVFO));
                double wfFreq = this->rigClient->getFreq();
                //core::modComManager.callInterface(gui::waterfall.selectedVFO, RADIO_IFACE_CMD_GET_MODE, NULL, &sdrmode);
                //core::modComManager.callInterface(gui::waterfall.selectedVFO, RADIO_IFACE_CMD_GET_BANDWIDTH, NULL, &sdrbw);

                if(sdrFreq == wfFreq) {
                    // Do nothing but make sure lastFreq is set
                    this->lastFreq = wfFreq;
                }
                else if(wfFreq != this->lastFreq) {
                    // Give priority to the radio
                    this->lastFreq = wfFreq;
                    tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, this->lastFreq);
                }
                else if(sdrFreq != this->lastFreq) {
                    this->lastFreq = sdrFreq;
                    this->rigClient->setFreq(this->lastFreq);
                }
                lastCall = currentCall;
            }
            sched_yield();
        }
    }
    
    std::shared_ptr<SyncService> open() {
        return std::make_shared<SyncService>();
    }
}