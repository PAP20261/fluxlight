/* ═══════════════════════════════════════════════════════════════
   FluxLight — script.js
   Controlo da Fita LED WS2812B (via ESP32) e Lâmpadas Shelly RGBW
   ═══════════════════════════════════════════════════════════════

   ──────────────────────────────────────────────────────────────
   ⚙️  CONFIGURAÇÃO — alterar aqui antes de usar
   ──────────────────────────────────────────────────────────────
   1. ESP32_IP       → IP do ESP32 (ver no Serial Monitor do Arduino IDE)
   2. SHELLY_IPS     → IPs locais de cada lâmpada Shelly
   ══════════════════════════════════════════════════════════════= */

// ─── CONFIGURAÇÃO PRINCIPAL ────────────────────────────────────
// ❗ ALTERAR: IP do ESP32 (verificar no Serial Monitor após arranque)
const ESP32_IP = "http://192.168.1.100";

// ❗ ALTERAR: IPs locais das lâmpadas Shelly RGBW E27
const SHELLY_IPS = [
  "192.168.1.101",  // Lâmpada 1
  "192.168.1.102",  // Lâmpada 2
];
// ──────────────────────────────────────────────────────────────

// ─── ESTADO DA APLICAÇÃO ──────────────────────────────────────
const state = {
  led:     { on: false, color: "#ff6a00", brightness: 200 },
  bulbs:   [
    { on: false, color: "#ffffff", brightness: 100 },  // Lâmpada 1 (índice 0)
    { on: false, color: "#ffffff", brightness: 100 },  // Lâmpada 2 (índice 1)
  ],
  effect:  null,          // Efeito atualmente ativo
  master:  false,         // Estado global ON/OFF
  esp32Ok: false,         // Ligação ao ESP32 estabelecida
};

// Timeout para pedidos HTTP (ms)
const HTTP_TIMEOUT = 4000;

// ═══════════════════════════════════════════════════════════════
// 1. INICIALIZAÇÃO
// ═══════════════════════════════════════════════════════════════
document.addEventListener("DOMContentLoaded", () => {
  // Inicializar ícones Lucide
  if (window.lucide) lucide.createIcons();

  // Verificar ligação ao ESP32
  checkConnection();

  // Atualizar interface com estado inicial
  syncUI();

  // Verificar ligação periodicamente (a cada 15 s)
  setInterval(checkConnection, 15000);
});

// ═══════════════════════════════════════════════════════════════
// 2. COMUNICAÇÃO HTTP COM O ESP32
// ═══════════════════════════════════════════════════════════════

/**
 * Envia pedido GET ao ESP32 com tratamento de erro e timeout.
 * @param {string} endpoint  — ex: "/led/on"
 * @param {string} desc      — descrição para notificação
 * @returns {Promise<boolean>}
 */
async function esp32Request(endpoint, desc = "") {
  const url = `${ESP32_IP}${endpoint}`;

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), HTTP_TIMEOUT);

    const res = await fetch(url, {
      method: "GET",
      signal: controller.signal,
      mode: "cors",
    });

    clearTimeout(timer);

    if (!res.ok) throw new Error(`HTTP ${res.status}`);

    if (desc) showToast(`✅ ${desc}`, "success");
    setConnectionStatus(true);
    return true;

  } catch (err) {
    const msg = err.name === "AbortError" ? "Timeout — ESP32 não respondeu" : `Erro: ${err.message}`;
    if (desc) showToast(`❌ ${msg}`, "error");
    console.warn(`[FluxLight] ESP32 request failed: ${url}`, err);
    setConnectionStatus(false);
    return false;
  }
}

/**
 * Verifica se o ESP32 está acessível (ping ao endpoint raiz).
 */
async function checkConnection() {
  try {
    const controller = new AbortController();
    setTimeout(() => controller.abort(), 3000);

    const res = await fetch(`${ESP32_IP}/status`, {
      method: "GET",
      signal: controller.signal,
      mode: "cors",
    });

    setConnectionStatus(res.ok || res.status === 404); // 404 = ESP32 respondeu mas rota não existe

  } catch {
    setConnectionStatus(false);
  }
}

// ═══════════════════════════════════════════════════════════════
// 3. FITA LED WS2812B
// ═══════════════════════════════════════════════════════════════

