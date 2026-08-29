/*
 * mqttTest.c
 *
 *  Created on: 26 jul 2026
 *      Author: joaquzin
 */

#include <string.h>
#include "ch.h"
#include "hal.h"

// WIZnet ioLibrary includes
#include "wizchip_conf.h"
#include "socket.h"
#include "dhcp.h"

// Paho MQTT Embedded-C includes
#include "MQTTClient.h"

/* ========================================================================== */
/* 1. CONFIGURATION DEFINITIONS                                               */
/* ========================================================================== */
#define MQTT_SOCKET_NUM        1
#define DHCP_SOCKET_NUM        0
#define BACKOFF_MIN_DELAY_MS   1000
#define BACKOFF_MAX_DELAY_MS   60000
#define BACKOFF_MULTIPLIER     2

static uint32_t current_backoff_delay = BACKOFF_MIN_DELAY_MS;
static uint8_t dhcp_rx_buffer[548]; // Minimum size required by WIZnet DHCP
static uint8_t mqtt_tx_buffer[256];
static uint8_t mqtt_rx_buffer[256];
static uint8_t broker_ip[] = {192, 168, 1, 10};

typedef enum {
  STATE_INIT_NETWORK,
  STATE_CONNECT_BROKER,
  STATE_MQTT_OPERATIONAL
} mqtt_state_t;

// Paho platform timer struct required for the single-file setup
typedef struct TimerPaho {
  sysinterval_t start_time;
  sysinterval_t timeout_ticks;
} TimerPaho;

/* ========================================================================== */
/* 2. PAHO TIMER INTERFACE IMPLEMENTATION                                     */
/* ========================================================================== */
void TimerInit(TimerPaho *timer) {
  timer->start_time = 0;
  timer->timeout_ticks = 0;
}

char TimerIsExpired(TimerPaho *timer) {
  sysinterval_t now = chVTGetSystemTimeX();
  return (chTimeDiffX(timer->start_time, now) >= timer->timeout_ticks);
}

void TimerCountdownMS(TimerPaho *timer, unsigned int timeout_ms) {
  timer->start_time = chVTGetSystemTimeX();
  timer->timeout_ticks = TIME_MS2I(timeout_ms);
}

void TimerCountdown(TimerPaho *timer, unsigned int timeout_seconds) {
  TimerCountdownMS(timer, timeout_seconds * 1000);
}

int TimerLeftMS(TimerPaho *timer) {
  sysinterval_t now = chVTGetSystemTimeX();
  sysinterval_t elapsed = chTimeDiffX(timer->start_time, now);
  if (elapsed >= timer->timeout_ticks)
    return 0;
  return (int)TIME_I2MS(timer->timeout_ticks - elapsed);
}

/* ========================================================================== */
/* 3. W5500 SPI INTERFACE CALLBACKS                                           */
/* ========================================================================== */
//void w5500_cs_select(void) {
//  palClearLine(LINE_SPI1_CS);
//}
//
//void w5500_cs_deselect(void) {
//  palSetLine(LINE_SPI1_CS);
//}
//
//uint8_t w5500_spi_read(void) {
//  uint8_t rx_data;
//  spiReceive(&SPID1, 1, &rx_data);
//  return rx_data;
//}
//
//void w5500_spi_write(uint8_t tx_data) {
//  spiSend(&SPID1, 1, &tx_data);
//}

void w5500_spi_init(void) {
  reg_wizchip_cs_cbfunc(w5500_cs_select, w5500_cs_deselect);
  reg_wizchip_spi_cbfunc(w5500_spi_read, w5500_spi_write);
}

/* ========================================================================== */
/* 4. NON-BLOCKING PAHO NETWORK WRAPPERS                                      */
/* ========================================================================== */
int w5500_paho_read(Network *n, unsigned char *buffer, int len, int timeout_ms) {
  TimerPaho timer;
  TimerCountdownMS(&timer, timeout_ms);
  int bytes_read = 0;

  while (bytes_read < len && !TimerIsExpired(&timer)) {
    int32_t ret = getSn_RX_RSR(n->my_socket);
    if (ret > 0) {
      int wanted = (len - bytes_read < ret) ? (len - bytes_read) : ret;
      int32_t received = recv(n->my_socket, &buffer[bytes_read], wanted);
      if (received < 0)
        return -1;
      bytes_read += received;
    }
    else if (ret < 0) {
      return -1;
    }
    else {
      chThdYield();
    }
  }
  return bytes_read;
}

