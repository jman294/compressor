echo "src/fpaq0"; TIMEFORMAT='%3R'; time src/fpaq0 c corpora/jquery-3.3.1.min.js output/jquery-3.3.1.min.js.fpaq0

echo "scale = 3; 1 - (`wc -c < output/jquery-3.3.1.min.js.fpaq0` / `wc -c < corpora/jquery-3.3.1.min.js`)" | bc

echo -e "jquery-3.3.1.min.js\n"
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape c corpora/jquery-3.3.1.min.js output/jquery-3.3.1.min.js.pt

echo "scale = 3; 1 - (`wc -c < output/jquery-3.3.1.min.js.pt` / `wc -c < corpora/jquery-3.3.1.min.js`)" | bc

echo -e "jquery-3.3.1.min.js\n"
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time zstd corpora/jquery-3.3.1.min.js -o output/jquery-3.3.1.min.js.zst

echo "scale = 3; 1 - (`wc -c < output/jquery-3.3.1.min.js.zst` / `wc -c < corpora/jquery-3.3.1.min.js`)" | bc

echo -e "jquery-3.3.1.min.js\n"
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time brotli -v corpora/jquery-3.3.1.min.js -o output/jquery-3.3.1.min.js.br

echo "scale = 3; 1 - (`wc -c < output/jquery-3.3.1.min.js.br` / `wc -c < corpora/jquery-3.3.1.min.js`)" | bc

echo -e "jquery-3.3.1.min.js\n"
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time src/fpaq0 c corpora/vim.small.c output/vim.small.c.fpaq0

echo "scale = 3; 1 - (`wc -c < output/vim.small.c.fpaq0` / `wc -c < corpora/vim.small.c`)" | bc

echo -e "vim.small.c\n"
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape c corpora/vim.small.c output/vim.small.c.pt

echo "scale = 3; 1 - (`wc -c < output/vim.small.c.pt` / `wc -c < corpora/vim.small.c`)" | bc

echo -e "vim.small.c\n"
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time zstd corpora/vim.small.c -o output/vim.small.c.zst

echo "scale = 3; 1 - (`wc -c < output/vim.small.c.zst` / `wc -c < corpora/vim.small.c`)" | bc

echo -e "vim.small.c\n"
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time brotli -v corpora/vim.small.c -o output/vim.small.c.br

echo "scale = 3; 1 - (`wc -c < output/vim.small.c.br` / `wc -c < corpora/vim.small.c`)" | bc

echo -e "vim.small.c\n"
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time src/fpaq0 c corpora/test/finnish-bank-utils.min.js output/test/finnish-bank-utils.min.js.fpaq0

echo "scale = 3; 1 - (`wc -c < output/test/finnish-bank-utils.min.js.fpaq0` / `wc -c < corpora/test/finnish-bank-utils.min.js`)" | bc

echo -e "test/finnish-bank-utils.min.js\n"
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape c corpora/test/finnish-bank-utils.min.js output/test/finnish-bank-utils.min.js.pt

echo "scale = 3; 1 - (`wc -c < output/test/finnish-bank-utils.min.js.pt` / `wc -c < corpora/test/finnish-bank-utils.min.js`)" | bc

echo -e "test/finnish-bank-utils.min.js\n"
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time zstd corpora/test/finnish-bank-utils.min.js -o output/test/finnish-bank-utils.min.js.zst

echo "scale = 3; 1 - (`wc -c < output/test/finnish-bank-utils.min.js.zst` / `wc -c < corpora/test/finnish-bank-utils.min.js`)" | bc

echo -e "test/finnish-bank-utils.min.js\n"
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time brotli -v corpora/test/finnish-bank-utils.min.js -o output/test/finnish-bank-utils.min.js.br

echo "scale = 3; 1 - (`wc -c < output/test/finnish-bank-utils.min.js.br` / `wc -c < corpora/test/finnish-bank-utils.min.js`)" | bc

echo -e "test/finnish-bank-utils.min.js\n"
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time src/fpaq0 c corpora/atest-sincos.c output/atest-sincos.c.fpaq0

echo "scale = 3; 1 - (`wc -c < output/atest-sincos.c.fpaq0` / `wc -c < corpora/atest-sincos.c`)" | bc

echo -e "atest-sincos.c\n"
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape c corpora/atest-sincos.c output/atest-sincos.c.pt

