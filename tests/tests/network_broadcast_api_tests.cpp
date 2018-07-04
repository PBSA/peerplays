

/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <boost/test/unit_test.hpp>

#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/protocol/proposal.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace
{
    transfer_operation make_transfer_operation(const account_id_type& from, const account_id_type& to, const asset& amount)
    {
        transfer_operation transfer;
        transfer.from = from;
        transfer.to   = to;
        transfer.amount = amount;
        
        return transfer;
    }
    
    void propose_operation(database_fixture& fixture, const operation& op)
    {
        signed_transaction transaction;
        set_expiration(fixture.db, transaction);
        
        transaction.operations = {op};
        
        fixture.db.create<proposal_object>([&](proposal_object& proposal)
        {
            proposal.proposed_transaction = transaction;
        });
    }
    
    struct proposed_operations_digest_accumulator
    {
        typedef void result_type;
        
        template<class T>
        void operator()(const T&) const
        {}
        
        void operator()(const proposal_create_operation& proposal) const
        {
            for (auto& operation: proposal.proposed_ops)
            {
                proposed_operations_digests.push_back(fc::digest(operation.op));
            }
        }
        
        mutable std::vector<fc::sha256> proposed_operations_digests;
    };
    
    void check_transactions_duplicates(database& db, const signed_transaction& transaction)
    {
        const auto& proposal_index = db.get_index<proposal_object>();
        std::set<fc::sha256> existed_operations_digests;
        
        proposal_index.inspect_all_objects( [&](const object& obj){
            const proposal_object& proposal = static_cast<const proposal_object&>(obj);
            for (auto& operation: proposal.proposed_transaction.operations)
            {
                existed_operations_digests.insert(fc::digest(operation));
            }
        });
        
        proposed_operations_digest_accumulator digest_accumulator;
        for (auto& operation: transaction.operations)
        {
            operation.visit(digest_accumulator);
        }
        
        for (auto& digest: digest_accumulator.proposed_operations_digests)
        {
            FC_ASSERT(existed_operations_digests.count(digest) == 0, "Proposed operation is already pending for approval.");
        }
    }
}

BOOST_FIXTURE_TEST_SUITE( check_transactions_duplicates_tests, database_fixture )

BOOST_AUTO_TEST_CASE( test_exception_throwing_for_the_same_operation_proposed_twice )
{
    try
    {
        ACTORS((alice))
        
        ::propose_operation(*this, make_transfer_operation(account_id_type(), alice_id, asset(500)));
        
        proposal_create_operation operation_proposal;
        operation_proposal.proposed_ops = {op_wrapper(make_transfer_operation(account_id_type(), alice_id, asset(500)))};
        
        signed_transaction trx;
        set_expiration( db, trx );
        trx.operations = {operation_proposal};
        
        BOOST_CHECK_THROW(check_transactions_duplicates(db, trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( test_check_passes_without_duplication )
{
    try
    {
        ACTORS((alice))
        
        proposal_create_operation operation_proposal;
        operation_proposal.proposed_ops = {op_wrapper(make_transfer_operation(account_id_type(), alice_id, asset(500)))};
        
        signed_transaction trx;
        set_expiration( db, trx );
        trx.operations = {operation_proposal};
        
        BOOST_CHECK_NO_THROW(check_transactions_duplicates(db, trx));
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( test_exception_throwing_for_the_same_operation_with_different_assets )
{
    try
    {
        ACTORS((alice))
        
        ::propose_operation(*this, make_transfer_operation(account_id_type(), alice_id, asset(500)));
        
        proposal_create_operation operation_proposal;
        operation_proposal.proposed_ops = {op_wrapper(make_transfer_operation(account_id_type(), alice_id, asset(501)))};
        
        signed_transaction trx;
        set_expiration( db, trx );
        trx.operations = {operation_proposal};
        
        BOOST_CHECK_NO_THROW(check_transactions_duplicates(db, trx));
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_SUITE_END()
