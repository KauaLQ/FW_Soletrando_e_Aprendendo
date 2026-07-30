#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>

void wifiConfigInit(const String &ssidPadrao, const String &senhaPadrao);
void wifiConfigTick();
String wifiConfigObterSSID();
String wifiConfigObterSenha();
bool wifiConfigConsumirFlagAtualizada();
IPAddress wifiConfigObterIPPortal();

#endif