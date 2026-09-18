-- Player feedback is accepted through a narrow RPC and stored outside the
-- exposed API schema. Only approved administrators can read it.
create table ultimatum_private.feedback_reports (
  id uuid primary key,
  installation_id uuid not null,
  user_id uuid references auth.users(id) on delete set null,
  kind text not null check (kind in ('feedback','bug')),
  message text not null check (char_length(message) between 20 and 4000),
  reply_email text check (reply_email is null or char_length(reply_email) between 3 and 254),
  context jsonb not null default '{}'::jsonb check (jsonb_typeof(context)='object'),
  status text not null default 'new' check (status in ('new','reviewed','closed')),
  created_at timestamptz not null default now()
);
create index feedback_reports_created_idx on ultimatum_private.feedback_reports(created_at desc);
create index feedback_reports_installation_idx on ultimatum_private.feedback_reports(installation_id,created_at desc);
create index feedback_reports_user_idx on ultimatum_private.feedback_reports(user_id,created_at desc) where user_id is not null;
alter table ultimatum_private.feedback_reports enable row level security;
revoke all on ultimatum_private.feedback_reports from public,anon,authenticated;

create function ultimatum_private.submit_feedback(
  p_id uuid,
  p_installation_id uuid,
  p_kind text,
  p_message text,
  p_reply_email text default null,
  p_context jsonb default '{}'::jsonb
)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  account_id uuid := case when coalesce((auth.jwt()->>'is_anonymous')::boolean,false) then null else auth.uid() end;
  clean_message text := btrim(coalesce(p_message,''));
  clean_email text := nullif(lower(btrim(coalesce(p_reply_email,''))), '');
  clean_context jsonb;
  existing jsonb;
  inserted_at timestamptz;
begin
  if p_id is null or p_installation_id is null then
    raise exception 'A report identifier is required.' using errcode='22023';
  end if;
  select jsonb_build_object('id',id,'created_at',created_at) into existing
    from ultimatum_private.feedback_reports
    where id=p_id and installation_id=p_installation_id;
  if existing is not null then return existing; end if;
  if p_kind not in ('feedback','bug') then
    raise exception 'Choose feedback or bug report.' using errcode='22023';
  end if;
  if char_length(clean_message) not between 20 and 4000 then
    raise exception 'Feedback must be between 20 and 4,000 characters.' using errcode='22023';
  end if;
  if clean_email is not null and (char_length(clean_email)>254 or clean_email !~ '^[^[:space:]@]+@[^[:space:]@]+\.[^[:space:]@]+$') then
    raise exception 'Enter a valid reply email address.' using errcode='22023';
  end if;
  if p_context is null or jsonb_typeof(p_context)<>'object' or octet_length(p_context::text)>4096 then
    raise exception 'Device details are invalid.' using errcode='22023';
  end if;
  clean_context:=jsonb_strip_nulls(jsonb_build_object(
    'page',nullif(left(p_context->>'page',160),''),
    'build',nullif(left(p_context->>'build',80),''),
    'platform',nullif(left(p_context->>'platform',40),''),
    'viewport',nullif(left(p_context->>'viewport',40),''),
    'user_agent',nullif(left(p_context->>'user_agent',512),'')
  ));
  if (select count(*) from ultimatum_private.feedback_reports where installation_id=p_installation_id and created_at>now()-interval '1 hour')>=5 then
    raise exception 'feedback_rate_limit' using errcode='P0001';
  end if;
  if account_id is not null and (select count(*) from ultimatum_private.feedback_reports where user_id=account_id and created_at>now()-interval '1 hour')>=10 then
    raise exception 'feedback_rate_limit' using errcode='P0001';
  end if;
  insert into ultimatum_private.feedback_reports(id,installation_id,user_id,kind,message,reply_email,context)
  values(p_id,p_installation_id,account_id,p_kind,clean_message,clean_email,clean_context)
  returning created_at into inserted_at;
  return jsonb_build_object('id',p_id,'created_at',inserted_at);
end;
$$;
revoke all on function ultimatum_private.submit_feedback(uuid,uuid,text,text,text,jsonb) from public;
grant execute on function ultimatum_private.submit_feedback(uuid,uuid,text,text,text,jsonb) to anon,authenticated;

create function public.submit_feedback(
  p_id uuid,
  p_installation_id uuid,
  p_kind text,
  p_message text,
  p_reply_email text default null,
  p_context jsonb default '{}'::jsonb
)
returns jsonb
language sql
security invoker
set search_path = ''
as $$
  select ultimatum_private.submit_feedback(p_id,p_installation_id,p_kind,p_message,p_reply_email,p_context);
$$;
revoke all on function public.submit_feedback(uuid,uuid,text,text,text,jsonb) from public;
grant execute on function public.submit_feedback(uuid,uuid,text,text,text,jsonb) to anon,authenticated;

create function ultimatum_private.admin_feedback_reports()
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare result jsonb;
begin
  if not exists (
    select 1 from auth.users u
    join ultimatum_private.admin_emails a on a.email=lower(u.email)
    where u.id=auth.uid() and u.email is not null
      and not coalesce((auth.jwt()->>'is_anonymous')::boolean,false)
  ) then
    raise exception 'This account is not an Ultimatum administrator.' using errcode='42501';
  end if;
  select jsonb_build_object(
    'total',(select count(*) from ultimatum_private.feedback_reports),
    'new_count',(select count(*) from ultimatum_private.feedback_reports where status='new'),
    'reports',coalesce((
      select jsonb_agg(to_jsonb(report) order by report.created_at desc)
      from (
        select f.id,f.kind,f.message,f.reply_email,f.context,f.status,f.created_at,lower(u.email) as account_email
        from ultimatum_private.feedback_reports f
        left join auth.users u on u.id=f.user_id
        order by f.created_at desc limit 100
      ) report
    ),'[]'::jsonb)
  ) into result;
  return result;
end;
$$;
revoke all on function ultimatum_private.admin_feedback_reports() from public,anon,authenticated;
grant execute on function ultimatum_private.admin_feedback_reports() to authenticated;

create function public.admin_feedback_reports()
returns jsonb
language sql
security invoker
set search_path = ''
as $$ select ultimatum_private.admin_feedback_reports(); $$;
revoke all on function public.admin_feedback_reports() from public,anon;
grant execute on function public.admin_feedback_reports() to authenticated;
