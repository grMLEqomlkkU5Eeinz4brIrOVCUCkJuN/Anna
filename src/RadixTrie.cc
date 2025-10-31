#include "RadixTrie.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <numeric>
#include <unordered_map>

RadixTrie::RadixTrie() : root(std::make_unique<RadixNode>()), word_count_(0) {}

RadixTrie::ChildVec::iterator RadixTrie::find_child(RadixNode *node, char c) {
	auto &v = node->children;
	return std::lower_bound(
		v.begin(), v.end(), c,
		[](const std::pair<char, std::unique_ptr<RadixNode>> &p, char key) {
			return p.first < key;
		});
}

RadixTrie::ChildVec::const_iterator RadixTrie::find_child(const RadixNode *node,
														  char c) {
	const auto &v = node->children;
	return std::lower_bound(
		v.begin(), v.end(), c,
		[](const std::pair<char, std::unique_ptr<RadixNode>> &p, char key) {
			return p.first < key;
		});
}

size_t RadixTrie::common_prefix_length(std::string_view s1,
									   std::string_view s2) const noexcept {
	size_t i = 0;
	const size_t min_len = std::min(s1.length(), s2.length());
	while (i < min_len && s1[i] == s2[i]) {
		++i;
	}
	return i;
}

RadixNode *RadixTrie::find_node(std::string_view word) const {
	if (!root || word.empty())
		return nullptr;

	RadixNode *current = root.get();
	size_t pos = 0;

	while (pos < word.length()) {
		char first_char = word[pos];
		auto it = find_child(current, first_char);

		if (it == current->children.end() || it->first != first_char) {
			return nullptr; // Path doesn't exist
		}

		RadixNode *child = it->second.get();
		const std::string &child_key = child->key;

		if (pos + child_key.length() > word.length()) {
			return nullptr; // Child key is longer than remaining word
		}

		// Check if the child's key matches the remaining word portion
		if (word.substr(pos, child_key.length()) != child_key) {
			return nullptr; // Keys don't match
		}

		pos += child_key.length();
		current = child;
	}

	return current;
}

// file streaming, but the user decides the size
size_t RadixTrie::bulk_insert_from_file(const std::string &path,
										size_t buffer_size) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + path);
	}

	// Allocate buffer on heap to avoid stack overflow
	auto buffer = std::make_unique<char[]>(buffer_size);
	if (!buffer) {
		throw std::runtime_error("Failed to allocate buffer of size: " +
								 std::to_string(buffer_size));
	}

	size_t words_inserted = 0;
	std::string carry; // carry over a partial line between chunks

	while (!file.eof()) {
		file.read(buffer.get(), buffer_size);
		std::streamsize bytes_read = file.gcount();

		if (bytes_read <= 0) {
			break;
		}

		std::streamsize line_start = 0;
		for (std::streamsize i = 0; i < bytes_read; ++i) {
			char c = buffer[i];
			if (c == '\n' || c == '\r') {
				// We found a line boundary: [line_start, i)
				std::streamsize seg_len = i - line_start;
				if (seg_len > 0 || !carry.empty()) {
					if (!carry.empty()) {
						carry.append(buffer.get() + line_start,
									 static_cast<size_t>(seg_len));
						// Trim carry
						size_t b = 0, e = carry.size();
						while (e > b && std::isspace(static_cast<unsigned char>(
											carry[e - 1])))
							--e;
						while (b < e && std::isspace(static_cast<unsigned char>(
											carry[b])))
							++b;
						if (e > b) {
							std::string_view word_view(carry.data() + b, e - b);
							insert(word_view);
							++words_inserted;
						}
						carry.clear();
					} else {
						// Create a view into the current buffer segment and
						// trim without copying
						const char *seg_ptr = buffer.get() + line_start;
						size_t b = 0, e = static_cast<size_t>(seg_len);
						while (e > b && std::isspace(static_cast<unsigned char>(
											seg_ptr[e - 1])))
							--e;
						while (b < e && std::isspace(static_cast<unsigned char>(
											seg_ptr[b])))
							++b;
						if (e > b) {
							std::string_view word_view(seg_ptr + b, e - b);
							insert(word_view);
							++words_inserted;
						}
					}
				}

				// Skip consecutive CR/LF characters
				while (i + 1 < bytes_read &&
					   (buffer[i + 1] == '\n' || buffer[i + 1] == '\r')) {
					++i;
				}
				line_start = i + 1;
			}
		}

		// Handle remaining partial line at the end of the buffer
		if (line_start < bytes_read) {
			carry.append(buffer.get() + line_start,
						 static_cast<size_t>(bytes_read - line_start));
		}
	}

	// Process any remaining carry as the last line
	if (!carry.empty()) {
		size_t b = 0, e = carry.size();
		while (e > b && std::isspace(static_cast<unsigned char>(carry[e - 1])))
			--e;
		while (b < e && std::isspace(static_cast<unsigned char>(carry[b])))
			++b;
		if (e > b) {
			std::string_view word_view(carry.data() + b, e - b);
			insert(word_view);
			++words_inserted;
		}
	}

	file.close();
	return words_inserted;
}

