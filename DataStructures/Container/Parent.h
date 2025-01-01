#pragma once

#include <cassert>
#include <vector>

template <typename TYPE_TO_STORE = uint64_t>
struct Parent {
public:
    std::vector<std::vector<TYPE_TO_STORE>> storage;

    Parent(size_t n, size_t numElements) : storage(n, std::vector<TYPE_TO_STORE>(numElements)){};

    inline bool isValid(size_t n) const noexcept { return n < storage.size(); }

    inline bool isElementOfElement(size_t n, size_t element) const noexcept {
        return isValid(n) && element < storage[n].size();
    }

    void fill(TYPE_TO_STORE defaultValue) noexcept {
        for (size_t n = 0; n < storage.size(); ++n) {
            fill(n, defaultValue);
        }
    }

    void fill(size_t n, TYPE_TO_STORE defaultValue) noexcept { fill(n, defaultValue, 0, storage[n].size()); }

    void fill(size_t n, TYPE_TO_STORE defaultValue, size_t from, size_t to) noexcept {
        assert(isValid(n));
        assert(from <= to);
        assert(to <= storage[n].size());

        std::fill(storage[n].begin() + from, storage[n].begin() + to, defaultValue);
    }

    const TYPE_TO_STORE& getElement(size_t element, size_t numberOfElement) const noexcept {
        assert(isElementOfElement(element, numberOfElement));
        return storage[element][numberOfElement];
    }

    void setElement(size_t element, size_t numberOfElement, TYPE_TO_STORE newElement) noexcept {
        assert(isElementOfElement(element, numberOfElement));
        storage[element][numberOfElement] = newElement;
    }
};
