#include "ecs.hpp"

namespace ecs {

size_t componentId::getNextId()
{
    static size_t idCounter = 0;
    assert(idCounter < MaxComponents);
    return idCounter++;
}

World::EntityIterator& World::EntityIterator::operator++()
{
    entityIndex_++;
    const auto& world = list_->world;
    while (entityIndex_ < world.getEntityCount()
        && (!world.isValid(entityIndex_) || !world.hasComponents(entityIndex_, list_->mask)))
        entityIndex_++;
    if (entityIndex_ >= world.getEntityCount()) {
        entityIndex_ = MaxIndex;
    }
    return *this;
}

World::EntityIterator World::EntityIterator::operator++(int)
{
    EntityIterator ret(*this);
    operator++();
    return ret;
}

bool World::EntityIterator::operator==(const EntityIterator& other) const
{
    return list_ == other.list_ && entityIndex_ == other.entityIndex_;
}

bool World::EntityIterator::operator!=(const EntityIterator& other) const
{
    return !operator==(other);
}

EntityHandle World::EntityIterator::operator*() const
{
    return list_->world.getEntityHandle(entityIndex_);
}

EntityHandle World::createEntity()
{
    if (entityIdFreeList_.empty()) {
        componentMasks_.push_back(0);
        entityValid_.push_back(false);
        assert(componentMasks_.size() == entityValid_.size());
        return EntityHandle(*this, componentMasks_.size() - 1);
    } else {
        const auto entityId = entityIdFreeList_.top();
        entityIdFreeList_.pop();
        assert(entityId < componentMasks_.size() && entityId < entityValid_.size());
        componentMasks_[entityId] = 0;
        entityValid_[entityId] = false;
        return EntityHandle(*this, entityId);
    }
}

EntityHandle World::getEntityHandle(EntityId entityId)
{
    assert(entityId < componentMasks_.size()); // entity has existed
    return EntityHandle(*this, entityId);
}

void World::destroyEntity(EntityId entityId)
{
    assert(componentMasks_.size() >= entityId); // entity exists
    for (size_t compId = 0; compId < pools_.size(); ++compId) {
        const auto hasComponent = (componentMasks_[entityId] & (1ull << compId)) > 0;
        if (pools_[compId] && hasComponent)
            pools_[compId]->remove(entityId);
    }
    componentMasks_[entityId] = 0;
    entityIdFreeList_.push(entityId);
}

void World::flush()
{
    entityValid_.assign(entityValid_.size(), true);
}

void World::flush(EntityId entityId)
{
    assert(entityId < entityValid_.size());
    entityValid_[entityId] = true;
}

bool World::hasComponents(EntityId entityId, ComponentMask mask) const
{
    assert(componentMasks_.size() > entityId);
    return (componentMasks_[entityId] & mask) == mask;
}

ComponentMask World::getComponentMask(EntityId entityId) const
{
    assert(componentMasks_.size() > entityId);
    return componentMasks_[entityId];
}

// EntityHandle implementation

void EntityHandle::destroy()
{
    assert(isValid());
    world_->destroyEntity(id_);
    id_ = InvalidEntity;
}

bool EntityHandle::isValid() const
{
    return world_ && id_ != InvalidEntity;
}

EntityHandle::operator bool() const
{
    return isValid();
}

bool EntityHandle::operator==(const EntityHandle& other)
{
    return world_ == other.world_ && id_ == other.id_;
}

bool EntityHandle::operator!=(const EntityHandle& other)
{
    return !(*this == other);
}

EntityId EntityHandle::getId() const
{
    return id_;
}

World* EntityHandle::getWorld() const
{
    return world_;
}

EntityHandle::EntityHandle(World& world, EntityId id)
    : world_(&world)
    , id_(id)
{
}
}
