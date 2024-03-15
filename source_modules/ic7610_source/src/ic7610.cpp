#include "ic7610.h"
#include <utils/flog.h>
#include <mutex>

std::vector<uint8_t>icHeader = {0xFE, 0xFE, 0x98, 0xE0};

namespace ic7610 {

    Client::Client() :
        m_FTHandle(NULL),
        m_bIsUsb3(TRUE),
        m_running(false)
    {

    }

    Client::~Client()
    {
        if(m_running)
        {
            m_running = false;
        }
        if (m_FTHandle)
        {
            FT_Close(m_FTHandle);
            m_FTHandle = NULL;
        }
    }

    void Client::start()
    {
        if(initialize())
        {
            m_running = true;
            // Start worker
            workerThread = std::thread(&Client::worker, this);
        }
    }

    void Client::stop()
    {
        if(m_running)
        {
            m_running = false;
            FT_AbortPipe(m_FTHandle, IQ_EP);
        }
    }

    void Client::close()
    {
        // Wait for worker to exit
        out.stopWriter();
        if (workerThread.joinable()) { workerThread.join(); }
        out.clearWriteStop();
    }
    
    bool Client::sendPacket(std::vector<uint8_t>pkt)
    {
        ULONG transferred = 0;

        int pad = pkt.size() % 4;

        for(int i = 0; i < pad; i++)
            pkt.push_back(0xFF);


        FT_STATUS ftStatus = FT_WritePipe(m_FTHandle, 0x02, pkt.data(), pkt.size(), &transferred, NULL);
        if(FT_FAILED(ftStatus))
        {
            flog::debug("Failed to send FTDI packet");
            return false;
        }
        return true;
    }

    std::vector<uint8_t> Client::getPacket()
    {
        unsigned long transferred = 0;
        uint8_t tbuff[64];

        FT_STATUS ftStatus = FT_ReadPipe(m_FTHandle, 0x82, tbuff, sizeof(tbuff), &transferred, NULL);
        if(FT_FAILED(ftStatus))
        {
            flog::debug("Failed to get response");
            return std::vector<uint8_t>();
        }
        return std::vector<uint8_t>(tbuff, tbuff + transferred);
    }

    bool Client::checkResponse(std::vector<uint8_t> pkt)
    {
        if(pkt.size() < 8)
        {
            flog::info("bad response size");
            return false;
        }

        if(pkt.at(0) != 0xFE || pkt.at(1) != 0xFE)
        {
            flog::info("Header error");
            return false;
        }

        if(pkt.at(4) == 0xFB && pkt.at(5) == 0xFD)
            return true;

        fprintf(stderr, "RESPONSE: ");
        for(int i = 0; i < pkt.size(); i++)
            fprintf(stderr, "%02x ", *(pkt.data() + i));
        fprintf(stderr, "\n");
        
        return false;
    }

    bool Client::setGain(uint8_t gain)
    {
        unsigned char a;
        unsigned char resp[16];
        std::vector<uint8_t>pkt(icHeader);

        pkt.push_back(0x14);
        pkt.push_back(0x02);

        uint8_t fbuff[2];
        a = 0;
        a |= (gain % 10);
        gain /= 10;
        a |= (gain % 10)<<4;
        gain /= 10;
        fbuff[1] = a;
        a = 0;
        a |= (gain % 10);
        gain /= 10;
        a |= (gain % 10)<<4;
        fbuff[0] = a;

        pkt.push_back(fbuff[0]);
        pkt.push_back(fbuff[1]);

        pkt.push_back(0xFD);

        if(!sendPacket(pkt))
        {
            flog::info("Failed to send set gain packet");
            return false;
        }

        std::vector<uint8_t> response = getPacket();
        if(response.size() == 0)
        {
            flog::info("Set gain failed to get response packet");
            return false;
        }

        if(!checkResponse(response))
        {
            flog::info("Failed to set gain");
            return false;
        }
        return true;
    }

