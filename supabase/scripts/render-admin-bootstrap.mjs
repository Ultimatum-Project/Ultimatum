import {localSetting} from "../../clients/site/scripts/cloud-environments.mjs";

const email=(localSetting("ULTIMATUM_ADMIN_EMAIL")||"").trim().toLowerCase();
if(!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email))throw Error("Set ULTIMATUM_ADMIN_EMAIL in the ignored root .env.local.");
const escaped=email.replaceAll("'","''");
console.log(`-- Run once in the intended Supabase project's SQL editor.\ninsert into ultimatum_private.admin_emails(email)\nvalues ('${escaped}')\non conflict do nothing;`);
