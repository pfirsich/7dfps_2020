#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <limits>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

namespace ecs {

using ComponentMask = uint64_t;
static_assert(std::is_unsigned<ComponentMask>::value, "ComponentMask type must be unsigned");
static const ComponentMask AllComponents = std::numeric_limits<ComponentMask>::max();
static const size_t MaxComponents = std::numeric_limits<ComponentMask>::digits;

using EntityId = uint32_t;
static const EntityId InvalidEntity = std::numeric_limits<EntityId>::max();

using IndexType = uint32_t;
static const IndexType MaxIndex = std::numeric_limits<IndexType>::max();

// I did it in a dumb way before and now I borrowed from EnTT. Thanks, skypjack!
namespace componentId {
    size_t getNextId();

    template <typename ComponentType>
    size_t get()
    {
        static auto id = getNextId();
        return id;
    }
}

template <typename... Args>
constexpr ComponentMask componentMask()
{
    constexpr auto one = static_cast<ComponentMask>(1);
    return (... | (one << componentId::get<typename std::remove_const<Args>::type>()));
}

struct ComponentPoolBase {
    virtual ~ComponentPoolBase() = default;
    virtual void remove(EntityId entityId) = 0;
};

template <typename ComponentType>
class ComponentPool : public ComponentPoolBase {
public:
    ComponentPool() = default;
    ~ComponentPool();
    ComponentPool(const ComponentPool& other) = delete;
    ComponentPool& operator=(const ComponentPool& other) = delete;

    template <typename... Args>
    ComponentType& add(EntityId entityId, Args... args);

    bool has(EntityId entityId) const;

    ComponentType& get(EntityId entityId);

    void remove(EntityId entityId) override;

    static constexpr size_t DefaultBlockSize = 64;

private:
    // https://gist.github.com/pfirsich/72ec22c4407013eccfab3a78f2ac7a23
    template <class T>
    static constexpr size_t getBlockSizeImpl(const T* /*t*/, ...)
    {
        return DefaultBlockSize;
    }

    template <class T>
    static constexpr typename std::enable_if_t<!std::is_void_v<decltype(T::BlockSize)>, size_t>
    getBlockSizeImpl(const T* /*t*/, int)
    {
        return T::BlockSize;
    }

    static const size_t BlockSize = getBlockSizeImpl(static_cast<ComponentType*>(nullptr), 0);
    static_assert(BlockSize > 0);
    static const size_t COMPONENT_SIZE = sizeof(ComponentType);

    static constexpr std::pair<size_t, size_t> getIndices(EntityId entityId)
    {
        return std::pair<size_t, size_t>(entityId / BlockSize, entityId % BlockSize);
    }

    ComponentType* getPointer(size_t blockIndex, size_t componentIndex)
    {
        assert(blocks_[blockIndex].data);
        return reinterpret_cast<ComponentType*>(blocks_[blockIndex].data) + componentIndex;
    }

    void checkBlockUsage(size_t blockIndex);

    struct Block {
        void* data = nullptr;
        std::bitset<BlockSize> occupied;
    };

    std::vector<Block> blocks_;
};

template <typename ComponentType>
ComponentPool<ComponentType>::~ComponentPool()
{
    for (auto& block : blocks_) {
        operator delete(block.data);
        block.data = nullptr;
    }
}

template <typename ComponentType>
template <typename... Args>
ComponentType& ComponentPool<ComponentType>::add(EntityId entityId, Args... args)
{
    assert(!has(entityId));
    const auto [blockIndex, componentIndex] = getIndices(entityId);

    if (blocks_.size() < blockIndex + 1)
        blocks_.resize(blockIndex + 1);
    auto& block = blocks_[blockIndex];
    if (!block.data)
        block.data = operator new(BlockSize* COMPONENT_SIZE);
    block.occupied[componentIndex] = true;
    auto component
        = new (getPointer(blockIndex, componentIndex)) ComponentType(std::forward<Args>(args)...);

    return *component;
}

template <typename ComponentType>
bool ComponentPool<ComponentType>::has(EntityId entityId) const
{
    const auto [blockIndex, componentIndex] = getIndices(entityId);
    return blocks_.size() > blockIndex && blocks_[blockIndex].occupied[componentIndex];
}

template <typename ComponentType>
ComponentType& ComponentPool<ComponentType>::get(EntityId entityId)
{
    assert(has(entityId));
    const auto [blockIndex, componentIndex] = getIndices(entityId);
    return *getPointer(blockIndex, componentIndex);
}

template <typename ComponentType>
void ComponentPool<ComponentType>::remove(EntityId entityId)
{
    assert(has(entityId));
    const auto [blockIndex, componentIndex] = getIndices(entityId);
    auto component = getPointer(blockIndex, componentIndex);
    component->~ComponentType();
    blocks_[blockIndex].occupied[componentIndex] = false;
    checkBlockUsage(blockIndex);
}

template <typename ComponentType>
void ComponentPool<ComponentType>::checkBlockUsage(size_t blockIndex)
{
    auto& block = blocks_[blockIndex];
    if (block.occupied.none()) { // block is unused
        operator delete(block.data);
        block.data = nullptr;
    }
}

class EntityHandle;

class World {
public:
    struct EntityList;

