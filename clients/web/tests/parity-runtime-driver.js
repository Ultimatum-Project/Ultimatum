// Isolated real-engine coverage for the parity features shared with iOS.
(function () {
  const runtimeModule = () => document.ultimatumRuntimeModule || window.UltimatumRuntimeModule || window.Module;
  const client = new window.UltimatumEngineClient(runtimeModule);
  const report = document.createElement("pre");
  report.id = "runtimeReport";
  report.style.cssText = "position:fixed;top:0;left:0;max-height:100px;max-width:100%;overflow:auto;z-index:90;background:#101820;color:white;font:11px monospace;white-space:pre-wrap";
  document.body.append(report);
  const results = [], errors = [];
  let phase = "world-prepare", generation = 0, before = null, since = Date.now();
  const assert = (condition, message) => { if (!condition) throw Error(message); };
  const write = status => { report.textContent = JSON.stringify({status,phase,results,errors},null,2); };
  const next = value => { phase=value; since=Date.now(); write("RUNNING"); };
  const pass = (name, details={}) => results.push({name,status:"PASS",...details});
  const mobile = matchMedia("(max-width:650px), (max-width:980px) and (max-height:500px)").matches;
  const dungeonDpadMatches = overhead => {
    const expected=overhead ? {north:"North",south:"South",west:"West",east:"East"} :
      {north:"Forward",south:"Back",west:"Turn left",east:"Turn right"};
    return Object.entries(expected).every(([move,label]) => {
      const buttons=[...document.querySelectorAll(`.dpad [data-move="${move}"]`)];
      return buttons.length>0 && buttons.every(button=>button.getAttribute("aria-label")===label);
    });
  };
  const prepare = (scenario, target) => {
    generation=client.call("zu4_web_test_generation","number");
    assert(client.call("zu4_web_test_prepare","number",["number"],[scenario]),"Fixture request rejected");
    next(target);
  };
  const fixtureReady = state => client.call("zu4_web_test_generation","number")!==generation &&
    ["command","combat"].includes(state.inputMode) && !state.interactionActive;
  const timer=setInterval(()=>{
    try {
      if(!runtimeModule()?.ccall || !document.querySelector("#engineStatus").hidden) return;
      assert(Date.now()-since<15000,"Timed out in "+phase);
      const state=client.snapshot();
      if(phase==="world-prepare") {
        client.ensureDirectory("/game-data-check");
        client.ensureDirectory("/game-data-check");
        pass("Current Emscripten filesystem accepts an existing account game-data workspace",{analyzePath:typeof runtimeModule().FS.analyzePath});
        prepare(20,"world");
      }
      else if(phase==="world") {
        if(!fixtureReady(state)) return;
        assert(state.capabilities.explorationMap && state.capabilities.mapPins,"Map capabilities are not declared");
        assert(state.minimap?.kind===1 && client.mapPixels(state.minimap)?.length===33*33*4,"World minimap is not a live RGBA map");
        assert(!document.querySelector("#mapButton").disabled && !document.querySelector("#minimapButton").disabled,"Map controls are disabled");
        document.querySelector("#mapButton").click(); next("world-map");
      } else if(phase==="world-map") {
        const dialog=document.querySelector("#mapDialog"), map=client.explorationMap();
        if(!dialog.open || !map) return;
        assert(map.kind===1 && map.width===256 && map.height===256,"World exploration map dimensions are wrong");
        assert(document.querySelector("#explorationMapCanvas").width===map.width,"World map canvas did not render");
        assert(client.setMapPin(map.x,map.y,"Parity QA"),"Could not pin the explored party cell");
        const pinned=client.snapshot();
        assert(pinned.mapPins.some(pin=>pin.x===map.x&&pin.y===map.y&&pin.label==="Parity QA"),"Map pin did not enter engine state");
        assert(client.removeMapPin(map.x,map.y),"Could not remove the map pin");
        document.querySelector("#mapClose").click();
        pass("World minimap, explored map and engine-owned pins",{size:[map.width,map.height],player:[map.x,map.y]});
        prepare(25,"dungeon");
      } else if(phase==="dungeon") {
        if(!fixtureReady(state)) return;
        assert(state.dungeon.active && state.capabilities.dungeonControls,"Dungeon semantic state is missing");
        assert(state.minimap?.kind===2,"Dungeon minimap is not declared as a floor map");
        if(document.querySelector("#dungeonActions").hidden) return;
        if(!dungeonDpadMatches(state.dungeon.overhead)) return;
        if(mobile) {
          const visibleSearch=[...document.querySelectorAll("button")].filter(button=>button.getClientRects().length&&button.textContent.trim()==="Search");
          assert(visibleSearch.length===1,"Dungeon phone HUD contains duplicate Search controls");
          assert(document.querySelector("#mobileAction3").textContent.trim()==="Torch","Dungeon Torch did not enter the mobile action deck");
        }
        const map=client.explorationMap();
        assert(map?.kind===2 && map.width===8 && map.height===8,"Dungeon exploration floor is unavailable");
        const oldView=state.dungeon.overhead;
        document.querySelector(mobile ? "#mobileAction6" : "#dungeonView").click();
        before={...state,dungeon:{...state.dungeon,overhead:oldView}}; next("dungeon-view");
      } else if(phase==="dungeon-view") {
        if(state.dungeon.overhead===before.dungeon.overhead) return;
        if(!dungeonDpadMatches(state.dungeon.overhead)) return;
        if(state.dungeon.overhead) {
          before=state;
          document.querySelector('.dpad [data-move="east"]').click();
          next("dungeon-cardinal-east");return;
        }
        document.querySelector(mobile ? "#minimapButton" : '#dungeonActions [data-open-map]').click();
        assert(document.querySelector("#mapDialog").open,"Dungeon floor map did not open from the action bar");
        document.querySelector("#mapClose").click();
        pass("Dungeon view toggle and explored floor map",{overhead:state.dungeon.overhead});
        prepare(32,"combat");
      } else if(phase==="dungeon-cardinal-east") {
        if(state.moves===before.moves) return;
        assert(state.dungeon.facing==="East","East overhead input did not update cardinal facing");
        assert(state.location.x===before.location.x+1 && state.location.y===before.location.y,"East overhead input did not move one cell east");
        document.querySelector(mobile ? "#minimapButton" : '#dungeonActions [data-open-map]').click();
        assert(document.querySelector("#mapDialog").open,"Dungeon floor map did not open from the action bar");
        document.querySelector("#mapClose").click();
        pass("Dungeon north-up cardinal movement and explored floor map",{from:[before.location.x,before.location.y],to:[state.location.x,state.location.y],facing:state.dungeon.facing});
        prepare(32,"combat");
      } else if(phase==="combat") {
        if(!fixtureReady(state)) return;
        assert(state.combat.active && state.capabilities.combatTargeting,"Combat semantic state is missing");
        if(document.querySelector("#combatActions").hidden) return;
        // The encounter can begin on a creature turn. Wait until a party
        // member owns the turn and a classic directional target is legal.
        if(!state.party.some(member=>member.active) || !state.combat.targets.length) return;
        if(mobile) {
          const repeat=document.querySelector("#mobileDpadCenter");
          assert(!repeat.hidden && repeat.textContent.trim()==="Repeat attack","Combat Repeat attack did not enter the D-pad center");
        }
        const nextTarget=document.querySelector(mobile ? "#mobileAction5" : '#combatActions [data-gameplay-action="16"][data-parameter="1"]');
        nextTarget.click(); next("combat-selected");
      } else if(phase==="combat-selected") {
        if(!state.combat.selected) return;
        if(document.querySelector("#combatAttack").disabled) return;
        assert(document.querySelectorAll(".combat-target").length===state.combat.targets.length,"Target overlays do not match engine targets");
        document.querySelector(mobile ? "#mobileAction6" : "#combatClear").click(); next("combat-cleared");
      } else if(phase==="combat-cleared") {
        if(state.combat.selected) return;
        pass("Combat target list, selection overlay and clear action",{targets:state.combat.targets.length});
        clearInterval(timer); write("SUITE PASS");
      }
    } catch(error) { clearInterval(timer); errors.push(error.message || String(error)); write("FAIL"); }
  },80);
  write("LOADING");
})();
