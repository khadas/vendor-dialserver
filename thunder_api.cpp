#include "thunder_api.h"
#include "aml_device_property.h"
#include <iostream>
#include <protocols/JSONRPCLink.h>

#define PARAM_FRIENDLY_NAME "setup_friendly_name"
#define PARAM_MODEL_NAME "information_model_name"
#define MAXSIZE 256
static char *spDefaultUuid = "deadbeef-wlfy-beef-dead-beefdeadbeef";
static char *spDefaulFName = "AMLOGICDEV";
static char *spDefaultMName = "OTT-Defult";
#define ARRAY_SIZE(e) (sizeof(e) / sizeof(*(e)))

const char *params_interest[] = {PARAM_FRIENDLY_NAME, PARAM_MODEL_NAME};

static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_contoller = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_network = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_rdkshell = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_system = nullptr;

extern "C" {
int activateApp(const char *callsign, const char *url) {
  std::cout << "the launchurl is :" << std::string(url) << std::endl;
  uint32_t ret;
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
      if ((!strcmp(curAppForDial->name, "YouTubeTV") || !strcmp(curAppForDial->name, "YouTubeKids"))
          && (strstr(url,"loader=yts")!= nullptr)) {
        launchtype.Set("url", url);
      }
      ret = g_wpe_contoller->Set<JsonObject>(1000, std::string("configitem@") + callsign, launchtype);
      std::cout << "amldial-controller configItem@" <<  std::string(callsign) << "return value:" << ret << std::endl;
    }
    if (!strcmp(curAppForDial->handler, "Netflix")) {
      JsonObject queryString;
      queryString.Set("querystring",url);
      queryString.Set("launchtosuspend",false);
      ret = g_wpe_contoller->Set<JsonObject>(1000, std::string("configitem@") + callsign, queryString);
      std::cout << "amldial-controller configItem@" <<  std::string(callsign) << "return value:" << ret << std::endl;
    }
    // Activate the app
    JsonObject callsignObj = JsonObject(std::string("{\"callsign\": \"") + callsign + "\"}");
    ret =
        g_wpe_contoller->Set<JsonObject>(1000, "activate", callsignObj);
    std::cout << "amldial-controller activate() return value:" << ret << std::endl;
  }
  if (!strcmp(curAppForDial->handler, "YouTube")) {
    std::string launchURL;
    launchURL = launchURL + "\"" + url + "\"";
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
  JsonObject callsignObj = JsonObject(std::string("{\"callsign\": \"") + callsign + "\"}");
  uint32_t result =
      g_wpe_contoller->Set<JsonObject>(2000, "deactivate", callsignObj);
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
  g_wpe_contoller =
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

void modifyDialProperty(const JsonObject &param) {
  std::cout << "amldial-device property changed" << std::endl;
  if (param.HasLabel(params_interest[0]) && !param[params_interest[0]].String().empty())
    snprintf(friendly_name, MAXSIZE, "%s", param[params_interest[0]].String().c_str());
  if (param.HasLabel(params_interest[1]) && !param[params_interest[2]].String().empty())
    snprintf(model_name, MAXSIZE, "%s", param[params_interest[1]].String().c_str());
}

void setDialProperty(char *friendlyname, char *uuid, char *modelname) {
  char mac_addr[18] = {0};
  // get_mac(mac_addr, "eth0") ? strcat(uuid, mac_addr) : strncpy(uuid, spDefaultUuid, MAXSIZE-1);
  if (get_mac(mac_addr, "eth0"))
    strcat(uuid, mac_addr);
  else 
    snprintf(uuid, MAXSIZE, "%s", spDefaultUuid);
  if (AmlDeviceGetProperty("DIALSERVER_NAME", friendlyname, MAXSIZE))
    snprintf(friendlyname, MAXSIZE, "%s", spDefaulFName);
  if (AmlDeviceGetProperty("MODEL_NAME", modelname, MAXSIZE))
    snprintf(modelname, MAXSIZE, "%s", spDefaultMName);

  JsonArray invokeArr;
  JsonObject invokeResult;
  for (int i = 0; i < ARRAY_SIZE(params_interest); i++)
    invokeArr.Add(JsonValue(params_interest[i]));
  uint32_t ret = g_wpe_system->Invoke<JsonArray, JsonObject>(2000, "getCfgParams", invokeArr, invokeResult);
  if (ret != WPEFramework::Core::ERROR_NONE || !invokeResult.HasLabel("params")) {
    std::cout << "amldial-getCfgParams() failed, return value:" << ret << std::endl;
    fprintf(stderr,"in setDialProperty(),the properties: friendlyname: %s, uuid: %s, modelname: %s\n", friendlyname, uuid, modelname);
    return;
  }
  JsonObject cfgParams = invokeResult["params"].Object();
  // Match the dial properties with cfgparams: friendlyname, modelname
  if (!cfgParams[params_interest[0]].String().empty())
    snprintf(friendlyname, MAXSIZE, "%s", cfgParams[params_interest[0]].String().c_str());
  if (!cfgParams[params_interest[1]].String().empty())
    snprintf(modelname, MAXSIZE, "%s", cfgParams[params_interest[1]].String().c_str());

  if ((ret = g_wpe_system->Subscribe<JsonObject>(
             1000, "cfgParamsChanged", &modifyDialProperty)) ==
        WPEFramework::Core::ERROR_NONE) {
      std::cout << "amldial-subscribe cfgParamsChanged success.\n" << std::endl;
  }
  fprintf(stderr,"in setDialProperty(),the properties: friendlyname: %s, uuid: %s, modelname: %s\n", friendlyname, uuid, modelname);
}

DIALStatus getAppStatus(const char *callsign) {
  ASSERT(g_wpe_contoller != nullptr);
  JsonArray getStatusResult;
  int result = g_wpe_contoller->Get<JsonArray>(
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
