#!/usr/bin/env python3
"""
QHandle Payload Generator & Decoder for qubic-cli

Provides command-line utilities and importable functions to encode binary payloads
for all QHandle smart contract procedures and functions for use with:
    qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> <AMOUNT> <INPUT_TYPE> <HEX_PAYLOAD>
    qubic-cli -sendcustomfunction <CONTRACT_INDEX> <INPUT_TYPE> <HEX_PAYLOAD>
"""

import argparse
import struct
import sys
from typing import Tuple, Optional

# Character sets and constants
ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

def decode_identity(identity: str) -> bytes:
    """Decodes a 60-character Qubic identity or 64-character hex into 32-byte public key."""
    identity = identity.strip()
    if len(identity) == 64:
        try:
            return bytes.fromhex(identity)
        except ValueError:
            pass

    if len(identity) != 60:
        raise ValueError(f"Identity must be 60 characters (or 64 hex chars), got {len(identity)}")

    identity = identity.upper()
    pubkey = bytearray(32)
    for i in range(4):
        val = 0
        for j in range(13, -1, -1):
            c = identity[i * 14 + j]
            idx = ALPHABET.find(c)
            if idx == -1:
                raise ValueError(f"Invalid character '{c}' in identity")
            val = val * 26 + idx
        pubkey[i * 8:(i + 1) * 8] = struct.pack("<Q", val)
    return bytes(pubkey)

def encode_identity(pubkey: bytes) -> str:
    """Encodes a 32-byte public key into a 60-character Qubic identity."""
    if len(pubkey) != 32:
        raise ValueError("Public key must be exactly 32 bytes")

    chars = []
    for i in range(4):
        val = struct.unpack("<Q", pubkey[i * 8:(i + 1) * 8])[0]
        for _ in range(14):
            chars.append(ALPHABET[val % 26])
            val //= 26

    # 4 checksum chars (placeholder for test/demo identities if K12 not in Python stdlib)
    # The first 56 characters uniquely define the 32-byte public key.
    chars.extend(["A", "A", "A", "A"])
    return "".join(chars)

def canonicalize_name(name: str) -> bytes:
    """Converts name to lowercase, strips trailing/leading space, pads to 32 bytes."""
    clean = name.strip().lower()
    raw = clean.encode("ascii")
    if len(raw) > 32:
        raise ValueError(f"Handle name exceeds 32 bytes: '{name}'")
    return raw.ljust(32, b"\x00")

# ----------------------------------------------------------------------
# Procedure Encoders (qubic-cli -sendcustomtransaction)
# ----------------------------------------------------------------------

def encode_commit_registration(commitment: bytes) -> str:
    """Procedure 1: CommitRegistration (32 bytes)"""
    if len(commitment) != 32:
        raise ValueError("Commitment must be 32 bytes")
    return commitment.hex()

def encode_register_handle(name: str, target: bytes, salt: int, years: int) -> str:
    """Procedure 2: RegisterHandle (80 bytes: 32B name, 32B target, 8B salt, 2B years, 6B pad)"""
    name_bytes = canonicalize_name(name)
    if len(target) != 32:
        raise ValueError("Target must be 32 bytes")
    # Struct alignment: uint16 years followed by 6 bytes padding to align struct to 8-byte boundary
    payload = name_bytes + target + struct.pack("<QH6x", salt, years)
    assert len(payload) == 80, f"Expected 80 bytes, got {len(payload)}"
    return payload.hex()

def encode_renew_handle(name: str, years: int) -> str:
    """Procedure 3: RenewHandle (36 bytes: 32B name, 2B years, 2B pad)"""
    name_bytes = canonicalize_name(name)
    payload = name_bytes + struct.pack("<H2x", years)
    assert len(payload) == 36, f"Expected 36 bytes, got {len(payload)}"
    return payload.hex()

def encode_transfer_handle(name: str, new_owner: bytes) -> str:
    """Procedure 4: TransferHandle (64 bytes: 32B name, 32B newOwner)"""
    name_bytes = canonicalize_name(name)
    if len(new_owner) != 32:
        raise ValueError("New owner must be 32 bytes")
    payload = name_bytes + new_owner
    assert len(payload) == 64, f"Expected 64 bytes, got {len(payload)}"
    return payload.hex()

def encode_set_target(name: str, new_target: bytes) -> str:
    """Procedure 5: SetTarget (64 bytes: 32B name, 32B newTarget)"""
    name_bytes = canonicalize_name(name)
    if len(new_target) != 32:
        raise ValueError("New target must be 32 bytes")
    payload = name_bytes + new_target
    assert len(payload) == 64, f"Expected 64 bytes, got {len(payload)}"
    return payload.hex()

def encode_set_primary(name: str, clear: bool = False) -> str:
    """Procedure 6: SetPrimary (33 bytes: 32B name, 1B clear)"""
    name_bytes = canonicalize_name(name)
    payload = name_bytes + struct.pack("<?", clear)
    assert len(payload) == 33, f"Expected 33 bytes, got {len(payload)}"
    return payload.hex()

