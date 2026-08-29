# WIZNET files.
WIZNETSRC = ${CHIBIOS}/ext/wiznet/Ethernet/socket.c \
			${CHIBIOS}/ext/wiznet/Ethernet/wizchip_conf.c \
            ${CHIBIOS}/ext/wiznet/Ethernet/W5500/w5500.c \
            ${CHIBIOS}/ext/wiznet/Internet/DNS/dns.c \
            ${CHIBIOS}/os/various/wiznet_bindings/wiznet.c \

WIZNETINC = ${CHIBIOS}/ext/wiznet/Ethernet \
            ${CHIBIOS}/ext/wiznet/Ethernet/W5500 \
            ${CHIBIOS}/ext/wiznet/Internet/DNS \
			$(CHIBIOS)/os/various/wiznet_bindings \
			