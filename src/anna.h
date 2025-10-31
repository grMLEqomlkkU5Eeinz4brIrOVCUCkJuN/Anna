#ifndef ANNA_H
#define ANNA_H

#include "RadixTrie.h"
#include <algorithm>
#include <fstream>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class AnagramFinder {
  private:
	std::vector<std::string> dictionary;
	bool isSorted;
	size_t numThreads;
	RadixTrie trie; // use seshat's radix trie for validation

  public:
	// Constructor with default thread count
	AnagramFinder(size_t threads = std::thread::hardware_concurrency())
		: isSorted(false), numThreads(threads > 0 ? threads : 1) {}

	// Set number of threads to use
	void setThreadCount(size_t threads) {
		numThreads = (threads > 0) ? threads : 1;
	}

	// Get current thread count
	size_t getThreadCount() const { return numThreads; }

	// Load dictionary from file
	// Returns true on success, false on failure
	bool loadDictionary(const std::string &filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			return false;
		}

		dictionary.clear();
		trie.clear();
		std::string word;
		while (file >> word) {
			dictionary.push_back(word);
			trie.insert(std::string_view(word));
		}

		file.close();
		isSorted = false;
		return true;
	}

	// Load dictionary from vector of words
	void loadDictionary(const std::vector<std::string> &words) {
		dictionary = words;
		isSorted = false;
		trie.clear();
		for (const auto &w : words) {
			if (!w.empty()) {
				trie.insert(std::string_view(w));
			}
		}
	}

	// Add a single word to dictionary
	void addWord(const std::string &word) {
		dictionary.push_back(word);
		isSorted = false;
		if (!word.empty()) {
			trie.insert(std::string_view(word));
		}
	}

	// Clear the dictionary
	void clear() {
		dictionary.clear();
		isSorted = false;
		trie.clear();
	}

	// Get dictionary size
	size_t size() const { return dictionary.size(); }

	// Sort dictionary for binary search (call this after loading for faster
	// isValid)
	void sortDictionary() {
		std::sort(dictionary.begin(), dictionary.end());
		isSorted = true;
	}

	// Check if a word exists in dictionary
	bool isValid(const std::string &word) const {
		if (word.empty())
			return false;
		return trie.search(std::string_view(word));
	}

	// Find all valid anagrams of input string (multi-threaded)
	// User must pre-process input (remove spaces, lowercase, etc.)
	std::vector<std::string> findAnagrams(const std::string &input) const {
		if (dictionary.empty()) {
			return {};
		}

		// Create sorted signature of input
		std::string sortedInput = input;
		std::sort(sortedInput.begin(), sortedInput.end());

		size_t dictSize = dictionary.size();
		size_t actualThreads = std::min(numThreads, dictSize);
		size_t chunkSize = dictSize / actualThreads;

		std::vector<std::future<std::vector<std::string>>> futures;

		// Launch threads
		for (size_t i = 0; i < actualThreads; ++i) {
			size_t start = i * chunkSize;
			size_t end =
				(i == actualThreads - 1) ? dictSize : (i + 1) * chunkSize;

			futures.push_back(std::async(
				std::launch::async, [this, &sortedInput, &input, start, end]() {
					return this->findAnagramsInRange(
						sortedInput, input.length(), start, end);
				}));
		}

		// Collect results
		std::vector<std::string> results;
		for (auto &future : futures) {
			std::vector<std::string> partialResults = future.get();
			results.insert(results.end(), partialResults.begin(),
						   partialResults.end());
		}

		return results;
	}

	// Generate all permutations and return valid ones (multi-threaded)
	// Warning: Still slow for strings longer than ~10 characters
	// User must pre-process input
	std::vector<std::string>
	findValidPermutations(std::string input, size_t maxResults = 1000) const {
		std::sort(input.begin(), input.end());

		// Generate all permutations first
		std::vector<std::string> allPermutations;
		std::string temp = input;

		do {
			allPermutations.push_back(temp);
			if (allPermutations.size() >= maxResults * 10) {
				// Avoid generating too many permutations
				break;
			}
		} while (std::next_permutation(temp.begin(), temp.end()));

		size_t totalPerms = allPermutations.size();
		if (totalPerms == 0) {
			return {};
		}

		// Determine actual number of threads
		size_t actualThreads = std::min(numThreads, totalPerms);
		size_t chunkSize = totalPerms / actualThreads;

		std::vector<std::future<std::vector<std::string>>> futures;
		std::atomic<size_t> totalFound(0);

		// Launch threads
		for (size_t i = 0; i < actualThreads; ++i) {
			size_t start = i * chunkSize;
			size_t end =
				(i == actualThreads - 1) ? totalPerms : (i + 1) * chunkSize;

			futures.push_back(std::async(
				std::launch::async, [this, &allPermutations, start, end,
									 maxResults, &totalFound]() {
					return this->checkPermutationsInRange(
						allPermutations, start, end, maxResults, totalFound);
				}));
		}

		// Collect results
		std::vector<std::string> validWords;
		for (auto &future : futures) {
			std::vector<std::string> partialResults = future.get();
			validWords.insert(validWords.end(), partialResults.begin(),
							  partialResults.end());
			if (validWords.size() >= maxResults) {
				validWords.resize(maxResults);
				break;
			}
		}

		return validWords;
	}

	// Check if two strings are anagrams of each other
	static bool areAnagrams(const std::string &s1, const std::string &s2) {
		if (s1.length() != s2.length()) {
			return false;
		}

		std::string sorted1 = s1;
		std::string sorted2 = s2;
		std::sort(sorted1.begin(), sorted1.end());
		std::sort(sorted2.begin(), sorted2.end());

		return sorted1 == sorted2;
	}

	// Get all words from dictionary that match a given length (multi-threaded)
	std::vector<std::string> getWordsByLength(size_t length) const {
		size_t dictSize = dictionary.size();

		if (dictSize == 0) {
			return {};
		}

		size_t actualThreads = std::min(numThreads, dictSize);
		size_t chunkSize = dictSize / actualThreads;

		std::vector<std::future<std::vector<std::string>>> futures;

		for (size_t i = 0; i < actualThreads; ++i) {
			size_t start = i * chunkSize;
			size_t end =
				(i == actualThreads - 1) ? dictSize : (i + 1) * chunkSize;

			futures.push_back(
				std::async(std::launch::async, [this, length, start, end]() {
					std::vector<std::string> results;
					for (size_t j = start; j < end; ++j) {
						if (dictionary[j].length() == length) {
							results.push_back(dictionary[j]);
						}
					}
					return results;
				}));
		}

		std::vector<std::string> results;
		for (auto &future : futures) {
			std::vector<std::string> partialResults = future.get();
			results.insert(results.end(), partialResults.begin(),
						   partialResults.end());
		}

		return results;
	}

	// Get direct access to dictionary (for advanced users)
	const std::vector<std::string> &getDictionary() const { return dictionary; }

  private:
	// Helper function to find anagrams in a range of dictionary words
	std::vector<std::string> findAnagramsInRange(const std::string &sortedInput,
												 size_t inputLength,
												 size_t start,
												 size_t end) const {

		std::vector<std::string> results;

		for (size_t i = start; i < end; ++i) {
			if (dictionary[i].length() == inputLength) {
				std::string sortedWord = dictionary[i];
				std::sort(sortedWord.begin(), sortedWord.end());

				if (sortedWord == sortedInput) {
					results.push_back(dictionary[i]);
				}
			}
		}

		return results;
	}

	// Helper function to check permutations in a range
	std::vector<std::string>
	checkPermutationsInRange(const std::vector<std::string> &permutations,
							 size_t start, size_t end, size_t maxResults,
							 std::atomic<size_t> &totalFound) const {

		std::vector<std::string> results;

		for (size_t i = start; i < end; ++i) {
			if (totalFound >= maxResults) {
				break;
			}

			if (isValid(permutations[i])) {
				results.push_back(permutations[i]);
				totalFound++;
			}
		}

		return results;
	}
};

#endif
