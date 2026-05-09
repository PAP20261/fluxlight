/*
 * ═══════════════════════════════════════════════════════════════
 *  FluxLight — esp32_code.ino
 *  Firmware para ESP32 DevKit V4
 *  Controla Fita LED WS2812B + Proxy para Lâmpadas Shelly RGBW
 * ═══════════════════════════════════════════════════════════════
 *
 *  Hardware necessário:
 *    - ESP32 DevKit V4
 *    - Fita LED WS2812B 5m (~150 LEDs)
 *    - Fonte 5V 10A
 *    - Lâmpadas Shelly Bulb RGBW E27
 *
 *  Pinagem:
 *    GPIO 13 → DATA da fita LED WS2812B
 *    GND     → GND comum (fonte + fita + ESP32)
 *    5V      → alimentação direta da fonte (NÃO do ESP32)
 *
 *  Bibliotecas necessárias (instalar no Arduino IDE):
 *    - Adafruit NeoPixel  (by Adafruit)
 *    - Arduino core for ESP32 (espressif)
 *
 *  ──────────────────────────────────────────────────────────────
 *  ⚙️  CONFIGURAÇÃO — alterar os valores abaixo
 *  ──────────────────────────────────────────────────────────────
 *
 * ═══════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>

// ═══════════════════════════════════════════════════════════════
//  1. CONFIGURAÇÃO — ALTERAR AQUI
// ═══════════════════════════════════════════════════════════════

// ❗ Wi-Fi — colocar o nome e password da tua rede
const char* ssid     = "O_TEU_WIFI";       // ← ALTERAR
const char* password = "A_TUA_PASSWORD";   // ← ALTERAR

// ❗ Fita LED WS2812B
#define LED_PIN   13    // ← ALTERAR se usares outro GPIO
#define NUM_LEDS  300  // ← ALTERAR conforme o número real de LEDs

// ❗ IPs locais das lâmpadas Shelly RGBW
// (verificar na app Shelly ou no router)
String shellyIPs[] = {
  "192.168.1.101",   // ← Lâmpada 1 — ALTERAR
  "192.168.1.102",   // ← Lâmpada 2 — ALTERAR
};
const int NUM_SHELLY = 2;  // ← ALTERAR se adicionares mais lâmpadas

// ═══════════════════════════════════════════════════════════════
//  2. OBJETOS GLOBAIS
// ═══════════════════════════════════════════════════════════════

// Servidor HTTP na porta 80
WebServer server(80);

// Fita NeoPixel
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ─── Estado global ────────────────────────────────────────────
bool  ledOn         = false;
uint8_t ledR = 255, ledG = 106, ledB = 0;  // Cor atual (laranja)
uint8_t ledBrightness = 200;                // Brilho atual (0–255)
String  currentEffect = "";                 // Efeito ativo ("" = nenhum)

// ─── Efeito — variáveis de animação ──────────────────────────
uint32_t effectTimer   = 0;   // Millis do último frame de efeito
uint8_t  effectStep    = 0;   // Passo de animação
bool     effectRunning = false;

// ═══════════════════════════════════════════════════════════════
//  3. SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n[FluxLight] A iniciar...");

  // ── Inicializar fita LED ──────────────────────────────────
  strip.begin();
  strip.setBrightness(ledBrightness);
  strip.clear();
  strip.show();
  Serial.printf("[FluxLight] Fita LED: %d LEDs no GPIO %d\n", NUM_LEDS, LED_PIN);

  // ── Ligar ao Wi-Fi ────────────────────────────────────────
  WiFi.begin(ssid, password);
  Serial.print("[FluxLight] A ligar ao Wi-Fi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[FluxLight] Wi-Fi ligado!");
    Serial.print("[FluxLight] IP do ESP32: ");
    Serial.println(WiFi.localIP());
    Serial.println("  >> Copiar este IP para o script.js");
  } else {
    Serial.println("\n[FluxLight] Falha na ligação Wi-Fi! Verificar SSID/password.");
  }

  // ── Registar endpoints HTTP ───────────────────────────────
  setupRoutes();

  // ── Iniciar servidor ──────────────────────────────────────
  server.begin();
  Serial.println("[FluxLight] Servidor HTTP iniciado na porta 80");
  Serial.println("[FluxLight] Pronto!\n");

  // Efeito de boot: flash laranja breve
  bootAnimation();
}

// ═══════════════════════════════════════════════════════════════
//  4. LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  // Tratar pedidos HTTP
  server.handleClient();

  // Executar frame do efeito ativo (non-blocking)
  if (effectRunning) {
    runEffect();
  }
}

// ═══════════════════════════════════════════════════════════════
//  5. ROTAS HTTP (API REST)
// ═══════════════════════════════════════════════════════════════
void setupRoutes() {

  // ── CORS — necessário para o browser aceitar respostas ───
  // (o addHeader é chamado em cada handler)

  // ── Status ────────────────────────────────────────────────
  server.on("/status", HTTP_GET, []() {
    addCORSHeaders();
    String json = "{\"status\":\"ok\",\"ledOn\":" + String(ledOn ? "true" : "false") +
                  ",\"effect\":\"" + currentEffect + "\"}";
    server.send(200, "application/json", json);
  });

  // ── Fita LED — ON ─────────────────────────────────────────
  // GET /led/on
  server.on("/led/on", HTTP_GET, []() {
    addCORSHeaders();
    ledOn = true;
    effectRunning = false;
    currentEffect = "";
    applyLEDColor();
    server.send(200, "application/json", "{\"led\":\"on\"}");
    Serial.println("[LED] ON");
  });

  // ── Fita LED — OFF ────────────────────────────────────────
  // GET /led/off
  server.on("/led/off", HTTP_GET, []() {
    addCORSHeaders();
    ledOn = false;
    effectRunning = false;
    currentEffect = "";
    strip.clear();
    strip.show();
    server.send(200, "application/json", "{\"led\":\"off\"}");
    Serial.println("[LED] OFF");
  });

  // ── Fita LED — COR ────────────────────────────────────────
  // GET /led/color?r=255&g=0&b=0
  server.on("/led/color", HTTP_GET, []() {
    addCORSHeaders();
    ledR = server.arg("r").toInt();
    ledG = server.arg("g").toInt();
    ledB = server.arg("b").toInt();
    ledOn = true;
    effectRunning = false;
    currentEffect = "";
    applyLEDColor();
    String msg = "{\"r\":" + String(ledR) + ",\"g\":" + String(ledG) + ",\"b\":" + String(ledB) + "}";
    server.send(200, "application/json", msg);
    Serial.printf("[LED] Cor: R=%d G=%d B=%d\n", ledR, ledG, ledB);
  });

  // ── Fita LED — BRILHO ─────────────────────────────────────
  // GET /led/brightness?value=200
  server.on("/led/brightness", HTTP_GET, []() {
    addCORSHeaders();
    ledBrightness = constrain(server.arg("value").toInt(), 0, 255);
    strip.setBrightness(ledBrightness);
    if (ledOn && !effectRunning) applyLEDColor();
    else if (effectRunning) strip.show();  // atualizar frame atual
    server.send(200, "application/json", "{\"brightness\":" + String(ledBrightness) + "}");
    Serial.printf("[LED] Brilho: %d\n", ledBrightness);
  });

  // ── EFEITO — RAINBOW ──────────────────────────────────────
  // GET /effect/rainbow
  server.on("/effect/rainbow", HTTP_GET, []() {
    addCORSHeaders();
    startEffect("rainbow");
    server.send(200, "application/json", "{\"effect\":\"rainbow\"}");
  });

  // ── EFEITO — DISCO ────────────────────────────────────────
  // GET /effect/disco
  server.on("/effect/disco", HTTP_GET, []() {
    addCORSHeaders();
    startEffect("disco");
    server.send(200, "application/json", "{\"effect\":\"disco\"}");
  });

  // ── EFEITO — FIRE ─────────────────────────────────────────
  // GET /effect/fire
  server.on("/effect/fire", HTTP_GET, []() {
    addCORSHeaders();
    startEffect("fire");
    server.send(200, "application/json", "{\"effect\":\"fire\"}");
  });

  // ── EFEITO — PULSE ────────────────────────────────────────
  // GET /effect/pulse
  server.on("/effect/pulse", HTTP_GET, []() {
    addCORSHeaders();
    startEffect("pulse");
    server.send(200, "application/json", "{\"effect\":\"pulse\"}");
  });

  // ── EFEITO — WAVE ─────────────────────────────────────────
  // GET /effect/wave
  server.on("/effect/wave", HTTP_GET, []() {
    addCORSHeaders();
    startEffect("wave");
    server.send(200, "application/json", "{\"effect\":\"wave\"}");
  });

  // ── EFEITO — FADE ─────────────────────────────────────────
  // GET /effect/fade
  server.on("/effect/fade", HTTP_GET, []() {
    addCORSHeaders();
    startEffect("fade");
    server.send(200, "application/json", "{\"effect\":\"fade\"}");
  });

  // ── EFEITO — STOP ─────────────────────────────────────────
  // GET /effect/stop
  server.on("/effect/stop", HTTP_GET, []() {
    addCORSHeaders();
    effectRunning = false;
    currentEffect = "";
    effectStep    = 0;
    if (ledOn) applyLEDColor();
    else { strip.clear(); strip.show(); }
    server.send(200, "application/json", "{\"effect\":\"stopped\"}");
    Serial.println("[Efeito] Parado");
  });

  // ── SHELLY — TODAS ON ─────────────────────────────────────
  // GET /shelly/on
  server.on("/shelly/on", HTTP_GET, []() {
    addCORSHeaders();
    for (int i = 0; i < NUM_SHELLY; i++) {
      shellyRequest(i, "on");
    }
    server.send(200, "application/json", "{\"shelly\":\"all_on\"}");
  });

  // ── SHELLY — TODAS OFF ────────────────────────────────────
  // GET /shelly/off
  server.on("/shelly/off", HTTP_GET, []() {
    addCORSHeaders();
    for (int i = 0; i < NUM_SHELLY; i++) {
      shellyRequest(i, "off");
    }
    server.send(200, "application/json", "{\"shelly\":\"all_off\"}");
  });

  // ── SHELLY — INDIVIDUAL ON/OFF ────────────────────────────
  // GET /shelly/0/on  |  /shelly/0/off  |  /shelly/1/on  ...
  // (registar para cada lâmpada)
  for (int i = 0; i < NUM_SHELLY; i++) {
    int idx = i;  // capturar por valor para o lambda

    String pathOn  = "/shelly/" + String(idx) + "/on";
    String pathOff = "/shelly/" + String(idx) + "/off";

    server.on(pathOn.c_str(), HTTP_GET, [idx]() {
      addCORSHeaders();
      shellyRequest(idx, "on");
      server.send(200, "application/json", "{\"shelly\":" + String(idx) + ",\"state\":\"on\"}");
    });

    server.on(pathOff.c_str(), HTTP_GET, [idx]() {
      addCORSHeaders();
      shellyRequest(idx, "off");
      server.send(200, "application/json", "{\"shelly\":" + String(idx) + ",\"state\":\"off\"}");
    });

    // ── SHELLY — COR INDIVIDUAL ───────────────────────────
    // GET /shelly/0/color?r=255&g=0&b=0
    String pathColor = "/shelly/" + String(idx) + "/color";
    server.on(pathColor.c_str(), HTTP_GET, [idx]() {
      addCORSHeaders();
      int r = server.arg("r").toInt();
      int g = server.arg("g").toInt();
      int b = server.arg("b").toInt();
      shellySetColor(idx, r, g, b);
      server.send(200, "application/json", "{\"shelly\":" + String(idx) + ",\"color\":\"set\"}");
    });

    // ── SHELLY — BRILHO INDIVIDUAL ────────────────────────
    // GET /shelly/0/brightness?value=80
    String pathBrightness = "/shelly/" + String(idx) + "/brightness";
    server.on(pathBrightness.c_str(), HTTP_GET, [idx]() {
      addCORSHeaders();
      int val = server.arg("value").toInt();
      shellySetBrightness(idx, val);
      server.send(200, "application/json", "{\"shelly\":" + String(idx) + ",\"brightness\":" + String(val) + "}");
    });
  }

  // ── 404 ───────────────────────────────────────────────────
  server.onNotFound([]() {
    addCORSHeaders();
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });
}

// ═══════════════════════════════════════════════════════════════
//  6. CONTROLO DA FITA LED
// ═══════════════════════════════════════════════════════════════

/** Aplica a cor sólida atual a todos os LEDs. */
void applyLEDColor() {
  uint32_t color = strip.Color(ledR, ledG, ledB);
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// ═══════════════════════════════════════════════════════════════
//  7. EFEITOS (non-blocking com millis)
// ═══════════════════════════════════════════════════════════════

/** Inicia um efeito, reiniciando o passo de animação. */
void startEffect(const String& name) {
  currentEffect = name;
  effectRunning = true;
  effectStep    = 0;
  effectTimer   = 0;
  ledOn         = true;
  Serial.printf("[Efeito] Iniciado: %s\n", name.c_str());
}

/** Executa um frame do efeito ativo (chamado no loop). */
void runEffect() {
  uint32_t now = millis();

  if (currentEffect == "rainbow") {
    if (now - effectTimer >= 20) {
      effectTimer = now;
      rainbowFrame();
      effectStep++;
    }
  }
  else if (currentEffect == "disco") {
    if (now - effectTimer >= 80) {
      effectTimer = now;
      discoFrame();
    }
  }
  else if (currentEffect == "fire") {
    if (now - effectTimer >= 30) {
      effectTimer = now;
      fireFrame();
    }
  }
  else if (currentEffect == "pulse") {
    if (now - effectTimer >= 15) {
      effectTimer = now;
      pulseFrame();
    }
  }
  else if (currentEffect == "wave") {
    if (now - effectTimer >= 25) {
      effectTimer = now;
      waveFrame();
      effectStep++;
    }
  }
  else if (currentEffect == "fade") {
    if (now - effectTimer >= 20) {
      effectTimer = now;
      fadeFrame();
      effectStep++;
    }
  }
}

// ── Rainbow: ciclo de cores ao longo da fita ──────────────────
void rainbowFrame() {
  for (int i = 0; i < NUM_LEDS; i++) {
    // Distribuir o espectro ao longo dos LEDs + deslocar pelo passo
    uint16_t hue = (uint32_t)(i * 65536L / NUM_LEDS + (uint32_t)effectStep * 256) & 0xFFFF;
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
  }
  strip.show();
}

// ── Disco: cores aleatórias rápidas ───────────────────────────
void discoFrame() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(random(256), random(256), random(256)));
  }
  strip.show();
}

