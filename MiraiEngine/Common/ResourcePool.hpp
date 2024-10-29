#pragma once

#include "Common/CommonInclude.hpp"
#include "Engine/Log.hpp"
#include <vector>
#include <numeric>

namespace mirai
{
    template <typename T>
    struct ResourcePool
    {

        ResourcePool(uint32_t pool_size, std::string name) : pool_size(pool_size),
                                                             name(name),
                                                             used_indices(0),
                                                             free_list_head(0)
        {
            free_list = new uint32_t[pool_size];
            resources = new T[pool_size];
            for (uint32_t i = 0; i < pool_size; ++i)
                free_list[i] = i;
        }

        ResourcePool(const ResourcePool &) = delete;
        ResourcePool operator=(const ResourcePool &) = delete;

        uint32_t obtain()
        {
            ASSERT(used_indices < pool_size);
            uint32_t index = free_list[free_list_head++];
            ++used_indices;
            return index;
        }

        T *access(ID id)
        {
            ASSERT(id.id < pool_size);
            return &resources[id.id];
        }

        T *access(uint32_t id)
        {
            ASSERT(id < pool_size);
            return &resources[id];
        }

        void release(ID id)
        {
            ASSERT(id.id < pool_size);
            free_list[--free_list_head] = id.id;
            --used_indices;
        }

        void release_all()
        {
            for (uint32_t i = 0; i < pool_size; ++i)
                free_list[i] = i;
            used_indices = 0;
            free_list_head = 0;
        }

        ~ResourcePool()
        {
            if (used_indices != 0)
                Log::Warn("ResourcePool[", name, "] has unfreed resources");

            delete[] resources;
            delete[] free_list;

            resources = nullptr;
            free_list = nullptr;
        }

        std::string name;
        uint32_t pool_size;

        uint32_t free_list_head;
        uint32_t used_indices;
        uint32_t *free_list;
        T *resources;
    };
} // namespace mirai