    uint8_t Client::getGain()
    {
        std::vector<uint8_t>pkt(icHeader);

        pkt.push_back(0x14);
        pkt.push_back(0x02);
        pkt.push_back(0xFD);

        if(!sendPacket(pkt))
        {
            flog::info("Failed to send get gain packet");
            return false;
        }

        std::vector<uint8_t> response = getPacket();
        if(response.size() == 0)
        {
            flog::info("Get gain failed to get response packet");
            return -1;
        }
        if(response.size() < 9 || response.at(4) != 0x14 || response.at(5) != 0x02)
        {
            flog::info("Bad get frequency response");
            return -1;
        }

        uint16_t gain = ((response.at(6)) & 0x0F) * 100;
        gain += ((response.at(7) >> 4) & 0x0F) * 10;
        gain += (response.at(7) & 0x0F);

        return (uint8_t)gain;
    }
    bool Client::setFrequency(double freq)
    {
        std::vector<uint8_t>pkt(icHeader);
        unsigned char a;
        unsigned char resp[16];
        uint32_t intFreq = (uint32_t)freq;

        pkt.push_back(0x25);
        pkt.push_back(0x00);

        for (int i = 0; i < 5; i++) 
        {
            a = 0;
            a |= (intFreq % 10);
            intFreq /= 10;
            a |= (intFreq % 10)<<4;
            intFreq /= 10;
            pkt.push_back(a);
        }

        pkt.push_back(0xFD);

        if(!sendPacket(pkt))
        {
            flog::info("Failed to send set frequency packet");
            return false;
        }

        std::vector<uint8_t> response = getPacket();
        if(response.size() == 0)
        {
            flog::info("Set frequency failed to get response packet");
            return false;
        }

        if(!checkResponse(response))
        {
            flog::info("Failed to set frequency");
            return false;
        }
        return true;
    }

    double Client::getFrequency()
    {
        std::vector<uint8_t>pkt(icHeader);
        pkt.push_back(0x25);
        pkt.push_back(0x00);
        pkt.push_back(0xFD);
        if(!sendPacket(pkt))
        {
            flog::info("Failed to send get frequency");
            return -1;
        }

        std::vector<uint8_t> response = getPacket();
        if(response.size() == 0)
        {
            flog::info("Get frequency failed to get response packet");
            return -1;
        }

        if(response.size() < 12 || response.at(4) != 0x25)
        {
            flog::info("Bad get frequency response");
            return -1;
        }

        uint64_t intFreq = 0;
        uint64_t mult = 0;
        for(int i = 0; i < 4; i++)
        {
            uint8_t in = response.at(i + 6);
            intFreq += (uint64_t)(in & 0x0F) * ((uint64_t)std::pow(10, mult++));
            intFreq += (uint64_t)((in >> 4) & 0x0F) * ((uint64_t)std::pow(10, mult++));
        }
        return (double)intFreq;
    }

    bool Client::setIqEnable(bool enable)
    {
        std::vector<uint8_t>pkt(icHeader);

        pkt.push_back(0x1A);
        pkt.push_back(0x0B);
        pkt.push_back((enable ? 0x01 : 0x00));
        pkt.push_back(0xFD);

        if(!sendPacket(pkt))
        {
            flog::info("Failed to send iq packet");
            return false;
        }

        std::vector<uint8_t> response = getPacket();
        if(response.size() == 0)
        {
            flog::info("get iq response packet");
            return false;
        }

        if(!checkResponse(response))
        {
            flog::info("Failed to set iq");
            return false;
        }

        return true;

    }

    DWORD Client::getNumDevicesConnected()
    {
        FT_STATUS status = FT_OK;
        DWORD dwNumDevices = 0;

        status = FT_ListDevices(&dwNumDevices, NULL, FT_LIST_NUMBER_ONLY);
        if(FT_FAILED(status))
        {
            flog::debug("ListDevices failed");
            return 0;
        }

        flog::debug("Found device");

        return dwNumDevices;
    }

    DWORD Client::getDeviceList(FT_DEVICE_LIST_INFO_NODE **ptDevicesInfo)
    {
        FT_STATUS ftStatus = FT_OK;
        DWORD dwNumDevices = 0;

        ftStatus = FT_CreateDeviceInfoList(&dwNumDevices);
        if (FT_FAILED(ftStatus))
        {
            flog::debug("Failed to get devices");
            return 0;
        }

        *ptDevicesInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * dwNumDevices);
        if (!(*ptDevicesInfo))
        {
            flog::debug("Malloc failure");
            return 0;
        }

