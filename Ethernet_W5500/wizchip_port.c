/*
 * wizchip_port.c
 *
 *  Created on: Oct 27, 2025
 *      Author: controllerstech
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

//jcf #include "main.h"
#include "wizchip_conf.h"
#include "stdio.h"
#include "string.h"
#include "socket.h"
#include "stdbool.h"
#include "DHCP/dhcp.h"
#include "DNS/dns.h"

//#define W5500_SPI hspi1
//#define LOG_UART  huart1
#define USE_DHCP  1


wiz_NetInfo netInfo = {
    .mac = {0x00, 0x11, 0x33, 0x47, 0x09, 0x7D},//00:11:32:47:09:7d
    .ip = {192, 168, 1, 210},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 1, 1},
    .dns = {8, 8, 8, 8},
#if USE_DHCP
	.dhcp = NETINFO_DHCP
#else
    .dhcp = NETINFO_STATIC
#endif
};

/*************************************************   NO Changes After This   ***************************************************************/

#define W5500_CS_LOW()     palClearLine(LINE_CSW5500)
#define W5500_CS_HIGH()    palSetLine(LINE_CSW5500)
#define W5500_RST_LOW()    palClearLine(LINE_RSTW5500)
#define W5500_RST_HIGH()   palSetLine(LINE_RSTW5500)


//extern SPI_HandleTypeDef &SPID1;
//extern UART_HandleTypeDef &SD1;

int _write(int fd, unsigned char *buf, int len) {
  if (fd == 1 || fd == 2) {                     // stdout or stderr ?
    chprintf((BaseSequentialStream*) &SD6,"%s\n\r",buf); //    HAL_UART_Transmit(&LOG_UART, buf, len, 999);  // Print to the UART
  }
  return len;
}

// SPI transmit/receive
void W5500_Select(void)   { palClearLine(LINE_CSW5500); }
void W5500_Unselect(void) { palSetLine(LINE_CSW5500); }

uint8_t W5500_ReadByte(void)
{
    uint8_t rx;
    uint8_t tx = 0xFF;
    spiExchange(&SPID3, 1, &tx, &rx);//    HAL_SPI_TransmitReceive(&W5500_SPI, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

void W5500_WriteByte(uint8_t byte)
{
  uint8_t rx;
  spiExchange(&SPID3, 1, &byte, &rx); // HAL_SPI_Transmit(&W5500_SPI, &byte, 1, HAL_MAX_DELAY);
}


#if USE_DHCP
volatile bool ip_assigned = false;
#define DHCP_SOCKET   7  // last available socket

uint8_t DHCP_buffer[548];


void Callback_IPAssigned(void) {
    ip_assigned = true;
}

void Callback_IPConflict(void) {
    ip_assigned = false;
}
#endif

#define DNS_SOCKET	  6  // 2nd last socket
uint8_t DNS_buffer[512];

int W5500_Init(void)
{
    uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};

    /***** Reset Sequence  *****/
    palClearLine(LINE_RSTW5500);//W5500_RST_LOW();
    chThdSleepMilliseconds(100);//HAL_Delay(50);
    palSetLine(LINE_RSTW5500);//W5500_RST_HIGH();
    chThdSleepMilliseconds(400);//HAL_Delay(200);

    /***** Register callbacks  *****/
    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
    reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);

    /***** Initialize the chip  *****/
    if (ctlwizchip(CW_INIT_WIZCHIP, (void*)memsize) == -1){
    	//printf("Error while initializing WIZCHIP\r\n");
    	return -1;
    }
    //printf("WIZCHIP Initialized\r\n");

    /***** check communication by reading Version  *****/
    uint8_t ver = getVERSIONR();
    if (ver != 0x04){
    	//printf("Error Communicating with W5500\t Version: 0x%02X\r\n", ver);
    	return -2;
    }
    //printf("Checking Link Status..\r\n");

 	/*****  CHeck Link Status  *****/
    uint8_t link = PHY_LINK_OFF;
    uint8_t retries = 10;
    while ((link != PHY_LINK_ON) && (retries > 0)){
        ctlwizchip(CW_GET_PHYLINK, &link);
        if (link != PHY_LINK_ON) retries--;
//          printf("Link: UP\r\n");
//        else printf("Link: DOWN Retrying : %d\r\n", 10-retries);
        chThdSleepMilliseconds(500);//HAL_Delay(500);
    }
    if (link != PHY_LINK_ON){
    	//printf ("Link is Down,please reconnect and retry\nExiting Setup..\r\n");
    	return 3;
    }

    /***** Use DHCP or Static IP  *****/
#if USE_DHCP
    //printf ("Using DHCP.. Please Wait..\r\n");
    setSHAR(netInfo.mac);
    DHCP_init(DHCP_SOCKET, DHCP_buffer);

    reg_dhcp_cbfunc(Callback_IPAssigned, Callback_IPAssigned, Callback_IPConflict);

    retries = 20;
    while((!ip_assigned) && (retries > 0)) {
        DHCP_run();
        chThdSleepMilliseconds(500);//HAL_Delay(500);
        retries--;
    }
    if(!ip_assigned) {
    	// DHCP Failed, switch to static IP
    	//printf ("DHCP Failed, switching to static IP\r\n");
    	ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
    }
    else {
    	// if IP is allocated, read it
        getIPfromDHCP(netInfo.ip);
        getGWfromDHCP(netInfo.gw);
        getSNfromDHCP(netInfo.sn);
        getDNSfromDHCP(netInfo.dns);

        // Now apply them to the chip
        ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
        //printf("DHCP IP assigned successfully\r\n");
    }

#else
    // use static IP (Not DHCP)
    //printf ("Using Static IP..\r\n");
    ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
#endif

    /***** Configure DNS  *****/
    chThdSleepMilliseconds(500);//HAL_Delay(500);
    //printf("Configuring DNS..\r\n");
    DNS_init(DNS_SOCKET, DNS_buffer);

    /***** Print assigned IP on the console  *****/
    wiz_NetInfo tmpInfo;
    ctlnetwork(CN_GET_NETINFO, &tmpInfo);
    //printf("IP: %d.%d.%d.%d\r\n", tmpInfo.ip[0], tmpInfo.ip[1], tmpInfo.ip[2], tmpInfo.ip[3]);
    //printf("SUBNET: %d.%d.%d.%d\r\n", tmpInfo.sn[0], tmpInfo.sn[1], tmpInfo.sn[2], tmpInfo.sn[3]);
    //printf("GATEWAY: %d.%d.%d.%d\r\n", tmpInfo.gw[0], tmpInfo.gw[1], tmpInfo.gw[2], tmpInfo.gw[3]);
    //printf("DNS: %d.%d.%d.%d\r\n", tmpInfo.dns[0], tmpInfo.dns[1], tmpInfo.dns[2], tmpInfo.dns[3]);

    return 0;
}