    class EntityIterator {
        // To be used with std::for_each, this has to be a ForwardIterator:
        // https://en.cppreference.com/w/cpp/named_req/ForwardIterator
        // http://anderberg.me/2016/07/04/c-custom-iterators/
    public:
        // https://en.cppreference.com/w/cpp/iterator/iterator_traits
        using iterator_category = std::forward_iterator_tag;
        using value_type = EntityHandle;
        using pointer = EntityHandle*;
        using reference = EntityHandle&;
        using difference_type = std::ptrdiff_t;

        EntityIterator() = default; // singular iterator
        EntityIterator(const EntityIterator& other) = default;
        EntityIterator& operator=(const EntityIterator& other) = default;

        EntityIterator(EntityList* list, IndexType index)
            : list_(list)
            , entityIndex_(index)
        {
        }

        EntityIterator& operator++();
        EntityIterator operator++(int);
        bool operator==(const EntityIterator& other) const;
        bool operator!=(const EntityIterator& other) const;
        EntityHandle operator*() const;

        EntityList* getList() const
        {
            return list_;
        }

        IndexType getIndex() const
        {
            return entityIndex_;
        }

    private:
        EntityList* list_ = nullptr;
        IndexType entityIndex_ = MaxIndex;
    };

    struct EntityList {
        EntityList(World& world, ComponentMask mask)
            : world(world)
            , mask(mask)
        {
        }

        ~EntityList() = default;

        EntityIterator begin()
        {
            // start at -1 and increment to get an invalid iterator if no entity matches
            // is this hackish?
            return ++EntityIterator(this, MaxIndex);
        }

        EntityIterator end()
        {
            return EntityIterator(this, MaxIndex);
        }

        World& world;
        ComponentMask mask;
    };

public:
    World() = default;
    ~World() = default;
    World(const World& other) = delete;
    World& operator=(const World& other) = delete;

    EntityHandle createEntity();
    EntityHandle getEntityHandle(EntityId entityId);

    void destroyEntity(EntityId entityId);

    template <typename ComponentType, typename... Args>
    ComponentType& addComponent(EntityId entityId, Args&&... args);

    bool hasComponents(EntityId entityId, ComponentMask mask) const;

    template <typename... Args>
    bool hasComponents(EntityId entityId) const;

    ComponentMask getComponentMask(EntityId entityId) const;

    template <typename ComponentType>
    ComponentType& getComponent(EntityId entityId);

    template <typename ComponentType>
    void removeComponent(EntityId entityId);

    bool isValid(EntityId entityId) const
    {
        assert(entityId < entityValid_.size());
        return entityValid_[entityId];
    }

    void flush(EntityId entityId);
    void flush(); // flush all

    size_t getEntityCount() const
    {
        return componentMasks_.size();
    }

    template <typename... Components, typename FuncType>
    void forEachEntity(FuncType func);

    template <typename... Components>
    EntityList entitiesWith()
    {
        return EntityList(*this, componentMask<Components...>());
    }

private:
    std::vector<ComponentMask> componentMasks_;
    std::vector<bool> entityValid_;
    // the free list is a min heap, so that we try to fill lower indices first
    std::priority_queue<EntityId, std::vector<EntityId>, std::greater<>> entityIdFreeList_;
    std::array<std::unique_ptr<ComponentPoolBase>, MaxComponents> pools_;

    template <typename ComponentType>
    ComponentPool<ComponentType>& getPool(bool alloc = true);
};

class EntityHandle {
public:
    EntityHandle() = default;
    ~EntityHandle() = default;
    EntityHandle(const EntityHandle& other) = default;
    EntityHandle& operator=(const EntityHandle& other) = default;

    void destroy();

    template <typename... Args>
    bool has() const;

    template <typename ComponentType, typename... Args>
    ComponentType& add(Args&&... args);

    template <typename ComponentType>
    ComponentType& get();

    template <typename ComponentType>
    ComponentType& getOrAdd();

    template <typename ComponentType>
    void remove();

    bool isValid() const;

    operator bool() const;

    bool operator==(const EntityHandle& other);

    bool operator!=(const EntityHandle& other);

    EntityId getId() const;

