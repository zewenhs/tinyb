/*
 * Author: Petre Eftime <petre.p.eftime@intel.com>
 * Copyright (c) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

using namespace tinyb;
using namespace std;
using namespace chrono;
std::condition_variable cv;

ofstream ofile;
int ntime = 0;
int ntime_b = 0;
int in_noti = 0;
struct itimerval tick;
const char * filename_adpcm = "test.adpcm";
const char * filename_pcm = "test.pcm";

void adpcm_to_pcm (signed short *ps, signed short *pd, int len);

/** Converts a raw temperature read from the sensor to a Celsius value.
 * @param[in] raw_temp The temperature read from the sensor (two bytes)
 * @return The Celsius value of the temperature
 */
static float celsius_temp(uint16_t raw_temp)
{
    const float SCALE_LSB = 0.03125;
    return ((float)(raw_temp >> 2)) * SCALE_LSB;
}

string fa_getSysTime()  
{  
     struct timeval tv;  
   
     gettimeofday(&tv,NULL);  
     struct tm* pTime;  
     pTime = localtime(&tv.tv_sec);  
          
     char sTemp[30] = {0};  
     snprintf(sTemp, sizeof(sTemp), "%04d%02d%02d%02d%02d%02ds%03d us%03d", pTime->tm_year+1900, \  
            pTime->tm_mon+1, pTime->tm_mday, pTime->tm_hour, pTime->tm_min, pTime->tm_sec, \  
            tv.tv_usec/1000,tv.tv_usec%1000);  
     return (string)sTemp;  
}  

void data_callback(BluetoothGattCharacteristic &c, std::vector<unsigned char> &data, void *userdata)
{
      /* Read temperature data and display it */
     unsigned char *data_c;
     //char *data_c;
     unsigned int size = data.size();
	  unsigned char buf[128] = {0};
     static int itime;
     char *data_d  = "abcdefg";
	 in_noti = 1;
	 ntime++;
     itime++;
     if(itime == 5)
     {
		cout<< "5: 当前时间：" << fa_getSysTime() << endl;  
     }
     
     if(itime == 6)
     {
		cout<< "6: 当前时间：" << fa_getSysTime() << endl;  
		//itime = 0;
   	 }
	 if(size > 0)
    	 data_c = data.data();
	memcpy(buf, data_c, 128);
 if(ntime == 1)	 
 { 
	 cout << "now open test.sdpcm " << endl;
	ofile.open("test.adpcm", ios::out | ios::binary | ios::trunc);
	if(ofile.is_open())
	{
         std::cout << " ofile open success! " << endl;
	
	}else
         std::cout << " ofile open failed! " << endl;
printf("zewen1:%x %x %x \n", *data_c, *(data_c + 1), *(data_c + 2));
	short i;
	for(i = 0; i < 128; i++)
	{
		ofile << *(data_c + i);
	}
	//ofile << buf; 
	//ofile << data_d; 
 }else if(ntime == 101)
 {
 	cout << "now close ofile!!!" << endl;
	ofile.close();
 }else if (ntime <101 && ntime > 1)
 {
	 //printf("itime:%d\n", itime);
short i;
	for(i = 0; i < 128; i++)
	{
		ofile << *(data_c + i);
	}
	printf("zewen2:%x %x %x \n", *data_c, *(data_c + 1), *(data_c + 2));
	//ofile << data_c;
 }else{
 	printf("zewen---> [FUNC]%s [LINE]:%d\n", __FUNCTION__, __LINE__);
	return;
 }
//	ofile.close();	
     
     std::cout << "now in callback size: " << size << " ntime: "<< ntime<<std::endl;
     if (size > 0) {
      //   data_c = data.data();
#if 0
         std::cout << "Raw data=[";
         for (unsigned i = 0; i < size; i++)
             std::cout << std::hex << static_cast<int>(data_c[i]) << ", ";
         std::cout << "] ";

         uint16_t ambient_temp, object_temp;
         object_temp = data_c[0] | (data_c[1] << 8);
         ambient_temp = data_c[2] | (data_c[3] << 8);

        // std::cout << "Ambient temp: " << celsius_temp(ambient_temp) << "C ";
         //std::cout << "Object temp: " << celsius_temp(object_temp) << "C ";
         std::cout << std::endl;

#endif 
    }
}

