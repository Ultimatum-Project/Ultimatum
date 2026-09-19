// Test-only presentation checks and shipped controls against real WASM turns.
(function () {
  const runtimeModule = () => document.ultimatumRuntimeModule || window.UltimatumRuntimeModule || window.Module;
  const client = new window.UltimatumEngineClient(runtimeModule);
  const report = document.createElement("pre");
  report.id = "runtimeReport";
  report.style.cssText = "position:fixed;top:0;left:0;max-height:65px;max-width:100%;overflow:auto;z-index:90;background:#101820;color:white;font:10px monospace;white-space:pre-wrap";
  document.body.append(report);
  const errors = [], results = [];
  window.addEventListener("error", e => errors.push(e.message));
  window.addEventListener("unhandledrejection", e => errors.push(String(e.reason)));
  let phase = "prepare", generation, before, geometry, drawerTrigger, mobile = false, since = Date.now();
  const rect = selector => {
    const r = document.querySelector(selector).getBoundingClientRect();
    return {x:r.x,y:r.y,width:r.width,height:r.height};
  };
  const visible = selector => [...document.querySelectorAll(selector)].find(element => element.getClientRects().length);
  const assert = (ok, message) => { if (!ok) throw Error(message); };
  const write = status => report.textContent = JSON.stringify({status,phase,viewport:[innerWidth,innerHeight],errors,results},null,2);
  const transition = next => { phase = next; since = Date.now(); write("RUNNING"); };
  const controlsReady = s => {
    const wait=visible('[data-key="32"]'), controls=visible('.mobile-control-deck, .command-bar');
    return s.inputMode === "command" && !s.interactionActive && wait && !wait.disabled && controls && !controls.inert;
  };
  const sameGeometry = (a,b) => Math.abs(a.height-b.height)<.5 && Math.abs(a.y-b.y)<.5;
  const pass = (name, details={}) => results.push({name,status:"PASS",...details});
  const timer = setInterval(() => {
    try {
      if (!runtimeModule()?.ccall || !document.querySelector("#engineStatus").hidden) return;
      assert(!errors.length, errors.join("; "));
      assert(Date.now()-since < 15000,`Timed out in ${phase} (module=${Boolean(runtimeModule()?.ccall)}, engineHidden=${document.querySelector("#engineStatus").hidden})`);
      const s = client.snapshot();
      if (phase === "prepare") {
        generation = client.call("zu4_web_test_generation","number");
        if (client.call("zu4_web_test_prepare","number",["number"],[36])) transition("fixture");
      } else if (phase === "fixture") {
        if (client.call("zu4_web_test_generation","number") === generation || !controlsReady(s) || Date.now()-since<250) return;
        mobile = matchMedia("(max-width:650px), (max-width:980px) and (max-height:500px)").matches;
        const dock = document.querySelector(".conversation-dock"), message = document.querySelector("#latestMessage");
        geometry = rect("#worldStage");
        const strip = rect(".conversation-dock");
        if (mobile) {
          assert(strip.height===0,"Normal feedback still consumes a separate mobile row");
          assert(rect(".topbar").height===0,"Portal header still consumes phone play space");
          if (innerHeight>500) assert(rect("#worldCanvas").width>=innerWidth*.86,"Game view does not dominate the portrait phone width");
          else assert(rect("#worldCanvas").height>=innerHeight*.62,"Game view does not dominate the landscape phone height");
          assert(document.querySelector("#mobileWorldLog").children.length<=3,"In-world log exceeds three lines");
        }
        const original = message.textContent;
        for (const text of ["Pass", "A much longer message about Britannia, its many towns, companions, and adventures. ".repeat(6)]) {
          message.textContent = text;
          assert(sameGeometry(geometry,rect("#worldStage")),"Message text resized the world");
          assert(rect(".conversation-dock").height===strip.height,"Message surface resized");
        }
        message.textContent = original;
        pass("Short and long messages preserve world geometry",{stripHeight:strip.height,world:geometry});
        if (mobile && innerHeight>500) {
          const actions=[...document.querySelectorAll(".mobile-action-grid button")];
          assert(actions.length===8,"Mobile action deck is not the native-style 2×4 grid");
          assert(actions.every(button=>button.getBoundingClientRect().width>=44&&button.getBoundingClientRect().height>=44),"Mobile action target below 44px");
          assert(actions.filter(button=>button.textContent.trim()==="Search").length<=1,"Mobile deck shows duplicate Search actions");
          pass("Native-style action deck and in-world log preserve play space",{world:geometry,canvas:rect("#worldCanvas"),logLines:document.querySelector("#mobileWorldLog").children.length});
        }
        for (const button of [...document.querySelectorAll(mobile ? ".mobile-control-deck button" : ".command-bar button")].filter(button=>button.getClientRects().length)) {
          const r=button.getBoundingClientRect();
          // The mobile shell is viewport-bound. Desktop intentionally scrolls;
          // still reject horizontal overflow into its adjacent party panel.
          const bar=rect(mobile ? ".mobile-control-deck" : ".command-bar");
          assert(r.right<=bar.x+bar.width+.5 && r.left>=bar.x-.5,"A command overflows its action area");
          if(mobile) assert(r.bottom<=innerHeight,"A mobile command is outside the viewport");
          if(mobile) assert(r.width>=44 && r.height>=44,"Touch target below 44px");
        }
        pass(mobile ? "Commands fit viewport with comfortable touch targets" : "Desktop commands fit their scrollable action area");
        before=s;
        visible('[data-key="32"]').click();transition("wait");
      } else if (phase === "wait" || phase === "search" || phase === "wait-again") {
        if (!controlsReady(s) || s.moves!==before.moves+1 || Date.now()-since<350) return;
        assert(s.location.x===before.location.x && s.location.y===before.location.y,"Stationary command moved the party");
        assert(sameGeometry(geometry,rect("#worldStage")),"Command resized the world");
        pass(phase === "search" ? "Search passes exactly one real engine turn" : "Wait passes exactly one real engine turn",{movesBefore:before.moves,movesAfter:s.moves});
        if(phase==="wait") {before=s;(mobile ? visible('[data-key="32"]') : visible('[data-key="115"]')).click();transition(mobile ? "wait-again" : "search");}
        else if(mobile) {
          before=s;drawerTrigger=visible('[data-open-panel="partyPanel"]');assert(drawerTrigger,"Mobile party drawer trigger is unavailable");drawerTrigger.click();transition("drawer");
        } else {
          before=s;
          [document.querySelector("#menuButton"),document.querySelector("#mobileMenuButton")].find(b=>b.getClientRects().length&&!b.disabled).click();transition("menu");
        }
      } else if(phase==="drawer") {
        if(!document.querySelector("#sidePanel").classList.contains("mobile-open")||Date.now()-since<150)return;
        assert(!document.querySelector("#sheetBackdrop").hidden&&drawerTrigger.getAttribute("aria-expanded")==="true","Shared mobile drawer did not expose its active state");
        assert(document.activeElement===document.querySelector("#mobileSheetClose"),"Mobile drawer did not focus its close control");
        document.querySelector("#mobileSheetClose").click();transition("drawer-close");
      } else if(phase==="drawer-close") {
        if(document.querySelector("#sidePanel").classList.contains("mobile-open")||Date.now()-since<150)return;
        assert(document.querySelector("#sheetBackdrop").hidden&&drawerTrigger.getAttribute("aria-expanded")==="false","Shared mobile drawer did not clear its active state");
        assert(document.activeElement===drawerTrigger,"Mobile drawer did not return focus to its invoking control");
        pass("Shared mobile drawer restores focus and main controls");
        [document.querySelector("#menuButton"),document.querySelector("#mobileMenuButton")].find(b=>b.getClientRects().length&&!b.disabled).click();transition("menu");
      } else if (phase === "menu") {
        if(s.prompt.title!=="Menu" || Date.now()-since<250) return;
        assert(s.moves===before.moves && sameGeometry(geometry,rect("#worldStage")),"Menu shifted the world or consumed a turn");
        assert(document.querySelector("#promptLabel").getClientRects().length,"Menu heading was hidden");
        assert(visible('.mobile-control-deck, .command-bar').inert,"Background controls remain active during Menu");
        pass("Menu preserves world geometry and semantic heading");
        const resume=[...document.querySelectorAll("[data-prompt-value]")].find(b=>b.dataset.promptValue==="\u001b"&&!b.disabled);
        assert(resume,"Resume is unavailable");resume.click();transition("resume");
      } else if(phase==="resume") {
        if(!controlsReady(s) || Date.now()-since<250) return;
        assert(s.moves===before.moves && sameGeometry(geometry,rect("#worldStage")),"Resume shifted world or consumed turn");
        pass("Resume restores main controls");clearInterval(timer);write("SUITE PASS");
      }
    } catch(e) {clearInterval(timer);errors.push(String(e));write("FAIL");}
  },50);
  write("LOADING");
})();
