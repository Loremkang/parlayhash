#include <mutex>
#include <shared_mutex>

#include "absl/container/flat_hash_map.h"


template <typename K,
	  typename V,
	  class Hash = std::hash<K>,
	  class KeyEqual = std::equal_to<K>,
	  typename Mutex = std::mutex,
	  typename ReadGuard = std::unique_lock<Mutex>,
	  typename WriteGuard = std::unique_lock<Mutex>>
struct unordered_map {

  using umap = absl::flat_hash_map<K, V, Hash, KeyEqual>;
  struct alignas(64) entry {
    Mutex m;
    umap sub_table;
  };

  std::vector<entry> table;
  long num_buckets;

  unsigned int hash_to_shard(const K& k) {
    return (Hash{}(k) * UINT64_C(0xbf58476d1ce4e5b9)) & (num_buckets-1);
  }

  std::optional<V> find(const K& k) {
    size_t idx = hash_to_shard(k);
    ReadGuard g_{table[idx].m};
    auto r = table[idx].sub_table.find(k);
    if (r != table[idx].sub_table.end()) return (*r).second;
    else return std::optional<V>();
  }

  std::optional<V> find_(const K& k) {
    return find(k);
  }

  bool insert(const K& k, const V& v) {
    size_t idx = hash_to_shard(k);
    WriteGuard g_{table[idx].m};
    bool result = table[idx].sub_table.insert(std::make_pair(k, v)).second;    
    return result;
  }

  bool remove(const K& k) {
    size_t idx = hash_to_shard(k);
    WriteGuard g_{table[idx].m};
    bool result = table[idx].sub_table.erase(k) == 1;
    return result;
  }

  unordered_map(size_t n) {
    int n_bits = std::round(std::log2(n));
    int bits = n_bits - 2;
    num_buckets = 1ull << bits; // must be a power of 2

    table = std::vector<entry>(num_buckets);
    for (int i=0; i < num_buckets; i++)
      table[i].sub_table = umap(n/num_buckets);
  }

  long size() {
    long n = 0;
    for (int i = 0; i < num_buckets; i++)
      n += table[i].sub_table.size();
    return n;}
};