int w5500_paho_write(Network *n, unsigned char *buffer, int len, int timeout_ms) {
  TimerPaho timer;
  TimerCountdownMS(&timer, timeout_ms);
  int bytes_written = 0;

  while (bytes_written < len && !TimerIsExpired(&timer)) {
    int32_t ret = send(n->my_socket, &buffer[bytes_written],
                       len - bytes_written);
    if (ret == SOCK_BUSY) {
      chThdYield();
      continue;
    }
    if (ret < 0)
      return -1;
    bytes_written += ret;
  }
  return bytes_written;
}

/* ========================================================================== */
/* 5. DHCP ASSIGNMENT CALLBACKS                                               */
/* ========================================================================== */
void cb_dhcp_ip_assign(void) {
  wiz_NetInfo net_info;
  wizchip_getnetinfo(&net_info);
  getIPfromDHCP(net_info.ip);
  getGWfromDHCP(net_info.gw);
  getSNfromDHCP(net_info.sn);
  getDNSfromDHCP(net_info.dns);
  net_info.dhcp = NETINFO_DHCP;
  wizchip_setnetinfo(&net_info);
}

void cb_dhcp_ip_conflict(void) {
  // Optional alert logic for IP conflicts
}

/* ========================================================================== */
/* 6. HEALTH AND RECOVERY HELPERS                                             */
/* ========================================================================== */
bool is_ethernet_cable_connected(void) {
  wiz_PhyStatus phy_status;
  wizphy_getphystatus(&phy_status);
  return (phy_status.ln == PHY_LINK_ON);
}

bool is_socket_connected(int socket_num) {
  return (getSn_SR(socket_num) == SOCK_ESTABLISHED);
}

void increase_backoff_delay(void) {
  current_backoff_delay *= BACKOFF_MULTIPLIER;
  if (current_backoff_delay > BACKOFF_MAX_DELAY_MS) {
    current_backoff_delay = BACKOFF_MAX_DELAY_MS;
  }
}

void reset_backoff_delay(void) {
  current_backoff_delay = BACKOFF_MIN_DELAY_MS;
}

/* ========================================================================== */
/* 7. MQTT INCOMING TOPIC CALLBACK                                            */
/* ========================================================================== */
void incoming_message_handler(MessageData *data) {
  char payload_string[64];
  int len = (data->message->payloadlen < 63) ? data->message->payloadlen : 63;
  memcpy(payload_string, data->message->payload, len);
  payload_string[len] = '\0';

  if (strcmp(payload_string, "LED_ON") == 0) {
    palSetLine(LINE_LED_RED);
  }
  else if (strcmp(payload_string, "LED_OFF") == 0) {
    palClearLine(LINE_LED_RED);
  }
}

