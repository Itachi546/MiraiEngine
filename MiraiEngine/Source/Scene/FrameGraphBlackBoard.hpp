#pragma once

#include "Common/HashMap.hpp"
#include <any>
#include <typeindex>

namespace mirai {
    class FrameGraphBlackBoard {
      public:
        FrameGraphBlackBoard() = default;
        FrameGraphBlackBoard(const FrameGraphBlackBoard &) = delete;
        FrameGraphBlackBoard(FrameGraphBlackBoard &&) = delete;
        FrameGraphBlackBoard &operator=(const FrameGraphBlackBoard &) = delete;
        FrameGraphBlackBoard &operator=(FrameGraphBlackBoard &&) = delete;

        template <typename T, typename... Args>
        T &add(Args &&...args);

        template <typename T>
        T &get();

        template <typename T>
        const T &get() const;

        template <typename T>
        bool has();

        ~FrameGraphBlackBoard() {
            storage.clear();
        }

      private:
        HashMap<std::type_index, std::any> storage;
    };

    template <typename T, typename... Args>
    inline T &FrameGraphBlackBoard::add(Args &&...args) {
        assert(!has<T>());

        return storage[typeid(T)].emplace<T>(T{std::forward<Args>(args)...});
    }

    template <typename T>
    inline T &FrameGraphBlackBoard::get() {
        return *std::any_cast<T>(&storage.at(typeid(T)));
    }

    template <typename T>
    inline const T &FrameGraphBlackBoard::get() const {
        return *static_cast<T>(&storage.at(typeid(T)));
    }

    template <typename T>
    inline bool FrameGraphBlackBoard::has() {
        return storage.find(typeid(T)) != storage.end();
    }
} // namespace mirai