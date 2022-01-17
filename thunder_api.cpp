#include "thunder_api.h"
#include "aml_device_property.h"
#include <iostream>
#include <protocols/JSONRPCLink.h>

static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_contoller = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_network = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_rdkshell = nullptr;
static WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>
    *g_wpe_system = nullptr;

extern "C" {
int activateApp(const char *appName, const char *url) {

  uint32_t ret;
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

  JsonObject launchtype = JsonObject("{\"launchtype\": \"launch=dial\"}");
  ret = g_wpe_contoller->Set<JsonObject>(1000, "configitem@Cobalt", launchtype);
  std::cout << "amldial-controller configitem@Cobalt() return value:" << ret << std::endl;

  if (strcmp(appName, "youtube") == 0) {
    if (strcmp(getAppStatus("Cobalt"),"suspended") == 0) {
      resumeApp("Cobalt");
    }
    if (strcmp(getAppStatus("Cobalt"),"deactivated") == 0) {
      JsonObject callsignObj = JsonObject("{\"callsign\": \"Cobalt\"}");
      uint32_t result =
          g_wpe_contoller->Set<JsonObject>(1000, "activate", callsignObj);
      std::cout << "amldial-controller activate() return value:" << result << std::endl;
    }
    std::string launchURL;
    launchURL = launchURL + "\"" + url + "\"";
    std::cout << "amldial-launchURL value:" << launchURL << std::endl;
    JsonObject setUrlResult;
    uint32_t result =
        WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
            "Cobalt.1")
            .Invoke(2000, "deeplink", launchURL, setUrlResult);
    std::cout << "amldial-cobalt deeplink() return value:" << result << std::endl;
    return 0;
  } else
    return -1;
}

int deActivateApp(const char *appName) {
  if (strcmp(appName, "youtube") == 0) {
    JsonObject callsignObj = JsonObject("{\"callsign\": \"Cobalt\"}");
    uint32_t result =
        g_wpe_contoller->Set<JsonObject>(2000, "deactivate", callsignObj);
    std::cout << "amldial-controller deactivate() return value:" << result << std::endl;
    _appHidden = false;
    return 0;
  } else
    return -1;
}

void changeMulticast(const JsonObject &s) {
  std::cout << "amldial-ip changed" << std::endl;
  addNewIpToMulticast();
}

int listenIpChange() {
  bool wpe_ready = false;
  while (!wpe_ready) {
    g_wpe_network =
        new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
            "org.rdk.Network.1");
    int result;
    if ((result = g_wpe_network->Subscribe<JsonObject>(
             1000, "onIPAddressStatusChanged", &changeMulticast)) ==
        WPEFramework::Core::ERROR_NONE) {
      std::cout << "amldial-wpe ready\n" << std::endl;
      wpe_ready = true;
    } else {
      std::cout << "amldial-wpe not ready\n" << std::endl;
      usleep(1000000);
      delete g_wpe_network;
    }
  }
  g_wpe_contoller =
      new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
          "Controller.1");
  g_wpe_rdkshell =
      new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
          "org.rdk.RDKShell.1");
  g_wpe_system =
      new WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>(
          "org.rdk.System.1");
  return 0;
}

bool getDialName(char *name, char *ret) {
  // Get DIAL Server name , manufacturer name and model name
  if (!AmlDeviceGetProperty(name, ret, 128)) {
    return true;
  }
  return false;
}

 const char* getAppStatus(const char *callsign) {
  ASSERT(g_wpe_contoller != nullptr);
  JsonArray getStatusResult;
  int result = g_wpe_contoller->Get<JsonArray>(
      2000, std::string("status@") + callsign, getStatusResult);
  if (result == WPEFramework::Core::ERROR_NONE) {
    if (getStatusResult.Elements().Count() == 1) {
      JsonObject obj = getStatusResult[0].Object();
      if (obj.HasLabel("state")) {
        const char *status_str[] = {"resumed", "suspended", "deactivated"};
        const std::string &state = obj["state"].String();
        std::cout << "amldial-" << callsign << " state is:" << state << std::endl;
        for (int i=0; i<sizeof(status_str)/sizeof(status_str[0]); i++)
          if (state == status_str[i])
            return status_str[i];
      }
    } 
  }
  std::cout << "amldial-getAppStatus() failed" << std::endl;
  return "deactivated";
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

}
