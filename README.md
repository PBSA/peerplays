Intro for new developers and witnesses
------------------------

This is a quick introduction to get new developers and witnesses up to speed on Peerplays blockchain.

Starting A Peerplays Node
-----------------

For Ubuntu 14.04 LTS and up users, see this link first:
    https://github.com/cryptonomex/graphene/wiki/build-ubuntu

and then proceed with:

    git clone https://github.com/pbsa/peerplays.git
    cd peerplays
    git submodule update --init --recursive
    cmake -DCMAKE_BUILD_TYPE=Debug .
    make
    ./programs/witness_node/witness_node

This will launch the witness node. If you would like to launch the command-line wallet, you must first specify a port
for communication with the witness node. To do this, add text to `witness_node_data_dir/config.ini` as follows, then
restart the node:

    rpc-endpoint = 127.0.0.1:8090

Then, in a separate terminal window, start the command-line wallet `cli_wallet`:

    ./programs/cli_wallet/cli_wallet

To set your initial password to 'password' use:

    >>> set_password password
    >>> unlock password

To import your initial balance:

    >>> import_balance nathan [5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3] true

If you send private keys over this connection, `rpc-endpoint` should be bound to localhost for security.

A list of CLI wallet commands is available
[here](https://github.com/PBSA/peerplays/blob/master/libraries/wallet/include/graphene/wallet/wallet.hpp).


Witness node
------------

The role of the witness node is to broadcast transactions, download blocks, and optionally sign them.

```
./witness_node --rpc-endpoint 127.0.0.1:8090 --enable-stale-production -w '"1.6.0"' '"1.6.1"' '"1.6.2"' '"1.6.3"' '"1.6.4"' '"1.6.5"' '"1.6.6"' '"1.6.7"' '"1.6.8"' '"1.6.9"' '"1.6.10"' '"1.6.11"' '"1.6.12"' '"1.6.13"' '"1.6.14"' '"1.6.15"' '"1.6.16"' '"1.6.17"' '"1.6.18"' '"1.6.19"' '"1.6.20"' '"1.6.21"' '"1.6.22"' '"1.6.23"' '"1.6.24"' '"1.6.25"' '"1.6.26"' '"1.6.27"' '"1.6.28"' '"1.6.29"' '"1.6.30"' '"1.6.31"' '"1.6.32"' '"1.6.33"' '"1.6.34"' '"1.6.35"' '"1.6.36"' '"1.6.37"' '"1.6.38"' '"1.6.39"' '"1.6.40"' '"1.6.41"' '"1.6.42"' '"1.6.43"' '"1.6.44"' '"1.6.45"' '"1.6.46"' '"1.6.47"' '"1.6.48"' '"1.6.49"' '"1.6.50"' '"1.6.51"' '"1.6.52"' '"1.6.53"' '"1.6.54"' '"1.6.55"' '"1.6.56"' '"1.6.57"' '"1.6.58"' '"1.6.59"' '"1.6.60"' '"1.6.61"' '"1.6.62"' '"1.6.63"' '"1.6.64"' '"1.6.65"' '"1.6.66"' '"1.6.67"' '"1.6.68"' '"1.6.69"' '"1.6.70"' '"1.6.71"' '"1.6.72"' '"1.6.73"' '"1.6.74"' '"1.6.75"' '"1.6.76"' '"1.6.77"' '"1.6.78"' '"1.6.79"' '"1.6.80"' '"1.6.81"' '"1.6.82"' '"1.6.83"' '"1.6.84"' '"1.6.85"' '"1.6.86"' '"1.6.87"' '"1.6.88"' '"1.6.89"' '"1.6.90"' '"1.6.91"' '"1.6.92"' '"1.6.93"' '"1.6.94"' '"1.6.95"' '"1.6.96"' '"1.6.97"' '"1.6.98"' '"1.6.99"' '"1.6.100"'
```

Testnet
----------------------

- chain-id - 82c339d32256728bcc4df63bcc3e3244f6140a16fef31f707a6613ad189156ae


### Add this seed node to config.ini
```
vim witness_node_data_dir/config.ini

seed-node = 213.184.225.234:59500
```

### Register your username at the faucet address
https://595-dev.pixelplex.by/


### Use the get_private_key_from_password command in cli_wallet with the password downloaded during your username signup to access your private keys. You will need owner, active, and memo
```
get_private_key_from_password the_key_you_received_from_the_faucet your_witness_username active
```
This will reveal an array `['PPYxxx', 'xxxx']`

### import_keys into your cli_wallet 
- use the second value in the array returned from the previous step for the private key
- be sure to wrap your username in quotes
```
import_key "your_witness_username" private_key_from_previous_step
```

### Upgrade your account to lifetime membership
```
upgrade_account your_witness_username true
```

### Create your witness (substitute the url for your witness information)
```
create_witness your_witness_username url true
```
Be sure to take note of the block_signing_key and use get_private_key so you can have your key pair for your config.

### Get your witness id
```
get_witness username (note the "id" for your config)
```

### Modify your witness_node config.ini to include **your** witness id and private key pair.
```
vim witness_node_data_dir/config.ini

witness-id = "1.6.x"
private-keys = ['PPYxxx'.'xxxx']
```

### start your witness with resync
```
./programs/cli_wallet/cli_wallet --resync --replay
```

### Ask to be voted in!

Join @Peerplays Telegram group to find information about the witness group.
http://t.me/@peerplayswitness

You will get logs that look like this:

```
2070264ms th_a       application.cpp:506           handle_block         ] Got block: #87913 time: 2017-05-27T16:34:30 latency: 264 ms from: bhuz-witness  irreversible: 87903 (-10)
2071000ms th_a       witness.cpp:204               block_production_loo ] Not producing block because slot has not yet arrived
2072000ms th_a       witness.cpp:204               block_production_loo ] Not producing block because slot has not yet arrived
2073000ms th_a       witness.cpp:201               block_production_loo ] Not producing block because it isn't my turn
```

Assuming you've received votes, you will start producing as a witness at the next maintenance interval (once per hour).


Running specific tests
----------------------

- `tests/chain_tests -t block_tests/name_of_test`


 
