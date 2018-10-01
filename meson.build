project('compressor', 'c')

sources = [
  'src/impl/model.c',
  'src/impl/compressor.c',
  'src/impl/compressorpredictor.c',
]

headers = [
  'src/include/packingtape/model.h',
  'src/include/packingtape/compressor.h',
  'src/include/packingtape/compressorpredictor.h',
]

install_headers(
  headers,
  subdir: 'packingtape'
)

lib_inc = include_directories('src/include/packingtape')

lib = library('packingtape',
  sources: [
    sources,
    headers
  ],
  soversion: 20,
  install: true,
  include_directories: lib_inc
)

exe_inc = include_directories('src/include')

executable('packingtape', 'src/main.c', link_with : lib, include_directories: exe_inc)
