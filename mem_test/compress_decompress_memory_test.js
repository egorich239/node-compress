var compress = require('../lib/compress');
var sys = require('sys')
var fs = require('fs')
var gzip = new compress.Gzip;
var gunzip = new compress.Gunzip;

var data = fs.readFileSync('testfile', 'utf8')
var count = 0;
compress.setApiWarnings(false);
function doFileCompress() {
    if (count >= 200) {
	console.log("Waiting 10 seconds for GC to collect any extra garbage...");
	setTimeout(function() {
	    console.log("Memory usage should be back to normal now!");
	    setTimeout(function() {
	    }, 5000);
	}, 10000)
	return;
    }

    count++;
    var gzipper = new compress.GzipStream();
    var compressedText = [];
    gzipper.setInputEncoding('utf8');
    gzipper.setEncoding('binary');
    gzipper.addListener('data', function(data) {
 	compressedText = compressedText + data;
    }).addListener('error', function(err) {
 	throw err;
    }).addListener('end', function() {
	doDecompress(compressedText, function(decompressedText) {
	    if (!(count % 10))
		console.log("Finished " + count);
 	    if (decompressedText !== data)
		throw "Texts didn't match!";
	    doFileCompress();
	});
    }); 
    gzipper.write(data);
    gzipper.close();
}

function doDecompress(data, callback) {
    var gunzip = new compress.GunzipStream();
    var returnData = '';
    gunzip.setEncoding('utf8');
    gunzip.setInputEncoding('binary');
    gunzip.addListener('data', function(data) {
	returnData = returnData + data;
    }).addListener('error', function(err) {
	throw err;
    }).addListener('end', function() {
	callback(returnData);
    });
    gunzip.write(data);
    gunzip.close();
}

console.log("Monitor node's memory usage while running this test!");
doFileCompress();