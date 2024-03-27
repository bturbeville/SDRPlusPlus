#include "ic7610.h"
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <gui/widgets/stepped_slider.h>
#include <dsp/routing/stream_link.h>
#include <utils/optionlist.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "ic7610_source",
    /* Description:     */ "Icom 7610 IQ source module for SDR++",
    /* Author:          */ "KN4B Bob Turbeville",
    /* Version:         */ 0, 1, 1,
    /* Max instances    */ 1
};

ConfigManager config;

class IC7610SourceModule : public ModuleManager::Instance {
public:
    IC7610SourceModule(std::string name) {
        this->name = name;

        lnk.init(NULL, &stream);
        
        sampleRate = 1920000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        config.acquire();
        if(config.conf["device"].contains("gain"))
            this->gain = config.conf["device"]["gain"];
        config.release();

        sigpath::sourceManager.registerSource("IC7610", &handler);
    }

    ~IC7610SourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("IC7610");
     }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuSelected(void* ctx) {
        IC7610SourceModule* _this = (IC7610SourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("IC7610SourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        IC7610SourceModule* _this = (IC7610SourceModule*)ctx;
        flog::info("IC7610SourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {

        IC7610SourceModule* _this = (IC7610SourceModule*)ctx;
        if (_this->running) { return; }
        
        _this->dev = ic7610::open();
        _this->lnk.setInput(&_this->dev->out);
        _this->lnk.start();
        _this->dev->start();
        
        uint8_t gain = _this->dev->getGain();
        if(gain >= 0)
            _this->gain = (int)gain;

        double rigFreq = _this->dev->getFrequency();
        flog::debug("Rig frequency {0}", rigFreq);
        _this->rigSet = true;
        tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, rigFreq);
        _this->freq = rigFreq; 

        _this->running = true;

        _this->rigThread = std::thread(&IC7610SourceModule::worker, _this);

        Sleep(400);
        flog::info("IC7610SourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {

        IC7610SourceModule* _this = (IC7610SourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        _this->dev->stop();
        _this->dev->close();
        _this->lnk.stop();

        if (_this->rigThread.joinable()) { 
            _this->rigThread.join(); 
        }

        flog::info("IC7610SourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        IC7610SourceModule* _this = (IC7610SourceModule*)ctx;
        if (_this->running) {
            _this->mtx.lock();
            if(_this->rigSet)
                _this->rigSet = false;
            else {
                _this->dev->setFrequency(freq);
                _this->sdrSet = true;
                _this->freq = freq;
                flog::info("IC7610SourceModule '{0}': Tune: {1}!", _this->name, (uint32_t)freq);
            }
            _this->mtx.unlock();

        }
    }

    static void menuHandler(void* ctx) {
        IC7610SourceModule* _this = (IC7610SourceModule*)ctx;

        SmGui::ForceSync();
        SmGui::LeftLabel("RF Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt("##ic7610_source_rf_gain", &_this->gain, 0, 255)) {
            if (_this->running) {
                _this->mtx.lock();
                _this->dev->setGain((uint8_t)_this->gain);
                _this->mtx.unlock();
            }
        }

    }

    void worker()
    {
        flog::info("rig thread running");
        while(running)
        {
            Sleep(200);
            mtx.lock();
            if(sdrSet)
                sdrSet = false;
            else
            {
                double rigFreq = this->dev->getFrequency();
                if(rigFreq != freq)
                {
                    rigSet = true;
                    freq = rigFreq;
                    mtx.unlock();
                    tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, rigFreq);
                    continue;
                }
            }
            uint8_t tgain = this->dev->getGain();
            if(tgain > 0)
                gain = (int)tgain;
            mtx.unlock();

        }
        flog::info("rig thread exit");

    }

    std::string name;
    int gain;
    bool enabled = false;
    bool running = false;
    bool rigSet = false;
    bool sdrSet = false;
    dsp::stream<dsp::complex_t> stream;
    dsp::routing::StreamLink<dsp::complex_t> lnk;
    double sampleRate;
    double freq = 0;
    SourceManager::SourceHandler handler;
    std::shared_ptr<ic7610::Client> dev;
    std::thread rigThread;
    std::mutex mtx;

};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["device"] = json({});
    config.setPath(core::args["root"].s() + "/ic7610_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new IC7610SourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (IC7610SourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}