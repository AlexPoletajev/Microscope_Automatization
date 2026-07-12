(() => {
  "use strict";

  const JOG_CANCEL = 0x85;
  const SOFT_RESET = 0x18;
  const DEADZONE = 0.12;
  const JOG_INTERVAL_MS = 110;
  const JOG_HORIZON_MS = 190;
  const RECONNECT_DELAY_MS = 1200;

  const elements = {
    pad: document.querySelector("#xy-pad"),
    handle: document.querySelector("#pad-handle"),
    vector: document.querySelector("#pad-vector"),
    direction: document.querySelector("#vector-direction"),
    vectorSpeed: document.querySelector("#vector-speed"),
    speed: document.querySelector("#speed-range"),
    speedOutput: document.querySelector("#speed-output"),
    connection: document.querySelector("#connection"),
    connectionLabel: document.querySelector("#connection-label"),
    machineState: document.querySelector("#machine-state"),
    positionX: document.querySelector("#position-x"),
    positionY: document.querySelector("#position-y"),
    zero: document.querySelector("#zero-position"),
    stop: document.querySelector("#jog-stop"),
    reset: document.querySelector("#reset-controller"),
    toast: document.querySelector("#toast")
  };

  let socket = null;
  let reconnectTimer = null;
  let jogTimer = null;
  let statusTimer = null;
  let toastTimer = null;
  let activePointer = null;
  let machineState = "Unknown";
  let vector = { x: 0, y: 0, magnitude: 0 };

  function setConnection(state, label) {
    elements.connection.dataset.state = state;
    elements.connectionLabel.textContent = label;
    elements.zero.disabled = state !== "online";
  }

  function showToast(message) {
    elements.toast.textContent = message;
    elements.toast.classList.add("visible");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => elements.toast.classList.remove("visible"), 2600);
  }

  function websocketUrl() {
    const protocol = location.protocol === "https:" ? "wss:" : "ws:";
    return `${protocol}//${location.host}/`;
  }

  function connect() {
    clearTimeout(reconnectTimer);
    if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;

    setConnection("connecting", "Verbinde...");
    socket = new WebSocket(websocketUrl());
    socket.binaryType = "arraybuffer";

    socket.addEventListener("open", () => {
      setConnection("online", "Verbunden");
      sendRealtime("?".charCodeAt(0));
      clearInterval(statusTimer);
      statusTimer = setInterval(() => sendRealtime("?".charCodeAt(0)), 500);
    });

    socket.addEventListener("message", event => {
      const read = data => parseResponse(data);
      if (event.data instanceof Blob) event.data.text().then(read);
      else if (event.data instanceof ArrayBuffer) read(new TextDecoder().decode(event.data));
      else read(String(event.data));
    });

    socket.addEventListener("close", () => {
      stopJog(false);
      clearInterval(statusTimer);
      setConnection("offline", "Offline");
      elements.machineState.textContent = "Verbindung unterbrochen";
      reconnectTimer = setTimeout(connect, RECONNECT_DELAY_MS);
    });

    socket.addEventListener("error", () => socket.close());
  }

  function parseResponse(payload) {
    for (const line of payload.split(/[\r\n]+/)) {
      if (!line) continue;
      if (line === "PING") {
        sendText("PING:60000:60000");
        continue;
      }
      if (line.startsWith("<") && line.endsWith(">")) parseStatus(line.slice(1, -1));
      if (line.startsWith("ALARM") || line.startsWith("error:")) showToast(line);
    }
  }

  function parseStatus(status) {
    const fields = status.split("|");
    machineState = fields[0];
    elements.machineState.textContent = translateState(machineState);
    const positionField = fields.find(field => field.startsWith("MPos:") || field.startsWith("WPos:"));
    if (!positionField) return;
    const values = positionField.slice(5).split(",");
    elements.positionX.textContent = formatPosition(values[0]);
    elements.positionY.textContent = formatPosition(values[1]);
  }

  function translateState(state) {
    const labels = { Idle: "Bereit", Jog: "Manuelle Bewegung", Run: "Programm läuft", Alarm: "Alarm", Hold: "Angehalten", Home: "Referenzfahrt" };
    return labels[state] || state;
  }

  function formatPosition(value) {
    const number = Number.parseFloat(value);
    return Number.isFinite(number) ? number.toFixed(3) : "--.---";
  }

  function sendText(command) {
    if (!socket || socket.readyState !== WebSocket.OPEN) return false;
    socket.send(command.endsWith("\n") ? command : `${command}\n`);
    return true;
  }

  function sendRealtime(byte) {
    if (!socket || socket.readyState !== WebSocket.OPEN) return false;
    socket.send(Uint8Array.of(byte));
    return true;
  }

  function updatePad(event) {
    const rect = elements.pad.getBoundingClientRect();
    const halfWidth = rect.width / 2;
    const halfHeight = rect.height / 2;
    let x = (event.clientX - rect.left - halfWidth) / halfWidth;
    let y = -(event.clientY - rect.top - halfHeight) / halfHeight;
    const rawMagnitude = Math.hypot(x, y);
    if (rawMagnitude > 1) {
      x /= rawMagnitude;
      y /= rawMagnitude;
    }
    const clampedMagnitude = Math.min(1, rawMagnitude);
    const scaledMagnitude = clampedMagnitude <= DEADZONE ? 0 : (clampedMagnitude - DEADZONE) / (1 - DEADZONE);
    vector = scaledMagnitude === 0 ? { x: 0, y: 0, magnitude: 0 } : { x, y, magnitude: scaledMagnitude };
    renderVector(x, y, scaledMagnitude);
  }

  function renderVector(x, y, magnitude) {
    const radius = elements.pad.clientWidth / 2;
    const px = x * radius * Math.min(1, Math.hypot(x, y));
    const py = -y * radius * Math.min(1, Math.hypot(x, y));
    elements.handle.style.transform = `translate(${px}px, ${py}px)`;
    elements.vector.style.width = `${Math.hypot(px, py)}px`;
    elements.vector.style.transform = `rotate(${Math.atan2(py, px)}rad)`;
    elements.vector.style.opacity = magnitude > 0 ? "1" : "0";

    const speed = Math.round(Number(elements.speed.value) * magnitude);
    elements.vectorSpeed.textContent = `${speed} mm/min`;
    elements.direction.textContent = directionLabel(x, y, magnitude);
  }

  function directionLabel(x, y, magnitude) {
    if (magnitude === 0) return "STOPP";
    const horizontal = Math.abs(x) > 0.28 ? (x > 0 ? "X+" : "X-") : "";
    const vertical = Math.abs(y) > 0.28 ? (y > 0 ? "Y+" : "Y-") : "";
    return [horizontal, vertical].filter(Boolean).join("  ") || "FEIN";
  }

  function sendJogSegment() {
    if (activePointer === null || vector.magnitude === 0) return;
    if (machineState !== "Idle" && machineState !== "Jog" && machineState !== "Unknown") {
      stopJog();
      showToast(`Bewegung in Zustand ${machineState} gesperrt`);
      return;
    }

    const feed = Number(elements.speed.value) * vector.magnitude;
    const distance = feed * JOG_HORIZON_MS / 60000;
    const xDistance = distance * vector.x;
    const yDistance = distance * vector.y;
    sendText(`$J=G91 G21 X${xDistance.toFixed(4)} Y${yDistance.toFixed(4)} F${feed.toFixed(1)}`);
  }

  function startJog(event) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      showToast("Keine Verbindung zum Controller");
      return;
    }
    if (activePointer !== null) return;
    activePointer = event.pointerId;
    elements.pad.setPointerCapture(event.pointerId);
    elements.pad.classList.add("active");
    updatePad(event);
    sendJogSegment();
    clearInterval(jogTimer);
    jogTimer = setInterval(sendJogSegment, JOG_INTERVAL_MS);
  }

  function moveJog(event) {
    if (event.pointerId !== activePointer) return;
    updatePad(event);
  }

  function stopJog(sendCancel = true) {
    clearInterval(jogTimer);
    jogTimer = null;
    if (activePointer !== null && elements.pad.hasPointerCapture(activePointer)) elements.pad.releasePointerCapture(activePointer);
    activePointer = null;
    vector = { x: 0, y: 0, magnitude: 0 };
    elements.pad.classList.remove("active");
    elements.handle.style.transform = "translate(0, 0)";
    elements.vector.style.width = "0";
    elements.vector.style.opacity = "0";
    elements.vectorSpeed.textContent = "0 mm/min";
    elements.direction.textContent = "STOPP";
    if (sendCancel) sendRealtime(JOG_CANCEL);
  }

  elements.pad.addEventListener("pointerdown", startJog);
  elements.pad.addEventListener("pointermove", moveJog);
  elements.pad.addEventListener("pointerup", () => stopJog());
  elements.pad.addEventListener("pointercancel", () => stopJog());
  elements.pad.addEventListener("lostpointercapture", () => stopJog());
  elements.speed.addEventListener("input", () => {
    elements.speedOutput.textContent = `${elements.speed.value} mm/min`;
  });
  elements.stop.addEventListener("click", () => stopJog());
  elements.reset.addEventListener("click", () => {
    stopJog();
    sendRealtime(SOFT_RESET);
    showToast("Controller wurde zurückgesetzt");
  });
  elements.zero.addEventListener("click", () => {
    stopJog();
    if (sendText("G10 L20 P0 X0 Y0")) showToast("Arbeitsposition X/Y genullt");
  });

  window.addEventListener("blur", () => stopJog());
  window.addEventListener("offline", () => stopJog());
  window.addEventListener("pagehide", () => stopJog());
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) stopJog();
  });

  if ("serviceWorker" in navigator) navigator.serviceWorker.register("/service-worker.js").catch(() => {});
  setConnection("offline", "Offline");
  connect();
})();
