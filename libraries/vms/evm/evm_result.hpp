#pragma once

#include <libethereum/Transaction.h>
#include <libethereum/TransactionReceipt.h>

#include <fc/reflect/reflect.hpp>

namespace vms { namespace evm {

struct evm_result {
   dev::eth::ExecutionResult exec_result;
   dev::eth::TransactionReceipt receipt;
   std::string state_root;
};

} }


FC_REFLECT_ENUM( dev::eth::TransactionException, 
                 (None)
                 (Unknown)
                 (BadRLP)
                 (InvalidFormat)
                 (OutOfGasIntrinsic)
                 (InvalidSignature)
                 (InvalidNonce)
                 (NotEnoughCash)
                 (OutOfGasBase)
                 (BlockGasLimitReached)
                 (BadInstruction)
                 (BadJumpDestination)
                 (OutOfGas)
                 (OutOfStack)
                 (StackUnderflow)
                 (RevertInstruction)
                 (InvalidZeroSignatureFormat)
                 (AddressAlreadyUsed) )

FC_REFLECT_ENUM( dev::eth::CodeDeposit,
                 (None)
                 (Failed)
                 (Success) )

FC_REFLECT( dev::Address, (m_data) )
FC_REFLECT( dev::h2048, (m_data) )
FC_REFLECT( dev::h256, (m_data) )

FC_REFLECT( dev::eth::LogEntry,
            (address)
            (topics)
            (data) )

FC_REFLECT( dev::eth::TransactionReceipt,
            (m_statusCodeOrStateRoot)
            (m_gasUsed)
            (m_bloom)
            (m_log) )

FC_REFLECT( dev::eth::ExecutionResult, 
            (gasUsed)
            (excepted)
            (newAddress)
            (output)
            (codeDeposit)
            (gasRefunded)
            (depositSize)
            (gasForDeposit) )

FC_REFLECT( vms::evm::evm_result,
            (exec_result)
            (receipt)
            (state_root) )

