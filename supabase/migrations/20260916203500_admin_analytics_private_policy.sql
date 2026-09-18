-- Explicitly document the deny-by-default boundary for the private event table.
create policy no_direct_product_event_access
on ultimatum_private.product_events
as restrictive
for all
to anon,authenticated
using (false)
with check (false);
