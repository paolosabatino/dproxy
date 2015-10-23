# compiler flags
CC = gcc
M4= m4

CFLAGS =-Wall -g -pthread

BIN_DIR=/sbin
CONFIG_DIR=/etc

##############################################
# uncoment the following if you have REDHAT 
DIST= -DREDHAT
CACHE_DIR=/var/cache
DHCP_LEASES=/var/state/dhcp.leases
RC_SCRIPT_DIR=/etc/rc.d/init.d

##############################################
# uncoment the following if you have DEBIAN
#DIST= -DDEBIAN
#CACHE_DIR=/var/cache
# not shure where the dhcp stuff is
#DHCP_LEASES=/var/state/dhcp.leases
#RC_SCRIPT_DIR=/etc/rc.d/init.d

##############################################
# uncoment the following if you have SUSE
#DIST= -DSUSE
#CACHE_DIR=/var/dproxy
# not shure where the dhcp stuff is
#DHCP_LEASES=/var/dhcpd/dhcp.leases
#RC_SCRIPT_DIR=/sbin/init.d

##############################################
# uncoment the following if you have SLACKWARE
#DIST= -DSLACK
#CACHE_DIR=/var/cache
#DHCP_LEASES=/var/state/dhcp.leases
######## END OF CONFIGURABLE OPTIONS #########

CACHE_FILE=$(CACHE_DIR)/dproxy.cache
CONFIG_FILE=$(CONFIG_DIR)/dproxy.conf

DEFAULTS= -DCACHE_FILE_DEFAULT=\"$(CACHE_FILE)\" \
	  -DDHCP_LEASES_DEFAULT=\"$(DHCP_LEASES)\" \
	  -DCONFIG_FILE_DEFAULT=\"$(CONFIG_FILE)\"

RCDEFS= $(DIST) -DBIN_DIR="$(BIN_DIR)" -DCONFIG_DIR="$(CONFIG_DIR)" 

# install stuf
INSTALL=install

OBJS = dproxy.o cache.o conf.o btree.o dns.o dns_server.o

all: dproxy dproxy.rc dproxy.conf

dproxy: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o : %.c Makefile
	$(CC) -c $(DEFAULTS) $(CFLAGS) $<

dproxy.rc:  dproxy.rc.m4 Makefile
	$(M4) $(RCDEFS) $< >$@

dproxy.conf:  dproxy
	@echo "***** expect an error message now (dproxy is building the config file)****"
	-@./dproxy -P > dproxy.conf
	@echo "**** Everything is ok with that ****"

clean:
	rm -f *.o *~ core dproxy dproxy.rc dproxy.conf

install: all 
	$(INSTALL) -s dproxy $(BIN_DIR)/dproxy
	$(INSTALL) -d $(CACHE_DIR)

	@if [ $(DIST) != "-DSLACK" ] ; \
	then \
		$(INSTALL) dproxy.rc $(RC_SCRIPT_DIR)/dproxy \
	else \
		@if ! grep -e dproxy $(SLACKWARE_SCRIPT) >/dev/null; then \
         echo 'Adding dproxy to $(SLACKWARE_SCRIPT)'; \
         echo '# dproxy (DNS caching proxy)' >>$(SLACKWARE_SCRIPT); \
         echo 'if [ -x $(BIN_DIR)/dproxy ]; then' >>$(SLACKWARE_SCRIPT); \
         echo '  echo "Starting dproxy"' >>$(SLACKWARE_SCRIPT); \
         echo '  $(BIN_DIR)/dproxy -c$(CONFIG_DIR)/dproxy.conf' >>$(SLACKWARE_SCRIPT); \
         echo 'fi' >>$(SLACKWARE_SCRIPT); \
      fi	\
	fi

	@if [ -f $(CONFIG_DIR)/dproxy.conf ] ; \
	then \
	   mv $(CONFIG_DIR)/dproxy.conf $(CONFIG_DIR)/dproxy.conf.saved ; \
	   echo "*******************************************************";\
	   echo "NOTE : your old dproxy configuration has been moved to"; \
	   echo "  $(CONFIG_DIR)/dproxy.conf.saved " ; \
	   echo "you may want to restore it before you restart dproxy." ; \
	   echo "*******************************************************";\
	fi
	$(INSTALL) dproxy.conf $(CONFIG_DIR)/dproxy.conf

uninstall:
	rm -f $(BIN_DIR)/dproxy
	rm -f $(CACHE_DIR)/dproxy.cache $(CACHE_DIR)/dproxy.
	rm -f $(RC_SCRIPT_DIR)/dproxy
	rm -f $(CONF_DIR)/dproxy.conf

dproxy.o: dproxy.c dproxy.h dns.h cache.h conf.h
cache.o: cache.c cache.h dproxy.h dns.h conf.h
conf.o: conf.c conf.h dproxy.h dns.h
btree.o: btree.c btree.h
dns.o: dns.c dns.h
dns_server.o: dns_server.c dns_server.h
