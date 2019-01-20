echo "src/fpaq0"; TIMEFORMAT='%3R'; time brotli -d output/jquery-3.3.1.min.js.br -o output/jquery-3.3.1.min.js

echo -e "jquery-3.3.1.min.js"
rm output/jquery-3.3.1.min.js
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape d output/jquery-3.3.1.min.js.pt output/jquery-3.3.1.min.js

echo -e "jquery-3.3.1.min.js"
rm output/jquery-3.3.1.min.js
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time unzstd output/jquery-3.3.1.min.js.zst -o output/jquery-3.3.1.min.js

echo -e "jquery-3.3.1.min.js"
rm output/jquery-3.3.1.min.js
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time src/fpaq0 d output/jquery-3.3.1.min.js.fpaq0 output/jquery-3.3.1.min.js

echo -e "jquery-3.3.1.min.js"
rm output/jquery-3.3.1.min.js
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time brotli -d output/vim.small.c.br -o output/vim.small.c

echo -e "vim.small.c"
rm output/vim.small.c
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape d output/vim.small.c.pt output/vim.small.c

echo -e "vim.small.c"
rm output/vim.small.c
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time unzstd output/vim.small.c.zst -o output/vim.small.c

echo -e "vim.small.c"
rm output/vim.small.c
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time src/fpaq0 d output/vim.small.c.fpaq0 output/vim.small.c

echo -e "vim.small.c"
rm output/vim.small.c
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time brotli -d output/test/finnish-bank-utils.min.js.br -o output/test/finnish-bank-utils.min.js

echo -e "test/finnish-bank-utils.min.js"
rm output/test/finnish-bank-utils.min.js
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape d output/test/finnish-bank-utils.min.js.pt output/test/finnish-bank-utils.min.js

echo -e "test/finnish-bank-utils.min.js"
rm output/test/finnish-bank-utils.min.js
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time unzstd output/test/finnish-bank-utils.min.js.zst -o output/test/finnish-bank-utils.min.js

echo -e "test/finnish-bank-utils.min.js"
rm output/test/finnish-bank-utils.min.js
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time src/fpaq0 d output/test/finnish-bank-utils.min.js.fpaq0 output/test/finnish-bank-utils.min.js

echo -e "test/finnish-bank-utils.min.js"
rm output/test/finnish-bank-utils.min.js
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time brotli -d output/atest-sincos.c.br -o output/atest-sincos.c

echo -e "atest-sincos.c"
rm output/atest-sincos.c
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape d output/atest-sincos.c.pt output/atest-sincos.c

echo -e "atest-sincos.c"
rm output/atest-sincos.c
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time unzstd output/atest-sincos.c.zst -o output/atest-sincos.c

echo -e "atest-sincos.c"
rm output/atest-sincos.c
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time src/fpaq0 d output/atest-sincos.c.fpaq0 output/atest-sincos.c

echo -e "atest-sincos.c"
rm output/atest-sincos.c
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time brotli -d output/test/sincos_drift.mix.br -o output/test/sincos_drift.mix

echo -e "test/sincos_drift.mix"
rm output/test/sincos_drift.mix
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape d output/test/sincos_drift.mix.pt output/test/sincos_drift.mix

echo -e "test/sincos_drift.mix"
rm output/test/sincos_drift.mix
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time unzstd output/test/sincos_drift.mix.zst -o output/test/sincos_drift.mix

echo -e "test/sincos_drift.mix"
rm output/test/sincos_drift.mix
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time src/fpaq0 d output/test/sincos_drift.mix.fpaq0 output/test/sincos_drift.mix

echo -e "test/sincos_drift.mix"
rm output/test/sincos_drift.mix
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time brotli -d output/test/uastar.c.br -o output/test/uastar.c

echo -e "test/uastar.c"
rm output/test/uastar.c
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape d output/test/uastar.c.pt output/test/uastar.c

echo -e "test/uastar.c"
rm output/test/uastar.c
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time unzstd output/test/uastar.c.zst -o output/test/uastar.c

echo -e "test/uastar.c"
rm output/test/uastar.c
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time src/fpaq0 d output/test/uastar.c.fpaq0 output/test/uastar.c

echo -e "test/uastar.c"
rm output/test/uastar.c
echo -e "\n"
echo "src/fpaq0"; TIMEFORMAT='%3R'; time brotli -d output/test/finn_uastar.mix.br -o output/test/finn_uastar.mix

echo -e "test/finn_uastar.mix"
rm output/test/finn_uastar.mix
echo -e "\n"
echo "build/packingtape"; TIMEFORMAT='%3R'; time build/packingtape d output/test/finn_uastar.mix.pt output/test/finn_uastar.mix

echo -e "test/finn_uastar.mix"
rm output/test/finn_uastar.mix
echo -e "\n"
echo "zstd"; TIMEFORMAT='%3R'; time unzstd output/test/finn_uastar.mix.zst -o output/test/finn_uastar.mix

echo -e "test/finn_uastar.mix"
rm output/test/finn_uastar.mix
echo -e "\n"
echo "brotli"; TIMEFORMAT='%3R'; time src/fpaq0 d output/test/finn_uastar.mix.fpaq0 output/test/finn_uastar.mix

echo -e "test/finn_uastar.mix"
rm output/test/finn_uastar.mix
echo -e "\n"