void signal_handler(int signum)
{
    if (signum == SIGINT) {
        cv.notify_all();
    }
}

#define NOT_IN_ASR 0
#define IN_ASR 1
#define MIC_ADPCM_FRAME_SIZE    128
#define MIC_SHORT_DEC_SIZE    248
void timer_func(int signo)
{
static short state = NOT_IN_ASR;
	if(state == NOT_IN_ASR)
	{
		if(in_noti)
		{
			if(ntime_b != ntime)
			{
				printf("zewen---> [FUNC]%s [LINE]:%d ntime is not equal to ntime_b\n", __FUNCTION__, __LINE__);
				ntime_b = ntime;
			}
			else
			{
				printf("zewen---> [FUNC]%s [LINE]:%d ntime is equal to ntime_b %d\n", __FUNCTION__, __LINE__, ntime);
				in_noti = 0;
				ntime = 0;
				ntime_b = 0;
				ofile.close();
				//Timeout to run first time
				tick.it_value.tv_sec = 0;
				tick.it_value.tv_usec = 100;
				//After first, the Interval time for clock
        			tick.it_interval.tv_sec = 0;
				tick.it_interval.tv_usec = 0;
				if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
				 	printf("Set timer failed!\n");
				state = IN_ASR;
			}
		}else
		{
			printf("zewen---> [FUNC]%s [LINE]:%d not in notify\n", __FUNCTION__, __LINE__);
		}
	}else{
		printf("zewen---> [FUNC]%s [LINE]:%d now need decode to pcm\n", __FUNCTION__, __LINE__);
#if 0
			long start, end, size;
			char * buff;
			char * audio;
			ifstream ifile(filename_adpcm, ios::in | ios::binary | ios::ate);
			 size = ifile.tellg();
			//ifile.seekg (0, ios::end); 
			//end = ifile.tellg();
			cout<<"file size: " << size << " bytes " << size/MIC_ADPCM_FREAME_SIZE << endl;
			ifile.seekg(0, ios::beg);

			buff = new char [size];
			audio = new char [size * 4];
			ifile.read(buff, size);
			ifile.close();
			
			int i = 0;
			while(i < size/MIC_ADPCM_FRAME_SIZE)
			{
				//usigned short *ps = (unsigned short *)buff + i * MIC_ADPCM_FRAME_SIZE;
				adpcm_to_pcm((signed short *)(buff + i * MIC_ADPCM_FRAME_SIZE), (signed short *)(audio + i * MIC_SHORT_DEC_SIZE), MIC_SHORT_DEC_SIZE );
				i++;
			}
			ofstream outfile(filename_pcm, ios::out | ios:: binary | ios::trunc);
			outfile << audio;
			outfile.close();

			delete[] buff;
			delete[] audio;
				//Timeout to run first time
				tick.it_value.tv_sec = 0;
				tick.it_value.tv_usec = 1000*100;
				//After first, the Interval time for clock
        		tick.it_interval.tv_sec = 0;
				tick.it_interval.tv_usec = 1000*50;
				if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
				 	printf("Set timer failed!\n");
				state = NOT_IN_ASR;
#endif	
	}
}	

/** This program reads the temperature from a
 * TI Sensor Tag(http://www.ti.com/ww/en/wireless_connectivity/sensortag2015/?INTC=SensorTag&HQS=sensortag)
 * Pass the MAC address of the sensor as the first parameter of the program.
 */
