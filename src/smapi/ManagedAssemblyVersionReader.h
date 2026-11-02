#pragma once

#include <QString>
#include <optional>

#include "SmapiVersion.h"

class ManagedAssemblyVersionReader {
   public:
    [[nodiscard]] std::optional<SmapiVersion> read(const QString& filePath,
                                                   QString* errorMessage = nullptr) const;
};
