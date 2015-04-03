/**
 * Test code for Gomspace panels
 *
 * @author Johan De Claville Christiansen
 * Copyright 2010 GomSpace ApS. All rights reserved.
 */

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <util/console.h>
#include <dev/spi.h>
#include <dev/gyro.h>
#include <dev/lm70.h>
#include <dev/max6675.h>
#include <dev/usart.h>
//----------------------------------
#include <freertos/semphr.h>
#include <dev/i2c.h>
#include <io/nanopower2.h>
#include <fat_sd/ff.h>
#include "fs.h"
#include <csp/csp.h>
#include <csp/csp_endian.h>
#define E_NO_ERR -1
#if ENABLE_RTC
#include <util/clock.h>
#include <dev/arm/ds1302.h>
#endif
//------------------------------------
extern spi_dev_t spi_dev;

/**
 * Chip selects:
 *
 * Ax	Low		High
 * -----------------
 * A1	5		6
 * A2	3		4
 * A3	1		2
 * A4	12		13
 * A5	10		11
 * A6	8		9
 *
 * Dataflash	0
 * SD-Card		15
 */

#define GYRO_MAP_SIZE	6
#define LM70_MAP_SIZE	6
#define NAME_MAP_SIZE	6

static spi_chip_t gyro_chip[GYRO_MAP_SIZE];
static spi_chip_t lm70_chip[LM70_MAP_SIZE];
//static spi_chip_t max6675_chip;

static const char * name_map[LM70_MAP_SIZE] = {"A1", "A2", "A3", "A4", "A5", "A6"};
static int gyro_map[GYRO_MAP_SIZE] = {1, 3, 5, 8, 10, 12};
static int lm70_map[LM70_MAP_SIZE] = {2, 4, 6, 9, 11, 13};
//int max6675_cs  = 1;
typedef struct __attribute__((packed)) {
	uint32_t tv_sec;
	uint32_t tv_nsec;
} timestamp_t;
void jumpTime(struct command_context * ctx){

	timestamp_t timestamp;
	uint32_t waitingTime;
		/* Set time in lib-c */

	if (sscanf(ctx->argv[1], "%u", &waitingTime) != 1){
		printf("Should input some time");
	}
	timestamp.tv_sec+=waitingTime;
	clock_set_time(&timestamp);
}
int seuvon(struct command_context * ctx){

	  eps_output_set_single(2,1,0);
		return CMD_ERROR_NONE;
}
int seuvoff(struct command_context * ctx){

	   eps_output_set_single(2,0,0);
	    return CMD_ERROR_NONE;
}



int seuvread(struct command_context * ctx){

	  uint8_t txdata[1];
	  uint8_t val[5];
	  txdata[0]=221;

	  if ( i2c_master_transaction(0,110,0,0,&val,5,4) == E_NO_ERR) {
	        hex_dump(val,5);
	  }
	  else
	  printf("ERROR!!  Get no reply from SEUV \r\n");

	  return CMD_ERROR_NONE;
}
int i2cread(struct command_context * ctx){

	unsigned int node;
	unsigned int rx;
	i2c_frame_t * frame = csp_buffer_get(I2C_MTU);
	    if (ctx->argc != 3){
		    printf("i2c check point1 error\r\n");
			return CMD_ERROR_SYNTAX;}

		if (sscanf(ctx->argv[1], "%u", &node) != 1){
		    printf("i2c check point2 error\r\n");
			return CMD_ERROR_SYNTAX;}

		if (sscanf(ctx->argv[2], "%u", &rx) != 1){
				    printf("i2c check point2 error\r\n");
					return CMD_ERROR_SYNTAX;}


		frame->dest = node;
	//	frame->len = txlen;
		frame->len_rx = rx;

		if (i2c_receive(0, &frame, 4) != E_NO_ERR) {

		  printf("ERROR!!  Get no reply from COM \r\n");
		  return CMD_ERROR_SYNTAX;
		}
		  hex_dump(frame,10);
		  return CMD_ERROR_NONE;
}
int seuvwrite(struct command_context * ctx){
	unsigned int node;

    if (ctx->argc != 2){
	    printf("i2c check point1 error\r\n");
		return CMD_ERROR_SYNTAX;}

	if (sscanf(ctx->argv[1], "%u", &node) != 1){
	    printf("i2c check point2 error\r\n");
		return CMD_ERROR_SYNTAX;}

	 uint8_t txdata= node;
		unsigned int addra = 110;

	  if ( i2c_master_transaction(0,addra,txdata,1,0,0,4) == E_NO_ERR) {
		    printf("configured SEUV: %x\r\n",node);
	  }
	  else
	  printf("ERROR!!  Get no reply from COM \r\n");

	  return CMD_ERROR_NONE;
}

