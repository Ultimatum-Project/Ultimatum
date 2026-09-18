-- Test project only: immutable portable save packages and atomic per-account heads.
create extension if not exists pgcrypto with schema extensions;
create schema if not exists ultimatum_private;
revoke all on schema ultimatum_private from public, anon, authenticated;

create table public.cloud_save_revisions (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  slot smallint not null check (slot between 1 and 3),
  parent_revision uuid references public.cloud_save_revisions(id),
  label text not null check (char_length(label) between 1 and 40),
  package text not null check (octet_length(package) <= 16777216),
  sha256 text not null check (sha256 ~ '^[0-9a-f]{64}$'),
  created_at timestamptz not null default now()
);
create index cloud_save_revisions_owner_slot_time on public.cloud_save_revisions(user_id, slot, created_at desc);
create table public.cloud_save_heads (
  user_id uuid not null references auth.users(id) on delete cascade,
  slot smallint not null check (slot between 1 and 3),
  current_revision uuid references public.cloud_save_revisions(id),
  primary key (user_id, slot)
);
alter table public.cloud_save_revisions enable row level security;
alter table public.cloud_save_heads enable row level security;
create policy own_cloud_revisions on public.cloud_save_revisions for select to authenticated
  using ((select auth.uid()) = user_id);
create policy own_cloud_heads on public.cloud_save_heads for select to authenticated
  using ((select auth.uid()) = user_id);
revoke all on public.cloud_save_revisions, public.cloud_save_heads from anon, authenticated;
grant select on public.cloud_save_revisions, public.cloud_save_heads to authenticated;

-- Only this private function may write heads/revisions. No caller-supplied owner.
-- Definer is necessary to keep direct table writes unavailable to clients.
create function ultimatum_private.publish_cloud_save(p_slot integer, p_expected uuid, p_package text)
returns uuid language plpgsql security definer set search_path = '' as $$
declare
  owner_id uuid := auth.uid();
  head_id uuid;
  revision_id uuid;
  payload jsonb;
  title text;
  total_size bigint := 0;
  entry jsonb;
  names text[] := array[]::text[];
begin
  if owner_id is null or coalesce((auth.jwt()->>'is_anonymous')::boolean, false) then
    raise exception 'Sign in to an Ultimatum account first.' using errcode = '42501';
  end if;
  if p_slot is null or p_slot not between 1 and 3 or p_package is null or octet_length(p_package) > 16777216 then
    raise exception 'Invalid cloud save.' using errcode = '22023';
  end if;
  payload := p_package::jsonb;
  if payload->>'format' is distinct from 'ultimatum-adventure' or payload->>'version' is distinct from '1'
     or payload->>'game' is distinct from 'ultima4' or payload->>'engine' is distinct from 'xu4'
     or jsonb_typeof(payload->'files') is distinct from 'array' then
    raise exception 'Unsupported adventure package.' using errcode = '22023';
  end if;
  for entry in select value from jsonb_array_elements(payload->'files') loop
    if entry->>'name' is null or entry->>'name' = any(names) or not (entry->>'name' = any(array[
       'party.sav','monsters.sav','outmonst.sav','dngmap.sav','topics.txt','journal-notebook.dat',
       'explored-map.dat','map-pins.dat','map-discoveries.dat','explored-dungeons.dat','conversations.json']))
       or jsonb_typeof(entry->'size') is distinct from 'number' or (entry->>'size')::bigint <= 0
       or jsonb_typeof(entry->'data') is distinct from 'string' then
      raise exception 'Invalid adventure files.' using errcode = '22023';
    end if;
    total_size := total_size + (entry->>'size')::bigint;
    names := array_append(names, entry->>'name');
  end loop;
  if total_size > 8388608 or not ('party.sav' = any(names) and 'monsters.sav' = any(names)) then
    raise exception 'Incomplete adventure package.' using errcode = '22023';
  end if;
  title := left(coalesce(nullif(payload->>'label',''), 'Adventure'), 40);
  insert into public.cloud_save_heads(user_id,slot) values(owner_id,p_slot) on conflict do nothing;
  select current_revision into head_id from public.cloud_save_heads
    where user_id = owner_id and slot = p_slot for update;
  if head_id is distinct from p_expected then
    raise exception 'cloud_conflict: Another device uploaded a checkpoint. Refresh and review both saves.' using errcode = 'P0001';
  end if;
  -- Network retries of identical bytes are harmless; other revisions stay immutable.
  select id into revision_id from public.cloud_save_revisions where id = head_id and package = p_package;
  if revision_id is not null then return revision_id; end if;
  insert into public.cloud_save_revisions(user_id,slot,parent_revision,label,package,sha256)
    values(owner_id,p_slot,head_id,title,p_package,encode(extensions.digest(convert_to(p_package,'UTF8'),'sha256'),'hex'))
    returning id into revision_id;
  update public.cloud_save_heads set current_revision = revision_id where user_id = owner_id and slot = p_slot;
  return revision_id;
end;
$$;
revoke all on function ultimatum_private.publish_cloud_save(integer,uuid,text) from public, anon, authenticated;
-- Invoker API wrapper keeps privileged code outside the exposed schema.
grant usage on schema ultimatum_private to authenticated;
grant execute on function ultimatum_private.publish_cloud_save(integer,uuid,text) to authenticated;
create function public.publish_cloud_save(p_slot integer, p_expected uuid, p_package text)
returns uuid language sql security invoker set search_path = '' as $$
  select ultimatum_private.publish_cloud_save(p_slot, p_expected, p_package);
$$;
revoke all on function public.publish_cloud_save(integer,uuid,text) from public, anon;
grant execute on function public.publish_cloud_save(integer,uuid,text) to authenticated;