echo "scale = 3; 1 - (`wc -c < output/atest-sincos.c.pt` / `wc -c < corpora/atest-sincos.c`)" | bc

echo -e "atest-sincos.c\n"
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time zstd corpora/atest-sincos.c -o output/atest-sincos.c.zst

echo "scale = 3; 1 - (`wc -c < output/atest-sincos.c.zst` / `wc -c < corpora/atest-sincos.c`)" | bc

echo -e "atest-sincos.c\n"
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time brotli -v corpora/atest-sincos.c -o output/atest-sincos.c.br

echo "scale = 3; 1 - (`wc -c < output/atest-sincos.c.br` / `wc -c < corpora/atest-sincos.c`)" | bc

echo -e "atest-sincos.c\n"
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time src/fpaq0 c corpora/test/sincos_drift.mix output/test/sincos_drift.mix.fpaq0

echo "scale = 3; 1 - (`wc -c < output/test/sincos_drift.mix.fpaq0` / `wc -c < corpora/test/sincos_drift.mix`)" | bc

echo -e "test/sincos_drift.mix\n"
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape c corpora/test/sincos_drift.mix output/test/sincos_drift.mix.pt

echo "scale = 3; 1 - (`wc -c < output/test/sincos_drift.mix.pt` / `wc -c < corpora/test/sincos_drift.mix`)" | bc

echo -e "test/sincos_drift.mix\n"
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time zstd corpora/test/sincos_drift.mix -o output/test/sincos_drift.mix.zst

echo "scale = 3; 1 - (`wc -c < output/test/sincos_drift.mix.zst` / `wc -c < corpora/test/sincos_drift.mix`)" | bc

echo -e "test/sincos_drift.mix\n"
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time brotli -v corpora/test/sincos_drift.mix -o output/test/sincos_drift.mix.br

echo "scale = 3; 1 - (`wc -c < output/test/sincos_drift.mix.br` / `wc -c < corpora/test/sincos_drift.mix`)" | bc

echo -e "test/sincos_drift.mix\n"
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time src/fpaq0 c corpora/test/uastar.c output/test/uastar.c.fpaq0

echo "scale = 3; 1 - (`wc -c < output/test/uastar.c.fpaq0` / `wc -c < corpora/test/uastar.c`)" | bc

echo -e "test/uastar.c\n"
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape c corpora/test/uastar.c output/test/uastar.c.pt

echo "scale = 3; 1 - (`wc -c < output/test/uastar.c.pt` / `wc -c < corpora/test/uastar.c`)" | bc

echo -e "test/uastar.c\n"
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time zstd corpora/test/uastar.c -o output/test/uastar.c.zst

echo "scale = 3; 1 - (`wc -c < output/test/uastar.c.zst` / `wc -c < corpora/test/uastar.c`)" | bc

echo -e "test/uastar.c\n"
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time brotli -v corpora/test/uastar.c -o output/test/uastar.c.br

echo "scale = 3; 1 - (`wc -c < output/test/uastar.c.br` / `wc -c < corpora/test/uastar.c`)" | bc

echo -e "test/uastar.c\n"
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time src/fpaq0 c corpora/test/finn_uastar.mix output/test/finn_uastar.mix.fpaq0

echo "scale = 3; 1 - (`wc -c < output/test/finn_uastar.mix.fpaq0` / `wc -c < corpora/test/finn_uastar.mix`)" | bc

echo -e "test/finn_uastar.mix\n"
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape c corpora/test/finn_uastar.mix output/test/finn_uastar.mix.pt

echo "scale = 3; 1 - (`wc -c < output/test/finn_uastar.mix.pt` / `wc -c < corpora/test/finn_uastar.mix`)" | bc

echo -e "test/finn_uastar.mix\n"
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time zstd corpora/test/finn_uastar.mix -o output/test/finn_uastar.mix.zst

echo "scale = 3; 1 - (`wc -c < output/test/finn_uastar.mix.zst` / `wc -c < corpora/test/finn_uastar.mix`)" | bc

echo -e "test/finn_uastar.mix\n"
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time brotli -v corpora/test/finn_uastar.mix -o output/test/finn_uastar.mix.br

echo "scale = 3; 1 - (`wc -c < output/test/finn_uastar.mix.br` / `wc -c < corpora/test/finn_uastar.mix`)" | bc

echo -e "test/finn_uastar.mix\n"
echo -e "\n"
