const { execSync } = require('child_process');
const { spawnSync } = require('child_process');
const fs = require('fs');

let files = ['jquery-3.3.1.min.js', 'vim.small.c',
               'test/finnish-bank-utils.min.js', 'atest-sincos.c',
               'test/sincos_drift.mix', 'test/uastar.c',
               'test/finn_uastar.mix']

let compressionCommands = ['build/packingtape c $1 $2.pt', 'zstd $1 -o $2.zst',
                           'brotli -v $1 -o $2.br', 'src/fpaq0 c $1 $2.fpaq0']

let decompressionCommands = ['build/packingtape d $1.pt $1', 'unzstd $1.zst -o $1',
                             'brotli -d $1.br -o $1', 'src/fpaq0 d $1.fpaq0 $1']

let compressionExtensions = {
  'build/packingtape': '.pt',
  'zstd': '.zst',
  'brotli': '.br',
  'src/fpaq0': '.fpaq0'
}

let timeString = 'TIMEFORMAT=\'%3R\'; time '

String.prototype.replaceAll = function(search, replacement) {
    var target = this;
    return target.split(search).join(replacement);
};

function shuffle(array) {
  var currentIndex = array.length, temporaryValue, randomIndex;

  // While there remain elements to shuffle...
  while (0 !== currentIndex) {

    // Pick a remaining element...
    randomIndex = Math.floor(Math.random() * currentIndex);
    currentIndex -= 1;

    // And swap it with the current element.
    temporaryValue = array[currentIndex];
    array[currentIndex] = array[randomIndex];
    array[randomIndex] = temporaryValue;
  }

  return array;
}

compressionCommands = shuffle(compressionCommands)
decompressionCommands = shuffle(decompressionCommands)

let testFile = ''

for (let i in files) {
  for (let j in compressionCommands) {
    let command = 'echo "' + compressionCommands[j].split(' ')[0] + '"; ' + timeString + compressionCommands[j]
                                       .replaceAll('$1', 'corpora/' + files[i])
                                       .replaceAll('$2', 'output/' + files[i])
                                       + '\n\n'
    testFile += command
    let extension = compressionExtensions[compressionCommands[j].split(' ')[0]]
    let o = files[i] + extension
    let ratioCommand = 'echo "scale = 3; 1 - (`wc -c < output/' + o + '` / `wc -c < corpora/' + files[i] + '`)" | bc\n\n'
    testFile += ratioCommand
    testFile += 'echo -e "' + files[i] + '\\n"\n'

    testFile += 'echo -e "\\n"\n'
  }
}

fs.writeFileSync('autogen-test.bash', testFile)
fs.chmodSync('autogen-test.bash', "755");

testFile = ''
for (let i in files) {
  for (let j in compressionCommands) {
    let dcommand =  'echo "' + compressionCommands[j].split(' ')[0] + '"; ' + timeString + decompressionCommands[j]
                                       .replaceAll('$1', 'output/' + files[i])
                                       + '\n\n'
    testFile += dcommand
    testFile += 'echo -e "' + files[i] + '"\n'
    testFile += 'rm output/' + files[i] + '\n'
    testFile += 'echo -e "\\n"\n'
  }
}

fs.writeFileSync('autogen-test-dec.bash', testFile)
fs.chmodSync('autogen-test-dec.bash', "755");
