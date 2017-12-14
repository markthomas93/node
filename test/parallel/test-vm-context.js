'use strict';
require('../common');
const assert = require('assert');

const vm = require('vm');
const Script = vm.Script;
let script = new Script('"passed";');

// Run in a new empty context
let context = vm.createContext();
let result = script.runInContext(context);
assert.strictEqual('passed', result);

// Create a new pre-populated context
context = vm.createContext({ 'foo': 'bar', 'thing': 'lala' });
assert.strictEqual('bar', context.foo);
assert.strictEqual('lala', context.thing);

// Test updating context
script = new Script('foo = 3;');
result = script.runInContext(context);
assert.strictEqual(3, context.foo);
assert.strictEqual('lala', context.thing);

// Issue GH-227:
assert.throws(() => {
  vm.runInNewContext('', null, 'some.js');
}, /^TypeError: sandbox must be an object$/);

// Issue GH-1140:
// Test runInContext signature
let gh1140Exception;
try {
  vm.runInContext('throw new Error()', context, 'expected-filename.js');
} catch (e) {
  gh1140Exception = e;
  assert.ok(/expected-filename/.test(e.stack),
            `expected appearance of filename in Error stack: ${e.stack}`);
}
// This is outside of catch block to confirm catch block ran.
assert.strictEqual(gh1140Exception.toString(), 'Error');

// GH-558, non-context argument segfaults / raises assertion
const nonContextualSandboxErrorMsg =
  /^TypeError: contextifiedSandbox argument must be an object\.$/;
const contextifiedSandboxErrorMsg =
    /^TypeError: sandbox argument must have been converted to a context\.$/;
[
  [undefined, nonContextualSandboxErrorMsg],
  [null, nonContextualSandboxErrorMsg], [0, nonContextualSandboxErrorMsg],
  [0.0, nonContextualSandboxErrorMsg], ['', nonContextualSandboxErrorMsg],
  [{}, contextifiedSandboxErrorMsg], [[], contextifiedSandboxErrorMsg]
].forEach((e) => {
  assert.throws(() => { script.runInContext(e[0]); }, e[1]);
  assert.throws(() => { vm.runInContext('', e[0]); }, e[1]);
});

// Issue GH-693:
// Test RegExp as argument to assert.throws
script = vm.createScript('const assert = require(\'assert\'); assert.throws(' +
                         'function() { throw "hello world"; }, /hello/);',
                         'some.js');
script.runInNewContext({ require: require });

// Issue GH-7529
script = vm.createScript('delete b');
let ctx = {};
Object.defineProperty(ctx, 'b', { configurable: false });
ctx = vm.createContext(ctx);
assert.strictEqual(script.runInContext(ctx), false);

// Error on the first line of a module should have the correct line and column
// number.
{
  let stack = null;
  assert.throws(() => {
    vm.runInContext(' throw new Error()', context, {
      filename: 'expected-filename.js',
      lineOffset: 32,
      columnOffset: 123
    });
  }, (err) => {
    stack = err.stack;
    return /^ \^/m.test(stack) &&
           /expected-filename\.js:33:131/.test(stack);
  }, `stack not formatted as expected: ${stack}`);
}

// https://github.com/nodejs/node/issues/6158
ctx = new Proxy({}, {});
assert.strictEqual(typeof vm.runInNewContext('String', ctx), 'function');
