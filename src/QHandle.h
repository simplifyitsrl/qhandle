using namespace QPI;

// QHandle - human-readable naming service resolving to wallet addresses on Qubic.
//
// Design rationale, sizing math and the reasoning behind every constant below live in
// DESIGN.md at the repo root. Three points that are load-bearing for review:
//
//  1. Registration is commit-reveal. A plain one-shot register broadcasts the desired name
//     in the clear before it is mined and is trivially sniped.
//  2. The handle key is a wrapper struct, NOT an id. HashFunction<m256i> hashes only the
//     first 8 bytes (qpi_hash_map_impl.h), which for raw characters means the first 8
//     characters - handle namespaces cluster hard on prefixes. A non-m256i key gets the
//     full-width KangarooTwelve hash instead.
//  3. Capacity is deliberately small (2^17). The full state digest is recomputed on every
//     tick this contract dirties its state, and that cost is charged to the execution fee
//     reserve. Oversizing at launch is paid for on every single registration.

constexpr uint64 QHANDLE_CAPACITY = 131072;                // 2^17, table capacity
constexpr uint64 QHANDLE_MAX_USABLE_CAPACITY = 104857;     // 80% load factor of 2^17, prevents linear probe degradation
constexpr uint64 QHANDLE_COMMIT_CAPACITY = 4096;           // 2^12, commitments live minutes
constexpr uint64 QHANDLE_COMMIT_MAX_USABLE_CAPACITY = 3276;// 80% load factor of 2^12
constexpr uint64 QHANDLE_NAME_MAX_LENGTH = 32;
constexpr uint64 QHANDLE_NAME_MIN_LENGTH = 3;

constexpr uint32 QHANDLE_EPOCHS_PER_YEAR = 52;             // epoch ~= 1 week
constexpr uint32 QHANDLE_MAX_YEARS = 5;
constexpr uint32 QHANDLE_GRACE_EPOCHS = 13;                // ~90 days, matches ENS convention

// Commit-reveal window, in ticks. Ticks are ~1-2s, so this is roughly 10 seconds to 6 hours.
// A reveal must be far enough after the commit that the commit is already final, and close
// enough that a stale commitment cannot be hoarded. Note that an epoch transition advances
// the tick counter discontinuously; that can only push a pending commitment past
// QHANDLE_COMMIT_MAX_TICK_AGE, i.e. it fails closed and the user simply re-commits.
constexpr uint32 QHANDLE_COMMIT_MIN_TICK_AGE = 5;
constexpr uint32 QHANDLE_COMMIT_MAX_TICK_AGE = 10000;

constexpr uint64 QHANDLE_CLEANUP_THRESHOLD_PERCENT = 30;   // same threshold Qx uses in END_TICK
constexpr uint64 QHANDLE_BURN_PERCENT = 30;                // revenue share -> executionFeeReserve

// PLACEHOLDER PRICING. These cannot be derived from the docs: the execution fee multiplier is
// set by computor quorum at runtime (SPECIAL_COMMAND_SET_EXECUTION_FEE_MULTIPLIER) and is not
// published as a constant. They MUST be re-calibrated against a measured testnet deployment of
// this exact state size before mainnet. See DESIGN.md section 8, item 1.
constexpr uint64 QHANDLE_REG_FEE_PER_YEAR_LEN3 = 1000000000;
constexpr uint64 QHANDLE_REG_FEE_PER_YEAR_LEN4 = 200000000;
constexpr uint64 QHANDLE_REG_FEE_PER_YEAR_LONG = 20000000;
constexpr uint64 QHANDLE_RENEWAL_FEE_PER_YEAR = 20000000;
constexpr uint64 QHANDLE_TRANSFER_FEE = 1000000;

// Character literals are forbidden in contracts (core/doc/contracts.md), so the charset
// checks are numeric.
constexpr uint8 QHANDLE_CHAR_HYPHEN = 45;
constexpr uint8 QHANDLE_CHAR_DIGIT_0 = 48;
constexpr uint8 QHANDLE_CHAR_DIGIT_9 = 57;
constexpr uint8 QHANDLE_CHAR_UPPER_A = 65;
constexpr uint8 QHANDLE_CHAR_UPPER_Z = 90;
constexpr uint8 QHANDLE_CHAR_LOWER_A = 97;
constexpr uint8 QHANDLE_CHAR_LOWER_Z = 122;
constexpr uint8 QHANDLE_CASE_OFFSET = 32;

// Status codes returned by every procedure and function.
constexpr uint8 QHANDLE_OK = 0;
constexpr uint8 QHANDLE_ERR_INVALID_NAME = 1;
constexpr uint8 QHANDLE_ERR_TAKEN = 2;
constexpr uint8 QHANDLE_ERR_NOT_FOUND = 3;
constexpr uint8 QHANDLE_ERR_EXPIRED = 4;
constexpr uint8 QHANDLE_ERR_NOT_OWNER = 5;
constexpr uint8 QHANDLE_ERR_INSUFFICIENT_FEE = 6;
constexpr uint8 QHANDLE_ERR_REGISTRY_FULL = 7;
constexpr uint8 QHANDLE_ERR_INVALID_YEARS = 8;
constexpr uint8 QHANDLE_ERR_NO_COMMITMENT = 9;
constexpr uint8 QHANDLE_ERR_COMMIT_TOO_YOUNG = 10;
constexpr uint8 QHANDLE_ERR_COMMIT_EXPIRED = 11;
constexpr uint8 QHANDLE_ERR_LOCKED = 12;
constexpr uint8 QHANDLE_ERR_NOT_RECLAIMABLE = 13;
constexpr uint8 QHANDLE_ERR_COMMIT_STORE_FULL = 14;

