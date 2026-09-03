#define NO_UEFI 1
#define GENERIC_K12 1
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

#if !defined(_MSC_VER)
static inline unsigned long long _umul128(unsigned long long a, unsigned long long b, unsigned long long* hi) {
    unsigned __int128 r = (unsigned __int128)a * b;
    *hi = (unsigned long long)(r >> 64);
    return (unsigned long long)r;
}
static inline long long _mul128(long long a, long long b, long long* hi) {
    __int128 r = (__int128)a * b;
    *hi = (long long)(r >> 64);
    return (long long)r;
}
#endif

void setMem(void* buffer, unsigned long long size, unsigned char value) { memset(buffer, value, size); }
void copyMem(void* destination, const void* source, unsigned long long length) { memcpy(destination, source, length); }

#include "platform/memory.h"
#include "kangaroo_twelve.h"

#include "contract_core/pre_qpi_def.h"
#include "qpi/qpi.h"

namespace QPI {
    template <typename T1, typename T2>
    inline void copyMemory(T1& dst, const T2& src) {
        memcpy(&dst, &src, sizeof(T1));
    }
}

static void __markContractStateDirty(unsigned int) {}
static void __beginFunctionOrProcedure(unsigned int) {}
static void __endFunctionOrProcedure(unsigned int) {}

// Test environment simulating QPI network state
struct TestEnv {
    uint32_t tick = 1000;
    uint16_t epoch = 10;
    QPI::id invocator = {1, 0, 0, 0};
    int64_t reward = 0;
    std::vector<std::pair<QPI::id, int64_t>> transfers;
    int64_t burned = 0;
    int64_t dividends = 0;
    int64_t feeReserve = 5000000000;

    void reset(uint32_t t = 1000, uint16_t ep = 10) {
        tick = t;
        epoch = ep;
        reward = 0;
        transfers.clear();
        burned = 0;
        dividends = 0;
    }
} g_env;

static char g_localsBuffer[65536];
void* QPI::QpiContextFunctionCall::__qpiAllocLocals(unsigned int sizeOfLocals) const {
    memset(g_localsBuffer, 0, sizeOfLocals);
    return g_localsBuffer;
}
void* __acquireScratchpad(unsigned long long size, bool initZero) {
    void* p = malloc(size);
    if (initZero && p) memset(p, 0, size);
    return p;
}
void __releaseScratchpad(void* ptr) {
    free(ptr);
}
void QPI::QpiContextFunctionCall::__qpiFreeLocals() const {}
void addDebugMessageAssert(const char*, const char*, unsigned int) {}

uint16_t QPI::QpiContextFunctionCall::epoch() const { return g_env.epoch; }
uint32_t QPI::QpiContextFunctionCall::tick() const { return g_env.tick; }
QPI::sint64 QPI::QpiContextFunctionCall::queryFeeReserve(uint32_t) const { return g_env.feeReserve; }

template <typename T>
QPI::id QPI::QpiContextFunctionCall::K12(const T& data) const {
    QPI::id digest;
    KangarooTwelve(&data, sizeof(data), &digest, 32);
    return digest;
}

QPI::sint64 QPI::QpiContextProcedureCall::transfer(const QPI::id& dest, QPI::sint64 amount) const {
    g_env.transfers.push_back({dest, amount});
    return amount;
}
QPI::sint64 QPI::QpiContextProcedureCall::burn(QPI::sint64 amount, uint32_t) const {
    g_env.burned += amount;
    return amount;
}
bool QPI::QpiContextProcedureCall::distributeDividends(QPI::sint64 perShare) const {
    g_env.dividends += perShare;
    return true;
}

struct TestProcedureCallContext : public QPI::QpiContextProcedureCall
{
    TestProcedureCallContext(unsigned int contractIndex, const QPI::id& originator, long long invocationReward, unsigned char entryPoint)
        : QPI::QpiContextProcedureCall(contractIndex, originator, invocationReward, entryPoint) {}
};

