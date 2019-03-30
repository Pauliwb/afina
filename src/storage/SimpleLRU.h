#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage 
{
public:
    // Добавили _current_size(0), _lru_head(nullptr), _lru_tail(nullptr)
    SimpleLRU(size_t max_size = 1024) : _max_size(max_size), _current_size(0), _lru_head(nullptr), _lru_tail(nullptr) {}

    ~SimpleLRU()
     {
        _lru_index.clear();
        while (_lru_head != nullptr)
        {
            _lru_head.reset(_lru_head->next.release());
        }
    }

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

private:
    // LRU cache node
    using lru_node = struct lru_node 
    {
        const std::string key;
        std::string value;
        lru_node *prev;
        std::unique_ptr<lru_node> next;
    };

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;


    // Current number of bytes, stored in the cache. 
    std::size_t _current_size;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;
    lru_node *_lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    // X_X Добавили const
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>> _lru_index;

// X_X

    // 1) Put a new element 
    bool PutImpl(const std::string &key, const std::string &value);


    // 2) Set element value by _lru_index iterator
    bool SetIter(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                          std::less<std::string>>::iterator toset_it,
                 const std::string &value);

    // 3) Delete node by it's iterator in _lru_index
    bool DeleteIt(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                               std::less<std::string>>::iterator todel_it);


    // 4) Update node by it's reference
    bool UpdateList(lru_node &updated_node);


    // 5) Remove LRU-nodes until we get as much as needfree free space
    bool GetFreeImpl(size_t needfree);


};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
