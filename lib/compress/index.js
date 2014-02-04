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

var events = require('events');
var Buffer = require('buffer').Buffer;
var assert = require('assert');
var bindings = require('./compress-bindings');

function removed(str) {
  return function() {
    throw new Error(str);
  }
}


function fallbackConstructor(str) {
  return function() {
    throw new Error(str);
  }
}


function inherits(ctor, superCtor) {
  ctor.prototype = Object.create(superCtor.prototype, {
      constructor: {
          value: ctor,
          enumerable: false
      }
  });
}


var Gzip = bindings.Gzip ||
           fallbackConstructor('Library built without gzip support.');
Gzip.prototype.init = removed('Use constructor to create new gzip object.');
Gzip.prototype.deflate = removed('Use write() instead.');
Gzip.prototype.end = removed('Use close() instead.');


var Gunzip = bindings.Gunzip ||
             fallbackConstructor('Library built without gzip support.');
Gunzip.prototype.init = removed('Use constructor to create new gunzip object.');
Gunzip.prototype.inflate = removed('Use write() instead.');
Gunzip.prototype.end = removed('Use close() instead.')


var Bzip = bindings.Bzip ||
           fallbackConstructor('Library built without bzip support.');
Bzip.prototype.init = removed('Use constructor to create new bzip object.');
Bzip.prototype.deflate = removed('Use write() instead.');
Bzip.prototype.end = removed('Use close() instead.');


var Bunzip = bindings.Bunzip ||
             fallbackConstructor('Library built without bzip support.');
Bunzip.prototype.init = removed('Use constructor to create new bunzip object.');
Bunzip.prototype.inflate = removed('Use write() instead.');
Bunzip.prototype.end = removed('Use close() instead.')


var apiWarnings = true;
function setApiWarnings(value) {
  apiWarnings = value;
}


function apiWarning(str) {
  if (apiWarnings) {
    console.warn(str);
  }
};


// === CommonStream ===
// Common base for compress/decompress streams.
function CommonStream(ctor, args) {
  events.EventEmitter.call(this);

  this.dataQueue_ = [];
  this.paused_ = false;
  this.inputEncoding_ = null;
  this.outputEncoding_ = null;
  this.readable = true;
  this.writeable = true;

  this.impl_ = ctor.createInstance_.apply(
      null, Array.prototype.slice.call(args, 0));
}
inherits(CommonStream, events.EventEmitter);


CommonStream.prototype.pause = function() {
  this.paused_ = true;
};


CommonStream.prototype.resume = function() {
  this.paused_ = false;
  this.emitData_();
};


CommonStream.prototype.destroy = function() {
  this.readable = false;
  this.writeable = false;
  this.impl_.destroy();
};


CommonStream.prototype.write = function(data, opt_encoding) {
  if (!this.writeable) {
    return true;
  }

  var self = this;
  var buffer = null;

  var encoding = null;
  if (!Buffer.isBuffer(data)) {
    encoding = opt_encoding || this.inputEncoding_ || 'utf8';
  }

  if (encoding !== null) {
    // Not buffer input.
    var len = Buffer.byteLength(data, encoding);
    if (len > 0) {
      buffer = new Buffer(len);
      buffer.write(data, encoding, 0);
    }
  } else {
    buffer = data;
  }

  if (Buffer.isBuffer(buffer)) {
    this.impl_.write(buffer, function(err, data) {
      self.emitEvent_(err, data);
    });
  } else {
    process.nextTick(function() {
      self.emitEvent_(Error('Fishy input'), null);
    });
  }
  return true;
};


CommonStream.prototype.end = function() {
  this.writeable = false;
  this.close();
};


CommonStream.prototype.close = function() {
  var self = this;

  this.impl_.close(function(err, data) {
    self.emitEvent_(err, data, true);
  });
};


CommonStream.prototype.setInputEncoding = function(enc) {
  apiWarning('setInputEncoding() breaks standard streams API.\n' +
      '  The method is an extension to standard API and might be removed in ' +
      'future.');
  this.inputEncoding_ = enc;
};


CommonStream.prototype.emitData_ = function() {
  if (!this.paused_) {
    for (var i = 0; i < this.dataQueue_.length; ++i) {
      var data = this.dataQueue_[i];
      if (data !== null) {
        this.emit('data', data);
      } else {
        this.readable = false;
        this.emit('end');
      }
    }
    this.dataQueue_.length = 0;
  }
};

CommonStream.prototype.emitEvent_ = function(err, data, fin) {
  if (err) {
    this.readable = false;
    this.writeable = false;
    this.emit('error', err);
    return;
  }

  if (data.length != 0) {
    if (this.outputEncoding_ != 'binary') {
      var str = data;
      var len = Buffer.byteLength(str, 'binary');
      data = new Buffer(len);
      if (len > 0) {
        data.write(str, 'binary', 0); 
      }

      if (this.outputEncoding_ != null) {
        data = data.toString(this.outputEncoding_, 0, data.length);
      }
    }
    this.dataQueue_.push(data);
  }

  if (fin) {
    this.dataQueue_.push(null);
  }
  this.emitData_();
};


// === CompressStream ===
// Common base for compression streams.
function CompressStream(ctor, args) {
  CommonStream.call(this, ctor, args);
}
inherits(CompressStream, CommonStream);


CompressStream.prototype.setEncoding = function(enc) {
  assert.equal(enc, 'binary',
      'CompressStream emits either Buffer or binary string.');
  this.outputEncoding_ = 'binary';
};


// === DecompressStream ===
// Common base for decompress streams.
function DecompressStream(ctor, args) {
  CommonStream.call(this, ctor, args);
}
inherits(DecompressStream, CommonStream);


DecompressStream.prototype.setEncoding = function(enc) {
  this.outputEncoding_ = enc;
};


// === GzipStream ===
function GzipStream() {
  CompressStream.call(this, Gzip, arguments);
}
inherits(GzipStream, CompressStream);


// === GunzipStream ===
function GunzipStream() {
  DecompressStream.call(this, Gunzip, arguments);
}
inherits(GunzipStream, DecompressStream);


// === BzipStream ===
function BzipStream() {
  CompressStream.call(this, Bzip, arguments);
}
inherits(BzipStream, CompressStream);


// === BunzipStream ===
function BunzipStream() {
  DecompressStream.call(this, Bunzip, arguments);
}
inherits(BunzipStream, DecompressStream);


exports.Gzip = Gzip;
exports.Gunzip = Gunzip;
exports.Bzip = Bzip;
exports.Bunzip = Bunzip;

exports.GzipStream = GzipStream;
exports.GunzipStream = GunzipStream;
exports.BzipStream = BzipStream;
exports.BunzipStream = BunzipStream;

exports.setApiWarnings = setApiWarnings;

exports.MAX_WBITS = bindings.MAX_WBITS;
exports.Z_DEFAULT_COMPRESSION = bindings.Z_DEFAULT_COMPRESSION;