/**
 * Liga ou desliga a fita LED.
 * Endpoints: GET /led/on  |  GET /led/off
 */
async function toggleLED(isOn) {
  state.led.on = isOn;
  const endpoint = isOn ? "/led/on" : "/led/off";
  const desc = isOn ? "Fita LED ligada" : "Fita LED desligada";
  await esp32Request(endpoint, desc);
  syncUI();
}

/**
 * Aplica a cor selecionada à fita LED.
 * Endpoint: GET /led/color?r=255&g=100&b=0
 */
async function applyLEDColor() {
  const hex    = document.getElementById("ledColor").value;
  const { r, g, b } = hexToRGB(hex);

  state.led.color = hex;
  updateColorBars();

  await esp32Request(`/led/color?r=${r}&g=${g}&b=${b}`, `Cor aplicada (${hex.toUpperCase()})`);
}

/**
 * Aplica o brilho à fita LED.
 * Endpoint: GET /led/brightness?value=200
 */
async function applyLEDBrightness(value) {
  state.led.brightness = parseInt(value);
  await esp32Request(`/led/brightness?value=${value}`, `Brilho: ${value}`);
}

// ═══════════════════════════════════════════════════════════════
// 4. LÂMPADAS SHELLY RGBW (via ESP32 proxy ou diretamente)
// ═══════════════════════════════════════════════════════════════
// O ESP32 age como proxy para as Shelly.
// Endpoint global: GET /shelly/on  |  /shelly/off
// Para controlo individual, o ESP32 deve reencaminhar para o IP correto.
// (Alternativa: chamar a Shelly diretamente no mesmo CORS; ver comentários abaixo)

/**
 * Liga ou desliga uma lâmpada Shelly individual.
 * @param {number} index  — 1 ou 2 (número da lâmpada, começa em 1)
 * @param {boolean} isOn
 */
async function toggleBulb(index, isOn) {
  const i = index - 1;  // índice 0-based no array
  state.bulbs[i].on = isOn;

  // Opção A: via ESP32 proxy (endpoint com índice)
  await esp32Request(
    isOn ? `/shelly/${i}/on` : `/shelly/${i}/off`,
    isOn ? `Lâmpada ${index} ligada` : `Lâmpada ${index} desligada`
  );

  // Opção B (comentado): chamar Shelly diretamente (mesmo segmento de rede)
  // const ip = SHELLY_IPS[i];
  // const action = isOn ? "on" : "off";
  // await fetch(`http://${ip}/light/0?turn=${action}`, { mode: "no-cors" });

  syncUI();
}

/**
 * Liga todas as lâmpadas Shelly.
 * Endpoint: GET /shelly/on
 */
async function shellyAllOn() {
  state.bulbs.forEach(b => (b.on = true));

  // Atualizar toggles
  document.getElementById("bulb1Toggle").checked = true;
  document.getElementById("bulb2Toggle").checked = true;

  await esp32Request("/shelly/on", "Todas as lâmpadas ligadas");
  syncUI();
}

/**
 * Desliga todas as lâmpadas Shelly.
 * Endpoint: GET /shelly/off
 */
async function shellyAllOff() {
  state.bulbs.forEach(b => (b.on = false));
  document.getElementById("bulb1Toggle").checked = false;
  document.getElementById("bulb2Toggle").checked = false;

  await esp32Request("/shelly/off", "Todas as lâmpadas desligadas");
  syncUI();
}

/**
 * Aplica cor a uma lâmpada Shelly.
 * Endpoint: GET /shelly/{index}/color?r=255&g=255&b=255
 */
async function applyBulbColor(index) {
  const i   = index - 1;
  const hex = document.getElementById(`bulb${index}Color`).value;
  const { r, g, b } = hexToRGB(hex);

  state.bulbs[i].color = hex;
  document.getElementById(`bulb${index}ColorBar`).style.background = hex;

  await esp32Request(`/shelly/${i}/color?r=${r}&g=${g}&b=${b}`, `Cor lâmpada ${index}`);
}

/**
 * Aplica brilho a uma lâmpada Shelly (0–100%).
 * Endpoint: GET /shelly/{index}/brightness?value=80
 */
async function applyBulbBrightness(index, value) {
  const i = index - 1;
  state.bulbs[i].brightness = parseInt(value);
  await esp32Request(`/shelly/${i}/brightness?value=${value}`, `Brilho lâmpada ${index}: ${value}%`);
}

