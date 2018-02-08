/****
 *compile:
 *g++ -o ota_test ota_test.cpp --std=c++11 -ltinyb
 * *****/
#include <tinyb.hpp>

#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

#include <fstream>
#include <unistd.h>
#include <string.h>

using namespace tinyb;
using namespace std;

typedef unsigned char u8 ;
typedef signed char s8;

typedef unsigned short u16;
typedef signed short s16;

typedef int s32;
typedef unsigned int u32;

typedef long long s64;
typedef unsigned long long u64;


typedef struct{
	u16 adr_index;
	u8	data[16];
	u16 crc_16;
}rf_packet_att_ota_data_t;

unsigned short crc16 (unsigned char *pD, int len)
{

    static unsigned short poly[2]={0, 0xa001};              //0x8005 <==> 0xa001
    unsigned short crc = 0xffff;
    unsigned char ds;
    int i,j;

    for(j=len; j>0; j--)
    {
        unsigned char ds = *pD++;
        for(i=0; i<8; i++)
        {
            crc = (crc >> 1) ^ poly[(crc ^ ds ) & 1];
            ds = ds >> 1;
        }
    }

     return crc;
}

std::atomic<bool> running(true);

void signal_handler(int signum)
{
    if (signum == SIGINT) {
        running = false;
    }
}

#define FIRMWARE_NAME	"8267_remote.bin"
#define CMD_OTA_START		0xff01
#define CMD_OTA_END		0xff02
#define OTA_BUF_LEN		20
#define OTA_DAT_LEN		16
#define OTA_CRC_LEN		18

#define TELINK_OTA_UUID_SERVICE			"00010203-0405-0607-0809-0a0b0c0d1912"
#define TELINK_SPP_DATA_OTA				"00010203-0405-0607-0809-0a0b0c0d2b12"


int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Run as: " << argv[0] << " <device_address>" << std::endl;
        exit(1);
    }

	long size;
	char * fwbuf;
	ifstream ifile(FIRMWARE_NAME, ios::in | ios::binary | ios::ate);
	if(ifile.is_open())
		printf("firmware open successfully!!!\n");
	else
	{
		printf("zewen---> [FUNC]%s [LINE]:%d ifile open failed!!!, please check if there have firmware file in current directory, exit!!!\n", __FUNCTION__, __LINE__);
		return -1;
	}

	size = ifile.tellg();
	ifile.seekg(0, std::ios::beg);
	fwbuf = new char [size];
	ifile.read(fwbuf, size);

	ifile.close();

    BluetoothManager *manager = nullptr;
    try {
        manager = BluetoothManager::get_bluetooth_manager();
    } catch(const std::runtime_error& e) {
        std::cerr << "Error while initializing libtinyb: " << e.what() << std::endl;
        delete[] fwbuf;
		exit(1);
    }

    /* Start the discovery of devices */
    bool ret = manager->start_discovery();
    std::cout << "Started discovery device = " << (ret ? "true" : "false") << std::endl;

    BluetoothDevice *tl_remote = NULL;
    BluetoothGattService *ota_service = NULL;

    for (int i = 0; i < 15; ++i) {
       // std::cout << "Discovered devices: " << std::endl;
        /* Get the list of devices */
        auto list = manager->get_devices();

        for (auto it = list.begin(); it != list.end(); ++it) {

        //    std::cout << "Class = " << (*it)->get_class_name() << " ";
          //  std::cout << "Path = " << (*it)->get_object_path() << " ";
            //std::cout << "Name = " << (*it)->get_name() << " ";
            //std::cout << "Connected = " << (*it)->get_connected() << " ";
            //std::cout << std::endl;

            /* Search for the device with the address given as a parameter to the program */
            if ((*it)->get_address() == argv[1])
                tl_remote = (*it).release();
        }

        /* Free the list of devices and stop if the device was found */
        if (tl_remote != nullptr)
            break;
        /* If not, wait and try again */
        std::this_thread::sleep_for(std::chrono::seconds(4));
        std::cout << std::endl;
    }

    /* Stop the discovery (the device was found or number of tries ran out */
    ret = manager->stop_discovery();
	std::this_thread::sleep_for(std::chrono::seconds(2));
    //std::cout << "Stopped = " << (ret ? "true" : "false") << std::endl;

    if (tl_remote == nullptr) {
        std::cout << "Could not find device " << argv[1] << std::endl;
     	delete[] fwbuf;
		return -1;
    }

    /* Connect to the device and get the list of services exposed by it */
    std::cout << "now connect to device ..." << std::endl;
    tl_remote->connect();
    //std::cout << "Discovered services: " << std::endl;
    std::cout << "now get service:" << std::endl;
    while (true) {
        /* Wait for the device to come online */
        std::this_thread::sleep_for(std::chrono::seconds(4));

        auto list = tl_remote->get_services();
        if (list.empty())
            continue;

        for (auto it = list.begin(); it != list.end(); ++it) {
            //std::cout << "Class = " << (*it)->get_class_name() << " ";
            //std::cout << "Path = " << (*it)->get_object_path() << " ";
            //std::cout << "UUID = " << (*it)->get_uuid() << " ";
            //std::cout << "Device = " << (*it)->get_device().get_object_path() << " ";
            //std::cout << std::endl;

            /* Search for the temperature service, by UUID */
            if ((*it)->get_uuid() == TELINK_OTA_UUID_SERVICE)    
                ota_service = (*it).release();
        }
        break;
    }

    if (ota_service == nullptr) {
        std::cout << "Could not find OTA service" << std::endl;
       	delete[] fwbuf;
		return -1;
    }
	
   	BluetoothGattCharacteristic *ota_char = nullptr;

    auto list = ota_service->get_characteristics();
    //std::cout << "Discovered characteristics: " << std::endl;
    std::cout << "now get characteristics:" << std::endl;
    for (auto it = list.begin(); it != list.end(); ++it) {

       // std::cout << "Class = " << (*it)->get_class_name() << " ";
        //std::cout << "Path = " << (*it)->get_object_path() << " ";
        //std::cout << "UUID = " << (*it)->get_uuid() << " ";
        //std::cout << "Service = " << (*it)->get_service().get_object_path() << " ";
        //std::cout << std::endl;

		 if((*it)->get_uuid() == TELINK_SPP_DATA_OTA)
            ota_char = (*it).release();
    }

    if (ota_char == nullptr) {
        std::cout << "Could not find OTA characteristics." << std::endl;

    }

