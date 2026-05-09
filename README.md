# 💡 FluxLight — Sistema de Iluminação Inteligente
**Prova de Aptidão Profissional (PAP)**

---

## 📁 Estrutura do Projeto

```
FluxLight/
├── index.html        ← Interface web (estrutura HTML5)
├── style.css         ← Estilos visuais (dark mode, responsivo)
├── script.js         ← Lógica JavaScript (comunicação com ESP32)
├── esp32_code.ino    ← Firmware Arduino para ESP32 DevKit V4
└── README.md         ← Este ficheiro
```

---

## ⚙️ Configuração Rápida

### 1. Firmware ESP32 (`esp32_code.ino`)

Abrir no Arduino IDE e alterar:

```cpp
// Wi-Fi
const char* ssid     = "O_TEU_WIFI";       // ← Nome da rede
const char* password = "A_TUA_PASSWORD";   // ← Password

// Fita LED
#define LED_PIN   13    // ← GPIO (padrão: 13)
#define NUM_LEDS  150   // ← Número de LEDs da fita

// IPs das Shelly
String shellyIPs[] = {
  "192.168.1.101",   // ← IP Lâmpada 1
  "192.168.1.102",   // ← IP Lâmpada 2
};
```

**Bibliotecas necessárias (Arduino IDE → Gestor de Bibliotecas):**
- `Adafruit NeoPixel` by Adafruit
- Placa: `ESP32 Dev Module` (via Gestor de Placas: espressif)

**Após gravar:** Abrir o Serial Monitor (115200 baud) e copiar o IP do ESP32.

---

### 2. Aplicação Web (`script.js`)

Alterar no topo do ficheiro:

```js
const ESP32_IP  = "http://192.168.1.100";  // ← IP do ESP32

const SHELLY_IPS = [
  "192.168.1.101",  // ← IP Lâmpada 1
  "192.168.1.102",  // ← IP Lâmpada 2
];
```

---

## 🔌 Pinagem ESP32

| Ligação | Pino ESP32 | Pino Fita / Fonte |
|---------|-----------|-------------------|
| DATA    | GPIO 13   | DIN da fita WS2812B |
| GND     | GND       | GND da fonte + GND da fita |
| 5V      | —         | 5V direto da fonte 5V 10A |

> ⚠️ **NÃO alimentar a fita pelo 5V do ESP32.** Usar sempre a fonte 5V 10A diretamente.

---

## 🌐 API REST do ESP32

| Endpoint | Descrição |
|----------|-----------|
| `GET /status` | Estado atual (JSON) |
| `GET /led/on` | Ligar fita LED |
| `GET /led/off` | Desligar fita LED |
| `GET /led/color?r=255&g=0&b=0` | Definir cor RGB |
| `GET /led/brightness?value=200` | Brilho (0–255) |
| `GET /effect/rainbow` | Efeito Rainbow |
| `GET /effect/disco` | Efeito Disco |
| `GET /effect/fire` | Efeito Fire |
| `GET /effect/pulse` | Efeito Pulse |
| `GET /effect/wave` | Efeito Wave |
| `GET /effect/fade` | Efeito Fade |
| `GET /effect/stop` | Parar efeito |
| `GET /shelly/on` | Ligar todas as Shelly |
| `GET /shelly/off` | Desligar todas as Shelly |
| `GET /shelly/0/on` | Ligar Lâmpada 1 |
| `GET /shelly/0/off` | Desligar Lâmpada 1 |
| `GET /shelly/0/color?r=255&g=255&b=255` | Cor Lâmpada 1 |
| `GET /shelly/0/brightness?value=80` | Brilho Lâmpada 1 (0–100%) |

---

## 📡 API Shelly RGBW (referência)

```
http://<IP_SHELLY>/light/0?turn=on
http://<IP_SHELLY>/light/0?turn=off
http://<IP_SHELLY>/light/0?red=255&green=0&blue=0&white=0&gain=100&turn=on
http://<IP_SHELLY>/light/0?gain=80
```

---

## 🚀 Como Utilizar

1. Gravar o `esp32_code.ino` no ESP32 via Arduino IDE
2. Verificar o IP no Serial Monitor (115200 baud)
3. Atualizar `ESP32_IP` no `script.js`
4. Abrir o `index.html` no browser (mesma rede Wi-Fi)
5. Controlar tudo pela interface web

**GitHub Pages:** Fazer upload de `index.html`, `style.css` e `script.js`. O ESP32 tem de estar acessível na mesma rede.

---

## 🛠️ Hardware

| Componente | Especificação |
|-----------|--------------|
| Microcontrolador | ESP32 DevKit V4 |
| Fita LED | WS2812B 5m ~150 LEDs |
| Fonte de alimentação | 5V 10A |
| Lâmpadas inteligentes | Shelly Bulb RGBW E27 Wi-Fi |

---

*FluxLight — PAP 2024/2025*
