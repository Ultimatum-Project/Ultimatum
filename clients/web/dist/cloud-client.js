(function(global) {
  'use strict';
  class CloudClient {
    constructor({config=global.UltimatumCloudConfig,storage,sdk=global.UltimatumSupabase}={}) {
      if(!config?.url || !config?.key)throw Error('Accounts are not configured in this build.');
      this.client=sdk.createClient(config.url,config.key,{global:{fetch:async(input,init)=>{
        const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),15000);
        const callerSignal=init?.signal,onAbort=()=>controller.abort();
        if(callerSignal?.aborted)controller.abort();else callerSignal?.addEventListener('abort',onAbort,{once:true});
        try{return await global.fetch(input,{...init,signal:controller.signal});}
        finally{clearTimeout(timer);callerSignal?.removeEventListener('abort',onAbort);}
      }},auth:{storageKey:'ultimatum-account-'+new URL(config.url).hostname,...(storage?{storage}:{}),
        persistSession:true,autoRefreshToken:true,detectSessionInUrl:false}});
    }
    async session(){const {data,error}=await this.client.auth.getSession();if(error)throw error;return data.session;}
    async requireAccount(){const session=await this.session();if(!session||session.user.is_anonymous)throw Error('Sign in to an Ultimatum account first.');return session.user.id;}
    async sendCode(email){const {error}=await this.client.auth.signInWithOtp({email,options:{shouldCreateUser:true}});if(error)throw error;}
    async verifyCode(email,token){const {data,error}=await this.client.auth.verifyOtp({email,token,type:'email'});if(error)throw error;return data.session;}
    async signOut(){const {error}=await this.client.auth.signOut({scope:'local'});if(error)throw error;}
    async event(name,metadata={}){
      const session=await this.session();if(!session||session.user.is_anonymous)return false;
      const eventId=global.crypto.randomUUID();
      const {error}=await this.client.rpc('record_product_event',{p_event_id:eventId,p_name:name,p_metadata:metadata});
      if(error)throw error;return true;
    }
    async submitFeedback({id,installationId,kind,message,email=null,context={}}){
      const {data,error}=await this.client.rpc('submit_feedback',{p_id:id,p_installation_id:installationId,p_kind:kind,p_message:message,p_reply_email:email||null,p_context:context});
      if(error){
        if(error.message?.includes('feedback_rate_limit'))throw Error('Thanks for sending several reports. Please wait an hour before sending another.');
        throw error;
      }
      return data;
    }
    async resources(){
      const user=await this.requireAccount();
      const {data,error}=await this.client.from('account_resources').select('id,kind,game,label,current_version,legacy_slot,metadata,created_at,updated_at,current:account_resource_versions!account_resources_current_version_fkey(id,label,created_at,sha256,content_fingerprint,logical_bytes,stored_bytes,device_name,metadata)').eq('user_id',user).order('updated_at',{ascending:false});
      if(error)throw error;return data;
    }
    async summary(){await this.requireAccount();const {data,error}=await this.client.rpc('account_storage_summary');if(error)throw error;return data;}
    async history(resource,offset=0){
      const user=await this.requireAccount();
      const {data,error}=await this.client.from('account_resource_versions').select('id,resource_id,label,created_at,sha256,content_fingerprint,logical_bytes,stored_bytes,device_name,metadata').eq('user_id',user).eq('resource_id',resource).order('created_at',{ascending:false}).order('id',{ascending:false}).range(offset,offset+19);
      if(error)throw error;return data;
    }
    async upload(resource,expected,text,deviceName){
      await this.requireAccount();global.UltimatumAdventureStore.decode(text);
      const {data,error}=await this.client.rpc('publish_account_adventure',{p_resource:resource||null,p_expected:expected||null,p_package:text,p_device:deviceName||null});
      if(error){
        if(error.message?.includes('cloud_conflict'))throw Error('This adventure changed on another device. Review both versions before choosing one.');
        if(error.message?.includes('storage_quota'))throw Error('Your 100 MB storage is full. Open Storage to remove older checkpoints or game data.');
        throw error;
      }
      return data;
    }
    async uploadGameData(resource,expected,text,deviceName){
      await this.requireAccount();await global.UltimatumGameData.decodePackage(text);
      const {data,error}=await this.client.rpc('publish_account_game_data',{p_resource:resource||null,p_expected:expected||null,p_package:text,p_device:deviceName||null});
      if(error){
        if(error.message?.includes('cloud_conflict'))throw Error('Game data changed on another device. Refresh before replacing it.');
        if(error.message?.includes('storage_quota'))throw Error('Your 100 MB storage is full. Open Storage to remove older checkpoints or game data.');
        throw error;
      }
      return data;
    }
    async downloadPackage(id){
      const user=await this.requireAccount();
      const {data,error}=await this.client.from('account_resource_versions').select('package,sha256').eq('user_id',user).eq('id',id).single();
      if(error)throw error;
      const bytes=new TextEncoder().encode(data.package);
      const hash=[...new Uint8Array(await global.crypto.subtle.digest('SHA-256',bytes))].map(n=>n.toString(16).padStart(2,'0')).join('');
      if(hash!==data.sha256)throw Error('The cloud checkpoint failed its integrity check. Your local save is unchanged.');
      return data.package;
    }
    async download(id){
      const text=await this.downloadPackage(id);global.UltimatumAdventureStore.decode(text);return text;
    }
    async downloadGameData(id){
      const text=await this.downloadPackage(id);await global.UltimatumGameData.decodePackage(text);return text;
    }
    async deleteVersion(id){await this.requireAccount();const {data,error}=await this.client.rpc('delete_account_version',{p_version:id});if(error)throw error;return data;}
    async deleteResource(id,expected){await this.requireAccount();const {data,error}=await this.client.rpc('delete_account_resource',{p_resource:id,p_expected:expected});if(error){if(error.message?.includes('cloud_conflict'))throw Error('This adventure changed on another device. Refresh before removing it.');throw error;}return data;}
  }
  global.UltimatumCloudClient=CloudClient;
})(window);
