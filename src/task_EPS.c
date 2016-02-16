  /*
 * Battery CCV.c
 *
 *  Created on: 	2016/02/02
 *  Last update:	2016/02/02
 *      Author: Kenny Huang, Eddie Yeh
 */
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <util/hexdump.h>
#include <util/timestamp.h>
#include <csp/csp_endian.h>
#include <dev/i2c.h>
#include <nanomind.h>
#include <fat_sd/ff.h>
#include <vfs/vfs.h>
#include "parameter.h"
#include "tele_function.h"
#include "subsystem.h"
#include "fs.h"

 
FATFS fs[2];
FRESULT res;
FIL file;
UINT br, bw;
FILINFO *fno;
int cleanfile = 0;
char fileName1[30];
extern int eps_write(uint8_t *frameCont, int choose);

void EPS_Task(void * pvParameters) 
{
	uint8_t uchar_eps[12]; // neccessary
	uint8_t rxbuf[131]; //total housekeeping data
	uint8_t txbuf[2];
	txbuf[0] = 0x08; 			//ID = 8 EPS HK port
	txbuf[1] = 0x00; 			
	int i = 0, delay=0, period = 0,rate[60],j = 0;
	char choose;
	int ccv ;
	uint16_t vboost3,vbatt=0,current_in=0,current_out,btemp,cursun=0;
	char time_unit[4];
	// clear all memery space
	for (i = 0 ; i < 12 ; i++)
		uchar_eps[i] = 0;
	// uint32_t seconds[] = {0};
	
	printf("a.Record the closed circuit voltage \n");
	printf("b. Charge cycle \n");
	printf("d. Overcharge cycle \n");
	printf("Point which test ");
	scanf("%c", &choose);

	for (i = 0; i < 60; i ++)
		rate[i] = 0;

	i = 0;
	switch (choose)
	{
	case 'a' :
		delay = 1 * delay_time_based;
		ccv = 30;
		strcpy(time_unit, "sec");
		break;
	case 'b':
		printf("Specify the delay time \n");
		scanf("%d", &delay);
		delay = delay * delay_time_based;
		strcpy(time_unit, "min");
		break;
	case 'd':
		delay = 1 * delay_time_based;
		strcpy(time_unit, "sec");
		for (i = 0; i < 60; i++)
			rate[i] = 0;
		i = 0;
		break;
	}
		
	while (1)
	{
		printf(" %d %s \n", i,time_unit);
		i++;
		if (i2c_master_transaction_2(0, eps_node, &txbuf, 2, &rxbuf, 133, eps_delay) == E_NO_ERR)
		{
			memcpy(&uchar_eps[0], &rxbuf[6], 2); // vboost
			memcpy(&uchar_eps[2], &rxbuf[8], 2); //vbatt
			memcpy(&uchar_eps[4], &rxbuf[14], 2); //current in
			memcpy(&uchar_eps[6], &rxbuf[16], 2); //current from boost converters
			memcpy(&uchar_eps[8], &rxbuf[18], 2); //current out
			memcpy(&uchar_eps[10], &rxbuf[122], 2); // battery temperature

			//hex_dump(uchar_eps, 12);		//printf data

			// arrange the right format 
			vboost3 = uchar_eps[0];
			vboost3 = (vboost3 << 8) + uchar_eps[1];

			vbatt = uchar_eps[2];
			vbatt = (vbatt << 8) + uchar_eps[3];

			current_in = uchar_eps[4];
			current_in = (current_in << 8) + uchar_eps[5];

			cursun = uchar_eps[6];
			cursun = (cursun << 8) + uchar_eps[7];

			current_out = uchar_eps[8];
			current_out = (current_out << 8) + uchar_eps[9];

			btemp = uchar_eps[10];
			btemp = (btemp << 8) + uchar_eps[11];

			printf("Input voltage: %" PRIu16 "\n", vboost3);
			printf("Battery voltage: %" PRIu16 "\n", vbatt);
			printf("Input current: %" PRIu16 "\n", current_in);
			printf("Input battery current: %" PRIu16 "\n", cursun);
			printf("Output current: %" PRIu16 "\n", current_out);
			printf("Battery temperature: %" PRIu16 "\n", btemp);

			eps_write(uchar_eps,choose);			//write into SD card
			vTaskDelay(delay);
		}
		else 
			printf("Error, cannot communicate with EPS\n");

		if (i == ccv && choose == 97) // judge the charge current
			break;

		if (choose == 98 && cursun < 100) // wherether does the circumstance of the lower current maintent over 3 mins
			period++;
		if (choose == 98 && cursun >= 100)
			period = 0;

		if (choose == 100) //overcharge judge mechamism
		{
			if ((vbatt - rate[j]) >= 0.01*rate[j]) //wherether does the circumstance change
				period = 0;				
			else
				period++;

			printf("Vbatt: %d rate[%d}: %d period: %d \n", vbatt, j, rate[j], period);
			rate[j] = vbatt;
			j++;

			if (j == 60)
				j = 0;
		}
			
		
		if (period > 3 && choose == 98) // trigger the warning signal for charge
		{
			delay = 10 * delay_time_based;
			break;
		}

		if (period > 180 && choose == 100) // trigger the warning signal for overcharge
		{
			delay = 10 * delay_time_based;
			break;
		}

	}
	
	if (period > 3 ) //warning signal
		for (i = 0; i < 60; i++)
		{
			printf(" rest time : %d sec \n",i*10);
			vTaskDelay(delay);
		}
	/* End of init */
	vTaskDelete(NULL);
}

int eps_write(uint8_t *frameCont, int choose)
{		
	if (cleanfile == 0)
	{
		if (choose == 97)
		{
		f_unlink("Battery/battery_a.bin");
		strcpy(fileName1, "Battery/battery_a.bin");
		}
		if (choose == 98)
		{
		f_unlink("Battery/battery_b.bin");
		strcpy(fileName1, "Battery/battery_b.bin");
		}
		if (choose == 100)
		{
		f_unlink("Battery/battery_d.bin");
		strcpy(fileName1, "Battery/battery_d.bin");
		}
	}
	cleanfile = 1;

	//write into file	
	res = f_open(&file, fileName1, FA_OPEN_ALWAYS | FA_WRITE );
	if (res != FR_OK)
	{
		printf("open fail .. \n");
		f_close(&file);
		return Error;
	}
	f_lseek(&file, file.fsize);
	res = f_write(&file, &frameCont[0], 2, &bw);
	if (res != FR_OK)
	{
		printf("write fail .. \n");
		f_close(&file);
		return Error;
	}

	res = f_write(&file, &frameCont[2], 2, &bw);
	res = f_write(&file, &frameCont[4], 2, &bw);
	res = f_write(&file, &frameCont[6], 2, &bw);
	res = f_write(&file, &frameCont[8], 2, &bw);
	res = f_write(&file, &frameCont[10], 2, &bw);
	f_close(&file);

	return No_Error;	
}	