-- Run against the connected Ultimatum project. Synthetic reports roll back.
begin;
insert into auth.users(id,aud,role,email,raw_app_meta_data,raw_user_meta_data,created_at,last_sign_in_at) values
 ('feed0000-0000-4000-8000-000000000001','authenticated','authenticated','feedback-admin@ultimatum.invalid','{}','{}',now(),now()),
 ('feed0000-0000-4000-8000-000000000002','authenticated','authenticated','feedback-player@ultimatum.invalid','{}','{}',now(),now());
insert into ultimatum_private.admin_emails(email) values('feedback-admin@ultimatum.invalid');

select set_config('request.jwt.claims','{"role":"anon"}',true);
set local role anon;
select public.submit_feedback('feed0000-0000-4000-8000-000000000010','feed0000-0000-4000-8000-000000000020','bug','The controls stopped responding after opening the menu.','PLAYER@EXAMPLE.INVALID','{"page":"/play/","build":"test","discarded":"value"}');
select public.submit_feedback('feed0000-0000-4000-8000-000000000010','feed0000-0000-4000-8000-000000000020','bug','The controls stopped responding after opening the menu.','PLAYER@EXAMPLE.INVALID','{}');
do $$ begin
  begin perform * from ultimatum_private.feedback_reports;raise exception 'direct report access allowed';
  exception when insufficient_privilege then null;end;
end $$;
reset role;

do $$ begin
  if (select count(*) from ultimatum_private.feedback_reports where id='feed0000-0000-4000-8000-000000000010')<>1 then raise exception 'report retry was duplicated';end if;
  if (select reply_email from ultimatum_private.feedback_reports where id='feed0000-0000-4000-8000-000000000010')<>'player@example.invalid' then raise exception 'email was not normalized';end if;
  if (select context ? 'discarded' from ultimatum_private.feedback_reports where id='feed0000-0000-4000-8000-000000000010') then raise exception 'unreviewed context was stored';end if;
end $$;

select set_config('request.jwt.claims','{"sub":"feed0000-0000-4000-8000-000000000002","role":"authenticated"}',true);
set local role authenticated;
select public.submit_feedback('feed0000-0000-4000-8000-000000000011','feed0000-0000-4000-8000-000000000021','feedback','I would like a clearer reminder when a cloud save completes.',null,'{}');
do $$ begin
  begin perform public.admin_feedback_reports();raise exception 'non-admin feedback access allowed';
  exception when insufficient_privilege then null;end;
end $$;
reset role;

select set_config('request.jwt.claims','{"sub":"feed0000-0000-4000-8000-000000000001","role":"authenticated"}',true);
set local role authenticated;
do $$ declare inbox jsonb; begin
  inbox:=public.admin_feedback_reports();
  if (inbox->>'total')::bigint<2 then raise exception 'reports missing';end if;
  if jsonb_array_length(inbox->'reports')<2 then raise exception 'report list missing';end if;
  if not exists(select 1 from jsonb_array_elements(inbox->'reports') r where r->>'account_email'='feedback-player@ultimatum.invalid') then raise exception 'account email missing';end if;
end $$;
reset role;
select 'Feedback report assertions passed' as result;
rollback;