// ═══════════════════════════════════════════════════════════════
// 5. EFEITOS LED
// ═══════════════════════════════════════════════════════════════
// Endpoints:
//   GET /effect/rainbow
//   GET /effect/disco
//   GET /effect/fire
//   GET /effect/pulse
//   GET /effect/wave
//   GET /effect/fade
//   GET /effect/stop  (parar efeito)

/**
 * Ativa um efeito na fita LED.
 * @param {string} name  — nome do efeito (rainbow, disco, fire, pulse, wave, fade)
 */
async function applyEffect(name) {
  state.effect = name;

  // Limpar seleção anterior
  document.querySelectorAll(".effect-card").forEach(el => el.classList.remove("active"));

  // Marcar card ativo
  const card = document.getElementById(`fx-${name}`);
  if (card) card.classList.add("active");

  // Atualizar stat dashboard
  document.getElementById("statEffect").textContent = capitalize(name);

  // Enviar para ESP32
  await esp32Request(`/effect/${name}`, `Efeito "${capitalize(name)}" ativado`);
}

/**
 * Para o efeito atual e restaura a fita LED à cor sólida.
 */
async function stopEffect() {
  state.effect = null;
  document.querySelectorAll(".effect-card").forEach(el => el.classList.remove("active"));
  document.getElementById("statEffect").textContent = "Nenhum";
  await esp32Request("/effect/stop", "Efeito parado");
}

// ═══════════════════════════════════════════════════════════════
// 6. CENAS RÁPIDAS
// ═══════════════════════════════════════════════════════════════

/**
 * Aplica uma cena predefinida (cor + brilho).
 * @param {string} scene  — relax | focus | party | night
 */
async function applyScene(scene) {
  const scenes = {
    relax:  { color: "#ff7700", brightness: 120, effect: null,       shellyColor: "#ff4400", desc: "🌅 Modo Relaxar" },
    focus:  { color: "#ffffff", brightness: 255, effect: null,       shellyColor: "#ffffff", desc: "💡 Modo Foco" },
    party:  { color: "#ff00ff", brightness: 255, effect: "rainbow",  shellyColor: "#00ff88", desc: "🎉 Modo Festa" },
    night:  { color: "#ff1100", brightness: 30,  effect: "fade",     shellyColor: "#220000", desc: "🌙 Modo Noite" },
  };

  const cfg = scenes[scene];
  if (!cfg) return;

  // Aplicar cor e brilho na fita LED
  setColor("led", cfg.color);
  document.getElementById("ledBrightness").value = cfg.brightness;
  updateBrightLabel(cfg.brightness, "ledBrightVal");
  await esp32Request(`/led/brightness?value=${cfg.brightness}`);

  // Ativar efeito se definido
  if (cfg.effect) {
    await applyEffect(cfg.effect);
  } else {
    await stopEffect();
  }

  showToast(`${cfg.desc} aplicado`, "success");
}

// ═══════════════════════════════════════════════════════════════
// 7. CONTROLO MESTRE (liga/desliga tudo)
// ═══════════════════════════════════════════════════════════════
async function masterToggle() {
  state.master = !state.master;

  const btn = document.getElementById("masterBtn");
  btn.classList.toggle("on", state.master);

  if (state.master) {
    // Ligar tudo
    document.getElementById("ledToggle").checked = true;
    await toggleLED(true);
    await shellyAllOn();
    showToast("⚡ Tudo ligado", "success");
  } else {
    // Desligar tudo
    document.getElementById("ledToggle").checked = false;
    await toggleLED(false);
    await shellyAllOff();
    await stopEffect();
    showToast("🔌 Tudo desligado", "info");
  }
}

// ═══════════════════════════════════════════════════════════════
// 8. INTERFACE — NAVEGAÇÃO ENTRE SECÇÕES
// ═══════════════════════════════════════════════════════════════

const SECTION_TITLES = {
  dashboard: { title: "Dashboard",   sub: "Visão geral do sistema" },
  strip:     { title: "Fita LED",    sub: "WS2812B · ESP32 GPIO 13 · 150 LEDs" },
  bulbs:     { title: "Lâmpadas",    sub: "Shelly RGBW E27 · Controlo Wi-Fi" },
  effects:   { title: "Efeitos",     sub: "Animações dinâmicas na fita LED" },
  settings:  { title: "Definições",  sub: "Configuração de IPs e parâmetros" },
};

