#ifndef POCKETDB_TRANSACTIONREPOSITORY_HPP
#define POCKETDB_TRANSACTIONREPOSITORY_HPP

#include "pocketdb/pocketnet.h"
#include "pocketdb/repositories/BaseRepository.hpp"
#include "pocketdb/models/base/Block.hpp"
#include "pocketdb/models/base/Transaction.hpp"

namespace PocketDb
{
    using namespace PocketTx;

    class TransactionRepository : public BaseRepository
    {
    public:
        explicit TransactionRepository(SQLiteDatabase &db) : BaseRepository(db)
        {
        }

        void Init() override
        {
            SetupSqlStatements();
        }

        bool Insert(const shared_ptr<Transaction> &transaction)
        {
            assert(m_database.m_db);
            auto result = true;

            // First set transaction
            if (TryBindInsertTransactionStatement(m_insert_transaction_stmt, transaction))
            {
                result &= TryStepStatement(m_insert_transaction_stmt);

                sqlite3_clear_bindings(m_insert_transaction_stmt);
                sqlite3_reset(m_insert_transaction_stmt);
            }

            // Second set payload
            if (TryBindInsertPayloadStatement(m_insert_payload_stmt, transaction))
            {
                result &= TryStepStatement(m_insert_payload_stmt);

                sqlite3_clear_bindings(m_insert_payload_stmt);
                sqlite3_reset(m_insert_payload_stmt);
            }

            return result;
        }

        bool BulkInsert(const std::vector<shared_ptr<Transaction>> &transactions)
        {
            assert(m_database.m_db);

            if (!m_database.BeginTransaction())
                return false;

            try
            {
                for (const auto &transaction : transactions)
                {
                    if (!Insert(transaction))
                        throw std::runtime_error(strprintf("%s: can't insert in transaction\n", __func__));
                }

                if (!m_database.CommitTransaction())
                    throw std::runtime_error(strprintf("%s: can't commit transaction\n", __func__));

            } catch (std::exception &ex)
            {
                m_database.AbortTransaction();
                return false;
            }

            return true;
        }

        // TODO (brangr): implement
        void Delete(const shared_ptr<std::string> &id)
        {
            if (!m_database.m_db)
            {
                throw std::runtime_error(strprintf("SQLiteDatabase: database didn't opened\n"));
            }

            if (!TryBindStatementText(m_delete_transaction_stmt, 1, id))
            {
                //TODO
                return;
            }

            int res = sqlite3_step(m_delete_transaction_stmt);
            if (res != SQLITE_DONE)
            {
                LogPrintf("%s: Unable to execute statement: %s\n", __func__, sqlite3_errstr(res));
            }

            sqlite3_clear_bindings(m_delete_transaction_stmt);
            sqlite3_reset(m_delete_transaction_stmt);
        }

    private:
        sqlite3_stmt *m_insert_transaction_stmt{nullptr};
        sqlite3_stmt *m_delete_transaction_stmt{nullptr};

        sqlite3_stmt *m_insert_payload_stmt{nullptr};
        sqlite3_stmt *m_delete_payload_stmt{nullptr};

        void SetupSqlStatements()
        {
            m_insert_transaction_stmt = SetupSqlStatement(
                m_insert_transaction_stmt,
                " INSERT INTO Transactions ("
                "   TxType,"
                "   TxId,"
                "   Block,"
                "   TxOut,"
                "   TxTime,"
                "   Address,"
                "   Int1,"
                "   Int2,"
                "   Int3,"
                "   Int4,"
                "   Int5,"
                "   String1,"
                "   String2,"
                "   String3,"
                "   String4,"
                "   String5)"
                " SELECT ?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?"
                " WHERE not exists (select 1 from Transactions t where t.TxId = ?)"
                " ;");

            m_delete_transaction_stmt = SetupSqlStatement(
                m_delete_transaction_stmt,
                " DELETE FROM Transactions"
                " WHERE TxId = ?"
                " ;");

            // TODO (brangr): update stmt for transaction

            m_insert_payload_stmt = SetupSqlStatement(
                m_insert_payload_stmt,
                " INSERT INTO Payload ("
                " TxID,"
                " Data)"
                " SELECT ?,?"
                " WHERE not exists (select 1 from Payload p where p.TxId = ?)"
                " ;");

            m_delete_payload_stmt = SetupSqlStatement(
                m_delete_payload_stmt,
                " DELETE FROM PAYLOAD"
                " WHERE TxId = ?"
                " ;");
        }

        static bool TryBindInsertTransactionStatement(sqlite3_stmt *stmt, const shared_ptr<Transaction> &transaction)
        {
            auto result = TryBindStatementInt(stmt, 1, transaction->GetTxTypeInt());
            result &= TryBindStatementText(stmt, 2, transaction->GetTxId());
            result &= TryBindStatementInt64(stmt, 3, transaction->GetBlock());
            result &= TryBindStatementInt64(stmt, 4, transaction->GetTxOut());
            result &= TryBindStatementInt64(stmt, 5, transaction->GetTxTime());
            result &= TryBindStatementText(stmt, 6, transaction->GetAddress());
            result &= TryBindStatementInt64(stmt, 7, transaction->GetInt1());
            result &= TryBindStatementInt64(stmt, 8, transaction->GetInt2());
            result &= TryBindStatementInt64(stmt, 9, transaction->GetInt3());
            result &= TryBindStatementInt64(stmt, 10, transaction->GetInt4());
            result &= TryBindStatementInt64(stmt, 11, transaction->GetInt5());
            result &= TryBindStatementText(stmt, 12, transaction->GetString1());
            result &= TryBindStatementText(stmt, 13, transaction->GetString2());
            result &= TryBindStatementText(stmt, 14, transaction->GetString3());
            result &= TryBindStatementText(stmt, 15, transaction->GetString4());
            result &= TryBindStatementText(stmt, 16, transaction->GetString5());
            result &= TryBindStatementText(stmt, 17, transaction->GetTxId());

            if (!result)
                sqlite3_clear_bindings(stmt);

            return result;
        }

        static bool TryBindInsertPayloadStatement(sqlite3_stmt *stmt, const shared_ptr<Transaction> &transaction)
        {
            auto result = TryBindStatementText(stmt, 1, transaction->GetTxId());
            result &= TryBindStatementText(stmt, 2, transaction->GetPayloadStr());
            result &= TryBindStatementText(stmt, 3, transaction->GetTxId());

            if (!result)
                sqlite3_clear_bindings(stmt);

            return result;
        }

        static bool TryStepStatement(sqlite3_stmt *stmt)
        {
            int res = sqlite3_step(stmt);
            if (res != SQLITE_ROW && res != SQLITE_DONE)
                LogPrintf("%s: Unable to execute statement: %s: %s\n",
                    __func__, sqlite3_sql(stmt), sqlite3_errstr(res));

            return !(res != SQLITE_ROW && res != SQLITE_DONE);
        }
    };

} // namespace PocketDb

#endif // POCKETDB_TRANSACTIONREPOSITORY_HPP
