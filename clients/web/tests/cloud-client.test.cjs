const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const {webcrypto}=require('node:crypto');
const context={Uint8Array,TextEncoder,crypto:webcrypto,btoa,atob,URL,AbortController,setTimeout,clearTimeout};context.window=context;
context.UltimatumGameData={decodePackage:async text=>{const value=JSON.parse(text);if(value.format!=="ultimatum-game-data")throw Error("Unsupported cloud game-data package.");return value;}};
vm.runInNewContext(fs.readFileSync(require.resolve('../dist/adventure-store.js'),'utf8'),context);
vm.runInNewContext(fs.readFileSync(require.resolve('../dist/cloud-client.js'),'utf8'),context);
const Store=context.UltimatumAdventureStore,Cloud=context.UltimatumCloudClient;
const text=Store.encode({files:{'party.sav':new Uint8Array([1]),'monsters.sav':new Uint8Array([2]),'map-pins.dat':new Uint8Array([3]),'journal-notebook.dat':new Uint8Array([4])}},'Fixture');
function fixture(){
 let user={id:'owner',email:'fixture@example.invalid'},row=null,rpcError=null;const calls=[];
 const query={select(value){calls.push(['select',value]);return this},eq(...args){calls.push(args);return this},order(...args){calls.push(['order',...args]);return this},range(...args){calls.push(['range',...args]);return this},single:async()=>({data:row,error:null})};
 const client={auth:{getSession:async()=>({data:{session:user?{user}:null},error:null}),signInWithOtp:async args=>{calls.push(args);return {error:null}},verifyOtp:async args=>{calls.push(args);return {data:{session:{user}},error:null}},signOut:async args=>{calls.push(args);user=null;return {error:null}}},from:()=>query,rpc:async(name,args)=>{calls.push({name,args});return {data:name==='account_storage_summary'?{used_bytes:10,quota_bytes:104857600}:{resource_id:'resource',revision_id:'revision',content_fingerprint:Store.fingerprint(Store.decode(text).files)},error:rpcError}}};
 const sdk={createClient:(url,key,options)=>{calls.push(options);return client}};
 return {cloud:new Cloud({config:{url:'https://test.supabase.co',key:'publishable'},sdk}),calls,setUser:value=>user=value,setRow:value=>row=value,setError:value=>rpcError=value};
}
test('Passwordless sign-in uses verified email codes and project-scoped storage',async()=>{
 const f=fixture();await f.cloud.sendCode('fixture@example.invalid');await f.cloud.verifyCode('fixture@example.invalid','123456');
 assert.equal(f.calls[0].auth.detectSessionInUrl,false);assert.match(f.calls[0].auth.storageKey,/test\.supabase\.co/);
 assert.deepEqual(JSON.parse(JSON.stringify(f.calls[1])),{email:'fixture@example.invalid',options:{shouldCreateUser:true}});
 assert.equal(f.calls[2].type,'email');assert.equal(f.calls[2].token,'123456');
});
test('Adventure publication uses stable resources, reviewed revisions and device metadata',async()=>{
 const f=fixture(),result=await f.cloud.upload('resource','reviewed-revision',text,'iPhone');
 const call=f.calls.find(value=>value.name==='publish_account_adventure');
 assert.equal(result.revision_id,'revision');assert.deepEqual(JSON.parse(JSON.stringify(call.args)),{p_resource:'resource',p_expected:'reviewed-revision',p_package:text,p_device:'iPhone'});
 f.setError({message:'cloud_conflict: stale'});await assert.rejects(()=>f.cloud.upload('resource','stale',text,'Web'),/changed on another device/);
 f.setError({message:'storage_quota: full'});await assert.rejects(()=>f.cloud.upload(null,null,text,'Web'),/100 MB storage is full/);
 await assert.rejects(()=>f.cloud.upload(null,null,'{}','Web'),/Unsupported/);
});
test('Game-data publication uses its private resource endpoint',async()=>{
 const f=fixture(),packageText=JSON.stringify({format:'ultimatum-game-data'});
 await f.cloud.uploadGameData('game-resource','game-revision',packageText,'Web');
 const call=f.calls.find(value=>value.name==='publish_account_game_data');
 assert.deepEqual(JSON.parse(JSON.stringify(call.args)),{p_resource:'game-resource',p_expected:'game-revision',p_package:packageText,p_device:'Web'});
});
test('Downloads verify SHA-256 and scope version reads to the account',async()=>{
 const f=fixture(),sha256=Buffer.from(await webcrypto.subtle.digest('SHA-256',new TextEncoder().encode(text))).toString('hex');
 f.setRow({package:text,sha256});assert.equal(await f.cloud.download('revision'),text);
 assert(f.calls.some(value=>Array.isArray(value)&&value[0]==='user_id'&&value[1]==='owner'));assert(f.calls.some(value=>Array.isArray(value)&&value[0]==='id'&&value[1]==='revision'));
 f.setRow({package:text+' ',sha256});await assert.rejects(()=>f.cloud.download('revision'),/integrity/);
});
test('Storage and deletion operations stay account scoped',async()=>{
 const f=fixture();assert.equal((await f.cloud.summary()).quota_bytes,104857600);
 await f.cloud.deleteVersion('old');await f.cloud.deleteResource('resource','head');
 assert(f.calls.some(value=>value.name==='delete_account_version'&&value.args.p_version==='old'));
 assert(f.calls.some(value=>value.name==='delete_account_resource'&&value.args.p_expected==='head'));
});
test('Play metrics are account scoped and send only reviewed event fields',async()=>{
 const f=fixture();assert.equal(await f.cloud.event('game_session_started',{game:'ultima4',platform:'web',action:'continue'}),true);
 const call=f.calls.find(value=>value.name==='record_product_event');
 assert.match(call.args.p_event_id,/^[0-9a-f-]{36}$/);assert.equal(call.args.p_name,'game_session_started');
 assert.deepEqual(JSON.parse(JSON.stringify(call.args.p_metadata)),{game:'ultima4',platform:'web',action:'continue'});
 f.setUser({id:'guest',is_anonymous:true});const before=f.calls.length;assert.equal(await f.cloud.event('game_session_started'),false);assert.equal(f.calls.length,before);
});
test('Feedback submission works without an account and sends only explicit fields',async()=>{
 const f=fixture();f.setUser(null);
 const input={id:'feed0000-0000-4000-8000-000000000001',installationId:'feed0000-0000-4000-8000-000000000002',kind:'bug',message:'The menu stopped responding after I opened it.',email:'player@example.invalid',context:{page:'/play/',build:'test'}};
 await f.cloud.submitFeedback(input);
 const call=f.calls.find(value=>value.name==='submit_feedback');
 assert.deepEqual(JSON.parse(JSON.stringify(call.args)),{p_id:input.id,p_installation_id:input.installationId,p_kind:'bug',p_message:input.message,p_reply_email:input.email,p_context:input.context});
 f.setError({message:'feedback_rate_limit'});await assert.rejects(()=>f.cloud.submitFeedback(input),/wait an hour/);
});
test('Guest, anonymous and signed-out clients cannot use the library; signout is device-local',async()=>{
 const f=fixture();f.setUser(null);await assert.rejects(()=>f.cloud.upload(null,null,text,'Web'),/Sign in/);
 f.setUser({id:'guest',is_anonymous:true});await assert.rejects(()=>f.cloud.download('x'),/Sign in/);
 f.setUser({id:'owner'});await f.cloud.signOut();assert.equal(f.calls.at(-1).scope,'local');await assert.rejects(()=>f.cloud.download('x'),/Sign in/);
});
