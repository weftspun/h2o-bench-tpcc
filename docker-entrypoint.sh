#!/bin/bash
set -e

# Start FDB server if not already running
if ! pgrep fdbserver >/dev/null 2>&1; then
    mkdir -p /var/lib/foundationdb/data /var/lib/foundationdb/log /etc/foundationdb
    echo "docker:docker@127.0.0.1:4500" > /etc/foundationdb/fdb.cluster
    fdbserver --cluster_file=/etc/foundationdb/fdb.cluster \
        --datadir=/var/lib/foundationdb/data \
        --public_address=127.0.0.1:4500 \
        --listen_address=127.0.0.1:4500 \
        --logdir=/var/lib/foundationdb/log &
    sleep 3
    # Configure the database
    fdbcli --exec "configure new single memory" || true
    sleep 1
fi

# Load World table if not already loaded
if ! fdbcli --exec "get tfb/w/\x00\x00\x00\x01" 2>/dev/null | grep -q "value"; then
    echo "Loading World table..."
    /opt/h2o-bench-tpcc/bin/tpcc_loader -c/etc/foundationdb/fdb.cluster -w1
fi

# Start the server
exec /opt/h2o-bench-tpcc/bin/h2o-bench-tpcc \
    -a${THREADS:-4} \
    -c/etc/foundationdb/fdb.cluster \
    -w1 \
    -p${PORT:-8080}
