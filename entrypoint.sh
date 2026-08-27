#!/bin/sh
set -e

# Compatible with the same env vars as vladgh/minidlna, so existing
# docker-compose.yml files should work unmodified. See README.md for
# the full list.

PUID="${PUID:-1000}"
PGID="${PGID:-1000}"

if [ -n "$TZ" ]; then
	if [ -f "/usr/share/zoneinfo/$TZ" ]; then
		cp "/usr/share/zoneinfo/$TZ" /etc/localtime
		echo "$TZ" > /etc/timezone
	else
		echo "WARNING: TZ=$TZ not found under /usr/share/zoneinfo, skipping" >&2
	fi
fi

# Alpine's minidlna user was created at fixed IDs; re-home it to
# whatever PUID/PGID the host wants, so bind-mounted media/db
# ownership lines up with the calling user.
CURRENT_UID="$(id -u minidlna)"
CURRENT_GID="$(id -g minidlna)"
if [ "$CURRENT_GID" != "$PGID" ]; then
	deluser minidlna >/dev/null 2>&1 || true
	delgroup minidlna >/dev/null 2>&1 || true
	addgroup -g "$PGID" -S minidlna
	adduser -u "$PUID" -S -G minidlna -H -h /minidlna minidlna
elif [ "$CURRENT_UID" != "$PUID" ]; then
	deluser minidlna >/dev/null 2>&1 || true
	adduser -u "$PUID" -S -G minidlna -H -h /minidlna minidlna
fi

CONF=/etc/minidlna.conf
: > "$CONF"

echo "port=${MINIDLNA_PORT:-8200}" >> "$CONF"
echo "db_dir=/minidlna/cache" >> "$CONF"
echo "log_dir=/minidlna/cache" >> "$CONF"
echo "inotify=${MINIDLNA_INOTIFY:-yes}" >> "$CONF"

if [ -n "$MINIDLNA_NETWORK_INTERFACE" ]; then
	echo "network_interface=$MINIDLNA_NETWORK_INTERFACE" >> "$CONF"
fi

if [ -n "$MINIDLNA_ROOT_CONTAINER" ]; then
	echo "root_container=$MINIDLNA_ROOT_CONTAINER" >> "$CONF"
fi

if [ -n "$MINIDLNA_FRIENDLY_NAME" ]; then
	echo "friendly_name=$MINIDLNA_FRIENDLY_NAME" >> "$CONF"
fi

if [ -n "$MINIDLNA_NOTIFY_INTERVAL" ]; then
	echo "notify_interval=$MINIDLNA_NOTIFY_INTERVAL" >> "$CONF"
fi

# Any MINIDLNA_MEDIA_DIR / MINIDLNA_MEDIA_DIR_1 / MINIDLNA_MEDIA_DIR_N
# env var becomes a media_dir line. Value can be a bare path, or
# TYPE,path (A/V/P prefix) to restrict that dir to one media type.
env | grep '^MINIDLNA_MEDIA_DIR' | while IFS='=' read -r _ value; do
	echo "media_dir=$value" >> "$CONF"
done

echo "album_art_names=Cover.jpg/cover.jpg/AlbumArtSmall.jpg/albumartsmall.jpg/AlbumArt.jpg/albumart.jpg/Album.jpg/album.jpg/Folder.jpg/folder.jpg/Thumb.jpg/thumb.jpg" >> "$CONF"

chown -R minidlna:minidlna /minidlna/cache

exec su-exec minidlna minidlnad -d -f "$CONF" -P /minidlna/cache/minidlna.pid
