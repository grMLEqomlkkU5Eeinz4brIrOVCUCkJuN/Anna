#include "Seshat.h"
#include <limits>

Napi::FunctionReference Seshat::constructor;

Seshat::Seshat(const Napi::CallbackInfo &info)
	: Napi::ObjectWrap<Seshat>(info) {}

Napi::Object Seshat::Init(Napi::Env env, Napi::Object exports) {
	Napi::Function func = DefineClass(
		env, "Seshat",
		{InstanceMethod("insert", &Seshat::Insert),
		 InstanceMethod("insertBatch", &Seshat::InsertBatch),
		 InstanceMethod("insertFromFile", &Seshat::InsertFromFile),
		 InstanceMethod("insertFromFileAsync", &Seshat::InsertFromFileAsync),
		 InstanceMethod("search", &Seshat::Search),
		 InstanceMethod("searchBatch", &Seshat::SearchBatch),
		 InstanceMethod("startsWith", &Seshat::StartsWith),
		 InstanceMethod("wordsWithPrefix", &Seshat::WordsWithPrefix),
		 InstanceMethod("remove", &Seshat::Remove),
		 InstanceMethod("removeBatch", &Seshat::RemoveBatch),
		 InstanceMethod("empty", &Seshat::Empty),
		 InstanceMethod("size", &Seshat::Size),
		 InstanceMethod("clear", &Seshat::Clear),
		 // New analytics methods
		 InstanceMethod("getHeightStats", &Seshat::GetHeightStats),
		 InstanceMethod("getMemoryStats", &Seshat::GetMemoryStats),
		 InstanceMethod("getWordMetrics", &Seshat::GetWordMetrics),
		 InstanceMethod("patternSearch", &Seshat::PatternSearch)});

	constructor = Napi::Persistent(func);
	constructor.SuppressDestruct();

	exports.Set("Seshat", func);
	return exports;
}

