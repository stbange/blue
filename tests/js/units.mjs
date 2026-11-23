import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import { Window } from "happy-dom";
import { load, site } from "./modules.mjs";

test("pack archives preserve edited source, samples and macro state", async () => {
  const api = await load("packs.js");
  const manifest = {
    format: 1,
    luaApi: 1,
    id: "studio.pack",
    presets: { T: "presets/t.json" },
    assets: { a: "a.wav" },
    script: "voice.lua",
  };
  const blob = await api.exportPack(
    manifest,
    "T",
    "{}",
    "blue.control(1,'x')",
    [0.5],
    async () => new Uint8Array([82, 73]),
  );
  const data = api.unpack(new Uint8Array(await blob.arrayBuffer()));
  assert.equal(data.manifest.id, "studio.pack");
  assert.equal(data.assets["a.wav"][1], 73);
  assert.deepEqual(
    JSON.parse(new TextDecoder().decode(data.assets["presets/t.json"])).macros,
    [0.5],
  );
  for (const name of ["../a", "/abs", "x//y", "dir/./a", ""])
    assert.throws(() => api.relative(name));
  assert.throws(() => api.unpack(new Uint8Array(17 * 1024 * 1024)));
  await assert.rejects(
    api.exportPack({ ...manifest, id: "a/b" }, "T", "{}", "", [], () => {}),
  );
});
test("sound editor applies typed controls, inserted effects and envelope points", async () => {
  const win = new Window(),
    doc = win.document,
    container = doc.createElement("div");
  function Option(text, value) {
    const o = doc.createElement("option");
    o.text = text;
    o.value = value;
    return o;
  }
  const api = await load("editor.js", { document: doc, Option });
  let result;
  api.mountEditor(
    container,
    JSON.stringify({ lanes: [{ source: "table" }] }),
    (s) => (result = JSON.parse(s)),
  );
  for (const select of container.querySelectorAll("select")) {
    select.selectedIndex = 1;
    select.onchange();
  }
  for (const input of container.querySelectorAll("input")) {
    if (input.type === "number") input.value = input.min;
    else input.value = "1 0.2";
    input.onchange();
  }
  container.querySelector("button").click();
  for (const input of container.querySelectorAll("input[type=number]"))
    input.onchange();
  [...container.querySelectorAll("button")].at(-1).click();
  assert.equal(result.envelope.length, 1);
  assert.equal(result.lanes[0].inserts.length, 2);
  api.mountEditor(container,JSON.stringify({lanes:[{source:"sine"}]}),(text)=>result=JSON.parse(text));
  const source=container.querySelector("select");source.value="saw";source.onchange();
  const level=container.querySelector('input[type="number"]');level.value="1.05";level.onchange();
  [...container.querySelectorAll("button")].at(-1).click();
  const golden=JSON.parse(fs.readFileSync(new URL("../fixtures/editor-gesture.json",import.meta.url),"utf8"));
  assert.deepEqual(result,golden);
});
test("author worker compiles complete data and rejects invalid or missing resources", async () => {
  const writes = new Map(),
    replies = [];
  const module = {
    FS: {
      mkdirTree() {},
      writeFile(k, v) {
        writes.set(k, v);
      },
    },
    ccall: () => 1,
    UTF8ToString: () => "name",
    _author_label: () => 2,
    _author_size: () => 8,
    _author_error: () => 2,
    HEAPU8: new Uint8Array(16),
  };
  const self = {};
  let notify;
  const data = {
    "pack.json":
      '{"presets":{"T":"t.json"},"script":"t.lua","assets":{"s":"s.wav"}}',
    "t.json": '{"sample":"s"}',
    "t.lua": "",
    "s.wav": "SAMP",
  };
  await load("worker.js", {
    self,
    importScripts() {},
    BlueAuthor: () => Promise.resolve(module),
    postMessage: (value) => {
      replies.push(value);
      notify(value);
    },
    fetch: async (u) => ({
      ok: u.pathname.slice(1) in data,
      status: 404,
      arrayBuffer: async () =>
        new TextEncoder().encode(data[u.pathname.slice(1)]).buffer,
    }),
  });
  const send = (args = {}) =>
    new Promise((resolve) => {
      notify = resolve;
      self.onmessage({
        data: { id: 7, root: "https://test/", preset: "T", ...args },
      });
    });
  assert.equal((await send()).buffer.byteLength, 8);
  assert.equal(
    (await send({ script: "change", patch: "{}" })).mapping,
    "change",
  );
  assert.match((await send({ preset: "bad" })).error, /Invalid/);
  delete data["s.wav"];
  assert.match((await send()).error, /404/);
  const assets = Object.fromEntries(
    Object.entries(data).map(([k, v]) => [k, new TextEncoder().encode(v)]),
  );
  assert.match((await send({ assets })).error, /Missing/);
  module.ccall = () => 0;
  assert.ok((await send({ patch: "{}" })).error);
});
test("worklet stages bounded chunks and recorder returns the live stereo samples", async () => {
  const types = {},
    messages = [];
  let commits = 0,
    available = false,
    queued;
  const memory = { buffer: new ArrayBuffer(100000) };
  const api = {
    memory,
    _initialize() {},
    audio_prepare: () => 1,
    audio_left: () => 0,
    audio_right: () => 8192,
    audio_event: (...args) => (queued = args),
    audio_event_at: (...args) => (queued = args),
    audio_size: () => 16384,
    audio_stage: () => (available ? 32768 : 0),
    audio_commit: () => commits++,
    audio_render: (n) => new Float32Array(memory.buffer, 0, n).fill(0.5),
  };
  class Base {
    constructor() {
      this.port = { postMessage: (m) => messages.push(m) };
    }
  }
  await load("worklet.js", {
    AudioWorkletProcessor: Base,
    registerProcessor: (name, type) => (types[name] = type),
    sampleRate: 48000,
    currentFrame: 1200,
    WebAssembly: {
      Instance: class {
        constructor() {
          this.exports = api;
        }
      },
    },
  });
  const audio = new types["blue-render"]({
    processorOptions: { module: null },
  });
  const out = [[new Float32Array(128), new Float32Array(128)]];
  const send = (data) => audio.port.onmessage({ data });
  send({ c: 0, i: 69, v: 1 });
  assert.deepEqual(queued, [0, 69, 1]);
  send({c:0,i:64,v:.5,frame:1237});
  assert.deepEqual(queued,[0,64,.5,37]);
  send({ buffer: new ArrayBuffer(5) });
  send({ id: 3, buffer: new ArrayBuffer(16384) });
  audio.process([], out);
  available = true;
  audio.process([], out);
  audio.process([], out);
  assert.equal(commits, 1);
  assert.equal(out[0][0][0], 0.5);
  assert.equal(messages[0].applied, 3);
  const rec = new types["blue-recorder"]();
  rec.process([[]], out);
  rec.port.onmessage({ data: false });
  rec.port.onmessage({ data: true });
  rec.process([[out[0][0]]], out);
  rec.process(out, out);
  rec.port.onmessage({ data: false });
  assert.equal(messages[1].samples.length, 512);
  assert.equal(messages[1].samples[1], 0.5);
  api.audio_prepare = () => 0;
  assert.throws(() => new types["blue-render"]({ processorOptions: {} }));
});
test("XR lifecycle, note ownership and listener-relative controls", async () => {
  const events = [],
    listeners = {},
    space = {},
    poses = [];
  const source = { gripSpace: {}, handedness: "right" };
  let loop,
    fail = false;
  const gl = new Proxy(
    {
      COMPILE_STATUS: 1,
      LINK_STATUS: 2,
      getShaderParameter: () => !fail,
      getProgramParameter: () => true,
      makeXRCompatible: async () => {},
    },
    { get: (t, k) => t[k] ?? (() => ({})) },
  );
  const session = {
    addEventListener: (k, f) => (listeners[k] = f),
    requestAnimationFrame: (f) => (loop = f),
    requestReferenceSpace: async () => space,
    updateRenderState(s) {
      this.renderState = s;
    },
    async end() {
      listeners.end();
    },
  };
  const nav = {};
  let status;
  const { enterXR } = await load("xr.js", {
    navigator: nav,
    document: { createElement: () => ({ getContext: () => gl }) },
    XRWebGLLayer: class {
      getViewport() {
        return { x: 0, y: 0, width: 100, height: 100 };
      }
    },
  });
  const start = () =>
    enterXR(
      (...a) => events.push(a),
      (s) => (status = s),
      (...a) => poses.push(a),
    );
  await assert.rejects(start());
  nav.xr = { requestSession: async () => session };
  await start();
  const frame = {
    getPose: () => ({ transform: { position: { x: -0.45, y: 0, z: -1.2 } } }),
    getViewerPose: () => ({
      transform: {
        inverse: { matrix: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1] },
      },
      views: [{ transform: { inverse: { matrix: [] } } }],
    }),
  };
  const input = { inputSource: source, frame };
  listeners.selectstart(input);
  listeners.squeezestart(input);
  loop(0, frame);
  listeners.squeezeend(input);
  listeners.selectend(input);
  listeners.selectstart(input);
  listeners.inputsourceschange({ removed: [source] });
  loop(1, { getViewerPose: () => null });
  await session.end();
  assert.ok(events.some((e) => e[0] === 0));
  assert.ok(events.some((e) => e[0] === 1));
  assert.ok(poses.length);
  assert.match(status, /Returned/);
  fail = true;
  await assert.rejects(start());
});

