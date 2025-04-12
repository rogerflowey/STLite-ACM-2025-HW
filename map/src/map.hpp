/**
 * implement a container like std::map
 */
#ifndef SJTU_MAP_HPP
#define SJTU_MAP_HPP

// only for std::less<T>
#include <functional>
#include <concepts>
#include <cstddef>

#include "utility.hpp"
#include "exceptions.hpp"


namespace sjtu {
  template<
    class Key,
    class T,
    class Compare = std::less<Key> >
  //typedef int Key;
  //typedef int T;
  //typedef std::less<int> Compare;
  class map {
  public:
    typedef pair<const Key, T> value_type;
  private:
    struct DataNode;
    struct Node {
      DataNode* data;
      Node *ls=nullptr,*rs=nullptr,*parent=nullptr;

      int height=0;

      explicit Node(const value_type& value) {
        data = new DataNode(value);
        data->node = this;
      }

      ~Node() {
        delete data;
        delete ls;
        delete rs;
      }

      Node* copy() {
        Node* temp = new Node(data->data);
        temp->height = height;
        if(ls) {
          temp->ls = ls->copy();
          temp->ls->parent = temp;
        }
        if(rs) {
          temp->rs = rs->copy();
          temp->rs->parent=temp;
        }
        return temp;
      }

      Node* next() {
        Node* temp = this;
        if(rs) {
          temp = rs;
          while (temp->ls) {
            temp = temp->ls;
          }
          return temp;
        }
        while (temp->parent&&temp->parent->rs==temp) {
          temp = temp->parent;
        }
        return temp->parent;
      }

      Node* prev() {
        Node* temp = this;
        if(ls) {
          temp = ls;
          while (temp->rs) {
            temp = temp->rs;
          }
          return temp;
        }
        while (temp->parent&&temp->parent->ls==temp) {
          temp = temp->parent;
        }
        return temp->parent;
      }

      Node* swap(Node* other) {
        std::swap(data,other->data);
        data->node = this;
        other->data->node=other;
        return other;
      }
      void update_height() {
        height = std::max(ls?ls->height:0,rs?rs->height:0)+1;
      }


    };

    struct DataNode {
      value_type data;
      Node* node;

      explicit DataNode(const value_type& value):data(value) {
        node = nullptr;
      }
    };

    Node* root;
    size_t _size;

    mutable Node* front_cache;
    mutable Node* last_cache;
    mutable bool is_dirty;

    Compare cmp;


    struct FindResult {
      Node* curr;
      Node* father;
      bool is_left;


      /**
       * 任何其他操作都可能导致失效
       *
       */
      Node* new_node(const value_type& v,map& map) {
        Node* temp = new Node(v);
        temp->parent = father;
        if (father == nullptr) {
          map.root = temp;
        } else {
          if (is_left) {
            father->ls = temp;
          } else {
            father->rs = temp;
          }
        }
        ++map._size;
        map.is_dirty = true;
        return temp;
      }
    };


    /**
     * @return curr:已存在的节点（若找到） father: 插入位置的父节点
     *
     */
    FindResult find_unique(const Key& key) const{
      Node* temp = root;
      Node* father = nullptr;
      bool comp = false;
      while(temp) {
        father = temp;
        comp = cmp(key,temp->data->data.first);
        if(!comp&&!cmp(temp->data->data.first,key)) {
          return {temp,nullptr,false};
        }
        temp = (comp?temp->ls:temp->rs);
      }
      return {nullptr,father,comp};
    }

    Node* find_min() const{
      Node* temp = root;
      while (temp&&temp->ls) {
        temp = temp->ls;
      }
      return temp;
    }

    Node* find_max() const{
      Node* temp = root;
      while (temp&&temp->rs) {
        temp = temp->rs;
      }
      return temp;
    }

    Node* front() const{
      if(is_dirty) {
        front_cache = find_min();
        last_cache = find_max();
        is_dirty = false;
      }
      return front_cache;
    }

    Node* last() const{
      if(is_dirty) {
        front_cache = find_min();
        last_cache = find_max();
        is_dirty = false;
      }
      return last_cache;
    }

