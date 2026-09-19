(function(global) {
  "use strict";
  const icon=name=>name==="bookmark" ? '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M6 3h12v18l-6-4-6 4z"/></svg>' : '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 3h12v6M4 3v18h16V11M12 8v8M8 12h8"/></svg>';
  const prose=text=>String(text || "").trim().split(/\n\s*\n+/).map(p=>p.replace(/\s*\n\s*/g," ")).join("\n\n");
  class JournalUI {
    constructor(engine,hooks) {
      this.engine=engine;this.hooks=hooks;this.mode="all";this.source=null;this.editor=null;this.busy=false;
      this.ui=Object.fromEntries([...document.querySelectorAll('[id^="journal"]')].map(el=>[el.id,el]));
      const ui=this.ui;
      document.querySelectorAll('[data-open-journal]').forEach(button=>button.addEventListener("click",()=>this.open(button)));
      document.querySelectorAll('[data-journal-mode]').forEach(button=>button.addEventListener("click",()=>{
        this.mode=button.dataset.journalMode;this.source=null;this.cancelEdit();this.render();
      }));
      ui.journalSearch.addEventListener("input",()=>{this.source=null;this.render();});
      ui.journalClose.addEventListener("click",()=>this.close());
      ui.journalBack.addEventListener("click",()=>{this.source=null;this.cancelEdit();this.render();});
      ui.journalDialog.addEventListener("cancel",event=>{event.preventDefault();this.editor ? this.cancelEdit() : this.close();});
      ui.journalNewNote.addEventListener("click",()=>this.edit(-1,"0"));
      ui.journalCancelEdit.addEventListener("click",()=>this.cancelEdit());
      ui.journalEditor.addEventListener("submit",event=>{
        event.preventDefault();const text=ui.journalNoteText.value;
        if(!text.trim() || text.includes("\0") || new TextEncoder().encode(text).length>4000) return this.status("Write a non-empty note of at most 4,000 UTF-8 bytes.");
        const {passage,id}=this.editor;
        this.mutate(()=>engine.saveJournalNote(passage,id,text));
      });
      ui.journalDeleteNote.addEventListener("click",()=>{ui.journalDeleteConfirm.hidden=false;ui.journalConfirmDelete.focus();});
      ui.journalCancelDelete.addEventListener("click",()=>{ui.journalDeleteConfirm.hidden=true;ui.journalDeleteNote.focus();});
      ui.journalConfirmDelete.addEventListener("click",()=>this.mutate(()=>engine.deleteJournalNote(this.editor.id)));
    }
    status(text="") { this.ui.journalStatus.textContent=text; }
    open(trigger) {
      if(this.busy || this.ui.journalDialog.open)return;
      if(!this.engine.openJournal())return this.hooks.toast("Journal is available during exploration and conversations, outside combat.");
      this.trigger=trigger;this.data=this.engine.journal();this.source=null;this.editor=null;
      this.ui.journalSearch.value="";this.status();this.render();
      if(this.hooks.overlays)this.hooks.overlays.open("journal",{trigger});else{this.ui.journalDialog.showModal();this.ui.journalClose.focus({preventScroll:true});}
    }
    close() {
      if(this.busy)return;
      if(!this.engine.closeJournal())return this.status("The journal could not return control. Keep this tab open.");
      this.editor=null;if(this.hooks.overlays)this.hooks.overlays.close("journal");else this.ui.journalDialog.close();this.hooks.resume();
      if(!this.hooks.overlays)this.trigger?.focus({preventScroll:true});
    }
    element(tag,text,className="") {
      const element=document.createElement(tag);element.textContent=text;element.className=className;return element;
    }
    button(label,action) {
      const button=this.element("button",label);button.type="button";button.addEventListener("click",action);return button;
    }
    iconButton(name,label,action) {
      const button=this.button("",action);button.innerHTML=icon(name);button.className="journal-icon";
      button.setAttribute("aria-label",label);button.title=label;return button;
    }
    render() {
      const ui=this.ui,data=this.data;
      ui.journalEditor.hidden=true;ui.journalContent.hidden=false;
      ui.journalSearch.closest(".journal-search").hidden=false;
      document.querySelectorAll('[data-journal-mode]').forEach(b=>b.disabled=false);
      ui.journalBack.hidden=this.source===null;ui.journalNewNote.hidden=this.mode!=="notes" || this.source!==null;
      ui.journalNewNote.disabled=!data.writable;
      document.querySelectorAll('[data-journal-mode]').forEach(b=>b.setAttribute("aria-pressed",String(b.dataset.journalMode===this.mode)));
      ui.journalTitle.textContent=this.source || ({all:"Journal",clues:"Current clues",notes:"My notes"}[this.mode]);
      ui.journalContent.replaceChildren();
      if(!data.writable)this.status("Journal metadata could not be read. Existing files are preserved; editing is disabled.");
      const query=ui.journalSearch.value.trim().toLocaleLowerCase();
      const matches=(...fields)=>fields.join(" ").toLocaleLowerCase().includes(query);
      if(this.mode==="notes" && this.source===null) {
        for(const note of data.notes) {
          const passage=data.passages.find(p=>p.index===note.passage);
          if(!matches(note.text,passage?.source || "",passage?.topic || ""))continue;
          const row=this.button(prose(note.text),()=>this.edit(note.passage,note.id));row.dataset.journalNoteId=note.id;
          row.append(this.element("small",passage ? `Attached to ${passage.source}` : "Personal note · No source"));ui.journalContent.append(row);
        }
      } else {
        const passages=data.passages.filter(p=>(this.mode!=="clues" || p.favorite) && (this.source===null || p.source===this.source) &&
          matches(p.source,p.speaker,p.place,p.topic,p.kind,p.text,data.notes.find(n=>n.passage===p.index)?.text || ""));
        if(this.source!==null) passages.forEach(p=>this.passage(p));
        else {
          const groups=new Map();for(const p of passages)groups.set(p.source,[...(groups.get(p.source) || []),p]);
          for(const [source,entries] of groups) {
            const row=this.button(source || "Recorded passage",()=>{this.source=source;this.render();ui.journalBack.focus();});
            row.dataset.journalSource=source;row.append(this.element("small",`${entries.length} recorded passage${entries.length===1 ? "" : "s"} · ${entries[0].kind || "conversation"}`));ui.journalContent.append(row);
          }
        }
      }
      if(!ui.journalContent.childElementCount) {
        const empty=query ? "No matching recorded knowledge or notes." : this.mode==="clues" ? "No current clues. Bookmark a recorded passage to keep it here." : this.mode==="notes" ? "No personal notes. Use New note, or attach a note to a recorded passage." : "Conversations, writings, and shrine visions appear here only after you encounter them.";
        ui.journalContent.append(this.element("p",empty,"journal-empty"));
      }
      if(this.mode==="all" && this.source===null) {
        const archive=this.hooks.legacyHistory().filter(e=>matches(e.text,e.location,e.speaker));
        if(archive.length) {
          const details=document.createElement("details");details.className="journal-legacy";
          details.append(this.element("summary",`Web conversation transcripts (${archive.length})`));
          details.append(this.element("p","Earlier web transcripts remain available here. Bookmarks and attached notes use recorded Journal passages.","panel-note"));
          for(const entry of archive) {
            const article=document.createElement("article");article.append(this.element("small",`${entry.location} · ${entry.speaker} · Move ${entry.moves}`),this.element("p",prose(entry.text)));details.append(article);
          }
          ui.journalContent.append(details);
        }
      }
    }
    passage(p) {
      const article=document.createElement("article");article.className="journal-passage";article.dataset.journalPassage=String(p.index);
      const header=this.element("header","");header.append(this.element("h3",p.topic || p.kind || "Recorded passage"));
      const bookmark=this.iconButton("bookmark",p.favorite ? "Remove bookmark" : "Bookmark passage",()=>this.mutate(()=>this.engine.favoritePassage(p.index)));
      bookmark.dataset.journalBookmark=String(p.index);bookmark.setAttribute("aria-pressed",String(p.favorite));bookmark.disabled=!this.data.writable;
      const note=this.iconButton("note",p.noteId!=="0" ? "Edit attached note" : "Add attached note",()=>this.edit(p.index,p.noteId));
      note.dataset.journalAttach=String(p.index);note.disabled=!this.data.writable;header.append(bookmark,note);
      article.append(header,this.element("small",`${p.place || p.source} · ${p.kind || "conversation"}`),this.element("p",prose(p.text)));
      const attached=this.data.notes.find(n=>n.id===p.noteId);
      if(attached)article.append(this.element("aside",`My note: ${attached.text}`,"journal-attached-note"));
      this.ui.journalContent.append(article);
    }
    edit(passage,id) {
      if(!this.data.writable || this.busy)return;
      this.editor={passage,id};const ui=this.ui;
      ui.journalContent.hidden=true;ui.journalEditor.hidden=false;ui.journalBack.hidden=true;
      ui.journalNewNote.hidden=true;ui.journalSearch.closest(".journal-search").hidden=true;
      document.querySelectorAll('[data-journal-mode]').forEach(b=>b.disabled=true);
      ui.journalTitle.textContent=id==="0" ? "New note" : "Edit note";
      ui.journalNoteText.value=this.data.notes.find(n=>n.id===id)?.text || "";
      ui.journalDeleteNote.hidden=id==="0";ui.journalDeleteConfirm.hidden=true;
      this.status();ui.journalNoteText.focus({preventScroll:true});
    }
    cancelEdit() { if(this.busy)return;this.editor=null;this.status();this.render(); }
    async mutate(action) {
      if(this.busy)return;
      this.busy=true;const fs=this.engine.module.FS;
      const backup=this.engine.readAdventureFiles();this.status("Saving journal…");
      this.ui.journalDialog.querySelectorAll("button,input,textarea").forEach(el=>el.disabled=true);
      try {
        if(!action())throw Error("The journal change could not be saved. Check note limits and try again.");
        await this.hooks.persist();this.data=this.engine.journal();this.editor=null;this.render();this.status("Journal saved with this adventure.");
      } catch(error) {
        for(const name of ["topics.txt","journal-notebook.dat"]) {
          const path="/home/web_user/.xu4/"+name;
          if(backup[name])fs.writeFile(path,backup[name]);else if(this.engine.pathExists(path))fs.unlink(path);
        }
        this.engine.reloadJournal();this.data=this.engine.journal();this.status(error.message+" Previous journal data is unchanged.");
      } finally {
        this.busy=false;this.ui.journalDialog.querySelectorAll("button,input,textarea").forEach(el=>el.disabled=false);
        this.ui.journalNewNote.disabled=!this.data.writable;
        document.querySelectorAll('[data-journal-mode]').forEach(b=>b.disabled=Boolean(this.editor));
        this.ui.journalDialog.querySelectorAll('[data-journal-bookmark],[data-journal-attach]').forEach(el=>el.disabled=!this.data.writable);
      }
    }
  }
  global.UltimatumJournalUI=JournalUI;
})(window);
