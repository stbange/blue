import vm from "node:vm";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import * as zip from "fflate";
export const site = fileURLToPath(
  new URL("../../hosts/web/site/", import.meta.url),
);
export async function load(name, globals = {}, overrides = {}) {
  const context = vm.createContext({
    console,
    URL,
    Blob,
    TextEncoder,
    TextDecoder,
    Uint8Array,
    Float32Array,
    ArrayBuffer,
    structuredClone,
    ...globals,
  });
  const files = new Map();
  async function module(key) {
    if (files.has(key)) return files.get(key);
    const exports = overrides[key] || (key === "zip-code.mjs" ? zip : null);
    let result;
    if (exports) {
      result = new vm.SyntheticModule(
        Object.keys(exports),
        function () {
          for (const [k, v] of Object.entries(exports)) this.setExport(k, v);
        },
        { context },
      );
    } else
      result = new vm.SourceTextModule(
        fs.readFileSync(path.join(site, key), "utf8"),
        { context, identifier: path.join(site, key) },
      );
    files.set(key, result);
    await result.link((id) => module(id.replace(/^\.\//, "")));
    return result;
  }
  const mod = await module(name);
  await mod.evaluate();
  return mod.namespace;
}
