/* ============================================================
   BLDC FAN REMOTE — app logic
   Dual-purpose: tries the fan's local IP first (fast), falls back
   to HiveMQ Cloud over MQTT/WebSockets automatically, and keeps
   watching so it can hop back to local once you're home again.
============================================================ */

const DEFAULTS = {
  localIp: "192.168.10.141",
  host: "0f361fa0238547348bd1826499d0e794.s1.eu.hivemq.cloud",
  user: "",
  pass: "",
  cmdTopic: "fan/command",
  stateTopic: "fan/state"
};

function loadConfig() {
  const stored = localStorage.getItem("fanRemoteConfig");
  return stored ? JSON.parse(stored) : { ...DEFAULTS };
}
function saveConfig(cfg) {
  localStorage.setItem("fanRemoteConfig", JSON.stringify(cfg));
}

let cfg = loadConfig();

let mode = "off"; // "local" | "cloud" | "off"
let mqttClient = null;
let mqttConnecting = false;
let localCheckTimer = null;
let statePollTimer = null;

const el = {
  statusPill: document.getElementById("statusPill"),
  statusDot: document.getElementById("statusDot"),
  statusText: document.getElementById("statusText"),
  latency: document.getElementById("latency"),
  gauge: document.getElementById("gauge"),
  ring: document.getElementById("gaugeRingFg"),
  readoutValue: document.getElementById("readoutValue"),
  installBanner: document.getElementById("installBanner"),
  installBtn: document.getElementById("installBtn"),
  settingsBtn: document.getElementById("settingsBtn"),
  modalBackdrop: document.getElementById("modalBackdrop")
};

const RING_CIRCUMFERENCE = 2 * Math.PI * 74; // matches r=74 in svg

/* ---------------- status + gauge ---------------- */

function setStatus(newMode, label) {
  mode = newMode;
  el.statusPill.dataset.mode = newMode;
  el.statusText.textContent = label;
}

function speedFraction(state) {
  const map = { SPEED1: 1, SPEED2: 2, SPEED3: 3, SPEED4: 4, SPEED5: 5, SPEED6: 6, FULL: 6.6 };
  return (map[state] || 0) / 6.6;
}

function setGauge(state) {
  const clean = (state || "").trim().toUpperCase();
  el.readoutValue.textContent = clean || "—";

  const isOn = clean && clean !== "OFF";
  el.gauge.classList.toggle("off", !isOn);
  el.gauge.classList.toggle("spinning", isOn);

  const frac = speedFraction(clean);
  const offset = RING_CIRCUMFERENCE * (1 - (isOn ? Math.max(frac, 0.12) : 0));
  el.ring.style.strokeDasharray = `${RING_CIRCUMFERENCE}`;
  el.ring.style.strokeDashoffset = `${offset}`;
  el.ring.style.stroke = clean === "OFF" || !clean ? "var(--border-strong)" : "var(--amber)";

  // Faster spin at higher speed; FULL spins fastest.
  const speedNum = { SPEED1: 1, SPEED2: 2, SPEED3: 3, SPEED4: 4, SPEED5: 5, SPEED6: 6, FULL: 8 }[clean] || 0;
  const duration = speedNum ? Math.max(0.9, 3.6 - speedNum * 0.35) : 6;
  el.gauge.style.setProperty("--spin-duration", `${duration}s`);

  document.querySelectorAll(".key.speed").forEach((btn) => {
    btn.dataset.active = btn.dataset.cmd === clean ? "true" : "false";
  });
}

/* ---------------- local HTTP mode ---------------- */

function localUrl(path) {
  return `http://${cfg.localIp}${path}`;
}

async function tryLocal() {
  if (!cfg.localIp) return false;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 1200);
  try {
    const start = performance.now();
    const res = await fetch(localUrl("/state"), { signal: controller.signal, cache: "no-store" });
    clearTimeout(timeout);
    if (!res.ok) return false;
    const text = await res.text();
    const ms = Math.round(performance.now() - start);
    el.latency.textContent = `${ms}ms · same network`;
    setStatus("local", "Local");
    setGauge(text);
    return true;
  } catch (e) {
    clearTimeout(timeout);
    return false;
  }
}

function startLocalPolling() {
  stopStatePolling();
  statePollTimer = setInterval(async () => {
    const ok = await tryLocal();
    if (!ok) {
      stopStatePolling();
      connectCloud();
    }
  }, 4000);
}

function stopStatePolling() {
  if (statePollTimer) clearInterval(statePollTimer);
  statePollTimer = null;
}

async function sendLocal(command) {
  const res = await fetch(localUrl(`/command?cmd=${encodeURIComponent(command)}`), { cache: "no-store" });
  const text = await res.text();
  setGauge(text);
}

