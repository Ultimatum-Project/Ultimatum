-- Run only against ultimatum-test. All synthetic identities and events roll back.
begin;
insert into auth.users(id,aud,role,email,raw_app_meta_data,raw_user_meta_data,created_at,last_sign_in_at) values
 ('eaca0000-0000-4000-8000-000000000021','authenticated','authenticated','admin-test@ultimatum.invalid','{}','{}',now(),now()),
 ('eaca0000-0000-4000-8000-000000000022','authenticated','authenticated','player-test@ultimatum.invalid','{}','{}',now(),now());
insert into ultimatum_private.admin_emails(email) values('admin-test@ultimatum.invalid');

select set_config('request.jwt.claims','{"sub":"eaca0000-0000-4000-8000-000000000022","role":"authenticated"}',true);
set local role authenticated;
select public.record_product_event('eaca0000-0000-4000-8000-000000000099','game_session_started','{"game":"ultima4","platform":"test","action":"continue","discarded":"value"}');
select public.record_product_event('eaca0000-0000-4000-8000-000000000099','game_session_started','{"game":"ultima4"}');
do $$ begin
  begin perform public.admin_dashboard();raise exception 'non-admin dashboard access allowed';
  exception when insufficient_privilege then null;end;
end $$;
reset role;

select set_config('request.jwt.claims','{"sub":"eaca0000-0000-4000-8000-000000000021","role":"authenticated"}',true);
set local role authenticated;
do $$
declare dashboard jsonb;
begin
  dashboard:=public.admin_dashboard();
  if (dashboard->'overview'->>'total_accounts')::bigint<2 then raise exception 'accounts missing';end if;
  if (dashboard->'overview'->>'play_sessions_7d')::bigint<1 then raise exception 'sessions missing';end if;
  if jsonb_array_length(dashboard->'trend')<>30 then raise exception 'trend should contain 30 days';end if;
end $$;
reset role;
do $$ begin
  if exists(select 1 from ultimatum_private.product_events where metadata ? 'discarded') then raise exception 'event metadata was not minimized';end if;
  if (select count(*) from ultimatum_private.product_events where user_id='eaca0000-0000-4000-8000-000000000022')<>1 then raise exception 'event retry was duplicated';end if;
end $$;
select 'Admin analytics assertions passed' as result;
rollback;