// Insert method - uses string_view to avoid copies
Napi::Value Seshat::Insert(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsString()) {
		Napi::TypeError::New(env, "String argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	// Use string_view to avoid unnecessary string copy
	std::string word = info[0].As<Napi::String>().Utf8Value();
	std::string_view word_view(word);
	trie_.insert(word_view);

	return env.Undefined();
}

// Search method - uses string_view to avoid copies
Napi::Value Seshat::Search(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsString()) {
		Napi::TypeError::New(env, "String argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	// Use string_view to avoid unnecessary string copy
	std::string word = info[0].As<Napi::String>().Utf8Value();
	std::string_view word_view(word);
	bool found = trie_.search(word_view);

	return Napi::Boolean::New(env, found);
}

// StartsWith method
Napi::Value Seshat::StartsWith(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsString()) {
		Napi::TypeError::New(env, "String argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	std::string prefix = info[0].As<Napi::String>().Utf8Value();
	bool hasPrefix = trie_.starts_with(prefix);

	return Napi::Boolean::New(env, hasPrefix);
}

// WordsWithPrefix method
Napi::Value Seshat::WordsWithPrefix(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsString()) {
		Napi::TypeError::New(env, "String argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	std::string prefix = info[0].As<Napi::String>().Utf8Value();
	std::vector<std::string> words = trie_.words_with_prefix(prefix);

	// Pre-allocate array for batch string creation
	Napi::Array result = Napi::Array::New(env, words.size());
	for (size_t i = 0; i < words.size(); ++i) {
		result[i] = Napi::String::New(env, words[i].c_str(), words[i].length());
	}

	return result;
}

// Remove method
Napi::Value Seshat::Remove(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsString()) {
		Napi::TypeError::New(env, "String argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	std::string word = info[0].As<Napi::String>().Utf8Value();
	bool removed = trie_.remove(word);

	return Napi::Boolean::New(env, removed);
}

// Empty method
Napi::Value Seshat::Empty(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();
	bool isEmpty = trie_.empty();
	return Napi::Boolean::New(env, isEmpty);
}

// Size method
Napi::Value Seshat::Size(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();
	size_t size = trie_.size();
	// Use safe conversion for large numbers
	// Check if size can be safely converted to double (use a reasonable upper
	// bound)
	const size_t MAX_SAFE_SIZE =
		1ULL << 53; // 2^53 is the max safe integer in JavaScript
	if (size > MAX_SAFE_SIZE) {
		Napi::TypeError::New(env,
							 "Size too large to represent as JavaScript number")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}
	return Napi::Number::New(env, static_cast<double>(size));
}

// Clear method
Napi::Value Seshat::Clear(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();
	trie_.clear();
	return env.Undefined();
}

// Batch Insert method - reduces N-API overhead
Napi::Value Seshat::InsertBatch(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsArray()) {
		Napi::TypeError::New(env, "Array argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	Napi::Array words = info[0].As<Napi::Array>();
	uint32_t count = 0;

	// Process all words in a single C++ call
	for (uint32_t i = 0; i < words.Length(); ++i) {
		if (words.Get(i).IsString()) {
			std::string word = words.Get(i).As<Napi::String>().Utf8Value();
			if (!word.empty()) {
				std::string_view word_view(word);
				trie_.insert(word_view);
				count++;
			}
		}
	}

	return Napi::Number::New(env, count);
}

// Batch Search method - returns array of results
Napi::Value Seshat::SearchBatch(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsArray()) {
		Napi::TypeError::New(env, "Array argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	Napi::Array words = info[0].As<Napi::Array>();
	Napi::Array results = Napi::Array::New(env, words.Length());

	// Process all searches in a single C++ call
	for (uint32_t i = 0; i < words.Length(); ++i) {
		if (words.Get(i).IsString()) {
			std::string word = words.Get(i).As<Napi::String>().Utf8Value();
			std::string_view word_view(word);
			bool found = trie_.search(word_view);
			results.Set(i, Napi::Boolean::New(env, found));
		} else {
			results.Set(i, Napi::Boolean::New(env, false));
		}
	}

	return results;
}

// Batch Remove method - returns array of success flags
Napi::Value Seshat::RemoveBatch(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsArray()) {
		Napi::TypeError::New(env, "Array argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	Napi::Array words = info[0].As<Napi::Array>();
	Napi::Array results = Napi::Array::New(env, words.Length());

	// Process all removals in a single C++ call
	for (uint32_t i = 0; i < words.Length(); ++i) {
		if (words.Get(i).IsString()) {
			std::string word = words.Get(i).As<Napi::String>().Utf8Value();
			std::string_view word_view(word);
			bool removed = trie_.remove(word_view);
			results.Set(i, Napi::Boolean::New(env, removed));
		} else {
			results.Set(i, Napi::Boolean::New(env, false));
		}
	}

	return results;
}

// InsertFromFile method
Napi::Value Seshat::InsertFromFile(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	// Validate arguments
	if (info.Length() < 1 || !info[0].IsString()) {
		Napi::TypeError::New(env, "File path string argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	std::string file_path = info[0].As<Napi::String>().Utf8Value();

	// Default buffer size of 1MB, but allow override
	size_t buffer_size = 1024 * 1024; // 1MB default

	// Check for optional buffer size parameter
	if (info.Length() >= 2 && info[1].IsNumber()) {
		double buffer_size_double = info[1].As<Napi::Number>().DoubleValue();

		// Validate buffer size
		if (buffer_size_double <= 0 || buffer_size_double > SIZE_MAX) {
			Napi::RangeError::New(
				env, "Buffer size must be positive and within valid range")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}

		buffer_size = static_cast<size_t>(buffer_size_double);

		// Ensure minimum buffer size of 1KB for efficiency
		if (buffer_size < 1024) {
			buffer_size = 1024;
		}
	}

	try {
		size_t words_inserted =
			trie_.bulk_insert_from_file(file_path, buffer_size);
		return Napi::Number::New(env, static_cast<double>(words_inserted));
	} catch (const std::exception &e) {
		Napi::Error::New(env,
						 std::string("Failed to insert from file: ") + e.what())
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}
}

// Async worker for insertFromFile
class InsertFromFileWorker : public Napi::AsyncWorker {
  public:
	InsertFromFileWorker(Seshat *instance, std::string filePath,
						 size_t bufferSize, Napi::Function &callback)
		: Napi::AsyncWorker(callback), instance_(instance),
		  filePath_(std::move(filePath)), bufferSize_(bufferSize),
		  wordsInserted_(0) {}

	void Execute() override {
		try {
			wordsInserted_ =
				instance_->trie_.bulk_insert_from_file(filePath_, bufferSize_);
		} catch (const std::exception &e) {
			SetError(e.what());
		}
	}

	void OnOK() override {
		Napi::HandleScope scope(Env());
		Callback().Call(
			{Env().Null(),
			 Napi::Number::New(Env(), static_cast<double>(wordsInserted_))});
	}

	void OnError(const Napi::Error &e) override {
		Napi::HandleScope scope(Env());
		Callback().Call({e.Value(), Env().Undefined()});
	}

  private:
	Seshat *instance_;
	std::string filePath_;
	size_t bufferSize_;
	size_t wordsInserted_;
};

// InsertFromFileAsync method
Napi::Value Seshat::InsertFromFileAsync(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 2 || !info[0].IsString() ||
		!info[info.Length() - 1].IsFunction()) {
		Napi::TypeError::New(env, "Expected (filePath: string, [bufferSize?: "
								  "number], callback: Function)")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	std::string file_path = info[0].As<Napi::String>().Utf8Value();

	// Default buffer size of 1MB
	size_t buffer_size = 1024 * 1024;

	// If a numeric second arg before callback is provided, treat as bufferSize
	if (info.Length() == 3 && info[1].IsNumber()) {
		double buffer_size_double = info[1].As<Napi::Number>().DoubleValue();
		if (buffer_size_double <= 0 || buffer_size_double > SIZE_MAX) {
			Napi::RangeError::New(
				env, "Buffer size must be positive and within valid range")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		buffer_size = static_cast<size_t>(buffer_size_double);
		if (buffer_size < 1024)
			buffer_size = 1024;
	}

	Napi::Function cb = info[info.Length() - 1].As<Napi::Function>();

	auto *worker = new InsertFromFileWorker(this, file_path, buffer_size, cb);
	worker->Queue();
	return env.Undefined();
}

// Get height statistics
Napi::Value Seshat::GetHeightStats(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	try {
		auto stats = trie_.get_height_stats();

		Napi::Object result = Napi::Object::New(env);
		result.Set("minHeight", Napi::Number::New(env, stats.min_height));
		result.Set("maxHeight", Napi::Number::New(env, stats.max_height));
		result.Set("averageHeight",
				   Napi::Number::New(env, stats.average_height));
		result.Set("modeHeight", Napi::Number::New(env, stats.mode_height));

		// Convert heights vector to JS array
		Napi::Array heights_array =
			Napi::Array::New(env, stats.all_heights.size());
		for (size_t i = 0; i < stats.all_heights.size(); ++i) {
			heights_array[i] = Napi::Number::New(env, stats.all_heights[i]);
		}
		result.Set("allHeights", heights_array);

		return result;
	} catch (const std::exception &e) {
		Napi::Error::New(env,
						 std::string("Failed to get height stats: ") + e.what())
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}
}

// Get memory statistics
Napi::Value Seshat::GetMemoryStats(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	try {
		auto stats = trie_.get_memory_stats();

		Napi::Object result = Napi::Object::New(env);
		result.Set(
			"totalBytes",
			Napi::Number::New(env, static_cast<double>(stats.total_bytes)));
		result.Set("nodeCount", Napi::Number::New(env, static_cast<double>(
														   stats.node_count)));
		result.Set(
			"stringBytes",
			Napi::Number::New(env, static_cast<double>(stats.string_bytes)));
		result.Set(
			"overheadBytes",
			Napi::Number::New(env, static_cast<double>(stats.overhead_bytes)));
		result.Set("bytesPerWord",
				   Napi::Number::New(env, stats.bytes_per_word));

		return result;
	} catch (const std::exception &e) {
		Napi::Error::New(env,
						 std::string("Failed to get memory stats: ") + e.what())
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}
}

// Get word metrics
Napi::Value Seshat::GetWordMetrics(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	try {
		auto metrics = trie_.get_word_metrics();

		Napi::Object result = Napi::Object::New(env);
		result.Set("minLength", Napi::Number::New(env, metrics.min_length));
		result.Set("maxLength", Napi::Number::New(env, metrics.max_length));
		result.Set("averageLength",
				   Napi::Number::New(env, metrics.average_length));
		result.Set("modeLength", Napi::Number::New(env, metrics.mode_length));
		result.Set("totalCharacters",
				   Napi::Number::New(
					   env, static_cast<double>(metrics.total_characters)));

		// Convert length distribution to JS array
		Napi::Array dist_array =
			Napi::Array::New(env, metrics.length_distribution.size());
		for (size_t i = 0; i < metrics.length_distribution.size(); ++i) {
			dist_array[i] =
				Napi::Number::New(env, metrics.length_distribution[i]);
		}
		result.Set("lengthDistribution", dist_array);

		return result;
	} catch (const std::exception &e) {
		Napi::Error::New(env,
						 std::string("Failed to get word metrics: ") + e.what())
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}
}

// Pattern search with wildcards
Napi::Value Seshat::PatternSearch(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsString()) {
		Napi::TypeError::New(env, "Pattern string argument expected")
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}

	try {
		std::string pattern = info[0].As<Napi::String>().Utf8Value();
		std::vector<std::string> matches = trie_.pattern_search(pattern);

		// Convert results to JS array
		Napi::Array result = Napi::Array::New(env, matches.size());
		for (size_t i = 0; i < matches.size(); ++i) {
			result[i] =
				Napi::String::New(env, matches[i].c_str(), matches[i].length());
		}

		return result;
	} catch (const std::exception &e) {
		Napi::Error::New(
			env, std::string("Failed to perform pattern search: ") + e.what())
			.ThrowAsJavaScriptException();
		return env.Undefined();
	}
}

// Forward declare Anna initializer from Anna.cc
Napi::Object InitAnna(Napi::Env env, Napi::Object exports);

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Seshat::Init(env, exports);
    InitAnna(env, exports);
    return exports;
}

// Register the module
NODE_API_MODULE(seshat, Init)