    World* getWorld() const;

private:
    World* world_ = nullptr;
    EntityId id_ = InvalidEntity;

    EntityHandle(World& world, EntityId id);

    friend EntityHandle World::createEntity();
    friend EntityHandle World::getEntityHandle(EntityId);
};

// Implementation

template <typename ComponentType>
ComponentPool<ComponentType>& World::getPool(bool alloc)
{
    const auto compId = componentId::get<ComponentType>();
    assert(compId < pools_.size());
    if (alloc && !pools_[compId]) {
        pools_[compId] = std::make_unique<ComponentPool<ComponentType>>();
    }
    assert(pools_[compId]);
    return *static_cast<ComponentPool<ComponentType>*>(pools_[compId].get());
}

template <typename ComponentType, typename... Args>
ComponentType& World::addComponent(EntityId entityId, Args&&... args)
{
    assert(componentMasks_.size() > entityId);
    assert(!hasComponents<ComponentType>(entityId));
    componentMasks_[entityId] |= componentMask<ComponentType>();
    return getPool<ComponentType>().add(entityId, std::forward<Args>(args)...);
}

template <typename... Args>
bool World::hasComponents(EntityId entityId) const
{
    return hasComponents(entityId, componentMask<Args...>());
}

template <typename ComponentType>
ComponentType& World::getComponent(EntityId entityId)
{
    assert(hasComponents<ComponentType>(entityId));
    // make getPool not alloc, so we don't have to protect getComponent with a mutex (later)
    // this should never trigger an allocation anyways, since we assert hasComponent above,
    // so this is just an extra safety measure
    return getPool<typename std::remove_const<ComponentType>::type>(false).get(entityId);
}

template <typename ComponentType>
void World::removeComponent(EntityId entityId)
{
    getPool<ComponentType>().remove(entityId);
}

template <bool isConst, typename ComponentType>
ComponentMask constFilteredComponentMaskSingle()
{
    if constexpr (std::is_const<ComponentType>::value == isConst) {
        return componentMask<ComponentType>();
    } else {
        return 0;
    }
}

template <bool isConst, typename... Args>
ComponentMask constFilteredComponentMask()
{
    return (... | constFilteredComponentMaskSingle<isConst, Args>());
}

template <typename... Components, typename FuncType>
void World::forEachEntity(FuncType func)
{
    constexpr auto entityHandleOnly = std::is_invocable_r_v<void, FuncType, EntityHandle>;
    constexpr auto entityHandleAndComponents
        = std::is_invocable_r_v<void, FuncType, EntityHandle, Components&...>;
    constexpr auto componentsOnly = std::is_invocable_r_v<void, FuncType, Components&...>;
    static_assert(entityHandleOnly || entityHandleAndComponents || componentsOnly,
        "Function signature has to be either void(EntityHandle), void(Components...) or "
        "void(EntityHandle, Components...).");
    static_assert((entityHandleOnly && !(entityHandleAndComponents || componentsOnly))
        || (entityHandleAndComponents && !(entityHandleOnly || componentsOnly))
        || (componentsOnly && !(entityHandleAndComponents || entityHandleOnly)));
    // EntityHandle has to be passed by value to the invokable, because the EntityHandle returned
    // from the EntityIterator is a temporary
    auto entityList = entitiesWith<Components...>();
    if constexpr (entityHandleOnly) {
        std::for_each(entityList.begin(), entityList.end(), func);
    } else if constexpr (entityHandleAndComponents) {
        std::for_each(entityList.begin(), entityList.end(),
            [func](EntityHandle e) { func(e, e.get<Components>()...); });
    } else { // componentsOnly
        std::for_each(entityList.begin(), entityList.end(),
            [func](EntityHandle e) { func(e.get<Components>()...); });
    }
}

template <typename ComponentType, typename... Args>
ComponentType& EntityHandle::add(Args&&... args)
{
    return world_->addComponent<ComponentType>(id_, std::forward<Args>(args)...);
}

template <typename... Args>
bool EntityHandle::has() const
{
    return world_->hasComponents<Args...>(id_);
}

template <typename ComponentType>
ComponentType& EntityHandle::get()
{
    return world_->getComponent<ComponentType>(id_);
}

template <typename ComponentType>
ComponentType& EntityHandle::getOrAdd()
{
    static_assert(std::is_default_constructible<ComponentType>(),
        "Component type must be default constructible.");
    if (!world_->hasComponents<ComponentType>(id_))
        world_->addComponent<ComponentType>(id_);
    return world_->getComponent<ComponentType>(id_);
}

template <typename ComponentType>
void EntityHandle::remove()
{
    world_->removeComponent<ComponentType>(id_);
}

} // namespace ecs
