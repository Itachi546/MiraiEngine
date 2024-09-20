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

        T *access(uint32_t index)
        {
            ASSERT(index < pool_size);
            return &resources[index];
        }

        void release(uint32_t index)
        {
            ASSERT(index < pool_size);
            free_list[--free_list_head] = index;
            --used_indices;
        }

        void release_zero_initialize(uint32_t index)
        {
            release(index);
            std::memset(&resources[index], 0, sizeof(T));
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
        }

        std::string name;
        uint32_t pool_size;

        uint32_t free_list_head;
        uint32_t used_indices;
        uint32_t *free_list;
        T *resources;
    };
} // namespace mirai