def encode_set_lock(name: str, locked: bool = True) -> str:
    """Procedure 7: SetLock (33 bytes: 32B name, 1B locked)"""
    name_bytes = canonicalize_name(name)
    payload = name_bytes + struct.pack("<?", locked)
    assert len(payload) == 33, f"Expected 33 bytes, got {len(payload)}"
    return payload.hex()

def encode_reclaim_expired(name: str) -> str:
    """Procedure 8: ReclaimExpired (32 bytes: 32B name)"""
    name_bytes = canonicalize_name(name)
    assert len(name_bytes) == 32
    return name_bytes.hex()

def encode_prune_commitment(committer: bytes, commitment: bytes) -> str:
    """Procedure 9: PruneCommitment (64 bytes: 32B committer, 32B commitment)"""
    if len(committer) != 32 or len(commitment) != 32:
        raise ValueError("Committer and commitment must be 32 bytes")
    payload = committer + commitment
    assert len(payload) == 64, f"Expected 64 bytes, got {len(payload)}"
    return payload.hex()

# ----------------------------------------------------------------------
# Function Encoders & Decoders (qubic-cli -sendcustomfunction)
# ----------------------------------------------------------------------

def encode_resolve_handle(name: str) -> str:
    """Function 2: ResolveHandle input (32 bytes)"""
    return canonicalize_name(name).hex()

def encode_reverse_resolve(address: bytes) -> str:
    """Function 3: ReverseResolve input (32 bytes)"""
    if len(address) != 32:
        raise ValueError("Address must be 32 bytes")
    return address.hex()

def decode_resolve_output(hex_data: str) -> dict:
    """Decodes 71-byte ResolveHandle_output: target(32), owner(32), regEpoch(2), expEpoch(2), len(1), locked(1), status(1)"""
    data = bytes.fromhex(hex_data.strip())
    if len(data) < 71:
        raise ValueError(f"Response too short: expected at least 71 bytes, got {len(data)}")
    target = data[:32]
    owner = data[32:64]
    reg_epoch, exp_epoch, length, locked, status = struct.unpack("<HHBBB", data[64:71])
    return {
        "target_hex": target.hex(),
        "target_identity": encode_identity(target),
        "owner_hex": owner.hex(),
        "owner_identity": encode_identity(owner),
        "registered_epoch": reg_epoch,
        "expiry_epoch": exp_epoch,
        "length": length,
        "locked": bool(locked),
        "status": status
    }

def decode_reverse_resolve_output(hex_data: str) -> dict:
    """Decodes 34-byte ReverseResolve_output: name(32), len(1), status(1)"""
    data = bytes.fromhex(hex_data.strip())
    if len(data) < 34:
        raise ValueError(f"Response too short: expected at least 34 bytes, got {len(data)}")
    raw_name = data[:32]
    length, status = struct.unpack("<BB", data[32:34])
    name_str = raw_name[:length].decode("ascii", errors="replace")
    return {
        "name": name_str,
        "length": length,
        "status": status
    }

