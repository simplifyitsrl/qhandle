#define NO_UEFI 1
#define GENERIC_K12 1
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cstdlib>

void setMem(void* buffer, unsigned long long size, unsigned char value) { memset(buffer, value, size); }
void copyMem(void* destination, const void* source, unsigned long long length) { memcpy(destination, source, length); }

#include "platform/memory.h"
#include "kangaroo_twelve.h"

// Base-26 Qubic identity decoding / encoding
static bool decodeIdentity(const std::string& identity, uint8_t* publicKey)
{
    if (identity.length() == 64) {
        // Hex string format
        for (int i = 0; i < 32; ++i) {
            std::string byteStr = identity.substr(i * 2, 2);
            publicKey[i] = (uint8_t)strtoul(byteStr.c_str(), nullptr, 16);
        }
        return true;
    }
    if (identity.length() != 60) {
        return false;
    }
    unsigned char publicKeyBuffer[32];
    for (int i = 0; i < 4; i++) {
        *((unsigned long long*)&publicKeyBuffer[i << 3]) = 0;
        for (int j = 14; j-- > 0; ) {
            char c = identity[i * 14 + j];
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
            if (c < 'A' || c > 'Z') return false;
            *((unsigned long long*)&publicKeyBuffer[i << 3]) = *((unsigned long long*)&publicKeyBuffer[i << 3]) * 26 + (c - 'A');
        }
    }
    memcpy(publicKey, publicKeyBuffer, 32);
    return true;
}

static std::string encodeIdentity(const uint8_t* pubkey)
{
    uint8_t publicKey[32];
    memcpy(publicKey, pubkey, 32);
    char identity[61] = {0};
    for (int i = 0; i < 4; i++) {
        unsigned long long fragment = *((unsigned long long*)&publicKey[i << 3]);
        for (int j = 0; j < 14; j++) {
            identity[i * 14 + j] = (char)(fragment % 26 + 'A');
            fragment /= 26;
        }
    }
    unsigned int checksum = 0;
    KangarooTwelve(publicKey, 32, (uint8_t*)&checksum, 3);
    checksum &= 0x3FFFF;
    for (int i = 0; i < 4; i++) {
        identity[56 + i] = (char)(checksum % 26 + 'A');
        checksum /= 26;
    }
    identity[60] = 0;
    return std::string(identity);
}

