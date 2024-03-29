#include "thunder_api.h"
#include <iostream>
#include <protocols/JSONRPCLink.h>

#include "Property.h"
#include "PropertyEvent.h"
#include "PlatformEvent.h"

#define PARAM_FRIENDLY_NAME "setup_friendly_name"
#define PARAM_MODEL_NAME "information_model_name"
#define MAXSIZE 256
static char *spDefaultUuid = "deadbeef-wlfy-beef-dead-beefdeadbeef";
static char *spDefaulFName = "AMLOGICDEV";
static char *spDefaultMName = "OTT-Defult";
static char *spDefaultEthInterface = "eth0";
static char *spDefaultWifiInterface = "wlan0";
#define ARRAY_SIZE(e) (sizeof(e) / sizeof(*(e)))

const char *params_interest[] = {PARAM_FRIENDLY_NAME, PARAM_MODEL_NAME};

static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_controller = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_network = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_rdkshell = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_system = nullptr;

using namespace Amlogic::Platform;
namespace WPEFramework {
namespace Plugin {

class PersistDialServerName : public Amlogic::Platform::PlatformEvent::Subscriber {
private:
  PersistDialServerName(const PersistDialServerName &) = delete;
  PersistDialServerName &operator=(const PersistDialServerName &) = delete;

  void onEvent(Amlogic::Platform::Event *event);

public:
  PersistDialServerName();
  virtual ~PersistDialServerName();
};


PersistDialServerName::PersistDialServerName() {
  PlatformEvent::GetInstance().subscribe("property", this);
}

PersistDialServerName::~PersistDialServerName() {
  PlatformEvent::GetInstance().unsubscribe("property", this);
}

void PersistDialServerName::onEvent(Amlogic::Platform::Event *event) {
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
  }
}
} // namespace Plugin
} // namespace WPEFramework

extern "C" {
int activateApp(const char *callsign, const char *url) {
  std::cout << "the launchurl is :" << std::string(url) << std::endl;
  uint32_t ret;
  // Passive mode
  if (_passivemode) {
    JsonObject broadParamsTemp;
    string paramsTemp;
    JsonObject broadParams;
    JsonObject broadResult;
    broadParamsTemp.Set("callsign", callsign);
    broadParamsTemp.Set("url", url);
    broadParamsTemp.ToString(paramsTemp);

    broadParams.Set("event", "dialLaunchRequest");
    broadParams.Set("params", paramsTemp.c_str());
    ret = g_wpe_system->Invoke(2000, "broadcastEvent", broadParams, broadResult);
    std::cout << "amldial-system broadcastEvent()_dialLaunchRequest return value:" << ret << std::endl;
    return 0;
  }

  // Wake On Lan
  JsonObject powerParams;
  JsonObject powerResult;
  ret = g_wpe_system->Invoke(2000, "getPowerState", powerParams, powerResult);
  std::cout << "amldial-system getPowerState() return value:" << ret << std::endl;
  if (WPEFramework::Core::ERROR_NONE == ret && !powerResult.Get("powerState").Value().compare("ON")) {
      // Do nothing
  } else {
      // Ask power management to power on
      powerParams.Set("powerState", "ON");
      powerParams.Set("standbyReason", "DIAL StartApplication");
      ret = g_wpe_system->Invoke(2000, "setPowerState", powerParams, powerResult);
      std::cout << "amldial-system setPowerState() return value:" << ret << std::endl;
  }

  // Active mode
  DIALStatus appStatus = getAppStatus(callsign);
  if (appStatus == kDIALStatusHide) {
    resumeApp(callsign);
    if (!strcmp(curAppForDial->handler, "Netflix")) {
      JsonObject systemCmdParams, systemCmdResult;
      systemCmdParams.Set("Command",url);
      std::string linkAppCallsign = callsign;
      linkAppCallsign.append(".1");
      ret =
          WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
              linkAppCallsign.c_str())
              .Invoke(2000, "systemcommand", systemCmdParams, systemCmdResult);
      std::cout << "amldial-netflix systemcommand() return value:" << ret << std::endl;
    }
  }

  if (appStatus == kDIALStatusStopped) {
    // Config the app to be activated
    if (!strcmp(curAppForDial->handler, "YouTube")) {
      JsonObject launchtype = JsonObject("{\"launchtype\": \"launch=dial\"}");
      if (!strcmp(curAppForDial->name, "YouTubeTV") && (strstr(url,"loader=yts")!= nullptr)) {
        launchtype.Set("url", url);
      }
      ret = g_wpe_controller->Set<JsonObject>(1000, std::string("configitem@") + callsign, launchtype);
      std::cout << "amldial-controller configItem@" <<  std::string(callsign) << "return value:" << ret << std::endl;
    }
    if (!strcmp(curAppForDial->handler, "Netflix")) {
      JsonObject queryString;
      queryString.Set("querystring",url);
      queryString.Set("launchtosuspend",false);
      ret = g_wpe_controller->Set<JsonObject>(1000, std::string("configitem@") + callsign, queryString);
      std::cout << "amldial-controller configItem@" <<  std::string(callsign) << "return value:" << ret << std::endl;
    }
    if (!strcmp(curAppForDial->handler, "Amazon")) {
      JsonObject launchtype;
      launchtype.Set("launch_in_background",false);
      ret = g_wpe_controller->Set<JsonObject>(1000, std::string("configitem@") + callsign, launchtype);
      std::cout << "amldial-controller configItem@" <<  std::string(callsign) << "return value:" << ret << std::endl;
    }
    // Activate the app
    JsonObject callsignObj = JsonObject(std::string("{\"callsign\": \"") + callsign + "\"}");
    ret =
        g_wpe_controller->Set<JsonObject>(1000, "activate", callsignObj);
    std::cout << "amldial-controller activate() return value:" << ret << std::endl;
  }
  if (!strcmp(curAppForDial->handler, "YouTube")) {
    std::string launchURL;
    launchURL = launchURL + "\"" + url + "&launch=dial" + "\"";
    std::cout << "amldial-launchURL value:" << launchURL << std::endl;
    JsonObject setUrlResult;
    std::string linkAppCallsign = callsign;
    linkAppCallsign.append(".1");
    ret =
        WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
            linkAppCallsign.c_str())
            .Invoke(2000, "deeplink", launchURL, setUrlResult);
    std::cout << "amldial-cobalt.. deeplink() return value:" << ret << std::endl;
  }
  // SetFocus
  JsonObject setFocusParams, setFocusResult;
  setFocusParams.Set("client",callsign);
  ret = g_wpe_rdkshell->Invoke(2000, "setFocus", setFocusParams, setFocusResult);
  std::cout << "amldial-rdkshell setFocus() return value:" << ret << std::endl;
  return 0;
}

