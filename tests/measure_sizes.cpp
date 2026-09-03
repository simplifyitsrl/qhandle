// Measures the real, compiler-computed layout of the QHandle contract state.
//
// The sizing table in DESIGN.md section 4 is derived from these numbers rather than from hand
// arithmetic. Two of them are load-bearing:
//
//   - sizeof(CommitmentPreimage) must be exactly 72 with NO padding, because qpi.K12 hashes it
//     raw. Padding bytes would make the same commitment hash differently on another compiler,
//     silently breaking every pending registration.
//   - sizeof(StateData) sets the per-tick state digest cost charged to the execution fee
//     reserve. That is the constraint that fixed launch capacity at 2^17 rather than 2^20.
//
// Build:
//   g++ -std=c++20 -mavx2 -w -I ../core/src -I ../core -I ../src -o measure_sizes measure_sizes.cpp

#define NO_UEFI 1
#include "contract_core/pre_qpi_def.h"
#include "qpi/qpi.h"

#define QHANDLE_CONTRACT_INDEX 99
#define CONTRACT_INDEX QHANDLE_CONTRACT_INDEX
#define CONTRACT_STATE_TYPE QHANDLE
#define CONTRACT_STATE2_TYPE QHANDLE2
#include "QHandle.h"

#include <cstdio>

int main()
{
    printf("QHandle state layout (measured)\n");
    printf("===============================\n\n");

    printf("sizeof(id)                 = %zu (align %zu)\n", sizeof(QPI::id), alignof(QPI::id));
    printf("sizeof(Name)               = %zu (align %zu)\n", sizeof(QHANDLE::Name), alignof(QHANDLE::Name));
    printf("sizeof(Record)             = %zu (align %zu)\n", sizeof(QHANDLE::Record), alignof(QHANDLE::Record));
    printf("sizeof(Commitment)         = %zu\n", sizeof(QHANDLE::Commitment));
    printf("sizeof(CommitmentPreimage) = %zu  <-- must be 72 exactly, no padding (32+32+8)\n",
           sizeof(QHANDLE::CommitmentPreimage));

    printf("\nsizeof(StateData)          = %zu bytes = %.2f MB\n",
           sizeof(QHANDLE::StateData), sizeof(QHANDLE::StateData) / 1048576.0);
    printf("  _handles                 = %zu bytes = %.2f MB\n",
           sizeof(QPI::HashMap<QHANDLE::Name, QHANDLE::Record, QHANDLE_CAPACITY>),
           sizeof(QPI::HashMap<QHANDLE::Name, QHANDLE::Record, QHANDLE_CAPACITY>) / 1048576.0);
    printf("  _primary                 = %zu bytes = %.2f MB\n",
           sizeof(QPI::HashMap<QPI::id, QHANDLE::Name, QHANDLE_CAPACITY>),
           sizeof(QPI::HashMap<QPI::id, QHANDLE::Name, QHANDLE_CAPACITY>) / 1048576.0);
    printf("  _commitments             = %zu bytes = %.2f KB\n",
           sizeof(QPI::HashMap<QPI::id, QHANDLE::Commitment, QHANDLE_COMMIT_CAPACITY>),
           sizeof(QPI::HashMap<QPI::id, QHANDLE::Commitment, QHANDLE_COMMIT_CAPACITY>) / 1024.0);

    printf("\nregistry capacity          = %llu slots (2^17)\n", (unsigned long long)QHANDLE_CAPACITY);
    printf("usable at 80%% load factor  = %llu handles\n", (unsigned long long)(QHANDLE_CAPACITY * 8 / 10));
    printf("share of the 1 GB limit    = %.1f%%\n",
           100.0 * sizeof(QHANDLE::StateData) / (1024.0 * 1024 * 1024));

    printf("\npublic interface structs (must stay padding-stable across compilers)\n");
    printf("  RegisterHandle_input     = %zu\n", sizeof(QHANDLE::RegisterHandle_input));
    printf("  ResolveHandle_output     = %zu\n", sizeof(QHANDLE::ResolveHandle_output));
    printf("  CommitRegistration_input = %zu\n", sizeof(QHANDLE::CommitRegistration_input));

    // Fail the build's intent if the hashed preimage ever acquires padding.
    if (sizeof(QHANDLE::CommitmentPreimage) != 72)
    {
        printf("\nFAIL: CommitmentPreimage has padding; K12 commitments are not portable.\n");
        return 1;
    }
    printf("\nOK\n");
    return 0;
}
