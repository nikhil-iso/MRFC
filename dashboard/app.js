const BAUD_RATE = 115200;
const MAX_SAMPLES = 180;
const TELEMETRY_COLUMNS = [
  "time_ms",
  "ax_g",
  "ay_g",
  "az_g",
  "gx_deg_s",
  "gy_deg_s",
  "gz_deg_s",
  "temp_C",
  "pressure_Pa",
  "pressure_baseline_Pa",
  "altitude_rel_m",
  "altitude_lpf_m",
  "a_total_g",
  "a_total_lpf_g",
];
const REQUIRED_COLUMNS = [
  "time_ms",
  "ax_g",
  "ay_g",
  "az_g",
  "gx_deg_s",
  "gy_deg_s",
  "gz_deg_s",
];
const AXIS_COLORS = {
  x: "#ff7b72",
  y: "#4dd39a",
  z: "#59b8ff",
};
const DEFAULT_HEADER_MAP = TELEMETRY_COLUMNS.reduce((map, column, index) => {
  map[column] = index;
  return map;
}, {});

class RingBuffer {
  constructor(capacity) {
    this.capacity = capacity;
    this.items = new Array(capacity);
    this.length = 0;
    this.start = 0;
  }

  clear() {
    this.items = new Array(this.capacity);
    this.length = 0;
    this.start = 0;
  }

  push(value) {
    if (this.length < this.capacity) {
      this.items[(this.start + this.length) % this.capacity] = value;
      this.length += 1;
      return;
    }

    this.items[this.start] = value;
    this.start = (this.start + 1) % this.capacity;
  }

  toArray() {
    const result = [];
    for (let index = 0; index < this.length; index += 1) {
      result.push(this.items[(this.start + index) % this.capacity]);
    }
    return result;
  }
}

class ComplementaryOrientation {
  constructor(alpha = 0.96) {
    this.alpha = alpha;
    this.reset();
  }

  reset() {
    this.rollRad = 0;
    this.pitchRad = 0;
    this.yawRad = 0;
    this.lastTimeMs = null;
    this.initialized = false;
  }

  update(sample) {
    const accelRollRad = Math.atan2(sample.ay_g, sample.az_g || 1e-6);
    const accelPitchRad = Math.atan2(
      -sample.ax_g,
      Math.max(Math.hypot(sample.ay_g, sample.az_g), 1e-6),
    );

    if (!this.initialized) {
      this.rollRad = accelRollRad;
      this.pitchRad = accelPitchRad;
      this.yawRad = 0;
      this.lastTimeMs = sample.time_ms;
      this.initialized = true;

      return this.toDegrees();
    }

    let dtSeconds = (sample.time_ms - this.lastTimeMs) / 1000;
    if (!Number.isFinite(dtSeconds) || dtSeconds <= 0 || dtSeconds > 0.25) {
      dtSeconds = 0.05;
    }

    this.lastTimeMs = sample.time_ms;

    this.rollRad =
      this.alpha * (this.rollRad + degreesToRadians(sample.gx_deg_s) * dtSeconds) +
      (1 - this.alpha) * accelRollRad;
    this.pitchRad =
      this.alpha *
        (this.pitchRad + degreesToRadians(sample.gy_deg_s) * dtSeconds) +
      (1 - this.alpha) * accelPitchRad;
    this.yawRad += degreesToRadians(sample.gz_deg_s) * dtSeconds;

    return this.toDegrees();
  }

  toDegrees() {
    return {
      pitch: radiansToDegrees(this.pitchRad),
      roll: radiansToDegrees(this.rollRad),
      yaw: radiansToDegrees(this.yawRad),
    };
  }
}

const elements = {
  connectButton: document.querySelector("#connectButton"),
  disconnectButton: document.querySelector("#disconnectButton"),
  resetOrientationButton: document.querySelector("#resetOrientationButton"),
  connectionStatus: document.querySelector("#connectionStatus"),
  streamStatus: document.querySelector("#streamStatus"),
  sampleCount: document.querySelector("#sampleCount"),
  lastPacketAge: document.querySelector("#lastPacketAge"),
  supportMessage: document.querySelector("#supportMessage"),
  accelChart: document.querySelector("#accelChart"),
  gyroChart: document.querySelector("#gyroChart"),
  rocketModel: document.querySelector("#rocketModel"),
  axValue: document.querySelector("#axValue"),
  ayValue: document.querySelector("#ayValue"),
  azValue: document.querySelector("#azValue"),
  gxValue: document.querySelector("#gxValue"),
  gyValue: document.querySelector("#gyValue"),
  gzValue: document.querySelector("#gzValue"),
  pitchValue: document.querySelector("#pitchValue"),
  rollValue: document.querySelector("#rollValue"),
  yawValue: document.querySelector("#yawValue"),
};