constexpr uint8 QHANDLE_FLAG_LOCKED = 1;

struct QHANDLE2
{
};

struct QHANDLE : public ContractBase
{
	// ---------------------------------------------------------------------------------
	// Types
	// ---------------------------------------------------------------------------------

	// A canonical handle: lowercase, zero-padded to the full width, with no embedded zeros.
	//
	// Canonicalisation is a correctness requirement, not hygiene. The generic HashFunction
	// runs KangarooTwelve over sizeof(KeyT) raw bytes, so any non-zero garbage in the tail
	// yields a different hash for the same name. Nothing may reach _handles without passing
	// through _Canonicalize first.
	struct Name
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> chars;

		// Required: QPI::Array supplies no operator==, and HashMap::getElementIndex needs one.
		// Unrolled comparison avoids stack variable / loop index allocation on function call stack.
		bool operator==(const Name& other) const
		{
			return chars.get(0) == other.chars.get(0)
				&& chars.get(1) == other.chars.get(1)
				&& chars.get(2) == other.chars.get(2)
				&& chars.get(3) == other.chars.get(3)
				&& chars.get(4) == other.chars.get(4)
				&& chars.get(5) == other.chars.get(5)
				&& chars.get(6) == other.chars.get(6)
				&& chars.get(7) == other.chars.get(7)
				&& chars.get(8) == other.chars.get(8)
				&& chars.get(9) == other.chars.get(9)
				&& chars.get(10) == other.chars.get(10)
				&& chars.get(11) == other.chars.get(11)
				&& chars.get(12) == other.chars.get(12)
				&& chars.get(13) == other.chars.get(13)
				&& chars.get(14) == other.chars.get(14)
				&& chars.get(15) == other.chars.get(15)
				&& chars.get(16) == other.chars.get(16)
				&& chars.get(17) == other.chars.get(17)
				&& chars.get(18) == other.chars.get(18)
				&& chars.get(19) == other.chars.get(19)
				&& chars.get(20) == other.chars.get(20)
				&& chars.get(21) == other.chars.get(21)
				&& chars.get(22) == other.chars.get(22)
				&& chars.get(23) == other.chars.get(23)
				&& chars.get(24) == other.chars.get(24)
				&& chars.get(25) == other.chars.get(25)
				&& chars.get(26) == other.chars.get(26)
				&& chars.get(27) == other.chars.get(27)
				&& chars.get(28) == other.chars.get(28)
				&& chars.get(29) == other.chars.get(29)
				&& chars.get(30) == other.chars.get(30)
				&& chars.get(31) == other.chars.get(31);
		}
	};

	struct Record
	{
		id owner;               // may transfer, renew, repoint; not necessarily the target
		id target;              // what ResolveHandle returns
		uint32 expiryEpoch;     // exclusive: expired once epoch >= expiryEpoch
		uint32 registeredEpoch;
		uint8 length;           // real length, 3..32; lets clients render without the padding
		uint8 flags;
		uint16 reserved;        // explicit, so the layout does not depend on the compiler
	};

	struct Commitment
	{
		id committer;
		uint32 commitTick;
		uint32 reserved;
	};

	// Preimage of a registration commitment. The client hashes this exact struct with K12 and
	// sends only the digest to CommitRegistration; the fields are revealed later to
	// RegisterHandle. Binding the owner prevents a watcher from replaying an observed
	// commitment, and the salt prevents a dictionary attack over likely names.
	struct CommitmentPreimage
	{
		Name name;
		id owner;
		uint64 salt;
	};

	// Storage key preimage for commitments in _commitments. Binding the committer to the commitment
	// prevents an attacker from front-running an observed commitment digest to block the committer.
	struct CommitmentKeyPreimage
	{
		id committer;
		id commitment;
	};

	// ---------------------------------------------------------------------------------
	// Public interface
	//
	// Input and output structs may only contain integer/bool types, id, Array, BitArray and
	// structs of those. Name qualifies (it holds a single Array<uint8, 32>); HashMap never
	// crosses this boundary.
	// ---------------------------------------------------------------------------------