struct TestFunctionCallContext : public QPI::QpiContextFunctionCall
{
    TestFunctionCallContext(unsigned int contractIndex, const QPI::id& originator, long long invocationReward, unsigned char entryPoint)
        : QPI::QpiContextFunctionCall(contractIndex, originator, invocationReward, entryPoint) {}
};

#define QHANDLE_CONTRACT_INDEX 99
#define CONTRACT_INDEX QHANDLE_CONTRACT_INDEX
#define CONTRACT_STATE_TYPE QHANDLE
#define CONTRACT_STATE2_TYPE QHANDLE2
#include "QHandle.h"
#include "qpi/impl/qpi_hash_map_impl.h"

// Helper Harness exposing contract procedures and functions directly
struct TestHarness : public QHANDLE
{
    static QPI::Array<QPI::uint8, 32> makeNameArray(const std::string& str) {
        QPI::Array<QPI::uint8, 32> arr{};
        for (size_t i = 0; i < str.size() && i < 32; ++i) {
            arr.set(i, (QPI::uint8)str[i]);
        }
        return arr;
    }

    static QPI::id makeId(uint64_t a, uint64_t b = 0, uint64_t c = 0, uint64_t d = 0) {
        return QPI::id(a, b, c, d);
    }
};

#define CALL_PROC(procName, in, out, loc) do { \
    TestProcedureCallContext _pCtx(99, g_env.invocator, g_env.reward, 0); \
    QHANDLE::__impl_##procName(_pCtx, state, in, out, loc); \
} while(0)

#define CALL_FUNC(funcName, in, out, loc) do { \
    TestFunctionCallContext _fCtx(99, g_env.invocator, 0, 0); \
    QHANDLE::__impl_##funcName(_fCtx, state, in, out, loc); \
} while(0)

static int totalTests = 0;
static int failedTests = 0;

#define TEST_ASSERT(cond, msg) do { \
    totalTests++; \
    if (!(cond)) { \
        std::cerr << "FAILED: " << msg << " (line " << __LINE__ << ")" << std::endl; \
        failedTests++; \
    } \
} while(0)

