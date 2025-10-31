// It is important to do the following steps to import the native addon in an ESM context, it needs to be understood that since the native addon is a compiled C++ project
// typescript cannot import and read the types and other relevant information from the addon. 
// To circumvent this, we use the createRequire function to create a require function for this ESM module and use that to import the addon.

// I highly recommend that you read this repository to understand more: https://github.com/xan105/node-addons/blob/main/lib/napi.js
// xan's repo has been a great resource for learning more about more interesting stuff about nodejs and addons, especicially methods for checking against tampering
// But again this repository will be focused on N-API and C++/C

// Import createRequire to use require() in ESM context
import { createRequire } from "module";

// Create a require function for this ESM module
const require = createRequire(import.meta.url);

// Import the native addon using the created require function
// The compiled module name matches the target_name in binding.gyp
const addon = require("../build/Release/seshat.node") as {
	Seshat: new () => {
		insert(word: string): void;
		insertBatch(words: string[]): number;
		insertFromFile(path: string, bufferSize?: number): number;
		insertFromFileAsync(path: string, cb: (err: any, count?: number) => void): void;
		search(word: string): boolean;
		searchBatch(words: string[]): boolean[];
		startsWith(prefix: string): boolean;
		wordsWithPrefix(prefix: string): string[];
		remove(word: string): boolean;
		removeBatch(words: string[]): boolean[];
		empty(): boolean;
		size(): number;
		clear(): void;
		getHeightStats(): any;
		getMemoryStats(): any;
		getWordMetrics(): any;
		patternSearch(pattern: string): string[];
	};
	Anna: new (threads?: number) => {
		loadDictionary(path: string): boolean;
		loadWords(words: string[]): void;
		addWord(word: string): void;
		clear(): void;
		size(): number;
		isValid(word: string): boolean;
		findAnagrams(input: string): string[];
		findValidPermutations(input: string, maxResults?: number): string[];
		getWordsByLength(length: number): string[];
		setThreadCount(n: number): void;
		getThreadCount(): number;
	};
};

export const { Seshat, Anna } = addon;