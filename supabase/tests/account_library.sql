-- Run only against ultimatum-test. Synthetic identities and writes roll back.
begin;
insert into auth.users(id,aud,role,raw_app_meta_data,raw_user_meta_data) values
 ('eaca0000-0000-4000-8000-000000000011','authenticated','authenticated','{}','{}'),
 ('eaca0000-0000-4000-8000-000000000012','authenticated','authenticated','{}','{}');
select set_config('request.jwt.claims','{"sub":"eaca0000-0000-4000-8000-000000000011","role":"authenticated"}',true);
set local role authenticated;
do $$
declare
 first_result jsonb; second_result jsonb; v_resource uuid; first_id uuid; second_id uuid;
 game_result jsonb; game_resource uuid; game_revision uuid; game_files jsonb:='[]'::jsonb; game_fixture text; i integer;
 fixture text := '{"format":"ultimatum-adventure","version":1,"game":"ultima4","engine":"xu4","label":"Account fixture","savedAt":1,"files":[{"name":"party.sav","size":1,"crc32":"a505df1b","data":"AQ=="},{"name":"monsters.sav","size":1,"crc32":"3c0c8ea1","data":"Ag=="}]}';
begin
 first_result:=public.publish_account_adventure(null,null,fixture,'Test device');
 v_resource:=(first_result->>'resource_id')::uuid;first_id:=(first_result->>'revision_id')::uuid;
 if (first_result->>'quota_bytes')::bigint<>104857600 then raise exception 'wrong quota'; end if;
 if (public.publish_account_adventure(v_resource,first_id,fixture,'Test device')->>'revision_id')<>first_id::text then raise exception 'retry duplicated version';end if;
 second_result:=public.publish_account_adventure(v_resource,first_id,replace(fixture,'Account fixture','Second checkpoint'),'Other device');
 second_id:=(second_result->>'revision_id')::uuid;
 if (select count(*) from public.account_resource_versions where resource_id=v_resource)<>2 then raise exception 'history missing';end if;
 begin
  perform public.publish_account_adventure(v_resource,first_id,fixture,'Stale device');raise exception 'stale upload accepted';
 exception when sqlstate 'P0001' then if sqlerrm not like 'cloud_conflict:%' then raise;end if;end;
 if (public.account_storage_summary()->>'used_bytes')::bigint<=0 then raise exception 'usage missing';end if;
 if public.delete_account_version(first_id)<=0 then raise exception 'history did not free space';end if;
 begin delete from public.account_resources where id=v_resource;raise exception 'direct delete allowed';exception when insufficient_privilege then null;end;
 if public.delete_account_resource(v_resource,second_id)<=0 then raise exception 'resource did not free space';end if;
 for i in 1..100 loop
  game_files:=game_files||jsonb_build_array(jsonb_build_object('name','FILE'||lpad(i::text,3,'0')||'.DAT','size',1,'sha256','4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a','data','AQ=='));
 end loop;
 game_files:=game_files||jsonb_build_array(
  jsonb_build_object('name','AVATAR.EXE','size',1,'sha256','4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a','data','AQ=='),
  jsonb_build_object('name','TITLE.EXE','size',1,'sha256','4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a','data','AQ=='),
  jsonb_build_object('name','CHARSET.EGA','size',1,'sha256','4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a','data','AQ=='));
 game_fixture:=jsonb_build_object('format','ultimatum-game-data','version',1,'game','ultima4','profile','test-profile','label','Private game data','files',game_files)::text;
 game_result:=public.publish_account_game_data(null,null,game_fixture,'Test device');
 game_resource:=(game_result->>'resource_id')::uuid;game_revision:=(game_result->>'revision_id')::uuid;
 if (game_result->>'logical_bytes')::bigint<>103 then raise exception 'game data logical bytes incorrect';end if;
 if (public.account_storage_summary()->>'game_data_bytes')::bigint<=0 then raise exception 'game data usage missing';end if;
 if public.delete_account_resource(game_resource,game_revision)<=0 then raise exception 'game data did not free space';end if;
end $$;
reset role;
select set_config('request.jwt.claims','{"sub":"eaca0000-0000-4000-8000-000000000012","role":"authenticated"}',true);
set local role authenticated;
do $$ begin
 if exists(select 1 from public.account_resources) or exists(select 1 from public.account_resource_versions) then raise exception 'cross-account disclosure';end if;
end $$;
reset role;
select 'Account library assertions passed' as result;
rollback;
