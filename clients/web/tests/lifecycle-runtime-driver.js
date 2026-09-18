// Simulates the document lifecycle edge followed by a browser-process reload.
// The authoritative slot must reopen without returning through the title UI.
(function () {
  const client=new window.UltimatumEngineClient(()=>window.Module);
  const key="ultimatum-lifecycle-runtime-v1";
  const report=document.createElement("pre");
  report.id="runtimeReport";
  report.style.cssText="position:fixed;top:0;left:0;max-height:100px;max-width:100%;overflow:auto;z-index:90;background:#101820;color:white;font:11px monospace;white-space:pre-wrap";
  document.body.append(report);
  let phase="loading",generation=0,beforeMoves=0,since=Date.now();
  const prior=JSON.parse(sessionStorage.getItem(key)||"null");
  const write=(status,extra={})=>{report.textContent=JSON.stringify({status,phase,...extra},null,2);};
  const fail=error=>{clearInterval(timer);write("FAIL",{error:error.message||String(error)});};
  const timer=setInterval(()=>{
    try {
      if(!window.Module?.ccall || !document.querySelector("#engineStatus").hidden) return;
      if(Date.now()-since>20000) throw Error("Timed out in "+phase);
      const state=client.snapshot();
      if(prior) {
        if(document.querySelector("#adventureDialog").open || state.inputMode!=="command") return;
        const lifecycle=JSON.parse(sessionStorage.getItem("ultimatum-last-lifecycle-save-v1")||"null");
        if(!lifecycle) throw Error("The page lifecycle did not record a secured checkpoint");
        if(state.moves!==prior.moves) throw Error(`Reload restored ${state.moves} moves instead of ${prior.moves}`);
        if(!sessionStorage.getItem("ultimatum-active-session-v1")) throw Error("Active adventure marker was lost");
        sessionStorage.removeItem(key);
        clearInterval(timer);
        write("SUITE PASS",{savedMoves:prior.moves,restoredMoves:state.moves,lifecycleReason:lifecycle.reason,dialogOpen:false});
        return;
      }
      if(phase==="loading") {
        generation=client.call("zu4_web_test_generation","number");
        if(!client.call("zu4_web_test_prepare","number",["number"],[20])) return;
        phase="fixture";since=Date.now();write("RUNNING");return;
      }
      if(phase==="fixture") {
        if(client.call("zu4_web_test_generation","number")===generation || state.inputMode!=="command") return;
        beforeMoves=state.moves;
        document.querySelector('[data-key="32"]').click();
        phase="turn";since=Date.now();write("RUNNING");return;
      }
      if(phase==="turn") {
        if(state.moves<=beforeMoves || state.inputMode!=="command") return;
        document.querySelector("#saveButton").click();
        phase="save";since=Date.now();write("RUNNING",{moves:state.moves});return;
      }
      if(phase==="save") {
        const active=sessionStorage.getItem("ultimatum-active-session-v1");
        if(!active || document.querySelector("#saveButton").disabled) return;
        sessionStorage.setItem(key,JSON.stringify({moves:state.moves}));
        window.dispatchEvent(new PageTransitionEvent("pagehide",{persisted:false}));
        phase="pagehide";since=Date.now();write("RUNNING",{moves:state.moves});return;
      }
      if(phase==="pagehide") {
        if(!sessionStorage.getItem("ultimatum-last-lifecycle-save-v1")) return;
        clearInterval(timer);location.reload();
      }
    } catch(error) {fail(error);}
  },100);
  write("LOADING");
})();