    void rotate_left(Node* node) {
      if((!node)||(!node->rs)){return;}
      Node* temp = node->rs;
      temp->parent = node->parent;
      if(root!=node) {
        ((node->parent->ls==node)?node->parent->ls:node->parent->rs)=temp;
      }else {
        root = temp;
      }
      node->rs = temp->ls;
      if(node->rs) node->rs->parent = node;
      temp->ls = node;
      node->parent = temp;
      node->update_height();
      temp->update_height();
    }

    void rotate_right(Node* node) {
      if((!node)||(!node->ls)){return;}
      Node* temp = node->ls;
      temp->parent = node->parent;
      if(root!=node) {
        ((node->parent->ls==node)?node->parent->ls:node->parent->rs)=temp;
      }else {
        root = temp;
      }
      node->ls = temp->rs;
      if(node->ls) node->ls->parent = node;
      temp->rs = node;
      node->parent = temp;
      node->update_height();
      temp->update_height();
    }

    inline static int h(Node* node){
      return node?node->height:0;
    }

    //from oi.wiki
    void maintain(Node* node) {
      //TODO
      Node* parent = node->parent;
      node->update_height();
      Node* ls = node->ls;
      Node* rs = node->rs;
      if(h(ls)-h(rs)==2) {
        if(h(ls->ls)>=h(ls->rs)) {
          rotate_right(node);
        } else {
          rotate_left(ls);
          rotate_right(node);
        }
      } else if(h(ls)-h(rs)==-2) {
        if(h(rs->ls)<=h(rs->rs)) {
          rotate_left(node);
        } else {
          rotate_right(rs);
          rotate_left(node);
        }
      }
      if(parent) {
        maintain(parent);
      }
    }

  public:

    /**
     * see BidirectionalIterator at CppReference for help.
     *
     * if there is anything wrong throw invalid_iterator.
     *     like it = map.begin(); --it;
     *       or it = map.end(); ++end();
     */
    class const_iterator;

    class iterator {
    private:
      DataNode* ptr;
      map* map_ptr;
      friend const_iterator;
      friend map;
    public:
      iterator(): map_ptr(nullptr) {
        ptr = nullptr;
      }

      iterator(const iterator &other): map_ptr(other.map_ptr) {
        ptr = other.ptr;
      }

      explicit iterator(DataNode* ptr,map* map_ptr):ptr(ptr),map_ptr(map_ptr){};


      iterator operator++(int) {
        if(ptr == nullptr) {
          throw invalid_iterator();
        }
        iterator temp = *this;
        auto temp_ptr = ptr->node->next();
        if(!temp_ptr) {
          ptr = nullptr;
        }else {
          ptr = temp_ptr->data;
        }
        return temp;
      }


      iterator &operator++() {
        if(ptr == nullptr) {
          throw invalid_iterator();
        }
        auto temp_ptr = ptr->node->next();
        if(!temp_ptr) {
          ptr = nullptr;
        }else {
          ptr = temp_ptr->data;
        }
        return *this;
      }

      iterator operator--(int) {
        iterator temp = *this;
        auto temp_ptr = ptr?ptr->node->prev():map_ptr->last();
        if(!temp_ptr) {
          throw invalid_iterator();
        }
        ptr = temp_ptr->data;
        return temp;
      }

      iterator &operator--() {
        auto temp_ptr = ptr?ptr->node->prev():map_ptr->last();
        if(!temp_ptr) {
          throw invalid_iterator();
        }
        ptr = temp_ptr->data;
        return *this;
      }

      /**
       * a operator to check whether two iterators are same (pointing to the same memory).
       */
      value_type &operator*() const {
        if(ptr==nullptr) {
          throw invalid_iterator();
        }
        return ptr->data;
      }

      bool operator==(const iterator &rhs) const {
        return ptr == rhs.ptr&&map_ptr==rhs.map_ptr;
      }

      bool operator==(const const_iterator &rhs) const {
        return ptr == rhs.ptr&&map_ptr==rhs.map_ptr;
      }

      /**
       * some other operator for iterator.
       */
      bool operator!=(const iterator &rhs) const {
        return !(*this==rhs);
      }

