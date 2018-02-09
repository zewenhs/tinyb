/*
 * Copyright (c) 2018 Telink Semiconductor.
 *
 * g++ -o notifications notifications.cpp --std=c++11 -ltinyb
 */

#include <tinyb.hpp>

#include <vector>
#include <iostream>
#include <thread>
#include <csignal>
#include <condition_variable>
#include <chrono>

#include <string>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <string.h>

#include <fcntl.h>
#include <linux/input.h>

#include <errno.h>
#include <stdlib.h>

using namespace tinyb;
using namespace std;
using namespace chrono;

std::condition_variable cv;

#define NOT_IN_ASR 0
#define IN_ASR 1
#define MIC_ADPCM_FRAME_SIZE    128
#define MIC_SHORT_DEC_SIZE    248
#define MIC_PCM_BUF_SIZE    496

static short state = NOT_IN_ASR;

ofstream ofile;
ifstream ifile;
ofstream outfile;
int ntime = 0;
int ntime_b = 0;
struct itimerval tick;
const char * filename_adpcm = "test.adpcm";
const char * filename_pcm = "test.pcm";
const char * filename_wav = "test.wav";

void adpcm_to_pcm (signed short *ps, signed short *pd, int len);


//#define PCM_DATA_SIZE     99200
//char savedata[PCM_DATA_SIZE]; // 解压后的一包PCM数据
unsigned char RiffHeader[] = {
    'R' , 'I' , 'F' , 'F' , // Chunk ID (RIFF)
    0x70, 0x70, 0x70, 0x70, // Chunk payload size (calculate after rec!)
    'W' , 'A' , 'V' , 'E' , // RIFF resource format type
    'f' , 'm' , 't' , ' ' , // Chunk ID (fmt )
    0x10, 0x00, 0x00, 0x00, // Chunk payload size (0x14 = 20 bytes)
    0x01, 0x00,             // Format Tag ()
    0x01, 0x00,             // Channels (1)
    0x80, 0x3e, 0x00, 0x00, // Byte rate       32.0K
    //0x40, 0x1f, 0x00, 0x00, // Sample Rate,  = 8.0kHz
    0x00, 0x7d, 0x00, 0x00, // Sample Rate,  = 16.0kHz
    0x02, 0x00,             // BlockAlign == NumChannels * BitsPerSample/8
    0x10, 0x00     // BitsPerSample  PCM_16
};
unsigned char RIFFHeader504[] = {
    'd' , 'a' , 't' , 'a' , // Chunk ID (data)
    0x70, 0x70, 0x70, 0x70  // Chunk payload size (calculate after rec!)
};

void pcm2wav(const char *pcm_file, const char *wav_file) {
    FILE *fpi,*fpo;
    unsigned long iLen,temp;
    unsigned long i = 0;
    unsigned long j;
    int headflag = -1;
	
	char *savedata;

    fpi=fopen(pcm_file,"rb");
    if(fpi==NULL) {
        printf("\nread error!\n");
        printf("\n%ld\n",i);
        exit(0);
    }
    fseek(fpi,0,SEEK_END);
    temp = ftell(fpi);
    printf("temp:%lu\n", temp);
	savedata = new char [temp];

    fpo=fopen(wav_file,"w+");
    if(fpo==NULL) {
        printf("\nwrite error!\n");
        exit(0);
    }
    fseek(fpo,0,SEEK_SET);
    fwrite(RiffHeader,sizeof(RiffHeader),1,fpo);
    fwrite(RIFFHeader504,sizeof(RIFFHeader504),1,fpo);
    fseek(fpi,0,SEEK_SET);
    fread(savedata,1,temp,fpi);
    fwrite(savedata,temp,1,fpo);

    // ChunkSize
    RiffHeader[4] = (unsigned char)((36 + temp)&0x000000ff);
    RiffHeader[5] = (unsigned char)(((36 + temp)&0x0000ff00)>>8);
    RiffHeader[6] = (unsigned char)(((36 + temp)&0x00ff0000)>>16);
    RiffHeader[7] = (unsigned char)(((36 + temp)&0xff000000)>>24);
    fseek(fpo,4,SEEK_SET);
    fwrite(&RiffHeader[4],4,1,fpo);

    RIFFHeader504[4] = (unsigned char)(temp&0x000000ff);
    RIFFHeader504[5] = (unsigned char)((temp&0x0000ff00)>>8);
    RIFFHeader504[6] = (unsigned char)((temp&0x00ff0000)>>16);
    RIFFHeader504[7] = (unsigned char)((temp&0xff000000)>>24);
    fseek(fpo,40,SEEK_SET);
    fwrite(&RIFFHeader504[4],4,1,fpo);
    fclose(fpi);
    fclose(fpo);
	delete[] savedata;
    printf("\n==========================Done=========================\n");
}

