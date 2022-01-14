.PHONY: clean
.DEFAULT_GOAL=all

OBJS := main.o dial_server.o mongoose.o quick_ssdp.o url_lib.o dial_data.o system_callbacks.o nf_callbacks.o
OBJS_CXX := thunder_api.o
HEADERS := $(wildcard *.h)

LOCAL_INCLUDE := -I$(STAGING_DIR)/usr/include/WPEFramework/
CFLAGS += $(LOCAL_INCLUDE)
LDFLAGS += -lWPEFrameworkProtocols -lWPEFrameworkPlugins -lWPEFrameworkCore -lamldeviceproperty

%.c: $(HEADERS)

$(OBJS) : %.o: %.c $(HEADERS)
	$(CC) -Wall -g -fPIC -c $< -o $@

$(OBJS_CXX) : %.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wall -g -fPIC -c $< -o $@

all: dialserver

dialserver: $(OBJS) $(OBJS_CXX)
	$(CC) $(CFLAGS) -Wall -Werror -Wl,-rpath,. -g $(OBJS) $(OBJS_CXX) $(LDFLAGS) -ldl -lpthread -lrt -lstdc++ -o $@

clean:
	rm -f *.o dialserver
