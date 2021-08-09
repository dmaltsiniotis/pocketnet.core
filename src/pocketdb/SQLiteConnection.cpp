// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/SQLiteConnection.h"

namespace PocketDb
{
    SQLiteConnection::SQLiteConnection()
    {
        SQLiteDbInst = make_shared<SQLiteDatabase>(false, true);
        SQLiteDbInst->Init(
            (GetDataDir() / "pocketdb").string(),
            (GetDataDir() / "pocketdb" / "main.sqlite3").string()
        );

        WebRepoInst = make_shared<WebRepository>(*SQLiteDbInst);
        WebUserRepoInst = make_shared<WebUserRepository>(*SQLiteDbInst);
        ExplorerRepoInst = make_shared<ExplorerRepository>(*SQLiteDbInst);
    }

    SQLiteConnection::~SQLiteConnection()
    {
        SQLiteDbInst->m_connection_mutex.lock();

        WebRepoInst->Destroy();
        WebUserRepoInst->Destroy();
        ExplorerRepoInst->Destroy();

        SQLiteDbInst->m_connection_mutex.unlock();
        SQLiteDbInst->Close();
    }


} // namespace PocketDb