function showSection(name, navEl) {
  // Esconder todas as secções
  document.querySelectorAll(".section").forEach(s => s.classList.add("hidden"));

  // Mostrar secção alvo
  const target = document.getElementById(`section-${name}`);
  if (target) target.classList.remove("hidden");

  // Atualizar nav
  document.querySelectorAll(".nav-item").forEach(n => n.classList.remove("active"));
  if (navEl) navEl.classList.add("active");

  // Atualizar títulos
  const t = SECTION_TITLES[name] || { title: name, sub: "" };
  document.getElementById("pageTitle").textContent = t.title;
  document.getElementById("pageSub").textContent   = t.sub;

  // Fechar sidebar mobile
  if (window.innerWidth <= 700) toggleSidebar(false);

  // Reinicializar ícones Lucide na nova secção
  if (window.lucide) lucide.createIcons();
}

// ─── SIDEBAR MOBILE ───────────────────────────────────────────
function toggleSidebar(forceClose) {
  const sidebar = document.getElementById("sidebar");
  const overlay = document.getElementById("overlay");

  if (forceClose === false || sidebar.classList.contains("open")) {
    sidebar.classList.remove("open");
    overlay.classList.remove("show");
  } else {
    sidebar.classList.add("open");
    overlay.classList.add("show");
  }
}

// ═══════════════════════════════════════════════════════════════
// 9. SINCRONIZAÇÃO DA INTERFACE
// ═══════════════════════════════════════════════════════════════

/** Atualiza todos os elementos visuais com base no estado. */
function syncUI() {
  // ── Stat cards ──
  const ledStatus   = document.getElementById("statLedStatus");
  const ledDot      = document.getElementById("statLedDot");
  const bulb1Status = document.getElementById("statBulb1Status");
  const bulb1Dot    = document.getElementById("statBulb1Dot");
  const bulb2Status = document.getElementById("statBulb2Status");
  const bulb2Dot    = document.getElementById("statBulb2Dot");

  if (ledStatus)   ledStatus.textContent   = state.led.on ? "ON" : "OFF";
  if (ledDot)      ledDot.classList.toggle("on", state.led.on);
  if (bulb1Status) bulb1Status.textContent = state.bulbs[0].on ? "ON" : "OFF";
  if (bulb1Dot)    bulb1Dot.classList.toggle("on", state.bulbs[0].on);
  if (bulb2Status) bulb2Status.textContent = state.bulbs[1].on ? "ON" : "OFF";
  if (bulb2Dot)    bulb2Dot.classList.toggle("on", state.bulbs[1].on);

  // ── Cor preview bars ──
  updateColorBars();

  // ── Ícones das lâmpadas ──
  document.getElementById("bulbIcon1")?.classList.toggle("on", state.bulbs[0].on);
  document.getElementById("bulbIcon2")?.classList.toggle("on", state.bulbs[1].on);

  // ── Quick preview (dashboard) ──
  const qPreview = document.getElementById("qLedPreview");
  if (qPreview) qPreview.style.background = state.led.color;

  // ── IPs na secção Definições ──
  const dispBulb1 = document.getElementById("displayBulb1Ip");
  const dispBulb2 = document.getElementById("displayBulb2Ip");
  if (dispBulb1) dispBulb1.textContent = SHELLY_IPS[0] || "—";
  if (dispBulb2) dispBulb2.textContent = SHELLY_IPS[1] || "—";

  const bulb1ipEl = document.getElementById("bulb1ip");
  const bulb2ipEl = document.getElementById("bulb2ip");
  if (bulb1ipEl) bulb1ipEl.textContent = SHELLY_IPS[0] || "—";
  if (bulb2ipEl) bulb2ipEl.textContent = SHELLY_IPS[1] || "—";
}

/** Atualiza as barras de cor dos controles. */
function updateColorBars() {
  const bar = document.getElementById("ledColorBar");
  if (bar) bar.style.background = state.led.color;

  const b1bar = document.getElementById("bulb1ColorBar");
  if (b1bar) b1bar.style.background = state.bulbs[0].color;

  const b2bar = document.getElementById("bulb2ColorBar");
  if (b2bar) b2bar.style.background = state.bulbs[1].color;
}

