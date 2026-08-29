# WIZNET files.

#falta 	    $(CHIBIOS)/os/various/syscalls.c \
	    wiznet-iolibrary-driver/Internet/FTPClient/ftpc.c \
	    wiznet-iolibrary-driver/Internet/FTPServer/ftpd.c \
	    wiznet-iolibrary-driver/Internet/httpServer/httpParser.c \
	    wiznet-iolibrary-driver/Internet/httpServer/httpServer.c \
	    wiznet-iolibrary-driver/Internet/httpServer/httpUtil.c \
	    wiznet-iolibrary-driver/Internet/SNTP/sntp.c \
	    wiznet-iolibrary-driver/Internet/TFTP/tftp.c

WIZNETSRC = \
	    wiznet_chibios.c \
	    wiznet-iolibrary-driver/Ethernet/W5100/w5100.c \
	    wiznet-iolibrary-driver/Ethernet/W5100S/w5100s.c \
	    wiznet-iolibrary-driver/Ethernet/W5200/w5200.c \
	    wiznet-iolibrary-driver/Ethernet/W5300/w5300.c \
	    wiznet-iolibrary-driver/Ethernet/W5500/w5500.c \
	    wiznet-iolibrary-driver/Ethernet/wizchip_conf.c \
	    wiznet-iolibrary-driver/Internet/socket.c \
	    wiznet-iolibrary-driver/Internet/DHCP/dhcp.c \
	    wiznet-iolibrary-driver/Internet/DNS/dns.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTClient.c \
	    wiznet-iolibrary-driver/Internet/MQTT/mqtt_interface.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTConnectServer.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTConnectClient.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTFormat.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTPacket.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTSerializePublish.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTSubscribeClient.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTSubscribeServer.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.c \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src/MQTTDeserializePublish.c \
	    wiznet-iolibrary-driver/Internet/TFTP/netutil.c \



# falta 	    $(CHIBIOS_CONTRIB)/os/various/wiznet-iolibrary-driver_bindings 
#	    wiznet-iolibrary-driver/Internet/FTPClient \
#	    wiznet-iolibrary-driver/Internet/FTPServer \
#	    wiznet-iolibrary-driver/Internet/httpServer \
#	    wiznet-iolibrary-driver/Internet/SNTP \
#	    wiznet-iolibrary-driver/Internet/TFTP


WIZNETINC = \
	    wiznet-iolibrary-driver/Ethernet/W5100 \
	    wiznet-iolibrary-driver/Ethernet/W5100S \
	    wiznet-iolibrary-driver/Ethernet/W5200 \
	    wiznet-iolibrary-driver/Ethernet/W5300 \
	    wiznet-iolibrary-driver/Ethernet/W5500 \
	    wiznet-iolibrary-driver/Ethernet \
	    wiznet-iolibrary-driver/Internet/DHCP \
	    wiznet-iolibrary-driver/Internet/DNS \
	    wiznet-iolibrary-driver/Internet/MQTT \
	    wiznet-iolibrary-driver/Internet/MQTT/MQTTPacket/src


# Shared variables
ALLCSRC += $(WIZNETSRC)
ALLINC  += $(WIZNETINC)