// ── Fire: simulação de chama (tons vermelhos/laranjas/amarelos) ─
void fireFrame() {
  static uint8_t heat[150];  // array de calor por LED

  // Arrefecer cada célula aleatoriamente
  for (int i = 0; i < NUM_LEDS; i++) {
    if (heat[i] > 0) {
      heat[i] = max(0, heat[i] - random(2, 6));
    }
  }

  // Propagar calor da base para o topo
  for (int i = NUM_LEDS - 1; i > 1; i--) {
    heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2]) / 3;
  }

  // Ignição aleatória na base
  if (random(255) < 120) {
    int pos = random(NUM_LEDS / 4);
    heat[pos] = min(255, heat[pos] + random(160, 255));
  }

  // Mapear calor para cor (preto → vermelho → laranja → amarelo → branco)
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t h = heat[i];
    uint8_t r, g, b;
    if (h < 85) {
      r = h * 3; g = 0; b = 0;
    } else if (h < 170) {
      r = 255; g = (h - 85) * 3; b = 0;
    } else {
      r = 255; g = 255; b = (h - 170) * 3;
    }
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// ── Pulse: pulsação suave de brilho ───────────────────────────
void pulseFrame() {
  static uint8_t pulseVal  = 0;
  static bool    pulseUp   = true;

  if (pulseUp) {
    pulseVal += 3;
    if (pulseVal >= 252) { pulseVal = 252; pulseUp = false; }
  } else {
    pulseVal -= 3;
    if (pulseVal <= 3)   { pulseVal = 3;   pulseUp = true;  }
  }

  uint32_t color = strip.Color(
    (uint8_t)(ledR * pulseVal / 255),
    (uint8_t)(ledG * pulseVal / 255),
    (uint8_t)(ledB * pulseVal / 255)
  );

  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
  strip.show();
}

