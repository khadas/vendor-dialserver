.PHONY: clean
.DEFAULT_GOAL=all

OBJS := main.o dial_server.o mongoose.o quick_ssdp.o url_lib.o dial_data.o system_callbacks.o nf_callbacks.o amazon_callbacks.o
HEADERS := $(wildcard *.h)

LOCAL_INCLUDE := -I$(STAGING_DIR)/usr/include/platform_service/
ifneq ($(WITH_APP_MANAGER),y)
LOCAL_INCLUDE += -I$(STAGING_DIR)/usr/include/WPEFramework/
OBJS_CXX := thunder_api.o
LDFLAGS += -lWPEFrameworkProtocols -lWPEFrameworkPlugins -lWPEFrameworkCore
else
OBJS_CXX := appman.o
CFLAGS += -DWITH_APP_MANAGER
LDFLAGS += -lamldbus -lsystemd
endif
LDFLAGS += -laml_platform_client
CFLAGS += $(LOCAL_INCLUDE)

# BUILDDIR used for out-of-source compile
BUILDDIR?=./

%.c: $(HEADERS)

$(addprefix $(BUILDDIR)/,$(OBJS)) : $(BUILDDIR)/%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -Wall -g -fPIC -c $< -o $@

$(addprefix $(BUILDDIR)/,$(OBJS_CXX)) : $(BUILDDIR)/%.o: %.cpp $(HEADERS)
	$(CXX) $(CFLAGS) -Wall -g -fPIC -c $< -o $@

_dummy := $(shell mkdir -p $(BUILDDIR))

all: $(BUILDDIR)/dialserver

$(BUILDDIR)/dialserver: $(addprefix $(BUILDDIR)/,$(OBJS) $(OBJS_CXX))
	$(CC) $(CFLAGS) -Wall -Werror -Wl,-rpath,. -g $+ $(LDFLAGS) -ldl -lpthread -lrt -lstdc++ -o $@

clean:
	rm -f $(BUILDDIR)/dialserver $(addprefix $(BUILDDIR)/,$(OBJS) $(OBJS_CXX))
