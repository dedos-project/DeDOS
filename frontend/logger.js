var colors = {
    red: "\x1b[31m",
    green: "\x1b[32m",
    yellow: "\x1b[33m",
    cyan: "\x1b[36m",
    reset: '\x1b[0m'
};

var log = {};

log.info = function (a, ...rest) {
    console.log(colors.green + a + colors.reset, ...rest);
}

log.error = function(a, ...rest) {
    console.log(colors.red + a + colors.reset, ...rest);
}

log.warn = function(a, ...rest) {
    console.log(colors.yellow + a + colors.reset, ...rest);
}

log.debug = function(a, ...rest) {
    console.log(a, ...rest);
}

log.note = function(a, ...rest) {
    console.log(colors.cyan + a + colors.reset, ...rest);
}

module.exports = log;