test("flat host dispatches performance commands and manages user revisions", async () => {
  const win = new Window({ url: "https://test/blue/" }),
    document = win.document;
  document.body.innerHTML = fs
    .readFileSync(site + "index.html", "utf8")
    .replace(/<script[^>]*>.*?<\/script>/gs, "");
  const el = (id) => document.getElementById(id),
    manifest = { format: 1, id: "factory", presets: { Tone: "tone.json" } };
  el("scope").getContext = () => ({
    clearRect() {},
    stroke() {},
    beginPath() {},
    lineTo() {},
    moveTo() {},
  });
  const records = new Map(),
    db = {
      close() {},
      transaction() {
        const tx = {
          objectStore: () => ({
            get(key) {
              const req = { result: records.get(key) };
              queueMicrotask(() => tx.oncomplete());
              return req;
            },
            put(value, key) {
              records.set(key, value);
              queueMicrotask(() => tx.oncomplete());
              return {};
            },
          }),
        };
        return tx;
      },
    };
  const indexedDB = {
    open() {
      const req = { result: db };
      queueMicrotask(() => req.onsuccess());
      return req;
    },
  };
  let worker,
    paint,
    edited,
    xrCalls = 0;
  const sounds = [],
    nodes = [];
  class Worker {
    constructor() {
      worker = this;
    }
    postMessage(data) {
      this.last = data;
      queueMicrotask(() =>
        this.onmessage({
          data: {
            id: data.id,
            buffer: new ArrayBuffer(4),
            document: data.patch || '{"lanes":[{"source":"sine"}]}',
            mapping: data.script || "mapping",
            labels: ["Bend"],
          },
        }),
      );
    }
  }
  class AudioWorkletNode {
    constructor(ctx, name) {
      this.port = {
        postMessage: (msg) => {
          sounds.push(msg);
          if (msg?.buffer)
            queueMicrotask(() =>
              this.port.onmessage({ data: { applied: msg.id } }),
            );
        },
      };
      nodes.push(this);
    }
    connect(n) {
      return n;
    }
  }
  const analyser = {
    connect() {},
    getFloatTimeDomainData(arr) {
      arr.fill(0.1);
    },
  };
  class AudioContext {
    constructor() {
      this.audioWorklet = { addModule: async () => {} };
    }
    resume() {
      return Promise.resolve();
    }
    close() {
      return Promise.resolve();
    }
    createAnalyser() {
      return analyser;
    }
  }
  class Option {
    constructor(text, value) {
      const o = document.createElement("option");
      o.text = text;
      o.value = value;
      return o;
    }
  }
  const input = {},
    navigator = {
      requestMIDIAccess: async () => ({ inputs: new Map([["MIDI", input]]) }),
    };
  const fetch = async (url) => ({
    ok: true,
    json: async () =>
      String(url).endsWith("index.json")
        ? { packs: ["packs/factory"] }
        : manifest,
    text: async () => "",
    arrayBuffer: async () => new ArrayBuffer(2),
  });
  const fakeURL = class extends URL {
    static createObjectURL() {
      return "blob:test";
    }
    static revokeObjectURL() {}
  };
  const overrides = {
    "editor.js": {
      mountEditor(c, s, apply) {
        edited = apply;
      },
    },
    "xr.js": {
      enterXR: async () => {
        xrCalls++;
      },
    },
    "packs.js": {
      exportPack: async (m, p, d, s, v, read) => {
        await read("tone.json");
        return new Blob();
      },
      unpack: () => ({ manifest, assets: { "tone.json": new Uint8Array() } }),
    },
  };
  await load(
    "app.js",
    {
      window: win,
      document,
      navigator,
      indexedDB,
      fetch,
      Worker,
      AudioWorkletNode,
      AudioContext,
      URL: fakeURL,
      Option,
      WebAssembly: { compileStreaming: async () => ({}) },
      requestAnimationFrame: (f) => {
        paint = f;
      },
      setTimeout: () => 0,
    },
    overrides,
  );
  const settle = () => new Promise((r) => setImmediate(r));
  await settle();
  assert.equal(el("name").textContent, "Tone");
  el("record").onclick();
  el("xr").onclick();
  assert.match(el("status").textContent, /Start audio/);
  await el("start").onclick();
  await settle();
  assert.equal(el("start").disabled, true);
  const keyboard = { key: "a", target: el("status"), preventDefault() {} };
  win.onkeydown(keyboard);
  win.onkeyup(keyboard);
  win.onkeydown({ ...keyboard, ctrlKey: true });
  win.onblur();
  const key = el("key60");
  key.setPointerCapture = () => {};
  key.onpointerdown({ pointerId: 9 });
  key.onlostpointercapture();
  el("m0").value = ".75";
  el("m0").oninput();
  assert.ok(sounds.some((m) => m?.c === 6 && m.v === 0.75));
  paint();
  el("record").onclick();
  el("record").onclick();
  nodes[1].port.onmessage({
    data: { samples: new Float32Array([0.3, 0.1]), rate: 48000 },
  });
  el("mapping").value = "updated";
  el("apply").onclick();
  await settle();
  await el("save").onclick();
  assert.match(el("status").textContent, /Saved/);
  el("reload").onclick();
  await settle();
  await el("export").onclick();
  await el("import").onchange({
    target: { files: [{ arrayBuffer: async () => new ArrayBuffer(5) }] },
  });
  await settle();
  assert.match(el("pack").value, /user/);
  el("pack").onchange();
  await settle();
  await el("export").onclick();
  edited('{"lanes":[{"source":"fm"}]}');
  await settle();
  assert.match(el("document").value, /fm/);
  el("library").firstChild.onclick();
  await settle();
  await el("midi").onclick();
  for (const msg of [
    [128, 60, 0],
    [144, 60, 80],
    [176, 64, 127],
    [176, 1, 70],
    [208, 70, 0],
    [224, 0, 64],
  ])
    input.onmidimessage({ data: msg });
  el("xr").onclick();
  await settle();
  assert.equal(xrCalls, 1);
  worker.onmessage({ data: { id: worker.last.id, error: "bad patch" } });
  assert.equal(el("status").textContent, "bad patch");
  worker.onerror({ message: "unavailable" });
  assert.equal(el("status").textContent, "unavailable");
  await win.happyDOM.abort();
});
