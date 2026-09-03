// Standalone test for QHANDLE::_Canonicalize, the security-critical entry point.
//
// Every name reaching the registry passes through it. A bug here means either two distinct
// keys that render identically (a phishing primitive in an address resolver) or the same
// name hashing to two different buckets.
//
// _Canonicalize touches neither `qpi` nor `state`, so it is exercised directly here without
// standing up the full contract execution harness in core/test.
//
// Build:  g++ -std=c++20 -mavx2 -w -I ../core/src -I ../core -I ../src -o test_canonicalize test_canonicalize.cpp

#define NO_UEFI 1
#include "contract_core/pre_qpi_def.h"
#include "qpi/qpi.h"

#define QHANDLE_CONTRACT_INDEX 99
#define CONTRACT_INDEX QHANDLE_CONTRACT_INDEX
#define CONTRACT_STATE_TYPE QHANDLE
#define CONTRACT_STATE2_TYPE QHANDLE2
#include "QHandle.h"

#include <cstdio>
#include <cstring>
#include <string>

struct Harness : public QHANDLE
{
    static bool run(const std::string& raw, std::string& canonical, unsigned& length)
    {
        _Canonicalize_input in{};
        _Canonicalize_output out{};
        _Canonicalize_locals loc{};

        for (size_t i = 0; i < raw.size() && i < 32; ++i)
            in.raw.set(i, (QPI::uint8)raw[i]);

        __impl__Canonicalize(
            *(const QPI::QpiContextFunctionCall*)nullptr,
            *(const QPI::ContractState<QHANDLE::StateData, 99>*)nullptr,
            in, out, loc);

        canonical.clear();
        for (unsigned i = 0; i < 32; ++i)
        {
            QPI::uint8 c = out.name.chars.get(i);
            canonical.push_back(c == 0 ? '.' : (char)c);   // '.' renders the zero padding
        }
        length = out.length;
        return out.valid;
    }
};

static int failures = 0;
static int checks = 0;

static void expectValid(const std::string& raw, const std::string& wantChars, unsigned wantLen)
{
    ++checks;
    std::string canon; unsigned len = 0;
    bool ok = Harness::run(raw, canon, len);
    std::string want = wantChars + std::string(32 - wantChars.size(), '.');
    if (!ok || canon != want || len != wantLen)
    {
        printf("FAIL  accept(%-34s) -> valid=%d len=%u canon=[%s]\n            want valid=1 len=%u canon=[%s]\n",
               raw.c_str(), (int)ok, len, canon.c_str(), wantLen, want.c_str());
        ++failures;
    }
}

static void expectRejected(const std::string& raw, const char* why)
{
    ++checks;
    std::string canon; unsigned len = 0;
    if (Harness::run(raw, canon, len))
    {
        printf("FAIL  reject(%-34s) -> was ACCEPTED as [%s]   (%s)\n", raw.c_str(), canon.c_str(), why);
        ++failures;
    }
}

int main()
{
    // --- accepted ---
    expectValid("alice",                          "alice", 5);
    expectValid("abc",                            "abc", 3);                    // minimum length
    expectValid("a1b2c3",                         "a1b2c3", 6);
    expectValid("my-handle",                      "my-handle", 9);
    expectValid("a-b-c",                          "a-b-c", 5);
    expectValid("12345",                          "12345", 5);
    expectValid("abcdefghijklmnopqrstuvwxyz012345", "abcdefghijklmnopqrstuvwxyz012345", 32); // exactly max

    // --- case folding: Alice and alice must be the SAME key, not two registrations ---
    expectValid("Alice",                          "alice", 5);
    expectValid("ALICE",                          "alice", 5);
    expectValid("aLiCe",                          "alice", 5);

    // --- length bounds ---
    expectRejected("",                            "empty");
    expectRejected("a",                           "1 char, below minimum");
    expectRejected("ab",                          "2 chars, below minimum");

    // --- charset ---
    expectRejected("alice.qubic",                 "dot not in charset");
    expectRejected("alice_bob",                   "underscore not in charset");
    expectRejected("alice bob",                   "space");
    expectRejected("alice!",                      "punctuation");
    expectRejected("alice@qubic",                 "at sign");
    expectRejected("\xC3\xA9lise",                "UTF-8 / non-ASCII must be rejected");
    expectRejected("\xF0\x9F\x98\x80\x61\x62\x63","emoji must be rejected");
    expectRejected("alice\x7F",                   "DEL control char");

    // --- hyphen placement ---
    expectRejected("-alice",                      "leading hyphen");
    expectRejected("alice-",                      "trailing hyphen");
    expectRejected("al--ice",                     "doubled hyphen");
    expectRejected("---",                         "all hyphens");

    // --- the aliasing attacks: two byte patterns that must not both be storable ---
    // "abc\0def" would render as "abc" but hash differently from a clean "abc".
    {
        ++checks;
        std::string raw = "abc"; raw.push_back('\0'); raw += "def";
        std::string canon; unsigned len = 0;
        if (Harness::run(raw, canon, len))
        {
            printf("FAIL  reject(abc\\0def) -> ACCEPTED as [%s]  (embedded NUL aliases a shorter name)\n", canon.c_str());
            ++failures;
        }
    }
    // A name at exactly max length with trailing garbage past the array is impossible, but a
    // short name whose padding is dirty must not be accepted as a distinct key.
    {
        ++checks;
        std::string raw = "alice"; raw.push_back('\0'); raw.push_back('\0'); raw += "x";
        std::string canon; unsigned len = 0;
        if (Harness::run(raw, canon, len))
        {
            printf("FAIL  reject(alice\\0\\0x) -> ACCEPTED as [%s]  (dirty padding aliases 'alice')\n", canon.c_str());
            ++failures;
        }
    }

    // --- canonical form is idempotent and collision-free for the case variants ---
    {
        ++checks;
        std::string a, b, c; unsigned la, lb, lc;
        Harness::run("Alice", a, la);
        Harness::run("alice", b, lb);
        Harness::run("ALICE", c, lc);
        if (a != b || b != c)
        {
            printf("FAIL  case variants produced different canonical keys\n");
            ++failures;
        }
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