void data_callback(BluetoothGattCharacteristic &c, std::vector<unsigned char> &data, void *userdata) {
    //cout << "Entern callback" << endl;
    unsigned char *data_c;
    unsigned char buf[MIC_ADPCM_FRAME_SIZE] = {0};
    unsigned int size = data.size();

    if(state == IN_ASR) {
        return;
    }

    ntime++;
  
    if(size > 0) {
        data_c = data.data();
        //memcpy(buf, data_c, 128);
    }
    if(ntime == 1) {
#if 1
        cout << "now open test.adpcm " << endl;
        ofile.open(filename_adpcm, ios::out | ios::binary | ios::trunc);
        if (ofile.is_open()) {
            std::cout << " ofile open success! " << endl;
        } else
            std::cout << " fatal error!!! ofile open failed! " << endl;
#endif
        short i;
        for(i = 0; i < MIC_ADPCM_FRAME_SIZE; i++) {
            ofile << *(data_c + i);
        }
    } else {
        short i;
        for(i = 0; i < MIC_ADPCM_FRAME_SIZE; i++) {
            ofile << *(data_c + i);
        }
    }
    if (size > 0) {
#if 0
        std::cout << "Raw data=[";
        for (unsigned i = 0; i < size; i++)
            std::cout << std::hex << static_cast<int>(data_c[i]) << ", ";
        std::cout << "] ";
#endif

#if 1
        tick.it_value.tv_sec = 0;
        tick.it_value.tv_usec = 1000*200;
        //After first, the Interval time for clock
        tick.it_interval.tv_sec = 0;
        tick.it_interval.tv_usec = 0;
        if(setitimer(ITIMER_REAL, &tick, NULL) < 0) {
            printf("Set timer failed!\n");
        }
#endif
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        cv.notify_all();
    }
}