def main():
    parser = argparse.ArgumentParser(description="QHandle Payload Generator & Decoder for qubic-cli")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # commit
    p_commit = subparsers.add_parser("commit", help="Generate payload for CommitRegistration (Proc 1)")
    p_commit.add_argument("commitment", help="Commitment hash (hex or Qubic identity)")

    # register
    p_reg = subparsers.add_parser("register", help="Generate payload for RegisterHandle (Proc 2)")
    p_reg.add_argument("name", help="Handle name (e.g. alice)")
    p_reg.add_argument("target", help="Target address (Qubic identity or hex)")
    p_reg.add_argument("salt", type=lambda s: int(s, 0), help="Commitment salt (uint64)")
    p_reg.add_argument("years", type=int, help="Registration duration (1-5)")

    # renew
    p_ren = subparsers.add_parser("renew", help="Generate payload for RenewHandle (Proc 3)")
    p_ren.add_argument("name", help="Handle name")
    p_ren.add_argument("years", type=int, help="Renewal duration (1-5)")

    # transfer
    p_tx = subparsers.add_parser("transfer", help="Generate payload for TransferHandle (Proc 4)")
    p_tx.add_argument("name", help="Handle name")
    p_tx.add_argument("new_owner", help="New owner address (Qubic identity or hex)")

    # set-target
    p_st = subparsers.add_parser("set-target", help="Generate payload for SetTarget (Proc 5)")
    p_st.add_argument("name", help="Handle name")
    p_st.add_argument("new_target", help="New target address (Qubic identity or hex)")

    # set-primary
    p_sp = subparsers.add_parser("set-primary", help="Generate payload for SetPrimary (Proc 6)")
    p_sp.add_argument("name", help="Handle name")
    p_sp.add_argument("--clear", action="store_true", help="Clear primary reverse record")

    # set-lock
    p_sl = subparsers.add_parser("set-lock", help="Generate payload for SetLock (Proc 7)")
    p_sl.add_argument("name", help="Handle name")
    p_sl.add_argument("locked", type=int, choices=[0, 1], help="1 to lock, 0 to unlock")

    # reclaim
    p_rec = subparsers.add_parser("reclaim", help="Generate payload for ReclaimExpired (Proc 8)")
    p_rec.add_argument("name", help="Handle name")

    # prune
    p_pr = subparsers.add_parser("prune", help="Generate payload for PruneCommitment (Proc 9)")
    p_pr.add_argument("committer", help="Committer address (identity or hex)")
    p_pr.add_argument("commitment", help="Commitment hash (identity or hex)")

    # resolve
    p_res = subparsers.add_parser("resolve", help="Generate payload for ResolveHandle (Func 2)")
    p_res.add_argument("name", help="Handle name")

    # reverse-resolve
    p_rev = subparsers.add_parser("reverse-resolve", help="Generate payload for ReverseResolve (Func 3)")
    p_rev.add_argument("address", help="Address (identity or hex)")

    # decode-resolve
    p_dres = subparsers.add_parser("decode-resolve", help="Decode hex output of ResolveHandle")
    p_dres.add_argument("hex_data", help="Hex string from node response")

    # decode-reverse
    p_drev = subparsers.add_parser("decode-reverse", help="Decode hex output of ReverseResolve")
    p_drev.add_argument("hex_data", help="Hex string from node response")

    args = parser.parse_args()

    try:
        if args.command == "commit":
            commit_bytes = decode_identity(args.commitment)
            hex_payload = encode_commit_registration(commit_bytes)
            print(f"Procedure:   1 (CommitRegistration)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 1 {hex_payload}")

        elif args.command == "register":
            target_bytes = decode_identity(args.target)
            hex_payload = encode_register_handle(args.name, target_bytes, args.salt, args.years)
            clean_name = args.name.strip().lower()
            fee = 20_000_000 * args.years
            if len(clean_name) == 3:
                fee = 1_000_000_000 * args.years
            elif len(clean_name) == 4:
                fee = 200_000_000 * args.years
            print(f"Procedure:   2 (RegisterHandle)")
            print(f"Fee (QUBIC): {fee}")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> {fee} 2 {hex_payload}")

        elif args.command == "renew":
            hex_payload = encode_renew_handle(args.name, args.years)
            fee = 20_000_000 * args.years
            print(f"Procedure:   3 (RenewHandle)")
            print(f"Fee (QUBIC): {fee}")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> {fee} 3 {hex_payload}")

        elif args.command == "transfer":
            new_owner_bytes = decode_identity(args.new_owner)
            hex_payload = encode_transfer_handle(args.name, new_owner_bytes)
            print(f"Procedure:   4 (TransferHandle)")
            print(f"Fee (QUBIC): 1000000")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 1000000 4 {hex_payload}")

        elif args.command == "set-target":
            new_target_bytes = decode_identity(args.new_target)
            hex_payload = encode_set_target(args.name, new_target_bytes)
            print(f"Procedure:   5 (SetTarget)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 5 {hex_payload}")

        elif args.command == "set-primary":
            hex_payload = encode_set_primary(args.name, args.clear)
            print(f"Procedure:   6 (SetPrimary)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 6 {hex_payload}")

        elif args.command == "set-lock":
            hex_payload = encode_set_lock(args.name, bool(args.locked))
            print(f"Procedure:   7 (SetLock)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 7 {hex_payload}")

        elif args.command == "reclaim":
            hex_payload = encode_reclaim_expired(args.name)
            print(f"Procedure:   8 (ReclaimExpired)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 8 {hex_payload}")

        elif args.command == "prune":
            committer_bytes = decode_identity(args.committer)
            commit_bytes = decode_identity(args.commitment)
            hex_payload = encode_prune_commitment(committer_bytes, commit_bytes)
            print(f"Procedure:   9 (PruneCommitment)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 9 {hex_payload}")

        elif args.command == "resolve":
            hex_payload = encode_resolve_handle(args.name)
            print(f"Function:    2 (ResolveHandle)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomfunction <CONTRACT_INDEX> 2 {hex_payload}")

        elif args.command == "reverse-resolve":
            addr_bytes = decode_identity(args.address)
            hex_payload = encode_reverse_resolve(addr_bytes)
            print(f"Function:    3 (ReverseResolve)")
            print(f"Payload Hex: {hex_payload}")
            print(f"CLI Command: qubic-cli -sendcustomfunction <CONTRACT_INDEX> 3 {hex_payload}")

        elif args.command == "decode-resolve":
            res = decode_resolve_output(args.hex_data)
            print(f"Status:           {res['status']}")
            print(f"Target Identity:  {res['target_identity']} ({res['target_hex']})")
            print(f"Owner Identity:   {res['owner_identity']} ({res['owner_hex']})")
            print(f"Registered Epoch: {res['registered_epoch']}")
            print(f"Expiry Epoch:     {res['expiry_epoch']}")
            print(f"Length:           {res['length']}")
            print(f"Locked:           {'yes' if res['locked'] else 'no'}")

        elif args.command == "decode-reverse":
            res = decode_reverse_resolve_output(args.hex_data)
            print(f"Status: {res['status']}")
            print(f"Name:   {res['name']}")
            print(f"Length: {res['length']}")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
