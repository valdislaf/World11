#include "world/WorldRegistry.hpp"

#include <algorithm>
#include <stdexcept>

namespace hg::world {

void WorldRegistry::registerWorld(WorldDefinition definition, WorldFactory factory) {
  if (!factory) {
    throw std::invalid_argument("WorldRegistry::registerWorld requires a factory");
  }
  if (definition.id <= 0) {
    throw std::invalid_argument("WorldRegistry::registerWorld requires definition.id > 0");
  }
  entries_[definition.id] = Entry{std::move(definition), std::move(factory)};
}

bool WorldRegistry::hasWorld(int world_id) const {
  return entries_.find(world_id) != entries_.end();
}

std::unique_ptr<IWorld> WorldRegistry::createWorld(int world_id) const {
  const auto it = entries_.find(world_id);
  if (it == entries_.end()) {
    throw std::out_of_range("WorldRegistry::createWorld: unknown world_id");
  }
  return it->second.factory();
}

const WorldDefinition& WorldRegistry::definition(int world_id) const {
  const auto it = entries_.find(world_id);
  if (it == entries_.end()) {
    throw std::out_of_range("WorldRegistry::definition: unknown world_id");
  }
  return it->second.definition;
}

std::vector<int> WorldRegistry::worldIds() const {
  std::vector<int> ids;
  ids.reserve(entries_.size());
  for (const auto& item : entries_) {
    ids.push_back(item.first);
  }
  return ids;
}

int WorldRegistry::worldCount() const {
  return static_cast<int>(entries_.size());
}

int WorldRegistry::maxWorldId() const {
  int max_id = 0;
  for (const auto& item : entries_) {
    max_id = std::max(max_id, item.first);
  }
  return max_id;
}

}  // namespace hg::world