// ── Wave: onda de luz a percorrer a fita ─────────────────────
void waveFrame() {
  for (int i = 0; i < NUM_LEDS; i++) {
    // Seno do ângulo para criar onda suave
    float angle = (i + effectStep) * 0.15f;
    uint8_t brightness = (uint8_t)((sin(angle) * 0.5f + 0.5f) * 255);
    strip.setPixelColor(i, strip.Color(
      (uint8_t)(ledR * brightness / 255),
      (uint8_t)(ledG * brightness / 255),
      (uint8_t)(ledB * brightness / 255)
    ));
  }
  strip.show();
}

// ── Fade: transição suave entre cores do espectro ────────────
void fadeFrame() {
  // Ciclo HSV completo ao longo do tempo
  uint16_t hue = (uint32_t)effectStep * 180;
  uint32_t color = strip.gamma32(strip.ColorHSV(hue, 255, ledBrightness));
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
  strip.show();
}

// ═══════════════════════════════════════════════════════════════
//  8. CONTROLO SHELLY RGBW (via HTTPClient)
// ═══════════════════════════════════════════════════════════════

/**
 * Liga ou desliga uma lâmpada Shelly individual.
 * API Shelly: GET http://<IP>/light/0?turn=on|off
 */
void shellyRequest(int index, const String& action) {
  if (index >= NUM_SHELLY) return;

  HTTPClient http;
  String url = "http://" + shellyIPs[index] + "/light/0?turn=" + action;

  http.begin(url);
  http.setTimeout(3000);

  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    Serial.printf("[Shelly %d] %s — OK\n", index + 1, action.c_str());
  } else {
    Serial.printf("[Shelly %d] Erro HTTP: %d — URL: %s\n", index + 1, code, url.c_str());
  }

  http.end();
}

