/* webui.c
 *
 * Part of DLNA Bacon (https://github.com/downing-labs/dlna-bacon)
 * A derivative of MiniDLNA / ReadyMedia, GPLv2.
 * Developed by Downing Labs, 2026.
 *
 * Custom status page + rescan endpoint. Kept out of upnphttp.c so
 * upstream merges only ever touch two small hooks there, not this file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "config.h"
#include "event.h"
#include "upnpglobalvars.h"
#include "upnphttp.h"
#include "webui.h"
#include "clients.h"
#include "sql.h"
#include "utils.h"
#include "log.h"

#define WEBUI_BODY_SIZE 24576

static const char *
webui_relative_time(time_t t)
{
	static char buf[32];
	long diff;

	if (t == 0)
	{
		snprintf(buf, sizeof(buf), "never");
		return buf;
	}

	diff = (long)difftime(time(NULL), t);
	if (diff < 0)
		diff = 0;

	if (diff < 60)
		snprintf(buf, sizeof(buf), "just now");
	else if (diff < 3600)
		snprintf(buf, sizeof(buf), "%ld min ago", diff / 60);
	else if (diff < 86400)
		snprintf(buf, sizeof(buf), "%ld hr ago", diff / 3600);
	else
		snprintf(buf, sizeof(buf), "%ld days ago", diff / 86400);

	return buf;
}

/* files.db's mtime is updated whenever the scanner writes to the
 * database, which is a reliable proxy for "last time content changed"
 * without needing a new global variable or touching minidlna.c. */
static time_t
webui_last_scan_time(void)
{
	char path[PATH_MAX];
	struct stat st;

	snprintf(path, sizeof(path), "%s/files.db", db_path);
	if (stat(path, &st) != 0)
		return 0;
	return st.st_mtime;
}

/* Returns 1 if addr falls inside one of our configured LAN interfaces.
 * Clients outside every configured subnet (e.g. a different VLAN) get
 * labeled rather than shown a bare broadcast MAC placeholder. */
static int
webui_client_on_main_network(struct in_addr addr)
{
	int i;
	for (i = 0; i < n_lan_addr; i++)
	{
		if ((addr.s_addr & lan_addr[i].mask.s_addr) ==
		    (lan_addr[i].addr.s_addr & lan_addr[i].mask.s_addr))
			return 1;
	}
	return 0;
}

static void
webui_write_head(struct string_s *str)
{
	strcatf(str,
		"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
		"<title>" SERVER_NAME " " MINIDLNA_VERSION "</title>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
		"<link rel=\"icon\" href=\"data:image/svg+xml,"
		"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
		"<rect width='32' height='32' rx='6' fill='rgb(247,244,238)'/>"
		"<g stroke-linecap='round' fill='none'>"
		"<path d='M4 10 Q10 6 16 10 T28 10' stroke='rgb(216,90,48)' stroke-width='5'/>"
		"<path d='M4 10 Q10 6 16 10 T28 10' stroke='rgb(250,236,231)' stroke-width='1.5' stroke-dasharray='2,2'/>"
		"<path d='M4 16 Q10 12 16 16 T28 16' stroke='rgb(216,90,48)' stroke-width='5'/>"
		"<path d='M4 16 Q10 12 16 16 T28 16' stroke='rgb(250,236,231)' stroke-width='1.5' stroke-dasharray='2,2'/>"
		"<path d='M4 22 Q10 18 16 22 T28 22' stroke='rgb(216,90,48)' stroke-width='5'/>"
		"<path d='M4 22 Q10 18 16 22 T28 22' stroke='rgb(250,236,231)' stroke-width='1.5' stroke-dasharray='2,2'/>"
		"</g></svg>\">"
		"<style>"
		":root{--pg:#f7f4ee;--sf:#ffffff;--tx:#2b2620;--tx2:#7a7368;--bd:#e8e2d6;"
		"--accent:#d85a30;--accent-tx:#4a1b0c;--accent-bg:#faece7;"
		"--teal-tx:#04342c;--green:#639922;--grey:#b4b2a9;}"
		"[data-theme=dark]{--pg:#1c1a17;--sf:#262320;--tx:#f1ede4;--tx2:#a39c8e;"
		"--bd:#3a352e;--accent:#f0997b;--accent-tx:#f5c4b3;--accent-bg:#4a1b0c;"
		"--teal-tx:#9fe1cb;--green:#97c459;--grey:#5f5e5a;}"
		"@media (prefers-color-scheme:dark){:root:not([data-theme=light]){"
		"--pg:#1c1a17;--sf:#262320;--tx:#f1ede4;--tx2:#a39c8e;--bd:#3a352e;"
		"--accent:#f0997b;--accent-tx:#f5c4b3;--accent-bg:#4a1b0c;"
		"--teal-tx:#9fe1cb;--green:#97c459;--grey:#5f5e5a;}}");
	strcatf(str,
		"body{background:var(--pg);color:var(--tx);"
		"font-family:-apple-system,'Segoe UI',sans-serif;margin:0;"
		"padding:1.5rem 10%%;}"
		".seg{display:flex;border:1px solid var(--bd);border-radius:8px;"
		"overflow:hidden;}"
		".seg button{border:none;background:var(--sf);color:var(--tx2);"
		"font-size:13px;padding:6px 12px;cursor:pointer;}"
		".seg button.on{background:var(--accent);color:#fff;}"
		".card{background:var(--sf);border:1px solid var(--bd);"
		"border-radius:12px;padding:1rem 1.25rem;}"
		".grid{display:grid;grid-template-columns:"
		"repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:1.25rem;}"
		"table{width:100%%;border-collapse:collapse;font-size:13px;}"
		"th{text-align:left;color:var(--tx2);font-weight:500;padding:8px 10px;"
		"border-bottom:1px solid var(--bd);}"
		"td{padding:8px 10px;border-bottom:1px solid var(--bd);}"
		".orb{display:inline-block;width:10px;height:10px;border-radius:50%%;}"
		"</style></head><body>");
}

