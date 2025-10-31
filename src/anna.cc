#include <napi.h>
#include "anna.h"

class Anna : public Napi::ObjectWrap<Anna> {
  public:
	static Napi::Object Init(Napi::Env env, Napi::Object exports) {
		Napi::Function func = DefineClass(
			env, "Anna",
			{
				InstanceMethod("loadDictionary", &Anna::LoadDictionary),
				InstanceMethod("loadWords", &Anna::LoadWords),
				InstanceMethod("addWord", &Anna::AddWord),
				InstanceMethod("clear", &Anna::Clear),
				InstanceMethod("size", &Anna::Size),
				InstanceMethod("isValid", &Anna::IsValid),
				InstanceMethod("findAnagrams", &Anna::FindAnagrams),
				InstanceMethod("findValidPermutations",
							   &Anna::FindValidPermutations),
				InstanceMethod("getWordsByLength", &Anna::GetWordsByLength),
				InstanceMethod("setThreadCount", &Anna::SetThreadCount),
				InstanceMethod("getThreadCount", &Anna::GetThreadCount),
			});

		constructor = Napi::Persistent(func);
		constructor.SuppressDestruct();
		exports.Set("Anna", func);
		return exports;
	}

	explicit Anna(const Napi::CallbackInfo &info)
		: Napi::ObjectWrap<Anna>(info), finder_() {
		if (info.Length() >= 1 && info[0].IsNumber()) {
			size_t threads =
				static_cast<size_t>(info[0].As<Napi::Number>().DoubleValue());
			finder_.setThreadCount(threads);
		}
	}

  private:
    static Napi::FunctionReference constructor;
    AnagramFinder finder_;

	Napi::Value LoadDictionary(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsString()) {
			Napi::TypeError::New(env, "Expected file path string")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		std::string path = info[0].As<Napi::String>().Utf8Value();
		bool ok = finder_.loadDictionary(path);
		return Napi::Boolean::New(env, ok);
	}

	Napi::Value LoadWords(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsArray()) {
			Napi::TypeError::New(env, "Expected array of strings")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		Napi::Array arr = info[0].As<Napi::Array>();
		std::vector<std::string> words;
		words.reserve(arr.Length());
		for (uint32_t i = 0; i < arr.Length(); ++i) {
			if (arr.Get(i).IsString()) {
				words.push_back(arr.Get(i).As<Napi::String>().Utf8Value());
			}
		}
		finder_.loadDictionary(words);
		return env.Undefined();
	}

	Napi::Value AddWord(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsString()) {
			Napi::TypeError::New(env, "Expected string")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		std::string word = info[0].As<Napi::String>().Utf8Value();
		finder_.addWord(word);
		return env.Undefined();
	}

	Napi::Value Clear(const Napi::CallbackInfo &info) {
		finder_.clear();
		return info.Env().Undefined();
	}

	Napi::Value Size(const Napi::CallbackInfo &info) {
		return Napi::Number::New(info.Env(),
								 static_cast<double>(finder_.size()));
	}

	Napi::Value IsValid(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsString()) {
			Napi::TypeError::New(env, "Expected string")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		std::string word = info[0].As<Napi::String>().Utf8Value();
		return Napi::Boolean::New(env, finder_.isValid(word));
	}

	Napi::Value FindAnagrams(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsString()) {
			Napi::TypeError::New(env, "Expected input string")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		std::string input = info[0].As<Napi::String>().Utf8Value();
		auto result = finder_.findAnagrams(input);
		Napi::Array out = Napi::Array::New(env, result.size());
		for (size_t i = 0; i < result.size(); ++i) {
			out[i] =
				Napi::String::New(env, result[i].c_str(), result[i].size());
		}
		return out;
	}

	Napi::Value FindValidPermutations(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsString()) {
			Napi::TypeError::New(env, "Expected input string")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		std::string input = info[0].As<Napi::String>().Utf8Value();
		size_t maxResults = 1000;
		if (info.Length() >= 2 && info[1].IsNumber()) {
			double d = info[1].As<Napi::Number>().DoubleValue();
			if (d > 0)
				maxResults = static_cast<size_t>(d);
		}
		auto result = finder_.findValidPermutations(input, maxResults);
		Napi::Array out = Napi::Array::New(env, result.size());
		for (size_t i = 0; i < result.size(); ++i) {
			out[i] =
				Napi::String::New(env, result[i].c_str(), result[i].size());
		}
		return out;
	}

	Napi::Value GetWordsByLength(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsNumber()) {
			Napi::TypeError::New(env, "Expected length number")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		size_t length =
			static_cast<size_t>(info[0].As<Napi::Number>().DoubleValue());
		auto result = finder_.getWordsByLength(length);
		Napi::Array out = Napi::Array::New(env, result.size());
		for (size_t i = 0; i < result.size(); ++i) {
			out[i] =
				Napi::String::New(env, result[i].c_str(), result[i].size());
		}
		return out;
	}

	Napi::Value SetThreadCount(const Napi::CallbackInfo &info) {
		Napi::Env env = info.Env();
		if (info.Length() < 1 || !info[0].IsNumber()) {
			Napi::TypeError::New(env, "Expected threads number")
				.ThrowAsJavaScriptException();
			return env.Undefined();
		}
		size_t threads =
			static_cast<size_t>(info[0].As<Napi::Number>().DoubleValue());
		finder_.setThreadCount(threads);
		return env.Undefined();
	}

	Napi::Value GetThreadCount(const Napi::CallbackInfo &info) {
		return Napi::Number::New(info.Env(),
								 static_cast<double>(finder_.getThreadCount()));
	}
};

Napi::FunctionReference Anna::constructor;

// Helper to allow other translation units to initialize this class
Napi::Object InitAnna(Napi::Env env, Napi::Object exports) {
	return Anna::Init(env, exports);
}

 