#if 1
	int ii, jj;
	u32 ota_adr = 0;
	int ota_start[2] = {0x01, 0xff};
	int ota_end[2] = {0x02, 0xff};
	u8 adr_index_max_l;
	u8 adr_index_max_h;
	u8 ota_buf[OTA_BUF_LEN];
	rf_packet_att_ota_data_t *p = (rf_packet_att_ota_data_t *)ota_buf;
	std::vector<unsigned char> cmd_start(2, 0);
	std::vector<unsigned char> cmd_value(20, 0);
	std::vector<unsigned char> cmd_end(6, 0);

	cmd_start[0] = 0x01;
	cmd_start[1] = 0xff;
	printf("sending ota start:\n");	
	bool sucess = ota_char->write_value(cmd_start);
	if(!sucess)
	{
		printf("zewen---> [FUNC]%s [LINE]:%d write value failed!!!\n", __FUNCTION__, __LINE__);
		delete[] fwbuf;
		return -1;
	}
	usleep(20 * 1000);

	printf("sending ota data ...\n");
#if 1
	for(ii = 0; ii < size / OTA_DAT_LEN; ii++)
	{
		p->adr_index = ota_adr >> 4;
		memcpy(p->data, fwbuf + ota_adr, OTA_DAT_LEN);
		p->crc_16 = crc16((u8 *)&(p->adr_index), OTA_CRC_LEN);	

		for(jj = 0; jj < 20; jj++){
			cmd_value[jj] = *(ota_buf + jj);
		}

		sucess = ota_char->write_value(cmd_value);
		if(!sucess)
		{
			printf("zewen---> [FUNC]%s [LINE]:%d write value failed!!!\n", __FUNCTION__, __LINE__);
			delete[] fwbuf;
			return -1;
		}
	
		usleep(15 * 1000);
		
		ota_adr += 16;
	}
	//printf("%d\n", ii);
#endif 
	int rem = size % OTA_DAT_LEN;
	if(rem)
	{
		p->adr_index = ota_adr >> 4;	
		memcpy(p->data, fwbuf + ota_adr, rem);
		memset(p->data + rem, 0xff, OTA_DAT_LEN - rem);
		p->crc_16 = crc16((u8 *)&(p->adr_index), OTA_CRC_LEN);	
		
		for(jj = 0; jj < 20; jj++){
			cmd_value[jj] = *(ota_buf + jj);
			//printf("0x%02X ", cmd_value[jj]);
		}	
		sucess = ota_char->write_value(cmd_value);
		if(!sucess)
		{
			printf("zewen---> [FUNC]%s [LINE]:%d write value failed!!!\n", __FUNCTION__, __LINE__);
			delete[] fwbuf;
			return -1;
		}
	}
	if(!rem)
		ota_adr -= 1;

	p->adr_index = CMD_OTA_END;
	adr_index_max_l = ota_adr >> 4;
	adr_index_max_h = ota_adr >> 12;
	p->data[0] = adr_index_max_l;
	p->data[1] = adr_index_max_h;
	p->data[2] = ~adr_index_max_l;
	p->data[3] = ~adr_index_max_h;
	memset(p->data + 4, 0, 12);
	for(jj = 0; jj < 6; jj++){
		cmd_end[jj] = *(ota_buf + jj);
	//	printf("0x%02X ", cmd_end[jj]);	
	}
	printf("sending ota end:\n");
	sucess = ota_char->write_value(cmd_end);
	if(!sucess)
	{
		printf("zewen---> [FUNC]%s [LINE]:%d write value failed!!!\n", __FUNCTION__, __LINE__);
		delete[] fwbuf;
		return -1;
	}

	printf("\033[32mOTA update completely!!!\033[0m\n");
	
#endif

    /* Disconnect from the device */
     try {
      tl_remote->disconnect();
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}
