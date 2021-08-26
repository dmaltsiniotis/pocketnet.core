#include "pocketdb/models/dto/Post.h"

namespace PocketTx
{
    Post::Post(const string& hash, int64_t time) : Transaction(hash, time)
    {
        SetType(PocketTxType::CONTENT_POST);
    }

    shared_ptr<UniValue> Post::Serialize() const
    {
        auto result = Transaction::Serialize();

        result->pushKV("address", GetAddress() ? *GetAddress() : "");
        result->pushKV("txidRepost", GetRelayTxHash() ? *GetRelayTxHash() : "");

        // For olf protocol edited content
        // txid     - original content hash
        // txidEdit - actual transaction hash
        if (*GetRootTxHash() == *GetHash())
        {
            result->pushKV("txid", *GetHash());
            result->pushKV("txidEdit", "");
        }
        else
        {
            result->pushKV("txid", *GetRootTxHash());
            result->pushKV("txidEdit", *GetHash());
        }

        result->pushKV("lang", (m_payload && m_payload->GetString1()) ? *m_payload->GetString1() : "en");
        result->pushKV("caption", (m_payload && m_payload->GetString2()) ? *m_payload->GetString2() : "");
        result->pushKV("message", (m_payload && m_payload->GetString3()) ? *m_payload->GetString3() : "");
        result->pushKV("url", (m_payload && m_payload->GetString7()) ? *m_payload->GetString7() : "");
        result->pushKV("settings", (m_payload && m_payload->GetString6()) ? *m_payload->GetString6() : "");

        result->pushKV("type", 0);
        result->pushKV("caption_", "");
        result->pushKV("message_", "");
        result->pushKV("scoreSum", 0);
        result->pushKV("scoreCnt", 0);
        result->pushKV("reputation", 0);

        UniValue vImages(UniValue::VARR);
        if (m_payload && m_payload->GetString5())
            vImages.read(*m_payload->GetString5());
        result->pushKV("images", vImages);

        UniValue vTags(UniValue::VARR);
        if (m_payload && m_payload->GetString4())
            vTags.read(*m_payload->GetString4());
        result->pushKV("tags", vTags);

        return result;
    }

    void Post::Deserialize(const UniValue& src)
    {
        Transaction::Deserialize(src);
        if (auto[ok, val] = TryGetStr(src, "address"); ok) SetAddress(val);
        if (auto[ok, val] = TryGetStr(src, "txidRepost"); ok) SetRelayTxHash(val);

        SetRootTxHash(*GetHash());
        if (auto[ok, valTxIdEdit] = TryGetStr(src, "txidEdit"); ok)
            if (auto[ok, valTxId] = TryGetStr(src, "txid"); ok)
                SetRootTxHash(valTxId);
    }

    void Post::DeserializeRpc(const UniValue& src)
    {
        if (auto[ok, val] = TryGetStr(src, "txAddress"); ok) SetAddress(val);
        if (auto[ok, val] = TryGetStr(src, "txidEdit"); ok) SetRootTxHash(val);
        if (auto[ok, val] = TryGetStr(src, "txidRepost"); ok) SetRelayTxHash(val);

        SetRootTxHash(*GetHash());
        if (auto[ok, val] = TryGetStr(src, "id"); ok)
            SetRootTxHash(val);

        GeneratePayload();

        if (auto[ok, val] = TryGetStr(src, "l"); ok && val.length() == 2) m_payload->SetString1(val);
        else m_payload->SetString1("en");

        if (auto[ok, val] = TryGetStr(src, "c"); ok) m_payload->SetString2(val);
        if (auto[ok, val] = TryGetStr(src, "m"); ok) m_payload->SetString3(val);
        if (auto[ok, val] = TryGetStr(src, "u"); ok) m_payload->SetString7(val);
        if (auto[ok, val] = TryGetStr(src, "s"); ok) m_payload->SetString6(val);
        if (auto[ok, val] = TryGetStr(src, "t"); ok) m_payload->SetString4(val);
        if (auto[ok, val] = TryGetStr(src, "i"); ok) m_payload->SetString5(val);
    }

    shared_ptr<string> Post::GetAddress() const { return m_string1; }
    void Post::SetAddress(string value) { m_string1 = make_shared<string>(value); }

    shared_ptr<string> Post::GetRootTxHash() const { return m_string2; }
    void Post::SetRootTxHash(string value) { m_string2 = make_shared<string>(value); }

    shared_ptr<string> Post::GetRelayTxHash() const { return m_string3; }
    void Post::SetRelayTxHash(string value) { m_string3 = make_shared<string>(value); }

    bool Post::IsEdit() const { return *m_string2 != *m_hash; }

    void Post::DeserializePayload(const UniValue& src)
    {
        Transaction::DeserializePayload(src);

        if (auto[ok, val] = TryGetStr(src, "lang"); ok) m_payload->SetString1(val);
        else m_payload->SetString1("en");

        if (auto[ok, val] = TryGetStr(src, "caption"); ok) m_payload->SetString2(val);
        if (auto[ok, val] = TryGetStr(src, "message"); ok) m_payload->SetString3(val);
        if (auto[ok, val] = TryGetStr(src, "tags"); ok) m_payload->SetString4(val);
        if (auto[ok, val] = TryGetStr(src, "url"); ok) m_payload->SetString7(val);
        if (auto[ok, val] = TryGetStr(src, "images"); ok) m_payload->SetString5(val);
        if (auto[ok, val] = TryGetStr(src, "settings"); ok) m_payload->SetString6(val);
    }

    void Post::BuildHash()
    {
        std::string data;

        data += m_payload->GetString7() ? *m_payload->GetString7() : "";
        data += m_payload->GetString2() ? *m_payload->GetString2() : "";
        data += m_payload->GetString3() ? *m_payload->GetString3() : "";

        if (m_payload->GetString4() && !(*m_payload->GetString4()).empty())
        {
            UniValue tags(UniValue::VARR);
            tags.read(*m_payload->GetString4());
            for (size_t i = 0; i < tags.size(); ++i)
            {
                data += (i ? "," : "");
                data += tags[i].get_str();
            }
        }

        if (m_payload->GetString5() && !(*m_payload->GetString5()).empty())
        {
            UniValue images(UniValue::VARR);
            images.read(*m_payload->GetString5());
            for (size_t i = 0; i < images.size(); ++i)
            {
                data += (i ? "," : "");
                data += images[i].get_str();
            }
        }

        if (GetRootTxHash() && *GetRootTxHash() != *GetHash())
            data += *GetRootTxHash();

        data += GetRelayTxHash() ? *GetRelayTxHash() : "";

        Transaction::GenerateHash(data);
    }
} // namespace PocketTx