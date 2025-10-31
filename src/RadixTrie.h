#pragma once
#include "RadixNode.h"
#include <string_view>
#include <vector>

class RadixTrie {
  private:
	std::unique_ptr<RadixNode> root;
	size_t word_count_;

	// When i found out about "using", i was like: "don't tell me using uint64 =
	// long long; is proably a thing" and it is, and that amazes me for some
	// reason (i came from C)
	using ChildVec = std::vector<std::pair<char, std::unique_ptr<RadixNode>>>;
	static ChildVec::iterator find_child(RadixNode *node, char c);
	static ChildVec::const_iterator find_child(const RadixNode *node, char c);

	size_t common_prefix_length(std::string_view s1,
								std::string_view s2) const noexcept;
	RadixNode *find_node(std::string_view word) const;

	void collect_words_from_node(const RadixNode *node,
								 const std::string &prefix,
								 std::vector<std::string> &result) const;
	void cleanup_orphaned_nodes(std::string_view word);
	void split_node(RadixNode *current, char first_char, size_t common_len,
					const std::string &child_key, std::string_view remaining);
	void calculate_heights_recursive(const RadixNode *node, int current_depth,
									 std::vector<int> &heights) const;
	void collect_word_lengths_recursive(const RadixNode *node,
										int current_length,
										std::vector<int> &lengths) const;

	size_t calculate_memory_recursive(const RadixNode *node) const;
	void pattern_match_recursive(const RadixNode *node,
								 const std::string &current_word,
								 const std::string &pattern,
								 std::vector<std::string> &results) const;
	bool matches_pattern(const std::string &word,
						 const std::string &pattern) const;

  public:
	struct HeightStats {
		int min_height;
		int max_height;
		double average_height;
		int mode_height;
		std::vector<int> all_heights;
	};

	struct MemoryStats {
		size_t total_bytes;
		size_t node_count;
		size_t string_bytes;
		size_t overhead_bytes;
		double bytes_per_word;
	};

	struct WordMetrics {
		int min_length;
		int max_length;
		double average_length;
		int mode_length;
		std::vector<int> length_distribution;
		size_t total_characters;
	};

	RadixTrie();

	void insert(std::string_view word);
	bool search(std::string_view word) const;
	bool starts_with(std::string_view prefix) const;
	std::vector<std::string> words_with_prefix(std::string_view prefix) const;
	bool remove(std::string_view word);
	bool empty() const noexcept;
	size_t size() const noexcept;
	void clear();

	// default buffer size of 1MB
	size_t bulk_insert_from_file(const std::string &path,
								 size_t buffer_size = 1024 * 1024);

	HeightStats get_height_stats() const;
	MemoryStats get_memory_stats() const;
	WordMetrics get_word_metrics() const;
	std::vector<std::string> pattern_search(const std::string &pattern) const;
};