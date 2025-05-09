#ifndef SJTU_VECTOR_HPP
#define SJTU_VECTOR_HPP

#define _GNU_SOURCE // For mremap
#include <sys/mman.h> // For mmap, munmap, mremap, MAP_FAILED, PROT_READ, PROT_WRITE, MAP_PRIVATE, MAP_ANONYMOUS
#include <unistd.h>   // For sysconf, _SC_PAGESIZE
#include <cstddef>    // For size_t
#include <cstring>    // For memmove (memcpy is not directly used by us for full copies anymore)
#include <cstdio>     // For perror (though we prefer exceptions)
#include <cstdlib>    // For exit (though we prefer exceptions)
#include <new>        // For std::bad_alloc, placement new

#include "exceptions.hpp" // Should define sjtu::std_bad_alloc, index_out_of_bound, etc.

#include <climits>
#include <iostream> // For debugging, can be removed

namespace sjtu {

  class std_bad_alloc : public exception {
    /* __________________________ */
  };
  /**
   * a data container like std::vector
   * store data in a successive memory and support random access.
   */
  template<typename T>
  class vector {
  private:
    T *data;
    // int capacity; // Replaced by capacity_bytes
    size_t capacity_bytes; // Stores capacity in bytes
    size_t _size;          // Changed from int to size_t

    static constexpr int SIZE = sizeof(T); // Kept as int, though size_t would be more idiomatic for sizeof
    static constexpr int DEFAULT_SIZE_ELEMENTS = 16; // Renamed for clarity, original DEFAULT_SIZE
    static constexpr float EXPAND_RATE = 2.0f; // Made float literal explicit
    // static constexpr float SHRINK_RATE = 0.25f; // Kept commented

    void free_resource() {
      if (data) {
        for (size_t i = 0; i < _size; ++i) {
          data[i].~T();
        }
        if (capacity_bytes > 0) {
          if (munmap(data, capacity_bytes) == -1) {
            perror("munmap failed in free_resource"); // Or throw a runtime_error
          }
        }
        data = nullptr;
        _size = 0;
        capacity_bytes = 0;
      }
    }