const state = {
  port: null,
  reader: null,
  readLoopPromise: null,
  keepReading: false,
  inputBuffer: "",
  headerMap: null,
  samples: new RingBuffer(MAX_SAMPLES),
  sampleCount: 0,
  lastPacketPerfMs: null,
  latestSample: null,
  orientationEstimator: new ComplementaryOrientation(),
  absoluteOrientationDeg: { pitch: 0, roll: 0, yaw: 0 },
  zeroOrientationDeg: { pitch: 0, roll: 0, yaw: 0 },
  displayOrientationDeg: { pitch: 0, roll: 0, yaw: 0 },
  disconnectInProgress: false,
  pendingRender: false,
};

function degreesToRadians(value) {
  return (value * Math.PI) / 180;
}

function radiansToDegrees(value) {
  return (value * 180) / Math.PI;
}

function formatSigned(value, digits) {
  if (!Number.isFinite(value)) {
    return "--";
  }

  const normalized = Object.is(value, -0) ? 0 : value;
  return normalized.toFixed(digits);
}

function setConnectionStatus(message) {
  elements.connectionStatus.textContent = message;
}

function setStreamStatus(message) {
  elements.streamStatus.textContent = message;
}

function setSupportMessage(message) {
  elements.supportMessage.textContent = message;
}

function resetTelemetryState() {
  state.inputBuffer = "";
  state.headerMap = null;
  state.samples.clear();
  state.sampleCount = 0;
  state.lastPacketPerfMs = null;
  state.latestSample = null;
  state.orientationEstimator.reset();
  state.absoluteOrientationDeg = { pitch: 0, roll: 0, yaw: 0 };
  state.zeroOrientationDeg = { pitch: 0, roll: 0, yaw: 0 };
  state.displayOrientationDeg = { pitch: 0, roll: 0, yaw: 0 };
  elements.sampleCount.textContent = "0";
  render();
}

function updateControls() {
  const connected = state.port !== null;
  elements.connectButton.disabled = !("serial" in navigator) || connected;
  elements.disconnectButton.disabled = !connected;
  elements.resetOrientationButton.disabled = !connected || state.sampleCount === 0;
}

function detectHeader(line) {
  const columns = line.split(",").map((entry) => entry.trim());
  const hasRequiredColumns = REQUIRED_COLUMNS.every((column) =>
    columns.includes(column),
  );

  if (!hasRequiredColumns) {
    return false;
  }

  state.headerMap = columns.reduce((map, column, index) => {
    map[column] = index;
    return map;
  }, {});
  setStreamStatus("Streaming");
  setSupportMessage(
    "CSV header detected. Streaming live IMU data from the Teensy.",
  );

  return true;
}

function looksLikeTelemetryRow(line) {
  const values = line.split(",");
  if (values.length < TELEMETRY_COLUMNS.length) {
    return false;
  }

  for (const column of REQUIRED_COLUMNS) {
    const parsedValue = Number.parseFloat(values[DEFAULT_HEADER_MAP[column]]);
    if (!Number.isFinite(parsedValue)) {
      return false;
    }
  }

  return true;
}

function enableDefaultHeaderMap() {
  state.headerMap = DEFAULT_HEADER_MAP;
  setStreamStatus("Streaming");
  setSupportMessage(
    "Streaming resumed without a CSV header. Using the current firmware's default telemetry layout.",
  );
}

function parseSample(line) {
  if (!state.headerMap) {
    return null;
  }

  const values = line.split(",");
  const sample = {};

  for (const column of REQUIRED_COLUMNS) {
    const columnIndex = state.headerMap[column];
    if (columnIndex === undefined || columnIndex >= values.length) {
      return null;
    }

    const parsedValue = Number.parseFloat(values[columnIndex]);
    if (!Number.isFinite(parsedValue)) {
      return null;
    }

    sample[column] = parsedValue;
  }

  return sample;
}

