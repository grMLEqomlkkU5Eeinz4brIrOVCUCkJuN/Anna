"use strict";

// Load the compiled native addon directly to avoid ESM interop issues
const addon = require("../build/Release/seshat.node");
const { Seshat, Anna } = addon;

describe("Seshat (RadixTrie) basic operations", () => {
	test("insert/search/size", () => {
		const t = new Seshat();
		expect(t.empty()).toBe(true);
		expect(t.size()).toBe(0);

		t.insert("cat");
		t.insert("dog");
		t.insert("catalog");

		expect(t.empty()).toBe(false);
		expect(t.size()).toBe(3);
		expect(t.search("cat")).toBe(true);
		expect(t.search("dog")).toBe(true);
		expect(t.search("bird")).toBe(false);
	});

	test("insertBatch / startsWith / wordsWithPrefix / remove", () => {
		const t = new Seshat();
		const words = ["apple", "app", "apply", "apt", "banana"];
		const inserted = t.insertBatch(words);
		expect(inserted).toBe(words.length);

		expect(t.startsWith("ap")).toBe(true);
		const pref = t.wordsWithPrefix("app");
		expect(pref.sort()).toEqual(["app", "apple", "apply"].sort());

		expect(t.remove("apt")).toBe(true);
		expect(t.search("apt")).toBe(false);
		expect(t.size()).toBe(words.length - 1);
	});
});

describe("Anna (AnagramFinder wrapper) behavior", () => {
	test("loadWords / isValid uses trie behind the scenes", () => {
		const a = new Anna();
		a.loadWords(["cat", "dog", "god"]);
		expect(a.size()).toBe(3);
		expect(a.isValid("cat")).toBe(true);
		expect(a.isValid("bird")).toBe(false);
	});

	test("findAnagrams returns expected matches", () => {
		const a = new Anna();
		a.loadWords(["cat", "tac", "act", "dog"]);
		const res = a.findAnagrams("tac");
		// Order is not guaranteed; compare as sets
		expect(new Set(res)).toEqual(new Set(["cat", "tac", "act"]));
	});

	test("getWordsByLength filters correctly", () => {
		const a = new Anna();
		a.loadWords(["a", "bb", "ccc", "dd", "eee"]);
		const twos = a.getWordsByLength(2).sort();
		expect(twos).toEqual(["bb", "dd"]);
	});
});