int fstestwod(struct command_context * ctx){

	     uint8_t rewod[102];
			for(int i=0;i<102;i++)
			{
				rewod[i]=0;
			}
			for (int i =1;i<=3;i++){
				 wod_read(0,rewod);  //read the data from the FS, second parameter is the day.
				hex_dump(rewod,102);
			}
			return CMD_ERROR_NONE;
}

int comhk2(struct command_context * ctx){

	 uint8_t txdata[1];
	  txdata[0] = 65;
	  uint8_t val[1];

	  if ( i2c_master_transaction(0,81, txdata,1,&val,1,10) == E_NO_ERR) {
	        hex_dump(val,1);
	  }
	  else
	  printf("ERROR!!  Get no reply from COM \r\n");

	  return CMD_ERROR_NONE;
}
int comhk(struct command_context * ctx){

	 uint8_t txdata[1];
	  txdata[0] = 26;
	  uint8_t val[16];

	  if ( i2c_master_transaction(0,80, txdata,1,&val,16,10) == E_NO_ERR) {
	        hex_dump(val,16);
	  }
	  else
	  printf("ERROR!!  Get no reply from COM \r\n");

	  return CMD_ERROR_NONE;
}


//----------------------------------------------------------
int cmd_panels_init(struct command_context *ctx) {

	int i;

	for (i = 0; i < GYRO_MAP_SIZE; i++) {
		gyro_spi_setup_cs(&spi_dev, &gyro_chip[i], gyro_map[i]);
		gyro_setup(&gyro_chip[i]);
	}

	for (i = 0; i < LM70_MAP_SIZE; i++) {
		lm70_spi_setup_cs(&spi_dev, &lm70_chip[i], lm70_map[i]);
	}

	//max6675_spi_setup_cs(&spi_dev, &max6675_chip, max6675_cs);
	return CMD_ERROR_NONE;

}

int cmd_panels_test(struct command_context *ctx) {

	while(1) {
		int i;
		if (usart_messages_waiting(USART_CONSOLE) != 0)
			break;

		spi_chip_t * chip;
		for (i = 0; i < GYRO_MAP_SIZE; i++) {
			chip = &gyro_chip[i];
			printf("\E[2KGyro %d %s Angle\t\t\t %.3f [deg]\r\n", i, name_map[i], gyro_read_angle(chip));
			printf("\E[2KGyro %d %s Rate\t\t\t %.3f [deg/sec]\r\n", i, name_map[i], gyro_read_rate(chip));
		}

		printf("\E[2K\r\n");

		for (i = 0; i < LM70_MAP_SIZE; i++) {
			chip = &lm70_chip[i];
			printf("\E[2KLM70 %d %s Temp\t\t\t %.3f [C]\r\n", i, name_map[i], lm70_read_temp(chip));
		}

		printf("\E[2K\r\n");

		//printf("\E[2KMax6675 Temp\t\t\t %.2f [C]\r\n", max6675_read_temp(&max6675_chip));

		vTaskDelay(0.30*configTICK_RATE_HZ);

		printf("\E[20A\r");

	}
	return CMD_ERROR_NONE;

}

int cmd_panels_test_cont(struct command_context *ctx) {

	while(1) {
		int i;
		if (usart_messages_waiting(USART_CONSOLE) != 0)
			break;

		spi_chip_t * chip;
		for (i = 0; i < GYRO_MAP_SIZE; i++) {
			chip = &gyro_chip[i];
			printf(" %.4f, ", gyro_read_rate(chip));
		}

		vTaskDelay(0.10*configTICK_RATE_HZ);

		printf("\n\r");
	}
	return CMD_ERROR_NONE;
}

