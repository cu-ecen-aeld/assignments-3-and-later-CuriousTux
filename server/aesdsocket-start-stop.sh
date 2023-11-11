#!/bin/sh

case "$1" in
    start)
        echo "Starting assignment-5-server"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        # start-stop-daemon -S -n aesdsocket -a ~/Learning/assignment-1-CuriousTux/server/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping assignment-5-server"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0