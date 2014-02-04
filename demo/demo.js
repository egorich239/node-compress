/*
 * Copyright 2010, Ivan Egorov (egorich.3.04@gmail.com).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

var compress=require("../lib/compress");
var events=require('events');
var sys=require("sys");
var posix=require("fs");
var Buffer = require('buffer').Buffer;

function seq() {
  var self = this;
  var funs = Array.prototype.slice.call(arguments, 0);

  this.start = function(initial){
    process(0, initial);
  }

  function process(index, state) {
    funs[index](state, function(err, new_state) {
      if (err) throw err;

      ++index;
      if (index == funs.length) {
        self.emit('success', new_state);
        return;
      }

      process(index, new_state);
    });
  }
}
sys.inherits(seq, events.EventEmitter);

function createBuffer(str, enc) {
  enc = enc || 'utf8';
  var len = Buffer.byteLength(str, enc);
  var buf = new Buffer(len);
  buf.write(str, enc, 0);
  return buf;
}


// Create gzip stream
var gzip = new compress.Gzip(4, 16 + MAX_WBITS);
sys.puts('gzip created');

// Pump data to be compressed
var gzdata1, gzdata2, gzdata3;
var compression = new seq(
    function(state, continuation){
      gzip.write(createBuffer("My data that needs "), function(err, data) {
        sys.puts("Compressed size: " + data.length);
        continuation(err, state + data.toString('binary'));
      });
    },
    function(state, continuation){
      gzip.write(createBuffer("to be compressed. 01234567890."), function(err, data) {
        sys.puts("Compressed size: " + data.length);
        continuation(err, state + data.toString('binary'));
      }); 
    },
    function(state, continuation){
      gzip.close(function(err, data) {
        sys.puts("Last bit: " + data.length);
        data = state + data.toString('binary');
        sys.puts("Total compressed size: " + data.length);
        continuation(err, data);
      });
    }
);

var gunzip = new compress.Gunzip();
var decompression = new seq(
    function(state, continuation){
      gunzip.write(createBuffer(state.input[0], 'binary'), function(err, data) {
        state.output += data;
        continuation(err, state);
      });
    },
    function(state, continuation){
      gunzip.write(createBuffer(state.input[1], 'binary'), function(err, data) {
        state.output += data;
        continuation(err, state);
      });
    },
    function(state, continuation){
      gunzip.close(function(err, data){
        state.output += data;
        continuation(err, state);
      });
    }
);


compression.addListener('success', doDecompress);
doCompress();

function doCompress() {
  compression.start('');
}

function doDecompress(data) {
  sys.puts('Decompressing');
  decompression.addListener('success', function(data){
    sys.puts(data.output);
  });

  decompression.start({input:[data.substr(0,25), data.substr(25)],output:''});
}