int cmd_gyro_setup(struct command_context *ctx) {

	/* Setup the SPI device */
	extern spi_dev_t spi_dev;
	gyro_spi_setup_cs(&spi_dev, &gyro_chip[0], gyro_map[0]);

	/* Initialise gyro */
	gyro_setup(&gyro_chip[0]);
	return CMD_ERROR_NONE;

}

int cmd_gyro_test(struct command_context *ctx) {
	gyro_test(&gyro_chip[0]);
	return CMD_ERROR_NONE;
}

int cmd_gyro_autonull(struct command_context *ctx) {
	gyro_autonull(&gyro_chip[0]);
	return CMD_ERROR_NONE;
}

int cmd_gyro_write_offset(struct command_context *ctx) {

	char * args = command_args(ctx);
	float offset;

	if (args == NULL || sscanf(args, "%f", &offset) != 1) {
		printf("Gyro offset is %.4f [deg/sec]\r\n", gyro_read_offset(&gyro_chip[0]));
	} else {
		printf("Setting offset %.4f [deg/sec]\r\n", offset);
		gyro_write_offset(&gyro_chip[0], offset);
	}

	return CMD_ERROR_NONE;

}

int cmd_lm70_init(struct command_context *ctx) {
	extern spi_dev_t spi_dev;
	lm70_spi_setup_cs(&spi_dev, &lm70_chip[0], lm70_map[0]);
	return CMD_ERROR_NONE;
}

int cmd_lm70_test(struct command_context *ctx) {

	while(1) {
		if (usart_messages_waiting(USART_CONSOLE) != 0)
			break;

		printf("Temp %f\r\n", lm70_read_temp(&lm70_chip[0]));
		vTaskDelay(100);
	}

	return CMD_ERROR_NONE;

}

struct command gyro_commands[] = {
	{
		.name = "setup",
		.help = "Gyro setup",
		.handler = cmd_gyro_setup,
	},{
		.name = "test",
		.help = "Gyro test",
		.handler = cmd_gyro_test,
	},{
		.name = "null",
		.help = "Gyro autonull",
		.handler = cmd_gyro_autonull,
	},{
		.name = "offset",
		.help = "Gyro offset",
		.handler = cmd_gyro_write_offset,
	},
};

struct command lm70_commands[] = {
	{
		.name = "init",
		.help = "LM70 setup",
		.handler = cmd_lm70_init,
	},{
		.name = "test",
		.help = "LM70 test",
		.handler = cmd_lm70_test,
	},
};

struct command panels_subcommands[] = {
	{
		.name = "init",
		.help = "Panels init",
		.handler = cmd_panels_init,
	},{
		.name = "test",
		.help = "Panels test",
		.handler = cmd_panels_test,
	},{
		.name = "cont",
		.help = "Panels test (cont)",
		.handler = cmd_panels_test_cont,
	},
};

struct command __root_command panels_commands[] = {
	{
			    .name = "seuvon",
			    .help = "power on seuv",
			    .handler = seuvon,
    },{
    		    .name = "seuvoff",
			    .help = "power off seuv",
			    .handler = seuvoff,
    },{
			    .name = "seuvwrite",
		    	.help = "configure SEUV",
		    	.usage = "<node>",
			    .handler = seuvwrite,
    },{
				.name = "seuvread",
				.help = "Take data from a configured Channel",
				.handler = seuvread,
	},{
			.name = "i2cread",
			.help = "Take data from i2c Channel",
			.usage = "<node> <rx>",
			.handler = i2cread,
	},{
				.name = "fstestwod",
				.help = "Read and print WOD data inside Flesystem ",
				.handler = fstestwod,
	},{
				.name = "comhk",
				.help = "retrive com hk data ",
				.handler = comhk,
	 },{
				.name = "comhk2",
				.help = "retrive com transmitter state ",
				.handler = comhk2,
	 },{
			 	 .name = "jt",
			 	 .help = "jump specific time",
			 	 .handler = jumpTime,
	 },{
		.name = "gyro",
		.help = "Gyro commands",
		.chain = INIT_CHAIN(gyro_commands),
	},{
		.name = "lm70",
		.help = "LM70 commands",
		.chain = INIT_CHAIN(lm70_commands),
	},{
		.name = "panels",
		.help = "Panels commands",
		.chain = INIT_CHAIN(panels_subcommands),
	},
};

void cmd_panels_setup(void) {
	command_register(panels_commands);
}