function applySample(sample) {
  state.latestSample = sample;
  state.samples.push(sample);
  state.sampleCount += 1;
  state.lastPacketPerfMs = performance.now();
  elements.sampleCount.textContent = String(state.sampleCount);

  state.absoluteOrientationDeg = state.orientationEstimator.update(sample);
  if (state.sampleCount === 1) {
    state.zeroOrientationDeg = { ...state.absoluteOrientationDeg };
  }

  state.displayOrientationDeg = {
    pitch: state.absoluteOrientationDeg.pitch - state.zeroOrientationDeg.pitch,
    roll: state.absoluteOrientationDeg.roll - state.zeroOrientationDeg.roll,
    yaw: state.absoluteOrientationDeg.yaw - state.zeroOrientationDeg.yaw,
  };

  updateControls();
  scheduleRender();
}

function processLine(rawLine) {
  const line = rawLine.trim();
  if (!line) {
    return;
  }

  if (!state.headerMap) {
    if (detectHeader(line)) {
      return;
    }

    if (!looksLikeTelemetryRow(line)) {
      return;
    }

    enableDefaultHeaderMap();
  }

  const sample = parseSample(line);
  if (!sample) {
    return;
  }

  applySample(sample);
}

function processInputBuffer() {
  let newlineIndex = state.inputBuffer.indexOf("\n");

  while (newlineIndex !== -1) {
    const line = state.inputBuffer.slice(0, newlineIndex);
    state.inputBuffer = state.inputBuffer.slice(newlineIndex + 1);
    processLine(line);
    newlineIndex = state.inputBuffer.indexOf("\n");
  }
}

function scheduleRender() {
  if (state.pendingRender) {
    return;
  }

  state.pendingRender = true;
  window.requestAnimationFrame(() => {
    state.pendingRender = false;
    render();
  });
}

function resetOrientation() {
  state.zeroOrientationDeg = { ...state.absoluteOrientationDeg };
  state.displayOrientationDeg = { pitch: 0, roll: 0, yaw: 0 };
  scheduleRender();
}

function resizeCanvasToDisplaySize(canvas) {
  const devicePixelRatio = window.devicePixelRatio || 1;
  const width = Math.max(1, Math.round(canvas.clientWidth * devicePixelRatio));
  const height = Math.max(1, Math.round(canvas.clientHeight * devicePixelRatio));

  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }
}

function computeSeriesRange(samples, keys, minimumSpan) {
  let minimum = Infinity;
  let maximum = -Infinity;

  for (const sample of samples) {
    for (const key of keys) {
      const value = sample[key];
      if (!Number.isFinite(value)) {
        continue;
      }

      minimum = Math.min(minimum, value);
      maximum = Math.max(maximum, value);
    }
  }

  if (!Number.isFinite(minimum) || !Number.isFinite(maximum)) {
    return {
      min: -minimumSpan / 2,
      max: minimumSpan / 2,
    };
  }

  minimum = Math.min(minimum, 0);
  maximum = Math.max(maximum, 0);

  let span = maximum - minimum;
  if (span < minimumSpan) {
    const center = (maximum + minimum) / 2;
    span = minimumSpan;
    minimum = center - span / 2;
    maximum = center + span / 2;
  }

  const padding = span * 0.12;
  return {
    min: minimum - padding,
    max: maximum + padding,
  };
}

