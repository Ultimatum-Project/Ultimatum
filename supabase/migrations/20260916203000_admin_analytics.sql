-- Private, account-scoped product analytics and an aggregate-only admin API.
-- The public wrappers are callable through PostgREST; all privileged work stays
-- in the unexposed private schema and verifies the caller inside the database.
create table ultimatum_private.admin_emails (
  email text primary key check (email = lower(email))
);
revoke all on ultimatum_private.admin_emails from public, anon, authenticated;
-- Intentionally empty on install. Grant an administrator explicitly after
-- migration; never seed a personal identity in distributable schema history.

create table ultimatum_private.product_events (
  id bigint generated always as identity primary key,
  event_id uuid not null,
  user_id uuid not null references auth.users(id) on delete cascade,
  event_name text not null check (event_name in ('game_session_started')),
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  unique(user_id,event_id)
);
create index product_events_name_time_idx
  on ultimatum_private.product_events(event_name,created_at desc);
create index product_events_user_time_idx
  on ultimatum_private.product_events(user_id,created_at desc);
alter table ultimatum_private.product_events enable row level security;
revoke all on ultimatum_private.product_events from public, anon, authenticated;

create function ultimatum_private.record_product_event(
  p_event_id uuid,
  p_name text,
  p_metadata jsonb
)
returns void
language plpgsql
security definer
set search_path = ''
as $$
declare
  owner_id uuid := auth.uid();
  clean_metadata jsonb;
begin
  if owner_id is null or coalesce((auth.jwt()->>'is_anonymous')::boolean,false) then
    raise exception 'Sign in to an Ultimatum account first.' using errcode='42501';
  end if;
  if p_event_id is null or p_name is distinct from 'game_session_started' then
    raise exception 'Unsupported product event.' using errcode='22023';
  end if;
  if (select count(*) from ultimatum_private.product_events
      where user_id=owner_id and created_at>now()-interval '1 hour') >= 120 then
    return;
  end if;
  clean_metadata:=jsonb_build_object(
    'game',left(coalesce(nullif(p_metadata->>'game',''),'ultima4'),40),
    'platform',left(coalesce(nullif(p_metadata->>'platform',''),'unknown'),20),
    'action',case when p_metadata->>'action' in ('new','continue') then p_metadata->>'action' else 'unknown' end
  );
  insert into ultimatum_private.product_events(event_id,user_id,event_name,metadata)
  values(p_event_id,owner_id,p_name,clean_metadata)
  on conflict(user_id,event_id) do nothing;
end;
$$;
revoke all on function ultimatum_private.record_product_event(uuid,text,jsonb) from public,anon,authenticated;
grant execute on function ultimatum_private.record_product_event(uuid,text,jsonb) to authenticated;

create function public.record_product_event(
  p_event_id uuid,
  p_name text,
  p_metadata jsonb default '{}'::jsonb
)
returns void
language sql
security invoker
set search_path = ''
as $$
  select ultimatum_private.record_product_event(p_event_id,p_name,p_metadata);
$$;
revoke all on function public.record_product_event(uuid,text,jsonb) from public,anon;
grant execute on function public.record_product_event(uuid,text,jsonb) to authenticated;

create function ultimatum_private.admin_dashboard()
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  result jsonb;
begin
  if not exists (
    select 1
    from auth.users u
    join ultimatum_private.admin_emails a on a.email=lower(u.email)
    where u.id=auth.uid() and u.email is not null
      and not coalesce((auth.jwt()->>'is_anonymous')::boolean,false)
  ) then
    raise exception 'This account is not an Ultimatum administrator.' using errcode='42501';
  end if;

  select jsonb_build_object(
    'generated_at',now(),
    'overview',jsonb_build_object(
      'total_accounts',(select count(*) from auth.users where email is not null),
      'new_accounts_7d',(select count(*) from auth.users where email is not null and created_at>=now()-interval '7 days'),
      'new_accounts_30d',(select count(*) from auth.users where email is not null and created_at>=now()-interval '30 days'),
      'active_accounts_7d',(select count(*) from auth.users where email is not null and last_sign_in_at>=now()-interval '7 days'),
      'active_accounts_30d',(select count(*) from auth.users where email is not null and last_sign_in_at>=now()-interval '30 days'),
      'accounts_with_adventures',(select count(distinct user_id) from public.account_resources where kind='adventure'),
      'adventures_total',(select count(*) from public.account_resources where kind='adventure'),
      'game_data_resources',(select count(*) from public.account_resources where kind='game_data'),
      'play_sessions_7d',(select count(*) from ultimatum_private.product_events where event_name='game_session_started' and created_at>=now()-interval '7 days'),
      'play_sessions_30d',(select count(*) from ultimatum_private.product_events where event_name='game_session_started' and created_at>=now()-interval '30 days'),
      'checkpoint_versions',(select count(*) from public.account_resource_versions),
      'checkpoint_uploads_7d',(select count(*) from public.account_resource_versions where created_at>=now()-interval '7 days'),
      'storage_bytes',(select coalesce(sum(stored_bytes),0) from public.account_resource_versions)
    ),
    'trend',(
      select coalesce(jsonb_agg(jsonb_build_object(
        'date',day::date,
        'accounts',(select count(*) from auth.users u where u.email is not null and u.created_at>=day and u.created_at<day+interval '1 day'),
        'sessions',(select count(*) from ultimatum_private.product_events e where e.event_name='game_session_started' and e.created_at>=day and e.created_at<day+interval '1 day'),
        'uploads',(select count(*) from public.account_resource_versions v where v.created_at>=day and v.created_at<day+interval '1 day')
      ) order by day),'[]'::jsonb)
      from generate_series(date_trunc('day',now())-interval '29 days',date_trunc('day',now()),interval '1 day') day
    ),
    'games',(
      select coalesce(jsonb_agg(to_jsonb(g) order by g.game),'[]'::jsonb)
      from (
        select r.game,count(*) filter(where r.kind='adventure') as adventures,
          count(distinct r.user_id) filter(where r.kind='adventure') as players,
          (select count(*) from ultimatum_private.product_events e
           where e.event_name='game_session_started' and e.metadata->>'game'=r.game
             and e.created_at>=now()-interval '30 days') as sessions_30d
        from public.account_resources r group by r.game
      ) g
    ),
    'recent_accounts',(
      select coalesce(jsonb_agg(to_jsonb(a) order by a.created_at desc),'[]'::jsonb)
      from (
        select u.email,u.created_at,u.last_sign_in_at,
          (select count(*) from public.account_resources r where r.user_id=u.id) as resources,
          (select coalesce(sum(v.stored_bytes),0) from public.account_resource_versions v where v.user_id=u.id) as storage_bytes
        from auth.users u where u.email is not null
        order by u.created_at desc limit 20
      ) a
    )
  ) into result;
  return result;
end;
$$;
revoke all on function ultimatum_private.admin_dashboard() from public,anon,authenticated;
grant execute on function ultimatum_private.admin_dashboard() to authenticated;

create function public.admin_dashboard()
returns jsonb
language sql
security invoker
set search_path = ''
as $$
  select ultimatum_private.admin_dashboard();
$$;
revoke all on function public.admin_dashboard() from public,anon;
grant execute on function public.admin_dashboard() to authenticated;