int deActivateApp(const char *callsign) {
  // Passive mode
  if (_passivemode) {
    JsonObject broadParamsTemp;
    string paramsTemp;
    JsonObject broadParams;
    JsonObject broadResult;
    broadParamsTemp.Set("callsign", callsign);
    broadParamsTemp.ToString(paramsTemp);

    broadParams.Set("event", "dialStopRequest");
    broadParams.Set("params", paramsTemp.c_str());
    uint32_t ret = g_wpe_system->Invoke(2000, "broadcastEvent", broadParams, broadResult);
    std::cout << "amldial-system broadcastEvent()_dialStopRequest return value:" << ret << std::endl;
    _appHidden = false;
    return 0;
  }

  // Active mode
  JsonObject callsignObj = JsonObject(std::string("{\"callsign\": \"") + callsign + "\"}");
  uint32_t result =
      g_wpe_controller->Set<JsonObject>(2000, "deactivate", callsignObj);
  std::cout << "amldial-controller deactivate() return value:" << result << std::endl;
  _appHidden = false;
  return 0;
}

void changeMulticast(const JsonObject &s) {
  std::cout << "amldial-ip changed" << std::endl;
  addNewIpToMulticast();
}

int listenIpChange() {
  bool wpe_ready = false;
  while (!wpe_ready && ssdp_running) {
    g_wpe_network =
        new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
            "org.rdk.Network.1");
    usleep(1000000);
    int result;
    if ((result = g_wpe_network->Subscribe<JsonObject>(
             1000, "onIPAddressStatusChanged", &changeMulticast)) ==
        WPEFramework::Core::ERROR_NONE) {
      std::cout << "amldial-wpe ready\n" << std::endl;
      wpe_ready = true;
    } else {
      std::cout << "amldial-wpe not ready\n" << std::endl;
      delete g_wpe_network;
    }
  }
  // Link to required pullgins
  g_wpe_controller =
      new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
          "Controller.1");
  g_wpe_rdkshell =
      new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
          "org.rdk.RDKShell.1");
  g_wpe_system =
      new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
          "org.rdk.System.1");
  // usleep(1000000);
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
  string _friendlyname, _modelname, _eth_interface, _wifi_interface;
  if (Amlogic::Platform::Property::get("persist.dialserver.name", _friendlyname))
    snprintf(friendlyname, MAXSIZE, "%s", _friendlyname.c_str());
  else
    snprintf(friendlyname, MAXSIZE, "%s", spDefaulFName);
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
  static WPEFramework::Plugin::PersistDialServerName _obj;
}

