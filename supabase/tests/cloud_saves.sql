-- Run ONLY against ultimatum-test. Everything, including synthetic users, rolls back.
begin;
insert into auth.users(id,aud,role,raw_app_meta_data,raw_user_meta_data) values
 ('eaca0000-0000-4000-8000-000000000001','authenticated','authenticated','{}','{}'),
 ('eaca0000-0000-4000-8000-000000000002','authenticated','authenticated','{}','{}');
select set_config('request.jwt.claims','{"sub":"eaca0000-0000-4000-8000-000000000001","role":"authenticated"}',true);
set local role authenticated;
do $$
declare first_id uuid; second_id uuid; fixture text := '{"format":"ultimatum-adventure","version":1,"game":"ultima4","engine":"xu4","label":"Isolated SQL fixture","files":[{"name":"party.sav","size":1,"crc32":"a505df1b","data":"AQ=="},{"name":"monsters.sav","size":1,"crc32":"3c0c8ea1","data":"Ag=="}]}';
begin
 first_id:=public.publish_cloud_save(1,null,fixture);
 if public.publish_cloud_save(1,first_id,fixture) <> first_id then raise exception 'retry duplicated revision'; end if;
 second_id:=public.publish_cloud_save(1,first_id,replace(fixture,'Isolated SQL fixture','Second checkpoint'));
 if (select count(*) from public.cloud_save_revisions)<>2 then raise exception 'history lost'; end if;
 if (select parent_revision from public.cloud_save_revisions where id=second_id)<>first_id then raise exception 'parent lost'; end if;
 begin
  perform public.publish_cloud_save(1,first_id,fixture);raise exception 'stale upload accepted';
 exception when sqlstate 'P0001' then
  if sqlerrm not like 'cloud_conflict:%' then raise; end if;
 end;
 begin update public.cloud_save_heads set current_revision=first_id;raise exception 'direct head write allowed';exception when insufficient_privilege then null;end;
 begin delete from public.cloud_save_revisions;raise exception 'history deletion allowed';exception when insufficient_privilege then null;end;
 begin perform public.publish_cloud_save(4,null,fixture);raise exception 'invalid slot accepted';exception when invalid_parameter_value then null;end;
 begin perform public.publish_cloud_save(2,null,replace(fixture,'party.sav','AVATAR.EXE'));raise exception 'game data accepted';exception when invalid_parameter_value then null;end;
 if (select current_revision from public.cloud_save_heads where slot=1)<>second_id then raise exception 'conflict changed head';end if;
end $$;
reset role;
select set_config('request.jwt.claims','{"sub":"eaca0000-0000-4000-8000-000000000002","role":"authenticated"}',true);
set local role authenticated;
do $$ begin
 if exists(select 1 from public.cloud_save_heads) or exists(select 1 from public.cloud_save_revisions) then raise exception 'cross-account disclosure'; end if;
end $$;
reset role;
select set_config('request.jwt.claims','{"role":"anon"}',true);
set local role anon;
do $$ begin
 begin perform public.publish_cloud_save(1,null,'{}');raise exception 'anonymous publish allowed';exception when insufficient_privilege then null;end;
 begin perform 1 from public.cloud_save_revisions;raise exception 'anonymous read allowed';exception when insufficient_privilege then null;end;
end $$;
reset role;
select 'Cloud SQL assertions passed: CAS, history, immutability, private accounts, anon denial, bounds, allowlist' as result;
rollback;