      bool operator!=(const const_iterator &rhs) const {
        return !(*this==rhs);
      }

      /**
       * for the support of it->first.
       * See <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/> for help.
       */
      value_type *operator->() const
        noexcept {
        return &(ptr->data);
      }
    };

    class const_iterator {
      // it should have similar member method as iterator.
      //  and it should be able to construct from an iterator.
    private:
      // data members.
      const DataNode* ptr;
      const map* map_ptr;
      friend iterator;
      friend map;
    public:
      const_iterator() {
        ptr = nullptr;
        map_ptr = nullptr;
      }

      const_iterator(const const_iterator &other) {
        ptr = other.ptr;
        map_ptr = other.map_ptr;
      }

      const_iterator(const iterator &other) {
        ptr = other.ptr;
        map_ptr = other.map_ptr;
      }

      explicit const_iterator(const DataNode* ptr,const map* map_ptr):ptr(ptr),map_ptr(map_ptr) {};


      const_iterator operator++(int) {
        if(ptr == nullptr) {
          throw invalid_iterator();
        }
        const_iterator temp = *this;
        auto temp_ptr = ptr->node->next();
        if(!temp_ptr) {
          ptr = nullptr;
        }else {
          ptr = temp_ptr->data;
        }
        return temp;
      }


      const_iterator &operator++() {
        if(ptr == nullptr) {
          throw invalid_iterator();
        }
        auto temp_ptr = ptr->node->next();
        if(!temp_ptr) {
          ptr = nullptr;
        }else {
          ptr = temp_ptr->data;
        }
        return *this;
      }

      const_iterator operator--(int) {
        const_iterator temp = *this;
        auto temp_ptr = ptr?ptr->node->prev():map_ptr->last();
        if(!temp_ptr) {
          throw invalid_iterator();
        }
        ptr = temp_ptr->data;
        return temp;
      }

      const_iterator &operator--() {
        auto temp_ptr = ptr?ptr->node->prev():map_ptr->last();
        if(!temp_ptr) {
          throw invalid_iterator();
        }
        ptr = temp_ptr->data;
        return *this;
      }

      /**
       * a operator to check whether two iterators are same (pointing to the same memory).
       */
      const value_type &operator*() const {
        if(ptr==nullptr) {
          throw invalid_iterator();
        }
        return ptr->data;
      }

      bool operator==(const iterator &rhs) const {
        return ptr == rhs.ptr&&map_ptr==rhs.map_ptr;
      }

      bool operator==(const const_iterator &rhs) const {
        return ptr == rhs.ptr&&map_ptr==rhs.map_ptr;
      }

      /**
       * some other operator for iterator.
       */
      bool operator!=(const iterator &rhs) const {
        return !(*this==rhs);
      }

      bool operator!=(const const_iterator &rhs) const {
        return !(*this==rhs);
      }

      /**
       * for the support of it->first.
       * See <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/> for help.
       */
      const value_type *operator->() const
        noexcept {
        return &(ptr->data);
      }
    };


    map(){
      _size = 0;
      root = nullptr;
      front_cache = last_cache = nullptr;
      is_dirty = false;
    }

    map(const map &other) {
      if(!other.root) {
        _size = 0;
        root = nullptr;
        front_cache = last_cache = nullptr;
        is_dirty = false;
        return;
      }
      _size = other._size;
      root = other.root->copy();
      front_cache = last_cache = nullptr;
      is_dirty = true;
    }

    map &operator=(const map &other) {
      if (this != &other) {
        map tmp(other);
        std::swap(root, tmp.root);
        std::swap(_size, tmp._size);
        std::swap(cmp, tmp.cmp);
        front_cache = last_cache = nullptr;
        is_dirty = true;
      }
      return *this;
    }

    ~map() {
      delete root;

    }

    /**
     * access specified element with bounds checking
     * Returns a reference to the mapped value of the element with key equivalent to key.
     * If no such element exists, an exception of type `index_out_of_bound'
     */
    T &at(const Key &key) {
      auto [temp,_unused1,_unused2] = find_unique(key);
      if(!temp) {
        throw index_out_of_bound();
      }
      return temp->data->data.second;
    }

