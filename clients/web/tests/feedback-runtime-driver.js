(async()=>{
  const report={ok:false,assertions:[]};
  const assert=(condition,message)=>{if(!condition)throw Error(message);report.assertions.push(message);};
  const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  try{
    document.querySelectorAll('dialog[open]').forEach(dialog=>dialog.close());
    let submission=null;
    adventures.cloudUI.cloud.submitFeedback=async value=>{submission=value;return {id:value.id};};
    ui.feedbackButton.click();
    assert(ui.feedbackDialog.open,'Feedback button opens the modal');
    assert(document.activeElement===ui.feedbackMessage,'Message field receives focus');
    document.querySelector('input[name=feedbackKind][value=bug]').checked=true;
    ui.feedbackMessage.value='The test menu stopped responding after opening it.';
    ui.feedbackMessage.dispatchEvent(new Event('input',{bubbles:true}));
    ui.feedbackEmail.value='tester@example.invalid';
    ui.feedbackDiagnostics.checked=true;
    ui.feedbackForm.requestSubmit();
    await wait(0);
    assert(submission?.kind==='bug','Bug report type is submitted');
    assert(submission?.message===ui.feedbackMessage.value||ui.feedbackMessage.value==='', 'Message is submitted and then cleared');
    assert(submission?.email==='tester@example.invalid','Optional reply email is submitted');
    assert(submission?.context?.platform==='web'&&submission.context.user_agent,'Opt-in diagnostics are submitted');
    assert(ui.feedbackStatus.dataset.state==='success','Successful submission is confirmed inline');
    ui.cancelFeedbackButton.click();
    assert(!ui.feedbackDialog.open,'Cancel closes the modal');
    ui.dataDialog.showModal();
    document.querySelector('#dataDialog [data-open-feedback]').click();
    assert(ui.feedbackDialog.open&&!ui.dataDialog.open,'Onboarding can open feedback without game data');
    ui.cancelFeedbackButton.click();
    assert(ui.dataDialog.open,'Closing feedback returns to onboarding');
    report.ok=true;
  }catch(error){report.error=error.stack||error.message;}
  const output=document.createElement('pre');output.id='runtimeReport';output.textContent=JSON.stringify(report);document.body.append(output);
})();
