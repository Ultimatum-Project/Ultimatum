create policy feedback_reports_no_direct_access
  on ultimatum_private.feedback_reports
  as restrictive for all to anon,authenticated
  using (false) with check (false);
