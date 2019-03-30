#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) 
{
    auto found_it = _lru_index.find(key);
    if (found_it == _lru_index.end()) 
    {
        return PutImpl(key, value);
    }
    return SetIter(found_it, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) 
{
    if (_lru_index.find(key) != _lru_index.end()) 
    {
        return false;
    }
    return PutImpl(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) 
{
 	auto found_it = _lru_index.find(key);
    if (found_it == _lru_index.end()) 
    {
        return false;
    }

    return SetIter(found_it, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) 
{
	auto todel_it = _lru_index.find(key);
    if (todel_it == _lru_index.end())
    {
        return false;
    }

    return DeleteIt(todel_it);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value)
{
	auto found_it = _lru_index.find(key);
    if (found_it == _lru_index.end()) 
    {
        return false;
    }

    value = found_it->second.get().value;
    return UpdateList(found_it->second.get()); 
}

///////////////// 1) PutImpl, 2) SetIter, 3) DeleteIt, 4) UpdateList, 5) GetFreeImpl


// 1) Put a new element 
bool SimpleLRU::PutImpl(const std::string &key, const std::string &value) 
{
    size_t addsize = key.size() + value.size();
    if (!GetFreeImpl(addsize)) 
    {
        return false;
    }

    std::unique_ptr<lru_node> toput{new lru_node{key, value}};

    if (_lru_tail != nullptr) 
    {
        toput->prev = _lru_tail;
        _lru_tail->next.swap(toput);
        _lru_tail = _lru_tail->next.get();
    } 
    else
    {
        _lru_head.swap(toput);
        _lru_tail = _lru_head.get();
    }

    _lru_index.insert(std::make_pair(std::reference_wrapper<const std::string>(_lru_tail->key),
                                     std::reference_wrapper<lru_node>(*_lru_tail)));
    _current_size += addsize;
    return true;
}


// 2) Set element value by _lru_index iterator
bool SimpleLRU::SetIter(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                                 std::less<std::string>>::iterator toset_it, const std::string &value)
{
    lru_node &tmp_node = toset_it->second;
    size_t delta_size = value.size() - tmp_node.value.size();

    if (!GetFreeImpl(delta_size)) 
    {
        return false;
    }

    tmp_node.value = value;
    return UpdateList(tmp_node);
}



// 3) Delete node by it's iterator in _lru_index
bool SimpleLRU::DeleteIt(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                                      std::less<std::string>>::iterator todel_it) 
{
    // Delete from list
    std::unique_ptr<lru_node> tmp;
    lru_node &todel_node = todel_it->second;
    _current_size -= todel_node.key.size() + todel_node.value.size();

    if (todel_node.next) 
    {
        todel_node.next->prev = todel_node.prev;
    }
    if (todel_node.prev) 
    {
        tmp.swap(todel_node.prev->next); // extend lifetime of todel_node
        todel_node.prev->next = std::move(todel_node.next);
    } 
    else 
    {
        tmp.swap(_lru_head); // extend lifetime of todel_node
        _lru_head = std::move(todel_node.next);
    }

    //Delete from index
    _lru_index.erase(todel_it);
    return true;
}


// 4) Update node by it's reference
bool SimpleLRU::UpdateList(lru_node &updated_node) 
{
    if (&updated_node == _lru_tail) 
    {
        return true;
    }
    if (&updated_node == _lru_head.get()) 
    {
        _lru_head.swap(updated_node.next);
        _lru_head->prev = nullptr;
    } 
    else 
    {
        updated_node.next->prev = updated_node.prev;
        updated_node.prev->next.swap(updated_node.next);
    }

    _lru_tail->next.swap(updated_node.next);
    updated_node.prev = _lru_tail;
    _lru_tail = &updated_node;
    return true;
}


// 5) Remove LRU-nodes until we get as much as needfree free space
bool SimpleLRU::GetFreeImpl(size_t needfree) 
{
    if (needfree > _max_size)
    {
        return false;
    }
    while (_max_size - _current_size < needfree)
    {
        // Delete node
        std::unique_ptr<lru_node> tmp;
        _current_size -= _lru_head->key.size() + _lru_head->value.size();

        _lru_head->next->prev = nullptr;
        tmp.swap(_lru_head);
        _lru_head = std::move(tmp->next);

        _lru_index.erase(tmp->key);
    }
    return true;
}



} // namespace Backend
} // namespace Afina