int main()
{
    std::cout << "Starting QHandle Stateful Integration Tests..." << std::endl;

    // Allocate contract state on heap
    auto* statePtr = new QPI::ContractState<QHANDLE::StateData, 99>();
    memset(statePtr, 0, sizeof(*statePtr));
    auto& state = *statePtr;

    const QPI::id ALICE = TestHarness::makeId(101);
    const QPI::id BOB = TestHarness::makeId(102);
    const QPI::id CHARLIE = TestHarness::makeId(103);
    const QPI::id TARGET_ALICE = TestHarness::makeId(201);
    const QPI::id TARGET_BOB = TestHarness::makeId(202);

    // =========================================================================
    // TEST 1: Fees & Stats Read-Only Functions
    // =========================================================================
    {
        QHANDLE::Fees_input in{};
        QHANDLE::Fees_output out{};
        QHANDLE::Fees_locals loc{};
        CALL_FUNC(Fees, in, out, loc);
        TEST_ASSERT(out.registrationFeePerYearLength3 == 1000000000, "3-char reg fee");
        TEST_ASSERT(out.registrationFeePerYearLength4 == 200000000, "4-char reg fee");
        TEST_ASSERT(out.registrationFeePerYearLength5Plus == 20000000, "5+-char reg fee");
        TEST_ASSERT(out.renewalFeePerYear == 20000000, "Renewal fee");
        TEST_ASSERT(out.maxYears == 5, "Max years is 5");
        TEST_ASSERT(out.graceEpochs == 13, "Grace epochs is 13");

        QHANDLE::Stats_input sin{};
        QHANDLE::Stats_output sout{};
        QHANDLE::Stats_locals sloc{};
        CALL_FUNC(Stats, sin, sout, sloc);
        TEST_ASSERT(sout.handlePopulation == 0, "Initial population 0");
        TEST_ASSERT(sout.handleCapacity == QHANDLE_MAX_USABLE_CAPACITY, "Capacity is soft cap 104857");
    }

    // =========================================================================
    // TEST 2: Commit-Reveal & Front-Running Defense
    // =========================================================================
    {
        g_env.reset(1000, 10);
        g_env.invocator = ALICE;

        // Alice computes commitment for "alice"
        QHANDLE::ComputeCommitment_input cin{};
        cin.name = TestHarness::makeNameArray("alice");
        cin.owner = ALICE;
        cin.salt = 999999;
        QHANDLE::ComputeCommitment_output cout{};
        QHANDLE::ComputeCommitment_locals cloc{};
        CALL_FUNC(ComputeCommitment, cin, cout, cloc);
        TEST_ASSERT(cout.status == QHANDLE_OK, "ComputeCommitment ok");
        QPI::id aliceCommitment = cout.commitment;
        TEST_ASSERT(aliceCommitment != NULL_ID, "Alice commitment non-null");

        // Alice commits
        QHANDLE::CommitRegistration_input commitIn{ aliceCommitment };
        QHANDLE::CommitRegistration_output commitOut{};
        QHANDLE::CommitRegistration_locals commitLoc{};
        CALL_PROC(CommitRegistration, commitIn, commitOut, commitLoc);
        TEST_ASSERT(commitOut.status == QHANDLE_OK, "Alice CommitRegistration ok");
        TEST_ASSERT(commitOut.commitTick == 1000, "Commit tick is 1000");

        // Bob observes Alice's commitment hash and attempts to front-run by committing the same hash
        g_env.invocator = BOB;
        QHANDLE::CommitRegistration_output bobCommitOut{};
        QHANDLE::CommitRegistration_locals bobCommitLoc{};
        CALL_PROC(CommitRegistration, commitIn, bobCommitOut, bobCommitLoc);
        TEST_ASSERT(bobCommitOut.status == QHANDLE_OK, "Bob commit succeeds under Bob's slot");

        // Advance ticks to 1020 (window: min 5, max 10000)
        g_env.tick = 1020;

        // Bob tries to reveal using Alice's name and salt
        g_env.invocator = BOB;
        g_env.reward = 20000000; // 1 year fee
        QHANDLE::RegisterHandle_input bobRegIn{};
        bobRegIn.name = TestHarness::makeNameArray("alice");
        bobRegIn.target = TARGET_BOB;
        bobRegIn.salt = 999999;
        bobRegIn.years = 1;
        QHANDLE::RegisterHandle_output bobRegOut{};
        QHANDLE::RegisterHandle_locals bobRegLoc{};
        CALL_PROC(RegisterHandle, bobRegIn, bobRegOut, bobRegLoc);
        TEST_ASSERT(bobRegOut.status == QHANDLE_ERR_NO_COMMITMENT, "Bob cannot reveal Alice's commitment!");

        // Alice reveals and registers
        g_env.invocator = ALICE;
        g_env.reward = 25000000; // overpaying by 5,000,000
        QHANDLE::RegisterHandle_input aliceRegIn{};
        aliceRegIn.name = TestHarness::makeNameArray("alice");
        aliceRegIn.target = TARGET_ALICE;
        aliceRegIn.salt = 999999;
        aliceRegIn.years = 1;
        QHANDLE::RegisterHandle_output aliceRegOut{};
        QHANDLE::RegisterHandle_locals aliceRegLoc{};
        CALL_PROC(RegisterHandle, aliceRegIn, aliceRegOut, aliceRegLoc);
        TEST_ASSERT(aliceRegOut.status == QHANDLE_OK, "Alice registration succeeds!");
        TEST_ASSERT(aliceRegOut.feePaid == 20000000, "Alice charged 20M");
        TEST_ASSERT(aliceRegOut.expiryEpoch == 10 + 52, "Alice expiryEpoch is 62");

        // Verify refund: 5,000,000 refunded to Alice
        TEST_ASSERT(!g_env.transfers.empty(), "Refund transfer was executed");
        TEST_ASSERT(g_env.transfers.back().first == ALICE, "Refunded to Alice");
        TEST_ASSERT(g_env.transfers.back().second == 5000000, "Refunded 5,000,000 excess");
    }

    // =========================================================================
    // TEST 3: Handle Resolution
    // =========================================================================
    {
        // Resolve "alice"
        QHANDLE::ResolveHandle_input rin{ TestHarness::makeNameArray("alice") };
        QHANDLE::ResolveHandle_output rout{};
        QHANDLE::ResolveHandle_locals rloc{};
        CALL_FUNC(ResolveHandle, rin, rout, rloc);
        TEST_ASSERT(rout.status == QHANDLE_OK, "Resolve alice OK");
        TEST_ASSERT(rout.target == TARGET_ALICE, "Resolved target is TARGET_ALICE");
        TEST_ASSERT(rout.owner == ALICE, "Resolved owner is ALICE");
        TEST_ASSERT(rout.length == 5, "Resolved length is 5");
        TEST_ASSERT(rout.expiryEpoch == 62, "Expiry epoch is 62");

        // Case-insensitivity: "ALICE" resolves to the exact same record
        QHANDLE::ResolveHandle_input rinUpper{ TestHarness::makeNameArray("ALICE") };
        QHANDLE::ResolveHandle_output routUpper{};
        QHANDLE::ResolveHandle_locals rlocUpper{};
        CALL_FUNC(ResolveHandle, rinUpper, routUpper, rlocUpper);
        TEST_ASSERT(routUpper.status == QHANDLE_OK, "Resolve ALICE (upper) OK");
        TEST_ASSERT(routUpper.target == TARGET_ALICE, "Resolved ALICE target matches");

        // Non-existent name
        QHANDLE::ResolveHandle_input rnone{ TestHarness::makeNameArray("nonexistent") };
        QHANDLE::ResolveHandle_output rnoneOut{};
        QHANDLE::ResolveHandle_locals rnoneLoc{};
        CALL_FUNC(ResolveHandle, rnone, rnoneOut, rnoneLoc);
        TEST_ASSERT(rnoneOut.status == QHANDLE_ERR_NOT_FOUND, "Nonexistent handle returns NOT_FOUND");
    }

    // =========================================================================
    // TEST 4: SetTarget & SetPrimary & ReverseResolve
    // =========================================================================
    {
        // Alice sets primary to "alice"
        g_env.invocator = ALICE;
        g_env.reward = 0;
        QHANDLE::SetPrimary_input spin{};
        spin.name = TestHarness::makeNameArray("alice");
        spin.clear = false;
        QHANDLE::SetPrimary_output spout{};
        QHANDLE::SetPrimary_locals sploc{};
        CALL_PROC(SetPrimary, spin, spout, sploc);
        TEST_ASSERT(spout.status == QHANDLE_OK, "SetPrimary OK");

        // Reverse resolve ALICE
        QHANDLE::ReverseResolve_input rrin{ ALICE };
        QHANDLE::ReverseResolve_output rrout{};
        QHANDLE::ReverseResolve_locals rrloc{};
        CALL_FUNC(ReverseResolve, rrin, rrout, rrloc);
        TEST_ASSERT(rrout.status == QHANDLE_OK, "ReverseResolve OK");
        TEST_ASSERT(rrout.length == 5, "Reverse name length 5");

        // Alice repoints target to a new hot address
        const QPI::id HOT_WALLET = TestHarness::makeId(888);
        QHANDLE::SetTarget_input stin{};
        stin.name = TestHarness::makeNameArray("alice");
        stin.newTarget = HOT_WALLET;
        QHANDLE::SetTarget_output stout{};
        QHANDLE::SetTarget_locals stloc{};
        CALL_PROC(SetTarget, stin, stout, stloc);
        TEST_ASSERT(stout.status == QHANDLE_OK, "SetTarget OK");

        // Check ResolveHandle returns new HOT_WALLET but owner remains ALICE
        QHANDLE::ResolveHandle_input rin{ TestHarness::makeNameArray("alice") };
        QHANDLE::ResolveHandle_output rout{};
        QHANDLE::ResolveHandle_locals rloc{};
        CALL_FUNC(ResolveHandle, rin, rout, rloc);
        TEST_ASSERT(rout.target == HOT_WALLET, "Resolved target is now HOT_WALLET");
        TEST_ASSERT(rout.owner == ALICE, "Resolved owner is still ALICE");

        // Bob cannot repoint Alice's target
        g_env.invocator = BOB;
        CALL_PROC(SetTarget, stin, stout, stloc);
        TEST_ASSERT(stout.status == QHANDLE_ERR_NOT_OWNER, "Bob cannot repoint Alice's handle");
    }

    // =========================================================================
    // TEST 5: SetLock & TransferHandle
    // =========================================================================
    {
        g_env.invocator = ALICE;
        // Alice locks her handle
        QHANDLE::SetLock_input lockIn{};
        lockIn.name = TestHarness::makeNameArray("alice");
        lockIn.locked = true;
        QHANDLE::SetLock_output lockOut{};
        QHANDLE::SetLock_locals lockLoc{};
        CALL_PROC(SetLock, lockIn, lockOut, lockLoc);
        TEST_ASSERT(lockOut.status == QHANDLE_OK, "SetLock(true) OK");

        // Attempting to transfer while locked must fail
        g_env.reward = 1000000; // transfer fee
        QHANDLE::TransferHandle_input tin{};
        tin.name = TestHarness::makeNameArray("alice");
        tin.newOwner = CHARLIE;
        QHANDLE::TransferHandle_output tout{};
        QHANDLE::TransferHandle_locals tloc{};
        CALL_PROC(TransferHandle, tin, tout, tloc);
        TEST_ASSERT(tout.status == QHANDLE_ERR_LOCKED, "Transfer fails when locked");

        // Unlock
        lockIn.locked = false;
        CALL_PROC(SetLock, lockIn, lockOut, lockLoc);
        TEST_ASSERT(lockOut.status == QHANDLE_OK, "SetLock(false) OK");

        // Transfer to Charlie succeeds
        g_env.reward = 1000000;
        CALL_PROC(TransferHandle, tin, tout, tloc);
        TEST_ASSERT(tout.status == QHANDLE_OK, "Transfer to Charlie OK");

        // Resolve verifies ownership changed and target followed ownership to Charlie
        QHANDLE::ResolveHandle_input rin{ TestHarness::makeNameArray("alice") };
        QHANDLE::ResolveHandle_output rout{};
        QHANDLE::ResolveHandle_locals rloc{};
        CALL_FUNC(ResolveHandle, rin, rout, rloc);
        TEST_ASSERT(rout.owner == CHARLIE, "Owner is now Charlie");
        TEST_ASSERT(rout.target == CHARLIE, "Target followed ownership to Charlie");

        // Old owner Alice's primary reverse record was cleared
        QHANDLE::ReverseResolve_input rrin{ ALICE };
        QHANDLE::ReverseResolve_output rrout{};
        QHANDLE::ReverseResolve_locals rrloc{};
        CALL_FUNC(ReverseResolve, rrin, rrout, rrloc);
        TEST_ASSERT(rrout.status == QHANDLE_ERR_NOT_FOUND, "Alice's primary record was dropped on transfer");
    }

    // =========================================================================
    // TEST 6: Renewal & Grace Period & Expiry
    // =========================================================================
    {
        // Handle "alice" currently expires at epoch 62
        // Advance time to epoch 60
        g_env.epoch = 60;

        // Bob renews "alice" for Charlie for 2 years (permissionless renewal)
        g_env.invocator = BOB;
        g_env.reward = 40000000; // 2 years * 20M
        QHANDLE::RenewHandle_input renIn{};
        renIn.name = TestHarness::makeNameArray("alice");
        renIn.years = 2;
        QHANDLE::RenewHandle_output renOut{};
        QHANDLE::RenewHandle_locals renLoc{};
        CALL_PROC(RenewHandle, renIn, renOut, renLoc);
        TEST_ASSERT(renOut.status == QHANDLE_OK, "Permissionless renewal succeeds");
        TEST_ASSERT(renOut.expiryEpoch == 62 + 52 * 2, "Expiry extended from 62 to 166");

        // Fast-forward past expiry (epoch 166) to epoch 170 (within grace period 13 epochs)
        g_env.epoch = 170;
        QHANDLE::ResolveHandle_input rin{ TestHarness::makeNameArray("alice") };
        QHANDLE::ResolveHandle_output rout{};
        QHANDLE::ResolveHandle_locals rloc{};
        CALL_FUNC(ResolveHandle, rin, rout, rloc);
        TEST_ASSERT(rout.status == QHANDLE_ERR_EXPIRED, "Expired handle resolves to ERR_EXPIRED");
        TEST_ASSERT(rout.target == NULL_ID, "Expired handle returns NULL_ID target");

        // While in grace period, cannot be reclaimed yet
        QHANDLE::ReclaimExpired_input recIn{ TestHarness::makeNameArray("alice") };
        QHANDLE::ReclaimExpired_output recOut{};
        QHANDLE::ReclaimExpired_locals recLoc{};
        CALL_PROC(ReclaimExpired, recIn, recOut, recLoc);
        TEST_ASSERT(recOut.status == QHANDLE_ERR_NOT_RECLAIMABLE, "Cannot reclaim within grace period");

        // Fast forward past grace period (166 + 13 = 179) -> epoch 180
        g_env.epoch = 180;
        CALL_PROC(ReclaimExpired, recIn, recOut, recLoc);
        TEST_ASSERT(recOut.status == QHANDLE_OK, "ReclaimExpired succeeds past grace period");

        // Name is now deleted from registry
        CALL_FUNC(ResolveHandle, rin, rout, rloc);
        TEST_ASSERT(rout.status == QHANDLE_ERR_NOT_FOUND, "Handle deleted after reclaim");
    }

    // =========================================================================
    // TEST 7: END_TICK Dividends and Burn
    // =========================================================================
    {
        g_env.reset(5000, 20);
        // Add revenue to earned amount
        state.mut()._earnedAmount = 100000000; // 100,000,000 QUBIC earned
        state.mut()._burnedAmount = 0;
        state.mut()._distributedAmount = 0;

        QPI::NoData endIn{}, endOut{};
        QHANDLE::END_TICK_locals endLoc{};
        TestProcedureCallContext endCtx(99, g_env.invocator, 0, 0);
        QHANDLE::__impl___endTick(endCtx, state, endIn, endOut, endLoc);

        // 30% of 100M = 30M target burn
        TEST_ASSERT(g_env.burned == 30000000, "Burned exactly 30,000,000 QUBIC");
        TEST_ASSERT(state.get()._burnedAmount == 30000000, "State recorded 30M burn");

        // 70% of 100M = 70,000,000 distributable
        // 70,000,000 / 676 = 103,550 per share
        // 103,550 * 676 = 69,999,800 distributed
        TEST_ASSERT(g_env.dividends == 103550, "Dividends per share is 103,550");
        TEST_ASSERT(state.get()._distributedAmount == 103550 * 676, "Distributed amount recorded correctly");
    }

    delete statePtr;

    std::cout << "\n==========================================" << std::endl;
    std::cout << "Test Summary: " << totalTests << " assertions, " << failedTests << " failures" << std::endl;
    std::cout << "==========================================" << std::endl;

    return failedTests == 0 ? 0 : 1;
}