/**
 * Define a cor de uma lâmpada Shelly RGBW.
 * API Shelly: GET http://<IP>/light/0?red=255&green=0&blue=0&gain=100&white=0&turn=on
 */
void shellySetColor(int index, int r, int g, int b) {
  if (index >= NUM_SHELLY) return;

  HTTPClient http;
  String url = "http://" + shellyIPs[index] +
               "/light/0?red=" + String(r) +
               "&green=" + String(g) +
               "&blue=" + String(b) +
               "&white=0&gain=100&turn=on";

  http.begin(url);
  http.setTimeout(3000);
  int code = http.GET();
  Serial.printf("[Shelly %d] Set color R%d G%d B%d — HTTP %d\n", index + 1, r, g, b, code);
  http.end();
}

/**
 * Define o brilho de uma lâmpada Shelly RGBW (0–100).
 * API Shelly: GET http://<IP>/light/0?gain=<value>
 */
void shellySetBrightness(int index, int brightness) {
  if (index >= NUM_SHELLY) return;

  int gain = constrain(brightness, 0, 100);
  HTTPClient http;
  String url = "http://" + shellyIPs[index] + "/light/0?gain=" + String(gain);

  http.begin(url);
  http.setTimeout(3000);
  int code = http.GET();
  Serial.printf("[Shelly %d] Brilho: %d%% — HTTP %d\n", index + 1, gain, code);
  http.end();
}

// ═══════════════════════════════════════════════════════════════
//  9. UTILITÁRIOS
// ═══════════════════════════════════════════════════════════════

/** Adiciona os cabeçalhos CORS a todas as respostas HTTP. */
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

/** Animação de arranque — flash laranja → apagar. */
void bootAnimation() {
  // Acender tudo a laranja
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(255, 106, 0));
  strip.show();
  delay(400);

  // Apagar com fade
  for (int b = 255; b >= 0; b -= 5) {
    strip.setBrightness(b);
    strip.show();
    delay(5);
  }

  strip.clear();
  strip.show();
  strip.setBrightness(ledBrightness);
  Serial.println("[FluxLight] Boot animation completa.");
}
