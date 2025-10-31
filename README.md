## Anna and Seshat

Anna provides an anagram and dictionary utility. Validation uses Seshat, a radix trie implemented in C++ and exposed with N-API.

### npm install
```
npm i anna-the-gram
```

### Build

```bash
npm ci
npm run build
```

The native module is built at `build/Release/seshat.node`.

### Use in Node.js

The package exports the native classes via the ESM entry.

```ts
import { Seshat, Anna } from "cppintegrations";

// Seshat: trie operations
const trie = new Seshat();
trie.insertBatch(["cat", "dog", "catalog"]);
trie.search("dog");
trie.startsWith("cat");
trie.wordsWithPrefix("cat");

// Anna: anagram finder
const anna = new Anna();
anna.loadWords(["cat", "act", "tac", "dog"]);
anna.isValid("cat");
anna.findAnagrams("tac");
```

During local development, you can also load the native binary directly:

```js
const addon = require("./build/Release/seshat.node");
const { Seshat, Anna } = addon;
```

### Loading a word list file

Seshat supports bulk loading from a newline-delimited file:

```js
const t = new Seshat();
const loaded = t.insertFromFile("./enable1.txt", 1024 * 1024);
console.log(loaded, t.size());
```

### Tests

```bash
npm test
```

Test suites:

- `test/native.test.js` covers Seshat and Anna basics
- `test/enable.test.js` optionally runs with the ENABLE word list

The ENABLE test checks for a file before running. Provide it with one of:

- Set `ENABLE_WORD_LIST=/path/to/enable1.txt`
- Place `enable1.txt` in the project root

### CI

GitHub Actions builds and runs tests on Linux, macOS, and Windows for Node 20 and 22. It attempts to download the ENABLE list for the optional tests.


