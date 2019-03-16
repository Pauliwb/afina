#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) 
{ 
	auto found_it = _lru_index.find(key);
    if (found_it != _lru_index.end()) 
    {
        return SetIter(found_it, value);
    }

    size_t add_size = key.size() + value.size();
    if (!Check_free(add_size)) 
    {
        return false;  // Error: Cannot free enough memory!
    }

    std::unique_ptr<lru_node> tmp(new lru_node{key, value, nullptr, nullptr});

    if (_lru_tail != nullptr) 
    {
        tmp->prev = _lru_tail;
        _lru_tail->next.swap(tmp);
        _lru_tail = (_lru_tail->next).get();
    } 
    else 
    {
        _lru_head.swap(tmp);
        _lru_tail = _lru_head.get();
    }

    _lru_index.insert(std::make_pair(std::reference_wrapper<const std::string>(_lru_tail->key), std::reference_wrapper<lru_node>(*_lru_tail)));
    _current_size += add_size;

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) 
{
    auto found_it = _lru_index.find(key);
	if (found_it != _lru_index.end()) 
	{
        return false;
    }

    return Put(key, value); 
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

///////////////// Check_free, SetIter,  DeleteIt, UpdateList

bool SimpleLRU::Check_free(size_t size_need) 
{
    while(size_need > _max_size - _current_size) 
    {
        if (_lru_head == nullptr) 
        {
            return false;
        }

        {
        std::unique_ptr<lru_node> old_head;
        _current_size -= _lru_head->key.size() + _lru_head->value.size();

        _lru_head->next->prev = nullptr;
        old_head.swap(_lru_head);
        _lru_head = std::move(old_head->next);

        _lru_index.erase(old_head->key);
        }
    }

    return true;
}


// Set element value by _lru_index iterator
bool SimpleLRU::SetIter(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                                 std::less<std::string>>::iterator toset_it, const std::string &value)
{
    lru_node &tmp_node = toset_it->second;
    size_t delta_size = value.size() - tmp_node.value.size();

    if (!Check_free(delta_size)) 
    {
        return false;
    }

    tmp_node.value = value;
    return UpdateList(tmp_node);
}

// Delete node by it's iterator in _lru_index
bool SimpleLRU::DeleteIt(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                                      std::less<std::string>>::iterator todel_it) 
{
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
        tmp.swap(_lru_head); 
        _lru_head = std::move(todel_node.next);
    }

    //Delete from index
    _lru_index.erase(todel_it);
    return true;
}



// Update node by it's reference
bool SimpleLRU::UpdateList(lru_node &updated_ref) 
{
    if (&updated_ref == _lru_tail) 
    {
        return true;
    }
    if (&updated_ref == _lru_head.get()) 
    {
        _lru_head.swap(updated_ref.next);
        _lru_head->prev = nullptr;
    } 
    else 
    {
        updated_ref.next->prev = updated_ref.prev;
        updated_ref.prev->next.swap(updated_ref.next);
    }

    _lru_tail->next.swap(updated_ref.next);
    updated_ref.prev = _lru_tail;
    _lru_tail = &updated_ref;
    return true;
}

} // namespace Backend
} // namespace Afina













