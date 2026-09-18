// Isolated fixtures only. All choices click the shipped UI against real WASM.
(function () {
  const ESC = "\u001b";
  const client = new window.UltimatumEngineClient(() => window.Module);
  const report = document.createElement("pre");
  report.id = "runtimeReport";
  report.style.cssText = "position:fixed;top:0;left:0;max-width:100%;max-height:100px;overflow:auto;z-index:90;background:#101820;color:white;font:11px monospace;white-space:pre-wrap";
  document.body.append(report);
  const cases = [
    {name:"Browse and cancel", fixture:20, steps:["explore",ESC,"travel",ESC,ESC], check:(a,b)=>b.moves===a.moves},
    {name:"Search location",fixture:20,steps:["explore","s"],check:(a,b)=>b.moves===a.moves+1},
    {name:"Make camp",fixture:20,steps:["explore","h"],check:(a,b)=>campAmbush ? campBattleState?.inputMode==="combat"&&campBattleState.messages.some(m=>m.includes("Ambushed!"))&&b.moves>=a.moves+1 : b.moves===a.moves+1&&b.party[0].hp>a.party[0].hp},
    {name:"Quest item cancellation", fixture:20, steps:["explore","u",ESC], check:(a,b)=>b.moves===a.moves},
    {name:"Use Horn", fixture:20, steps:["explore","u","horn"], check:(a,b)=>b.moves===a.moves+1},
    {name:"Open door", fixture:22, steps:["explore","o","n"], check:(a,b)=>b.moves===a.moves+1 && !(target()&1)},
    {name:"Unlock door", fixture:23, steps:["explore","j","n"], check:(a,b)=>b.moves===a.moves+1 && b.supplies.keys===a.supplies.keys-1 && !(target()&2)},
    {name:"Open chest", fixture:24, steps:["explore","g"], check:(a,b)=>b.moves===a.moves+1 && b.gold>a.gold && !(target()&4)},
    {name:"Light torch", fixture:25, steps:["explore","i"], check:(a,b)=>b.moves===a.moves+1 && b.supplies.torches===a.supplies.torches-1},
    {name:"Climb ladder", fixture:29, steps:["travel","k"], check:(a,b)=>b.moves===a.moves+1 && b.location.z===a.location.z-1},
    {name:"Descend ladder", fixture:30, steps:["travel","d"], check:(a,b)=>b.moves===a.moves+1 && b.location.z===a.location.z+1},
    {name:"Gem Peer", fixture:20, steps:["explore","p",ESC], check:(a,b)=>b.moves===a.moves+1 && b.supplies.gems===a.supplies.gems-1},
    {name:"Dungeon Gem Peer",fixture:25,steps:["explore","p",ESC],check:(a,b)=>b.moves===a.moves+1&&b.supplies.gems===a.supplies.gems-1&&b.viewMode===a.viewMode},
    {name:"Board horse", fixture:31, steps:["travel","b"], check:(a,b)=>b.moves===a.moves+1 && b.transport!==a.transport},
    {name:"Urge horse", fixture:26, steps:["travel","y"], check:(a,b)=>b.moves===a.moves+1},
    {name:"Leave horse", fixture:26, steps:["travel","x"], check:(a,b)=>b.moves===a.moves+1 && b.transport!==a.transport},
    {name:"Fire cannon", fixture:27, steps:["travel","f","n"], check:(a,b)=>b.moves===a.moves+1},
    {name:"Ascend balloon", fixture:28, steps:["travel","k"], check:(a,b)=>b.moves===a.moves+1},
    {name:"Land balloon",fixture:35,steps:["travel","d"],check:(a,b)=>b.moves===a.moves+1&&!client.call("zu4_web_test_flying","number")},
    {name:"Sextant", fixture:20, steps:["travel","l"], check:(a,b)=>b.moves===a.moves+1},
    {name:"Controls preferences", fixture:20, steps:["controls","bump","direct","walk",ESC,ESC], check:(a,b)=>b.moves===a.moves && b.preferences.bumpInteractions!==a.preferences.bumpInteractions && b.preferences.directInteractions!==a.preferences.directInteractions && b.preferences.tapToWalk!==a.preferences.tapToWalk && b.storageRevision>=a.storageRevision+3},
    ...["classic","ultimatum","assisted"].map(profile=>({name:"Profile "+profile,fixture:20,steps:["experience","profile",profile,"apply",ESC,ESC],check:(a,b)=>b.moves===a.moves && b.preferences.profile===profile && !b.preferences.customized})),
    {name:"Custom movement", fixture:20, steps:["experience","customize","movement","all",ESC,ESC], check:(a,b)=>b.moves===a.moves && b.preferences.customized && !b.preferences.filterMovementMessages},
    {name:"Restore profile defaults", fixture:20, steps:["experience","restore","apply",ESC,ESC],check:(a,b)=>b.moves===a.moves && !b.preferences.customized && b.preferences.bumpInteractions===a.preferences.bumpInteractions},
    {name:"Debug session switches", fixture:20, steps:["debug","page_session","collision","opacity","wind_lock",ESC,ESC,ESC], check:(a,b)=>b.moves===a.moves && b.debugState.collisionOverride && b.debugState.seeThroughWalls && !b.debugState.windLocked},
    {name:"Debug gem preview", fixture:20, steps:["debug","page_session","peer_preview",ESC,ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.supplies.gems===a.supplies.gems},
    {name:"Debug grant cancelled", fixture:20, steps:["debug","page_party","reagents",ESC,ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && JSON.stringify(b.supplies.reagents)===JSON.stringify(a.supplies.reagents)},
    {name:"Debug grant and recovery snapshot",fixture:20,steps:["debug","page_party","reagents","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.supplies.reagents.every(n=>n===99) && backupExists()},
    {name:"Debug grant equipment",fixture:20,steps:["debug","page_party","equipment","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.inventory.weapons.filter(i=>i.type>0).every(i=>i.count>=8) && b.inventory.armor.filter(i=>i.type>0).every(i=>i.count===8)},
    {name:"Debug party stats",fixture:20,steps:["debug","page_party","stats","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves&&client.call("zu4_web_test_stats","number")===150},
    {name:"Debug grant items",fixture:20,steps:["debug","page_party","items","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.gold===9999 && b.supplies.gems===99 && b.supplies.keys===99},
    {name:"Debug grant mixtures",fixture:20,steps:["debug","page_party","mixtures","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.spells.every(i=>i.mixtures===99)},
    {name:"Debug recruit companions",fixture:20,steps:["debug","page_party","companions","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.party.length>a.party.length},
    {name:"Debug complete virtues",fixture:20,steps:["debug","page_party","virtues","apply",ESC,"page_diagnostics","virtue_values",ESC,ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && history.some(p=>p.title==="Virtue values" && (p.context.match(/Avatar/g)||[]).length===8)},
    {name:"Debug advance moons",fixture:20,steps:["debug","page_world","moons","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.moons.trammel===(a.moons.trammel+1)%8},
    {name:"Debug set wind",fixture:20,steps:["debug","page_world","wind","east","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.wind==="East"},
    {name:"Debug named destination",fixture:20,steps:["debug","page_navigation","goto","5","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.location.x===82 && b.location.y===106},
    {name:"Debug moongate",fixture:20,steps:["debug","page_navigation","moongate","1","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && (b.location.x!==a.location.x||b.location.y!==a.location.y)},
    {name:"Debug dungeon entrance",fixture:20,steps:["debug","page_navigation","dungeon","0","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves+1 && b.location.x===240 && b.location.y===73},
    {name:"Debug altar room",fixture:20,steps:["debug","page_navigation","altar","0","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves+1 && b.location.z===7 && b.location.name==="Deceit"},
    {name:"Debug Lord British",fixture:20,steps:["debug","page_navigation","lord_british","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves+1 && b.location.context!==a.location.context},
    {name:"Debug exit map without snapshot",fixture:21,steps:["debug","page_navigation","exit_map","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.location.name==="Britannia"},
    {name:"Debug summon creature",fixture:20,steps:["debug","page_world","summon","rat","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves&&client.call("zu4_web_test_objects","number")===1},
    {name:"Debug create transport",fixture:20,steps:["debug","page_world","transport","h","north","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves&&client.call("zu4_web_test_objects","number")===1},
    {name:"Debug destroy object",fixture:33,steps:["debug","page_danger","destroy","apply","apply","n",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves&&client.call("zu4_web_test_objects","number")===0},
    {name:"Debug clear creatures",fixture:34,steps:["debug","page_danger","clear_creatures","apply","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves&&client.call("zu4_web_test_objects","number")===0},
    {name:"Debug final altar",fixture:20,steps:["debug","page_danger","final_altar","apply","apply",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves+1&&b.location.z===7&&b.location.name==="The Great Stygian Abyss"},
    {name:"Debug end combat",fixture:32,steps:["debug","page_danger","end_combat","apply","apply"],check:(a,b)=>a.inputMode==="combat"&&b.inputMode==="command"&&b.location.name==="Britannia"},
    {name:"Download debug recovery",fixture:20,steps:["debug","page_diagnostics","export_checkpoint",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves && b.recoveryDownload.request>a.recoveryDownload.request && client.recoveryBytes(b.recoveryDownload)?.[0]===80 && client.recoveryBytes(b.recoveryDownload)?.[1]===75},
    {name:"Danger action cancelled",fixture:20,steps:["debug","page_danger","clear_creatures",ESC,ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves},
    {name:"Save Game",fixture:20,steps:["save"],check:(a,b)=>b.moves===a.moves && b.storageRevision>a.storageRevision && b.saveRevision>a.saveRevision}
  ];
  const params = new URL(location.href).searchParams;
  const persistenceKey="ultimatum-menu-runtime-"+location.search;
  const reloaded=JSON.parse(sessionStorage.getItem(persistenceKey)||"null");
  const selected = params.get("case");
  const tests = params.has("debug_gate") ? [
    {name:"Debug compile gate",fixture:20,steps:[ESC],check:(a,b)=>!b.capabilities.debugTools&&!b.debugState&&b.moves===a.moves},
    {name:"Profile without VGA",fixture:20,steps:["experience","profile","assisted","apply",ESC,ESC],check:(a,b)=>b.moves===a.moves&&b.preferences.profile==="assisted"&&!b.preferences.customized&&b.video==="ega"&&!b.vgaAvailable},
    {name:"Movement preference without VGA",fixture:20,steps:["experience","customize","movement","all",ESC,ESC],check:(a,b)=>b.moves===a.moves&&!b.preferences.filterMovementMessages&&b.video==="ega"},
    {name:"Unavailable VGA disabled",fixture:20,steps:["experience","customize","graphics",ESC,ESC,ESC],check:(a,b)=>b.moves===a.moves&&b.video==="ega"}
  ] : selected ? cases.filter(t=>t.name===selected) : params.has("debug_only") ? cases.filter(t=>t.name.startsWith("Debug ") && !t.name.includes("gem")) : cases;
  let index=0, step=0, phase="prepare", generation=0, before, started=Date.now(), previousId=0;
  let mapSeen=false;
  let campAmbush=false;
  let campBattleState=null;
  const results=[], history=[];
  const target=()=>client.call("zu4_web_test_target_state","number");
  const backupExists=()=>{
    const fs=window.Module.FS;
    const root="/save/web-debug-checkpoints";
    // Use the engine's diagnostics path below if the platform root differs.
    const roots=[root,"/persistent/save/web-debug-checkpoints","/persistent/web-debug-checkpoints"];
    const walk=(path,depth)=>{
      if(depth>5) return false;
      try { return fs.readdir(path).filter(n=>n!=="."&&n!=="..").some(n=>n==="web-debug-checkpoints" ? fs.readdir(path+"/"+n).filter(x=>x!=="."&&x!=="..").some(x=>{try{fs.stat(path+"/"+n+"/"+x+"/party.sav");return true;}catch{return false;}}) : fs.isDir(fs.stat(path+"/"+n).mode)&&walk(path+"/"+n,depth+1)); } catch { return false; }
    };
    return roots.some(p=>{try{return fs.readdir(p).some(n=>{try{fs.stat(p+"/"+n+"/party.sav");return true;}catch{return false;}});}catch{return false;}})||walk("/",0);
  };
  const write=(status,extra={})=>report.textContent=JSON.stringify({status,test:tests[index]?.name,phase,step,history,results,...extra},null,2);
  const finish=(state)=>{
    const test=tests[index];
    const assertions={effect:test.check(before,state),commandsReturned:state.inputMode==="command",controlsEnabled:!document.querySelector(".command[data-key]").disabled,sheetClosed:!document.querySelector(".semantic-prompt")};
    if(test.name.includes("gem")||test.name.includes("Gem")) assertions.mapRendered=mapSeen;
    results.push({name:test.name,status:Object.values(assertions).every(Boolean)?"PASS":"FAIL",assertions,before,after:state,history:[...history]});
    if(!Object.values(assertions).every(Boolean)) throw Error("Assertion failed: "+JSON.stringify(assertions));
    if(++index===tests.length){
      clearInterval(timer);write("PERSISTING");
      // The app owns the mount; this independent observer must flush the
      // actual FS, not rely on its own uninitialized mount flag.
      client.syncSaves(false).then(()=>{
        sessionStorage.setItem(persistenceKey,JSON.stringify({results,preferences:state.preferences,video:state.video,config:window.Module.FS.readFile("/home/web_user/.xu4/xu4rc",{encoding:"utf8"}).split("\n").slice(0,8)}));
        location.reload();
      }).catch(error=>write("FAIL",{error:error.message}));
      return;
    }
    phase="prepare";step=0;previousId=0;mapSeen=false;campAmbush=false;campBattleState=null;history.length=0;started=Date.now();write("RUNNING");
  };
  const timer=setInterval(()=>{
    try {
      if(!window.Module?.ccall || !document.querySelector("#engineStatus").hidden) return;
      const state=client.snapshot();
      if(reloaded){
        const persisted=JSON.stringify(state.preferences)===JSON.stringify(reloaded.preferences)&&state.video===reloaded.video;
        const sessionReset=!state.debugState || (!state.debugState.collisionOverride&&!state.debugState.seeThroughWalls);
        sessionStorage.removeItem(persistenceKey);clearInterval(timer);
        report.textContent=JSON.stringify({status:persisted&&sessionReset?"SUITE PASS":"FAIL",results:reloaded.results,persistence:{persisted,sessionReset,expected:reloaded.preferences,actual:state.preferences,expectedConfig:reloaded.config,actualConfig:window.Module.FS.readFile("/home/web_user/.xu4/xu4rc",{encoding:"utf8"}).split("\n").slice(0,8)}},null,2);
        return;
      }
      if(Date.now()-started>30000) throw Error("Timeout: "+JSON.stringify(state.prompt)+" input="+state.inputMode);
      if(phase==="prepare"){
        generation=client.call("zu4_web_test_generation","number");
        if(!client.call("zu4_web_test_prepare","number",["number"],[tests[index].fixture])) return;
        phase="fixture";write("RUNNING");return;
      }
      if(phase==="fixture"){
        if(client.call("zu4_web_test_generation","number")===generation || !["command","combat"].includes(state.inputMode)) return;
        if(document.querySelector("#locationName").textContent!==state.location.name ||
           document.querySelector("#partyCount").textContent!==`${state.party.length} ${state.party.length===1?"member":"members"}` ||
           Number(document.querySelector("#turnCount").textContent.replace(/,/g,""))!==state.moves) return;
        before=state;
        const world=document.querySelector("#worldCanvas").getBoundingClientRect();
        before.worldRect={x:world.x,y:world.y,width:world.width,height:world.height};
        if([22,23,24].includes(tests[index].fixture) && !target()) throw Error("Fixture has no expected target");
        const button=[document.querySelector("#menuButton"),document.querySelector("#mobileMenuButton")].find(b=>b.getClientRects().length&&!b.disabled);
        if(!button) return;
        button.click();
        if(client.openMenu()) throw Error("Duplicate menu open accepted");
        phase="choices";write("RUNNING");return;
      }
      if(step===tests[index].steps.length && state.inputMode==="combat" && tests[index].name==="Make camp" && !campAmbush){
        const menu=[document.querySelector("#menuButton"),document.querySelector("#mobileMenuButton")].find(b=>b.getClientRects().length&&!b.disabled);
        if(!menu) return;
        campAmbush=true;
        campBattleState=state;
        tests[index].steps.push("debug","page_danger","end_combat","apply","apply");
        menu.click();previousId=0;write("RUNNING");return;
      }
      if(step===tests[index].steps.length && state.inputMode==="command"){
        if(document.querySelector(".conversation-dock").dataset.inputMode==="command") finish(state);
        return;
      }
      const p=state.prompt;
      if(!p.id && p.kind==="direction" && tests[index].steps[step]==="n") {
        const direction=document.querySelector('[data-prompt-key="1001"]');
        if(!direction) return;
        history.push({title:"Choose direction",kind:p.kind,value:"n"});
        direction.click();step++;write("RUNNING");return;
      }
      if(!p.id||p.id===previousId||p.submitted || document.querySelector(".conversation-dock").dataset.promptId!==String(p.id)) return;
      const dock=document.querySelector(".conversation-dock").getBoundingClientRect();
      if(dock.top<0||dock.bottom>innerHeight+1||dock.left<0||dock.right>innerWidth+1) throw Error("Prompt is outside the visible viewport");
      if(p.title==="Menu"&&step===0){
        const world=document.querySelector("#worldCanvas").getBoundingClientRect();
        for(const name of ["x","y","width","height"]) if(Math.abs(world[name]-before.worldRect[name])>1) throw Error("Opening Menu changed world "+name);
      }
      if(client.openMenu()||client.saveAdventure()||client.activatePrimaryAction()||client.tapWorld(0,1)) throw Error("Mutation accepted while menu open");
      if(params.has("debug_gate") && p.options.some(o=>o.value==="debug")) throw Error("Debug Tools visible in disabled build");
      for(const option of p.options.filter(o=>o.disabled)){
        if(client.submitAnswer(option.value,p.id)) throw Error("Disabled answer accepted");
        const disabled=[...document.querySelectorAll("[data-prompt-value]")].find(b=>b.dataset.promptValue===option.value);
        if(!disabled?.disabled) throw Error("Unavailable feature appears enabled");
      }
      if(state.peerMap){
        const pixels=client.mapPixels(state.peerMap);
        if(!pixels || pixels.length!==state.peerMap.width*state.peerMap.height*4 || document.querySelector("#peerMapCanvas").hidden) throw Error("Peer map not rendered");
        if(state.location.context==="Dungeon") {
          const colors=new Set();
          for(let at=0;at<pixels.length;at+=4) colors.add(`${pixels[at]},${pixels[at+1]},${pixels[at+2]}`);
          if(colors.size<2) throw Error("Dungeon map does not distinguish terrain");
        }
        mapSeen=true;
      }
      if(previousId && client.submitAnswer("apply",previousId)) throw Error("Stale answer accepted");
      const value=tests[index].steps[step];
      const button=[...document.querySelectorAll("[data-prompt-value]")].find(b=>b.dataset.promptValue===value);
      history.push({title:p.title,context:p.context,kind:p.kind,value,options:p.options.map(o=>o.value)});
      if(!button || button.disabled) throw Error("Missing choice "+JSON.stringify(value)+" in "+JSON.stringify(p));
      button.click();
      if(client.submitAnswer(value,p.id)) throw Error("Duplicate answer accepted");
      previousId=p.id;step++;write("RUNNING");
    } catch(error){clearInterval(timer);write("FAIL",{error:error.message||"FS/engine error "+error.errno});}
  },180);
})();