void RadixTrie::collect_words_from_node(
	const RadixNode *node, const std::string &prefix,
	std::vector<std::string> &result) const {
	if (!node)
		return;

	std::string full_word = prefix + node->key;

	if (node->is_end) {
		result.push_back(full_word);
	}

	for (const auto &[ch, child] : node->children) {
		collect_words_from_node(child.get(), full_word, result);
	}
}

void RadixTrie::insert(std::string_view word) {
	if (word.empty())
		return;

	RadixNode *current = root.get();
	size_t pos = 0;

	while (pos < word.length()) {
		char first_char = word[pos];
		auto it = find_child(current, first_char);

		if (it == current->children.end() || it->first != first_char) {
			// No child with this first character, create new node
			auto new_node = std::make_unique<RadixNode>(
				std::string(word.data() + pos, word.length() - pos), current,
				first_char);
			new_node->is_end = true;
			current->children.insert(
				it, std::make_pair(first_char, std::move(new_node)));
			++word_count_;
			return;
		}

		RadixNode *child = it->second.get();
		const std::string &child_key = child->key;
		std::string_view remaining(word.data() + pos, word.length() - pos);

		size_t common_len = common_prefix_length(child_key, remaining);

		if (common_len == child_key.length()) {
			// The child's key is a complete prefix of the remaining word
			pos += common_len;
			current = child;

			if (pos == word.length()) {
				// Word ends here
				if (!child->is_end) {
					child->is_end = true;
					++word_count_;
				}
				return;
			}
		} else {
			// Need to split the child node - use helper method
			split_node(current, first_char, common_len, child_key, remaining);
			pos += common_len;
			current = find_child(current, first_char)->second.get();

			if (pos == word.length()) {
				// Word ends at the intermediate node
				if (!current->is_end) {
					current->is_end = true;
					++word_count_;
				}
				return;
			}
		}
	}
}

bool RadixTrie::search(std::string_view word) const {
	RadixNode *node = find_node(word);
	return node != nullptr && node->is_end;
}

bool RadixTrie::starts_with(std::string_view prefix) const {
	if (prefix.empty()) {
		return !empty();
	}

	RadixNode *current = root.get();
	size_t pos = 0;

	while (pos < prefix.length() && current) {
		char first_char = prefix[pos];
		auto it = find_child(current, first_char);

		if (it == current->children.end() || it->first != first_char) {
			return false; // Path doesn't exist
		}

		RadixNode *child = it->second.get();
		const std::string &child_key = child->key;

		if (pos + child_key.length() > prefix.length()) {
			// Child key is longer than remaining prefix
			// Check if the prefix matches the beginning of the child key
			return prefix.substr(pos) ==
				   child_key.substr(0, prefix.length() - pos);
		}

		// Check if the child's key matches the remaining prefix portion
		if (prefix.substr(pos, child_key.length()) != child_key) {
			return false; // Keys don't match
		}

		pos += child_key.length();
		current = child;
	}

	// If we've consumed the entire prefix, it's a valid prefix
	return pos == prefix.length();
}