        ftStatus = FT_GetDeviceInfoList(*ptDevicesInfo, &dwNumDevices);
        if (FT_FAILED(ftStatus))
        {
            flog::debug("Failed to get device list");
            free(*ptDevicesInfo);
            *ptDevicesInfo = NULL;
            return 0;
        }

        return dwNumDevices;

    }

    DWORD Client::getDeviceIndex(FT_DEVICE_LIST_INFO_NODE *ptDevicesInfo, DWORD dwNumDevices)
    {
        FT_STATUS ftStatus = FT_OK;

        for(DWORD i = 0; i < dwNumDevices; i++)
        {
            flog::debug("Device {0}", ptDevicesInfo[i].Description);
            if(strcmp(ptDevicesInfo[i].Description, "IC-7610 SuperSpeed-FIFO Bridge") == 0)
                return i;
        }
        flog::debug("No IC7610 found");
        return 0xFFFFFFFF;
    }

    bool Client::initialize()
    {
        bool bResult = FALSE;
        FT_STATUS ftStatus = FT_OK;
        int iDeviceNumber = 0;
        int iNumTries = 0;
        DWORD dwNumDevices = 0;
        FT_DEVICE_LIST_INFO_NODE *ptDevicesInfo = NULL;
        FT_60XCONFIGURATION oConfig = { 0 };
        FT_DEVICE_DESCRIPTOR DeviceDescriptor;
        FT_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor;
        FT_INTERFACE_DESCRIPTOR InterfaceDescriptor;
        FT_PIPE_INFORMATION Pipe;
        FT_STATUS ftResult = FT_OK;


        if((dwNumDevices = getNumDevicesConnected()) == 0)
        {
            flog::info("No D3xx devices found");
            return false;
        }

        if(getDeviceList(&ptDevicesInfo) == 0)
        {
            flog::info("No device connected!");
            return false;
        }

        m_dwDeviceIndex = getDeviceIndex(ptDevicesInfo, dwNumDevices);
        if(m_dwDeviceIndex == 0xFFFFFFFF)
        {
            flog::info("No D3xx in list");
            free(ptDevicesInfo);
            return false;
        }

        free(ptDevicesInfo);

        m_FTHandle = NULL;

        ftStatus = FT_Create("IC-7610 SuperSpeed-FIFO Bridge", FT_OPEN_BY_DESCRIPTION, &m_FTHandle);

        if(FT_FAILED(ftStatus))
        {
            flog::info("Failed to open device");
            return false;
        }

        uint16_t uVID = 0;
        uint16_t uPID = 0;

        ftStatus = FT_GetVIDPID(m_FTHandle, &uVID, &uPID);
        if(FT_FAILED(ftStatus))
        {
            flog::debug("Failed to get VID:PID");
        }

        flog::debug("Found VID {0} PID {1}", uVID, uPID);
        /*
        ZeroMemory(&DeviceDescriptor, sizeof(FT_DEVICE_DESCRIPTOR));
        ZeroMemory(&ConfigurationDescriptor, sizeof(FT_CONFIGURATION_DESCRIPTOR));
        ZeroMemory(&InterfaceDescriptor, sizeof(FT_INTERFACE_DESCRIPTOR));
        ZeroMemory(&Pipe, sizeof(FT_PIPE_INFORMATION));
    
        // Get configuration descriptor to determine the number of interfaces in the configuration
        ftResult = FT_GetDeviceDescriptor(m_FTHandle, &DeviceDescriptor);
        if (FT_FAILED(ftResult))
        {
            flog::info("Failed to get Device descriptor");
            return false;
        }
        if (DeviceDescriptor.bcdUSB < 0x0300)
        {
            m_bIsUsb3 = false;
        }
        // Get configuration descriptor to determine the number of interfaces in the configuration
        ftResult = FT_GetConfigurationDescriptor(m_FTHandle, &ConfigurationDescriptor);
        if (FT_FAILED(ftResult))
        {
            flog::info("Failed to get Configuration descriptor");
            return FALSE;
        }

        flog::debug("Number of Interfaces {0}", ConfigurationDescriptor.bNumInterfaces);
        // Get interface descriptor to determine the number of endpoints in the interface
        UCHAR ucEPOffset = 0;
        UCHAR ucInterfaceIndex = 1;

        if (ConfigurationDescriptor.bNumInterfaces == 1)
        {
            ftResult = FT_GetInterfaceDescriptor(m_FTHandle, 0, &InterfaceDescriptor);
            ucEPOffset = 2;
            ucInterfaceIndex = 0;
        }
        else //if (ConfigurationDescriptor.bNumInterfaces == 2)
        {
            ftResult = FT_GetInterfaceDescriptor(m_FTHandle, 1, &InterfaceDescriptor);
        }
        if (FT_FAILED(ftResult))
        {
            return FALSE;
        }
        flog::debug("Number of endpoints {0}", InterfaceDescriptor.bNumEndpoints);

        //a_pucNumReadEP = 0;
        //a_pucNumWriteEP = 0;
    
        for (UCHAR i=ucEPOffset; i<InterfaceDescriptor.bNumEndpoints; i++)
        {
            ftResult = FT_GetPipeInformation(m_FTHandle, ucInterfaceIndex, i, &Pipe);
            if (FT_FAILED(ftResult))
            {
                return FALSE;
            }

            if (FT_IS_READ_PIPE(Pipe.PipeId))
            {
                flog::debug("Read: {0} - {1}", (int)i, Pipe.PipeId);
                //a_pucReadEP[*a_pucNumReadEP] = Pipe.PipeId;
                //(*a_pucNumReadEP)++;
            }
            else
            {
                flog::debug("Write: {0} - {1}", (int)i, Pipe.PipeId);
                //a_pucWriteEP[*a_pucNumWriteEP] = Pipe.PipeId;
                //(*a_pucNumWriteEP)++;
            }
        }

		ftStatus = FT_GetChipConfiguration(m_FTHandle, &oConfig);
		if (FT_FAILED(ftStatus))
			fprintf(stderr,"Failed to get configuration %ld\n", ftStatus);
        else
    		fprintf(stderr, "FIFO Mode %d\n", oConfig.FIFOMode);
        */
        // Enable IQ stream on Main VFO
    
        return setIqEnable(true);

    }
    
    void Client::worker()
    {
        bool bFailed = FALSE;
        FT_STATUS ftStatus;
        uint32_t ulTimeMs = 0;
        uint32_t i = 0;
        uint8_t* pBuffer = NULL;
        ULONG ulActualBytesTransferred = 0;
        ULONG ulActualBytesToTransfer = 32768;
        
        pBuffer = new uint8_t[32768];

        ftStatus = FT_SetStreamPipe(m_FTHandle, FALSE, FALSE, IQ_EP, ulActualBytesToTransfer);
        if (FT_FAILED(ftStatus))
        {
            goto exit;
        }

        flog::debug("Worker starting");

        // Get band 0 frequency
        unsigned long transferred = 0;


        while (m_running)
        {
            ulActualBytesTransferred = 0;

            ftStatus = FT_ReadPipe(m_FTHandle, IQ_EP, pBuffer, ulActualBytesToTransfer, &ulActualBytesTransferred, NULL);
            if (FT_FAILED(ftStatus))
            {
                flog::info("Read pipe failed");
                fprintf(stderr, "%ld\n", ftStatus);
                break;
            }
            ULONG i = 0;
            volk_16i_s32f_convert_32f((float*)out.writeBuf, (int16_t*)pBuffer, 32768.0f, ulActualBytesTransferred / 2);
            out.swap(ulActualBytesTransferred / 4);
        }
    exit:
        flog::info("Worker thread exited");
        FT_ClearStreamPipe(m_FTHandle, FALSE, FALSE, IQ_EP);

        if (pBuffer)
        {
            delete[] pBuffer;
        }

        return;
    }

    std::shared_ptr<Client> open() 
    {
        return std::make_shared<Client>();
    }
   
}