public:
	struct Fees_input
	{
	};
	struct Fees_output
	{
		uint64 registrationFeePerYearLength3;
		uint64 registrationFeePerYearLength4;
		uint64 registrationFeePerYearLength5Plus;
		uint64 renewalFeePerYear;
		uint64 transferFee;
		uint32 maxYears;
		uint32 graceEpochs;
	};

	struct ResolveHandle_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
	};
	struct ResolveHandle_output
	{
		id target;
		id owner;
		uint32 expiryEpoch;
		uint32 registeredEpoch;
		uint8 length;
		uint8 flags;
		uint8 status;
		uint8 reserved;
	};

	struct ReverseResolve_input
	{
		id addr;
	};
	struct ReverseResolve_output
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		uint32 expiryEpoch;
		uint8 length;
		uint8 status;
		uint16 reserved;
	};

	struct ComputeCommitment_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		id owner;
		uint64 salt;
	};
	struct ComputeCommitment_output
	{
		id commitment;
		uint8 status;
		uint8 reserved0;
		uint16 reserved1;
		uint32 reserved2;
	};

	struct Stats_input
	{
	};
	struct Stats_output
	{
		uint64 handlePopulation;
		uint64 handleCapacity;
		uint64 commitmentPopulation;
		uint64 commitmentCapacity;
		uint64 earnedAmount;
		uint64 distributedAmount;
		uint64 burnedAmount;
		sint64 executionFeeReserve;
	};

	struct CommitRegistration_input
	{
		id commitment;
	};
	struct CommitRegistration_output
	{
		uint32 commitTick;
		uint8 status;
		uint8 reserved0;
		uint16 reserved1;
	};

	struct RegisterHandle_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		id target;
		uint64 salt;
		uint32 years;
	};
	struct RegisterHandle_output
	{
		uint32 expiryEpoch;
		uint64 feePaid;
		uint8 status;
		uint8 reserved0;
		uint16 reserved1;
	};

	struct RenewHandle_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		uint32 years;
	};
	struct RenewHandle_output
	{
		uint32 expiryEpoch;
		uint64 feePaid;
		uint8 status;
		uint8 reserved0;
		uint16 reserved1;
	};

	struct TransferHandle_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		id newOwner;
	};
	struct TransferHandle_output
	{
		uint8 status;
	};

	struct SetTarget_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		id newTarget;
	};
	struct SetTarget_output
	{
		uint8 status;
	};

	struct SetPrimary_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		bit clear;
	};
	struct SetPrimary_output
	{
		uint8 status;
	};

	struct SetLock_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
		bit locked;
	};
	struct SetLock_output
	{
		uint8 status;
	};

	struct ReclaimExpired_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> name;
	};
	struct ReclaimExpired_output
	{
		uint8 status;
	};

	struct PruneCommitment_input
	{
		id committer;
		id commitment;
	};
	struct PruneCommitment_output
	{
		uint8 status;
	};

	// ---------------------------------------------------------------------------------
	// State
	// ---------------------------------------------------------------------------------

	struct StateData
	{
		// Forward registry. 32-byte key + 80-byte record + 0.25B occupation flags per slot.
		HashMap<Name, Record, QHANDLE_CAPACITY> _handles;

		// Reverse records: the one handle a wallet should display for an address. Opt-in and
		// set explicitly by the owner, like ENS reverse resolution. Deriving it automatically
		// from a record's target would let anyone squat a display name on someone else's
		// address just by pointing a handle at it.
		HashMap<id, Name, QHANDLE_CAPACITY> _primary;

		// Pending registration commitments. Keyed by an id, so the first-8-bytes hash applies -
		// harmless here, unlike for handle names, because a K12 digest is already uniform.
		HashMap<id, Commitment, QHANDLE_COMMIT_CAPACITY> _commitments;

		uint64 _earnedAmount;
		uint64 _distributedAmount;
		uint64 _burnedAmount;
	};

	// ---------------------------------------------------------------------------------
	// Private helpers
	// ---------------------------------------------------------------------------------

