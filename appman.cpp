#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

#include "thunder_api.h"
#include "Property.h"
#include "PropertyEvent.h"
#include "PlatformEvent.h"
#include "appmgr_dbus.h"
#include "NetEvent.h"

static struct aml_dbus *method_bus;

#define CALL_APPMGR(f, ...)                                                                                            \
  ambus_method_proxy<decltype(&appmgr_dbus::f)>::call(method_bus, APP_MANAGER_SERVICE_NAME, APP_MANAGER_OBJECT_PATH,   \
                                                      APP_MANAGER_INTERFACE_NAME, #f, ##__VA_ARGS__)

namespace appmgr_dbus {
int LaunchApp(const std::string &appname);
int SetAppState(const std::string &appname, int state);
int GetAppState(const std::string &appname);
int SetDeepLink(const std::string &appname, const std::string &url);
int SetFocus(const std::string &appname);
int KillApp(const std::string &appname);
}; // namespace appmgr_dbus


#define PARAM_FRIENDLY_NAME "setup_friendly_name"
#define PARAM_MODEL_NAME "information_model_name"
#define MAXSIZE 256
static char *spDefaultUuid = "deadbeef-wlfy-beef-dead-beefdeadbeef";
static char *spDefaultFName = "AMLOGICDEV";
static char *spDefaultMName = "OTT-Defult";
static char *spDefaultEthInterface = "eth0";
static char *spDefaultWifiInterface = "wlan0";
#define ARRAY_SIZE(e) (sizeof(e) / sizeof(*(e)))

const char *params_interest[] = {PARAM_FRIENDLY_NAME, PARAM_MODEL_NAME};

using namespace Amlogic::Platform;

class AmlPlfEventHandle : public Amlogic::Platform::PlatformEvent::Subscriber {
private:
  AmlPlfEventHandle(const AmlPlfEventHandle &) = delete;
  AmlPlfEventHandle &operator=(const AmlPlfEventHandle &) = delete;

  void onEvent(Amlogic::Platform::Event *event);

public:
  AmlPlfEventHandle();
  virtual ~AmlPlfEventHandle();
};


AmlPlfEventHandle::AmlPlfEventHandle() {
  PlatformEvent::GetInstance().subscribe("property", this);
  PlatformEvent::GetInstance().subscribe("network", this);
}

AmlPlfEventHandle::~AmlPlfEventHandle() {
  PlatformEvent::GetInstance().unsubscribe("property", this);
  PlatformEvent::GetInstance().unsubscribe("network", this);
}

void AmlPlfEventHandle::onEvent(Amlogic::Platform::Event *event) {
  if (event->getNamespace() == "property") {
    switch (event->getType()) {
      case PropertyEvent::EVENT_PROPERTY_CHANGED: {
        Property::PropertyChangedEvent *e =
            (Property::PropertyChangedEvent *)event;

        if (e->key == "persist.dialserver.name") {
          //Modify dialserver name
          snprintf(friendly_name, MAXSIZE, "%s", e->value.c_str());
        }
      } break;
    }
  } else if (event->getNamespace() == "network") {
    switch (event->getType()) {
    case NetEvent::EVENT_INTERFACE_IPADDRESS: {
      std::cout << "amldial-ip changed" << std::endl;
      addNewIpToMulticast();
    } break;
    default:
      break;
    }
  }
}

extern "C" {
int activateApp(const char *callsign, const char *url) {
  std::cout << "the launchurl is :" << std::string(url) << std::endl;
  uint32_t ret;
  // Passive mode
  if (_passivemode) {
    return 0;
  }

  // TODO: Wake On Lan

  // Active mode
  DIALStatus appStatus = getAppStatus(callsign);
  if (appStatus == kDIALStatusHide) {
    resumeApp(callsign);
    if (!strcmp(curAppForDial->handler, "Netflix")) {
    }
  } else if (appStatus == kDIALStatusStopped || appStatus == kDIALStatusError) {
    ret = CALL_APPMGR(LaunchApp, callsign);
    std::cout << "LaunchApp " << callsign << " return " << ret << std::endl;
  }
  if (!strcmp(curAppForDial->handler, "YouTube")) {
    std::string launchURL;
    launchURL = launchURL + "\"" + url + "&launch=dial" + "\"";
    std::cout << "amldial-launchURL value:" << launchURL << std::endl;
    ret = CALL_APPMGR(SetDeepLink, callsign, launchURL);
    std::cout << "SetDeepLink " << callsign << " " << launchURL << " return " << ret << std::endl;
  }
  // SetFocus
  ret = CALL_APPMGR(SetFocus, callsign);
  std::cout << "SetFocus " << callsign << " return " << ret << std::endl;
  return 0;
}

int deActivateApp(const char *callsign) {
  // Passive mode
  if (_passivemode) {
    return 0;
  }

  // Active mode
  int ret = CALL_APPMGR(KillApp, callsign);
  std::cout << "KillApp " << callsign << " return " << ret << std::endl;
  _appHidden = false;
  return 0;
}

int listenIpChange() {
  return 0;
}

bool get_mac(char* mac, const char *if_typ)
{
  struct ifreq tmp;
  int sock_mac;
  sock_mac = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_mac == -1)
  {
      return false;
  }
  memset(&tmp,0,sizeof(tmp));
  strncpy(tmp.ifr_name, if_typ, sizeof(tmp.ifr_name)-1);
  if ((ioctl( sock_mac, SIOCGIFHWADDR, &tmp)) < 0)
  {
      close(sock_mac);
      return false;
  }
  sprintf(mac, "%02x%02x%02x%02x%02x%02x",
          (unsigned char)tmp.ifr_hwaddr.sa_data[0],
          (unsigned char)tmp.ifr_hwaddr.sa_data[1],
          (unsigned char)tmp.ifr_hwaddr.sa_data[2],
          (unsigned char)tmp.ifr_hwaddr.sa_data[3],
          (unsigned char)tmp.ifr_hwaddr.sa_data[4],
          (unsigned char)tmp.ifr_hwaddr.sa_data[5]
          );
  close(sock_mac);
  return true;
}

void setDialProperty(char *friendlyname, char *uuid, char *modelname) {
  char mac_addr[18] = {0};
  // get_mac(mac_addr, "eth0") ? strcat(uuid, mac_addr) : strncpy(uuid, spDefaultUuid, MAXSIZE-1);
  if (get_mac(mac_addr, "eth0"))
    strcat(uuid, mac_addr);
  else
    snprintf(uuid, MAXSIZE, "%s", spDefaultUuid);
  std::string _friendlyname, _modelname, _eth_interface, _wifi_interface;
  if (Amlogic::Platform::Property::get("persist.dialserver.name", _friendlyname))
    snprintf(friendlyname, MAXSIZE, "%s", _friendlyname.c_str());
  else
    snprintf(friendlyname, MAXSIZE, "%s", spDefaultFName);
  if (Amlogic::Platform::Property::get("ro.model.name", _modelname))
    snprintf(modelname, MAXSIZE, "%s", _modelname.c_str());
  else
    snprintf(modelname, MAXSIZE, "%s", spDefaultMName);
  if (Amlogic::Platform::Property::get("ro.ethernet.interface", _eth_interface))
    snprintf(eth_interface, IFNAMSIZ, "%s", _eth_interface.c_str());
  else
    snprintf(eth_interface, IFNAMSIZ, "%s", spDefaultEthInterface);
  if (Amlogic::Platform::Property::get("ro.wifi.interface", _wifi_interface))
    snprintf(wifi_interface, IFNAMSIZ, "%s", _wifi_interface.c_str());
  else
    snprintf(wifi_interface, IFNAMSIZ, "%s", spDefaultWifiInterface);

  std::cout << "in setDialProperty(),the properties are friendlyname:" <<  friendlyname << " modelname:" << modelname << " uuid:" << uuid << std::endl;
  static AmlPlfEventHandle _obj;
}

DIALStatus getAppStatus(const char *callsign) {
  int state = CALL_APPMGR(GetAppState, callsign);
  DIALStatus ret;
  const char *st;
  switch (state) {
  case APPMGR_APPSTATE_STOPPED:
    ret = kDIALStatusStopped;
    st = "stopped";
    break;
  case APPMGR_APPSTATE_SUSPEND:
    ret = kDIALStatusHide;
    st = "suspended";
    break;
  case APPMGR_APPSTATE_RUNNING:
    ret =  kDIALStatusRunning;
    st = "running";
    break;
  case APPMGR_APPSTATE_INVALID:
  default:
    st = "invalid";
    ret = kDIALStatusError;
    break;
  }
  std::cout << "GetAppState " << callsign << " return " << state << ": " << st << std::endl;
  return ret;
}

bool hideApp(const char* callsign) {
  // Passive mode
  if (_passivemode) {
    return true;
  }

  // Active mode
  int ret = CALL_APPMGR(SetAppState, callsign, APPMGR_APPSTATE_SUSPEND);
  std::cout << "hideApp " << callsign << " return " << ret << std::endl;
  return true;
}

bool resumeApp(const char* callsign) {
  int ret = CALL_APPMGR(SetAppState, callsign, APPMGR_APPSTATE_RUNNING);
  std::cout << "resumeApp " << callsign << " return " << ret << std::endl;
  return true;
}

int loadConfig(const char* confFile) {
  method_bus = ambus_ensure_run(NULL);
  setClientAmbus(method_bus, method_bus);
  FILE *fp = fopen(confFile, "r");
  if (fp == NULL) {
    std::cout << "loadConfig fail to open file " << confFile << std::endl;
    return 1;
  }
  char *line = NULL;
  size_t len = 0;
  char *key, *val;
  struct appInfo *head = NULL, *app = NULL;
  while (getline(&line, &len, fp) >= 0) {
    char *p = line;
    while (isspace(*p)) ++p;
    if (*p == '#')
      continue;
    else if (*p == '[') {
      p++;
      while (isspace(*p)) ++p;
      key = p;
      while (*p && *p != ']') ++p;
      *p = '\0';
      if (strcmp(key, "configuration") != 0) {
        app = (struct appInfo *)calloc(1, sizeof(*app));
        app->name = strdup(key);
        app->next = head;
        head = app;
      } else
        app = NULL;
    } else if (isalnum(*p)) {
      key = p;
      while ((isalnum(*p) || *p == '_') && *p != '=') ++p;
      if (*p != '=') {
        std::cout << "parsing error in line " << line << std::endl;
        continue;
      }
      *p++ = '\0';
      while (isspace(*p)) ++p;
      val = p;
      while (*p && !isspace(*p)) ++p;
      *p = '\0';
      if (app == NULL) {
        if (strcmp(key, "passivemode") == 0) {
          _passivemode = strcmp(val, "true") == 0;
        }
      } else if (strcmp(key, "handler") == 0) {
        app->handler = strdup(val);
      } else if (strcmp(key, "hide") == 0) {
        app->hide = strdup(val);
      } else if (strcmp(key, "url") == 0) {
        app->url = strdup(val);
      } else if (strcmp(key, "callsign") == 0) {
        app->callsign = strdup(val);
      }
    }
  }
  free(line);
  fclose(fp);
  // appInfoList first node not used, malloc additional node
  appInfoList = app = (struct appInfo *)calloc(1, sizeof(*app));
  app->next = head;
  return 0;
}

}
