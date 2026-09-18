-- Extensible account library: stable resources, immutable versions and a
-- server-enforced 100 MB private allowance. Existing checkpoint history is
-- copied without changing revision identifiers.
create table public.account_resources (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  kind text not null check (kind in ('adventure','game_data')),
  game text not null check (char_length(game) between 1 and 40),
  label text not null check (char_length(label) between 1 and 80),
  current_version uuid,
  legacy_slot smallint check (legacy_slot between 1 and 3),
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);
create unique index account_resources_owner_legacy_slot
  on public.account_resources(user_id,legacy_slot) where kind='adventure' and legacy_slot is not null;
create index account_resources_owner_updated on public.account_resources(user_id,updated_at desc);

create table public.account_resource_versions (
  id uuid primary key default gen_random_uuid(),
  resource_id uuid not null references public.account_resources(id) on delete cascade,
  user_id uuid not null references auth.users(id) on delete cascade,
  parent_version uuid references public.account_resource_versions(id) on delete set null,
  label text not null check (char_length(label) between 1 and 80),
  package text not null check (octet_length(package) <= 16777216),
  sha256 text not null check (sha256 ~ '^[0-9a-f]{64}$'),
  content_fingerprint text not null,
  logical_bytes bigint not null check (logical_bytes > 0),
  stored_bytes bigint not null check (stored_bytes > 0),
  device_name text check (device_name is null or char_length(device_name) between 1 and 80),
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now()
);
create index account_resource_versions_resource_time
  on public.account_resource_versions(resource_id,created_at desc,id desc);
create index account_resource_versions_owner on public.account_resource_versions(user_id);
alter table public.account_resources add constraint account_resources_current_version_fkey
  foreign key(current_version) references public.account_resource_versions(id) on delete set null;

alter table public.account_resources enable row level security;
alter table public.account_resource_versions enable row level security;
create policy own_account_resources on public.account_resources for select to authenticated
  using ((select auth.uid())=user_id);
create policy own_account_resource_versions on public.account_resource_versions for select to authenticated
  using ((select auth.uid())=user_id);
revoke all on public.account_resources,public.account_resource_versions from anon,authenticated;
grant select on public.account_resources,public.account_resource_versions to authenticated;

-- Preserve the test-era cloud library in the new model. These tables remain as
-- rollback evidence but new clients publish only through account resources.
insert into public.account_resources(user_id,kind,game,label,legacy_slot,created_at,updated_at)
select h.user_id,'adventure','ultima4',coalesce(r.label,'Adventure'),h.slot,
       coalesce(r.created_at,now()),coalesce(r.created_at,now())
from public.cloud_save_heads h
left join public.cloud_save_revisions r on r.id=h.current_revision;

insert into public.account_resource_versions
  (id,resource_id,user_id,parent_version,label,package,sha256,content_fingerprint,
   logical_bytes,stored_bytes,metadata,created_at)
select r.id,a.id,r.user_id,null,r.label,r.package,r.sha256,
       coalesce((select string_agg((e.value->>'name')||':'||(e.value->>'crc32'),'|' order by e.ordinality)
                 from jsonb_array_elements(r.package::jsonb->'files') with ordinality e(value,ordinality)),''),
       coalesce((select sum((e.value->>'size')::bigint)
                 from jsonb_array_elements(r.package::jsonb->'files') e(value)),1),
       octet_length(r.package),jsonb_build_object('migrated_from','cloud_save_revisions'),r.created_at
from public.cloud_save_revisions r
join public.account_resources a on a.user_id=r.user_id and a.kind='adventure' and a.legacy_slot=r.slot;

update public.account_resource_versions v set parent_version=r.parent_revision
from public.cloud_save_revisions r where v.id=r.id;

update public.account_resources a set current_version=h.current_revision
from public.cloud_save_heads h
where a.user_id=h.user_id and a.legacy_slot=h.slot and a.kind='adventure';

create function ultimatum_private.publish_account_adventure(
  p_resource uuid,p_expected uuid,p_package text,p_device text default null)
returns jsonb language plpgsql security definer set search_path='' as $$
declare
  owner_id uuid:=auth.uid();
  resource_row public.account_resources;
  head_id uuid;
  revision_id uuid;
  payload jsonb;
  title text;
  total_size bigint:=0;
  stored_size bigint;
  used_size bigint;
  entry jsonb;
  names text[]:=array[]::text[];
  content_id text;
  allowed constant text[]:=array['party.sav','monsters.sav','outmonst.sav','dngmap.sav','topics.txt',
    'journal-notebook.dat','explored-map.dat','map-pins.dat','map-discoveries.dat','explored-dungeons.dat','conversations.json'];
  quota constant bigint:=104857600;
