#pragma once

#include "registry/types.h"

#include "esp_err.h"

#include <string>
#include <vector>

namespace ipbuilding::config {

class ConfigStore {
public:
    esp_err_t init();
    const std::vector<registry::ModuleConfig> &modules() const { return modules_; }

private:
    esp_err_t parse_json(const char *json_text);
    registry::SemanticType parse_semantic_type(const char *value) const;

    std::vector<registry::ModuleConfig> modules_;
    bool mounted_ = false;
};

}  // namespace ipbuilding::config