static std::string toHex(const uint8_t* data, size_t len)
{
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

static bool fromHex(const std::string& hex, std::vector<uint8_t>& out)
{
    if (hex.length() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        out.push_back((uint8_t)strtoul(byteStr.c_str(), nullptr, 16));
    }
    return true;
}

static void canonicalizeName(const std::string& raw, uint8_t* out)
{
    memset(out, 0, 32);
    for (size_t i = 0; i < raw.length() && i < 32; ++i) {
        char c = raw[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        out[i] = (uint8_t)c;
    }
}

#include "contract_core/pre_qpi_def.h"
#include "qpi/qpi.h"
#define QHANDLE_CONTRACT_INDEX 99
#define CONTRACT_INDEX QHANDLE_CONTRACT_INDEX
#define CONTRACT_STATE_TYPE QHANDLE
#define CONTRACT_STATE2_TYPE QHANDLE2
#include "QHandle.h"

using CommitmentPreimage = QHANDLE::CommitmentPreimage;
using CommitRegistration_input = QHANDLE::CommitRegistration_input;
using RegisterHandle_input = QHANDLE::RegisterHandle_input;
using RenewHandle_input = QHANDLE::RenewHandle_input;
using TransferHandle_input = QHANDLE::TransferHandle_input;
using SetTarget_input = QHANDLE::SetTarget_input;
using SetPrimary_input = QHANDLE::SetPrimary_input;
using SetLock_input = QHANDLE::SetLock_input;
using ReclaimExpired_input = QHANDLE::ReclaimExpired_input;
using PruneCommitment_input = QHANDLE::PruneCommitment_input;
using ResolveHandle_input = QHANDLE::ResolveHandle_input;
using ReverseResolve_input = QHANDLE::ReverseResolve_input;

static void printUsage(const char* prog)
{
    std::cout << "QHandle CLI Helper & Payload Encoder\n"
              << "Usage: " << prog << " <command> [arguments...]\n\n"
              << "Commands:\n"
              << "  compute-commitment <name> <owner_identity_or_hex> <salt>\n"
              << "  encode-commit <commitment_hex_or_identity>\n"
              << "  encode-register <name> <target_identity_or_hex> <salt> <years>\n"
              << "  encode-renew <name> <years>\n"
              << "  encode-transfer <name> <new_owner_identity_or_hex>\n"
              << "  encode-set-target <name> <new_target_identity_or_hex>\n"
              << "  encode-set-primary <name> [clear=0|1]\n"
              << "  encode-set-lock <name> <locked=0|1>\n"
              << "  encode-reclaim <name>\n"
              << "  encode-prune <committer_identity_or_hex> <commitment_hex_or_identity>\n"
              << "  encode-resolve <name>\n"
              << "  encode-reverse-resolve <identity_or_hex>\n"
              << "  decode-resolve-response <hex_data>\n"
              << "  decode-reverse-resolve-response <hex_data>\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "compute-commitment") {
        if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " compute-commitment <name> <owner_identity_or_hex> <salt>\n";
            return 1;
        }
        CommitmentPreimage preimage{};
        canonicalizeName(argv[2], (uint8_t*)&preimage.name);
        if (!decodeIdentity(argv[3], (uint8_t*)&preimage.owner)) {
            std::cerr << "Error: invalid owner identity or hex: " << argv[3] << "\n";
            return 1;
        }
        preimage.salt = strtoull(argv[4], nullptr, 0);

        uint8_t commitment[32];
        KangarooTwelve((const uint8_t*)&preimage, sizeof(preimage), commitment, 32);

        std::cout << "Commitment Hex:      " << toHex(commitment, 32) << "\n";
        std::cout << "Commitment Identity: " << encodeIdentity(commitment) << "\n";
        return 0;
    }

    if (cmd == "encode-commit") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " encode-commit <commitment_hex_or_identity>\n";
            return 1;
        }
        CommitRegistration_input in{};
        if (!decodeIdentity(argv[2], (uint8_t*)&in.commitment)) {
            std::cerr << "Error: invalid commitment: " << argv[2] << "\n";
            return 1;
        }
        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   1 (CommitRegistration)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 1 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-register") {
        if (argc < 6) {
            std::cerr << "Usage: " << argv[0] << " encode-register <name> <target_identity_or_hex> <salt> <years>\n";
            return 1;
        }
        RegisterHandle_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);
        if (!decodeIdentity(argv[3], (uint8_t*)&in.target)) {
            std::cerr << "Error: invalid target identity: " << argv[3] << "\n";
            return 1;
        }
        in.salt = strtoull(argv[4], nullptr, 0);
        in.years = (uint16_t)atoi(argv[5]);

        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::string nameStr = argv[2];
        uint64_t fee = 20000000ULL * in.years;
        if (nameStr.length() == 3) fee = 1000000000ULL * in.years;
        else if (nameStr.length() == 4) fee = 200000000ULL * in.years;

        std::cout << "Procedure:   2 (RegisterHandle)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Fee (QUBIC): " << fee << "\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> " << fee << " 2 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-renew") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " encode-renew <name> <years>\n";
            return 1;
        }
        RenewHandle_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);
        in.years = (uint16_t)atoi(argv[3]);

        uint64_t fee = 20000000ULL * in.years;
        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   3 (RenewHandle)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Fee (QUBIC): " << fee << "\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> " << fee << " 3 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-transfer") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " encode-transfer <name> <new_owner_identity_or_hex>\n";
            return 1;
        }
        TransferHandle_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);
        if (!decodeIdentity(argv[3], (uint8_t*)&in.newOwner)) {
            std::cerr << "Error: invalid new owner: " << argv[3] << "\n";
            return 1;
        }
        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   4 (TransferHandle)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Fee (QUBIC): 1000000\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 1000000 4 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-set-target") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " encode-set-target <name> <new_target_identity_or_hex>\n";
            return 1;
        }
        SetTarget_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);
        if (!decodeIdentity(argv[3], (uint8_t*)&in.newTarget)) {
            std::cerr << "Error: invalid new target: " << argv[3] << "\n";
            return 1;
        }
        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   5 (SetTarget)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 5 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-set-primary") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " encode-set-primary <name> [clear=0|1]\n";
            return 1;
        }
        SetPrimary_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);
        in.clear = (argc >= 4) ? (uint8_t)atoi(argv[3]) : 0;

        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   6 (SetPrimary)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 6 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-set-lock") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " encode-set-lock <name> <locked=0|1>\n";
            return 1;
        }
        SetLock_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);
        in.locked = (uint8_t)atoi(argv[3]);

        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   7 (SetLock)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 7 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-reclaim") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " encode-reclaim <name>\n";
            return 1;
        }
        ReclaimExpired_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);

        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   8 (ReclaimExpired)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 8 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-prune") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " encode-prune <committer_identity_or_hex> <commitment_hex_or_identity>\n";
            return 1;
        }
        PruneCommitment_input in{};
        if (!decodeIdentity(argv[2], (uint8_t*)&in.committer)) {
            std::cerr << "Error: invalid committer: " << argv[2] << "\n";
            return 1;
        }
        if (!decodeIdentity(argv[3], (uint8_t*)&in.commitment)) {
            std::cerr << "Error: invalid commitment: " << argv[3] << "\n";
            return 1;
        }

        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Procedure:   9 (PruneCommitment)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        std::cout << "CLI Command: ./qubic-cli -sendcustomtransaction <SEED> <CONTRACT_INDEX> 0 9 " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-resolve") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " encode-resolve <name>\n";
            return 1;
        }
        ResolveHandle_input in{};
        canonicalizeName(argv[2], (uint8_t*)&in.name);
        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Function:    2 (ResolveHandle)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        return 0;
    }

    if (cmd == "encode-reverse-resolve") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " encode-reverse-resolve <identity_or_hex>\n";
            return 1;
        }
        ReverseResolve_input in{};
        if (!decodeIdentity(argv[2], (uint8_t*)&in.addr)) {
            std::cerr << "Error: invalid address: " << argv[2] << "\n";
            return 1;
        }
        std::string hex = toHex((const uint8_t*)&in, sizeof(in));
        std::cout << "Function:    3 (ReverseResolve)\n";
        std::cout << "Input Size:  " << sizeof(in) << " bytes\n";
        std::cout << "Payload Hex: " << hex << "\n";
        return 0;
    }

    if (cmd == "decode-resolve-response") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " decode-resolve-response <hex_data>\n";
            return 1;
        }
        std::vector<uint8_t> bytes;
        if (!fromHex(argv[2], bytes) || bytes.size() < 71) {
            std::cerr << "Error: response data too short (need >= 71 bytes)\n";
            return 1;
        }
        uint8_t target[32], owner[32];
        memcpy(target, bytes.data(), 32);
        memcpy(owner, bytes.data() + 32, 32);
        uint16_t regEpoch = *(uint16_t*)(bytes.data() + 64);
        uint16_t expEpoch = *(uint16_t*)(bytes.data() + 66);
        uint8_t len = bytes[68];
        uint8_t locked = bytes[69];
        uint8_t status = bytes[70];

        std::cout << "Status:           " << (int)status << "\n";
        std::cout << "Target Identity:  " << encodeIdentity(target) << " (" << toHex(target, 32) << ")\n";
        std::cout << "Owner Identity:   " << encodeIdentity(owner) << " (" << toHex(owner, 32) << ")\n";
        std::cout << "Registered Epoch: " << regEpoch << "\n";
        std::cout << "Expiry Epoch:     " << expEpoch << "\n";
        std::cout << "Length:           " << (int)len << "\n";
        std::cout << "Locked:           " << (locked ? "yes" : "no") << "\n";
        return 0;
    }

    if (cmd == "decode-reverse-resolve-response") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " decode-reverse-resolve-response <hex_data>\n";
            return 1;
        }
        std::vector<uint8_t> bytes;
        if (!fromHex(argv[2], bytes) || bytes.size() < 34) {
            std::cerr << "Error: response data too short (need >= 34 bytes)\n";
            return 1;
        }
        char nameBuf[33] = {0};
        memcpy(nameBuf, bytes.data(), 32);
        uint8_t len = bytes[32];
        uint8_t status = bytes[33];
        if (len < 32) nameBuf[len] = 0;

        std::cout << "Status: " << (int)status << "\n";
        std::cout << "Name:   " << nameBuf << "\n";
        std::cout << "Length: " << (int)len << "\n";
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    printUsage(argv[0]);
    return 1;
}