void
webui_send_status(struct upnphttp * h)
{
	struct string_s str;
	char *body;
	int a, v, p, f, i, scanning;
	time_t last_scan;

	body = malloc(WEBUI_BODY_SIZE);
	if (!body)
	{
		Send500(h);
		return;
	}
	str.data = body;
	str.size = WEBUI_BODY_SIZE;
	str.off = 0;

	h->respflags = FLAG_HTML;

	a = sql_get_int_field(db, "SELECT count(*) from DETAILS where MIME glob 'a*'");
	v = sql_get_int_field(db, "SELECT count(*) from DETAILS where MIME glob 'v*'");
	p = sql_get_int_field(db, "SELECT count(*) from DETAILS where MIME glob 'i*'");
	f = sql_get_int_field(db, "SELECT count(*) from OBJECTS where CLASS glob 'container.storageFolder*'");
	last_scan = webui_last_scan_time();
	scanning = GETFLAG(SCANNING_MASK) || GETFLAG(RESCAN_MASK);

	webui_write_head(&str);

	strcatf(&str,
		"<div style=\"display:flex;justify-content:space-between;"
		"align-items:center;margin-bottom:1.5rem;\">"
		"<div><div style=\"font-size:20px;font-weight:500;\">" SERVER_NAME "</div>"
		"<div style=\"font-size:13px;color:var(--tx2);\">Media server status</div>"
		"</div><div class=\"seg\" id=\"theme-seg\">"
		"<button data-t=\"light\">Light</button>"
		"<button data-t=\"dark\">Dark</button>"
		"<button data-t=\"system\" class=\"on\">System</button>"
		"</div></div>");

	strcatf(&str, "<div class=\"grid\">");
	strcatf(&str,
		"<div class=\"card\"><div style=\"font-size:13px;color:var(--tx2);"
		"margin-bottom:6px;\">Video files</div><div style=\"font-size:28px;"
		"font-weight:500;color:var(--accent-tx);\">%d</div></div>", v);
	strcatf(&str,
		"<div class=\"card\"><div style=\"font-size:13px;color:var(--tx2);"
		"margin-bottom:6px;\">Folders</div><div style=\"font-size:28px;"
		"font-weight:500;\">%d</div></div>", f);
	strcatf(&str,
		"<div class=\"card\"><div style=\"font-size:13px;color:var(--tx2);"
		"margin-bottom:6px;\">Audio files</div><div style=\"font-size:28px;"
		"font-weight:500;\">%d</div></div>", a);
	strcatf(&str,
		"<div class=\"card\"><div style=\"font-size:13px;color:var(--tx2);"
		"margin-bottom:6px;\">Image files</div><div style=\"font-size:28px;"
		"font-weight:500;\">%d</div></div>", p);
	strcatf(&str, "</div>");

	strcatf(&str,
		"<div class=\"card\" style=\"display:flex;align-items:center;"
		"justify-content:space-between;margin-bottom:1.25rem;"
		"background:var(--accent-bg);border-color:var(--accent);\">"
		"<div><div style=\"font-weight:500;color:var(--accent-tx);\">"
		"%s <span style=\"font-weight:400;\">(Last scan: %s)</span></div>"
		"<div style=\"font-size:13px;color:var(--accent-tx);opacity:0.85;\">"
		"Add new files, then rescan to make them visible on your devices."
		"</div></div>"
		"<button id=\"rescan-btn\" style=\"background:var(--accent);color:#fff;"
		"border:none;border-radius:8px;padding:10px 18px;font-size:14px;"
		"font-weight:500;cursor:pointer;\" onclick=\"triggerRescan()\">"
		"Rescan library</button></div>",
		scanning ? "Scan in progress" : "Library is up to date",
		webui_relative_time(last_scan));

	strcatf(&str,
		"<div class=\"card\" style=\"margin-bottom:1.25rem;\">"
		"<div style=\"font-weight:500;margin-bottom:10px;\">Connected clients</div>"
		"<table><tr><th style=\"width:32px;\"></th><th>Type</th>"
		"<th>IP address</th><th>Network</th></tr>");
	for (i = 0; i < CLIENT_CACHE_SLOTS; i++)
	{
		int on_main;
		if (!clients[i].addr.s_addr)
			continue;
		on_main = webui_client_on_main_network(clients[i].addr);
		strcatf(&str,
			"<tr><td><span class=\"orb\" style=\"background:var(--%s);\">"
			"</span></td><td>%s</td><td>%s</td><td%s>%s</td></tr>",
			clients[i].connections > 0 ? "green" : "grey",
			clients[i].type->name,
			inet_ntoa(clients[i].addr),
			on_main ? "" : " style=\"color:var(--teal-tx);\"",
			on_main ? "Main" : "Cross-network");
	}
	strcatf(&str, "</table></div>");

	strcatf(&str,
		"<div style=\"text-align:center;font-size:12px;color:var(--tx2);"
		"padding-top:0.5rem;\">Developed by Downing Labs &middot; 2026</div>");

	strcatf(&str,
		"<script>"
		"(function(){"
		"var saved=localStorage.getItem('dlnabacon-theme')||'system';"
		"function apply(mode){"
		"var root=document.documentElement;"
		"if(mode==='system'){root.removeAttribute('data-theme');}"
		"else{root.setAttribute('data-theme',mode);}"
		"var btns=document.querySelectorAll('#theme-seg button');"
		"for(var j=0;j<btns.length;j++){"
		"btns[j].classList.toggle('on',btns[j].getAttribute('data-t')===mode);}"
		"localStorage.setItem('dlnabacon-theme',mode);}"
		"var btns=document.querySelectorAll('#theme-seg button');"
		"for(var j=0;j<btns.length;j++){"
		"btns[j].addEventListener('click',function(){"
		"apply(this.getAttribute('data-t'));});}"
		"apply(saved);"
		"})();"
		/* While a scan is running, poll fast so the page catches the
		 * moment it finishes without the person needing to refresh
		 * manually. Once idle, fall back to a slow background poll so
		 * changes made by someone else (or by inotify) still show up
		 * eventually without any action at all. */
		"var scanning=%s;"
		"setTimeout(function(){location.reload();},scanning?1500:15000);"
		"function triggerRescan(){"
		"var btn=document.getElementById('rescan-btn');"
		"btn.disabled=true;btn.textContent='Rescanning...';"
		"fetch('/rescan').then(function(){"
		"setTimeout(function(){location.reload();},500);"
		"}).catch(function(){"
		"btn.disabled=false;btn.textContent='Rescan library';});}"
		"</script></body></html>",
		scanning ? "true" : "false");

	BuildResp_upnphttp(h, str.data, str.off);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
	free(body);
}

/* Sets the same flag sigusr2() sets on receiving SIGUSR2 -- but since
 * we're already inside the running daemon's process, we skip the
 * signal entirely and flip the flag directly. minidlna.c's main loop
 * (patched separately) picks this up and runs a non-destructive
 * rescan without a restart. */
void
webui_trigger_rescan(struct upnphttp * h)
{
	static const char body[] = "OK";

	if (!GETFLAG(SCANNING_MASK) && !GETFLAG(RESCAN_MASK))
		SETFLAG(RESCAN_MASK);

	h->respflags = FLAG_HTML;
	BuildResp_upnphttp(h, body, sizeof(body) - 1);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}
