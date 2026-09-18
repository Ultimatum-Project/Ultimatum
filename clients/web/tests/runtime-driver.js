// Served only by serve-runtime.py. Exercise the actual UI and real engine,
// recording observable assertions in the page instead of mocking snapshots.
(function () {
  const params = new URL(location.href).searchParams;
  const suite = params.has("suite");
  const cases = suite ? [3,1,2,4,5,6,7,8,9,10,12,13,14,15]
    .map(scenario => ({scenario, cancel:false})).concat([
      {scenario:1, mode:"sell"}, {scenario:2, mode:"sell"},
      {scenario:4, mode:"ale"},
      {scenario:3, cancel:true}, {scenario:5, cancel:true},
      {scenario:10, cancel:true}, {scenario:14, cancel:true},
      {scenario:11, cancel:false}, {scenario:16, cancel:false}
    ]) : [{scenario:Number(params.get("fixture") || 3),
      cancel:params.has("cancel"), mode:params.get("mode") || ""}];
  let scenario = cases[0].scenario, cancel = cases[0].cancel, caseIndex = 0;
  let mode = cases[0].mode || "";
  const results = [];
  const report = document.createElement("pre");
  report.id = "runtimeReport";
  report.style.cssText = "position:fixed;top:0;left:0;max-width:100%;max-height:180px;overflow:auto;z-index:90;background:#101820;color:#fff;padding:8px;font:12px monospace;white-space:pre-wrap;";
  document.body.append(report);
  const history = [];
  let started = false, before, previousId = null, finished = false, actions = 0, npcTopics = 0, duplicates = 0, stale = 0, invalidAmounts = 0, startedAt;
  const client = new window.UltimatumEngineClient(() => window.Module);
  const totals = s => ({gold:s.gold,food:s.food,moves:s.moves,members:s.party.length,hp:s.party.map(p=>p.hp),weapons:s.inventory.weapons.reduce((n,i)=>n+i.count,0),armor:s.inventory.armor.reduce((n,i)=>n+i.count,0)});
  const write = (status, extra = {}) => { report.textContent = JSON.stringify({status,scenario,cancel,mode,actions,history,results,...extra},null,2); };
  function finish(state) {
    finished = true;
    const after = totals(state);
    const assertions = { returnedToCommands:state.inputMode === "command", mainControlsEnabled:!document.querySelector(".command[data-key]").disabled, sheetClosed:!document.querySelector(".semantic-prompt") };
    assertions.duplicateAnswersRejected = duplicates === actions;
    assertions.staleAnswersRejected = stale === Math.max(0, actions-1);
    if (cancel) assertions.cancelDidNotPay = after.gold === before.gold;
    else if (scenario === 1) assertions.changedWeaponInventory = mode === "sell" ? after.weapons < before.weapons && after.gold > before.gold : after.weapons > before.weapons && after.gold < before.gold;
    else if (scenario === 2) assertions.changedArmorInventory = mode === "sell" ? after.armor < before.armor && after.gold > before.gold : after.armor > before.armor && after.gold < before.gold;
    else if (scenario === 3) assertions.boughtFood = after.food > before.food && after.gold < before.gold;
    else if (scenario === 6) assertions.healedLeader = state.party[0].hp === state.party[0].maxHp && after.gold < before.gold;
    else if (scenario === 10) assertions.donatedExactlyTen = after.gold === before.gold - 10;
    else if (scenario === 11) assertions.recruitedCompanion = after.members === before.members + 1;
    else if (scenario === 12) assertions.healedParty = state.party.every(p=>p.hp === p.maxHp);
    else if (scenario === 15) assertions.answeredNpcQuestion = history.some(p=>p.kind === "confirmation" && p.answer === "yes");
    else if (scenario === 16) assertions.healedSelectedCompanion = state.party.length >= 2 && state.party[1].hp === state.party[1].maxHp && state.party[0].hp === before.hp[0] && after.gold < before.gold;
    else if (scenario !== 13 && scenario !== 14) assertions.paidForService = after.gold < before.gold;
    if (scenario === 14) assertions.leftShrine = state.location.context !== "Shrine";
    if (mode === "ale") assertions.askedTavernTopic = history.some(p=>p.kind === "text" && p.answer === "rune");
    const status = Object.values(assertions).every(Boolean) ? "PASS" : "FAIL";
    results.push({scenario,cancel,mode,status,before,after,assertions,history:[...history]});
    write(status, {before,after,assertions,results});
    if (suite && ++caseIndex < cases.length) {
      scenario=cases[caseIndex].scenario; cancel=cases[caseIndex].cancel;
      mode=cases[caseIndex].mode || "";
      started=false; finished=false; before=null; previousId=null; actions=0; npcTopics=0; duplicates=0; stale=0; invalidAmounts=0; history.length=0;
    } else if (suite) {
      report.textContent=JSON.stringify({status:results.every(r=>r.status==="PASS") ? "SUITE PASS" : "SUITE FAIL",results},null,2);
    }
  }
  function choose(state) {
    const prompt = state.prompt;
    if (!prompt.id || prompt.submitted || prompt.id === previousId) return;
    if (document.querySelector(".conversation-dock").dataset.promptId !== String(prompt.id)) return;
    if (previousId !== null) {
      if (client.submitAnswer("y", previousId)) throw Error("A stale answer was accepted");
      stale++;
    }
    previousId = prompt.id;
    if (!before) { before = totals(state); before.gold=1000; before.food=100; }
    const options = prompt.options;
    let value;
    if (options.length === 1 && options[0].label === "Continue") value = options[0].value;
    else if (mode === "sell" && /what else/i.test(prompt.context)) value = "\u001b";
    else if (scenario === 10) value = prompt.kind === "amount" ? (cancel ? "\u001b" : "10") : npcTopics++ === 0 ? "give" : "bye";
    else if (scenario === 11) value = "join";
    else if (scenario === 12) value = prompt.kind === "confirmation" ? "n" : npcTopics++ === 0 ? "health" : "bye";
    else if (scenario === 13) value = npcTopics++ === 0 ? "Compassion" : "bye";
    else if (scenario === 15) value = prompt.kind === "confirmation" ? "yes" : npcTopics++ === 0 ? client.call("zu4_web_test_question_keyword", "string") : "bye";
    else if (scenario === 14) {
      if (prompt.title === "Choose a virtue") value = "Compassion";
      else if (prompt.title === "Meditation") value = "1";
      else if (prompt.title === "Speak thy mantra") value = cancel ? "\u001b" : "mu";
      else value = "\r";
    } else if (prompt.kind === "amount") value = cancel ? "\u001b" : mode === "ale" ? "3" : scenario === 3 ? "2" : "1";
    else if (scenario === 4 && mode === "ale" && prompt.kind === "text") value = "rune";
    else if (prompt.kind === "player") value = scenario === 16 ? "2" : "1";
    else if ([6,16].includes(scenario) && options.some(o=>o.label === "Healing")) value = options.find(o=>o.label === "Healing").value;
    else if (options.some(o=>o.value === "b" && o.label === "Buy")) value = mode === "sell" ? "s" : "b";
    else if (mode === "ale" && options.some(o=>o.label === "Ale")) value = options.find(o=>o.label === "Ale").value;
    else if (options.some(o=>o.value === "y" && o.label === "Yes")) value = /else|more|blood/i.test(prompt.context) ? "n" : "y";
    else value = options.find(o=>o.value !== "\u001b")?.value;
    if (value === undefined) throw Error("No answer selected");
    if (prompt.kind === "amount") {
      for (const bad of ["bad", "-1", "1".repeat(prompt.maxLength+1)])
        if (client.submitAnswer(bad, prompt.id)) throw Error("An invalid amount was accepted");
      invalidAmounts++;
    }
    history.push({id:prompt.id,kind:prompt.kind,title:prompt.title,context:prompt.context,labels:options.map(o=>o.label),answer:value});
    const button = [...document.querySelectorAll("[data-prompt-value]")].find(b=>b.dataset.promptValue === value);
    if (button) button.click();
    else {
      document.querySelector("#conversationInput").value = value;
      document.querySelector("#conversationForm").requestSubmit();
    }
    if (client.submitAnswer(value, prompt.id)) throw Error("A duplicate answer was accepted");
    duplicates++;
    actions++;
    write("RUNNING");
  }
  const timer = setInterval(() => {
    if (finished) { clearInterval(timer); return; }
    try {
      if (!window.Module?.ccall || !document.querySelector("#engineStatus").hidden) return;
      const state = client.snapshot();
      if (!started) {
        before = null;
        startedAt=Date.now();
        started = Boolean(client.call("zu4_web_test_prepare", "number", ["number"], [scenario]));
        write("STARTING");
        return;
      }
      if (actions && state.inputMode === "command") {
        if (document.querySelector(".conversation-dock").dataset.inputMode === "command") finish(state);
        return;
      }
      if (state.inputMode === "busy" && document.querySelector(".conversation-dock").dataset.inputMode === "busy") {
        if ([...document.querySelectorAll(".command, .dpad button")].some(button=>!button.disabled)) throw Error("Main controls became active during an unfinished interaction");
        if (!document.querySelector(".semantic-prompt")) throw Error("Interaction sheet closed during an animation");
        if (client.activatePrimaryAction() || client.tapWorld(0, 1) || client.saveAdventure()) throw Error("A gameplay mutation was accepted during an unfinished interaction");
        if (client.equip(0, 1, 1) !== -2) throw Error("Equipment was not blocked during an unfinished interaction");
      }
      if (!params.has("manual") && Date.now()-startedAt > 30000) throw Error("Timed out: " + state.inputMode + " " + JSON.stringify(state.prompt));
      if (params.has("manual")) { write("MANUAL", {prompt:state.prompt, state:totals(state)}); return; }
      choose(state);
    } catch (error) { finished=true; write("FAIL", {error:error.message}); }
  }, 220);
})();
