project('compressor', 'c')

sources = [
    'src/impl/model.c',
    'src/impl/compressor.c',
    'src/impl/compressorpredictor.c',
    'src/impl/decompressor.c',
    'src/impl/decompressorpredictor.c',
    'src/impl/util.c',
    ]

headers = [
    'src/include/packingtape/modelenum.h',
    'src/include/packingtape/model.h',
    'src/include/packingtape/util.h',
    'src/include/packingtape/decompressor.h',
    'src/include/packingtape/decompressorpredictor.h',
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

executable('packingtape', 'src/main.c', link_with: lib, include_directories: exe_inc)

# Testing
test_sources = [
  'compressorpredictor',
  'model'
]

foreach t: test_sources
  test_exec = executable(
      t,
      'test/' + t + '.spec.c',
      include_directories: [
        lib_inc,
        include_directories('test/include') # acutest header
      ],
      link_with: lib,
  )
  test(t, test_exec)
endforeach
