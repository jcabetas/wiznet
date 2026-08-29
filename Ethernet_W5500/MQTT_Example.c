/*
 * MQTT_Example.c
 *
 *  Created on: Dec 6, 2025
 *      Author: controllerstech
 */

//#include "main.h"
#include "ch.h"
#include "hal.h"

#include "wizchip_conf.h"
#include "socket.h"

#include "MQTT/MQTTClient.h"
#include "MQTT/mqtt_interface.h"
#include "DNS/dns.h"

#include <stdio.h>
#include <string.h>

//mosquitto_sub -h 192.168.1.61 -t "#" -u joaquin -P JeW31ZT9Rdx
thread_t *procesoYield = NULL;
extern void tickLed(uint16_t numVeces);

#define MQTT_SOCKET       0
#define MQTT_BUF_SIZE     1024
#define MQTT_KEEPALIVE    60

#undef USE_BROKER_HOSTNAME

#ifdef USE_BROKER_HOSTNAME
#define MQTT_BROKER_HOST "test.mosquitto.org"
#else
#define MQTT_BROKER_IP   { 192,168,1,61 }
#endif

#define MQTT_BROKER_PORT 1883

#define MQTT_CLIENT_ID   "joaquin"

/* MQTT buffers */
static uint8_t mqtt_sendbuf[MQTT_BUF_SIZE];
static uint8_t mqtt_readbuf[MQTT_BUF_SIZE];

/* MQTT Handles */
Network net;
MQTTClient client;

/* External Functions */
extern void led_Control(int state);

/*************************************************************
 *  Initialize W5500 network structure
 *************************************************************/
int mqtt_network_init(void)
{
	printf("=== MQTT START ===\r\n");
#ifdef USE_BROKER_HOSTNAME
    uint8_t broker_ip[4] = {0};
    uint8_t dns_server[4] = {8, 8, 8, 8};  // Configure DNS server (Google DNS)

    int8_t ret = DNS_run(dns_server, (uint8_t*)MQTT_BROKER_HOST, broker_ip);
    if (ret != 1)
    {
        printf("DNS failed: %d\r\n", ret);
        return -1;
    }

    printf("DNS Success: %d.%d.%d.%d\r\n",
            broker_ip[0], broker_ip[1], broker_ip[2], broker_ip[3]);
#else
    uint8_t broker_ip[4] = MQTT_BROKER_IP;
#endif
    // Open TCP socket for MQTT
    socket(MQTT_SOCKET, Sn_MR_TCP, 0, 0);

    printf("Connecting socket to broker...\r\n");
    if (connect(MQTT_SOCKET, broker_ip, MQTT_BROKER_PORT) != SOCK_OK)
    {
        printf("Socket connect failed!\r\n");
        return -2;
    }
    return 0;
}

/*************************************************************
 *  Connect to MQTT broker
 *************************************************************/
int mqtt_connect_broker(void)
{
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

    // 1. Setup network callbacks (using mqtt_interface.c)
    NewNetwork(&net, MQTT_SOCKET);

    // 2. Init MQTT Client
    MQTTClientInit(&client, &net,
                   30000,      // Command timeout
                   mqtt_sendbuf, MQTT_BUF_SIZE,
                   mqtt_readbuf, MQTT_BUF_SIZE);

    // 3. Configure connect packet
    data.MQTTVersion = 3;
    data.keepAliveInterval = MQTT_KEEPALIVE;
    data.cleansession = 1;
    data.clientID.cstring = MQTT_CLIENT_ID;
    data.username.cstring = "joaquin";
    data.password.cstring = "JeW31ZT9Rdx";

    printf("MQTT: Connecting to broker...\r\n");

    if (MQTTConnect(&client, &data) != SUCCESSS)
    {
        printf("MQTT: Connection FAILED!\r\n");
        return -1;
    }

    printf("MQTT: Connected successfully!\r\n");
    return 0;
}

/*************************************************************
 *  SUBSCRIPTION MESSAGE HANDLER
 *************************************************************/
