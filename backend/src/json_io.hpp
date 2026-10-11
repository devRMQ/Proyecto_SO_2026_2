#pragma once

#include "memory_sim.hpp"
#include "scheduler.hpp"
#include "sync_sim.hpp"

#include <nlohmann/json.hpp>
#include <string>

ScheduleConfig parse_schedule_section(const nlohmann::json& j);
SyncConfig parse_sync_section(const nlohmann::json& j);
void parse_memory_section(const nlohmann::json& j, ContiguousConfig& contig, PagingConfig& paging);

nlohmann::json run_from_input(const nlohmann::json& root, const std::string& module_override);
bool load_json_file(const std::string& path, nlohmann::json& out, std::string& err);
bool write_json_file(const std::string& path, const nlohmann::json& data, std::string& err);