std::vector<std::string>
RadixTrie::words_with_prefix(std::string_view prefix) const {
	std::vector<std::string> result;

	if (prefix.empty()) {
		// Return all words in the trie
		collect_words_from_node(root.get(), "", result);
		return result;
	}

	// Find the node that represents the prefix
	RadixNode *current = root.get();
	size_t pos = 0;

	while (pos < prefix.length() && current) {
		char first_char = prefix[pos];
		auto it = find_child(current, first_char);

		if (it == current->children.end() || it->first != first_char) {
			return result; // Prefix not found
		}

		RadixNode *child = it->second.get();
		const std::string &child_key = child->key;

		if (pos + child_key.length() > prefix.length()) {
			// Child key is longer than remaining prefix
			if (prefix.substr(pos) ==
				child_key.substr(0, prefix.length() - pos)) {
				// The prefix ends in the middle of this child's key
				// Collect all words from this child with the correct prefix
				collect_words_from_node(
					child, std::string(prefix.substr(0, pos)), result);
			}
			return result;
		}

		// Check if the child's key matches the remaining prefix portion
		if (prefix.substr(pos, child_key.length()) != child_key) {
			return result; // Keys don't match
		}

		pos += child_key.length();
		current = child;
	}

	// If we've consumed the entire prefix, collect from the current node
	if (pos == prefix.length() && current) {
		// Calculate the prefix up to (but not including) the current node
		std::string base_prefix(prefix);
		if (!current->key.empty() &&
			base_prefix.length() >= current->key.length()) {
			base_prefix.resize(base_prefix.length() - current->key.length());
		}
		collect_words_from_node(current, base_prefix, result);
	}

	return result;
}

void RadixTrie::cleanup_orphaned_nodes(std::string_view word) {
	if (word.empty() || !root)
		return;

	RadixNode *current = root.get();
	size_t pos = 0;

	// Find the node to delete
	while (pos < word.length() && current) {
		char first_char = word[pos];
		auto it = find_child(current, first_char);

		if (it == current->children.end() || it->first != first_char) {
			return; // Path doesn't exist
		}

		RadixNode *child = it->second.get();
		const std::string &child_key = child->key;

		if (pos + child_key.length() > word.length()) {
			return; // Child key is longer than remaining word
		}

		// Check if the child's key matches the remaining word portion
		if (word.substr(pos, child_key.length()) != child_key) {
			return; // Keys don't match
		}

		pos += child_key.length();
		current = child;
	}

	// Only clean up if we found the exact node and it's marked as end
	if (current && current->is_end) {
		// Clean up from the current node back to the root using parent pointers
		while (current && current->parent) {
			RadixNode *parent = current->parent;
			char char_to_remove = current->parent_char;

			// If the current node has no children and is not an end node, it
			// can be removed
			if (current->children.empty() && !current->is_end) {
				auto pit = find_child(parent, char_to_remove);
				if (pit != parent->children.end() &&
					pit->first == char_to_remove) {
					parent->children.erase(pit);
				}
				current = parent; // Move up to parent
			} else {
				// If this node has children or is an end node, stop cleanup
				break;
			}
		}
	}
}

bool RadixTrie::remove(std::string_view word) {
	if (word.empty() || !root)
		return false;

	RadixNode *node = find_node(word);
	if (node && node->is_end) {
		node->is_end = false;
		--word_count_; // Decrement counter

		// Clean up orphaned nodes
		cleanup_orphaned_nodes(word);
		return true;
	}
	return false;
}

bool RadixTrie::empty() const noexcept { return word_count_ == 0; }

size_t RadixTrie::size() const noexcept { return word_count_; }

void RadixTrie::clear() {
	root = std::make_unique<RadixNode>();
	word_count_ = 0; // Reset counter
}