protected:

	struct _Canonicalize_input
	{
		Array<uint8, QHANDLE_NAME_MAX_LENGTH> raw;
	};
	struct _Canonicalize_output
	{
		Name name;
		uint8 length;
		bit valid;
	};
	struct _Canonicalize_locals
	{
		uint64 i;
		uint8 c;
		uint8 length;
		bit sawTerminator;
	};

	// Validates and normalises a raw client-supplied name.
	//
	// Rules: length 3..32; charset a-z, 0-9 and hyphen; hyphen may not lead, trail or repeat;
	// uppercase is folded to lowercase rather than rejected, so Alice and alice cannot both
	// be registered. Unicode is rejected by construction - correct handling would need NFC
	// normalisation and confusable detection, and contracts may use no external libraries.
	// Without normalisation two byte-different names render identically, which in an address
	// resolver is a phishing primitive.
	PRIVATE_FUNCTION_WITH_LOCALS(_Canonicalize)
	{
		output.valid = false;
		output.length = 0;

		// Find the terminator and verify the whole tail is zero. An embedded zero followed by
		// non-zero bytes would produce two distinct keys rendering as the same name.
		locals.length = QHANDLE_NAME_MAX_LENGTH;
		locals.sawTerminator = false;
		for (locals.i = 0; locals.i < QHANDLE_NAME_MAX_LENGTH; ++locals.i)
		{
			locals.c = input.raw.get(locals.i);
			if (locals.c == 0)
			{
				if (!locals.sawTerminator)
				{
					locals.sawTerminator = true;
					locals.length = (uint8)locals.i;
				}
			}
			else
			{
				if (locals.sawTerminator)
				{
					return;
				}
			}
		}

		if (locals.length < QHANDLE_NAME_MIN_LENGTH)
		{
			return;
		}

		for (locals.i = 0; locals.i < locals.length; ++locals.i)
		{
			locals.c = input.raw.get(locals.i);

			if (locals.c >= QHANDLE_CHAR_UPPER_A && locals.c <= QHANDLE_CHAR_UPPER_Z)
			{
				locals.c += QHANDLE_CASE_OFFSET;
			}

			if (locals.c == QHANDLE_CHAR_HYPHEN)
			{
				// No leading, trailing or doubled hyphen.
				if (locals.i == 0 || locals.i + 1 == locals.length)
				{
					return;
				}
				if (input.raw.get(locals.i - 1) == QHANDLE_CHAR_HYPHEN)
				{
					return;
				}
			}
			else
			{
				if (!((locals.c >= QHANDLE_CHAR_LOWER_A && locals.c <= QHANDLE_CHAR_LOWER_Z)
					|| (locals.c >= QHANDLE_CHAR_DIGIT_0 && locals.c <= QHANDLE_CHAR_DIGIT_9)))
				{
					return;
				}
			}

			output.name.chars.set(locals.i, locals.c);
		}

		// Zero the padding explicitly - the hash covers all 32 bytes.
		for (locals.i = locals.length; locals.i < QHANDLE_NAME_MAX_LENGTH; ++locals.i)
		{
			output.name.chars.set(locals.i, 0);
		}

		output.length = locals.length;
		output.valid = true;
	}

	struct _RegistrationFee_input
	{
		uint8 length;
		uint32 years;
	};
	struct _RegistrationFee_output
	{
		uint64 amount;
	};

	// Short handles are priced up: they are the scarce, squattable ones.
	PRIVATE_FUNCTION(_RegistrationFee)
	{
		if (input.length == 3)
		{
			output.amount = QHANDLE_REG_FEE_PER_YEAR_LEN3 * input.years;
		}
		else
		{
			if (input.length == 4)
			{
				output.amount = QHANDLE_REG_FEE_PER_YEAR_LEN4 * input.years;
			}
			else
			{
				output.amount = QHANDLE_REG_FEE_PER_YEAR_LONG * input.years;
			}
		}
	}

	// ---------------------------------------------------------------------------------
	// User functions (read-only, free, and still served when the fee reserve is dormant)
	// ---------------------------------------------------------------------------------

	PUBLIC_FUNCTION(Fees)
	{
		output.registrationFeePerYearLength3 = QHANDLE_REG_FEE_PER_YEAR_LEN3;
		output.registrationFeePerYearLength4 = QHANDLE_REG_FEE_PER_YEAR_LEN4;
		output.registrationFeePerYearLength5Plus = QHANDLE_REG_FEE_PER_YEAR_LONG;
		output.renewalFeePerYear = QHANDLE_RENEWAL_FEE_PER_YEAR;
		output.transferFee = QHANDLE_TRANSFER_FEE;
		output.maxYears = QHANDLE_MAX_YEARS;
		output.graceEpochs = QHANDLE_GRACE_EPOCHS;
	}

	struct ResolveHandle_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		Record record;
	};

	// An expired handle resolves to NULL_ID with status EXPIRED, never to its stale target.
	// A wallet must not be able to pay the previous holder of a lapsed name.
	PUBLIC_FUNCTION_WITH_LOCALS(ResolveHandle)
	{
		output.target = NULL_ID;
		output.owner = NULL_ID;

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (!state.get()._handles.get(locals.canonOut.name, locals.record))
		{
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		output.expiryEpoch = locals.record.expiryEpoch;
		output.registeredEpoch = locals.record.registeredEpoch;
		output.length = locals.record.length;
		output.flags = locals.record.flags;

		if ((uint32)qpi.epoch() >= locals.record.expiryEpoch)
		{
			output.status = QHANDLE_ERR_EXPIRED;
			return;
		}

		output.target = locals.record.target;
		output.owner = locals.record.owner;
		output.status = QHANDLE_OK;
	}

	struct ReverseResolve_locals
	{
		Name name;
		Record record;
	};

	// The reverse record is validated against the forward one on every read: the handle must
	// still exist, still be live, and still be owned by the address being queried. That makes
	// stale reverse entries harmless without needing to keep the two maps in lockstep.
	PUBLIC_FUNCTION_WITH_LOCALS(ReverseResolve)
	{
		if (!state.get()._primary.get(input.addr, locals.name))
		{
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if (!state.get()._handles.get(locals.name, locals.record))
		{
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if (locals.record.owner != input.addr)
		{
			output.status = QHANDLE_ERR_NOT_OWNER;
			return;
		}

		output.expiryEpoch = locals.record.expiryEpoch;
		output.length = locals.record.length;

		if ((uint32)qpi.epoch() >= locals.record.expiryEpoch)
		{
			output.status = QHANDLE_ERR_EXPIRED;
			return;
		}

		output.name = locals.name.chars;
		output.status = QHANDLE_OK;
	}

	struct ComputeCommitment_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		CommitmentPreimage preimage;
	};

	// Convenience for clients: canonicalises the name and returns the exact digest that
	// CommitRegistration expects. Clients may compute it offline instead, but this guarantees
	// they agree with the contract on canonical form.
	PUBLIC_FUNCTION_WITH_LOCALS(ComputeCommitment)
	{
		output.commitment = NULL_ID;

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		locals.preimage.name = locals.canonOut.name;
		locals.preimage.owner = input.owner;
		locals.preimage.salt = input.salt;

		output.commitment = qpi.K12(locals.preimage);
		output.status = QHANDLE_OK;
	}

	PUBLIC_FUNCTION(Stats)
	{
		output.handlePopulation = state.get()._handles.population();
		output.handleCapacity = QHANDLE_MAX_USABLE_CAPACITY;
		output.commitmentPopulation = state.get()._commitments.population();
		output.commitmentCapacity = QHANDLE_COMMIT_MAX_USABLE_CAPACITY;
		output.earnedAmount = state.get()._earnedAmount;
		output.distributedAmount = state.get()._distributedAmount;
		output.burnedAmount = state.get()._burnedAmount;
		output.executionFeeReserve = qpi.queryFeeReserve(SELF_INDEX);
	}

	// ---------------------------------------------------------------------------------
	// User procedures
	//
	// Every one of these validates fully BEFORE the first state.mut(). state.mut() is what
	// marks the state dirty and triggers a full-state digest recompute at end of tick; a
	// rejected transaction that has already dirtied the state has burned that cost for
	// nothing, which is a cheap way to drain the fee reserve.
	// ---------------------------------------------------------------------------------

	struct CommitRegistration_locals
	{
		CommitmentKeyPreimage keyPreimage;
		Commitment commitment;
		id key;
	};

	// Step 1 of 2. Free, and deliberately reveals nothing: the digest binds the name, the
	// intended owner and a secret salt.
	PUBLIC_PROCEDURE_WITH_LOCALS(CommitRegistration)
	{
		if (qpi.invocationReward() > 0)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward());
		}

		if (input.commitment == NULL_ID)
		{
			output.status = QHANDLE_ERR_NO_COMMITMENT;
			return;
		}

		locals.keyPreimage.committer = qpi.invocator();
		locals.keyPreimage.commitment = input.commitment;
		locals.key = qpi.K12(locals.keyPreimage);

		// Re-committing the same digest by the same committer is idempotent and returns the existing tick.
		// Keying by (committer, commitment) prevents an observer from stealing or locking the slot.
		if (state.get()._commitments.get(locals.key, locals.commitment))
		{
			output.commitTick = locals.commitment.commitTick;
			output.status = QHANDLE_OK;
			return;
		}

		if (state.get()._commitments.population() >= QHANDLE_COMMIT_MAX_USABLE_CAPACITY)
		{
			output.status = QHANDLE_ERR_COMMIT_STORE_FULL;
			return;
		}

		locals.commitment.committer = qpi.invocator();
		locals.commitment.commitTick = qpi.tick();
		locals.commitment.reserved = 0;

		state.mut()._commitments.set(locals.key, locals.commitment);

		output.commitTick = locals.commitment.commitTick;
		output.status = QHANDLE_OK;
	}

	struct RegisterHandle_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		_RegistrationFee_input feeIn;
		_RegistrationFee_output feeOut;
		CommitmentKeyPreimage keyPreimage;
		CommitmentPreimage preimage;
		Commitment commitment;
		Record record;
		Record existing;
		Name staleName;
		id digest;
		id commitmentKey;
		uint32 currentEpoch;
		uint32 currentTick;
		uint32 age;
		bit slotIsFree;
	};

	// Step 2 of 2. Reveals the preimage, which must match a commitment made by this same
	// invocator, old enough to be final and young enough not to be hoarded.
	PUBLIC_PROCEDURE_WITH_LOCALS(RegisterHandle)
	{
		// A handle pointing at the null address is unrecoverable and would hand any wallet
		// that resolves it a null payee.
		if (input.target == NULL_ID)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (input.years == 0 || input.years > QHANDLE_MAX_YEARS)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_YEARS;
			return;
		}

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		// Verify the commitment.
		locals.preimage.name = locals.canonOut.name;
		locals.preimage.owner = qpi.invocator();
		locals.preimage.salt = input.salt;
		locals.digest = qpi.K12(locals.preimage);

		locals.keyPreimage.committer = qpi.invocator();
		locals.keyPreimage.commitment = locals.digest;
		locals.commitmentKey = qpi.K12(locals.keyPreimage);

		if (!state.get()._commitments.get(locals.commitmentKey, locals.commitment))
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_NO_COMMITMENT;
			return;
		}

		locals.currentTick = qpi.tick();
		if (locals.currentTick < locals.commitment.commitTick)
		{
			// Cannot happen with a monotonic tick counter; treated as a stale commitment
			// rather than trusted, so an unsigned underflow cannot open the window.
			locals.age = QHANDLE_COMMIT_MAX_TICK_AGE + 1;
		}
		else
		{
			locals.age = locals.currentTick - locals.commitment.commitTick;
		}

		if (locals.age < QHANDLE_COMMIT_MIN_TICK_AGE)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_COMMIT_TOO_YOUNG;
			return;
		}

		if (locals.age > QHANDLE_COMMIT_MAX_TICK_AGE)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_COMMIT_EXPIRED;
			return;
		}

		locals.currentEpoch = (uint32)qpi.epoch();

		// Availability. A name past expiry + grace is reclaimable and may be taken over here,
		// so a lapsed name is never unobtainable merely because nobody swept it.
		locals.slotIsFree = true;
		if (state.get()._handles.get(locals.canonOut.name, locals.existing))
		{
			if (locals.currentEpoch < locals.existing.expiryEpoch + QHANDLE_GRACE_EPOCHS)
			{
				if (qpi.invocationReward() > 0)
				{
					qpi.transfer(qpi.invocator(), qpi.invocationReward());
				}
				output.status = QHANDLE_ERR_TAKEN;
				return;
			}
			locals.slotIsFree = false;
		}
		else
		{
			if (state.get()._handles.population() >= QHANDLE_MAX_USABLE_CAPACITY)
			{
				if (qpi.invocationReward() > 0)
				{
					qpi.transfer(qpi.invocator(), qpi.invocationReward());
				}
				output.status = QHANDLE_ERR_REGISTRY_FULL;
				return;
			}
		}

		locals.feeIn.length = locals.canonOut.length;
		locals.feeIn.years = input.years;
		CALL(_RegistrationFee, locals.feeIn, locals.feeOut);

		if ((uint64)qpi.invocationReward() < locals.feeOut.amount)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INSUFFICIENT_FEE;
			return;
		}

		if ((uint64)qpi.invocationReward() > locals.feeOut.amount)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward() - locals.feeOut.amount);
		}

		// ---- validation complete; state may now be dirtied ----

		// Drop the previous holder's reverse record if it still points here, so the name does
		// not keep displaying for an address that no longer owns it.
		if (!locals.slotIsFree)
		{
			if (state.get()._primary.get(locals.existing.owner, locals.staleName))
			{
				if (locals.staleName == locals.canonOut.name)
				{
					state.mut()._primary.removeByKey(locals.existing.owner);
				}
			}
		}

		locals.record.owner = qpi.invocator();
		locals.record.target = input.target;
		locals.record.expiryEpoch = locals.currentEpoch + QHANDLE_EPOCHS_PER_YEAR * input.years;
		locals.record.registeredEpoch = locals.currentEpoch;
		locals.record.length = locals.canonOut.length;
		locals.record.flags = 0;
		locals.record.reserved = 0;

		state.mut()._handles.set(locals.canonOut.name, locals.record);
		state.mut()._commitments.removeByKey(locals.commitmentKey);
		state.mut()._earnedAmount += locals.feeOut.amount;

		output.expiryEpoch = locals.record.expiryEpoch;
		output.feePaid = locals.feeOut.amount;
		output.status = QHANDLE_OK;
	}

	struct RenewHandle_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		Record record;
		uint64 fee;
		uint32 currentEpoch;
		uint32 base;
	};

	// Permissionless on purpose: anyone may renew anyone's handle. It cannot harm the owner,
	// and it removes the "the key holder went dark and the name died" failure mode. ENS does
	// the same.
	PUBLIC_PROCEDURE_WITH_LOCALS(RenewHandle)
	{
		if (input.years == 0 || input.years > QHANDLE_MAX_YEARS)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_YEARS;
			return;
		}

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (!state.get()._handles.get(locals.canonOut.name, locals.record))
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		locals.currentEpoch = (uint32)qpi.epoch();

		// Past the grace period the name is reclaimable by anyone; renewing it then would let
		// the lapsed holder jump a queue they are no longer in.
		if (locals.currentEpoch >= locals.record.expiryEpoch + QHANDLE_GRACE_EPOCHS)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_EXPIRED;
			return;
		}

		locals.fee = QHANDLE_RENEWAL_FEE_PER_YEAR * input.years;

		if ((uint64)qpi.invocationReward() < locals.fee)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INSUFFICIENT_FEE;
			return;
		}

		if ((uint64)qpi.invocationReward() > locals.fee)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward() - locals.fee);
		}

		// Renewing inside the grace period extends from now, not from the lapsed expiry, so
		// the grace period cannot be farmed as free registration time.
		locals.base = locals.record.expiryEpoch;
		if (locals.base < locals.currentEpoch)
		{
			locals.base = locals.currentEpoch;
		}
		locals.record.expiryEpoch = locals.base + QHANDLE_EPOCHS_PER_YEAR * input.years;

		state.mut()._handles.replace(locals.canonOut.name, locals.record);
		state.mut()._earnedAmount += locals.fee;

		output.expiryEpoch = locals.record.expiryEpoch;
		output.feePaid = locals.fee;
		output.status = QHANDLE_OK;
	}

	struct TransferHandle_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		Record record;
		Name staleName;
	};

	PUBLIC_PROCEDURE_WITH_LOCALS(TransferHandle)
	{
		// Transferring to the null address destroys the handle irrecoverably.
		if (input.newOwner == NULL_ID)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_NOT_OWNER;
			return;
		}

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (!state.get()._handles.get(locals.canonOut.name, locals.record))
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if (locals.record.owner != qpi.invocator())
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_NOT_OWNER;
			return;
		}

		if ((locals.record.flags & QHANDLE_FLAG_LOCKED) != 0)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_LOCKED;
			return;
		}

		if ((uint32)qpi.epoch() >= locals.record.expiryEpoch)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_EXPIRED;
			return;
		}

		if ((uint64)qpi.invocationReward() < QHANDLE_TRANSFER_FEE)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INSUFFICIENT_FEE;
			return;
		}

		if ((uint64)qpi.invocationReward() > QHANDLE_TRANSFER_FEE)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward() - QHANDLE_TRANSFER_FEE);
		}

		// ---- validation complete ----

		// The old owner's reverse record must not survive the transfer.
		if (state.get()._primary.get(locals.record.owner, locals.staleName))
		{
			if (locals.staleName == locals.canonOut.name)
			{
				state.mut()._primary.removeByKey(locals.record.owner);
			}
		}

		// Target follows ownership unless the new owner repoints it; leaving it aimed at the
		// seller's address would be a live footgun for anyone paying the handle.
		locals.record.owner = input.newOwner;
		locals.record.target = input.newOwner;

		state.mut()._handles.replace(locals.canonOut.name, locals.record);
		state.mut()._earnedAmount += QHANDLE_TRANSFER_FEE;

		output.status = QHANDLE_OK;
	}

	struct SetTarget_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		Record record;
	};

	// Repoint without transferring ownership: a cold key can own the name while it resolves
	// to a hot spending address.
	PUBLIC_PROCEDURE_WITH_LOCALS(SetTarget)
	{
		if (input.newTarget == NULL_ID)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (!state.get()._handles.get(locals.canonOut.name, locals.record))
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if (locals.record.owner != qpi.invocator())
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_NOT_OWNER;
			return;
		}

		if ((uint32)qpi.epoch() >= locals.record.expiryEpoch)
		{
			if (qpi.invocationReward() > 0)
			{
				qpi.transfer(qpi.invocator(), qpi.invocationReward());
			}
			output.status = QHANDLE_ERR_EXPIRED;
			return;
		}

		if (qpi.invocationReward() > 0)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward());
		}

		locals.record.target = input.newTarget;
		state.mut()._handles.replace(locals.canonOut.name, locals.record);

		output.status = QHANDLE_OK;
	}

	struct SetPrimary_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		Record record;
	};

	PUBLIC_PROCEDURE_WITH_LOCALS(SetPrimary)
	{
		if (qpi.invocationReward() > 0)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward());
		}

		if (input.clear)
		{
			state.mut()._primary.removeByKey(qpi.invocator());
			output.status = QHANDLE_OK;
			return;
		}

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (!state.get()._handles.get(locals.canonOut.name, locals.record))
		{
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if (locals.record.owner != qpi.invocator())
		{
			output.status = QHANDLE_ERR_NOT_OWNER;
			return;
		}

		if ((uint32)qpi.epoch() >= locals.record.expiryEpoch)
		{
			output.status = QHANDLE_ERR_EXPIRED;
			return;
		}

		// Only a NEW entry can overflow the map; overwriting an existing one does not grow
		// the population, and must keep working even at full capacity.
		if (!state.get()._primary.contains(qpi.invocator()))
		{
			if (state.get()._primary.population() >= QHANDLE_MAX_USABLE_CAPACITY)
			{
				output.status = QHANDLE_ERR_REGISTRY_FULL;
				return;
			}
		}

		state.mut()._primary.set(qpi.invocator(), locals.canonOut.name);
		output.status = QHANDLE_OK;
	}

	struct SetLock_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		Record record;
	};

	// Opt-in transfer lock. Cheap protection against a compromised key being used to move a
	// valuable name out; the owner disables it deliberately before any legitimate sale.
	PUBLIC_PROCEDURE_WITH_LOCALS(SetLock)
	{
		if (qpi.invocationReward() > 0)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward());
		}

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (!state.get()._handles.get(locals.canonOut.name, locals.record))
		{
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if (locals.record.owner != qpi.invocator())
		{
			output.status = QHANDLE_ERR_NOT_OWNER;
			return;
		}

		if ((uint32)qpi.epoch() >= locals.record.expiryEpoch)
		{
			output.status = QHANDLE_ERR_EXPIRED;
			return;
		}

		if (input.locked)
		{
			locals.record.flags |= QHANDLE_FLAG_LOCKED;
		}
		else
		{
			locals.record.flags &= ~QHANDLE_FLAG_LOCKED;
		}

		state.mut()._handles.replace(locals.canonOut.name, locals.record);
		output.status = QHANDLE_OK;
	}

	struct ReclaimExpired_locals
	{
		_Canonicalize_input canonIn;
		_Canonicalize_output canonOut;
		Record record;
		Name staleName;
	};

	// Static memory means expired records do not free themselves; unreclaimed expiries are a
	// slow leak that ends in a full registry. Permissionless and free so that anyone - a
	// would-be registrant, a community bot - can sweep.
	PUBLIC_PROCEDURE_WITH_LOCALS(ReclaimExpired)
	{
		if (qpi.invocationReward() > 0)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward());
		}

		locals.canonIn.raw = input.name;
		CALL(_Canonicalize, locals.canonIn, locals.canonOut);
		if (!locals.canonOut.valid)
		{
			output.status = QHANDLE_ERR_INVALID_NAME;
			return;
		}

		if (!state.get()._handles.get(locals.canonOut.name, locals.record))
		{
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if ((uint32)qpi.epoch() < locals.record.expiryEpoch + QHANDLE_GRACE_EPOCHS)
		{
			output.status = QHANDLE_ERR_NOT_RECLAIMABLE;
			return;
		}

		if (state.get()._primary.get(locals.record.owner, locals.staleName))
		{
			if (locals.staleName == locals.canonOut.name)
			{
				state.mut()._primary.removeByKey(locals.record.owner);
			}
		}

		state.mut()._handles.removeByKey(locals.canonOut.name);
		output.status = QHANDLE_OK;
	}

	struct PruneCommitment_locals
	{
		CommitmentKeyPreimage keyPreimage;
		Commitment commitment;
		id key;
		uint32 age;
	};

	// Abandoned commitments (committed, never revealed) would otherwise fill the commitment
	// store and block registrations for everyone.
	PUBLIC_PROCEDURE_WITH_LOCALS(PruneCommitment)
	{
		if (qpi.invocationReward() > 0)
		{
			qpi.transfer(qpi.invocator(), qpi.invocationReward());
		}

		locals.keyPreimage.committer = input.committer;
		locals.keyPreimage.commitment = input.commitment;
		locals.key = qpi.K12(locals.keyPreimage);

		if (!state.get()._commitments.get(locals.key, locals.commitment))
		{
			output.status = QHANDLE_ERR_NOT_FOUND;
			return;
		}

		if (qpi.tick() < locals.commitment.commitTick)
		{
			locals.age = QHANDLE_COMMIT_MAX_TICK_AGE + 1;
		}
		else
		{
			locals.age = qpi.tick() - locals.commitment.commitTick;
		}

		if (locals.age <= QHANDLE_COMMIT_MAX_TICK_AGE)
		{
			output.status = QHANDLE_ERR_NOT_RECLAIMABLE;
			return;
		}

		state.mut()._commitments.removeByKey(locals.key);
		output.status = QHANDLE_OK;
	}

	// ---------------------------------------------------------------------------------
	// Registration
	// ---------------------------------------------------------------------------------

	REGISTER_USER_FUNCTIONS_AND_PROCEDURES()
	{
		REGISTER_USER_FUNCTION(Fees, 1);
		REGISTER_USER_FUNCTION(ResolveHandle, 2);
		REGISTER_USER_FUNCTION(ReverseResolve, 3);
		REGISTER_USER_FUNCTION(ComputeCommitment, 4);
		REGISTER_USER_FUNCTION(Stats, 5);

		REGISTER_USER_PROCEDURE(CommitRegistration, 1);
		REGISTER_USER_PROCEDURE(RegisterHandle, 2);
		REGISTER_USER_PROCEDURE(RenewHandle, 3);
		REGISTER_USER_PROCEDURE(TransferHandle, 4);
		REGISTER_USER_PROCEDURE(SetTarget, 5);
		REGISTER_USER_PROCEDURE(SetPrimary, 6);
		REGISTER_USER_PROCEDURE(SetLock, 7);
		REGISTER_USER_PROCEDURE(ReclaimExpired, 8);
		REGISTER_USER_PROCEDURE(PruneCommitment, 9);
	}

	INITIALIZE()
	{
		// The whole state is zeroed before INITIALIZE, and all fees are compile-time
		// constants for now, so there is nothing to set. Once fees become shareholder-
		// adjustable they move into StateData and get their defaults here.
	}

	struct END_TICK_locals
	{
		uint64 targetBurn;
		uint64 distributable;
		uint64 perShare;
		sint64 burnResult;
	};

	END_TICK_WITH_LOCALS()
	{
		// Burn a fixed share of revenue into the execution fee reserve. qpi.burn() is the only
		// way to refill it, and a contract that distributes 100% of revenue eventually runs the
		// reserve to zero, goes dormant, and stops registering and renewing.
		locals.targetBurn = div(state.get()._earnedAmount * QHANDLE_BURN_PERCENT, 100ULL);
		if (locals.targetBurn > state.get()._burnedAmount)
		{
			// burn() returns a negative value on failure (insufficient contract balance).
			// The shortfall must stay on the books, otherwise a single failed burn silently
			// cancels that obligation forever and the reserve is never refilled.
			locals.burnResult = qpi.burn((sint64)(locals.targetBurn - state.get()._burnedAmount));
			if (locals.burnResult >= 0)
			{
				state.mut()._burnedAmount = locals.targetBurn;
			}
		}

		// Distribute the remainder to the 676 IPO shareholders.
		locals.distributable = state.get()._earnedAmount - locals.targetBurn;
		if (locals.distributable > state.get()._distributedAmount)
		{
			locals.perShare = div(locals.distributable - state.get()._distributedAmount, 676ULL);
			if (locals.perShare > 0)
			{
				if (qpi.distributeDividends(locals.perShare))
				{
					state.mut()._distributedAmount += locals.perShare * NUMBER_OF_COMPUTORS;
				}
			}
		}

		// Compaction. Expensive and it reorders contents, so it is gated on the same 30%
		// removal threshold Qx uses.
		if (state.get()._handles.needsCleanup(QHANDLE_CLEANUP_THRESHOLD_PERCENT))
		{
			state.mut()._handles.cleanup();
		}
		if (state.get()._primary.needsCleanup(QHANDLE_CLEANUP_THRESHOLD_PERCENT))
		{
			state.mut()._primary.cleanup();
		}
		if (state.get()._commitments.needsCleanup(QHANDLE_CLEANUP_THRESHOLD_PERCENT))
		{
			state.mut()._commitments.cleanup();
		}
	}

	// QHandle issues no assets and wants no asset management rights, but the callbacks still
	// need explicit reject stubs rather than being left to a default.
	PRE_ACQUIRE_SHARES()
	{
		output.allowTransfer = false;
	}

	POST_ACQUIRE_SHARES()
	{
	}

	PRE_RELEASE_SHARES()
	{
	}

	POST_RELEASE_SHARES()
	{
	}

	POST_INCOMING_TRANSFER()
	{
		switch (input.type)
		{
		case TransferType::standardTransaction:
			// A bare transfer to this contract buys nothing; send it back rather than
			// silently absorbing it into revenue.
			qpi.transfer(input.sourceId, input.amount);
			break;
		case TransferType::qpiTransfer:
		case TransferType::revenueDonation:
			state.mut()._earnedAmount += input.amount;
			break;
		default:
			break;
		}
	}
};
