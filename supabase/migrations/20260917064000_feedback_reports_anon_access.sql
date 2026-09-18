-- The public security-invoker wrapper needs schema usage to reach the one
-- explicitly granted private submission function. The private schema remains
-- outside PostgREST and its tables retain no anon privileges.
grant usage on schema ultimatum_private to anon;