/* ========================================================================== */
/* 8. MAIN AUTOMATION THREAD                                                  */
/* ========================================================================== */
static THD_WORKING_AREA(waW5500MqttThread, 2048);
static THD_FUNCTION( W5500MqttThread, arg) {
  (void)arg;
  chRegSetThreadName("w5500_single_file");

  // SPI Configuration object should be defined in your board files or here
  // spiStart(&SPID1, &spi1_config);
  w5500_spi_init();

  uint8_t rx_tx_sizes[] = {2, 2, 2, 2, 0, 0, 0, 0};
  wizchip_init(rx_tx_sizes, rx_tx_sizes);

  Network n;
  n.my_socket = MQTT_SOCKET_NUM;
  n.mqttread = w5500_paho_read;
  n.mqttwrite = w5500_paho_write;

  MQTTClient client;
  MQTTClientInit(&client, &n, 1000, mqtt_tx_buffer, sizeof(mqtt_tx_buffer),
                 mqtt_rx_buffer, sizeof(mqtt_rx_buffer));

  mqtt_state_t current_state = STATE_INIT_NETWORK;
  systime_t last_dhcp_tick = chVTGetSystemTime();

  // Set fallback dummy MAC address for DHCP initialization
  wiz_NetInfo dummy_net = {.mac = {0x00, 0x08, 0xDC, 0xAA, 0xBB, 0xCC}, .dhcp =
                               NETINFO_DHCP};
  wizchip_setnetinfo(&dummy_net);
  DHCP_init(DHCP_SOCKET_NUM, dhcp_rx_buffer);
  reg_dhcp_cbfunc(cb_dhcp_ip_assign, cb_dhcp_ip_assign, cb_dhcp_ip_conflict);

  while (true) {
    // Core background ticking logic for DHCP lease renewal
    if (chVTTimeElapsedSinceX(last_dhcp_tick) >= TIME_MS2I(1000)) {
      DHCP_time_handler();
      DHCP_run();
      last_dhcp_tick = chVTGetSystemTime();
    }

    // Cable-drop recovery fallback
    if (!is_ethernet_cable_connected()) {
      if (current_state != STATE_INIT_NETWORK) {
        close(n.my_socket);
        DHCP_init(DHCP_SOCKET_NUM, dhcp_rx_buffer);
        current_state = STATE_INIT_NETWORK;
      }
      chThdSleepMilliseconds(500);
      continue;
    }

    switch (current_state) {
    case STATE_INIT_NETWORK: {
      uint8_t dhcp_state = DHCP_run();
      if (dhcp_state == DHCP_IP_ASSIGN || dhcp_state == DHCP_IP_CHANGED) {
        current_state = STATE_CONNECT_BROKER;
      }
      chThdSleepMilliseconds(100);
      break;
    }

    case STATE_CONNECT_BROKER: {
      close(n.my_socket);
      bool connection_successful = false;

      if (socket(n.my_socket, Sn_MR_TCP, 1883, 0) == n.my_socket) {
        if (connect(n.my_socket, broker_ip, 1883) == SOCK_OK) {
          MQTTPacket_connectData connect_data =
          MQTTPacket_connectData_initializer;
          connect_data.MQTTVersion = 3;
          connect_data.clientID.cstring = "ChibiOS_SingleFile";
          connect_data.keepAliveInterval = 60;

          if (MQTTConnect(&client, &connect_data) == 0) {
            connection_successful = true;
            current_state = STATE_MQTT_OPERATIONAL;
            reset_backoff_delay();
            MQTTSubscribe(&client, "device/commands", QOS0,
                          incoming_message_handler);
          }
        }
      }

      if (!connection_successful) {
        increase_backoff_delay();
        systime_t backoff_start = chVTGetSystemTime();

        while (chVTTimeElapsedSinceX(backoff_start)
            < TIME_MS2I(current_backoff_delay)) {
          if (chVTTimeElapsedSinceX(last_dhcp_tick) >= TIME_MS2I(1000)) {
            DHCP_time_handler();
            DHCP_run();
            last_dhcp_tick = chVTGetSystemTime();
          }
          if (!is_ethernet_cable_connected())
            break;
          chThdSleepMilliseconds(100);
        }
      }
      break;
    }
    case STATE_MQTT_OPERATIONAL: {
      int rc = MQTTYield(&client, 100);
      if (rc != 0 || !is_socket_connected(n.my_socket)) {
        current_state = STATE_CONNECT_BROKER;
        reset_backoff_delay();
        break;
      }
      // Periodic Heartbeat Publish
      MQTTMessage message;
      char payload[] = "Alive";
      message.qos = QOS0;
      message.retained = 0;
      message.payload = payload;
      message.payloadlen = strlen(payload);
      if (MQTTPublish(&client, "device/status", &message) != 0) {
        current_state = STATE_CONNECT_BROKER;
      }
      chThdSleepMilliseconds(2000);
      break;
    }
    }
  }
}

