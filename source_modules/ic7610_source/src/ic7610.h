#include <dsp/stream.h>
#include <dsp/types.h>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#if defined (__linux__) || defined (__APPLE__)
#include <unistd.h>
#include "ftd3xx.h"
#define Sleep(x) usleep(x * 1000)
#else
#include "FTD3XX.h"
#endif

#define IQ_EP       0x84
#define CTL_EP_OUT  0x02
#define CTL_EP_IN   0x82

namespace ic7610 {
    
    class Client {
    public:
        Client();
        ~Client();

        void close();
        void start();
        void stop();
        
        bool setFrequency(double freq);
        double getFrequency();
        bool setGain(uint8_t gain);
        uint8_t getGain();

        dsp::stream<dsp::complex_t> out;

    private:
        void worker();
        bool sendPacket(std::vector<uint8_t>);
        std::vector<uint8_t> getPacket();
        bool checkResponse(std::vector<uint8_t>);
        DWORD getNumDevicesConnected();
        DWORD getDeviceList(FT_DEVICE_LIST_INFO_NODE **ptDevicesInfo);
        DWORD getDeviceIndex(FT_DEVICE_LIST_INFO_NODE *ptDevicesInfo, DWORD dwNumDevices);
        bool setIqEnable(bool);
        bool initialize();

        double freq = 0;
        int blockSize = 63;

        std::thread workerThread;
        FT_HANDLE   m_FTHandle;
        bool        m_bIsUsb3;
	    uint8_t     m_FIFOMode;
	    FT_DEVICE   m_eIntfType;
	    DWORD       m_dwDeviceIndex;
        bool        m_running;

    };

    std::shared_ptr<Client> open();

}