/* ---------------- cloud MQTT mode ---------------- */

function connectCloud() {
  if (mqttConnecting) return;
  if (!cfg.host) {
    setStatus("off", "Not configured");
    return;
  }

  mqttConnecting = true;
  setStatus("off", "Connecting…");
  el.latency.textContent = "";

  const url = `wss://${cfg.host}:8884/mqtt`;
  const client = mqtt.connect(url, {
    username: cfg.user,
    password: cfg.pass,
    clientId: `webremote_${Math.random().toString(16).slice(2)}`,
    reconnectPeriod: 4000,
    connectTimeout: 6000
  });

  client.on("connect", () => {
    mqttConnecting = false;
    setStatus("cloud", "Cloud");
    el.latency.textContent = "via HiveMQ";
    client.subscribe(cfg.stateTopic);
    startLocalWatcher();
  });

  client.on("message", (topic, payload) => {
    if (topic === cfg.stateTopic) setGauge(payload.toString());
  });

  client.on("error", () => {
    mqttConnecting = false;
  });

  client.on("close", () => {
    if (mode === "cloud") setStatus("off", "Reconnecting…");
  });

  mqttClient = client;
}

function sendCloud(command) {
  if (mqttClient && mqttClient.connected) {
    mqttClient.publish(cfg.cmdTopic, command);
  }
}

function startLocalWatcher() {
  if (localCheckTimer) return;
  localCheckTimer = setInterval(async () => {
    if (mode === "local") return;
    const ok = await tryLocal();
    if (ok) {
      if (mqttClient) mqttClient.end(true);
      mqttClient = null;
      clearInterval(localCheckTimer);
      localCheckTimer = null;
      startLocalPolling();
    }
  }, 8000);
}

/* ---------------- send ---------------- */

async function send(command) {
  if (mode === "local") {
    try {
      await sendLocal(command);
    } catch (e) {
      sendCloud(command);
    }
  } else {
    sendCloud(command);
  }
}

document.querySelectorAll("[data-cmd]").forEach((btn) => {
  btn.addEventListener("click", () => send(btn.dataset.cmd));
});

/* ---------------- init ---------------- */

async function init() {
  const ok = await tryLocal();
  if (ok) {
    startLocalPolling();
  } else {
    connectCloud();
  }
}

/* ---------------- settings modal ---------------- */

function openSettings() {
  document.getElementById("cfgLocalIp").value = cfg.localIp || "";
  document.getElementById("cfgHost").value = cfg.host || "";
  document.getElementById("cfgUser").value = cfg.user || "";
  document.getElementById("cfgPass").value = cfg.pass || "";
  document.getElementById("cfgCmdTopic").value = cfg.cmdTopic || "";
  document.getElementById("cfgStateTopic").value = cfg.stateTopic || "";
  el.modalBackdrop.classList.add("show");
}

function closeSettings() {
  el.modalBackdrop.classList.remove("show");
}

function saveSettings() {
  cfg = {
    localIp: document.getElementById("cfgLocalIp").value.trim(),
    host: document.getElementById("cfgHost").value.trim(),
    user: document.getElementById("cfgUser").value,
    pass: document.getElementById("cfgPass").value,
    cmdTopic: document.getElementById("cfgCmdTopic").value.trim() || "fan/command",
    stateTopic: document.getElementById("cfgStateTopic").value.trim() || "fan/state"
  };
  saveConfig(cfg);
  closeSettings();

  stopStatePolling();
  if (mqttClient) {
    mqttClient.end(true);
    mqttClient = null;
  }
  if (localCheckTimer) {
    clearInterval(localCheckTimer);
    localCheckTimer = null;
  }
  init();
}

el.settingsBtn.addEventListener("click", openSettings);
document.getElementById("btnCancel").addEventListener("click", closeSettings);
document.getElementById("btnSave").addEventListener("click", saveSettings);

if (!localStorage.getItem("fanRemoteConfig")) {
  saveConfig(cfg);
}

/* ---------------- PWA: install to home screen ---------------- */

let deferredInstallPrompt = null;

window.addEventListener("beforeinstallprompt", (event) => {
  event.preventDefault();
  deferredInstallPrompt = event;
  el.installBanner.classList.add("show");
});

el.installBtn.addEventListener("click", async () => {
  if (!deferredInstallPrompt) return;
  deferredInstallPrompt.prompt();
  await deferredInstallPrompt.userChoice;
  deferredInstallPrompt = null;
  el.installBanner.classList.remove("show");
});

window.addEventListener("appinstalled", () => {
  el.installBanner.classList.remove("show");
});

if ("serviceWorker" in navigator) {
  window.addEventListener("load", () => {
    navigator.serviceWorker.register("./sw.js").catch(() => {});
  });
}

/* ---------------- go ---------------- */

init();