    const T &at(const Key &key) const {
      auto [temp,_unused1,_unused2] = find_unique(key);
      if(!temp) {
        throw index_out_of_bound();
      }
      return temp->data->data.second;
    }

    /**
     * access specified element
     * Returns a reference to the value that is mapped to a key equivalent to key,
     *   performing an insertion if such key does not already exist.
     */
    T &operator[](const Key &key)
      requires std::is_default_constructible_v<T>
    {
      auto find_result = find_unique(key);
      Node* temp = find_result.curr;
      if(!temp) {
        temp = find_result.new_node({key,T()},*this);
        maintain(temp);
        is_dirty = true;
      }
      return temp->data->data.second;
    }

    /**
     * behave like at() throw index_out_of_bound if such key does not exist.
     */
    const T &operator[](const Key &key) const
    {
      return at(key);
    }

    /**
     * return a iterator to the beginning
     */
    iterator begin() {
      return iterator((front()?front()->data:nullptr),this);
    }

    const_iterator cbegin() const {
      return const_iterator(front()?front()->data:nullptr,this);
    }

    /**
     * return a iterator to the end
     * in fact, it returns past-the-end.
     */
    iterator end() {
      return iterator(nullptr,this);
    }

    const_iterator cend() const {
      return const_iterator(nullptr,this);
    }

    /**
     * checks whether the container is empty
     * return true if empty, otherwise false.
     */
    bool empty() const {
      return root==nullptr;
    }

    /**
     * returns the number of elements.
     */
    size_t size() const {
      return _size;
    }

    /**
     * clears the contents
     */
    void clear() {
      delete root;
      root = nullptr;
      _size = 0;
      front_cache = last_cache = nullptr;
      is_dirty = false;
    }

    /**
     * insert an element.
     * return a pair, the first of the pair is
     *   the iterator to the new element (or the element that prevented the insertion),
     *   the second one is true if insert successfully, or false.
     */
    pair<iterator, bool> insert(const value_type &value) {
      auto find_result = find_unique(value.first);
      if(find_result.curr) {
        return {iterator(find_result.curr->data,this),false};
      }
      is_dirty = true;
      Node* temp = find_result.new_node(value,*this);
      maintain(temp);
      return {iterator(temp->data,this),true};
    }

    /**
     * erase the element at pos.
     *
     * throw if pos pointed to a bad element (pos == this->end() || pos points an element out of this)
     */
    void erase(iterator pos) {
      if(pos == this->end()||pos.map_ptr != this) {
        throw invalid_iterator();
      }
      --_size;
      is_dirty = true;
      Node* temp = pos.ptr->node;
      if (temp->ls&&temp->rs) {
        temp = temp->swap(temp->next());
      }
      if(temp->ls||temp->rs) {
        temp = temp->ls?temp->swap(temp->ls):temp->swap(temp->rs);
      }
      if(temp==root) {
        root = nullptr;
        delete temp;
        return;
      }
      auto temp_parent = temp->parent;
      (temp_parent->ls==temp?temp_parent->ls:temp_parent->rs) = nullptr;
      delete temp;
      maintain(temp_parent);
    }

    /**
     * Returns the number of elements with key
     *   that compares equivalent to the specified argument,
     *   which is either 1 or 0
     *     since this container does not allow duplicates.
     * The default method of check the equivalence is !(a < b || b > a)
     */
    size_t count(const Key &key) const {
      return find_unique(key).curr!=nullptr;
    }

    /**
     * Finds an element with key equivalent to key.
     * key value of the element to search for.
     * Iterator to an element with key equivalent to key.
     *   If no such element is found, past-the-end (see end()) iterator is returned.
     */
    iterator find(const Key &key) {
      auto result = find_unique(key);
      return iterator((result.curr?result.curr->data:nullptr),this);
    }

    const_iterator find(const Key &key) const {
      auto result = find_unique(key);
      return const_iterator((result.curr?result.curr->data:nullptr),this);
    }
  };
}

#endif
