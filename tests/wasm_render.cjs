/* Drive the built Wasm binaries using the same pack and score as blue-render. */
const fs=require("node:fs"),path=require("node:path");
async function main(){
  const [dist,id,score,output]=process.argv.slice(2);
  const author=await require(path.resolve(dist,"author.js"))();
  const root=path.resolve(dist,"content/packs/blue.factory");
  function copy(dir){
    author.FS.mkdirTree("/pack/"+dir);
    for(const entry of fs.readdirSync(path.join(root,dir),{withFileTypes:true})){
      const rel=path.join(dir,entry.name);
      if(entry.isDirectory())copy(rel);
      else author.FS.writeFile("/pack/"+rel.split(path.sep).join("/"),fs.readFileSync(path.join(root,rel)));
    }
  }copy("");
  const ptr=author.ccall("author_load","number",["string","string"],["/pack",id]);
  if(!ptr)throw Error(author.UTF8ToString(author._author_error()));
  const {instance}=await WebAssembly.instantiate(fs.readFileSync(path.join(dist,"audio.wasm")),{});
  const a=instance.exports;a._initialize();
  const [head,...events]=fs.readFileSync(score,"utf8").trim().split(/\r?\n/).map(line=>line.trim().split(/\s+/).map(Number));
  const [rate,total]=head;a.audio_init(rate);const bank=a.audio_stage();
  new Uint8Array(a.memory.buffer,bank,a.audio_size()).set(author.HEAPU8.subarray(ptr,ptr+author._author_size()));a.audio_commit();
  const left=new Float32Array(a.memory.buffer,a.audio_left(),128),right=new Float32Array(a.memory.buffer,a.audio_right(),128),pcm=new Float32Array(total*2);
  for(let i=0,cursor=0;i<total;){
    while(cursor<events.length && events[cursor][0]===i){a.audio_event(...events[cursor].slice(1));cursor++;}
    const n=Math.min(128,total-i,(events[cursor]?.[0]??total)-i);a.audio_render(n);
    for(let k=0;k<n;k++){pcm[(i+k)*2]=left[k];pcm[(i+k)*2+1]=right[k];}i+=n;
  }fs.writeFileSync(output,Buffer.from(pcm.buffer));
}
main().catch(e=>{console.error(e);process.exitCode=1;});
