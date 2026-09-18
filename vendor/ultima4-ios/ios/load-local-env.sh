#!/usr/bin/env bash

# Safe, allowlisted loader for the repository's ignored .env.local file.
# Existing process environment variables always win.
zu4_load_local_env() {
  local repo_root env_file original name value
  repo_root="$(cd "$ZU4_SRC/../.." && pwd)"
  env_file="$repo_root/.env.local"
  export ZU4_REPO_ROOT="$repo_root"
  [ -f "$env_file" ] || return 0
  while IFS= read -r original || [ -n "$original" ]; do
    original="${original%$'\r'}"
    case "$original" in ''|'#'*) continue ;; esac
    name="${original%%=*}"
    value="${original#*=}"
    case "$name" in
      ZU4_IOS_TEAM|ZU4_IOS_BUNDLE_ID|ZU4_IOS_VERSION|ZU4_IOS_BUILD_NUMBER|ZU4_IOS_DEBUG_TOOLS|ZU4_BUILD_DIR|ULTIMATUM_SUPABASE_PUBLIC_URL|ULTIMATUM_SUPABASE_PUBLIC_KEY|ULTIMATUM_SUPABASE_PUBLIC_PROJECT|ULTIMATUM_SUPABASE_TEST_URL|ULTIMATUM_SUPABASE_TEST_KEY|ULTIMATUM_SUPABASE_TEST_PROJECT|ULTIMATUM_CLOUD_AUDIENCE) ;;
      *) continue ;;
    esac
    if [ -z "${!name+x}" ]; then
      if [[ "$value" == \"*\" && "$value" == *\" ]] || [[ "$value" == \'*\' && "$value" == *\' ]]; then value="${value:1:${#value}-2}"; fi
      printf -v "$name" '%s' "$value"
      export "$name"
    fi
  done < "$env_file"
}

zu4_prepare_account_assets() {
  local audience="${1:-test}" required="${2:-}" flag=()
  [ "$audience" = "public" ] && flag+=(--public)
  [ "$required" = "required" ] && flag+=(--require-cloud)
  node "$ZU4_REPO_ROOT/clients/web/scripts/prepare-local-client.mjs" "${flag[@]}"
  export ZU4_ACCOUNT_UI="$ZU4_REPO_ROOT/clients/web/.cache/local-dist"
  export ZU4_CLOUD_SESSION_ACCOUNT
  ZU4_CLOUD_SESSION_ACCOUNT="$(tr -d '\r\n' < "$ZU4_ACCOUNT_UI/cloud-session-account.txt")"
}