void messageArrived(MessageData* md)
{
    char mBuffer[128];

    printf("\r\n--- MQTT Message Received ---\r\n");

    // if the message is received from "stm32/sub"
    if (memcmp(md->topicName->lenstring.data, "stm32/sub", md->topicName->lenstring.len) == 0)
	{
    	// Extract the message
    	int len = md->message->payloadlen;
        memcpy(mBuffer, md->message->payload, len);
        mBuffer[len] = 0;

        // perform the operation
//    	printf ("Payload: %s\r\n", mBuffer);
        if (strcmp(mBuffer, "ON")==0)	led_Control(1);
        if (strcmp(mBuffer, "OFF")==0)	led_Control(0);
    }

    // Log the topicname and message
    printf("Topic: %.*s\r\n", 	(int)md->topicName->lenstring.len,(char*)md->topicName->lenstring.data);
    printf("Payload: %.*s\r\n\r\n", (int)md->message->payloadlen, (char*)md->message->payload);
}

/*************************************************************
 *  Subscribe to topic
 *************************************************************/
int mqtt_subscribe(char *topic)
{
    if (MQTTSubscribe(&client, topic, QOS0, messageArrived) != SUCCESSS)
    {
        printf("MQTT: Subscription failed!\r\n");
        return -1;
    }

    printf("MQTT: Subscribed to [%s]\r\n", topic);
    return 0;
}

/*************************************************************
 *  Publish message
 *************************************************************/
int mqtt_publish(char *topic, const char* payload)
{
    MQTTMessage msg;

    msg.qos = QOS0;
    msg.retained = 0;
    msg.dup = 0;
    msg.payload = (void*)payload;
    msg.payloadlen = strlen(payload);

    if (MQTTPublish(&client, topic, &msg) == SUCCESSS)
    {
        tickLed(1);
        return 0; //printf("MQTT: Published → %s\r\n", payload);
    }
    else
        return 1; //printf("MQTT: Publish FAILED\r\n");
}

/*************************************************************
 *  MAIN MQTT LOOP
 *************************************************************/
int mqtt_yield(void)
{
	/* Handle incoming packets */
	return MQTTYield(&client, 0);
}

///*************************************************************
// *  MAIN APPLICATION ENTRY
// *************************************************************/
//void MQTT_Example(void)
//{
//    printf("=== MQTT EXAMPLE START ===\r\n");
//
//    mqtt_network_init();
//
//    if (mqtt_connect_broker() != 0)
//    {
//    	// error handler
//    }
//
//    mqtt_subscribe("topic");
//
//    uint32_t tick5s = HAL_GetTick();
//
//    while (1)
//    {
//    	mqtt_yield();
//        /* Publish every 5 seconds */
//        if (HAL_GetTick() - tick5s >= 5000)
//        {
//            tick5s = HAL_GetTick();
//            mqtt_publish("topic", "message");
//        }
//    }
//}

static THD_WORKING_AREA(waYield, 1024);
static THD_FUNCTION(threadYield, arg) {
    (void) arg;
    uint16_t ms = 0;
    uint16_t count = 0;
    char buffer[30];
    chRegSetThreadName("Yield");
    int error = W5500_Init();
    mqtt_network_init();
    mqtt_connect_broker();
    if (mqtt_subscribe("stm32/sub") != 0)
      tickLed(100);

    while (1)
    {
        uint8_t cfgr = getPHYCFGR();
        if (!(cfgr & 1))
        {
          uint8_t pp=0; // no está conectado
        }
        int error = mqtt_yield();
        if (ms>=5000)
        {
            snprintf (buffer, sizeof(buffer), "STM32 W5500 -> %d", count++);
            mqtt_publish("stm32/pub", buffer);
            ms = 0;
        }
            chThdSleepMilliseconds(50);
            ms += 50;
    }
}

void mqtt_initThread(void)
{
    if (!procesoYield)
    {
        procesoYield = chThdCreateStatic(waYield, sizeof(waYield), NORMALPRIO, threadYield, NULL);
    }
}