function drawChart(canvas, samples, series, minimumSpan, unitLabel) {
  resizeCanvasToDisplaySize(canvas);

  const context = canvas.getContext("2d");
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  const devicePixelRatio = window.devicePixelRatio || 1;
  const padding = { top: 18, right: 12, bottom: 22, left: 14 };
  const plotWidth = width - padding.left - padding.right;
  const plotHeight = height - padding.top - padding.bottom;

  context.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
  context.clearRect(0, 0, width, height);

  const range = computeSeriesRange(
    samples,
    series.map((entry) => entry.key),
    minimumSpan,
  );

  for (let lineIndex = 0; lineIndex <= 4; lineIndex += 1) {
    const y = padding.top + (plotHeight / 4) * lineIndex;
    context.strokeStyle =
      lineIndex === 2 ? "rgba(252, 163, 17, 0.24)" : "rgba(168, 178, 199, 0.12)";
    context.lineWidth = lineIndex === 2 ? 1.2 : 1;
    context.beginPath();
    context.moveTo(padding.left, y);
    context.lineTo(width - padding.right, y);
    context.stroke();
  }

  context.fillStyle = "rgba(168, 178, 199, 0.78)";
  context.font = '11px "Cascadia Code", "IBM Plex Mono", Consolas, monospace';
  context.fillText(
    `${range.max.toFixed(2)} ${unitLabel}`,
    padding.left,
    padding.top - 4,
  );
  context.fillText(
    `${range.min.toFixed(2)} ${unitLabel}`,
    padding.left,
    height - 6,
  );

  if (samples.length < 2) {
    context.fillStyle = "rgba(168, 178, 199, 0.62)";
    context.fillText("Waiting for telemetry...", padding.left, height / 2);
    return;
  }

  for (const entry of series) {
    context.strokeStyle = entry.color;
    context.lineWidth = 2.2;
    context.beginPath();

    samples.forEach((sample, index) => {
      const value = sample[entry.key];
      const normalizedY = (value - range.min) / (range.max - range.min);
      const x =
        padding.left + (index / (samples.length - 1)) * Math.max(plotWidth, 1);
      const y = padding.top + (1 - normalizedY) * plotHeight;

      if (index === 0) {
        context.moveTo(x, y);
      } else {
        context.lineTo(x, y);
      }
    });

    context.stroke();
  }
}

function renderNumericValues() {
  const sample = state.latestSample;
  if (!sample) {
    elements.axValue.textContent = "--";
    elements.ayValue.textContent = "--";
    elements.azValue.textContent = "--";
    elements.gxValue.textContent = "--";
    elements.gyValue.textContent = "--";
    elements.gzValue.textContent = "--";
  } else {
    elements.axValue.textContent = formatSigned(sample.ax_g, 3);
    elements.ayValue.textContent = formatSigned(sample.ay_g, 3);
    elements.azValue.textContent = formatSigned(sample.az_g, 3);
    elements.gxValue.textContent = formatSigned(sample.gx_deg_s, 1);
    elements.gyValue.textContent = formatSigned(sample.gy_deg_s, 1);
    elements.gzValue.textContent = formatSigned(sample.gz_deg_s, 1);
  }

  elements.pitchValue.textContent = formatSigned(
    state.displayOrientationDeg.pitch,
    1,
  );
  elements.rollValue.textContent = formatSigned(
    state.displayOrientationDeg.roll,
    1,
  );
  elements.yawValue.textContent = formatSigned(
    state.displayOrientationDeg.yaw,
    1,
  );
}

function renderOrientationModel() {
  const { pitch, roll, yaw } = state.displayOrientationDeg;
  elements.rocketModel.style.transform =
    `rotateZ(${yaw.toFixed(2)}deg) rotateX(${(-pitch).toFixed(2)}deg) rotateY(${roll.toFixed(2)}deg)`;
}

function render() {
  renderNumericValues();
  renderOrientationModel();

  const samples = state.samples.toArray();
  drawChart(
    elements.accelChart,
    samples,
    [
      { key: "ax_g", color: AXIS_COLORS.x },
      { key: "ay_g", color: AXIS_COLORS.y },
      { key: "az_g", color: AXIS_COLORS.z },
    ],
    2.2,
    "g",
  );
  drawChart(
    elements.gyroChart,
    samples,
    [
      { key: "gx_deg_s", color: AXIS_COLORS.x },
      { key: "gy_deg_s", color: AXIS_COLORS.y },
      { key: "gz_deg_s", color: AXIS_COLORS.z },
    ],
    120,
    "deg/s",
  );
}

function updateLastPacketAge() {
  if (!state.lastPacketPerfMs) {
    elements.lastPacketAge.textContent = "--";
    return;
  }

  const ageSeconds = (performance.now() - state.lastPacketPerfMs) / 1000;
  if (ageSeconds < 0.25) {
    elements.lastPacketAge.textContent = "Live";
    return;
  }

  elements.lastPacketAge.textContent = `${ageSeconds.toFixed(1)} s ago`;
}

