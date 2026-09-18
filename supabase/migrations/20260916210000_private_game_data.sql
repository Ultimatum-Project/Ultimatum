-- A single private, versioned Ultima IV game-data package per account. The
-- server verifies package shape, decoded byte sizes and every declared digest;
-- clients additionally match the files to a reviewed compatibility profile.
create unique index account_resources_owner_game_data
  on public.account_resources(user_id,game) where kind='game_data';

create function ultimatum_private.publish_account_game_data(
  p_resource uuid,p_expected uuid,p_package text,p_device text default null)
returns jsonb language plpgsql security definer set search_path='' as $$
declare
  owner_id uuid:=auth.uid();
  resource_row public.account_resources;
  head_id uuid;
  revision_id uuid;
  payload jsonb;
  title text;
  profile_id text;
  total_size bigint:=0;
  stored_size bigint;
  used_size bigint;
  entry jsonb;
  bytes bytea;
  names text[]:=array[]::text[];
  content_id text;
  quota constant bigint:=104857600;
begin
  if owner_id is null or coalesce((auth.jwt()->>'is_anonymous')::boolean,false) then
    raise exception 'Sign in to an Ultimatum account first.' using errcode='42501';
  end if;
  if p_package is null or octet_length(p_package)>16777216 or
     p_device is not null and (char_length(p_device)<1 or char_length(p_device)>80) then
    raise exception 'Invalid cloud game data.' using errcode='22023';
  end if;
  payload:=p_package::jsonb;
  profile_id:=payload->>'profile';
  if payload->>'format' is distinct from 'ultimatum-game-data' or payload->>'version' is distinct from '1'
     or payload->>'game' is distinct from 'ultima4' or profile_id is null or char_length(profile_id) not between 1 and 80
     or jsonb_typeof(payload->'files') is distinct from 'array' or jsonb_array_length(payload->'files')<>103 then
    raise exception 'Unsupported game-data package.' using errcode='22023';
  end if;
  for entry in select value from jsonb_array_elements(payload->'files') loop
    if coalesce(entry->>'name','') !~ '^[A-Z0-9][A-Z0-9._-]{0,63}$' or entry->>'name'=any(names)
       or jsonb_typeof(entry->'size') is distinct from 'number' or (entry->>'size')::bigint<=0 or (entry->>'size')::bigint>2097152
       or jsonb_typeof(entry->'data') is distinct from 'string' or coalesce(entry->>'sha256','') !~ '^[0-9a-f]{64}$' then
      raise exception 'Invalid game-data files.' using errcode='22023';
    end if;
    begin bytes:=decode(entry->>'data','base64'); exception when others then
      raise exception 'Invalid game-data encoding.' using errcode='22023';
    end;
    if octet_length(bytes)<>(entry->>'size')::bigint or
       encode(extensions.digest(bytes,'sha256'),'hex')<>entry->>'sha256' then
      raise exception 'Game-data integrity check failed.' using errcode='22023';
    end if;
    total_size:=total_size+octet_length(bytes);
    names:=array_append(names,entry->>'name');
  end loop;
  if total_size>33554432 or not ('AVATAR.EXE'=any(names) and 'TITLE.EXE'=any(names) and 'CHARSET.EGA'=any(names)) then
    raise exception 'Incomplete game-data package.' using errcode='22023';
  end if;
  select string_agg((e.value->>'name')||':'||(e.value->>'sha256'),'|' order by e.value->>'name')
    into content_id from jsonb_array_elements(payload->'files') e(value);
  title:=left(coalesce(nullif(payload->>'label',''),'Ultima IV game data'),80);
  stored_size:=octet_length(p_package);
  perform pg_catalog.pg_advisory_xact_lock(pg_catalog.hashtext(owner_id::text));
  if p_resource is null then
    select * into resource_row from public.account_resources
      where user_id=owner_id and kind='game_data' and game='ultima4' for update;
    if not found then
      insert into public.account_resources(user_id,kind,game,label,metadata)
        values(owner_id,'game_data','ultima4',title,jsonb_build_object('profile',profile_id)) returning * into resource_row;
    end if;
  else
    select * into resource_row from public.account_resources
      where id=p_resource and user_id=owner_id and kind='game_data' and game='ultima4' for update;
    if not found then raise exception 'Game data was not found.' using errcode='42501'; end if;
  end if;
  head_id:=resource_row.current_version;
  if head_id is distinct from p_expected then
    raise exception 'cloud_conflict: Another device changed this game data.' using errcode='P0001';
  end if;
  select id into revision_id from public.account_resource_versions where id=head_id and package=p_package;
  select coalesce(sum(stored_bytes),0) into used_size from public.account_resource_versions where user_id=owner_id;
  if revision_id is null then
    if used_size+stored_size>quota then
      raise exception 'storage_quota: This account needs % more bytes.',used_size+stored_size-quota using errcode='P0001';
    end if;
    insert into public.account_resource_versions
      (resource_id,user_id,parent_version,label,package,sha256,content_fingerprint,logical_bytes,stored_bytes,device_name,metadata)
      values(resource_row.id,owner_id,head_id,title,p_package,
       encode(extensions.digest(convert_to(p_package,'UTF8'),'sha256'),'hex'),content_id,total_size,stored_size,p_device,
       jsonb_build_object('profile',profile_id)) returning id into revision_id;
    update public.account_resources set current_version=revision_id,label=title,updated_at=now(),metadata=jsonb_build_object('profile',profile_id)
      where id=resource_row.id;
    used_size:=used_size+stored_size;
  end if;
  return jsonb_build_object('resource_id',resource_row.id,'revision_id',revision_id,
    'used_bytes',used_size,'quota_bytes',quota,'stored_bytes',stored_size,'logical_bytes',total_size,
    'content_fingerprint',content_id,'profile',profile_id);
end $$;

revoke all on function ultimatum_private.publish_account_game_data(uuid,uuid,text,text) from public,anon,authenticated;
grant execute on function ultimatum_private.publish_account_game_data(uuid,uuid,text,text) to authenticated;
create function public.publish_account_game_data(p_resource uuid,p_expected uuid,p_package text,p_device text default null)
returns jsonb language sql security invoker set search_path='' as $$
  select ultimatum_private.publish_account_game_data(p_resource,p_expected,p_package,p_device);
$$;
revoke all on function public.publish_account_game_data(uuid,uuid,text,text) from public,anon;
grant execute on function public.publish_account_game_data(uuid,uuid,text,text) to authenticated;
