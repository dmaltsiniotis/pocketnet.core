// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 Bitcoin developers
// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/repositories/ConsensusRepository.h"

namespace PocketDb
{
    void ConsensusRepository::Init() {}

    void ConsensusRepository::Destroy() {}

    bool ConsensusRepository::ExistsAnotherByName(const string& address, const string& name)
    {
        bool result = false;

        // TODO (brangr): implement sql for first user record - exists
        auto stmt = SetupSqlStatement(R"sql(
            SELECT 1
            FROM vUsersPayload ap
            WHERE ap.Name = ?
                and ap.Height is not null
                and not exists (
                    select 1
                    from vAccounts ac
                    where   ac.Hash = ap.TxHash
                        and a.Height is not null
                        and ac.AddressHash = ?
                )
        )sql");

        TryBindStatementText(stmt, 1, name);
        TryBindStatementText(stmt, 2, address);

        TryTransactionStep(__func__, [&]()
        {
            result = sqlite3_step(*stmt) == SQLITE_ROW;
            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    // Select all user profile edit transaction in chain
    // Transactions.Height is not null
    // TODO (brangr) (v0.21.0): change vUser to vAccounts and pass argument type
    tuple<bool, PTransactionRef> ConsensusRepository::GetLastAccount(const string& address)
    {
        PTransactionRef tx = nullptr;

        auto sql = FullTransactionSql;
        sql += R"sql(
            and t.String1 = ?
            and t.Last = 1
            and t.Height is not null
            and t.Type in (100, 101, 102)
        )sql";

        auto stmt = SetupSqlStatement(sql);
        TryBindStatementText(stmt, 1, address);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, transaction] = CreateTransactionFromListRow(stmt, true); ok)
                    tx = transaction;

            FinalizeSqlStatement(*stmt);
        });

        return {tx != nullptr, tx};
    }

    tuple<bool, PTransactionRef> ConsensusRepository::GetLastContent(const string& rootHash)
    {
        PTransactionRef tx = nullptr;

        auto sql = FullTransactionSql;
        sql += R"sql(
            and t.String2 = ?
            and t.Last = 1
            and t.Height is not null
            and t.Type in (200, 201, 202, 203, 204, 205, 206)
        )sql";

        auto stmt = SetupSqlStatement(sql);
        TryBindStatementText(stmt, 1, rootHash);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, transaction] = CreateTransactionFromListRow(stmt, true); ok)
                    tx = transaction;

            FinalizeSqlStatement(*stmt);
        });

        return {tx != nullptr, tx};
    }

    // TODO (brangr) (v0.21.0): change for vAccounts and pass type as argument
    bool ConsensusRepository::ExistsUserRegistrations(vector<string>& addresses, bool mempool)
    {
        auto result = false;

        if (addresses.empty())
            return result;

        // Build sql string
        string sql = R"sql(
            SELECT count(distinct(AddressHash))
            FROM vUsers
            WHERE 1=1
        )sql";

        sql += " and AddressHash in ( ";
        sql += join(vector<string>(addresses.size(), "?"), ",");
        sql += " ) ";

        if (!mempool)
            sql += " and Height is not null";

        // Compile sql
        auto stmt = SetupSqlStatement(sql);

        // Bind values
        for (size_t i = 0; i < addresses.size(); i++)
            TryBindStatementText(stmt, i + 1, addresses[i]);

        // Execute
        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = (value == (int) addresses.size());

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    tuple<bool, PocketTxType> ConsensusRepository::GetLastBlockingType(const string& address, const string& addressTo)
    {
        bool blockingExists = false;
        PocketTxType blockingType = PocketTxType::NOT_SUPPORTED;

        auto stmt = SetupSqlStatement(R"sql(
            SELECT b.Type
            FROM vBlockings b
            WHERE b.AddressHash = ?
                and b.AddressToHash = ?
                and b.Height is not null
                and b.Last = 1
            LIMIT 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementText(stmt, 2, addressTo);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                {
                    blockingExists = true;
                    blockingType = (PocketTxType) value;
                }
            }

            FinalizeSqlStatement(*stmt);
        });

        return {blockingExists, blockingType};
    }

    tuple<bool, PocketTxType> ConsensusRepository::GetLastSubscribeType(const string& address,
                                                                        const string& addressTo)
    {
        bool subscribeExists = false;
        PocketTxType subscribeType = PocketTxType::NOT_SUPPORTED;

        auto stmt = SetupSqlStatement(R"sql(
            SELECT s.Type
            FROM vSubscribes s
            WHERE s.AddressHash = ?
                and s.AddressToHash = ?
                and s.Height is not null
                and s.Last = 1
            LIMIT 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementText(stmt, 2, addressTo);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                {
                    subscribeExists = true;
                    subscribeType = (PocketTxType) value;
                }
            }

            FinalizeSqlStatement(*stmt);
        });

        return {subscribeExists, subscribeType};
    }

    // TODO (brangr) (v0.21.0): change for vContents and pass type as argument
    shared_ptr<string> ConsensusRepository::GetPostAddress(const string& postHash)
    {
        shared_ptr<string> result = nullptr;

        auto stmt = SetupSqlStatement(R"sql(
            SELECT p.AddressHash
            FROM vPosts p
            WHERE   p.Hash = ?
                and p.Height is not null
        )sql");

        TryBindStatementText(stmt, 1, postHash);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnString(*stmt, 0); ok)
                    result = make_shared<string>(value);

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    bool ConsensusRepository::ExistsComplain(const string& txHash, const string& postHash, const string& address)
    {
        bool result = false;

        auto stmt = SetupSqlStatement(R"sql(
            SELECT 1
            FROM vComplains c
            WHERE c.AddressHash = ?
                and c.PostTxHash = ?
                and c.Hash != ?
                and c.Height is not null
            LIMIT 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementText(stmt, 2, postHash);
        TryBindStatementText(stmt, 3, txHash);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = true;
            }

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }


    bool ConsensusRepository::ExistsScore(const string& address, const string& contentHash,
                                          PocketTxType type, bool mempool)
    {
        bool result = false;

        string sql = R"sql(
            SELECT 1
            FROM vScores s
            WHERE   s.AddressHash = ?
                and s.ContentTxHash = ?
                and s.Type = ?
        )sql";

        if (!mempool)
            sql += " and s.Height is not null";

        auto stmt = SetupSqlStatement(sql);
        TryBindStatementText(stmt, 1, address);
        TryBindStatementText(stmt, 2, contentHash);
        TryBindStatementInt(stmt, 3, (int) type);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = true;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    int64_t ConsensusRepository::GetUserBalance(const string& address)
    {
        int64_t result = 0;

        auto sql = R"sql(
            SELECT SUM(o.Value)
            FROM TxOutputs o
            JOIN Transactions t ON o.TxHash == t.Hash and t.Height is not null
            WHERE o.SpentHeight is null
                AND o.AddressHash = ?
        )sql";

        auto stmt = SetupSqlStatement(sql);
        TryBindStatementText(stmt, 1, address);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 0); ok)
                    result = value;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    int ConsensusRepository::GetUserReputation(const string& address)
    {
        int result = 0;

        auto sql = R"sql(
                select r.Value
                from Ratings r
                where r.Type = ?
                    and r.Id = (SELECT u.Id FROM vUsers u WHERE u.Height is not null and u.Last = 1 AND u.AddressHash = ? LIMIT 1)
                order by r.Height desc
                limit 1
            )sql";

        auto stmt = SetupSqlStatement(sql);
        TryBindStatementInt(stmt, 1, (int) RatingType::RATING_ACCOUNT);
        TryBindStatementText(stmt, 2, address);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = value;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    int ConsensusRepository::GetUserReputation(int addressId)
    {
        int result = 0;

        auto stmt = SetupSqlStatement(R"sql(
            select r.Value
            from Ratings r
            where r.Type = ?
                and r.Id = ?
            order by r.Height desc
            limit 1
        )sql");

        TryBindStatementInt(stmt, 1, (int) RatingType::RATING_ACCOUNT);
        TryBindStatementInt(stmt, 2, addressId);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = value;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    // Selects for get models data
    shared_ptr<ScoreDataDto> ConsensusRepository::GetScoreData(const string& txHash)
    {
        shared_ptr<ScoreDataDto> result = nullptr;

        auto stmt = SetupSqlStatement(R"sql(
            select
                s.Hash sTxHash,
                s.Type sType,
                s.Time sTime,
                s.Value sValue,
                sa.Id saId,
                sa.AddressHash saHash,
                c.Hash cTxHash,
                c.Type cType,
                c.Time cTime,
                c.Id cId,
                ca.Id caId,
                ca.AddressHash caHash
            from vScores s
                join vAccounts sa on sa.Height is not null and sa.AddressHash=s.AddressHash
                join vContents c on c.Height is not null and c.Hash=s.ContentTxHash
                join vAccounts ca on ca.Height is not null and ca.AddressHash=c.AddressHash
            where s.Hash = ? and s.Height is not null
            limit 1
        )sql");
        TryBindStatementText(stmt, 1, txHash);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                ScoreDataDto data;

                if (auto[ok, value] = TryGetColumnString(*stmt, 0); ok) data.ScoreTxHash = value;
                if (auto[ok, value] = TryGetColumnInt(*stmt, 1); ok) data.ScoreType = (PocketTxType) value;
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 2); ok) data.ScoreTime = value;
                if (auto[ok, value] = TryGetColumnInt(*stmt, 3); ok) data.ScoreValue = value;
                if (auto[ok, value] = TryGetColumnInt(*stmt, 4); ok) data.ScoreAddressId = value;
                if (auto[ok, value] = TryGetColumnString(*stmt, 5); ok) data.ScoreAddressHash = value;

                if (auto[ok, value] = TryGetColumnString(*stmt, 6); ok) data.ContentTxHash = value;
                if (auto[ok, value] = TryGetColumnInt(*stmt, 7); ok) data.ContentType = (PocketTxType) value;
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 8); ok) data.ContentTime = value;
                if (auto[ok, value] = TryGetColumnInt(*stmt, 9); ok) data.ContentId = value;
                if (auto[ok, value] = TryGetColumnInt(*stmt, 10); ok) data.ContentAddressId = value;
                if (auto[ok, value] = TryGetColumnString(*stmt, 11); ok) data.ContentAddressHash = value;

                result = make_shared<ScoreDataDto>(data);
            }

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    // Select many referrers
    shared_ptr<map<string, string>> ConsensusRepository::GetReferrers(const vector<string>& addresses, int minHeight)
    {
        shared_ptr<map<string, string>> result = make_shared<map<string, string>>();

        if (addresses.empty())
            return result;

        // Build sql string
        string sql = R"sql(
            select u.AddressHash, ifnull(u.ReferrerAddressHash,'')
            from vUsers u
            where u.Height is not null
                and u.Height >= ?
                and u.Height = (select min(u1.Height) from vUsers u1 where u1.Height is not null and u1.AddressHash=u.AddressHash)
                and u.ReferrerAddressHash is not null
        )sql";

        sql += " and u.AddressHash in ( ";
        sql += join(vector<string>(addresses.size(), "?"), ",");
        sql += " ) ";

        // Compile sql
        auto stmt = SetupSqlStatement(sql);

        // Bind values
        TryBindStatementInt(stmt, 1, minHeight);
        for (size_t i = 0; i < addresses.size(); i++)
            TryBindStatementText(stmt, i + 2, addresses[i]);

        // Execute
        TryTransactionStep(__func__, [&]()
        {
            while (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                if (auto[ok1, value1] = TryGetColumnString(*stmt, 1); ok1 && !value1.empty())
                    if (auto[ok2, value2] = TryGetColumnString(*stmt, 2); ok2 && !value2.empty())
                        result->emplace(value1, value2);
            }

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    // Select referrer for one account
    shared_ptr<string> ConsensusRepository::GetReferrer(const string& address, int minTime)
    {
        shared_ptr<string> result;

        auto stmt = SetupSqlStatement(R"sql(
            select ReferrerAddressHash
            from vUsers
            where Height is not null
                and Time >= ?
                and AddressHash = ?
            order by Height asc
            limit 1
        )sql");
        TryBindStatementInt(stmt, 1, minTime);
        TryBindStatementText(stmt, 2, address);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
            {
                if (auto[ok, value] = TryGetColumnString(*stmt, 0); ok && !value.empty())
                    result = make_shared<string>(value);
            }

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    int ConsensusRepository::GetUserLikersCount(int addressId)
    {
        int result = 0;

        auto stmt = SetupSqlStatement(R"sql(
            select count(1)
            from Ratings r
            where   r.Type = ?
                and r.Id = ?
        )sql");

        TryBindStatementInt(stmt, 1, (int) RatingType::RATING_ACCOUNT_LIKERS);
        TryBindStatementInt(stmt, 2, addressId);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = value;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    int ConsensusRepository::GetScoreContentCount(PocketTxType scoreType, PocketTxType contentType,
                                                  const string& scoreAddress, const string& contentAddress,
                                                  int height, const CTransactionRef& tx,
                                                  const std::vector<int>& values,
                                                  int64_t scoresOneToOneDepth)
    {
        int result = 0;

        if (values.empty())
            return result;

        // Build sql string
        string sql = R"sql(
            select count(1)
            from vScores s -- indexed by Transactions_GetScoreContentCount
            join vContents c -- indexed by Transactions_GetScoreContentCount_2
                on c.Type = ? and c.Hash = s.ContentTxHash and c.AddressHash = ?
                    and c.Height is not null and c.Height <= ?
            where   s.AddressHash = ?
                and s.Height is not null
                and s.Height <= ?
                and s.Time < ?
                and s.Time >= ?
                and s.Hash != ?
                and s.Type = ?
        )sql";

        sql += " and s.Value in (";
        sql += join(values | transformed(static_cast<std::string(*)(int)>(std::to_string)), ",");
        sql += " ) ";

        // Compile sql
        auto stmt = SetupSqlStatement(sql);

        // Bind values
        TryBindStatementInt(stmt, 1, contentType);
        TryBindStatementText(stmt, 2, contentAddress);
        TryBindStatementInt(stmt, 3, height);
        TryBindStatementText(stmt, 4, scoreAddress);
        TryBindStatementInt(stmt, 5, height);
        TryBindStatementInt64(stmt, 6, tx->nTime);
        TryBindStatementInt64(stmt, 7, (int64_t) tx->nTime - scoresOneToOneDepth);
        TryBindStatementText(stmt, 8, tx->GetHash().GetHex());
        TryBindStatementInt(stmt, 9, scoreType);

        // Execute
        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt(*stmt, 0); ok)
                    result = value;

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    tuple<bool, int64_t> ConsensusRepository::GetLastAccountHeight(const string& address)
    {
        tuple<bool, int64_t> result = {false, 0};

        auto stmt = SetupSqlStatement(R"sql(
            select a.Height
            from vAccounts a
            where   a.AddressHash = ?
                and a.Last = 1
                and a.Height is not null
        )sql");
        TryBindStatementText(stmt, 1, address);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 0); ok)
                    result = {true, value};

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }

    tuple<bool, int64_t> ConsensusRepository::GetTransactionHeight(const string& hash)
    {
        tuple<bool, int64_t> result = {false, 0};

        auto stmt = SetupSqlStatement(R"sql(
            select t.Height
            from Transactions t
            where   t.Hash = ?
                and t.Height is not null
        )sql");
        TryBindStatementText(stmt, 1, hash);

        TryTransactionStep(__func__, [&]()
        {
            if (sqlite3_step(*stmt) == SQLITE_ROW)
                if (auto[ok, value] = TryGetColumnInt64(*stmt, 0); ok)
                    result = {true, value};

            FinalizeSqlStatement(*stmt);
        });

        return result;
    }


    // Mempool counts

    int ConsensusRepository::CountMempoolBlocking(const string& address, const string& addressTo)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from Blockings
            where Height is null
                and AddressHash = ?
                and AddressToHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementText(stmt, 2, addressTo);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountMempoolSubscribe(const string& address, const string& addressTo)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vSubscribes
            where Height is null
                and AddressHash = ?
                and AddressToHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementText(stmt, 2, addressTo);

        return GetCount(__func__, stmt);
    }


    int ConsensusRepository::CountMempoolComment(const string& address)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComments
            where Height is null
                and AddressHash = ?
                and Type = 204
        )sql");

        TryBindStatementText(stmt, 1, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainCommentTime(const string& address, int64_t time)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComments
            where Height is not null
                and Time >= ?
                and AddressHash = ?
                and Type = 204
                and Last = 1
        )sql");

        TryBindStatementInt64(stmt, 1, time);
        TryBindStatementText(stmt, 2, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainCommentHeight(const string& address, int height)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComments
            where Height is not null
                and Height >= ?
                and AddressHash = ?
                and Type = 204
                and Last = 1
        )sql");

        TryBindStatementInt(stmt, 1, height);
        TryBindStatementText(stmt, 2, address);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolComplain(const string& address)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComplains
            where Height is null
                and AddressHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainComplainTime(const string& address, int64_t time)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComplains
            where Height is not null
                and Time >= ?
                and Last = 1
                and AddressHash = ?
        )sql");

        TryBindStatementInt64(stmt, 1, time);
        TryBindStatementText(stmt, 2, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainComplainHeight(const string& address, int height)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComplains
            where Height is not null
                and Height >= ?
                and Last = 1
                and AddressHash = ?
        )sql");

        TryBindStatementInt(stmt, 1, height);
        TryBindStatementText(stmt, 2, address);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolPost(const string& address)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vPosts
            where Height is null
                and AddressHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainPostTime(const string& address, int64_t time)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vPosts
            where Height is not null
                and AddressHash = ?
                and Time >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt64(stmt, 2, time);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainPostHeight(const string& address, int height)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vPosts
            where Height is not null
                and AddressHash = ?
                and Height >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt(stmt, 2, height);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolScoreComment(const string& address)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vScoreComments
            where Height is null
                and AddressHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainScoreCommentTime(const string& address, int64_t time)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vScoreComments
            where Height is not null
                and AddressHash = ?
                and Time >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt64(stmt, 2, time);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainScoreCommentHeight(const string& address, int height)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vScoreComments
            where Height is not null
                and AddressHash = ?
                and Height >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt(stmt, 2, height);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolScoreContent(const string& address)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vScoreContents
            where Height is null
                and AddressHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainScoreContentTime(const string& address, int64_t time)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vScoreContents
            where Height is not null
                and AddressHash = ?
                and Time >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt64(stmt, 2, time);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainScoreContentHeight(const string& address, int height)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vScoreContents
            where Height is not null
                and AddressHash = ?
                and Height >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt(stmt, 2, height);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolUser(const string& address)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vUsers
            where Height is null
                and AddressHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainUserTime(const string& address, int64_t time)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vUsers
            where Height is not null
                and AddressHash = ?
                and Time >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt64(stmt, 2, time);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainUserHeight(const string& address, int height)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vUsers
            where Height is not null
                and AddressHash = ?
                and Height >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt(stmt, 2, height);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolVideo(const string& address)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vVideos
            where Height is null
                and AddressHash = ?
        )sql");

        TryBindStatementText(stmt, 1, address);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainVideoTime(const string& address, int64_t time)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vVideos
            where Height is not null
                and AddressHash = ?
                and Time >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt64(stmt, 2, time);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainVideoHeight(const string& address, int height)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vVideos
            where Height is not null
                and AddressHash = ?
                and Height >= ?
                and Last = 1
        )sql");

        TryBindStatementText(stmt, 1, address);
        TryBindStatementInt(stmt, 2, height);

        return GetCount(__func__, stmt);
    }

    // EDITS

    int ConsensusRepository::CountMempoolCommentEdit(const string& rootTxHash)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComments
            where Height is null
                and RootTxHash = ?
        )sql");

        TryBindStatementText(stmt, 1, rootTxHash);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainCommentEdit(const string& rootTxHash)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vComments
            where Height is not null
                and RootTxHash = ?
        )sql");

        TryBindStatementText(stmt, 1, rootTxHash);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolPostEdit(const string& rootTxHash)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vPosts
            where Height is null
                and RootTxHash = ?
        )sql");

        TryBindStatementText(stmt, 1, rootTxHash);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainPostEdit(const string& rootTxHash)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vPosts
            where Height is not null
                and RootTxHash = ?
                and Hash != RootTxHash
        )sql");

        TryBindStatementText(stmt, 1, rootTxHash);

        return GetCount(__func__, stmt);
    }

    int ConsensusRepository::CountMempoolVideoEdit(const string& rootTxHash)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vVideos
            where Height is null
                and RootTxHash = ?
        )sql");

        TryBindStatementText(stmt, 1, rootTxHash);

        return GetCount(__func__, stmt);
    }
    int ConsensusRepository::CountChainVideoEdit(const string& rootTxHash)
    {
        auto stmt = SetupSqlStatement(R"sql(
            select count(*)
            from vVideos
            where Height is not null
                and RootTxHash = ?
                and Hash != RootTxHash
        )sql");

        TryBindStatementText(stmt, 1, rootTxHash);

        return GetCount(__func__, stmt);
    }

}