#pragma once

#include <QString>

struct SmapiVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    QString prerelease;

    [[nodiscard]] bool isValid() const { return major >= 0 && minor >= 0 && patch >= 0; }

    [[nodiscard]] QString toString() const {
        const auto base = QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
        return prerelease.isEmpty() ? base : base + "-" + prerelease;
    }

    friend bool operator==(const SmapiVersion&, const SmapiVersion&) = default;
};
