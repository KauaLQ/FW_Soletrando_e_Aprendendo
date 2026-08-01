#include "wifi_config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>

// ---------- Rede própria do ESP32 (portal de configuração) ----------
// Fica sempre no ar, com IP fixo, independente do WiFi da estação (STA)
static const char *AP_SSID  = "ESP32-Config";
static const char *AP_SENHA = "config1234"; // WPA2 exige 8+ caracteres

static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_MASCARA(255, 255, 255, 0);

static Preferences prefs;
static WebServer server(80);
static volatile uint32_t versaoCredenciais = 0;

// ---------- Páginas geradas dinamicamente ----------
static String paginaSalvo(const String &ssid) {
  String html = "<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuracao salva</title>";
  html += "<style>body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;background:linear-gradient(180deg,#0d47a1,#1976d2);";
  html += "color:#fff;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;padding:24px;box-sizing:border-box;text-align:center}";
  html += ".cartao{background:#1565c0;border-radius:20px;padding:32px 24px;max-width:360px;box-shadow:0 8px 24px rgba(0,0,0,.3)}";
  html += "h1{font-size:20px;margin:0 0 12px}p{font-size:15px;line-height:1.5;opacity:.92}";
  html += "a{color:#bbdefb;font-weight:600;text-decoration:none}</style></head><body>";
  html += "<div class='cartao'><h1>Credenciais salvas!</h1>";
  html += "<p>O dispositivo vai tentar se conectar a <b>" + ssid + "</b> agora.</p>";
  html += "<p>Se algo der errado, esta pagina continua disponivel neste mesmo endereco (192.168.4.1), pela rede " + String(AP_SSID) + ".</p>";
  html += "<p><a href='/'>&larr; Voltar</a></p></div></body></html>";
  return html;
}

// ---------- Handlers do servidor web ----------
static void tratarRaiz() {
  File arquivo = LittleFS.open("/index.html", "r");
  if (!arquivo) {
    server.send(500, "text/plain", "index.html nao encontrado no LittleFS. Envie os arquivos da pasta data/.");
    return;
  }
  server.streamFile(arquivo, "text/html");
  arquivo.close();
}

static void tratarSsidAtual() {
  server.send(200, "text/plain", wifiConfigObterSSID());
}

static void tratarSalvar() {
  String ssid = server.arg("ssid");
  String senha = server.arg("senha");
  ssid.trim();
 
  if (ssid.length() == 0) {
    server.send(400, "text/plain", "SSID nao pode ser vazio");
    return;
  }
 
  prefs.putString("ssid", ssid);
  prefs.putString("pass", senha);
  versaoCredenciais++;
 
  server.send(200, "text/html", paginaSalvo(ssid));
}

// ---------- API pública ----------
void wifiConfigInit(const String &ssidPadrao, const String &senhaPadrao) {
  prefs.begin("wifi_cfg", false);
  // Só grava os valores padrão do firmware se ainda não existir nada salvo
  // (ou seja, é a primeira vez que o dispositivo liga)
  if (!prefs.isKey("ssid")) {
    prefs.putString("ssid", ssidPadrao);
    prefs.putString("pass", senhaPadrao);
  }

  if (!LittleFS.begin(true)) { // true = formata sozinho se o filesystem vier vazio/corrompido
    Serial.println(F("[WIFI CONFIG] Falha ao montar o LittleFS"));
  }

  // AP sempre ativo junto com a estação (STA): a pagina de config nunca
  // fica inacessivel, independente do WiFi da estacao estar conectado
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_MASCARA);
  WiFi.softAP(AP_SSID, AP_SENHA);

  server.on("/", HTTP_GET, tratarRaiz);
  server.on("/ssid-atual", HTTP_GET, tratarSsidAtual);
  server.on("/salvar", HTTP_POST, tratarSalvar);
  server.begin();

  Serial.print(F("[WIFI CONFIG] Portal disponivel em http://"));
  Serial.print(AP_IP);
  Serial.print(F(" (rede "));
  Serial.print(AP_SSID);
  Serial.println(F(")"));
}

// Atende as requisições do servidor web de configuração (portal).
void wifiConfigTick() {
  server.handleClient();
}

// Credenciais atualmente salvas na NVS (as que devem ser usadas pra conectar)
String wifiConfigObterSSID() {
  return prefs.getString("ssid", "");
}

String wifiConfigObterSenha() {
  return prefs.getString("pass", "");
}

// Número de versão das credenciais salvas. incrementa a cada alteração pela página web.
uint32_t wifiConfigObterVersao() {
  return versaoCredenciais;
}

// IP do Access Point de configuração, pra mostrar na tela/logs
IPAddress wifiConfigObterIPPortal() {
  return AP_IP;
}