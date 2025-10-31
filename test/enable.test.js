"use strict";

const fs = require("fs");
const path = require("path");

// Load the compiled native addon directly
const addon = require("../build/Release/seshat.node");
const { Seshat } = addon;

// Resolve a likely ENABLE word list path
function resolveEnablePath() {
	const candidates = [
		process.env.ENABLE_WORD_LIST,
		path.join(process.cwd(), "enable1.txt"),
		path.join(process.cwd(), "enable.txt"),
		path.join(__dirname, "enable1.txt"),
		path.join(__dirname, "enable.txt"),
	].filter(Boolean);

	for (const p of candidates) {
		try {
			if (fs.existsSync(p)) return p;
		} catch (_) { }
	}
	return null;
}

const enablePath = resolveEnablePath();

(enablePath ? describe : describe.skip)("Seshat with ENABLE word list", () => {
	test("insertFromFile loads words and enables search", () => {
		// Read first non-empty line to assert a known word is present after load
		const content = fs.readFileSync(enablePath, "utf8");
		const firstWord = content.split(/\r?\n/).find((l) => l && l.trim().length > 0)?.trim();
		expect(typeof firstWord).toBe("string");
		expect(firstWord.length).toBeGreaterThan(0);

		const t = new Seshat();
		const inserted = t.insertFromFile(enablePath, 256 * 1024);
		expect(inserted).toBeGreaterThan(0);
		expect(t.size()).toBe(inserted);
		expect(t.search(firstWord)).toBe(true);
	});
});

if (!enablePath) {
	// Log a helpful hint without failing CI
	// eslint-disable-next-line no-console
	console.warn(
		"ENABLE word list not found. Set ENABLE_WORD_LIST or place enable1.txt in project root to run this test."
	);
}


