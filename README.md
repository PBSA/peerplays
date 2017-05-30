Intro for new developers and witnesses
------------------------

This is a quick introduction to get new developers and witnesses up to speed on Peerplays blockchain. It is intended for witnesses plannig to join a live, already deployed blockchain.

Starting A Peerplays Node
-----------------

For Ubuntu 14.04 LTS and up users, see this link first:
    https://github.com/cryptonomex/graphene/wiki/build-ubuntu

and then proceed with:

    git clone https://github.com/pbsa/peerplays.git
    cd peerplays
    git submodule update --init --recursive
    cmake -DCMAKE_BUILD_TYPE=Release .
    make
    ./programs/witness_node/witness_node
    
Launching the witness creates required directories. Next, **stop the witness** and continue.

    vi witness_node_data_dir/config.ini
    p2p-endpoint = 0.0.0.0:9777
    rpc-endpoint = 127.0.0.1:8090
    seed-node = 213.184.225.234:59500
    
Start the witness back up

    ./programs/witness_node/witness_node

Then, in a separate terminal window, start the command-line wallet `cli_wallet`:

    ./programs/cli_wallet/cli_wallet

To set your initial password to 'password' use:

    >>> set_password password
    >>> unlock password


A list of CLI wallet commands is available
[here](https://github.com/PBSA/peerplays/blob/master/libraries/wallet/include/graphene/wallet/wallet.hpp).


Testnet
----------------------
- chain-id - 5b37954aa0d33a8e0d57b084995c262a7c13dbc0693d3e96654e63ff45a9ceec

Register your username at the faucet address
---------------------------
https://595-dev.pixelplex.by/


Use the get_private_key_from_password command
---------------------------------
You will to generate owner and active keys

```
get_private_key_from_password the_key_you_received_from_the_faucet your_witness_username active
get_private_key_from_password the_key_you_received_from_the_faucet your_witness_username owner
```
This will reveal an array for each `['PPYxxx', 'xxxx']`

import_keys into your cli_wallet
-------------------------------
- use the second value in each array returned from the previous step for the private key
- be sure to wrap your username in quotes
- import all 2 keys received above
```
import_key "your_witness_username" xxxx
import_key "your_witness_username" xxxx
```

Upgrade your account to lifetime membership
--------------------------------
```
upgrade_account your_witness_username true
```

Create your witness (substitute the url for your witness information)
-------------------------------
- place quotes around url
```
create_witness your_witness_username "url" true
```
**Be sure to take note of the block_signing_key** 

IMPORTANT (issue below command using block_signing_key just obtained)
```
get_private_key block_signing_key
```
Compare this result to

```
dump_private_keys
```
You should see 3 pairs of keys. The last pair should match your block_signing_key and this is the one you will use in the next step!

Get your witness id
-----------------
```
get_witness username (note the "id" for your config)
```

Modify your witness_node config.ini to include **your** witness id and private key pair.
-------------------------
Comment out the existing private-key before adding yours
```
vim witness_node_data_dir/config.ini

witness-id = "1.6.x"
private-keys = ['block_signing_key','private_key_for_your_block_signing_key']
```

start your witness back up
------------------
```
./programs/witness_node/witness_node
```

If it fails to start, try with these flags (not for permanent use)

```
./programs/witness_node/witness_node --resync --replay
```

Vote for yourself
--------------
```
vote_for_witness your_witness_account your_witness_account true true
```

Ask to be voted in!
--------------

Join @Peerplays Telegram group to find information about the witness group.
http://t.me/@peerplayswitness

You will get logs that look like this:

```
2070264ms th_a       application.cpp:506           handle_block         ] Got block: #87913 time: 2017-05-27T16:34:30 latency: 264 ms from: bhuz-witness  irreversible: 87903 (-10)
2071000ms th_a       witness.cpp:204               block_production_loo ] Not producing block because slot has not yet arrived
2072000ms th_a       witness.cpp:204               block_production_loo ] Not producing block because slot has not yet arrived
2073000ms th_a       witness.cpp:201               block_production_loo ] Not producing block because it isn't my turn
```

Assuming you've received votes, you will start producing as a witness at the next maintenance interval (once per hour). You can check your votes with.

```
get_witness your_witness_account
```

systemd
----------------
It's important for your witness to start when your system boots up. The filepaths here assume that you installed your witness into `/home/ubuntu/peerplays`

Create a logfile to hold your stdout/err logging
```bash
sudo touch /var/log/peerplays.log
```

Save this file in your peerplays directory. `vi /home/ubuntu/peerplays/start.sh`
```bash
#!/bin/bash

cd /home/ubuntu/peerplays
./programs/witness_node/witness_node &> /var/log/peerplays.log
```
Make it executable
```bash
chmod 744 /home/ubuntu/peerplays/start.sh
```
Create this file: `sudo vi /etc/systemd/system/peerplays.service`
Note the path for start.sh. Change it to match where your start.sh file is if necessary.
```
[Unit]
Description=Peerplays Witness
After=network.target

[Service]
ExecStart=/home/ubuntu/peerplays/start.sh

[Install]
WantedBy = multi-user.target
```
Enable the service
```bash
systemctl enable peerplays.service
```
Make sure you don't get any errors
```bash
sudo systemctl status peerplays.service
```
Stop your witness if it is currently running form previous steps, then start it with the service.
```bash
sudo systemctl start peerplays.service
```
Check your logfile for entries
```bash
tail -f /var/log/peerplays.log
```


Running specific tests
----------------------

- `tests/chain_tests -t block_tests/name_of_test`


 
