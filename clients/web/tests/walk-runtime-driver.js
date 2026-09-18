// Test-only: shipped DOM controls and real WASM rules, isolated from LAN saves.
(function () {
  const client = new window.UltimatumEngineClient(() => window.Module);
  const report = document.createElement("pre");
  report.id = "runtimeReport";
  report.style.cssText = "position:fixed;top:0;left:0;max-width:100%;max-height:100px;overflow:auto;z-index:90;background:#101820;color:white;font:11px monospace;white-space:pre-wrap";
  document.body.append(report);
  const cases = [
    {name:"Three-square route and subsequent D-pad", fixture:36, action:"plain"},
    {name:"Latest rapid tap wins", fixture:36, action:"rapid"},
    {name:"Retarget after first square", fixture:36, action:"retarget"},
    {name:"D-pad cancels route", fixture:36, action:"cancel"},
    {name:"Menu cancels pending route step", fixture:36, action:"menu"},
    {name:"Approach door and open", fixture:39, action:"door"},
    {name:"Approach NPC, Goodbye, and return to movement", fixture:38, action:"npc"},
    {name:"Encounter after first square remains responsive", fixture:37, action:"encounter"}
  ];
  const selected = Number(new URL(location.href).searchParams.get("fixture"));
  const tests = selected ? cases.filter(t => t.fixture === selected) : cases;
  const errors = [], results = [];
  window.addEventListener("error", event => errors.push(event.message));
  window.addEventListener("unhandledrejection", event => errors.push(String(event.reason)));
  let index=0, phase="prepare", generation, before, started=Date.now(), phaseStarted=started, expected, previousPrompt=0;
  let doorFeedback=null;
  const write = (status, extra={}) => report.textContent=JSON.stringify({status,test:tests[index]?.name,phase,errors,results,...extra},null,2);
  const transition = next => { phase=next; phaseStarted=Date.now(); };
  const summary = s => ({moves:s.moves,location:s.location,mode:s.inputMode});
  const assert = (value, message) => { if(!value) throw Error(message); };
  const tap = (x,y) => {
    const canvas=document.querySelector("#worldCanvas"), r=canvas.getBoundingClientRect();
    canvas.dispatchEvent(new MouseEvent("click",{bubbles:true,clientX:r.left+r.width*(5.5+x)/11,clientY:r.top+r.height*(5.5+y)/11}));
  };
  const east = () => document.querySelector('[data-key="1004"]').click();
  const menu = () => {
    const button=[document.querySelector("#menuButton"),document.querySelector("#mobileMenuButton")].find(b=>b.getClientRects().length&&!b.disabled);
    assert(button,"Menu is not usable"); button.click();
  };
  const answer = value => {
    const b=[...document.querySelectorAll("[data-prompt-value]")].find(b=>b.dataset.promptValue===value&&!b.disabled);
    if(!b) return false; b.click(); return true;
  };
  const finish = state => {
    assert(!errors.length,"Engine/browser error: "+errors.join("; "));
    assert(!document.querySelector('[data-key="1004"]').disabled,"Movement did not return");
    results.push({name:tests[index].name,status:"PASS",before,after:summary(state),...(doorFeedback?{doorFeedback}:{})});
    if(++index===tests.length) { clearInterval(timer); write("SUITE PASS"); return; }
    transition("prepare"); started=Date.now(); previousPrompt=0; write("RUNNING");
  };
  const timer=setInterval(()=>{
    try {
      if(!window.Module?.ccall || !document.querySelector("#engineStatus").hidden) return;
      assert(!errors.length,"Engine/browser error: "+errors.join("; "));
      const state=client.snapshot(), test=tests[index];
      if(Date.now()-started>15000) throw Error("Timed out: "+JSON.stringify(summary(state)));
      if(phase==="prepare") {
        doorFeedback=null;
        generation=client.call("zu4_web_test_generation","number");
        if(!client.call("zu4_web_test_prepare","number",["number"],[test.fixture])) return;
        transition("fixture"); return;
      }
      if(phase==="fixture") {
        if(client.call("zu4_web_test_generation","number")===generation || Date.now()-phaseStarted<200 || state.inputMode!=="command") return;
        before=summary(state);
        if(test.action==="rapid") { tap(0,-3);tap(3,0);expected={x:before.location.x+3,y:before.location.y,moves:before.moves+3}; }
        else {tap(0,-3);expected={x:before.location.x,y:before.location.y-3,moves:before.moves+3};}
        transition("walking");write("RUNNING");return;
      }
      if(phase==="walking") {
        if(["retarget","cancel","menu"].includes(test.action)&&state.moves===before.moves+1) {
          assert(state.location.y===before.location.y-1,"First route step was incorrect");
          if(test.action==="retarget") {tap(3,0);expected={x:before.location.x+3,y:before.location.y-1,moves:before.moves+4};transition("destination");}
          else if(test.action==="cancel") {east();expected={x:before.location.x+1,y:before.location.y-1,moves:before.moves+2};transition("settle");}
          else {menu();transition("paused");}
          return;
        }
        if(test.action==="npc"&&state.prompt.kind==="text"&&state.prompt.id&&state.prompt.id!==previousPrompt) {
          if(!answer("bye")) return;
          previousPrompt=state.prompt.id;expected={x:before.location.x,y:before.location.y-2,moves:before.moves+3};transition("destination");return;
        }
        if(test.action==="encounter"&&state.inputMode==="combat") {menu();transition("combatMenu");return;}
        if(test.action==="door") expected={x:before.location.x,y:before.location.y-2,moves:before.moves+3};
        if(state.inputMode!=="command"||state.moves!==expected.moves) return;
        transition("destination");
      }
      if(phase==="paused"||phase==="combatMenu") {
        if(state.prompt.title!=="Menu"||Date.now()-phaseStarted<250) return;
        if(phase==="paused") assert(state.moves===before.moves+1,"Route continued behind Menu");
        if(!answer("\u001b")) return;
        transition(phase==="combatMenu"?"combatResumed":"resumed");return;
      }
      if(phase==="combatResumed") {
        if(state.inputMode!=="combat"||state.interactionActive||document.querySelector('[data-key="1004"]').disabled||document.querySelector(".command-bar").inert) return;
        assert(state.moves>=before.moves+1,"Encounter did not follow a move");finish(state);return;
      }
      if(phase==="resumed") {
        if(state.inputMode!=="command"||document.querySelector('[data-key="1004"]').disabled||document.querySelector(".command-bar").inert) return;
        expected={x:before.location.x+1,y:before.location.y-1,moves:before.moves+2};east();transition("settle");return;
      }
      if(phase==="destination") {
        if(state.inputMode!=="command"||state.moves!==expected.moves) return;
        if(document.querySelector('[data-key="1004"]').disabled||document.querySelector(".command-bar").inert) return;
        assert(state.location.x===expected.x&&state.location.y===expected.y,"Wrong route destination");
        if(test.action==="door") {
          // Snapshot polling can lead the shell's next animation-frame render.
          if(Date.now()-phaseStarted<150) return;
          assert(!(client.call("zu4_web_test_target_state","number")&1),"Door was not opened");
          const opened=state.messages.find(m=>m.includes("Opened!"));
          assert(opened,"Real engine did not report the opened door");
          assert(document.querySelector("#latestMessage").textContent===state.messages.at(-1).trim(),"Latest feedback retained edge whitespace");
          const log=[...document.querySelectorAll("#messageLog article p")].find(p=>p.textContent.includes("Opened!"));
          assert(log?.textContent===opened.trim(),"Log retained edge whitespace on Opened!");
          doorFeedback={engine:opened,strip:document.querySelector("#latestMessage").textContent,log:log.textContent};
        }
        expected.x++; expected.moves++;east();transition("settle");return;
      }
      if(phase==="settle"&&Date.now()-phaseStarted>350) {
        assert(state.moves===expected.moves&&state.location.x===expected.x&&state.location.y===expected.y,"Unexpected route continuation or unresponsive D-pad");
        assert(state.inputMode==="command"&&!state.interactionActive,"Input remained blocked");finish(state);
      }
    } catch(error) {clearInterval(timer);write("FAIL",{error:error.message,before,after:summary(client.snapshot())});}
  },20);
  write("STARTING");
})();