begin
  if owner_id is null or coalesce((auth.jwt()->>'is_anonymous')::boolean,false) then
    raise exception 'Sign in to an Ultimatum account first.' using errcode='42501';
  end if;
  if p_package is null or octet_length(p_package)>16777216 or
     p_device is not null and (char_length(p_device)<1 or char_length(p_device)>80) then
    raise exception 'Invalid cloud save.' using errcode='22023';
  end if;
  payload:=p_package::jsonb;
  if payload->>'format' is distinct from 'ultimatum-adventure' or payload->>'version' is distinct from '1'
     or payload->>'game' is distinct from 'ultima4' or payload->>'engine' is distinct from 'xu4'
     or jsonb_typeof(payload->'files') is distinct from 'array' then
    raise exception 'Unsupported adventure package.' using errcode='22023';
  end if;
  for entry in select value from jsonb_array_elements(payload->'files') loop
    if entry->>'name' is null or entry->>'name'=any(names) or not (entry->>'name'=any(allowed))
       or jsonb_typeof(entry->'size') is distinct from 'number' or (entry->>'size')::bigint<=0
       or jsonb_typeof(entry->'data') is distinct from 'string'
       or coalesce(entry->>'crc32','') !~ '^[0-9a-f]{8}$' then
      raise exception 'Invalid adventure files.' using errcode='22023';
    end if;
    total_size:=total_size+(entry->>'size')::bigint;
    names:=array_append(names,entry->>'name');
  end loop;
  if total_size>8388608 or not ('party.sav'=any(names) and 'monsters.sav'=any(names)) then
    raise exception 'Incomplete adventure package.' using errcode='22023';
  end if;
  select string_agg((e.value->>'name')||':'||(e.value->>'crc32'),'|' order by array_position(allowed,e.value->>'name'))
    into content_id from jsonb_array_elements(payload->'files') e(value);
  title:=left(coalesce(nullif(payload->>'label',''),'Adventure'),80);
  stored_size:=octet_length(p_package);
  perform pg_catalog.pg_advisory_xact_lock(pg_catalog.hashtext(owner_id::text));
  if p_resource is null then
    insert into public.account_resources(user_id,kind,game,label)
      values(owner_id,'adventure','ultima4',title) returning * into resource_row;
  else
    select * into resource_row from public.account_resources
      where id=p_resource and user_id=owner_id and kind='adventure' for update;
    if not found then raise exception 'Adventure was not found.' using errcode='42501'; end if;
  end if;
  head_id:=resource_row.current_version;
  if head_id is distinct from p_expected then
    raise exception 'cloud_conflict: Another device changed this adventure.' using errcode='P0001';
  end if;
  select id into revision_id from public.account_resource_versions
    where id=head_id and package=p_package;
  if revision_id is null then
    select coalesce(sum(stored_bytes),0) into used_size from public.account_resource_versions where user_id=owner_id;
    if used_size+stored_size>quota then
      raise exception 'storage_quota: This account needs % more bytes.' ,used_size+stored_size-quota using errcode='P0001';
    end if;
    insert into public.account_resource_versions
      (resource_id,user_id,parent_version,label,package,sha256,content_fingerprint,logical_bytes,stored_bytes,device_name,
       metadata)
      values(resource_row.id,owner_id,head_id,title,p_package,
       encode(extensions.digest(convert_to(p_package,'UTF8'),'sha256'),'hex'),content_id,total_size,stored_size,p_device,
       jsonb_build_object('saved_at',payload->'savedAt')) returning id into revision_id;
    update public.account_resources set current_version=revision_id,label=title,updated_at=now()
      where id=resource_row.id;
    used_size:=used_size+stored_size;
  else
    select coalesce(sum(stored_bytes),0) into used_size from public.account_resource_versions where user_id=owner_id;
  end if;
  return jsonb_build_object('resource_id',resource_row.id,'revision_id',revision_id,
    'used_bytes',used_size,'quota_bytes',quota,'stored_bytes',stored_size,'content_fingerprint',content_id);
end $$;

