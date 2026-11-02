#include "Nexus_fetchList.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <nlohmann/json.hpp>

#include <limits>

namespace {
using Json = nlohmann::json;
// Contract: https://graphql.nexusmods.com/#query-mods (offset/count, newest updates first).
constexpr auto pageQuery = R"(
query ModPage($offset: Int!, $count: Int!) {
  mods(filter: {gameDomainName: [{value: "stardewvalley", op: EQUALS}]},
       sort: [{updatedAt: {direction: DESC}}], offset: $offset, count: $count) {
    nodes { modId name summary version author pictureUrl endorsements }
    totalCount
  }
})";
}

QNetworkReply* NexusModClient::fetchModPage(int offset) {
    if (offset < 0 || offset > std::numeric_limits<int>::max() - pageSize) {
        emit requestFailed({"Invalid mod page offset.", 0});
        return nullptr;
    }
    const Json body = {{"query", pageQuery},
                       {"variables", {{"offset", offset}, {"count", pageSize}}}};
    auto* reply = request(QUrl("https://api.nexusmods.com/v2/graphql"),
                          QByteArray::fromStdString(body.dump()));
    if (!reply) return nullptr;
    connect(reply, &QNetworkReply::finished, this, [this, reply, offset] {
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            emit requestFailed({"Unable to load the Nexus mod page.", status});
        } else {
            QString error;
            const auto page = parseModPageResponse(reply->readAll(), offset, &error);
            if (page) emit modPageFetched(*page);
            else emit requestFailed({error, status});
        }
        reply->deleteLater();
    });
    return reply;
}

std::optional<NexusModPage> NexusModClient::parseModPageResponse(
    const QByteArray& response, int offset, QString* error) {
    auto fail = [error](const QString& message) -> std::optional<NexusModPage> {
        if (error) *error = message;
        return std::nullopt;
    };
    try {
        const auto document = Json::parse(response.cbegin(), response.cend(), nullptr, false);
        if (!document.is_object() || offset < 0) return fail("Invalid Nexus mod page response.");
        if (document.contains("errors") && document["errors"] != Json::array()) {
            // Do not expose remote messages, request headers or partial results.
            return fail("Nexus could not complete the mod list query. Please retry.");
        }
        const auto& page = document.at("data").at("mods");
        const auto& total = page.at("totalCount");
        const auto& nodes = page.at("nodes");
        if (!total.is_number_integer() || total < 0 ||
            total > std::numeric_limits<int>::max() || !nodes.is_array() ||
            nodes.size() > pageSize || nodes.size() > total.get<std::size_t>()) {
            return fail("Invalid Nexus mod page metadata.");
        }
        Json summaries = Json::array();
        for (auto node : nodes) {
            if (!node.is_object() || !node.contains("modId") ||
                !node["modId"].is_number_integer() || node["modId"] <= 0 ||
                node["modId"] > std::numeric_limits<qint64>::max()) {
                return fail("Invalid mod ID in Nexus page.");
            }
            node["mod_id"] = node["modId"];
            node["picture_url"] = node.value("pictureUrl", Json());
            node["endorsement_count"] = node.value("endorsements", Json());
            summaries.push_back(std::move(node));
        }
        auto mods = parseModListResponse(QByteArray::fromStdString(summaries.dump()), error);
        if (!mods) return std::nullopt;
        return NexusModPage{*mods, total.get<qint64>(), offset};
    } catch (const Json::exception&) {
        return fail("Invalid Nexus mod page fields.");
    }
}
