#ifndef CPPCONCURRENCYTOOLS_THREADSAFE_LOOKUP_TABLE_H
#define CPPCONCURRENCYTOOLS_THREADSAFE_LOOKUP_TABLE_H

#include <utility>
#include <list>
#include <shared_mutex>
#include <vector>
#include <map>

template<typename Key, typename Value, typename Hash=std::hash<Key> >
class threadsafe_lookup_table {
private: // member class
    class bucket_type {
    private:
        using bucket_value = std::pair<Key, Value>;
        using bucket_data = std::list<bucket_value>;
        using bucket_iterator = typename bucket_data::iterator;
        using bucket_const_iterator = typename bucket_data::const_iterator;
    public:
        bucket_data data;
        mutable std::shared_mutex mutex; // C++17
    private:
        bucket_const_iterator find_entry_for(Key const &key) const {
            return std::find_if(data.begin(), data.end(), [&](bucket_value const &item) { return item.first == key; });
        }

        bucket_iterator find_entry_for(Key const &key) {
            return std::find_if(data.begin(), data.end(), [&](bucket_value const &item) { return item.first == key; });
        }

    public:
        Value value_for(Key const &key, Value const &default_value) const {
            std::shared_lock<std::shared_mutex> lock(mutex);
            bucket_const_iterator const found_entry = find_entry_for(key);
            return (found_entry == data.end()) ? default_value : found_entry->second;
        }

        void add_or_update_mapping(Key const &key, Value const &value) {
            std::unique_lock<std::shared_mutex> lock(mutex);
            bucket_iterator found_entry = find_entry_for(key);
            if (found_entry == data.end()) {
                data.push_back(bucket_value(key, value));
            } else {
                found_entry->second = value;
            }
        }

        void remove_mapping(Key const &key) {
            std::unique_lock<std::shared_mutex> lock(mutex);
            bucket_iterator found_entry = find_entry_for(key);
            if (found_entry != data.end()) {
                data.erase(found_entry);
            }
        }
    };

private:
    std::vector<std::unique_ptr<bucket_type>> buckets;
    Hash hasher;

private:
    bucket_type &get_bucket(Key const &key) const {
        std::size_t const bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }

public:
    explicit threadsafe_lookup_table(unsigned num_buckets = 19, Hash const &hasher_ = Hash()) :
            buckets(num_buckets), hasher(hasher_) {
        for (unsigned i = 0; i < num_buckets; ++i) {
            buckets[i].reset(new bucket_type);
        }
    }

    threadsafe_lookup_table(threadsafe_lookup_table const &other) = delete;

    threadsafe_lookup_table &operator=(threadsafe_lookup_table const &other) = delete;

    Value value_for(Key const &key, Value const &default_value = Value()) const {
        return get_bucket(key).value_for(key, default_value);
    }

    void add_or_update_mapping(Key const &key, Value const &value) {
        get_bucket(key).add_or_update_mapping(key, value);
    }

    void remove_mapping(Key const &key) {
        get_bucket(key).remove_mapping(key);
    }

    std::map<Key, Value> get_map() const {
        std::vector<std::unique_lock<std::shared_mutex>> locks;
        for (unsigned i = 0; i < buckets.size(); ++i) {
            locks.push_back(std::unique_lock<std::shared_mutex>(buckets[i]->mutex));
        }
        std::map<Key, Value> res;
        for (unsigned i = 0; i < buckets.size(); ++i) {
            for (auto it = buckets[i]->data.cbegin(); it != buckets[i]->data.cend();
                 ++it) {
                res.insert(*it);
            }
        }
        return res;
    }
};

#endif //CPPCONCURRENCYTOOLS_THREADSAFE_LOOKUP_TABLE_H