void timer_func(int signo) {
#if 1
    state = IN_ASR;
    ofile.close();
    usleep(10);
    long start, end, size;
    char * buff;
    char * audio;
    //ifstream ifile(filename_adpcm, ios::in | ios::binary | ios::ate);
    ifile.open(filename_adpcm, ios::in | ios::binary | ios::ate);
    if(ifile.is_open())
        printf("zewen---> [FUNC]%s [LINE]:%d ifile open success\n", __FUNCTION__, __LINE__);
    else
        printf("zewen---> [FUNC]%s [LINE]:%d ifile open failed\n", __FUNCTION__, __LINE__);
    size = ifile.tellg();
    cout<<"file size: " << size << " bytes " << size/MIC_ADPCM_FRAME_SIZE <<  " ntime: " << ntime<< endl;
    ifile.seekg(0, ios::beg);

    buff = new char [size];
    audio = new char [MIC_PCM_BUF_SIZE];
    ifile.read(buff, size);
    ifile.close();

    //printf("now print buff\n");
    int ii = 1;
    outfile.open(filename_pcm, ios::out | ios:: binary | ios::trunc);
    if(outfile.is_open())
        printf("zewen---> [FUNC]%s [LINE]:%d outfile open success\n", __FUNCTION__, __LINE__);
    else
        printf("zewen---> [FUNC]%s [LINE]:%d outfile open failed\n", __FUNCTION__, __LINE__);
    int i = 0;
    while(i < size/MIC_ADPCM_FRAME_SIZE) {
        adpcm_to_pcm((signed short *)(buff + i * MIC_ADPCM_FRAME_SIZE), (signed short *)(audio), MIC_SHORT_DEC_SIZE );
        //printf("now print audio\n");
        ii = 1;
        for(; ii <= MIC_PCM_BUF_SIZE;  ii++) {
            outfile << *(audio + ii-1);
        }
        i++;
    }
    outfile.close();

    pcm2wav(filename_pcm, filename_wav);

    delete[] buff;
    delete[] audio;
    ntime = 0;
    state = NOT_IN_ASR;
#endif
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Run as: " << argv[0] << " <device_address>" << std::endl;
        exit(1);
    }

	system("sudo hciconfig hci0 down");
	sleep(1);
	system("sudo hciconfig hci0 up");
	sleep(1);
	return 0;
    signal(SIGALRM, timer_func);
    memset(&tick, 0, sizeof(tick));

    BluetoothManager *manager = nullptr;
    try {
        manager = BluetoothManager::get_bluetooth_manager();
    } catch(const std::runtime_error& e) {
        std::cerr << "Error while initializing libtinyb: " << e.what() << std::endl;
        exit(1);
    }

    /* Start the discovery of devices */
    bool ret = manager->start_discovery();
    std::cout << "Started = " << (ret ? "true" : "false") << std::endl;

    std::unique_ptr<BluetoothGattService> voice_service;

    std::string device_mac(argv[1]);
    auto rcu = manager->find<BluetoothDevice>(nullptr, &device_mac, nullptr, std::chrono::seconds(10));
    if (rcu == nullptr) {
        std::cout << "Device not found" << std::endl;
        return 1;
    } else {
        std::cout << "Device found" << std::endl;
    }

    rcu->enable_connected_notifications([] (BluetoothDevice &d, bool connected, void *usedata)
            { if (connected) std::cout << "Connected " << d.get_name() << std::endl;  }, NULL);
#if 0
    rcu->enable_trusted_notifications([] (BluetoothDevice &d, bool connected, void *usedata)
        { if (connected) std::cout << "Trusted " << d.get_name() << std::endl;  }, NULL);

    rcu->enable_paired_notifications([] (BluetoothDevice &d, bool connected, void *usedata)
        { if (connected) std::cout << "Paired " << d.get_name() << std::endl;  }, NULL);
