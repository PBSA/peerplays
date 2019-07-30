#!/bin/bash
PEERPLAYSD="/usr/local/bin/witness_node"

# For blockchain download
VERSION=`cat /etc/peerplays/version`

## Supported Environmental Variables
#
#   * $PEERPLAYSD_SEED_NODES
#   * $PEERPLAYSD_RPC_ENDPOINT
#   * $PEERPLAYSD_PLUGINS
#   * $PEERPLAYSD_REPLAY
#   * $PEERPLAYSD_RESYNC
#   * $PEERPLAYSD_P2P_ENDPOINT
#   * $PEERPLAYSD_WITNESS_ID
#   * $PEERPLAYSD_PRIVATE_KEY
#   * $PEERPLAYSD_TRACK_ACCOUNTS
#   * $PEERPLAYSD_PARTIAL_OPERATIONS
#   * $PEERPLAYSD_MAX_OPS_PER_ACCOUNT
#   * $PEERPLAYSD_TRUSTED_NODE
#

ARGS=""
# Translate environmental variables
if [[ ! -z "$PEERPLAYSD_SEED_NODES" ]]; then
    for NODE in $PEERPLAYSD_SEED_NODES ; do
        ARGS+=" --seed-node=$NODE"
    done
fi
if [[ ! -z "$PEERPLAYSD_RPC_ENDPOINT" ]]; then
    ARGS+=" --rpc-endpoint=${PEERPLAYSD_RPC_ENDPOINT}"
fi

if [[ ! -z "$PEERPLAYSD_REPLAY" ]]; then
    ARGS+=" --replay-blockchain"
fi

if [[ ! -z "$PEERPLAYSD_RESYNC" ]]; then
    ARGS+=" --resync-blockchain"
fi

if [[ ! -z "$PEERPLAYSD_P2P_ENDPOINT" ]]; then
    ARGS+=" --p2p-endpoint=${PEERPLAYSD_P2P_ENDPOINT}"
fi

if [[ ! -z "$PEERPLAYSD_WITNESS_ID" ]]; then
    ARGS+=" --witness-id=$PEERPLAYSD_WITNESS_ID"
fi

if [[ ! -z "$PEERPLAYSD_PRIVATE_KEY" ]]; then
    ARGS+=" --private-key=$PEERPLAYSD_PRIVATE_KEY"
fi

if [[ ! -z "$PEERPLAYSD_TRACK_ACCOUNTS" ]]; then
    for ACCOUNT in $PEERPLAYSD_TRACK_ACCOUNTS ; do
        ARGS+=" --track-account=$ACCOUNT"
    done
fi

if [[ ! -z "$PEERPLAYSD_PARTIAL_OPERATIONS" ]]; then
    ARGS+=" --partial-operations=${PEERPLAYSD_PARTIAL_OPERATIONS}"
fi

if [[ ! -z "$PEERPLAYSD_MAX_OPS_PER_ACCOUNT" ]]; then
    ARGS+=" --max-ops-per-account=${PEERPLAYSD_MAX_OPS_PER_ACCOUNT}"
fi

if [[ ! -z "$PEERPLAYSD_TRUSTED_NODE" ]]; then
    ARGS+=" --trusted-node=${PEERPLAYSD_TRUSTED_NODE}"
fi

## Link the peerplays config file into home
## This link has been created in Dockerfile, already
ln -f -s /etc/peerplays/config.ini /var/lib/peerplays

# Plugins need to be provided in a space-separated list, which
# makes it necessary to write it like this
if [[ ! -z "$PEERPLAYSD_PLUGINS" ]]; then
   $PEERPLAYSD --data-dir ${HOME} ${ARGS} ${PEERPLAYSD_ARGS} --plugins "${PEERPLAYSD_PLUGINS}"
else
   $PEERPLAYSD --data-dir ${HOME} ${ARGS} ${PEERPLAYSD_ARGS}
fi
