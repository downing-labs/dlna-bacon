/* webui.h
 *
 * Part of DLNA Bacon (https://github.com/downing-labs/dlna-bacon)
 * A derivative of MiniDLNA / ReadyMedia, GPLv2.
 * Developed by Downing Labs, 2026.
 *
 * This file is kept deliberately separate from upstream's upnphttp.c so
 * that future upstream releases only require re-pointing two small hooks
 * (see upnphttp.c) rather than re-merging a rewritten function.
 */
#ifndef __WEBUI_H__
#define __WEBUI_H__

#include "upnphttp.h"

void webui_send_status(struct upnphttp * h);
void webui_trigger_rescan(struct upnphttp * h);

#endif
