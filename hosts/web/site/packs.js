import { zipSync, unzipSync, strToU8, strFromU8 } from "./zip-code.mjs";
export function relative(file) {
  if (
    typeof file !== "string" ||
    !file ||
    file.startsWith("/") ||
    file.split("/").some((s) => !s || s === "." || s === "..") ||
    !/^[a-zA-Z0-9_./-]+$/.test(file)
  )
    throw Error("Invalid pack path " + file);
  return file;
}
export async function exportPack(
  manifest,
  preset,
  document,
  script,
  macros,
  read,
) {
  const id = manifest.id;
  if (!/^[a-zA-Z0-9_.-]+$/.test(id)) throw Error("Invalid pack ID");
  const folder = "packs/" + id,
    prefix = folder + "/";
  const files = Object.create(null),
    description = {
      ...manifest,
      presets: { [preset]: manifest.presets[preset] },
    };
  files["index.json"] = strToU8(JSON.stringify({ packs: [folder] }));
  files[prefix + "pack.json"] = strToU8(JSON.stringify(description, null, 2));
  files[prefix + relative(description.presets[preset])] = strToU8(
    JSON.stringify({ ...JSON.parse(document), macros }, null, 2),
  );
  files[prefix + relative(manifest.script)] = strToU8(script);
  for (const file of Object.values(manifest.assets || {}))
    files[prefix + relative(file)] = await read(file);
  return new Blob([zipSync(files)], { type: "application/zip" });
}
export function unpack(bytes) {
  if (bytes.byteLength > 16 * 1024 * 1024) throw Error("Pack exceeds 16 MiB");
  let total = 0,
    count = 0;
  const expanded = unzipSync(bytes, {
    filter(entry) {
      if (entry.name.endsWith("/")) return false;
      relative(entry.name);
      total += entry.originalSize;
      if (
        ++count > 512 ||
        total > 32 * 1024 * 1024 ||
        entry.originalSize > 2 * 1024 * 1024
      )
        throw Error("Pack exceeds content limits");
      return true;
    },
  });
  const text = (file) => {
    if (!expanded[file]) throw Error("Missing " + file);
    return strFromU8(expanded[file]);
  };
  const index = JSON.parse(text("index.json"));
  if (index.packs.length !== 1) throw Error("Select a single-pack ZIP");
  const prefix = relative(index.packs[0]) + "/",
    manifest = JSON.parse(text(prefix + "pack.json"));
  if (
    !/^[a-zA-Z0-9_.-]+$/.test(manifest.id) ||
    manifest.format !== 1 ||
    manifest.luaApi !== 1
  )
    throw Error("Incompatible pack");
  const assets = Object.create(null);
  for (const [file, content] of Object.entries(expanded))
    if (file.startsWith(prefix)) assets[file.slice(prefix.length)] = content;
  for (const rel of [
    manifest.script,
    ...Object.values(manifest.presets),
    ...Object.values(manifest.assets || {}),
  ])
    text(prefix + relative(rel));
  return { manifest, assets };
}