// ═══════════════════════════════════════════════════════════════
// 10. HELPERS DE INTERFACE
// ═══════════════════════════════════════════════════════════════

/**
 * Define a cor de um dispositivo a partir de um swatch.
 * @param {string} target — "led" | "bulb1" | "bulb2"
 * @param {string} hex    — cor em formato #RRGGBB
 */
function setColor(target, hex) {
  if (target === "led") {
    document.getElementById("ledColor").value = hex;
    state.led.color = hex;
    applyLEDColor();

  } else if (target === "bulb1") {
    document.getElementById("bulb1Color").value = hex;
    state.bulbs[0].color = hex;
    applyBulbColor(1);

  } else if (target === "bulb2") {
    document.getElementById("bulb2Color").value = hex;
    state.bulbs[1].color = hex;
    applyBulbColor(2);
  }
}

/** Atualiza a label do brilho ao arrastar o slider. */
function updateBrightLabel(value, elementId) {
  const el = document.getElementById(elementId);
  if (el) el.textContent = value;
}

// ═══════════════════════════════════════════════════════════════
// 11. DEFINIÇÕES — EDIÇÃO DE IPs EM RUNTIME
// ═══════════════════════════════════════════════════════════════

function editEspIp() {
  const input = document.getElementById("espIpInput");
  input.value = ESP32_IP;
  input.classList.remove("hidden");
  input.focus();
}

function saveEspIp(value) {
  if (value && value.startsWith("http")) {
    // Nota: esta alteração é só em runtime (sessão atual).
    // Para persistir, alterar a const ESP32_IP no topo do ficheiro.
    Object.defineProperty(window, "_ESP32_IP_OVERRIDE", { value, writable: true });
    document.getElementById("displayEspIp").textContent = value;
    document.getElementById("espIpInput").classList.add("hidden");
    showToast("IP do ESP32 atualizado (sessão atual)", "info");
  }
}

function editBulbIp(index) {
  const input = document.getElementById(`bulb${index}IpInput`);
  input.value = SHELLY_IPS[index - 1] || "";
  input.classList.remove("hidden");
  input.focus();
}

function saveBulbIp(index, value) {
  if (value) {
    SHELLY_IPS[index - 1] = value;
    document.getElementById(`displayBulb${index}Ip`).textContent = value;
    document.getElementById(`bulb${index}IpInput`).classList.add("hidden");
    document.getElementById(`bulb${index}ip`).textContent = value;
    showToast(`IP Lâmpada ${index} atualizado`, "info");
  }
}

// ═══════════════════════════════════════════════════════════════
// 12. ESTADO DE LIGAÇÃO
// ═══════════════════════════════════════════════════════════════

function setConnectionStatus(ok) {
  state.esp32Ok = ok;

  const dot  = document.getElementById("statusDot");
  const text = document.getElementById("statusText");

  if (!dot || !text) return;

  dot.className = "status-dot" + (ok ? " connected" : "");
  text.textContent = ok ? `Ligado — ${ESP32_IP}` : "ESP32 inacessível";
}

// ═══════════════════════════════════════════════════════════════
// 13. TOAST DE NOTIFICAÇÕES
// ═══════════════════════════════════════════════════════════════

let toastTimer = null;

/**
 * Mostra uma notificação temporária.
 * @param {string} msg   — mensagem
 * @param {string} type  — "success" | "error" | "info"
 * @param {number} ms    — duração em ms (padrão: 3000)
 */
function showToast(msg, type = "info", ms = 3000) {
  const toast = document.getElementById("toast");
  if (!toast) return;

  toast.textContent = msg;
  toast.className   = `toast ${type} show`;

  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    toast.classList.remove("show");
  }, ms);
}

// ═══════════════════════════════════════════════════════════════
// 14. UTILITÁRIOS
// ═══════════════════════════════════════════════════════════════

/**
 * Converte hex (#RRGGBB) para objeto { r, g, b }.
 */
function hexToRGB(hex) {
  const clean = hex.replace("#", "");
  return {
    r: parseInt(clean.substring(0, 2), 16),
    g: parseInt(clean.substring(2, 4), 16),
    b: parseInt(clean.substring(4, 6), 16),
  };
}

/** Capitaliza primeira letra. */
function capitalize(str) {
  return str.charAt(0).toUpperCase() + str.slice(1);
}
