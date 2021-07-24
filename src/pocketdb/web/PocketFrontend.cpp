// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 Bitcoin developers
// Copyright (c) 2018-2021 Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#include "pocketdb/web/PocketFrontend.h"

namespace PocketWeb
{
    using namespace std;

    tuple<bool, string> PocketFrontend::ReadFileFromDisk(const string& path)
    {
        try
        {
            string _path = (_rootPath / path).string();

            ifstream file(_path);
            if (file.good())
            {
                string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
                return {true, content};
            }

            return {false, ""};
        }
        catch (const std::exception& e)
        {
            LogPrintf("Warning: failed read file %s with error %s\n", path, e.what());
            return {false, ""};
        }
    }

    tuple<bool, shared_ptr<StaticFile>> PocketFrontend::ReadFile(const string& path)
    {
        // Try read file from disk
        auto[readOk, content] = ReadFileFromDisk(path);
        if (!readOk)
            return {false, nullptr};

        // Split parts for detect name
        auto _name = path;
        std::vector <std::string> pathParts;
        boost::split(pathParts, path, boost::is_any_of("/"));
        if (pathParts.size() > 1)
            _name = pathParts.back();

        // Build file struct
        auto file = shared_ptr<StaticFile>(new StaticFile{
            path,
            _name,
            DetectContentType(_name),
            content
        });

        return {true, file};
    }

    string PocketFrontend::DetectContentType(string fileName)
    {
        auto _extension = fileName;

        vector <string> nameParts;
        boost::split(nameParts, fileName, boost::is_any_of("."));
        if (nameParts.size() > 1)
            _extension = nameParts.back();

        if (MimeTypes.find(_extension) != MimeTypes.end())
            return MimeTypes[_extension];

        return MimeTypes["default"];
    }

    void PocketFrontend::Init()
    {
        _rootPath = GetDataDir() / "web";

        auto testContent = shared_ptr<StaticFile>(new StaticFile{
            "/test.html",
            "test.html",
            "",
            "<html><head><script src='main.js'></script></head><body>Hello World!</body></html>"
        });

        LOCK(CacheMutex);
        Cache.emplace(
            "/test.html",
            testContent
        );
    }

    void PocketFrontend::ClearCache()
    {
        LOCK(CacheMutex);
        Cache.clear();

        LogPrint(BCLog::RESTFRONTEND, "Cache cleared\n");
    }

    void PocketFrontend::CacheEmplace(const string& path, shared_ptr <StaticFile>& content)
    {
        // TODO (brangr): check content cacheable

        LOCK(CacheMutex);
        if (Cache.find(path) == Cache.end())
        {
            LogPrint(BCLog::RESTFRONTEND, "File '%s' emplaced in cache\n", path);
            Cache.emplace(path, content);
        }
    }

    tuple<bool, shared_ptr<StaticFile>> PocketFrontend::CacheGet(const string& path)
    {
        LOCK(CacheMutex);
        if (Cache.find(path) != Cache.end())
        {
            LogPrint(BCLog::RESTFRONTEND, "File '%s' found in cache\n", path);
            return {true, Cache.at(path)};
        }

        return {false, nullptr};
    }

    tuple <HTTPStatusCode, shared_ptr<StaticFile>> PocketFrontend::GetFile(const string& path, bool stopRecurse)
    {
        if (path.find("..") != std::string::npos)
            return {HTTP_BAD_REQUEST, nullptr};

        if (path.empty())
            return GetFile("/index.html", true);

        // Remove parameters - index.html?v2
        auto _path = path;
        std::vector <std::string> pathPrms;
        boost::split(pathPrms, path, boost::is_any_of("?"));
        if (pathPrms.size() > 1)
            _path = pathPrms.front();

        if (auto[ok, cacheContent] = CacheGet(_path); ok)
            return {HTTP_OK, cacheContent};

        // Return HTTP_FORBIDDEN if file too large or in blocked
        // TODO (brangr): Check restrictions

        // Read file and return answer with HTTP_OK
        auto[readOk, fileContent] = ReadFile(_path);
        if (!readOk)
        {
            if (!stopRecurse)
                return GetFile("/index.html", true);

            return {HTTP_NOT_FOUND, nullptr};
        }

        // Save in cache for future
        CacheEmplace(_path, fileContent);

        LogPrint(BCLog::RESTFRONTEND, "File '%s' readed from disk\n", _path);

        return {HTTP_OK, fileContent};
    }

} // namespace PocketWeb