void RadixTrie::split_node(RadixNode *current, char first_char,
						   size_t common_len, const std::string &child_key,
						   std::string_view /* remaining */) {
	// Create intermediate node with common prefix
	auto intermediate =
		std::make_unique<RadixNode>(std::string(child_key.data(), common_len));
	// Set parent linkage for the intermediate node
	intermediate->parent = current;
	intermediate->parent_char = first_char;

	// Get the old child before moving it
	auto it = find_child(current, first_char);
	auto old_child = std::move(it->second);

	// Update child's key to remaining part
	old_child->key.assign(child_key.data() + common_len,
						  child_key.length() - common_len);

	// Move the old child under the intermediate node
	char old_first_char = old_child->key[0];
	RadixNode *old_child_raw = old_child.get();
	auto insert_pos = find_child(intermediate.get(), old_first_char);
	intermediate->children.insert(
		insert_pos, std::make_pair(old_first_char, std::move(old_child)));
	// Fix old child's parent linkage
	old_child_raw->parent = intermediate.get();
	old_child_raw->parent_char = old_first_char;

	// Replace the old child with the intermediate node
	it->second = std::move(intermediate);
}

// Helper method to calculate heights recursively
void RadixTrie::calculate_heights_recursive(const RadixNode *node,
											int current_depth,
											std::vector<int> &heights) const {
	if (!node)
		return;

	if (node->is_end) {
		heights.push_back(current_depth);
	}

	for (const auto &[ch, child] : node->children) {
		calculate_heights_recursive(child.get(), current_depth + 1, heights);
	}
}

// Get trie height statistics
RadixTrie::HeightStats RadixTrie::get_height_stats() const {
	HeightStats stats;
	std::vector<int> heights;

	if (empty()) {
		stats.min_height = 0;
		stats.max_height = 0;
		stats.average_height = 0.0;
		stats.mode_height = 0;
		return stats;
	}

	calculate_heights_recursive(root.get(), 0, heights);

	stats.all_heights = heights;
	stats.min_height = *std::min_element(heights.begin(), heights.end());
	stats.max_height = *std::max_element(heights.begin(), heights.end());

	// Calculate average
	double sum = std::accumulate(heights.begin(), heights.end(), 0.0);
	stats.average_height = sum / heights.size();

	// Calculate mode (most frequent height)
	std::unordered_map<int, int> frequency;
	for (int height : heights) {
		frequency[height]++;
	}

	int max_count = 0;
	for (const auto &[height, count] : frequency) {
		if (count > max_count) {
			max_count = count;
			stats.mode_height = height;
		}
	}

	return stats;
}

// Helper method to calculate memory usage recursively
size_t RadixTrie::calculate_memory_recursive(const RadixNode *node) const {
	if (!node)
		return 0;

	size_t memory = sizeof(RadixNode) + node->key.size();

	for (const auto &[ch, child] : node->children) {
		memory += calculate_memory_recursive(child.get());
	}

	return memory;
}

// Get memory usage statistics
RadixTrie::MemoryStats RadixTrie::get_memory_stats() const {
	MemoryStats stats;

	if (empty()) {
		stats.total_bytes = sizeof(*this) + sizeof(RadixNode); // Just the root
		stats.node_count = 1;
		stats.string_bytes = 0;
		stats.overhead_bytes = stats.total_bytes;
		stats.bytes_per_word = 0.0;
		return stats;
	}

	// Count nodes and calculate memory
	size_t node_count = 0;
	size_t string_bytes = 0;

	std::function<void(const RadixNode *)> count_nodes =
		[&](const RadixNode *node) {
			if (!node)
				return;
			node_count++;
			string_bytes += node->key.size();
			for (const auto &[ch, child] : node->children) {
				count_nodes(child.get());
			}
		};

	count_nodes(root.get());

	stats.node_count = node_count;
	stats.string_bytes = string_bytes;
	stats.total_bytes =
		sizeof(*this) + node_count * sizeof(RadixNode) + string_bytes;
	stats.overhead_bytes = stats.total_bytes - string_bytes;
	stats.bytes_per_word = static_cast<double>(stats.total_bytes) / word_count_;

	return stats;
}

