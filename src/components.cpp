#include "components.hpp"

void comp::Hierarchy::removeParent(ecs::EntityHandle& entity)
{
    auto& entityHierarchy = entity.get<Hierarchy>();
    if (entityHierarchy.parent) {
        auto& parentHierarchy = entityHierarchy.parent.get<Hierarchy>();
        assert(parentHierarchy.firstChild);
        if (parentHierarchy.firstChild == entity) {
            assert(!entityHierarchy.prevSibling);
            parentHierarchy.firstChild = entityHierarchy.nextSibling;
        } else {
            assert(entityHierarchy.prevSibling);
            entityHierarchy.prevSibling.get<Hierarchy>().nextSibling = entityHierarchy.nextSibling;
        }
        entityHierarchy.nextSibling.get<Hierarchy>().prevSibling = entityHierarchy.prevSibling;
    }
    entityHierarchy.parent = ecs::EntityHandle();
    entityHierarchy.prevSibling = ecs::EntityHandle();
    entityHierarchy.nextSibling = ecs::EntityHandle();
}

void comp::Hierarchy::setParent(ecs::EntityHandle& entity, ecs::EntityHandle& parent)
{
    removeParent(entity);
    auto& entityHierarchy = entity.getOrAdd<Hierarchy>();
    entityHierarchy.parent = parent;
    auto& parentHierarchy = parent.getOrAdd<Hierarchy>();
    if (!parentHierarchy.firstChild) {
        parentHierarchy.firstChild = entity;
        entityHierarchy.prevSibling = ecs::EntityHandle();
    } else {
        auto last = parentHierarchy.firstChild;
        while (true) {
            const auto& h = last.get<Hierarchy>();
            if (h.nextSibling) {
                last = h.nextSibling;
            } else {
                break;
            }
        }
        last.get<Hierarchy>().nextSibling = entity;
        entityHierarchy.prevSibling = last;
    }
    entityHierarchy.nextSibling = ecs::EntityHandle();
}