async function closePortOnly() {
  if (!state.port) {
    return;
  }

  try {
    await state.port.close();
  } catch (error) {
    // Ignore close errors so disconnect can still finish cleanly.
  }

  state.port = null;
}

async function disconnectSerial(reason = "Disconnected") {
  if (state.disconnectInProgress) {
    return;
  }

  state.disconnectInProgress = true;
  state.keepReading = false;

  try {
    if (state.reader) {
      try {
        await state.reader.cancel();
      } catch (error) {
        // Ignore cancellation errors during shutdown.
      }
    }

    if (state.readLoopPromise) {
      try {
        await state.readLoopPromise;
      } catch (error) {
        // The stream loop already reported the error through the UI state.
      }
    }

    state.readLoopPromise = null;
    await closePortOnly();
    setConnectionStatus(reason);
    setStreamStatus("Idle");
    setSupportMessage(
      "Open this dashboard from localhost in Chrome or Edge for Web Serial access.",
    );
    updateControls();
  } finally {
    state.disconnectInProgress = false;
  }
}

async function readSerialLoop(port) {
  const reader = port.readable.getReader();
  const decoder = new TextDecoder();
  state.reader = reader;

  try {
    while (state.keepReading) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }

      if (!value) {
        continue;
      }

      state.inputBuffer += decoder.decode(value, { stream: true });
      processInputBuffer();
    }

    const trailingText = decoder.decode();
    if (trailingText) {
      state.inputBuffer += trailingText;
      processInputBuffer();
    }
  } catch (error) {
    if (state.keepReading) {
      setConnectionStatus("Read Error");
      setStreamStatus("Error");
      setSupportMessage(`Serial read failed: ${error.message}`);
    }
  } finally {
    try {
      reader.releaseLock();
    } catch (error) {
      // Ignore release errors during disconnect.
    }

    state.reader = null;
  }

  if (state.keepReading && state.port === port) {
    state.keepReading = false;
    await closePortOnly();
    setConnectionStatus("Connection Lost");
    setStreamStatus("Idle");
    setSupportMessage("The serial stream ended unexpectedly. Reconnect to resume.");
    updateControls();
  }
}

async function connectSerial() {
  if (!("serial" in navigator)) {
    setConnectionStatus("Unsupported");
    setStreamStatus("Unavailable");
    setSupportMessage(
      "This browser does not support Web Serial. Use Chrome or Edge on localhost.",
    );
    updateControls();
    return;
  }

  try {
    const port = await navigator.serial.requestPort();
    resetTelemetryState();
    setConnectionStatus("Connecting...");
    setStreamStatus("Opening");
    setSupportMessage("Opening the selected serial port...");

    await port.open({ baudRate: BAUD_RATE });
    state.port = port;
    state.keepReading = true;
    setConnectionStatus("Connected");
    setStreamStatus("Waiting for CSV Header");
    setSupportMessage(
      "Connected. Waiting for the Teensy to finish boot text and emit the CSV header.",
    );
    updateControls();

    state.readLoopPromise = readSerialLoop(port);
  } catch (error) {
    if (error.name === "NotFoundError") {
      setConnectionStatus("Disconnected");
      setStreamStatus("Idle");
      setSupportMessage("Serial port selection was canceled.");
      return;
    }

    setConnectionStatus("Connect Failed");
    setStreamStatus("Error");
    setSupportMessage(`Could not open the serial port: ${error.message}`);
    updateControls();
  }
}

elements.connectButton.addEventListener("click", connectSerial);
elements.disconnectButton.addEventListener("click", () => {
  disconnectSerial();
});
elements.resetOrientationButton.addEventListener("click", resetOrientation);

window.addEventListener("resize", scheduleRender);
window.setInterval(updateLastPacketAge, 250);

if ("serial" in navigator) {
  navigator.serial.addEventListener("disconnect", (event) => {
    if (state.port && event.target === state.port) {
      disconnectSerial("Device Disconnected");
    }
  });
} else {
  setConnectionStatus("Unsupported");
  setStreamStatus("Unavailable");
  setSupportMessage(
    "Web Serial is unavailable in this browser. Use Chrome or Edge and open the dashboard through localhost.",
  );
}

updateControls();
render();