#endif

    if (rcu != nullptr) {
        /* Connect to the device and get the list of services exposed by it */
        std::cout << "Will connect the device" << std::endl;
        rcu->connect();
        std::cout << "connected the device" <<  std::endl;

        std::cout << "Discovered services: " << std::endl;
        while (true) {
            /* Wait for the device to come online */
            std::this_thread::sleep_for(std::chrono::seconds(5));

            auto list = rcu->get_services();
            if (list.empty()) {
                std::cout << "not found services, retry" << std::endl;
                continue;
            }

            auto it = list.begin();
            for (; it != list.end(); ++it) {
                std::cout << "Class = " << (*it)->get_class_name() << " ";
                std::cout << "Path = " << (*it)->get_object_path() << " ";
                std::cout << "UUID = " << (*it)->get_uuid() << " ";
                std::cout << "Device = " << (*it)->get_device().get_object_path() << " ";
                //std::cout << "Appearance = " << (*it)->get_appearance() << " ";
                std::cout << std::endl;

                /* Search for the temperature service, by UUID */
                //if ((*it)->get_uuid() == "00010203-0405-0607-0809-0a0b0c0d1910")
                    //temperature_service = (*it).release();

                auto chars = (*it)->get_characteristics();
                for (auto it_char = chars.begin(); it_char != chars.end(); ++it_char) {
                    std::cout << "Class = " << (*it_char)->get_class_name() << " ";
                    std::cout << "Path = " << (*it_char)->get_object_path() << " ";
                    std::cout << "UUID = " << (*it_char)->get_uuid() << " ";
                    std::vector<std::string> flags = (*it_char)->get_flags();
                    //std::cout << "Flags = " << (*it_char)->get_flags() << " ";
                    std::cout << std::endl;
                }
                std::cout << "-------------------------------------" << std::endl;
                std::cout << std::endl << std::endl;
            }
            break;
        }

        std::string service_uuid("00010203-0405-0607-0809-0a0b0c0d1911");
        std::cout << "Waiting for service " << service_uuid ;
        voice_service = rcu->find(&service_uuid);
        std::cout << "  ... discovered" << std::endl;
    } else {
        ret = manager->stop_discovery();
        std::cerr << "RCU not found after 30 seconds, exiting" << std::endl;
        return 1;
    }

    /* Stop the discovery (the device was found or timeout was over) */
    ret = manager->stop_discovery();
    std::cout << "Is discovery ? Stopped = " << (ret ? "true" : "false") << std::endl;
    std::cout << sizeof(unsigned char) << " " << sizeof(signed short) << " " << sizeof(int) << std::endl;
    auto value_uuid = std::string("00010203-0405-0607-0809-0a0b0c0d2b18");
    auto temp_value = voice_service->find(&value_uuid);
    if(nullptr == temp_value ) {
        cout << "Not found the uuid" + value_uuid << endl;
    } else {
        cout << "Found the uuid"  + value_uuid << endl;
    }
    std::cout << "set callback" << std::endl;
    temp_value->enable_value_notifications(data_callback, nullptr);

    std::mutex m;
    std::unique_lock<std::mutex> lock(m);

    std::signal(SIGINT, signal_handler);
    cv.wait(lock);

    /* Disconnect from the device */
    if (rcu != nullptr)
        rcu->disconnect();
}

#define NUM_OF_ORIG_SAMPLE    2
static const signed char idxtbl[] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static const unsigned short steptbl[] = {
    7,  8,  9,  10,  11,  12,  13,  14,  16,  17,
    19,  21,  23,  25,  28,  31,  34,  37,  41,  45,
    50,  55,  60,  66,  73,  80,  88,  97,  107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767   };


////////////////////////////////////////////////////////////////////
/*  name ADPCM to pcm
    signed short *ps -> pointer to the adpcm source buffer
    signed short *pd -> pointer to the pcm destination buffer
    int len          -> decorded size
*/
void adpcm_to_pcm (signed short *ps, signed short *pd, int len){
    int i;
    //byte2,byte1: predict;  byte3: predict_idx; byte4:adpcm data len
    int predict = ps[0];
    int predict_idx = ps[1] & 0xff;
    int adpcm_len = (ps[1]>>8) & 0xff;
    unsigned char *pcode = (unsigned char *) (ps + NUM_OF_ORIG_SAMPLE);
    unsigned char code;
    code = *pcode ++;
    //byte5- byte128: 124 byte(62 sample) adpcm data
    for (i=0; i<len; i++) {
        if (1) {
            int step = steptbl[predict_idx];
            int diffq = step >> 3;
            if (code & 4) {
                diffq = diffq + step;
            }
            step = step >> 1;
            if (code & 2) {
                diffq = diffq + step;
            }
            step = step >> 1;
            if (code & 1) {
                diffq = diffq + step;
            }
            if (code & 8) {
                predict = predict - diffq;
            } else {
                predict = predict + diffq;
            }
            if (predict > 32767) {
                predict = 32767;
            } else if (predict < -32767) {
                predict = -32767;
            }
            predict_idx = predict_idx + idxtbl[code & 15];
            if(predict_idx < 0) {
                predict_idx = 0;
            } else if(predict_idx > 88) {
                predict_idx = 88;
            }
            if (i&1) {
                code = *pcode ++;
            } else {
                code = code >> 4;
            }
        }
        if (0 && i < NUM_OF_ORIG_SAMPLE) {
            *pd++ = ps[i];
        } else {
            *pd++ = predict;
        }
    }
}
