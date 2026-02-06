// ⚠️ TROCAR PELO IP DO TEU ESP32
const ESP32_IP = "http://192.168.1.45";

function ledOn() {
  fetch(`${ESP32_IP}/led/on`);
}

function ledOff() {
  fetch(`${ESP32_IP}/led/off`);
}

function setColor(hex) {
  const r = parseInt(hex.substring(1,3), 16);
  const g = parseInt(hex.substring(3,5), 16);
  const b = parseInt(hex.substring(5,7), 16);

  fetch(`${ESP32_IP}/led/color?r=${r}&g=${g}&b=${b}`);
}

function shellyOn() {
  fetch(`${ESP32_IP}/shelly/on`);
}

function shellyOff() {
  fetch(`${ESP32_IP}/shelly/off`);
}