DIALStatus getAppStatus(const char *callsign) {
  ASSERT(g_wpe_controller != nullptr);
  JsonArray getStatusResult;
  int result = g_wpe_controller->Get<JsonArray>(
      2000, std::string("status@") + callsign, getStatusResult);
  if (result == WPEFramework::Core::ERROR_NONE) {
    if (getStatusResult.Elements().Count() == 1) {
      JsonObject obj = getStatusResult[0].Object();
      if (obj.HasLabel("state")) {
        const std::string &state = obj["state"].String();
        std::cout << "amldial-" << callsign << " state is:" << state << std::endl;
        if(state == "resumed")
          return kDIALStatusRunning;
        else if(state == "suspended")
          return kDIALStatusHide;
        else if(state == "deactivated")
          return kDIALStatusStopped;
        else return kDIALStatusError;
      }
    }
  }
  std::cout << "amldial-getAppStatus() failed" << std::endl;
  return kDIALStatusError;
}

bool hideApp(const char* callsign) {
  // Passive mode
  if (_passivemode) {
    JsonObject broadParamsTemp;
    string paramsTemp;
    JsonObject broadParams;
    JsonObject broadResult;
    broadParamsTemp.Set("callsign", callsign);
    broadParamsTemp.ToString(paramsTemp);

    broadParams.Set("event", "dialHideRequest");
    broadParams.Set("params", paramsTemp.c_str());
    uint32_t ret = g_wpe_system->Invoke(2000, "broadcastEvent", broadParams, broadResult);
    std::cout << "amldial-system broadcastEvent()_dialHideRequest return value:" << ret << std::endl;
    _appHidden = true;
    return true;
  }

  // Active mode
  JsonObject invokeParams;
  JsonObject invokeResult;
  std::string invokeMethod;
  invokeParams.Set("callsign",callsign);
  uint32_t setStatus;
  if((setStatus = g_wpe_rdkshell->Invoke(2000, "suspend", invokeParams, invokeResult)) == WPEFramework::Core::ERROR_NONE) {
    _appHidden = true;
    return true;
  }
  else {
    std::cout << "amldial-rdkshell suspend() return value:" << setStatus << std::endl;
    return false;
  }
}

bool resumeApp(const char* callsign) {
  JsonObject invokeParams;
  JsonObject invokeResult;
  std::string invokeMethod;
  invokeParams.Set("callsign",callsign);
  invokeMethod = "launch";
  uint32_t setStatus;
  if((setStatus = g_wpe_rdkshell->Invoke(2000, invokeMethod.c_str(), invokeParams, invokeResult)) == WPEFramework::Core::ERROR_NONE) {
    invokeParams.Set("client",callsign);
    invokeMethod = "setFocus";
    setStatus = g_wpe_rdkshell->Invoke(2000, invokeMethod.c_str(), invokeParams, invokeResult);
    std::cout << "amldial-rdkshell setFocus() return value:" << setStatus << std::endl;
    _appHidden = false;
    return true;
  }
  else {
    std::cout << "amldial-rdkshell launch() return value:" << setStatus << std::endl;
    return false;
  }
}

int loadJson(const char* jsonFile) {
  uint32_t ret = 0;
  const std::string &path = jsonFile;
  WPEFramework::Core::File file(path);
  JsonObject jsonResult;
  if (!file.Open() || !jsonResult.IElement::FromFile(file)) {
    std::cout << "amldial-loadJson() fail to read jsonfile" << std::endl;
    ret = 1;
  }
  _passivemode = (jsonResult["configuration"].Object())["passivemode"].Boolean();
  appInfoList = (struct appInfo*)malloc(sizeof(struct appInfo));
  struct appInfo* temp =  appInfoList;
  JsonArray apps = (jsonResult["configuration"].Object())["apps"].Array();
  for (int i = 0; i < apps.Elements().Count(); i++) {
    JsonObject obj = apps[i].Object();
    struct appInfo* p = (struct appInfo*)malloc(sizeof(struct appInfo));
    p->name = strdup(obj["name"].String().c_str());
    p->handler = strdup(obj["handler"].String().c_str());
    p->callsign = strdup(obj["callsign"].String().c_str());
    p->hide = strdup(obj["hide"].String().c_str());
    p->url = strdup(obj["url"].String().c_str());
    p->next = nullptr;
    temp->next = p;
    temp = p;
  }
  return ret;
}

}