    void check_expand() {
        size_t current_element_capacity = (capacity_bytes > 0 && SIZE > 0) ? (capacity_bytes / SIZE) : 0;

        // _size is already incremented before check_expand is called in push_back/insert.
        // So, we expand if the new _size exceeds the old element capacity.
        if (_size > current_element_capacity) {
            size_t old_capacity_bytes = capacity_bytes;
            size_t new_element_capacity;

            if (current_element_capacity == 0) { // Initial allocation
                new_element_capacity = DEFAULT_SIZE_ELEMENTS;
                if (new_element_capacity == 0) new_element_capacity = 1; // Ensure at least 1 element
            } else { // Expanding existing allocation
                new_element_capacity = static_cast<size_t>(EXPAND_RATE * current_element_capacity);
                if (new_element_capacity < _size) { // Must be able to hold current number of elements
                    new_element_capacity = _size;
                }
                // Ensure actual growth if _size didn't force it significantly or EXPAND_RATE is <= 1
                if (new_element_capacity <= current_element_capacity && _size > current_element_capacity) {
                     new_element_capacity = current_element_capacity + 1; // Minimal growth to accommodate _size
                } else if (new_element_capacity <= current_element_capacity) {
                     new_element_capacity = current_element_capacity +1; // Ensure growth if _size didn't force it
                }

            }

            size_t new_capacity_bytes = new_element_capacity * SIZE;
            if (new_capacity_bytes == 0 && new_element_capacity > 0 && SIZE == 0) {
                // Handle T being an empty struct (sizeof(T) can be 1, but if it were 0 by some compiler extension)
                // mmap with size 0 is invalid. Let's ensure a minimal byte allocation if elements > 0.
                // Standard C++: sizeof empty class is at least 1. So SIZE > 0.
                // This case is mostly theoretical for standard C++.
            }
             if (new_capacity_bytes == 0 && new_element_capacity > 0) { // e.g. T is empty struct, SIZE might be 1
                // If we intend to store elements, we need some bytes.
                // Page size is a common minimum for mmap, but mmap can do smaller.
                // For simplicity, if new_element_capacity > 0 but new_capacity_bytes is 0 (e.g. SIZE=0),
                // this is an issue. However, sizeof(T) >= 1.
                // So, if new_element_capacity > 0, then new_capacity_bytes will be > 0.
             }


            void* new_data_ptr = nullptr;

            if (old_capacity_bytes == 0) { // Initial mmap
                if (new_capacity_bytes == 0) { // Cannot mmap 0 bytes if we intend to store elements
                    if (new_element_capacity > 0) { // We want space but calculated 0 bytes
                         // This should ideally not happen if new_element_capacity >= 1 and SIZE >=1
                         // If it does, force a minimal byte allocation, e.g., page size or SIZE.
                         long page_size = sysconf(_SC_PAGESIZE);
                         if (page_size <=0) page_size = 4096; // Fallback page size
                         new_capacity_bytes = (SIZE > 0) ? (new_element_capacity * SIZE) : (size_t)page_size;
                         if (new_capacity_bytes == 0) new_capacity_bytes = (size_t)page_size; // Absolute fallback
                    } else {
                        // No elements requested, no bytes needed. data remains nullptr.
                        // This path shouldn't be hit if _size > current_element_capacity triggered expansion.
                        return;
                    }
                }
                new_data_ptr = mmap(nullptr, new_capacity_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (new_data_ptr == MAP_FAILED) {
                    // perror("mmap failed in check_expand (initial allocation)");
                    throw sjtu::std_bad_alloc(); // Or your project's equivalent
                }
            } else { // mremap
                if (new_capacity_bytes == old_capacity_bytes) return; // No change needed
                if (new_capacity_bytes == 0) { // Shrinking to zero
                    munmap(data, old_capacity_bytes); // Unmap the old region
                    data = nullptr; // data is now null
                    capacity_bytes = 0; // capacity is zero
                    // _size should also be 0 if capacity is 0, ensure this invariant elsewhere or here.
                    return; // Allocation is now zero
                }

                new_data_ptr = mremap(data, old_capacity_bytes, new_capacity_bytes, MREMAP_MAYMOVE);
                if (new_data_ptr == MAP_FAILED) {
                    // perror("mremap failed in check_expand");
                    // Old mapping (data, old_capacity_bytes) is still valid.
                    throw sjtu::std_bad_alloc(); // Or your project's equivalent
                }
            }
            data = static_cast<T*>(new_data_ptr);
            capacity_bytes = new_capacity_bytes;
        }
    }

    /*
    void check_shrink() { // If you implement this with mremap:
      size_t current_element_capacity = (capacity_bytes > 0 && SIZE > 0) ? (capacity_bytes / SIZE) : 0;
      if (_size <= static_cast<size_t>(current_element_capacity * SHRINK_RATE) && current_element_capacity > DEFAULT_SIZE_ELEMENTS) {
        size_t new_element_capacity = static_cast<size_t>(current_element_capacity * SHRINK_RATE);
        if (new_element_capacity < _size) new_element_capacity = _size; // Don't shrink below current size
        if (new_element_capacity < DEFAULT_SIZE_ELEMENTS) new_element_capacity = DEFAULT_SIZE_ELEMENTS; // Don't shrink below default
        if (new_element_capacity == 0 && _size > 0) new_element_capacity = _size; // Safety for _size > 0

        size_t new_capacity_bytes = new_element_capacity * SIZE;
        if (new_capacity_bytes < capacity_bytes) { // Only if actually shrinking
            if (new_capacity_bytes == 0) { // Shrinking to zero elements
                if (_size == 0) { // Only if vector is empty
                    munmap(data, capacity_bytes);
                    data = nullptr;
                    capacity_bytes = 0;
                } // else, cannot shrink to 0 bytes if elements exist
            } else {
                void* new_data_ptr = mremap(data, capacity_bytes, new_capacity_bytes, MREMAP_MAYMOVE);
                if (new_data_ptr == MAP_FAILED) {
                    // perror("mremap shrink failed"); // Non-fatal, just couldn't shrink
                } else {
                    data = static_cast<T*>(new_data_ptr);
                    capacity_bytes = new_capacity_bytes;
                }
            }
        }
      }
    }
    */

  public:
    class const_iterator;
    class iterator {
    public:
      using difference_type = std::ptrdiff_t;
      using value_type = T;
      using pointer = T *;
      using reference = T &;
      using iterator_category = std::random_access_iterator_tag; // Corrected category

    private:
      int pos = -1; // Stays int, potential mismatch if size_t _size > INT_MAX
      vector *parent_ptr; // Changed to pointer to allow default construction / uninitialized state

      iterator(int p, vector *parent) : pos(p), parent_ptr(parent) {} // Constructor takes pointer

      friend class vector;
      friend class const_iterator;

    public:
      iterator() : pos(-1), parent_ptr(nullptr) {} // Default constructor

      iterator operator+(const int &n) const {
        if (!parent_ptr) throw invalid_iterator();
        iterator new_it(pos + n, parent_ptr);
        // Boundary checks can be added here or rely on vector's accessors
        return new_it;
      }
      iterator operator-(const int &n) const {
        if (!parent_ptr) throw invalid_iterator();
        iterator new_it(pos - n, parent_ptr);
        return new_it;
      }
      int operator-(const iterator &rhs) const {
        if (parent_ptr != rhs.parent_ptr) throw invalid_iterator();
        return pos - rhs.pos;
      }
      iterator &operator+=(const int &n) {
        if (!parent_ptr) throw invalid_iterator();
        pos += n;
        // It's common for iterators to go one past the end, but not further.
        // Dereferencing end() is UB.
        // if (pos > parent_ptr->_size) pos = parent_ptr->_size; // Clamping to end()
        // if (pos < 0) pos = 0; // Clamping to begin()
        return *this;
      }
      iterator &operator-=(const int &n) {
        if (!parent_ptr) throw invalid_iterator();
        pos -= n;
        return *this;
      }
      iterator operator++(int) { iterator temp = *this; ++(*this); return temp; }
      iterator &operator++() { if (!parent_ptr) throw invalid_iterator(); ++pos; return *this; }
      iterator operator--(int) { iterator temp = *this; --(*this); return temp; }
      iterator &operator--() { if (!parent_ptr) throw invalid_iterator(); --pos; return *this; }
      T &operator*() const {
        if (!parent_ptr) throw invalid_iterator();
        if (pos < 0 || static_cast<size_t>(pos) >= parent_ptr->_size) throw index_out_of_bound(); // Or invalid_iterator
        return parent_ptr->data[pos];
      }
       pointer operator->() const {
        if (!parent_ptr) throw invalid_iterator();
        if (pos < 0 || static_cast<size_t>(pos) >= parent_ptr->_size) throw index_out_of_bound();
        return &(parent_ptr->data[pos]);
      }
      bool operator==(const iterator &rhs) const { return parent_ptr == rhs.parent_ptr && pos == rhs.pos; }
      bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
      // Comparison with const_iterator
      bool operator==(const const_iterator &rhs) const {
        return parent_ptr == rhs.parent_ptr && pos == rhs.pos;
      };
      bool operator!=(const const_iterator &rhs) const {
        return !(*this == rhs);
      };
    };

    class const_iterator {
    public:
      using difference_type = std::ptrdiff_t;
      using value_type = T;
      using pointer = const T *; // const T*
      using reference = const T &; // const T&
      using iterator_category = std::random_access_iterator_tag; // Corrected category

    private:
      int pos = -1;
      const vector *parent_ptr; // Changed to pointer

      const_iterator(int p, const vector *parent) : pos(p), parent_ptr(parent) {}

      friend class vector;
      friend class iterator;

    public:
      const_iterator() : pos(-1), parent_ptr(nullptr) {} // Default constructor

      const_iterator operator+(const int &n) const {
        if (!parent_ptr) throw invalid_iterator();
        const_iterator new_it(pos + n, parent_ptr);
        return new_it;
      }
      const_iterator operator-(const int &n) const {
        if (!parent_ptr) throw invalid_iterator();
        const_iterator new_it(pos - n, parent_ptr);
        return new_it;
      }
      int operator-(const const_iterator &rhs) const {
        if (parent_ptr != rhs.parent_ptr) throw invalid_iterator();
        return pos - rhs.pos;
      }
      const_iterator &operator+=(const int &n) { if (!parent_ptr) throw invalid_iterator(); pos += n; return *this; }
      const_iterator &operator-=(const int &n) { if (!parent_ptr) throw invalid_iterator(); pos -= n; return *this; }
      const_iterator operator++(int) { const_iterator temp = *this; ++(*this); return temp; }
      const_iterator &operator++() { if (!parent_ptr) throw invalid_iterator(); ++pos; return *this; }
      const_iterator operator--(int) { const_iterator temp = *this; --(*this); return temp; }
      const_iterator &operator--() { if (!parent_ptr) throw invalid_iterator(); --pos; return *this; }
      const T &operator*() const {
        if (!parent_ptr) throw invalid_iterator();
        if (pos < 0 || static_cast<size_t>(pos) >= parent_ptr->_size) throw index_out_of_bound();
        return parent_ptr->data[pos];
      }
      pointer operator->() const {
        if (!parent_ptr) throw invalid_iterator();
        if (pos < 0 || static_cast<size_t>(pos) >= parent_ptr->_size) throw index_out_of_bound();
        return &(parent_ptr->data[pos]);
      }
      bool operator==(const const_iterator &rhs) const { return parent_ptr == rhs.parent_ptr && pos == rhs.pos; }
      bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
      // Comparison with iterator
      bool operator==(const iterator &rhs) const {
        return parent_ptr == rhs.parent_ptr && pos == rhs.pos;
      };
      bool operator!=(const iterator &rhs) const{
        return !(*this == rhs);
      };
    };




    vector() : data(nullptr), capacity_bytes(0), _size(0) {}

    vector(const vector &other) : data(nullptr), capacity_bytes(0), _size(0) {
      if (other.capacity_bytes > 0) {
        void* new_mem = mmap(nullptr, other.capacity_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (new_mem == MAP_FAILED) {
          throw sjtu::std_bad_alloc();
        }
        data = static_cast<T*>(new_mem);
        capacity_bytes = other.capacity_bytes;
        _size = other._size;
        try {
          for (size_t i = 0; i < _size; ++i) {
            new (data + i) T(other.data[i]);
          }
        } catch (...) {
          munmap(data, capacity_bytes); // Clean up allocated memory on exception during copy
          data = nullptr;
          capacity_bytes = 0;
          _size = 0;
          throw; // Re-throw the exception
        }
      } else {
        // other is empty and unallocated, so this vector is also empty and unallocated
        _size = other._size; // Should be 0
        // data, capacity_bytes already 0
      }
    }

    ~vector() {
      free_resource();
    }

    vector &operator=(const vector &other) {
      if (this == &other) {
        return *this;
      }

      // Temporary vector for strong exception guarantee (copy-and-swap idiom)
      // Or, allocate new, copy, then free old.
      // For mmap, direct re-allocation might be simpler if we ensure cleanup on failure.

      T* new_data_temp = nullptr;
      size_t new_capacity_bytes_temp = 0;

      if (other.capacity_bytes > 0) {
        void* new_mem = mmap(nullptr, other.capacity_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (new_mem == MAP_FAILED) {
          throw sjtu::std_bad_alloc();
        }
        new_data_temp = static_cast<T*>(new_mem);
        new_capacity_bytes_temp = other.capacity_bytes;

        size_t copied_count = 0;
        try {
          for (size_t i = 0; i < other._size; ++i) {
            new (new_data_temp + i) T(other.data[i]);
            copied_count++;
          }
        } catch (...) {
          // Destruct successfully copied elements
          for (size_t i = 0; i < copied_count; ++i) {
            (new_data_temp + i)->~T();
          }
          munmap(new_data_temp, new_capacity_bytes_temp);
          throw; // Re-throw
        }
      }

      // Successfully allocated and copied to temporary. Now destroy old state and assign new.
      free_resource(); // Destroy and unmap current data

      data = new_data_temp;
      capacity_bytes = new_capacity_bytes_temp;
      _size = other._size;

      return *this;
    }

    T &at(const size_t &pos) {
      if (pos >= _size) throw index_out_of_bound();
      return data[pos];
    }
    const T &at(const size_t &pos) const {
      if (pos >= _size) throw index_out_of_bound();
      return data[pos];
    }
    T &operator[](const size_t &pos) {
      if (pos >= _size) throw index_out_of_bound(); // As per your requirement
      return data[pos];
    }
    const T &operator[](const size_t &pos) const {
      if (pos >= _size) throw index_out_of_bound(); // As per your requirement
      return data[pos];
    }

    const T &front() const { if (empty()) throw container_is_empty(); return data[0]; }
    const T &back() const { if (empty()) throw container_is_empty(); return data[_size - 1]; }

    iterator begin() { return iterator(0, this); }
    const_iterator begin() const { return const_iterator(0, this); }
    const_iterator cbegin() const { return const_iterator(0, this); }
    iterator end() { return iterator(static_cast<int>(_size), this); } // Cast _size to int for iterator
    const_iterator end() const { return const_iterator(static_cast<int>(_size), this); }
    const_iterator cend() const { return const_iterator(static_cast<int>(_size), this); }

    bool empty() const { return _size == 0; }
    size_t size() const { return _size; }
    // Add capacity() method for users, returning element count
    size_t capacity() const { return (capacity_bytes > 0 && SIZE > 0) ? (capacity_bytes / SIZE) : 0; }


    void clear() {
      for (size_t i = 0; i < _size; ++i) {
        data[i].~T();
      }
      _size = 0;
      // Note: clear usually does not deallocate memory. If you want to deallocate,
      // you might add a shrink_to_fit() method or modify clear().
    }

    iterator insert(iterator it_pos, const T &value) {
      if (it_pos.parent_ptr != this) throw invalid_iterator();
      return insert(static_cast<size_t>(it_pos.pos), value);
    }

    iterator insert(const size_t &ind, const T &value) {
      if (ind > _size) throw index_out_of_bound();

      // If _size will reach capacity, check_expand needs to be called *before* memmove
      // Original logic: _size += 1; check_expand();
      // This means check_expand sees the *new* size.
      // Let's consider the case where data is nullptr (empty vector)

      if (_size == ((capacity_bytes > 0 && SIZE > 0) ? (capacity_bytes / SIZE) : 0) ) { // If we are at full capacity
          // Temporarily increment _size to correctly calculate new capacity in check_expand
          _size++;
          check_expand();
          _size--; // Revert _size, as it will be incremented properly later
      } else if (data == nullptr && _size == 0) { // Special case: inserting into totally empty vector
          _size++; // Target size is 1
          check_expand(); // This will perform initial mmap
          _size--; // Revert
      }


      // Shift elements
      if (ind < _size) { // Only move if not inserting at the very end
        // Use T* for pointer arithmetic with memmove if SIZE is used for byte count
        memmove(data + ind + 1, data + ind, (_size - ind) * SIZE);
      }

      new (data + ind) T(value);
      _size++;
      return iterator(static_cast<int>(ind), this);
    }

    iterator erase(iterator it_pos) {
      if (it_pos.parent_ptr != this) throw invalid_iterator();
      return erase(static_cast<size_t>(it_pos.pos));
    }

    iterator erase(const size_t &ind) {
      if (ind >= _size) throw index_out_of_bound();

      data[ind].~T();
      if (ind < _size - 1) { // Only move if not erasing the last element
        memmove(data + ind, data + ind + 1, (_size - 1 - ind) * SIZE);
      }
      _size--;
      // check_shrink(); // Optional
      return iterator(static_cast<int>(ind), this);
    }

    void push_back(const T &value) {
      // If _size will reach capacity, check_expand needs to be called
      if (_size == ((capacity_bytes > 0 && SIZE > 0) ? (capacity_bytes / SIZE) : 0) ) {
          // Temporarily increment _size for check_expand's logic
          _size++;
          check_expand();
          _size--; // Revert, actual increment is next
      } else if (data == nullptr && _size == 0) { // Special case: totally empty vector
          _size++;
          check_expand();
          _size--;
      }

      new (data + _size) T(value);
      _size++;
    }

    void pop_back() {
      if (empty()) throw container_is_empty();
      _size--;
      data[_size].~T();
      // check_shrink(); // Optional
    }
  };

  // ... (rest of the vector class definition above) ...

}; // End of class vector<T>

// --- Implementations for iterator and const_iterator cross-comparison ---

#endif // SJTU_VECTOR_HPP