#ifndef SJTU_PRIORITY_QUEUE_HPP
#define SJTU_PRIORITY_QUEUE_HPP

#include <cstddef>
#include <functional>
#include "exceptions.hpp"

namespace sjtu {
  /**
   * @brief a container like std::priority_queue which is a heap internal.
   * **Exception Safety**: The `Compare` operation might throw exceptions for certain data.
   * In such cases, any ongoing operation should be terminated, and the priority queue should be restored to its original state before the operation began.
   */
  template<typename T, class Compare = std::less<T> >
  class priority_queue {
    struct node {
      T data;
      node *son = nullptr, *sibling = nullptr;

      node(const T &data): data(data) {
      }

      ~node() {
        delete son;
        delete sibling;
        son = nullptr;
        sibling = nullptr;
      }


      node *copy() {
        node *temp = new node(data);
        if (son) {
          temp->son = son->copy();
        }
        if (sibling) {
          temp->sibling = sibling->copy();
        }
        return temp;
      }

      /**
       *@brief merge the siblings of a node
       *@return a node* of the root node for the merged nodes
       *@throw throw sjtu::runtime_error if there is an exception when merging.
       *If an error occured when merging the merged siblings and the paired nodes, siblings already merged will not be restored
       *
       */
      node *merge_sibling() {
        //from oi.wiki
        if (sibling == nullptr) {
          return this;
        }
        node *y = sibling;
        node *c = y->sibling;
        sibling = nullptr;
        y->sibling = nullptr;

        node* pair;
        try {
          pair = merge(this, y);
          if (c == nullptr) {
            return pair;
          }
          c = c->merge_sibling();
          return merge(c, pair);
        } catch (...) {
          sibling = y;
          y->sibling = c;
          throw sjtu::runtime_error();
        }
      }

      /**
       * @brief merge two nodes
       * @return node* of the new node
       */
      static node *merge(node *x, node *y) {
        //from oi.wiki
        if (x == nullptr) return y;
        if (y == nullptr) return x;

        if (Compare{}(x->data, y->data)) {
          std::swap(x, y);
        };
        y->sibling = x->son;
        x->son = y;
        return x;
      }
    };

    node *root;
    size_t _size;

    /**
     *
     */
    explicit priority_queue(node &single_node) {
      root = &single_node;
      _size = 1;
    }

  public:
    /**
     * @brief default constructor
     */
    priority_queue() {
      root = nullptr;
      _size = 0;
    }

    /**
     * @brief copy constructor
     * @param other the priority_queue to be copied
     */
    priority_queue(const priority_queue &other) {
      if (other.root) {
        root = other.root->copy();
      } else {
        root = nullptr;
      }
      _size = other._size;
    }

    /**
     * @brief deconstructor
     */
    ~priority_queue() {
      delete root;
      root = nullptr;
    }

    /**
     * @brief Assignment operator
     * @param other the priority_queue to be assigned from
     * @return a reference to this priority_queue after assignment
     */
    priority_queue &operator=(const priority_queue &other) {
      if (this == &other) {
        return *this;
      }
      this->~priority_queue();
      if (other.root) {
        root = other.root->copy();
      } else {
        root = nullptr;
      }
      _size = other._size;
      return *this;
    }

    /**
     * @brief get the top element of the priority queue.
     * @return a reference of the top element.
     * @throws container_is_empty if empty() returns true
     */
    const T &top() const {
      if (empty()) {
        throw container_is_empty();
      }
      return root->data;
    }

    /**
     * @brief push new element to the priority queue.
     * @param e the element to be pushed
     */
    void push(const T &e) {
      node *temp = new node(e);
      try {
        root = node::merge(root, temp);
      } catch (...) {
        delete temp;
        throw sjtu::runtime_error();
      }
      ++_size;
    }

    /**
     * @brief delete the top element from the priority queue.
     * @throws container_is_empty if empty() returns true
     */
    void pop() {
      if (empty()) {
        throw container_is_empty();
      }
      node *temp_root = nullptr;
      if (root->son) {
        temp_root = root->son->merge_sibling();
      }
      root->son = nullptr;
      root->sibling = nullptr;
      delete root;
      root = temp_root;
      --_size;
    }

    /**
     * @brief return the number of elements in the priority queue.
     * @return the number of elements.
     */
    size_t size() const {
      return _size;
    }

    /**
     * @brief check if the container is empty.
     * @return true if it is empty, false otherwise.
     */
    bool empty() const {
      return root == nullptr;
    }

    /**
     * @brief merge another priority_queue into this one.
     * The other priority_queue will be cleared after merging.
     * The complexity is at most O(logn).
     * @param other the priority_queue to be merged.
     */
    void merge(priority_queue &other) {
      try {
        root = node::merge(root, other.root);
        _size += other._size;
        other.root = nullptr;
        other._size = 0;
      } catch (...) {
        throw sjtu::runtime_error();
      }
    }
  };
}

#endif