// Helper method to collect word lengths recursively
void RadixTrie::collect_word_lengths_recursive(
	const RadixNode *node, int current_length,
	std::vector<int> &lengths) const {
	if (!node)
		return;

	int new_length = current_length + static_cast<int>(node->key.length());

	if (node->is_end) {
		lengths.push_back(new_length);
	}

	for (const auto &[ch, child] : node->children) {
		collect_word_lengths_recursive(child.get(), new_length, lengths);
	}
}

// Get word metrics
RadixTrie::WordMetrics RadixTrie::get_word_metrics() const {
	WordMetrics metrics;
	std::vector<int> lengths;

	if (empty()) {
		metrics.min_length = 0;
		metrics.max_length = 0;
		metrics.average_length = 0.0;
		metrics.mode_length = 0;
		metrics.total_characters = 0;
		return metrics;
	}

	collect_word_lengths_recursive(root.get(), 0, lengths);

	metrics.min_length = *std::min_element(lengths.begin(), lengths.end());
	metrics.max_length = *std::max_element(lengths.begin(), lengths.end());

	// Calculate average
	size_t total_chars = std::accumulate(lengths.begin(), lengths.end(), 0);
	metrics.total_characters = total_chars;
	metrics.average_length = static_cast<double>(total_chars) / lengths.size();

	// Calculate mode (most frequent length)
	std::unordered_map<int, int> frequency;
	for (int length : lengths) {
		frequency[length]++;
	}

	int max_count = 0;
	for (const auto &[length, count] : frequency) {
		if (count > max_count) {
			max_count = count;
			metrics.mode_length = length;
		}
	}

	// Create length distribution
	int max_length = metrics.max_length;
	metrics.length_distribution.resize(max_length + 1, 0);
	for (int length : lengths) {
		metrics.length_distribution[length]++;
	}

	return metrics;
}

// Pattern matching helper - checks if word matches pattern with wildcards
bool RadixTrie::matches_pattern(const std::string &word,
								const std::string &pattern) const {
	size_t word_idx = 0, pattern_idx = 0;
	size_t word_len = word.length(), pattern_len = pattern.length();

	while (word_idx < word_len && pattern_idx < pattern_len) {
		if (pattern[pattern_idx] == '?') {
			// '?' matches any single character
			word_idx++;
			pattern_idx++;
		} else if (pattern[pattern_idx] == '*') {
			// '*' matches zero or more characters
			if (pattern_idx + 1 == pattern_len) {
				return true; // '*' at end matches everything
			}

			// Try to match the rest of the pattern with remaining word
			for (size_t i = word_idx; i <= word_len; ++i) {
				if (matches_pattern(word.substr(i),
									pattern.substr(pattern_idx + 1))) {
					return true;
				}
			}
			return false;
		} else if (pattern[pattern_idx] == word[word_idx]) {
			// Exact character match
			word_idx++;
			pattern_idx++;
		} else {
			return false; // Mismatch
		}
	}

	// Handle trailing '*'
	while (pattern_idx < pattern_len && pattern[pattern_idx] == '*') {
		pattern_idx++;
	}

	return word_idx == word_len && pattern_idx == pattern_len;
}

// Recursive pattern matching
void RadixTrie::pattern_match_recursive(
	const RadixNode *node, const std::string &current_word,
	const std::string &pattern, std::vector<std::string> &results) const {
	if (!node)
		return;

	std::string full_word = current_word + node->key;

	if (node->is_end && matches_pattern(full_word, pattern)) {
		results.push_back(full_word);
	}

	for (const auto &[ch, child] : node->children) {
		pattern_match_recursive(child.get(), full_word, pattern, results);
	}
}

// Pattern search with wildcards (* and ?)
std::vector<std::string>
RadixTrie::pattern_search(const std::string &pattern) const {
	std::vector<std::string> results;

	if (pattern.empty() || empty()) {
		return results;
	}

	pattern_match_recursive(root.get(), "", pattern, results);

	// Sort results for consistent output
	std::sort(results.begin(), results.end());

	return results;
}