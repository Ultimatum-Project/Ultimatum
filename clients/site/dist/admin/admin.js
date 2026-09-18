(function(global){
  'use strict';
  const config=global.UltimatumCloudConfig;
  const client=global.UltimatumSupabase.createClient(config.url,config.key,{auth:{storageKey:'ultimatum-account-'+new URL(config.url).hostname,persistSession:true,autoRefreshToken:true,detectSessionInUrl:false}});
  const ui=Object.fromEntries(['auth','authForm','email','code','codeRow','submitAuth','changeEmail','authStatus','dashboard','identity','refresh','signOut','updated','accountMetrics','playMetrics','trend','games','feedbackCount','feedback','recent','dashboardStatus'].map(id=>[id,document.getElementById(id)]));
  let pendingEmail=null,busy=false;
  const number=value=>Number(value||0).toLocaleString();
  const bytes=value=>{value=Number(value||0);if(value<1024)return number(value)+' B';if(value<1048576)return (value/1024).toFixed(1)+' KB';if(value<1073741824)return (value/1048576).toFixed(1)+' MB';return (value/1073741824).toFixed(1)+' GB';};
  const date=value=>value?new Date(value).toLocaleDateString(undefined,{month:'short',day:'numeric',year:'numeric'}):'Never';
  function setBusy(value){busy=value;document.querySelectorAll('button,input').forEach(element=>element.disabled=value);}
  function status(message,target=ui.authStatus){target.textContent=message||'';}
  async function run(action,target=ui.authStatus){if(busy)return;setBusy(true);status('',target);try{await action();}catch(error){status(error.message||'The dashboard is temporarily unavailable.',target);}finally{setBusy(false);}}
  function metric(value,label,detail=''){const card=document.createElement('article');card.className='metric';const strong=document.createElement('strong'),span=document.createElement('span'),small=document.createElement('small');strong.textContent=value;span.textContent=label;small.textContent=detail;card.append(strong,span,small);return card;}
  function fillMetrics(container,items){container.replaceChildren(...items.map(item=>metric(...item)));}
  function renderFeedback(data){
    ui.feedbackCount.textContent=number(data.new_count)+' new · '+number(data.total)+' total';
    if(!data.reports.length){const empty=document.createElement('p');empty.className='feedback-empty';empty.textContent='No feedback has arrived yet.';ui.feedback.replaceChildren(empty);return;}
    ui.feedback.replaceChildren(...data.reports.map(report=>{
      const article=document.createElement('article');article.className='feedback-report';
      const header=document.createElement('header'),kind=document.createElement('span'),when=document.createElement('span');kind.className='report-kind';kind.textContent=report.kind==='bug'?'Bug report':'Feedback';when.className='report-date';when.textContent=new Date(report.created_at).toLocaleString();header.append(kind,when);
      const message=document.createElement('p');message.className='report-message';message.textContent=report.message;
      const footer=document.createElement('footer');
      if(report.reply_email){const reply=document.createElement('a');reply.href='mailto:'+report.reply_email;reply.textContent='Reply to '+report.reply_email;footer.append(reply);}
      if(report.account_email){const account=document.createElement('span');account.textContent='Account: '+report.account_email;footer.append(account);}
      const source=document.createElement('span');source.textContent=report.account_email?'Signed in':'Not signed in';footer.append(source);
      article.append(header,message,footer);
      if(report.context&&Object.keys(report.context).length){const details=document.createElement('details'),summary=document.createElement('summary'),copy=document.createElement('p');summary.textContent='Device details';copy.textContent=Object.entries(report.context).map(([key,value])=>key.replaceAll('_',' ')+': '+value).join(' · ');details.append(summary,copy);article.append(details);}
      return article;
    }));
  }
  function render(data,feedback){
    const o=data.overview,total=Math.max(1,Number(o.total_accounts)),conversion=Math.round(Number(o.accounts_with_adventures)/total*100);
    ui.updated.textContent='Updated '+new Date(data.generated_at).toLocaleString();
    fillMetrics(ui.accountMetrics,[[number(o.total_accounts),'Total accounts','All passwordless email accounts'],[number(o.new_accounts_7d),'New this week',number(o.new_accounts_30d)+' in the last 30 days'],[number(o.active_accounts_7d),'Active this week',number(o.active_accounts_30d)+' active in the last 30 days'],[conversion+'%','Protected a saved game',number(o.accounts_with_adventures)+' accounts with cloud saves']]);
    fillMetrics(ui.playMetrics,[[number(o.play_sessions_7d),'Play sessions this week',number(o.play_sessions_30d)+' in the last 30 days'],[number(o.adventures_total),'Cloud saves',number(o.game_data_resources)+' private game-data packages'],[number(o.checkpoint_uploads_7d),'Uploads this week',number(o.checkpoint_versions)+' saved versions overall'],[bytes(o.storage_bytes),'Storage used','Across all account libraries']]);
    const max=Math.max(1,...data.trend.flatMap(row=>[row.accounts,row.sessions,row.uploads].map(Number)));
    ui.trend.replaceChildren(...data.trend.map(row=>{const day=document.createElement('div');day.className='day';day.dataset.label=new Date(row.date+'T00:00:00').toLocaleDateString(undefined,{month:'numeric',day:'numeric'});day.title=`${day.dataset.label}: ${row.accounts} accounts, ${row.sessions} sessions, ${row.uploads} uploads`;for(const [kind,value] of [['accounts',row.accounts],['sessions',row.sessions],['uploads',row.uploads]]){const bar=document.createElement('i');bar.className='bar '+kind;bar.style.height=(Number(value)/max*100)+'%';bar.setAttribute('aria-hidden','true');day.append(bar);}return day;}));
    const gameNames={ultima4:'Ultima IV · Quest of the Avatar'};
    ui.games.replaceChildren(...(data.games.length?data.games:[{game:'ultima4',adventures:0,players:0,sessions_30d:0}]).map(game=>{const card=document.createElement('article');card.className='game';const h=document.createElement('h3');h.textContent=gameNames[game.game]||game.game;const dl=document.createElement('dl');for(const [label,value] of [['Players with cloud saves',number(game.players)],['Cloud saves',number(game.adventures)],['Signed-in sessions · 30 days',number(game.sessions_30d)]]){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=label;dd.textContent=value;dl.append(dt,dd);}card.append(h,dl);return card;}));
    ui.recent.replaceChildren(...data.recent_accounts.map(account=>{const tr=document.createElement('tr');for(const value of [account.email,date(account.created_at),date(account.last_sign_in_at),number(account.resources),bytes(account.storage_bytes)]){const td=document.createElement('td');td.textContent=value;tr.append(td);}return tr;}));
    renderFeedback(feedback);
  }
  async function load(){
    const {data:{session},error:sessionError}=await client.auth.getSession();if(sessionError)throw sessionError;
    if(!session){showSignedOut();return;}
    ui.identity.textContent=session.user.email||'Administrator';
    const [dashboardResult,feedbackResult]=await Promise.all([client.rpc('admin_dashboard'),client.rpc('admin_feedback_reports')]);
    const error=dashboardResult.error||feedbackResult.error;if(error)throw new Error(error.code==='42501'?'This account is not authorized to view the admin dashboard.':error.message);
    render(dashboardResult.data,feedbackResult.data);ui.auth.hidden=true;ui.dashboard.hidden=false;document.querySelectorAll('[data-signed-in]').forEach(element=>element.hidden=false);status('',ui.dashboardStatus);
  }
  function showSignedOut(){ui.auth.hidden=false;ui.dashboard.hidden=true;document.querySelectorAll('[data-signed-in]').forEach(element=>element.hidden=true);pendingEmail=null;ui.codeRow.hidden=true;ui.changeEmail.hidden=true;ui.email.readOnly=false;ui.code.value='';ui.submitAuth.textContent='Send sign-in code';}
  ui.authForm.addEventListener('submit',event=>{event.preventDefault();run(async()=>{
    if(!pendingEmail){const email=ui.email.value.trim();const {error}=await client.auth.signInWithOtp({email,options:{shouldCreateUser:true}});if(error)throw error;pendingEmail=email;ui.email.readOnly=true;ui.codeRow.hidden=false;ui.changeEmail.hidden=false;ui.submitAuth.textContent='Verify code';status('Check your email for the sign-in code.');ui.code.focus();return;}
    const token=ui.code.value.trim();if(!/^[0-9]{6,10}$/.test(token))throw Error('Enter the sign-in code from your email.');const {error}=await client.auth.verifyOtp({email:pendingEmail,token,type:'email'});if(error)throw error;await load();
  });});
  ui.changeEmail.addEventListener('click',showSignedOut);
  ui.refresh.addEventListener('click',()=>run(load,ui.dashboardStatus));
  ui.signOut.addEventListener('click',()=>run(async()=>{const {error}=await client.auth.signOut({scope:'local'});if(error)throw error;showSignedOut();status('Signed out.');},ui.dashboardStatus));
  client.auth.onAuthStateChange(event=>{if(event==='SIGNED_OUT')showSignedOut();});
  run(load);
})(window);
