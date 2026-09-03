#!/usr/bin/env bash
# ==============================================================================
# QHandle — testnet payload generator
#
# WHAT THIS DOES: derives two test identities, then generates and structurally
# validates the qubic-cli payloads for every QHandle procedure and function.
# It prints the exact commands you would run.
#
# WHAT THIS DOES NOT DO: it does NOT contact a node, submit any transaction, or
# verify on-chain behaviour. Nothing here proves the contract behaves correctly
# on a live network. Contract behaviour is covered by tests/test_qhandle_state.cpp
# (53 assertions against real contract code). End-to-end execution against a live
# testnet is milestone 2 and is not implemented yet.
#
# Exit code is non-zero if any payload fails to generate or has the wrong size.
# ==============================================================================

set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$DIR/.." && pwd)"

QUBIC_CLI="${QUBIC_CLI:-$PROJECT_ROOT/qubic-cli/qubic-cli}"
HELPER="${HELPER:-$DIR/qhandle_helper}"
NODE_IP="${NODE_IP:-127.0.0.1}"
NODE_PORT="${NODE_PORT:-21841}"
CONTRACT_INDEX="${CONTRACT_INDEX:-99}"
HANDLE="${HANDLE:-saturn}"
SALT="${SALT:-123456789}"

# Well-known testnet demo seeds. Override via env for a funded account.
# NOTE: seeds are NEVER printed. Generated commands show <ALICE_SEED> as a
# placeholder so this script's output is safe to paste into an issue or CI log.
ALICE_SEED="${ALICE_SEED:-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa}"
BOB_SEED="${BOB_SEED:-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb}"

GREEN='\033[0;32m'; RED='\033[0;31m'; BLUE='\033[0;34m'; YELLOW='\033[0;33m'; NC='\033[0m'

generated=0
failures=0

fail() { echo -e "${RED}[FAIL] $*${NC}" >&2; failures=$((failures + 1)); }
ok()   { echo -e "${GREEN}[OK] $*${NC}"; }

# emit <label> <expected_bytes> <helper args...>
# Generates a payload, checks it is non-empty and exactly the size of the
# contract's input struct, then prints it. Never claims on-chain behaviour.
emit() {
    local label="$1" expected="$2"; shift 2
    local out payload
    if ! out="$("$HELPER" "$@" 2>&1)"; then
        fail "$label: helper failed: $(echo "$out" | head -1)"
        return
    fi
    payload="$(echo "$out" | awk '/Payload Hex:/{print $3}')"
    if [ -z "$payload" ]; then
        fail "$label: no payload produced ($(echo "$out" | head -1))"
        return
    fi
    if [ "${#payload}" -ne $((expected * 2)) ]; then
        fail "$label: payload is ${#payload} hex chars, expected $((expected * 2)) (${expected}-byte input struct)"
        return
    fi
    echo "  payload : $payload"
    generated=$((generated + 1))
    ok "$label — ${expected}-byte payload"
}

echo -e "${BLUE}=== QHandle testnet payload generator ===${NC}"
echo -e "${YELLOW}Generates and size-checks payloads only. No node is contacted;"
echo -e "no on-chain behaviour is verified. See the header of this file.${NC}\n"

# ---- preconditions --------------------------------------------------------
[ -x "$HELPER" ] || {
    echo "Building helper..."
    g++ -std=c++20 -march=native -w -DNO_UEFI -DGENERIC_K12 \
        -I "$PROJECT_ROOT/core/src" -I "$PROJECT_ROOT/core" \
        -o "$HELPER" "$DIR/qhandle_cli_helper.cpp"
}
[ -x "$QUBIC_CLI" ] || { echo -e "${RED}qubic-cli not found at $QUBIC_CLI. Set QUBIC_CLI=...${NC}" >&2; exit 1; }

# ---- identity derivation --------------------------------------------------
# Hard failure if this does not work. The previous version fell back to 60 'A's,
# which decodes to NULL_ID in base-26 - and RegisterHandle rejects a NULL_ID
# target, so every payload built from it was invalid by construction.
derive_identity() {
    local seed="$1" name="$2" id
    id="$("$QUBIC_CLI" -seed "$seed" -showkeys 2>/dev/null | awk '/^Identity:/{print $2}')" || true
    if [ -z "$id" ] || [ "${#id}" -ne 60 ] || [[ ! "$id" =~ ^[A-Z]{60}$ ]]; then
        echo -e "${RED}Could not derive $name's identity from its seed.${NC}" >&2
        echo -e "${RED}Expected 60 uppercase chars from '\$QUBIC_CLI -seed <SEED> -showkeys', got: '${id}'${NC}" >&2
        exit 1
    fi
    printf '%s' "$id"
}

