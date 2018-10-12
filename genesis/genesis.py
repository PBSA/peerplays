import json
import yaml
from peerplays import PeerPlays
from peerplaysbase.account import BrainKey, Address


def pubkey():
    return format(next(key).get_public_key(), "TEST")


def address():
    pub = next(key).get_public_key()
    return format(Address.from_pubkey(pub), "TEST")


# We define a new BrainKey which is used to generate some new keys
# for genesis block
key = BrainKey("secret!")

# Let's load Alice(main net) genesis block as a basis
genesis = json.load(open("alice-genesis.json", "r"))

# Initial accounts are init* + faucet
genesis_accounts = list()
for i in range(11):
    genesis_accounts.append(dict(
        active_key=pubkey(),
        owner_key=pubkey(),
        is_lifetime_member=True,
        name="init{}".format(i)))

genesis_accounts.append(dict(
    active_key=pubkey(),
    owner_key=pubkey(),
    is_lifetime_member=True,
    name="faucet"))
genesis_accounts.append(dict(
    active_key=pubkey(),
    owner_key=pubkey(),
    is_lifetime_member=True,
    name="pbsa"))
genesis["initial_accounts"] = genesis_accounts

# BTF Asset
genesis["initial_assets"] = [
    {
        "accumulated_fees": 0,
        "collateral_records": [],
        "description": "Fun token. Supposed to be worthless!",
        "is_bitasset": False,
        "issuer_name": "pbsa",
        "max_supply": 21_000_000 * 10 ** 8,   # 21 Mio with precision 8
        "precision": 8,  # precision 8
        "symbol": "BTF"
    }
]

# Inicial balances (claimable) - replace PPY prefix with TEST prefix
btf_balances = []
for i, _ in enumerate(genesis["initial_balances"]):
    genesis["initial_balances"][i]["asset_symbol"] = "TEST"
    genesis["initial_balances"][i]["owner"] = "TEST" + genesis["initial_balances"][i]["owner"][3:]

# Let's distribute extra stake for sake of governance
sum_distributed = sum([int(x["amount"]) for x in genesis["initial_balances"]])
# 34% of supply are left for witnesses etc
initial_pbsa = str(int(genesis["max_core_supply"]) * 0.66 - sum_distributed)
genesis["initial_balances"].append(dict(
    amount=int(initial_pbsa),
    asset_symbol="TEST",
    owner=address()
))

# Update sining keys of initial witnesses
for i, _ in enumerate(genesis["initial_witness_candidates"]):
    genesis["initial_witness_candidates"][i]["block_signing_key"] = pubkey()

# Other stuff
genesis["initial_vesting_balances"] = []
genesis["initial_bts_accounts"] = []
genesis["max_core_supply"] = '400000000000000'

# Read params from Alice network
peerplays = PeerPlays("wss://node.peerplays.download")
params = peerplays.rpc.get_object("2.0.0")["parameters"]
genesis["initial_parameters"] = {
    'initial_parameters': params
}

# Store new genesis file as YAML file for better readbility
yaml.dump(genesis, open("genesis.yaml", "w"))

# .. and as json
json.dump(genesis, open("genesis.json", "w"))

print("Last sequence number: {}".format(key.sequence))
