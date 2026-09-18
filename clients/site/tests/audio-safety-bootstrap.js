// Test-only interception BEFORE engine startup: measure actual Web Audio PCM,
// then erase it. No test sample is allowed to reach the speakers/headphones.
(function () {
  const report=document.createElement("pre");report.id="webAudioSafetyReport";
  report.style.cssText="position:fixed;bottom:0;left:0;z-index:20000;background:#000;color:#9f9;font:12px monospace";
  addEventListener("DOMContentLoaded",()=>document.body.append(report));
  const stats={callbacks:0,samples:0,invalid:0,peak:0,sampleRate:0,silenced:true};
  const Audio=window.AudioContext || window.webkitAudioContext;
  const create=Audio.prototype.createScriptProcessor;
  Audio.prototype.createScriptProcessor=function(...args) {
    stats.sampleRate=this.sampleRate;
    report.textContent=JSON.stringify({...stats,contextState:this.state});
    // A real test click may unlock suspended audio; output remains muted.
    addEventListener("pointerdown",()=>this.resume(),{once:true});
    const node=create.apply(this,args);
    const connect=node.connect.bind(node);
    const mute=this.createGain();mute.gain.value=0;mute.connect(this.destination);
    node.connect=()=>connect(mute);
    Object.defineProperty(node,"onaudioprocess",{
      set(handler) {
        if(!handler)return;
        node.addEventListener("audioprocess",event=>{
          try {
            handler(event);stats.callbacks++;
            for(let c=0;c<event.outputBuffer.numberOfChannels;c++) {
              const pcm=event.outputBuffer.getChannelData(c);
              for(const value of pcm) {
                stats.samples++;
                if(!Number.isFinite(value) || Math.abs(value)>1)stats.invalid++;
                else stats.peak=Math.max(stats.peak,Math.abs(value));
              }
              pcm.fill(0);
            }
          } finally {
            for(let c=0;c<event.outputBuffer.numberOfChannels;c++)event.outputBuffer.getChannelData(c).fill(0);
            report.textContent=JSON.stringify(stats);
          }
        });
      }
    });
    return node;
  };
})();
