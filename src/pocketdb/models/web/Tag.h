// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETDB_MODEL_WEB_TAG_H
#define POCKETDB_MODEL_WEB_TAG_H

#include <string>
#include "pocketdb/models/base/PocketTypes.h"

namespace PocketDbWeb
{
    using namespace std;
    using namespace PocketTx;

    struct Tag
    {
        int64_t ContentId;
        string Value;

        Tag(int64_t contentId, const string& value);
    };

} // PocketDbWeb

#endif //POCKETDB_MODEL_WEB_TAG_H