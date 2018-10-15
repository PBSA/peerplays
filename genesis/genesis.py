import json
import yaml
from peerplays import PeerPlays
from peerplaysbase.account import BrainKey, Address


def pubkey():
    k = next(key)
    pub = format(k.get_public_key(), "TEST")
    pri = str(k.get_private_key())
    print("[\"{}\", \"{}\"]".format(pub, pri))
    return pub


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
        "max_supply": 21000000 * 10 ** 8,   # 21 Mio with precision 8
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
initial_pbsa = str(int(int(genesis["max_core_supply"]) * 0.66 - sum_distributed))
genesis["initial_balances"].append(dict(
    amount=initial_pbsa,
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

# Missing Fees from Alice
params["current_fees"]["parameters"].extend([
    [50, {}],
    [51, {"fee": 100000}],
    [52, {"fee": 100000}],
    [53, {"fee": 100000}],
    [54, {"fee": 100000}],
    [55, {"fee": 100000}],
    [56, {"fee": 100000}],
    [57, {"fee": 100000}],
    [58, {"fee": 100000}],
    [59, {"fee": 100000}],
    [60, {"fee": 100000}],
    [61, {"fee": 100000}],
    [62, {"fee": 100000}],
    [63, {"fee": 100000}],
    [64, {}],
    [65, {}],
    [66, {"fee": 100000}],
    [67, {}],
    [68, {"fee": 100000}],
    [69, {}],
    [70, {"fee": 100000}],
    [71, {"fee": 100000}],
    [72, {"fee": 100000}],
    [73, {"fee": 100000}],
    [74, {"fee": 100000}],
    [75, {}],
    [76, {}],

])
genesis["initial_parameters"] = params
# Updates to parameters and add new parameters
genesis["initial_parameters"].update({
    "block_interval": 3,
    "maintenance_interval": 60 * 60 * 24,
    "maintenance_skip_slots": 3,
    "committee_proposal_review_period": 1209600,
    "maximum_transaction_size": 2048,
    "maximum_block_size": 1228800000,
    "maximum_time_until_expiration": 86400,
    "maximum_proposal_lifetime": 2419200,
    "maximum_asset_whitelist_authorities": 10,
    "maximum_asset_feed_publishers": 10,
    "maximum_witness_count": 1001,
    "maximum_committee_count": 1001,
    "maximum_authority_membership": 10,
    "reserve_percent_of_fee": 2000,
    "network_percent_of_fee": 2000,
    "lifetime_referrer_percent_of_fee": 3000,
    "cashback_vesting_period_seconds": 31536000,
    "cashback_vesting_threshold": 10000000,
    "count_non_member_votes": True,
    "allow_non_member_whitelists": False,
    "witness_pay_per_block": 1000000,
    "worker_budget_per_day": "50000000000",
    "max_predicate_opcode": 1,
    "fee_liquidation_threshold": 10000000,
    "accounts_per_fee_scale": 1000,
    "account_fee_scale_bitshifts": 4,
    "max_authority_depth": 2,
    "witness_schedule_algorithm": 1,
    "min_round_delay": 0,
    "max_round_delay": 600,
    "min_time_per_commit_move": 0,
    "max_time_per_commit_move": 600,
    "min_time_per_reveal_move": 0,
    "max_time_per_reveal_move": 600,
    "rake_fee_percentage": 300,
    "maximum_registration_deadline": 2592000,
    "maximum_players_in_tournament": 256,
    "maximum_tournament_whitelist_length": 1000,
    "maximum_tournament_start_time_in_future": 2419200,
    "maximum_tournament_start_delay": 604800,
    "maximum_tournament_number_of_wins": 100,
    "extensions": {}
})

# Store new genesis file as YAML file for better readbility
yaml.dump(genesis, open("genesis.yaml", "w"))

# .. and as json
json.dump(genesis, open("genesis.json", "w"))

print("Last sequence number: {}".format(key.sequence))