echo -e "${BLUE}--- Identities ---${NC}"
ALICE_ID="$(derive_identity "$ALICE_SEED" Alice)"
BOB_ID="$(derive_identity "$BOB_SEED" Bob)"
echo "  Alice : $ALICE_ID"
echo "  Bob   : $BOB_ID"
echo "  contract index: $CONTRACT_INDEX"
echo

# ---- commitment -----------------------------------------------------------
echo -e "${BLUE}--- Commitment for '$HANDLE' (Alice) ---${NC}"
COMMIT_OUT="$("$HELPER" compute-commitment "$HANDLE" "$ALICE_ID" "$SALT")"
COMMIT_HEX="$(echo "$COMMIT_OUT" | awk '/Commitment Hex:/{print $3}')"
[ -n "$COMMIT_HEX" ] || { echo -e "${RED}commitment computation failed${NC}" >&2; exit 1; }
echo "  commitment: $COMMIT_HEX"
echo

# ---- payloads -------------------------------------------------------------
# Sizes are the contract's input struct sizes, from tests/measure_sizes.cpp.
echo -e "${BLUE}--- Payload generation ---${NC}"
emit "CommitRegistration (proc 1)" 32 encode-commit "$COMMIT_HEX"
emit "RegisterHandle     (proc 2)" 80 encode-register "$HANDLE" "$ALICE_ID" "$SALT" 1
emit "RenewHandle        (proc 3)" 36 encode-renew "$HANDLE" 2
emit "TransferHandle     (proc 4)" 64 encode-transfer "$HANDLE" "$BOB_ID"
emit "SetTarget          (proc 5)" 64 encode-set-target "$HANDLE" "$BOB_ID"
emit "SetPrimary         (proc 6)" 33 encode-set-primary "$HANDLE" 0
emit "SetLock            (proc 7)" 33 encode-set-lock "$HANDLE" 1
emit "ReclaimExpired     (proc 8)" 32 encode-reclaim "$HANDLE"
emit "PruneCommitment    (proc 9)" 64 encode-prune "$ALICE_ID" "$COMMIT_HEX"
emit "ResolveHandle      (func 2)" 32 encode-resolve "$HANDLE"
emit "ReverseResolve     (func 3)" 32 encode-reverse-resolve "$ALICE_ID"
echo

# ---- commands -------------------------------------------------------------
# Seeds are deliberately shown as placeholders, never interpolated.
echo -e "${BLUE}--- Commands to run against a node (seeds shown as placeholders) ---${NC}"
REG_PAYLOAD="$("$HELPER" encode-register "$HANDLE" "$ALICE_ID" "$SALT" 1 | awk '/Payload Hex:/{print $3}')"
COMMIT_PAYLOAD="$("$HELPER" encode-commit "$COMMIT_HEX" | awk '/Payload Hex:/{print $3}')"
RESOLVE_PAYLOAD="$("$HELPER" encode-resolve "$HANDLE" | awk '/Payload Hex:/{print $3}')"
cat <<CMDS
  # 1. Alice commits (free), then waits >= 5 ticks
  $QUBIC_CLI -seed <ALICE_SEED> -nodeip $NODE_IP -nodeport $NODE_PORT \\
      -sendcustomtransaction $CONTRACT_INDEX 0 1 $COMMIT_PAYLOAD

  # 2. Alice reveals and registers (20,000,000 qu for 1 year, 5+ char handle)
  $QUBIC_CLI -seed <ALICE_SEED> -nodeip $NODE_IP -nodeport $NODE_PORT \\
      -sendcustomtransaction $CONTRACT_INDEX 20000000 2 $REG_PAYLOAD

  # 3. Anyone resolves (free, read-only)
  $QUBIC_CLI -nodeip $NODE_IP -nodeport $NODE_PORT \\
      -sendcustomfunction $CONTRACT_INDEX 2 $RESOLVE_PAYLOAD
  # decode the response with:
  $HELPER decode-resolve-response <HEX_RESPONSE>
CMDS
echo

# ---- summary --------------------------------------------------------------
echo -e "${BLUE}=====================================================${NC}"
if [ "$failures" -eq 0 ]; then
    echo -e "${GREEN}$generated payloads generated, all correctly sized.${NC}"
    echo -e "${YELLOW}Not verified: on-chain behaviour. No node was contacted.${NC}"
else
    echo -e "${RED}$generated generated, $failures FAILED.${NC}"
fi
echo -e "${BLUE}=====================================================${NC}"
exit $((failures > 0 ? 1 : 0))
