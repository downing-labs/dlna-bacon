FROM alpine:3.20 AS builder

RUN apk add --no-cache \
	build-base automake autoconf libtool pkgconf \
	sqlite-dev libexif-dev jpeg-dev libid3tag-dev flac-dev \
	libvorbis-dev ffmpeg-dev linux-headers zlib-dev bsd-compat-headers

WORKDIR /src
COPY . .

RUN ./autogen.sh && \
	./configure && \
	make -j"$(nproc)"

FROM alpine:3.20

RUN apk add --no-cache \
	sqlite-libs libexif jpeg libid3tag flac libvorbis \
	ffmpeg-libs tini curl su-exec

RUN addgroup -S minidlna && adduser -S -G minidlna -H -h /minidlna minidlna && \
	mkdir -p /minidlna/cache /media && \
	chown -R minidlna:minidlna /minidlna

COPY --from=builder /src/minidlnad /usr/sbin/minidlnad
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

VOLUME /media
EXPOSE 8200/tcp 1900/udp

HEALTHCHECK --interval=10s --timeout=10s --retries=6 CMD \
	curl --silent --fail http://127.0.0.1:8200/status || exit 1

ENTRYPOINT ["/sbin/tini", "--"]
CMD ["/entrypoint.sh"]
