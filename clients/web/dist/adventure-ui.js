(function (global) {
  "use strict";
  const Store=global.UltimatumSaveStore;
  const ACTIVE_SESSION="ultimatum-active-session-v1";
  class AdventureUI {
    constructor(engine, hooks) {
      this.engine=engine;this.hooks=hooks;this.store=new Store();
      this.ui=Object.fromEntries([...document.querySelectorAll('[id^="adventure"]')].map(el=>[el.id,el]));
      this.mode="title";this.ready=false;this.busy=false;this.activeSlot=null;
      this.expected=null;this.saveQueue=Promise.resolve();this.records=[];
      this.newGame=false;this.sawCreationPrompt=false;this.saveRevision=0;this.lastSavedMoves=0;
      this.adventureRequest=0;this.confirmAction=null;this.pendingCheckpoint=null;
      this.ui.adventureSlots.addEventListener("click",event=>{
        const button=event.target.closest("[data-adventure-action]");
        if(button && !button.disabled) {
          // File pickers need the original trusted tap, especially on Safari.
          if(button.dataset.adventureAction==="import") {
            this.importSlot=Number(button.dataset.slot);this.ui.adventureImport.click();
          } else this.run(()=>this.action(button.dataset.adventureAction,Number(button.dataset.slot)));
        }
      });
      this.ui.adventureConfirmButton.addEventListener("click",()=>this.run(async()=>{
        const action=this.confirmAction;this.cancelConfirm();if(action) await action();
      }));
      this.ui.adventureCancelButton.addEventListener("click",()=>{this.cancelConfirm();});
      this.ui.adventureName.addEventListener("keydown",event=>{
        if(event.key==="Enter") {event.preventDefault();this.ui.adventureConfirmButton.click();}
      });
      this.ui.adventureResume.addEventListener("click",()=>this.close());
      this.ui.adventureSaveNow.addEventListener("click",()=>this.close("save"));
      this.ui.adventureData.addEventListener("click",()=>hooks.openData());
      this.ui.adventureRetry.addEventListener("click",()=>location.reload());
      this.ui.adventureExportPending.addEventListener("click",()=>{
        if(!this.pendingCheckpoint) return;
        this.download(Store.encode(this.pendingCheckpoint,this.pendingCheckpoint.summary.name),"ultimatum-unstored-checkpoint.u4save");
        this.exportedCheckpoint=this.pendingCheckpoint.fingerprint;
        this.status("Unstored checkpoint exported. You can now return to the title and import this backup as a saved game.");
      });
      this.ui.adventureTitleButton.addEventListener("click",()=>this.confirm("Return to the title screen? Progress since your last checkpoint will not be saved.",()=>this.reload()));
      this.ui.adventureImport.addEventListener("change",()=>this.run(()=>this.importFiles()));
      this.ui.adventureDialog.addEventListener("cancel",event=>{
        event.preventDefault();if(this.confirmAction) this.cancelConfirm();else if(this.mode==="manage") this.close();
      });
      const cloudDialog=document.querySelector('#cloudDialog');
      const cloudConfigured=Boolean(global.UltimatumCloudConfig?.url&&global.UltimatumCloudConfig?.key);
      if(cloudDialog && global.UltimatumCloudUI && cloudConfigured) {
        this.cloudUI=new global.UltimatumCloudUI(document.querySelector('#cloudPanel'),{
          list:()=>this.cloudSlots(),validate:async text=>{await hooks.whenRuntimeReady?.();const value=Store.decode(text);const summary=this.engine.validateAdventure(value.files);return `${summary.name} · ${summary.moves} moves`;},
          install:(slot,text,expected,cloud)=>this.installCloud(slot,text,expected,cloud),
          link:(slot,cloud,expected)=>this.store.setCloudLink(slot,cloud,expected),
          gameData:()=>hooks.gameData?.(),
          installGameData:text=>hooks.installGameData?.(text),
          close:()=>this.closeAccount(),
        });
        this.ui.adventureAccount.addEventListener('click',()=>this.openAccount());
        cloudDialog.addEventListener('cancel',event=>{event.preventDefault();if(this.cloudUI.confirmation)this.cloudUI.cancel();else if(!this.cloudUI.installing)this.closeAccount();});
        document.addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible')this.cloudUI.syncSavedAdventure().catch(error=>console.warn('Cloud sync deferred',error));});
      } else this.ui.adventureAccount.hidden=true;
      this.ui.adventureDialog.showModal();
    }
    openAccount(afterClose=null) {
      const cloudDialog=document.querySelector('#cloudDialog');
      if(!this.cloudUI||!cloudDialog)return false;
      if(afterClose!==null||!cloudDialog.open)this.afterAccountClose=afterClose;
      if(!cloudDialog.open)cloudDialog.showModal();
      this.cloudUI.run(()=>this.cloudUI.refresh());
      return true;
    }
    deferUntilAccountClose(callback) {
      const cloudDialog=document.querySelector('#cloudDialog');
      if(!this.cloudUI||!cloudDialog?.open)return false;
      this.afterAccountClose=callback;
      return true;
    }
    closeAccount() {
      const cloudDialog=document.querySelector('#cloudDialog');
      if(!cloudDialog?.open)return;
      cloudDialog.close();
      const afterClose=this.afterAccountClose;this.afterAccountClose=null;
      this.run(async()=>{await this.show(this.mode);if(afterClose)await afterClose();});
    }
    status(message) {this.ui.adventureStatus.textContent=message;}
    async run(action) {
      if(this.busy) return;
      this.busy=true;this.disable(true);
      try {await action();} catch(error) {this.status(error.message);this.hooks.toast(error.message);console.error(error);}
      finally {this.busy=false;this.disable(false);}
    }
    disable(disabled) {
      this.ui.adventureDialog.querySelectorAll("button").forEach(button=>{
        button.disabled=disabled || button.dataset.unavailable==="true";
      });
    }
    async prepare() {
      const files=(await this.store.wasMigrated()) ? {} : this.engine.readAdventureFiles();
      let summary=null;
      try {summary=this.engine.validateAdventure(files);} catch {
        if(Object.keys(files).length) summary={name:"Browser save — needs recovery",damaged:true};
      }
      const migrated=await this.store.migrate(summary ? files : null,summary);
      this.ready=true;
      const pending=sessionStorage.getItem("ultimatum-start");
      sessionStorage.removeItem("ultimatum-start");
      if(pending) {
        try {
          const choice=JSON.parse(pending);
          if(choice.action==="continue" || choice.action==="new") {
            await this.begin(choice.action,choice.slot);return;
          }
        } catch(error) {
          await this.show("title");this.status(error.message);return;
        }
      }
      const interrupted=sessionStorage.getItem(ACTIVE_SESSION);
      if(interrupted) {
        try {
          const session=JSON.parse(interrupted),record=await this.store.get(session.slot);
          if(record?.current) {
            this.checkpointInfo(record.current);
            await this.begin("continue",session.slot);
            this.hooks.toast("Restored the latest safe checkpoint after the tab reloaded.");
            return;
          }
        } catch(error) { console.warn("Could not resume interrupted adventure",error); }
        sessionStorage.removeItem(ACTIVE_SESSION);
      }
      await this.show("title");
      this.status(migrated ? (summary.damaged ? "Your damaged browser save is preserved. Export it for recovery; it will not be loaded automatically." : "Your existing browser save is preserved.") : "");
      this.cloudUI?.syncSavedAdventure().catch(error=>console.warn("Cloud sync deferred",error));
    }
    async show(mode) {
      this.mode=mode;
      this.cancelConfirm();
      this.ui.adventureTitle.textContent=mode==="title" ? "Quest of the Avatar" : "Saved Games";
      this.ui.adventureIntro.textContent=mode==="title" ? "Continue a saved game or start a new one." : "Each saved game keeps its latest checkpoint and one recovery checkpoint.";
      this.ui.adventureResume.hidden=this.ui.adventureTitleButton.hidden=mode!=="manage";
      this.ui.adventureSaveNow.hidden=mode!=="manage";
      this.ui.adventureSaveHelp.hidden=mode!=="manage" && !this.detailsSlot;
      this.ui.adventureExportPending.hidden=!this.pendingCheckpoint;
      this.ui.adventureSaveNow.dataset.unavailable=String(!this.hooks.state().prompt?.options?.some(option=>option.value==="save"));
      if(!this.ui.adventureDialog.open) this.ui.adventureDialog.showModal();
      this.records=await this.store.list();
      this.ui.adventureSlots.replaceChildren();
      const firstEmpty=[1,2,3].find(slot=>!this.records.some(record=>record.slot===slot));
      for(let slot=1;slot<=3;slot++) {
        const record=this.records.find(record=>record.slot===slot);
        if(!record&&slot!==firstEmpty)continue;
        let valid=false,previousValid=false;
        if(record) {
          try {this.checkpointInfo(record.current);valid=true;} catch {}
          try {this.checkpointInfo(record.previous);previousValid=true;} catch {}
        }
        const card=document.createElement("section");card.className="adventure-slot";
        card.classList.toggle("title-card",mode==="title" && this.detailsSlot!==slot);
        card.classList.toggle("occupied",Boolean(record));
        card.dataset.slot=String(slot);
        const heading=document.createElement("h3");
        heading.textContent=record ? `${record.label || record.current?.summary?.name || "Saved Game"}${this.activeSlot===slot ? " · Playing" : ""}` : "New Game";
        const details=document.createElement("p");
        const info=record?.current?.summary;
        details.textContent=record ? (valid ? `${info?.name || "Avatar"} · ${Number(info?.moves || 0).toLocaleString()} moves · ${info?.members || 1} party member${info?.members===1 ? "" : "s"} · ${info?.location===0 ? "Britannia" : "Dungeon, level " + ((info?.level || 0)+1)} · Saved ${new Date(record.current.savedAt).toLocaleString()}` : "This saved game cannot be loaded. Export it for safekeeping or restore its recovery checkpoint.") : "Begin a new journey.";
        if(record && valid && mode==="title" && this.detailsSlot!==slot)
          details.textContent=`${Number(info?.moves || 0).toLocaleString()} moves · ${info?.location===0 ? "Britannia" : "Dungeon, level " + ((info?.level || 0)+1)}`;
        const buttons=document.createElement("div");buttons.className="adventure-actions";
        const add=(action,label,unavailable=false)=>{
          const button=document.createElement("button");button.type="button";
          button.dataset.adventureAction=action;button.dataset.slot=String(slot);
          button.dataset.unavailable=String(unavailable);button.textContent=label;button.disabled=unavailable || !this.ready;
          buttons.append(button);
        };
        if(record) add("continue",this.activeSlot===slot ? "Load checkpoint" : "Continue",!valid);
        add("new",record ? "Start New Game" : "Start");
        const expanded=mode==="manage" || this.detailsSlot===slot;
        if(record && !expanded) add("manage","Manage Save");
        if(!record || expanded) add("import","Import Save",this.activeSlot===slot);
        if(record && expanded) {add("export","Export Backup");add("rename","Rename");if(record.previous) add("recover","Restore Previous",!previousValid || this.activeSlot===slot);add("delete","Delete Save",this.activeSlot===slot);}
        if(mode==="title" && this.detailsSlot===slot) add("done","Done");
        card.append(heading,details,buttons);this.ui.adventureSlots.append(card);
      }
      if(!this.ui.adventureDialog.open) this.ui.adventureDialog.showModal();
      this.disable(this.busy);
    }
    async cloudSlots() {
      await this.saveQueue;
      const records=await this.store.list();
      return records.map(record=>{
        let text=null;try {this.checkpointInfo(record.current);text=Store.encode(record.current,record.label);}catch {}
        return {slot:record.slot,label:record.label,text,fingerprint:record.current?.fingerprint ?? null,cloud:record.cloud || null,
          active:record.slot===this.activeSlot,summary:record.current?.summary ? `${record.current.summary.moves} moves` : 'Recovery needed'};
      });
    }
    async installCloud(slot,text,expected,cloud) {
      if(slot===this.activeSlot)throw Error('Return to the title before replacing the saved game you are playing.');
      if(this.pendingCheckpoint)throw Error('Export your unstored checkpoint before transferring cloud saves.');
      await this.hooks.whenRuntimeReady?.();
      await this.saveQueue;const value=Store.decode(text),summary=this.engine.validateAdventure(value.files);
      const record=await this.store.commit(slot,value.files,summary,value.label,expected,null);
      if(cloud)await this.store.setCloudLink(slot,cloud,record.current.fingerprint);
      return record;
    }
    checkpointInfo(checkpoint) {
      if(!checkpoint || Store.fingerprint(checkpoint.files)!==checkpoint.fingerprint) throw Error("The stored checkpoint is damaged.");
      return this.engine.validateAdventure(checkpoint.files);
    }
    confirm(message, action, name) {
      this.confirmTrigger=document.activeElement;
      this.confirmAction=action;this.ui.adventureConfirmText.textContent=message;
      this.ui.adventureConfirm.hidden=false;
      this.ui.adventureNameLabel.hidden=name===undefined;
      this.ui.adventureName.value=name ?? "";
      this.ui.adventureSlots.inert=true;this.ui.adventureFooter?.setAttribute("inert","");
      (name===undefined ? this.ui.adventureConfirmButton : this.ui.adventureName).focus({preventScroll:true});
      this.ui.adventureConfirm.scrollIntoView({block:"nearest"});
    }
    cancelConfirm() {
      this.confirmAction=null;this.ui.adventureConfirm.hidden=true;this.ui.adventureSlots.inert=false;this.ui.adventureFooter?.removeAttribute("inert");
      this.confirmTrigger?.focus({preventScroll:true});this.confirmTrigger=null;
    }
    async action(action, slot) {
      if(action==="manage" || action==="done") {
        this.detailsSlot=action==="manage" ? slot : null;this.status("");await this.show("title");return;
      }
      if(action==="export") {
        const record=this.records.find(record=>record.slot===slot);
        const checkpoint=slot===this.activeSlot && this.pendingCheckpoint ? this.pendingCheckpoint : record.current;
        const backupName=(record?.label || checkpoint.summary.name || "saved-game").trim().replace(/[^a-z0-9]+/gi,"-").replace(/^-|-$/g,"").toLowerCase() || "saved-game";
        this.download(Store.encode(checkpoint,record?.label || checkpoint.summary.name),`ultimatum-${backupName}.u4save`);
        if(checkpoint===this.pendingCheckpoint) this.exportedCheckpoint=checkpoint.fingerprint;
        this.status("Saved-game backup exported. Keep it somewhere safe.");return;
      }
      const record=await this.store.get(slot);
      if(action==="new") {
        if(record || this.hooks.isStarted()) return this.confirm(`Begin a new game here? Progress since your last checkpoint will be discarded.${record ? " The current saved game will remain available for recovery until later saves replace it." : ""}`,()=>this.begin("new",slot));
        return this.begin("new",slot);
      }
      if(action==="continue") {
        if(this.hooks.isStarted()) return this.confirm("Load this checkpoint? Any progress since your last save will be discarded.",()=>this.begin("continue",slot));
        return this.begin("continue",slot);
      }
      if(action==="rename") return this.confirm("Rename this saved game.",async()=>{await this.store.rename(slot,this.ui.adventureName.value);await this.show(this.mode);this.status("Saved game renamed.");},record.label);
      if(action==="delete") return this.confirm(`Delete ${record.label || "this saved game"}, including its recovery checkpoint? Export a backup first.`,async()=>{
        if(slot===this.activeSlot) throw Error("Return to the title screen before deleting the saved game you are playing.");
        await this.store.remove(slot,record.current.fingerprint);await this.show(this.mode);this.status("Saved game deleted.");
      });
      if(action==="recover") return this.confirm(`Restore the previous checkpoint for ${record.label || "this saved game"}? The current checkpoint will remain available for recovery.`,async()=>{
        if(slot===this.activeSlot) throw Error("Return to the title screen before restoring the saved game you are playing.");
        const fresh=await this.store.get(slot);this.checkpointInfo(fresh.previous);
        await this.store.restorePrevious(slot,fresh.current.fingerprint,fresh.previous.fingerprint);await this.show(this.mode);this.status("Previous checkpoint restored. Choose Continue when ready.");
      });
    }
    async begin(action, slot) {
      this.store.checkSlot(slot);
      if(!this.ready || !this.engine.hasGameData()) throw Error("Choose your Ultima IV game data first.");
      if(this.pendingCheckpoint && this.exportedCheckpoint!==this.pendingCheckpoint.fingerprint) throw Error("Your last checkpoint was not stored. Export it before reloading.");
      const record=await this.store.get(slot);
      if(action==="continue") this.checkpointInfo(record?.current);
      if(this.hooks.isStarted()) return this.reload({action,slot});
      this.activeSlot=slot;this.expected=record?.current?.fingerprint ?? null;
      this.newGame=action==="new";this.sawCreationPrompt=false;
      this.lastSavedMoves=record?.current?.summary?.moves || 0;
      this.engine.writeAdventureFiles(action==="continue" ? record.current.files : {});
      this.ui.adventureDialog.close();
      if(action==="continue") sessionStorage.setItem(ACTIVE_SESSION,JSON.stringify({slot,startedAt:Date.now()}));
      if(await this.hooks.start(action==="continue")===false) return;
      this.cloudUI?.cloud.event('game_session_started',{game:'ultima4',platform:'web',action}).catch(error=>console.warn('Play session metric deferred',error));
      if(action==="new") this.engine.sendKey(105);
    }
    reload(choice) {
      if(this.pendingCheckpoint && this.exportedCheckpoint!==this.pendingCheckpoint.fingerprint) throw Error("Your last checkpoint was not stored. Export it before reloading.");
      if(choice) sessionStorage.setItem("ultimatum-start",JSON.stringify(choice));
      else sessionStorage.removeItem(ACTIVE_SESSION);
      location.reload();
    }
    close(answer="\u001b") {
      if(this.busy || this.mode!=="manage") return;
      const state=this.hooks.state();
      if(!this.engine.submitAnswer(answer,state.prompt.id)) return this.status("The engine has moved on. Try opening Saved Games again.");
      this.ui.adventureDialog.close();this.hooks.focus();
    }
    async importFiles() {
      const selected=[...this.ui.adventureImport.files];this.ui.adventureImport.value="";
      if(!selected.length) return;
      if(selected.reduce((sum,file)=>sum+file.size,0)>16*1024*1024) throw Error("That backup is too large.");
      let imported;
      if(selected.length===1 && /\.(u4save|json)$/i.test(selected[0].name)) imported=Store.decode(await selected[0].text());
      else {
        const files=Object.create(null);
        for(const file of selected) {
          const name=file.name.toLowerCase();
          if(!Store.FILES.includes(name) || files[name]) throw Error("Choose party.sav, monsters.sav and any associated saved-game files.");
          files[name]=new Uint8Array(await file.arrayBuffer());
        }
        imported={files,label:"Imported save"};
      }
      const summary=this.engine.validateAdventure(imported.files);
      const slot=this.importSlot,record=await this.store.get(slot);
      const apply=async()=>{
        if(slot===this.activeSlot) throw Error("Return to the title screen before replacing the saved game you are playing.");
        await this.store.commit(slot,imported.files,summary,imported.label,record?.current?.fingerprint ?? null,null);
        await this.show(this.mode);this.status("Saved game imported and validated. Choose Continue when ready.");
      };
      if(record) this.confirm(`Replace ${record.label || "this saved game"} with ${summary.name}? The existing checkpoint will be retained for recovery.`,apply);
      else await apply();
    }
    download(text, filename) {
      const url=URL.createObjectURL(new Blob([text],{type:"application/json"}));
      const link=document.createElement("a");link.href=url;link.download=filename;
      document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),60000);
    }
    capture(message="Game saved on this device.",resetCloud=false) {
      // Snapshot bytes synchronously, before awaiting a transaction or another turn.
      const files=this.engine.readAdventureFiles();
      let summary;
      try {summary=this.engine.validateAdventure(files);} catch(error) {this.hooks.toast(error.message);return Promise.reject(error);}
      const checkpoint={files,summary,fingerprint:Store.fingerprint(files),savedAt:Date.now()};
      this.pendingCheckpoint=checkpoint;
      const job=this.saveQueue.catch(()=>{}).then(async()=>{
        const record=await this.store.commit(this.activeSlot,files,summary,undefined,this.expected,resetCloud?null:undefined);
        this.expected=record.current.fingerprint;this.lastSavedMoves=summary.moves;
        sessionStorage.setItem(ACTIVE_SESSION,JSON.stringify({slot:this.activeSlot,securedAt:Date.now()}));
        if(this.pendingCheckpoint===checkpoint) this.pendingCheckpoint=null;
        if(!this.pendingCheckpoint) this.ui.adventureExportPending.hidden=true;
        // The legacy working-copy mirror is not authoritative for slots.
        this.engine.persistSaves().catch(error=>console.warn("Working-copy mirror failed",error));
        if(message) this.hooks.toast(message);return record;
      });
      this.saveQueue=job;
      job.then(record=>this.cloudUI?.syncSavedAdventure(record.slot).catch(error=>console.warn("Cloud sync deferred",error)));
      job.catch(error=>{this.status(error.message);this.ui.adventureExportPending.hidden=false;this.hooks.toast("Save storage failed. Open Saved Games and export your checkpoint before leaving.");console.error(error);});
      return job;
    }
    observe(state) {
      if(this.newGame) {
        if(state.prompt?.id) this.sawCreationPrompt=true;
        if(state.ready) {
          this.newGame=false;this.lastSavedMoves=state.moves;
          this.capture("Your new game is saved on this device.",true).catch(()=>{});
        } else if(this.sawCreationPrompt && state.inputMode==="intro" && !state.interactionActive) {
          this.newGame=false;this.activeSlot=null;this.run(()=>this.show("title"));
        }
      }
      if(state.saveRevision>this.saveRevision) {
        this.saveRevision=state.saveRevision;
        this.capture(state.saveAutomatic ? "" : "Game saved on this device.").catch(()=>{});
      }
      if(state.adventureRequest>this.adventureRequest) {
        this.adventureRequest=state.adventureRequest;
        this.run(()=>this.show("manage"));
      }
      if(this.activeSlot && state.canSave && !this.pendingCheckpoint && state.moves-this.lastSavedMoves>=10) {
        // SDL-owned queued action rechecks legality after it arrives; no Asyncify re-entry.
        if(this.engine.requestCheckpoint()) this.lastSavedMoves=state.moves;
      }
    }
    async persistJournal() {
      if(!this.activeSlot || this.pendingCheckpoint) throw Error("Wait for your saved-game checkpoint to finish before editing the journal.");
      await this.saveQueue;
      const working=this.engine.readAdventureFiles();
      const metadata=Object.fromEntries(["topics.txt","journal-notebook.dat"].filter(name=>working[name]).map(name=>[name,working[name]]));
      const current=await this.store.get(this.activeSlot);
      this.engine.validateAdventure({...current?.current?.files,...metadata});
      const record=await this.store.updateJournal(this.activeSlot,metadata,this.expected);
      this.expected=record.current.fingerprint;
      this.records=this.records.map(old=>old.slot===record.slot ? record : old);
      this.engine.persistSaves().catch(error=>console.warn("Working-copy mirror failed",error));
    }
  }
  global.UltimatumAdventureUI=AdventureUI;
})(window);
