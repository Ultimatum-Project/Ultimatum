create index if not exists account_resource_versions_parent_version_idx
  on public.account_resource_versions (parent_version);

create index if not exists account_resources_current_version_idx
  on public.account_resources (current_version);