revoke all on function ultimatum_private.publish_account_adventure(uuid,uuid,text,text) from public,anon,authenticated;
grant usage on schema ultimatum_private to authenticated;
grant execute on function ultimatum_private.publish_account_adventure(uuid,uuid,text,text) to authenticated;
create function public.publish_account_adventure(p_resource uuid,p_expected uuid,p_package text,p_device text default null)
returns jsonb language sql security invoker set search_path='' as $$
  select ultimatum_private.publish_account_adventure(p_resource,p_expected,p_package,p_device);
$$;
revoke all on function public.publish_account_adventure(uuid,uuid,text,text) from public,anon;
grant execute on function public.publish_account_adventure(uuid,uuid,text,text) to authenticated;

create function public.account_storage_summary()
returns jsonb language sql stable security invoker set search_path='' as $$
  select jsonb_build_object(
    'used_bytes',coalesce(sum(v.stored_bytes),0),
    'quota_bytes',104857600,
    'adventures_bytes',coalesce(sum(v.stored_bytes) filter(where r.kind='adventure'),0),
    'game_data_bytes',coalesce(sum(v.stored_bytes) filter(where r.kind='game_data'),0),
    'history_bytes',coalesce(sum(v.stored_bytes) filter(where v.id is distinct from r.current_version),0))
  from public.account_resources r left join public.account_resource_versions v on v.resource_id=r.id
  where r.user_id=auth.uid();
$$;
revoke all on function public.account_storage_summary() from public,anon;
grant execute on function public.account_storage_summary() to authenticated;

create function ultimatum_private.delete_account_version(p_version uuid)
returns bigint language plpgsql security definer set search_path='' as $$
declare owner_id uuid:=auth.uid(); freed bigint;
begin
  if owner_id is null then raise exception 'Sign in first.' using errcode='42501'; end if;
  perform pg_catalog.pg_advisory_xact_lock(pg_catalog.hashtext(owner_id::text));
  select v.stored_bytes into freed from public.account_resource_versions v
    join public.account_resources r on r.id=v.resource_id
    where v.id=p_version and v.user_id=owner_id and r.current_version is distinct from v.id;
  if freed is null then raise exception 'Only an earlier checkpoint can be deleted.' using errcode='22023'; end if;
  delete from public.account_resource_versions where id=p_version and user_id=owner_id;
  return freed;
end $$;
revoke all on function ultimatum_private.delete_account_version(uuid) from public,anon,authenticated;
grant execute on function ultimatum_private.delete_account_version(uuid) to authenticated;
create function public.delete_account_version(p_version uuid)
returns bigint language sql security invoker set search_path='' as $$
  select ultimatum_private.delete_account_version(p_version);
$$;
revoke all on function public.delete_account_version(uuid) from public,anon;
grant execute on function public.delete_account_version(uuid) to authenticated;

create function ultimatum_private.delete_account_resource(p_resource uuid,p_expected uuid)
returns bigint language plpgsql security definer set search_path='' as $$
declare owner_id uuid:=auth.uid(); freed bigint; head_id uuid;
begin
  if owner_id is null then raise exception 'Sign in first.' using errcode='42501'; end if;
  perform pg_catalog.pg_advisory_xact_lock(pg_catalog.hashtext(owner_id::text));
  select current_version into head_id from public.account_resources
    where id=p_resource and user_id=owner_id for update;
  if not found then raise exception 'Adventure was not found.' using errcode='42501'; end if;
  if head_id is distinct from p_expected then
    raise exception 'cloud_conflict: Another device changed this adventure.' using errcode='P0001';
  end if;
  select coalesce(sum(stored_bytes),0) into freed from public.account_resource_versions where resource_id=p_resource;
  update public.account_resources set current_version=null where id=p_resource;
  delete from public.account_resources where id=p_resource and user_id=owner_id;
  return freed;
end $$;
revoke all on function ultimatum_private.delete_account_resource(uuid,uuid) from public,anon,authenticated;
grant execute on function ultimatum_private.delete_account_resource(uuid,uuid) to authenticated;
create function public.delete_account_resource(p_resource uuid,p_expected uuid)
returns bigint language sql security invoker set search_path='' as $$
  select ultimatum_private.delete_account_resource(p_resource,p_expected);
$$;
revoke all on function public.delete_account_resource(uuid,uuid) from public,anon;
grant execute on function public.delete_account_resource(uuid,uuid) to authenticated;