int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Run as: " << argv[0] << " <device_address>" << std::endl;
        exit(1);
    }

	    signal(SIGALRM, timer_func);
		memset(&tick, 0, sizeof(tick));
		//Timeout to run first time
		tick.it_value.tv_sec = 1;
		tick.it_value.tv_usec = 0;
		//After first, the Interval time for clock
        tick.it_interval.tv_sec = 1;
		tick.it_interval.tv_usec = 1000*50;

		if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
			 printf("Set timer failed!\n");

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

    std::unique_ptr<BluetoothGattService> temperature_service;

	std::cout<<"111111"<<std::endl;
    std::string device_mac(argv[1]);
    auto sensor_tag = manager->find<BluetoothDevice>(nullptr, &device_mac, nullptr, std::chrono::seconds(10));
    if (sensor_tag == nullptr) {
        std::cout << "Device not found" << std::endl;
        return 1;
    }
	std::cout<<"2222"<<std::endl;
	//std::cout<<"now while 1"<<std::endl;
	//while(1);
    sensor_tag->enable_connected_notifications([] (BluetoothDevice &d, bool connected, void *usedata)
        { if (connected) std::cout << "Connected " << d.get_name() << std::endl;  }, NULL);

	std::cout<<"3333"<<std::endl;
	//std::cout<<"now while 1"<<std::endl;
    if (sensor_tag != nullptr) {
        /* Connect to the device and get the list of services exposed by it */
        sensor_tag->connect();
	std::cout<<"4444"<<std::endl;
        //std::string service_uuid("f000aa00-0451-4000-b000-000000000000");
       // std::string service_uuid("0000180f-0000-1000-8000-00805f9b34fb");//BATTERY SERVICE
        std::string service_uuid("00010203-0405-0607-0809-0a0b0c0d1911");
        std::cout << "Waiting for service " << service_uuid << "to be discovered" << std::endl; 
        temperature_service = sensor_tag->find(&service_uuid);
	std::cout<<"6666"<<std::endl;
    } else {
       ret = manager->stop_discovery();
       std::cerr << "SensorTag not found after 30 seconds, exiting" << std::endl;
       return 1;
    }

	std::cout<<"8888"<<std::endl;
	//std::cout<<"now while 1"<<std::endl;
    /* Stop the discovery (the device was found or timeout was over) */
    ret = manager->stop_discovery();
    std::cout << "Stopped = " << (ret ? "true" : "false") << std::endl;
	std::cout << sizeof(unsigned char) << " " << sizeof(signed short) << " " << sizeof(int) << std::endl;
	std::cout << 0%992 << std::endl;
	std::cout << 10%992 << std::endl;
	std::cout << 992%992 << std::endl;
	std::cout << 32%992 << std::endl;
    //auto value_uuid = std::string("f000aa01-0451-4000-b000-000000000000");
    //auto value_uuid = std::string("00002a19-0000-1000-8000-00805f9b34fb");//battery
    auto value_uuid = std::string("00010203-0405-0607-0809-0a0b0c0d2b18");
    auto temp_value = temperature_service->find(&value_uuid);
#if 0
    auto config_uuid = std::string("f000aa02-0451-4000-b000-000000000000");
    auto temp_config = temperature_service->find(&config_uuid);

    auto period_uuid = std::string("f000aa03-0451-4000-b000-000000000000");
    auto temp_period = temperature_service->find(&period_uuid);


    /* Activate the temperature measurements */
    std::vector<unsigned char> config_on {0x01};
    temp_config->write_value(config_on);
    temp_period->write_value({100});
#endif
	std::cout << "set callback" << std::endl; 
    temp_value->enable_value_notifications(data_callback, nullptr);

    std::mutex m;
    std::unique_lock<std::mutex> lock(m);

    std::signal(SIGINT, signal_handler);

    cv.wait(lock);

    /* Disconnect from the device */
    if (sensor_tag != nullptr)
        sensor_tag->disconnect();
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
			}
			else {
				predict = predict + diffq;
			}

			if (predict > 32767) {
				predict = 32767;
			}
			else if (predict < -32767) {
				predict = -32767;
			}

			predict_idx = predict_idx + idxtbl[code & 15];

			if(predict_idx < 0) {
				predict_idx = 0;
			}
			else if(predict_idx > 88) {
				predict_idx = 88;
			}

			if (i&1) {
				code = *pcode ++;
			}
			else {
				code = code >> 4;
			}
		}

		if (0 && i < NUM_OF_ORIG_SAMPLE) {
			*pd++ = ps[i];
		}
		else {
			*pd++ = predict;
		}
	}
}

