importScripts("author.js");
const ready = BlueAuthor();
let chain = Promise.resolve();
self.onmessage = ({data}) => {
  chain = chain.then(() => prepare(data)).catch(error => postMessage({id:data.id,error:String(error.message)}));
};
async function prepare({id, root, preset, patch, script, assets}) {
  const module = await ready;
  const folder = "/revision";
  module.FS.mkdirTree(folder);
  async function get(file, override) {
    if (!file || file.startsWith("/") || file.includes("..") || !/^[\w.\-/]+$/.test(file)) throw Error("Invalid pack path " + file);
    let bytes;
    if (override !== undefined) bytes = new TextEncoder().encode(override);
    else if(assets){bytes=assets[file];if(!bytes)throw Error("Missing "+file);}
    else {
      const response = await fetch(new URL(file,root),{cache:"no-cache"});
      if (!response.ok) throw Error(file + " " + response.status);
      bytes = new Uint8Array(await response.arrayBuffer());
    }
    if(bytes.length>2*1024*1024) throw Error("Asset exceeds pack budget: "+file);
    const path = folder+"/"+file;
    module.FS.mkdirTree(path.substring(0,path.lastIndexOf("/")));
    module.FS.writeFile(path,bytes);
    return bytes;
  }
  const decode = b=>new TextDecoder().decode(b);
  const manifest=JSON.parse(decode(await get("pack.json")));
  const document=decode(await get(manifest.presets[preset],patch));
  const mapping=decode(await get(manifest.script,script));
  const sample=JSON.parse(document).sample;
  if(sample) await get(manifest.assets[sample]);
  const ptr=module.ccall("author_load","number",["string","string"],[folder,preset]);
  if(!ptr) throw Error(module.UTF8ToString(module._author_error()));
  const buffer=module.HEAPU8.slice(ptr,ptr+module._author_size()).buffer;
  const labels=Array.from({length:8},(_,i)=>module.UTF8ToString(module._author_label(i)));
  postMessage({id,buffer,document,mapping,labels},[buffer]);
}
