// Isolated loopback QA: real engine, real prompts, real local save storage.
(async function () {
  const report = document.createElement("pre");
  report.id = "desktopRuntimeReport";
  report.style.cssText = "position:fixed;left:0;top:0;z-index:10000;max-height:120px;overflow:auto;background:#000;color:#9f9;font:12px monospace;pointer-events:none";
  document.body.append(report);
  const results = JSON.parse(sessionStorage.getItem("desktop-results") || "[]");
  const pass = label => {results.push(label);report.textContent = JSON.stringify({status:"RUNNING",results});};
  const assert = (ok, label) => {if (!ok) throw Error(label);};
  const state = () => engine.snapshot();
  const wait = async (predicate, label) => {
    const until = Date.now() + 30000;
    while (Date.now() < until) {if (predicate()) return;await new Promise(resolve => setTimeout(resolve, 70));}
    throw Error("Timed out: " + label);
  };
  const option = value => document.querySelector(`[data-prompt-value="${CSS.escape(value)}"]:not(:disabled)`);
  const geometry = () => {
    const world = canvas.getBoundingClientRect(), dock = conversationDock.getBoundingClientRect();
    assert(dock.left >= world.right && dock.width > 200, "Prompt overlaps the game or leaves the side panel");
    assert(!ui.sidePanel.inert && !ui.topbar?.inert, "Desktop panel is incorrectly modal");
    assert(getComputedStyle(document.querySelector(".map-toolbar")).display === "none", "Duplicate desktop location header is visible");
  };
  const prepare = async scenario => {
    const before = engine.call("zu4_web_test_generation", "number");
    assert(engine.call("zu4_web_test_prepare", "number", ["number"], [scenario]), "Fixture request failed");
    await wait(() => (engine.call("zu4_web_test_generation", "number") > before && state().inputMode === "command") || option("bye"), "fixture setup");
    await wait(() => conversationDock.dataset.inputMode === state().inputMode, "UI catches engine state");
  };
  try {
    assert(desktopLayout.matches, "Run this suite at desktop width");
    if (sessionStorage.getItem("desktop-phase") === "reload") {
      await wait(() => ui.adventureDialog.open && document.querySelector('[data-adventure-action="continue"][data-slot="2"]:not(:disabled)'), "saved title");
      document.querySelector('[data-adventure-action="continue"][data-slot="2"]').click();
      await wait(() => state()?.ready && state().inputMode === "command", "Continue");
      assert(conversationHistory.some(entry => entry.speaker === "You" && entry.text === "Name"), "Conversation reply did not restore with adventure");
      assert(conversationHistory.some(entry => entry.speaker !== "You"), "NPC responses did not restore with adventure");
      pass("Conversation history restores with Continue, including player topics and NPC responses");
      const record = await adventures.store.get(2);
      const exported = window.UltimatumAdventureStore.encode(record.current);
      const decoded = window.UltimatumAdventureStore.decode(exported);
      assert(decoded.files["conversations.json"], "Export omits conversation history");
      pass("Save export/import preserves conversation history");
    } else {
      await wait(() => ui.adventureDialog.open && document.querySelector('[data-adventure-action="new"][data-slot="2"]:not(:disabled)'), "new adventure title");
      document.querySelector('[data-adventure-action="new"][data-slot="2"]').click();
      await wait(() => state()?.prompt?.title === "Name your Avatar" && !ui.conversationForm.hidden, "name prompt");
      geometry();
      assert(canvas.width === 320 && canvas.height === 200, "Intro was downsampled instead of retaining 320x200");
      pass("Name entry is in the side panel; full-resolution 320x200 intro remains unobscured");
      ui.conversationInput.value = "DesktopHero";
      ui.conversationInput.dispatchEvent(new KeyboardEvent("keydown", {key:"w",bubbles:true}));
      assert(state().prompt.title === "Name your Avatar" && ui.conversationInput.value === "DesktopHero", "WASD leaks out of text entry");
      ui.conversationForm.requestSubmit(ui.conversationSubmit);
      await wait(() => option("m"), "character selection");option("m").click();
      let stories = 0, questions = 0, previous = 0;
      await wait(() => {
        if (state()?.ready && state().inputMode === "command") return true;
        const prompt = state()?.prompt;
        if (!prompt?.id || prompt.id === previous || prompt.submitted) return false;
        const value = prompt.options.some(item => item.value === "a") ? "a" : "\r";
        const button = option(value);
        if (!button || Number(button.dataset.promptId) !== prompt.id) return false;
        geometry();
        if (prompt.context.startsWith("The story begins")) stories++;
        if (value === "a") questions++;
        previous = prompt.id;button.click();return false;
      }, "creation story and questions");
      assert(stories === 24 && questions === 7, "Creation did not complete all original story pages and virtue questions");
      pass("All 24 story pages and seven virtue questions work without covering the artwork");
      for (const [letter, arrow] of [["w","ArrowUp"],["a","ArrowLeft"],["s","ArrowDown"],["d","ArrowRight"]]) {
        const outcomes = [];
        for (const key of [letter, arrow]) {
          await prepare(36);
          const before = state().moves;
          canvas.dispatchEvent(new KeyboardEvent("keydown", {key,bubbles:true}));
          await wait(() => state().moves === before + 1 && state().inputMode === "command", key + " movement");
          outcomes.push([state().location.x,state().location.y]);
        }
        assert(JSON.stringify(outcomes[0]) === JSON.stringify(outcomes[1]), letter + " differs from arrow movement");
      }
      pass("WASD and arrow keys each advance exactly one turn to matching positions in all four directions");
      await prepare(36);
      const searchMove = state().moves;
      canvas.dispatchEvent(new KeyboardEvent("keydown", {key:"S",shiftKey:true,bubbles:true}));
      await wait(() => state().moves === searchMove + 1, "Shift S Search");
      assert(state().messages.some(message => /Searching/.test(message)), "Shift S does not preserve classic Search");
      pass("Shift S executes classic Search instead of moving south");
      await prepare(15);geometry();
      const namePrompt = state().prompt.id;
      option("name").click();
      await wait(() => option("name") && state().prompt.id !== namePrompt && conversationDock.dataset.promptId === String(state().prompt.id), "NPC Name response");
      assert(ui.latestMessage.textContent === readableProse(state().prompt.context), "NPC prose retains DOS hard wrapping");
      option("bye").click();
      await wait(() => state().inputMode === "command" && conversationDock.hidden && !document.querySelector('[data-key="32"]').disabled, "Goodbye returns gameplay");
      assert(conversationHistory.some(entry => entry.speaker === "You" && entry.text === "Name"), "Topic missing from history");
      pass("NPC topics, readable responses, Goodbye and restored main controls work in the side panel");
      await prepare(36);
      ui.saveButton.click();
      await wait(() => !ui.saveButton.disabled, "save completed");
      const record = await adventures.store.get(2);
      assert(record.current.files["conversations.json"], "Checkpoint does not contain transcript");
      pass("Local adventure checkpoint contains conversation history");
      sessionStorage.setItem("desktop-results", JSON.stringify(results));
      sessionStorage.setItem("desktop-phase", "reload");
      location.reload();return;
    }
    report.textContent = JSON.stringify({status:"PASS",results},null,2);
  } catch (error) {report.textContent = JSON.stringify({status:"FAIL",error:error.stack,results},null,2);}
})();
